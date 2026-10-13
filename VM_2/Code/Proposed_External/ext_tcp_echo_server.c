// ext_tcp_echo_server.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void pin_to_cpu(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity (warning)");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <msg_size> <cpu_core>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    int msg_size = atoi(argv[2]);
    int cpu_core = atoi(argv[3]);

    pin_to_cpu(cpu_core);

    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) die("socket");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(listen_fd, 1) < 0)
        die("listen");

    fprintf(stderr, "Listening on port %d, msg_size=%d, cpu=%d\n",
            port, msg_size, cpu_core);

    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);
    int conn = accept(listen_fd, (struct sockaddr *)&cli, &cli_len);
    if (conn < 0) die("accept");

    setsockopt(conn, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    void *buf = malloc((size_t)msg_size);
    if (!buf) die("malloc");
    memset(buf, 0, (size_t)msg_size);

    for (;;) {
        ssize_t r = recv(conn, buf, (size_t)msg_size, MSG_WAITALL);
        if (r <= 0) break;
        ssize_t off = 0;
        while (off < r) {
            ssize_t w = send(conn, (char *)buf + off, (size_t)(r - off), 0);
            if (w <= 0) die("send");
            off += w;
        }
    }

    free(buf);
    close(conn);
    close(listen_fd);
    return 0;
}
