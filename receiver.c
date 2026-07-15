/* BASELINE RECEIVER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender, via the relay (optional)
 *
 * This baseline forwards whatever arrives straight to the player: lost
 * frames stay lost, late frames stay late, duplicates are re-sent
 * harmlessly. All yours to fix — jitter buffer, reordering, recovery.
 *
 * Env vars available: T0, DURATION_S, DELAY_MS. Harness kills the process
 * at run end; a forever-loop is fine.
 */
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD 160
#define MAX_FRAMES 65536

#define TYPE_MASK 0xC0000000u
#define SEQ_MASK 0x3FFFFFFFu
#define DATA 0x00000000u
#define PAIR 0x40000000u

typedef struct {
    bool present;
    bool sent;
    unsigned char data[PAYLOAD];
} Frame;

typedef struct {
    bool present;
    unsigned char data[PAYLOAD];
} Parity;

static Frame frames[MAX_FRAMES];
static Parity pairs[MAX_FRAMES];

static void send_frame(int fd, struct sockaddr_in *player, uint32_t seq) {
    if (seq >= MAX_FRAMES || !frames[seq].present || frames[seq].sent)
        return;

    unsigned char packet[4 + PAYLOAD];
    uint32_t net_seq = htonl(seq);

    memcpy(packet, &net_seq, 4);
    memcpy(packet + 4, frames[seq].data, PAYLOAD);

    sendto(fd, packet, sizeof packet, 0, (struct sockaddr *)player,
           sizeof *player);

    frames[seq].sent = true;
}

static void recover_pair(int out_fd, struct sockaddr_in *player,
                         uint32_t base) {
    if (base + 1 >= MAX_FRAMES || !pairs[base].present)
        return;

    bool first = frames[base].present;
    bool second = frames[base + 1].present;

    if (first == second)
        return;

    uint32_t missing = first ? base + 1 : base;
    uint32_t known = first ? base : base + 1;

    for (int i = 0; i < PAYLOAD; i++)
        frames[missing].data[i] =
            pairs[base].data[i] ^ frames[known].data[i];

    frames[missing].present = true;
    send_frame(out_fd, player, missing);
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 4 + PAYLOAD) continue;

        uint32_t net_tag;
        memcpy(&net_tag, buf, 4);

        uint32_t tag = ntohl(net_tag);
        uint32_t type = tag & TYPE_MASK;
        uint32_t seq = tag & SEQ_MASK;

        if (type == DATA) {
            if (seq >= MAX_FRAMES) continue;

            if (!frames[seq].present) {
                memcpy(frames[seq].data, buf + 4, PAYLOAD);
                frames[seq].present = true;
            }

            send_frame(out_fd, &player, seq);

            uint32_t base = seq - (seq % 2);
            recover_pair(out_fd, &player, base);
        } else if (type == PAIR) {
            if (seq + 1 >= MAX_FRAMES) continue;

            if (!pairs[seq].present) {
                memcpy(pairs[seq].data, buf + 4, PAYLOAD);
                pairs[seq].present = true;
            }

            recover_pair(out_fd, &player, seq);
        }
    }

    return 0;
}