// io_uring_tcp_server_final.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <stdint.h>
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

static int make_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) die("socket");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror("setsockopt SO_REUSEADDR");
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_NODELAY");
    if (setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_QUICKACK");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(fd, 128) < 0)
        die("listen");

    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <port> <msg_size> <cpu_core>\n"
                "Example: %s 6000 64 2\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    int port     = atoi(argv[1]);
    int msg_size = atoi(argv[2]);
    int cpu_core = atoi(argv[3]);

    if (msg_size <= 0) {
        fprintf(stderr, "msg_size must be > 0\n");
        return EXIT_FAILURE;
    }

    pin_to_cpu(cpu_core);

    int lfd = make_listen_socket(port);
    fprintf(stderr,
            "# server_final: listening on port %d, msg_size=%d, cpu=%d\n",
            port, msg_size, cpu_core);

    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &clilen);
    if (cfd < 0) die("accept");

    int opt = 1;
    if (setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_NODELAY(conn)");
    if (setsockopt(cfd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_QUICKACK(conn)");

#ifdef SO_BUSY_POLL
    int busy_poll = 50; // usec
    setsockopt(cfd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));
#endif
#ifdef SO_BUSY_POLL_BUDGET
    int busy_budget = 64;
    setsockopt(cfd, SOL_SOCKET, SO_BUSY_POLL_BUDGET, &busy_budget, sizeof(busy_budget));
#endif

    fprintf(stderr, "# server_final: client connected\n");

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    unsigned int entries = 256;
    if (io_uring_queue_init_params(entries, &ring, &params) < 0)
        die("io_uring_queue_init_params");

    int fds[1] = { cfd };
    if (io_uring_register_files(&ring, fds, 1) < 0)
        die("io_uring_register_files");

    void *buf = NULL;
    if (posix_memalign(&buf, 64, (size_t)msg_size) != 0)
        die("posix_memalign");
    memset(buf, 0, (size_t)msg_size);

    for (;;) {
        struct io_uring_sqe *sqe_recv = io_uring_get_sqe(&ring);
        struct io_uring_sqe *sqe_send = io_uring_get_sqe(&ring);
        if (!sqe_recv || !sqe_send)
            die("io_uring_get_sqe");

        // RECV → IO_LINK → SEND
        io_uring_prep_recv(sqe_recv, 0, buf, msg_size, MSG_WAITALL);
        sqe_recv->flags     = IOSQE_FIXED_FILE | IOSQE_IO_LINK;
        sqe_recv->user_data = 1;

        io_uring_prep_send(sqe_send, 0, buf, msg_size, 0);
        sqe_send->flags     = IOSQE_FIXED_FILE;
        sqe_send->user_data = 2;

        int submit_ret = io_uring_submit(&ring);
        if (submit_ret < 0)
            die("io_uring_submit");
        if (submit_ret != 2)
            fprintf(stderr, "server_final: submitted only %d SQEs\n", submit_ret);

        int got_recv = 0, got_send = 0;

        while (!got_recv || !got_send) {
            struct io_uring_cqe *cqe = NULL;
            int ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0)
                die("io_uring_wait_cqe");

            int res = cqe->res;
            uint64_t ud = cqe->user_data;

            if (ud == 1) { // RECV
                if (res == 0) {
                    io_uring_cqe_seen(&ring, cqe);
                    goto out;
                }
                if (res < 0) {
                    fprintf(stderr, "server_final: recv error res=%d\n", res);
                    io_uring_cqe_seen(&ring, cqe);
                    goto out;
                }
                if (res != msg_size) {
                    fprintf(stderr,
                            "server_final: short recv %d/%d\n",
                            res, msg_size);
                }
                got_recv = 1;
            } else if (ud == 2) { // SEND
                if (res < 0) {
                    fprintf(stderr, "server_final: send error res=%d\n", res);
                    io_uring_cqe_seen(&ring, cqe);
                    goto out;
                }
                got_send = 1;
            }

            io_uring_cqe_seen(&ring, cqe);
        }
    }

out:
    fprintf(stderr, "# server_final: closing.\n");
    io_uring_unregister_files(&ring);
    io_uring_queue_exit(&ring);
    close(cfd);
    close(lfd);
    free(buf);
    return 0;
}
