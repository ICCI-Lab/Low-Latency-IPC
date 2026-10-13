// ext_tcp_client_fixed.c  -- simple io_uring + registered buffer
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <liburing.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static long long nsec_now(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0)
        die("clock_gettime");
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static int connect_tcp(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for %s\n", ip);
        exit(1);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("connect");

    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    return fd;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr,
                "Usage: %s <server_ip> <port> <msg_size> <iters> <warmup>\n"
                "Example: %s 192.168.174.130 6000 64 20000 2000 > out.csv\n",
                argv[0], argv[0]);
        return 1;
    }

    const char *ip   = argv[1];
    int port         = atoi(argv[2]);
    size_t msize     = strtoul(argv[3], NULL, 10);
    long iters       = strtol(argv[4], NULL, 10);
    long warmup      = strtol(argv[5], NULL, 10);

    int fd = connect_tcp(ip, port);
    fprintf(stderr,
            "# ext_tcp_client_fixed: ip=%s port=%d msize=%zu iters=%ld warmup=%ld\n",
            ip, port, msize, iters, warmup);

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    int ret = io_uring_queue_init_params(256, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        return 1;
    }

    // يك بافر واحد ثبت‌شده
    void *buf;
    int rc = posix_memalign(&buf, 4096, msize);
    if (rc != 0) {
        fprintf(stderr, "posix_memalign: %s\n", strerror(rc));
        return 1;
    }

    memset(buf, 0xAB, msize);

    struct iovec iov[1];
    iov[0].iov_base = buf;
    iov[0].iov_len  = msize;

    ret = io_uring_register_buffers(&ring, iov, 1);
    if (ret < 0) {
        fprintf(stderr, "io_uring_register_buffers: %s\n", strerror(-ret));
        return 1;
    }

    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    // --- WARMUP ---
    for (long i = 0; i < warmup; i++) {
        // SEND
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(send,warmup)");

        io_uring_prep_write_fixed(
            sqe,
            fd,
            buf,
            msize,
            0,
            0
        );
        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(send,warmup)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(send,warmup)");
        if (cqe->res < 0) {
            fprintf(stderr, "warmup send res=%d\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);
            goto out;
        }
        io_uring_cqe_seen(&ring, cqe);

        // RECV
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(recv,warmup)");

        io_uring_prep_read_fixed(
            sqe,
            fd,
            buf,
            msize,
            0,
            0
        );
        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(recv,warmup)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(recv,warmup)");
        if (cqe->res <= 0) {
            fprintf(stderr, "warmup recv res=%d\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);
            goto out;
        }
        io_uring_cqe_seen(&ring, cqe);
    }

    // --- MEASURE ---
    for (long i = 0; i < iters; i++) {
        long long t0 = nsec_now();

        // SEND
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(send)");

        io_uring_prep_write_fixed(
            sqe,
            fd,
            buf,
            msize,
            0,
            0
        );
        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(send)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(send)");
        if (cqe->res < 0) {
            fprintf(stderr, "send res=%d\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);
            goto out;
        }
        io_uring_cqe_seen(&ring, cqe);

        // RECV
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(recv)");

        io_uring_prep_read_fixed(
            sqe,
            fd,
            buf,
            msize,
            0,
            0
        );
        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(recv)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(recv)");
        if (cqe->res <= 0) {
            fprintf(stderr, "recv res=%d\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);
            goto out;
        }
        io_uring_cqe_seen(&ring, cqe);

        long long t1 = nsec_now();
        long long rtt = t1 - t0;

        printf("%lld\n", rtt);
    }

out:
    io_uring_unregister_buffers(&ring);
    io_uring_queue_exit(&ring);
    close(fd);
    free(buf);

    return 0;
}
