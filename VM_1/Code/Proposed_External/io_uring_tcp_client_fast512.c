// io_uring_tcp_client_fast512.c  (FAST64-style, fixed msg_size=512)
#define _GNU_SOURCE
#include <arpa/inet.h>
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
    if (sched_setaffinity(0, sizeof(set), &set) != 0) perror("sched_setaffinity (warning)");
}

static inline unsigned long long nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr,
            "Usage: %s <server_ip> <port> <iters> <warmup> <cpu_core>\n"
            "Example: %s 192.168.250.10 6000 20000 2000 2 > fast512_512B.csv\n",
            argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int port   = atoi(argv[2]);
    int iters  = atoi(argv[3]);
    int warmup = atoi(argv[4]);
    int cpu    = atoi(argv[5]);

    const int msg_size = 512;

    pin_to_cpu(cpu);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) die("socket");

    int opt = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) die("inet_pton");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("connect");

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    if (io_uring_queue_init_params(256, &ring, &params) < 0) die("io_uring_queue_init_params");

    int fds[1] = { sock };
    if (io_uring_register_files(&ring, fds, 1) < 0) die("io_uring_register_files");

    void *buf = NULL;
    if (posix_memalign(&buf, 64, (size_t)msg_size) != 0) die("posix_memalign");
    memset(buf, 0, (size_t)msg_size);

    fprintf(stderr,
        "# FAST512 (FAST64-style): server=%s port=%d iters=%d warmup=%d cpu=%d\n",
        server_ip, port, iters, warmup, cpu);

    printf("seq,rtt_ns\n");

    uint64_t seq = 0;
    int total = warmup + iters;

    for (int i = 0; i < total; ++i) {
        struct io_uring_sqe *sqe_send = io_uring_get_sqe(&ring);
        struct io_uring_sqe *sqe_recv = io_uring_get_sqe(&ring);
        if (!sqe_send || !sqe_recv) die("io_uring_get_sqe");

        seq++;
        memcpy(buf, &seq, sizeof(seq));

        io_uring_prep_send(sqe_send, 0, buf, msg_size, 0);
        sqe_send->flags = IOSQE_FIXED_FILE | IOSQE_IO_LINK;

        io_uring_prep_recv(sqe_recv, 0, buf, msg_size, 0);
        sqe_recv->flags = IOSQE_FIXED_FILE;

        unsigned long long t0 = nsec_now();

        int sret = io_uring_submit(&ring);
        if (sret < 0) die("io_uring_submit");
        if (sret != 2) fprintf(stderr, "submitted only %d SQEs\n", sret);

        struct io_uring_cqe *cqe = NULL;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe");

        int res = cqe->res;
        io_uring_cqe_seen(&ring, cqe);
        if (res < 0) die("IO recv (linked)");

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
