#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

static inline long now_ns() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000000LL + (long)tv.tv_usec * 1000LL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <msg_size> <count>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    long count = atol(argv[4]);

    if (port <= 0 || msg_size <= 0 || count <= 0) {
        fprintf(stderr, "Invalid port/msg_size/count\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    char *buf = malloc(msg_size);
    if (!buf) {
        perror("malloc buf");
        close(sock);
        return 1;
    }
    memset(buf, 'A', msg_size);

    long *samples = malloc(count * sizeof(long));
    if (!samples) {
        perror("malloc samples");
        free(buf);
        close(sock);
        return 1;
    }

    printf("[UDP-CLIENT] starting ping-pong to %s:%d  msg=%dB  count=%ld\n",
           server_ip, port, msg_size, count);

    for (long i = 0; i < count; i++) {
        long t0 = now_ns();

        ssize_t sent = sendto(sock, buf, msg_size, 0,
                              (struct sockaddr *)&srv, sizeof(srv));
        if (sent < 0) {
            perror("sendto");
            free(buf);
            free(samples);
            close(sock);
            return 1;
        }

        ssize_t n = recv(sock, buf, msg_size, 0);
        if (n < 0) {
            perror("recv");
            free(buf);
            free(samples);
            close(sock);
            return 1;
        }

        long t1 = now_ns();
        samples[i] = t1 - t0;
    }

    FILE *f = fopen("udp_pingpong.csv", "w");
    if (!f) {
        perror("fopen");
        free(buf);
        free(samples);
        close(sock);
        return 1;
    }

    fprintf(f, "idx,ns\n");
    for (long i = 0; i < count; i++) {
        fprintf(f, "%ld,%ld\n", i, samples[i]);
    }
    fclose(f);

    printf("[UDP-CLIENT] CSV written: udp_pingpong.csv\n");

    free(buf);
    free(samples);
    close(sock);

    return 0;
}
