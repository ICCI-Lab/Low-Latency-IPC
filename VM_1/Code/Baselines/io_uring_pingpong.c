#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <liburing.h>

#define ITER    100000
#define WARMUP  1000

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// read/write کمکی برای child (blocking کامل روی msg_size)
static ssize_t read_full(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t r = read(fd, (char*)buf + off, len - off);
        if (r <= 0) return r;
        off += (size_t)r;
    }
    return (ssize_t)off;
}

static ssize_t write_full(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, (const char*)buf + off, len - off);
        if (w <= 0) return w;
        off += (size_t)w;
    }
    return (ssize_t)off;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s msg_size\n", argv[0]);
        return 1;
    }

    int msg_size = atoi(argv[1]);
    if (msg_size <= 0 || msg_size > 65536) {
        fprintf(stderr, "msg_size must be 1..65536\n");
        return 1;
    }

    printf("[io_uring] msg_size=%d ...\n", msg_size);

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit(1);
    }

    char *buf = malloc(msg_size);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    memset(buf, 'A', msg_size);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        exit(1);
    }

    if (child == 0) {
        // ---------------- CHILD: read/write ساده با read_full/write_full ----------------
        close(sv[0]);
        for (;;) {
            ssize_t r = read_full(sv[1], buf, msg_size);
            if (r <= 0) break;
            ssize_t w = write_full(sv[1], buf, msg_size);
            if (w <= 0) break;
        }
        exit(0);
    }

    // ---------------- PARENT: io_uring روی sv[0] ----------------
    close(sv[1]);

    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        exit(1);
    }

    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;

    // ---- WARMUP ----
    for (int i = 0; i < WARMUP; i++) {
        // write_full با io_uring
        size_t off = 0;
        while (off < (size_t)msg_size) {
            size_t chunk = msg_size - off;
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_write(sqe, sv[0], buf + off, chunk, 0);
            io_uring_submit(&ring);
            io_uring_wait_cqe(&ring, &cqe);
            if (cqe->res <= 0) {
                fprintf(stderr, "warmup write error: %d\n", cqe->res);
                io_uring_cqe_seen(&ring, cqe);
                goto out;
            }
            off += (size_t)cqe->res;
            io_uring_cqe_seen(&ring, cqe);
        }

        // read_full با io_uring
        off = 0;
        while (off < (size_t)msg_size) {
            size_t chunk = msg_size - off;
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_read(sqe, sv[0], buf + off, chunk, 0);
            io_uring_submit(&ring);
            io_uring_wait_cqe(&ring, &cqe);
            if (cqe->res <= 0) {
                fprintf(stderr, "warmup read error: %d\n", cqe->res);
                io_uring_cqe_seen(&ring, cqe);
                goto out;
            }
            off += (size_t)cqe->res;
            io_uring_cqe_seen(&ring, cqe);
        }
    }

    uint64_t *lat = malloc(sizeof(uint64_t) * ITER);
    if (!lat) {
        perror("malloc lat");
        goto out;
    }

    // ---- MEASURE ----
    for (int i = 0; i < ITER; i++) {
        uint64_t t0 = now_ns();

        // write_full با io_uring
        size_t off = 0;
        while (off < (size_t)msg_size) {
            size_t chunk = msg_size - off;
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_write(sqe, sv[0], buf + off, chunk, 0);
            io_uring_submit(&ring);
            io_uring_wait_cqe(&ring, &cqe);
            if (cqe->res <= 0) {
                fprintf(stderr, "write error: %d\n", cqe->res);
                io_uring_cqe_seen(&ring, cqe);
                goto out_lat;
            }
            off += (size_t)cqe->res;
            io_uring_cqe_seen(&ring, cqe);
        }

        // read_full با io_uring
        off = 0;
        while (off < (size_t)msg_size) {
            size_t chunk = msg_size - off;
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_read(sqe, sv[0], buf + off, chunk, 0);
            io_uring_submit(&ring);
            io_uring_wait_cqe(&ring, &cqe);
            if (cqe->res <= 0) {
                fprintf(stderr, "read error: %d\n", cqe->res);
                io_uring_cqe_seen(&ring, cqe);
                goto out_lat;
            }
            off += (size_t)cqe->res;
            io_uring_cqe_seen(&ring, cqe);
        }

        uint64_t t1 = now_ns();
        lat[i] = t1 - t0;
    }

    // ---- SORT (ساده) ----
    for (int i = 0; i < ITER - 1; i++) {
        for (int j = i + 1; j < ITER; j++) {
            if (lat[j] < lat[i]) {
                uint64_t tmp = lat[i];
                lat[i] = lat[j];
                lat[j] = tmp;
            }
        }
    }

    {
        uint64_t p50 = lat[(int)(ITER * 0.50)];
        uint64_t p95 = lat[(int)(ITER * 0.95)];
        uint64_t p99 = lat[(int)(ITER * 0.99)];
        long double avg = 0;
        for (int i = 0; i < ITER; i++) avg += lat[i];
        avg /= ITER;

        printf("[io_uring] p50=%lu ns, p95=%lu ns, p99=%lu ns, avg=%.1Lf ns\n",
               p50, p95, p99, avg);
    }

out_lat:
    free(lat);
out:
    kill(child, SIGKILL);
    io_uring_queue_exit(&ring);
    free(buf);
    return 0;
}
