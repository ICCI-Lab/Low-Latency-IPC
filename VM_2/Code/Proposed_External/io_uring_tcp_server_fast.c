#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <signal.h>
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
    if (cpu < 0)
        return;
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
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(fd, 1024) < 0)
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

    signal(SIGPIPE, SIG_IGN);

    pin_to_cpu(cpu_core);

    int listen_fd = make_listen_socket(port);
    fprintf(stderr, "# io_uring TCP server FAST on port %d, msg_size=%d, cpu=%d\n",
            port, msg_size, cpu_core);

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
    if (conn_fd < 0) die("accept");

    int opt = 1;
    if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_NODELAY(conn)");
    if (setsockopt(conn_fd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_QUICKACK(conn)");

#ifdef SO_BUSY_POLL
    int busy_poll = 50; // microseconds
    setsockopt(conn_fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));
#endif
#ifdef SO_BUSY_POLL_BUDGET
    int busy_budget = 64;
    setsockopt(conn_fd, SOL_SOCKET, SO_BUSY_POLL_BUDGET, &busy_budget, sizeof(busy_budget));
#endif

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    unsigned int entries = 256;
    if (io_uring_queue_init_params(entries, &ring, &params) < 0) {
        die("io_uring_queue_init_params");
    }

    int fds[1] = { conn_fd };
    if (io_uring_register_files(&ring, fds, 1) < 0) {
        die("io_uring_register_files");
    }

    void *buf = NULL;
    if (posix_memalign(&buf, 64, (size_t)msg_size) != 0) {
        die("posix_memalign");
    }
    memset(buf, 0, (size_t)msg_size);

    fprintf(stderr, "# accepted client, starting echo loop\n");

    for (;;) {
        struct io_uring_sqe *sqe_recv = io_uring_get_sqe(&ring);
        if (!sqe_recv) die("io_uring_get_sqe recv");

        struct io_uring_sqe *sqe_send = io_uring_get_sqe(&ring);
        if (!sqe_send) die("io_uring_get_sqe send");

        io_uring_prep_recv(sqe_recv, 0, buf, msg_size, MSG_WAITALL);
        sqe_recv->flags     = IOSQE_FIXED_FILE | IOSQE_IO_LINK;
        sqe_recv->user_data = 1;

        io_uring_prep_send(sqe_send, 0, buf, msg_size, 0);
        sqe_send->flags     = IOSQE_FIXED_FILE;
        sqe_send->user_data = 2;

        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            die("io_uring_submit");
        } else if (ret != 2) {
            fprintf(stderr, "submit only %d SQEs\n", ret);
        }

        int got_recv = 0, got_send = 0;
        while (!got_recv || !got_send) {
            struct io_uring_cqe *cqe = NULL;
            ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) {
                fprintf(stderr, "io_uring_wait_cqe: %d\n", ret);
                die("io_uring_wait_cqe");
            }

            int res = cqe->res;
            unsigned long long ud = cqe->user_data;

            if (res <= 0 && ud == 1) {
                io_uring_cqe_seen(&ring, cqe);
                fprintf(stderr, "# recv res=%d, closing\n", res);
                goto out;
            }
            if (res < 0) {
                fprintf(stderr, "IO error: ud=%llu res=%d\n", ud, res);
                io_uring_cqe_seen(&ring, cqe);
                goto out;
            }

            if (ud == 1) got_recv = 1;
            else if (ud == 2) got_send = 1;

            io_uring_cqe_seen(&ring, cqe);
        }
    }

out:
    io_uring_unregister_files(&ring);
    io_uring_queue_exit(&ring);
    close(conn_fd);
    close(listen_fd);
    free(buf);
    return 0;
}
