#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>

#include "shm_rings.h"

#define SHM_NAME   "/ipc_fastpath"
#define SHM_SIZE   sizeof(shm_region_t)
#define ITER       100000        // تعداد پیام‌ها در فاز اندازه‌گیری
#define WARMUP     1000          // تعداد پیام‌های warm-up

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
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
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
    if (msg_size < 1 || msg_size > CHUNK_SIZE) {
        printf("msg_size must be between 1 and %d\n", CHUNK_SIZE);
        return 1;
    }

    printf("[pp_param] starting with msg_size=%d ...\n", msg_size);

    // shm تازه
    shm_unlink(SHM_NAME);
    int fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, SHM_SIZE) != 0) { perror("ftruncate"); return 1; }

    void *region = mmap(NULL, SHM_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);
    if (region == MAP_FAILED) { perror("mmap"); return 1; }

    shm_region_t *shm = (shm_region_t *)region;
    memset(shm, 0, sizeof(*shm));

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        // --------------------------
        //        CHILD (consumer)
        // --------------------------
        pin_cpu(2);

        int total = WARMUP + ITER;
        for (int i = 0; i < total; i++) {
            uint32_t idx;

            // از RX بخوان
            while (ring_pop(&shm->rx, &idx) != 0) {
                // spin تا producer چیزی بفرستد
            }

            // ack در completion ring
            while (ring_push(&shm->comp, idx) != 0) {
                // spin تا جا باز شود
            }
        }

        _exit(0);
    }

    // --------------------------
    //    PARENT (producer)
    // --------------------------
    pin_cpu(1);

    uint64_t *lat = malloc(sizeof(uint64_t) * ITER);
    if (!lat) { perror("malloc"); return 1; }

    uint8_t payload[CHUNK_SIZE];
    memset(payload, 'X', sizeof(payload));

    // ---------- warm-up ----------
    for (int i = 0; i < WARMUP; i++) {
        uint32_t idx = i % NUM_CHUNKS;
        memcpy(shm->umem.data[idx], payload, msg_size);

        while (ring_push(&shm->rx, idx) != 0) {
            // ring پر است → spin
        }

        uint32_t ack;
        while (ring_pop(&shm->comp, &ack) != 0) {
            // منتظر ack
        }
    }

    // ---------- فاز اندازه‌گیری ----------
    for (int i = 0; i < ITER; i++) {
        uint32_t idx = i % NUM_CHUNKS;
        memcpy(shm->umem.data[idx], payload, msg_size);

        uint64_t t0 = nsec_now();

        while (ring_push(&shm->rx, idx) != 0) {
            // ring پر است
        }

        uint32_t ack;
        while (ring_pop(&shm->comp, &ack) != 0) {
            // منتظر ack
        }

        uint64_t t1 = nsec_now();
        lat[i] = t1 - t0;
    }

    qsort(lat, ITER, sizeof(uint64_t), cmp_u64);

    uint64_t p50 = lat[(size_t)(ITER * 0.50)];
    uint64_t p95 = lat[(size_t)(ITER * 0.95)];
    uint64_t p99 = lat[(size_t)(ITER * 0.99)];

    long double sum = 0;
    for (int i = 0; i < ITER; i++) sum += lat[i];
    long double avg = sum / ITER;

    char outname[128];
    snprintf(outname, sizeof(outname),
             "../../Data/pingpong_%dB.csv", msg_size);

    FILE *f = fopen(outname, "w");
    if (!f) { perror("fopen"); return 1; }

    fprintf(f, "iter,lat_ns\n");
    for (int i = 0; i < ITER; i++)
        fprintf(f, "%d,%" PRIu64 "\n", i, lat[i]);

    fclose(f);

    printf("[pp_param] msg=%dB → p50=%" PRIu64 " ns, p95=%" PRIu64
           " ns, p99=%" PRIu64 " ns, avg=%.1Lf ns\n",
           msg_size, p50, p95, p99, avg);

    free(lat);
    wait(NULL);

    return 0;
}
