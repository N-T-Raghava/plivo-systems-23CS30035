/* BASELINE SENDER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver (YOUR wire format)
 *   bind 47004  <- feedback from your receiver, via the relay (optional)
 *
 * This baseline forwards each frame once, unchanged, and ignores feedback.
 * No redundancy, no retransmission. It cannot pass. That is the point.
 *
 * Env vars available if you want them: T0 (epoch seconds, float),
 * DURATION_S, DELAY_MS. The harness kills this process when the run ends,
 * so a forever-loop is fine.
 *
 * build: make        run: python3 run.py --delay_ms 60
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

#define DATA 0x00000000u
#define PAIR 0x40000000u
#define TRIPLE 0x80000000u

static int send_packet(int fd, const struct sockaddr_in *relay, uint32_t tag,
                       const unsigned char *payload) {
    unsigned char packet[PACKET_SIZE];
    uint32_t net_tag = htonl(tag);

    memcpy(packet, &net_tag, 4);
    memcpy(packet + 4, payload, PAYLOAD);

    ssize_t n = sendto(fd, packet, sizeof packet, 0,
                       (const struct sockaddr *)relay, sizeof *relay);

    return n == (ssize_t)sizeof packet;
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket 47010");
        return 1;
    }

    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        close(in_fd);
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket 47001");
        close(in_fd);
        return 1;
    }

    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    unsigned char history[3][PAYLOAD];
    unsigned char parity[PAYLOAD];

    uint32_t stored_seq[3] = {0};
    bool valid[3] = {false};

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n != PACKET_SIZE)
            continue;

        uint32_t net_seq;
        memcpy(&net_seq, buf, 4);

        uint32_t seq = ntohl(net_seq);
        if (seq >= 0x40000000u)
            continue;

        unsigned char *payload = buf + 4;
        uint32_t slot = seq % 3;

        memcpy(history[slot], payload, PAYLOAD);
        stored_seq[slot] = seq;
        valid[slot] = true;

        send_packet(out_fd, &relay, DATA | seq, payload);

        /* one parity packet for every pair */
        if (seq % 2 == 1) {
            uint32_t first = seq - 1;
            uint32_t a = first % 3;
            uint32_t b = seq % 3;

            if (valid[a] && valid[b] &&
                stored_seq[a] == first &&
                stored_seq[b] == seq) {
                for (int i = 0; i < PAYLOAD; i++)
                    parity[i] = history[a][i] ^ history[b][i];

                send_packet(out_fd, &relay, PAIR | first, parity);
            }
        }

        /* a second grouping uses the remaining bandwidth */
        if (seq % 3 == 2) {
            uint32_t first = seq - 2;
            uint32_t a = first % 3;
            uint32_t b = (first + 1) % 3;
            uint32_t c = seq % 3;

            if (valid[a] && valid[b] && valid[c] &&
                stored_seq[a] == first &&
                stored_seq[b] == first + 1 &&
                stored_seq[c] == seq) {
                for (int i = 0; i < PAYLOAD; i++)
                    parity[i] =
                        history[a][i] ^
                        history[b][i] ^
                        history[c][i];

                send_packet(out_fd, &relay, TRIPLE | first, parity);
            }
        }
    }

    return 0;
}