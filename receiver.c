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
#define PACKET_SIZE (4 + PAYLOAD)
#define MAX_FRAMES 65536
#define WORK_SIZE 32

#define TYPE_MASK 0xC0000000u
#define SEQ_MASK 0x3FFFFFFFu
#define DATA 0x00000000u
#define PAIR 0x40000000u
#define TRIPLE 0x80000000u

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
static Parity triples[MAX_FRAMES];

static void send_frame(int fd, const struct sockaddr_in *player, uint32_t seq) {
    if (seq >= MAX_FRAMES || !frames[seq].present || frames[seq].sent)
        return;

    unsigned char packet[PACKET_SIZE];
    uint32_t net_seq = htonl(seq);

    memcpy(packet, &net_seq, 4);
    memcpy(packet + 4, frames[seq].data, PAYLOAD);

    ssize_t n = sendto(fd, packet, sizeof packet, 0,
                       (const struct sockaddr *)player, sizeof *player);

    if (n == (ssize_t)sizeof packet)
        frames[seq].sent = true;
}

static bool recover_pair(uint32_t base, uint32_t *recovered) {
    if (base + 1 >= MAX_FRAMES || !pairs[base].present)
        return false;

    bool first = frames[base].present;
    bool second = frames[base + 1].present;

    if (first == second)
        return false;

    uint32_t missing = first ? base + 1 : base;
    uint32_t known = first ? base : base + 1;

    for (int i = 0; i < PAYLOAD; i++)
        frames[missing].data[i] =
            pairs[base].data[i] ^ frames[known].data[i];

    frames[missing].present = true;
    *recovered = missing;
    return true;
}

static bool recover_triple(uint32_t base, uint32_t *recovered) {
    if (base + 2 >= MAX_FRAMES || !triples[base].present)
        return false;

    int missing = -1;
    int count = 0;

    for (int i = 0; i < 3; i++) {
        if (!frames[base + i].present) {
            missing = i;
            count++;
        }
    }

    if (count != 1)
        return false;

    uint32_t seq = base + (uint32_t)missing;
    memcpy(frames[seq].data, triples[base].data, PAYLOAD);

    for (int j = 0; j < 3; j++) {
        uint32_t current = base + (uint32_t)j;
        if (current == seq)
            continue;

        for (int i = 0; i < PAYLOAD; i++)
            frames[seq].data[i] ^= frames[current].data[i];
    }

    frames[seq].present = true;
    *recovered = seq;
    return true;
}

static void recover_from(int out_fd, const struct sockaddr_in *player,
                         uint32_t seq) {
    uint32_t work[WORK_SIZE];
    int head = 0;
    int tail = 0;

    work[tail++] = seq;

    while (head < tail) {
        uint32_t current = work[head++];
        uint32_t recovered;

        uint32_t pair_base = current - (current % 2);
        if (recover_pair(pair_base, &recovered)) {
            send_frame(out_fd, player, recovered);

            if (tail < WORK_SIZE)
                work[tail++] = recovered;
        }

        uint32_t triple_base = current - (current % 3);
        if (recover_triple(triple_base, &recovered)) {
            send_frame(out_fd, player, recovered);

            if (tail < WORK_SIZE)
                work[tail++] = recovered;
        }
    }
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket 47002");
        return 1;
    }

    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        close(in_fd);
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket 47020");
        close(in_fd);
        return 1;
    }

    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n != PACKET_SIZE)
            continue;

        uint32_t net_tag;
        memcpy(&net_tag, buf, 4);

        uint32_t tag = ntohl(net_tag);
        uint32_t type = tag & TYPE_MASK;
        uint32_t seq = tag & SEQ_MASK;

        if (seq >= MAX_FRAMES)
            continue;

        if (type == DATA) {
            if (!frames[seq].present) {
                memcpy(frames[seq].data, buf + 4, PAYLOAD);
                frames[seq].present = true;
            }

            send_frame(out_fd, &player, seq);
            recover_from(out_fd, &player, seq);
        } else if (type == PAIR) {
            if (seq + 1 >= MAX_FRAMES)
                continue;

            if (!pairs[seq].present) {
                memcpy(pairs[seq].data, buf + 4, PAYLOAD);
                pairs[seq].present = true;
            }

            uint32_t recovered;
            if (recover_pair(seq, &recovered)) {
                send_frame(out_fd, &player, recovered);
                recover_from(out_fd, &player, recovered);
            }
        } else if (type == TRIPLE) {
            if (seq + 2 >= MAX_FRAMES)
                continue;

            if (!triples[seq].present) {
                memcpy(triples[seq].data, buf + 4, PAYLOAD);
                triples[seq].present = true;
            }

            uint32_t recovered;
            if (recover_triple(seq, &recovered)) {
                send_frame(out_fd, &player, recovered);
                recover_from(out_fd, &player, recovered);
            }
        }
    }

    return 0;
}