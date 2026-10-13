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
    if (port <= 0 || msg_size <= 0) {
        fprintf(stderr, "Invalid port or msg_size\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("[UDP-SERVER] listening on port %d, msg_size=%dB ...\n", port, msg_size);
    fflush(stdout);

    char *buf = malloc(msg_size);
    if (!buf) {
        perror("malloc");
        close(sock);
        return 1;
    }

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, msg_size, 0,
                             (struct sockaddr *)&cli, &clen);
        if (n < 0) {
            perror("recvfrom");
            break;
        }
        if (sendto(sock, buf, n, 0,
                   (struct sockaddr *)&cli, clen) < 0) {
            perror("sendto");
            break;
        }
    }

    free(buf);
    close(sock);
    return 0;
}
