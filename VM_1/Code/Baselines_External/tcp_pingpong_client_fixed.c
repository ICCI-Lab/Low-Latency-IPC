#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

static inline long now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <ip> <port> <msg_size> <count>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    int count = atoi(argv[4]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));

    int buf = 256*1024;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    char *out = malloc(msg_size);
    char *in  = malloc(msg_size);
    memset(out, 'A', msg_size);

    FILE *csv = fopen("tcp_pingpong.csv", "w");

    for (int i = 0; i < count; i++) {
        long t1 = now_ns();
        send(sock, out, msg_size, MSG_NOSIGNAL);
        recv(sock, in, msg_size, MSG_WAITALL);
        long t2 = now_ns();
        fprintf(csv, "%ld\n", t2 - t1);
    }

    fclose(csv);
    printf("[TCP-CLIENT] CSV written: tcp_pingpong.csv\n");
    return 0;
}
