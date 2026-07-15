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
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD 160
#define DATA 0x00000000u
#define PAIR 0x40000000u

static void send_packet(int fd, struct sockaddr_in *relay, uint32_t tag,
                        const unsigned char *payload) {
    unsigned char packet[4 + PAYLOAD];
    uint32_t net_tag = htonl(tag);

    memcpy(packet, &net_tag, 4);
    memcpy(packet + 4, payload, PAYLOAD);

    sendto(fd, packet, sizeof packet, 0, (struct sockaddr *)relay,
           sizeof *relay);
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    unsigned char previous[PAYLOAD];
    unsigned char parity[PAYLOAD];
    uint32_t previous_seq = 0;
    int have_previous = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 4 + PAYLOAD) continue;

        uint32_t net_seq;
        memcpy(&net_seq, buf, 4);
        uint32_t seq = ntohl(net_seq);
        unsigned char *payload = buf + 4;

        send_packet(out_fd, &relay, DATA | seq, payload);

        if (seq % 2 == 1 && have_previous && previous_seq == seq - 1) {
            for (int i = 0; i < PAYLOAD; i++)
                parity[i] = previous[i] ^ payload[i];

            send_packet(out_fd, &relay, PAIR | (seq - 1), parity);
        }

        memcpy(previous, payload, PAYLOAD);
        previous_seq = seq;
        have_previous = 1;
    }

    return 0;
}