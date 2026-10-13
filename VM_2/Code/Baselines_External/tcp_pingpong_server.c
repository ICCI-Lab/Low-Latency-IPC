#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <msg_size>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int msg_size = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    if (listen(sock, 1) < 0) {
        perror("listen"); return 1;
    }

    printf("[TCP-SERVER] listening on %d, msg_size=%dB ...\n", port, msg_size);

    struct sockaddr_in cli;
    socklen_t clen = sizeof(cli);
    int conn = accept(sock, (struct sockaddr*)&cli, &clen);
    if (conn < 0) { perror("accept"); return 1; }

    char *buf = malloc(msg_size);
    if (!buf) { perror("malloc"); return 1; }

    while (1) {
        int n = recv(conn, buf, msg_size, MSG_WAITALL);
        if (n <= 0) break;
        if (send(conn, buf, msg_size, 0) != msg_size) break;
    }

    close(conn);
    close(sock);
    free(buf);
    return 0;
}

