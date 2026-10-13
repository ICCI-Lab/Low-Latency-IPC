#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>

#define ITER    100000
#define WARMUP  1000

static void pin_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
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
    printf("[UDS] msg_size=%d ...\n", msg_size);

    uint8_t *buf = malloc(msg_size);
    memset(buf, 'A', msg_size);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/uds_pingpong.sock");
    unlink("/tmp/uds_pingpong.sock");

    int sv = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(sv, (struct sockaddr *)&addr, sizeof(addr));
    listen(sv, 1);

    pid_t pid = fork();
    if (pid < 0) exit(1);

    if (pid == 0) {
        // -----------------
        // SERVER
        // -----------------
        pin_cpu(2);

        int s2 = socket(AF_UNIX, SOCK_STREAM, 0);
        connect(s2, (struct sockaddr *)&addr, sizeof(addr));

        // warmup
        for (int i = 0; i < WARMUP; i++) {
            read(s2, buf, msg_size);
            write(s2, buf, msg_size);
        }

        // main
        for (int i = 0; i < ITER; i++) {
            read(s2, buf, msg_size);
            write(s2, buf, msg_size);
        }

        exit(0);
    }

    // -----------------
    // CLIENT
    // -----------------
    pin_cpu(1);
    int s = accept(sv, NULL, NULL);

    uint64_t *lat = malloc(sizeof(uint64_t) * ITER);

    // warmup
    for (int i = 0; i < WARMUP; i++) {
        write(s, buf, msg_size);
        read(s, buf, msg_size);
    }

    // main
    for (int i = 0; i < ITER; i++) {
        uint64_t t0 = nsec_now();
        write(s, buf, msg_size);
        read(s, buf, msg_size);
        uint64_t t1 = nsec_now();
        lat[i] = t1 - t0;
    }

    qsort(lat, ITER, sizeof(uint64_t), cmp_u64);

    uint64_t p50 = lat[(int)(ITER*0.5)];
    uint64_t p95 = lat[(int)(ITER*0.95)];
    uint64_t p99 = lat[(int)(ITER*0.99)];

    long double sum = 0;
    for (int i = 0; i < ITER; i++) sum += lat[i];
    long double avg = sum / ITER;

    printf("[UDS] p50=%" PRIu64 " ns, p95=%" PRIu64 " ns, p99=%" PRIu64 " ns, avg=%.1Lf ns\n",
           p50, p95, p99, avg);

    // CSV
    char outname[128];
    snprintf(outname, sizeof(outname),
             "../../Data/uds_%dB.csv", msg_size);

    FILE *f = fopen(outname, "w");
    fprintf(f, "iter,lat_ns\n");
    for (int i = 0; i < ITER; i++)
        fprintf(f, "%d,%" PRIu64 "\n", i, lat[i]);
    fclose(f);

    wait(NULL);
    return 0;
}
