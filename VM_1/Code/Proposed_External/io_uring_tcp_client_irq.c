#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
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

static inline unsigned long long nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static int wait_two_cqes(struct io_uring *ring,
                         struct io_uring_cqe **cqe1,
                         struct io_uring_cqe **cqe2) {
    int got = 0;
    int ret;

    *cqe1 = NULL;
    *cqe2 = NULL;

    // aggressive spin first
    for (int spin = 0; spin < 200 && got < 2; ++spin) {
        ret = io_uring_peek_cqe(ring, cqe1);
        if (ret == 0 && *cqe1) {
            got++;
            break;
        } else if (ret != -EAGAIN && ret != 0) {
            return ret;
        }
    }
    if (got == 0) {
        ret = io_uring_wait_cqe(ring, cqe1);
        if (ret < 0) return ret;
        got = 1;
    }

    // now wait for second CQE
    ret = io_uring_wait_cqe(ring, cqe2);
    if (ret < 0) return ret;
    got++;

    return got == 2 ? 0 : -1;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr,
                "Usage: %s <server_ip> <port> <msg_size> <iters> <warmup> <cpu_core>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    int iters = atoi(argv[4]);
    int warmup = atoi(argv[5]);
    int cpu_core = atoi(argv[6]);

    if (msg_size < (int)sizeof(unsigned long long)) {
        fprintf(stderr, "msg_size must be >= %zu\n", sizeof(unsigned long long));
        return EXIT_FAILURE;
    }

    pin_to_cpu(cpu_core);

    // --- create TCP socket ---
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) die("socket");

    int opt = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        perror("setsockopt TCP_NODELAY");

#ifdef SO_BUSY_POLL
    int busy_poll = 50; // microseconds
    setsockopt(sock, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));
#endif
#ifdef SO_BUSY_POLL_BUDGET
    int busy_budget = 64;
    setsockopt(sock, SOL_SOCKET, SO_BUSY_POLL_BUDGET, &busy_budget, sizeof(busy_budget));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        die("inet_pton");
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        die("connect");
    }

    // --- setup io_uring ---
    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    unsigned int entries = 256;
    if (io_uring_queue_init_params(entries, &ring, &params) < 0) {
        die("io_uring_queue_init_params");
        return EXIT_FAILURE;
    }

    // register socket as fixed file index=0
    int fds[1] = { sock };
    if (io_uring_register_files(&ring, fds, 1) < 0) {
        die("io_uring_register_files");
    }

    // allocate message buffer
    void *buf = NULL;
    if (posix_memalign(&buf, 64, (size_t)msg_size) != 0) {
        die("posix_memalign");
    }
    memset(buf, 0, (size_t)msg_size);

    fprintf(stderr,
            "# io_uring TCP client\n"
            "# server=%s port=%d msg_size=%d iters=%d warmup=%d cpu=%d\n",
            server_ip, port, msg_size, iters, warmup, cpu_core);

    // CSV header
    printf("seq,rtt_ns\n");

    unsigned long long seq = 0;

    int total = warmup + iters;
    for (int i = 0; i < total; ++i) {
        struct io_uring_sqe *sqe_send = io_uring_get_sqe(&ring);
        if (!sqe_send) die("io_uring_get_sqe send");

        struct io_uring_sqe *sqe_recv = io_uring_get_sqe(&ring);
        if (!sqe_recv) die("io_uring_get_sqe recv");

        seq++;
        memcpy(buf, &seq, sizeof(seq));

        io_uring_prep_send(sqe_send, 0, buf, msg_size, 0);
        sqe_send->flags |= IOSQE_FIXED_FILE;

        io_uring_prep_recv(sqe_recv, 0, buf, msg_size, 0);
        sqe_recv->flags |= IOSQE_FIXED_FILE;

        unsigned long long t0 = nsec_now();

        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            die("io_uring_submit");
        } else if (ret != 2) {
            fprintf(stderr, "submit only %d SQEs\n", ret);
        }

        struct io_uring_cqe *cqe1 = NULL, *cqe2 = NULL;
        ret = wait_two_cqes(&ring, &cqe1, &cqe2);
        if (ret < 0) {
            fprintf(stderr, "wait_two_cqes: %d\n", ret);
            die("wait_two_cqes");
        }

        int res1 = cqe1->res;
        int res2 = cqe2->res;

        io_uring_cqe_seen(&ring, cqe1);
        io_uring_cqe_seen(&ring, cqe2);

        if (res1 < 0 || res2 < 0) {
            fprintf(stderr, "IO error: send_res=%d recv_res=%d\n", res1, res2);
            die("IO");
        }

        unsigned long long t1 = nsec_now();
        unsigned long long rtt = t1 - t0;

        if (i >= warmup) {
            int logical_seq = i - warmup + 1;
            printf("%d,%llu\n", logical_seq, (unsigned long long)rtt);
        }
    }

    io_uring_unregister_files(&ring);
    io_uring_queue_exit(&ring);
    close(sock);
    free(buf);

    return 0;
}
