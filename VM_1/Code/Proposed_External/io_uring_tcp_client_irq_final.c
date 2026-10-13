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
#include <time.h>
#include <unistd.h>

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

static void pin_to_cpu(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        perror("sched_setaffinity (warning)");
}

static inline unsigned long long nsec_now(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) die("clock_gettime");
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

static void set_sock_opts(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef TCP_QUICKACK
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));
#endif
#ifdef SO_BUSY_POLL
    int busy_poll = 50; // usec
    setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));
#endif
#ifdef SO_BUSY_POLL_BUDGET
    int budget = 64;
    setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL_BUDGET, &budget, sizeof(budget));
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr,
            "Usage: %s <server_ip> <port> <msg_size> <iters> <warmup> <cpu_core>\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int port   = atoi(argv[2]);
    int msize  = atoi(argv[3]);
    int iters  = atoi(argv[4]);
    int warmup = atoi(argv[5]);
    int cpu    = atoi(argv[6]);

    if (msize <= 0) { fprintf(stderr, "msg_size must be > 0\n"); return EXIT_FAILURE; }

    pin_to_cpu(cpu);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) die("socket");
    set_sock_opts(sock);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) die("inet_pton");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("connect");

    struct io_uring ring;
    struct io_uring_params p;
    memset(&p, 0, sizeof(p));
    if (io_uring_queue_init_params(256, &ring, &p) < 0) die("io_uring_queue_init_params");

    int fds[1] = { sock };
    if (io_uring_register_files(&ring, fds, 1) < 0) die("io_uring_register_files");

    void *buf = NULL;
    if (posix_memalign(&buf, 64, (size_t)msize) != 0) die("posix_memalign");
    memset(buf, 0, (size_t)msize);

    // خروجي سريع و بدون بافر
    setvbuf(stdout, NULL, _IONBF, 0);

    fprintf(stderr,
        "# client_irq_final: ip=%s port=%d msg=%d iters=%d warmup=%d cpu=%d\n",
        server_ip, port, msize, iters, warmup, cpu);

    printf("seq,rtt_ns\n");

    unsigned long long seq = 0;
    int total = warmup + iters;

    for (int i = 0; i < total; ++i) {
        seq++;
        memcpy(buf, &seq, sizeof(seq));

        unsigned long long t0 = nsec_now();

        // ---- SEND (handle partial sends) ----
        size_t sent = 0;
        while (sent < (size_t)msize) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            if (!sqe) die("io_uring_get_sqe(send)");
            io_uring_prep_send(sqe, 0, (char*)buf + sent, (unsigned)((size_t)msize - sent), MSG_NOSIGNAL);
            sqe->flags = IOSQE_FIXED_FILE;
            sqe->user_data = 1;

            if (io_uring_submit_and_wait(&ring, 1) < 0) die("submit_and_wait(send)");

            struct io_uring_cqe *cqe = NULL;
            if (io_uring_wait_cqe(&ring, &cqe) < 0) die("wait_cqe(send)");
            int res = cqe->res;
            io_uring_cqe_seen(&ring, cqe);

            if (res < 0) { errno = -res; die("send res"); }
            if (res == 0) die("send returned 0");
            sent += (size_t)res;
        }

        // ---- RECV (handle partial recvs) ----
        size_t recvd = 0;
        while (recvd < (size_t)msize) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            if (!sqe) die("io_uring_get_sqe(recv)");
            // بدون MSG_WAITALL: هر چي آمد بگير، خودمان جمع ميکنيم
            io_uring_prep_recv(sqe, 0, (char*)buf + recvd, (unsigned)((size_t)msize - recvd), 0);
            sqe->flags = IOSQE_FIXED_FILE;
            sqe->user_data = 2;

            if (io_uring_submit_and_wait(&ring, 1) < 0) die("submit_and_wait(recv)");

            struct io_uring_cqe *cqe = NULL;
            if (io_uring_wait_cqe(&ring, &cqe) < 0) die("wait_cqe(recv)");
            int res = cqe->res;
            io_uring_cqe_seen(&ring, cqe);

            if (res < 0) { errno = -res; die("recv res"); }
            if (res == 0) die("server closed");
            recvd += (size_t)res;
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
