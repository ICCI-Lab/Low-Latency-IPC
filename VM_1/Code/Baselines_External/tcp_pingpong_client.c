#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

static inline long now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <msg_size> <count>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    int count = atoi(argv[4]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    printf("[TCP-CLIENT] ping-pong to %s:%d  msg=%dB  count=%d\n",
           ip, port, msg_size, count);

    char *buf = malloc(msg_size);
    memset(buf, 'A', msg_size);

    FILE *fp = fopen("tcp_pingpong.csv", "w");

    for (int i = 0; i < count; i++) {
        long t1 = now_ns();
        send(sock, buf, msg_size, 0);

        int n = recv(sock, buf, msg_size, MSG_WAITALL);
        if (n != msg_size) { perror("recv"); break; }

        long t2 = now_ns();
        fprintf(fp, "%ld\n", t2 - t1);
    }

    fclose(fp);
    close(sock);
    free(buf);

    printf("[TCP-CLIENT] CSV written: tcp_pingpong.csv\n");
    return 0;
}
