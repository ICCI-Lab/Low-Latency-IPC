#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>

#define ITER    100000
#define WARMUP  1000

static void pin_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
        exit(1);
    }
}

static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s <msg_size>\n", argv[0]);
        return 1;
    }

    int msg_size = atoi(argv[1]);
    printf("[PIPE] msg_size=%d ...\n", msg_size);

    uint8_t *buf = malloc(msg_size);
    memset(buf, 'P', msg_size);

    int p2c[2];   // parent → child
    int c2p[2];   // child → parent

    if (pipe(p2c) < 0) { perror("pipe"); return 1; }
    if (pipe(c2p) < 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        // -------------------
        //      CHILD
        // -------------------
        pin_cpu(2);

        close(p2c[1]); // child reads from p2c[0]
        close(c2p[0]); // child writes to c2p[1]

        // warm-up
        for (int i = 0; i < WARMUP; i++) {
            read(p2c[0], buf, msg_size);
            write(c2p[1], buf, msg_size);
        }

        for (int i = 0; i < ITER; i++) {
            read(p2c[0], buf, msg_size);
            write(c2p[1], buf, msg_size);
        }

        exit(0);
    }

    // -------------------
    //     PARENT
    // -------------------
    pin_cpu(1);

    close(p2c[0]); // parent writes to p2c[1]
    close(c2p[1]); // parent reads from c2p[0]

    uint64_t *lat = malloc(ITER * sizeof(uint64_t));

    // warm-up
    for (int i = 0; i < WARMUP; i++) {
        write(p2c[1], buf, msg_size);
        read(c2p[0], buf, msg_size);
    }

    // measure
    for (int i = 0; i < ITER; i++) {
        uint64_t t0 = nsec_now();
        write(p2c[1], buf, msg_size);
        read(c2p[0], buf, msg_size);
        uint64_t t1 = nsec_now();
        lat[i] = t1 - t0;
    }

    qsort(lat, ITER, sizeof(uint64_t), cmp_u64);

    uint64_t p50 = lat[(int)(ITER * 0.50)];
    uint64_t p95 = lat[(int)(ITER * 0.95)];
    uint64_t p99 = lat[(int)(ITER * 0.99)];

    long double sum = 0;
    for (int i = 0; i < ITER; i++) sum += lat[i];
    long double avg = sum / ITER;

    printf("[PIPE] p50=%" PRIu64 " ns, p95=%" PRIu64 " ns, p99=%" PRIu64 " ns, avg=%.1Lf ns\n",
           p50, p95, p99, avg);

    // CSV output
    char outname[128];
    snprintf(outname, sizeof(outname),
             "../../Data/pipe_%dB.csv", msg_size);

    FILE *f = fopen(outname, "w");
    fprintf(f, "iter,lat_ns\n");
    for (int i = 0; i < ITER; i++)
        fprintf(f, "%d,%" PRIu64 "\n", i, lat[i]);
    fclose(f);

    wait(NULL);
    return 0;
}
