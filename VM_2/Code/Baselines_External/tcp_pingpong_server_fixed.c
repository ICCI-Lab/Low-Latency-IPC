#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <msg_size>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int msg_size = atoi(argv[2]);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int one = 1;
    setsockopt(srv, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    int buf = 256 * 1024;
    setsockopt(srv, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
    setsockopt(srv, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    listen(srv, 1);
    printf("[TCP-SERVER] listening on %d msg=%dB\n", port, msg_size);

    int cli = accept(srv, NULL, NULL);
    if (cli < 0) { perror("accept"); return 1; }

    setsockopt(cli, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(cli, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));

    char *buf_in = malloc(msg_size);
    char *buf_out = malloc(msg_size);
    memset(buf_out, 'A', msg_size);

    while (1) {
        ssize_t n = recv(cli, buf_in, msg_size, MSG_WAITALL);
        if (n <= 0) break;
        send(cli, buf_out, msg_size, MSG_NOSIGNAL);
    }

    return 0;
}
