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

#include "shm_rings.h"

#define SHM_NAME   "/ipc_fastpath"
#define SHM_SIZE   sizeof(shm_region_t)

#define MSG_SIZE   64
#define ITER       100000

// pin thread on a given CPU
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
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int main(void) {
    printf("[pp_producer] starting...\n");

    // همیشه از صفر بساز؛ قبلی اگر بود پاک کن
    shm_unlink(SHM_NAME);

    int fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(fd, SHM_SIZE) != 0) {
        perror("ftruncate");
        return 1;
    }

    void *region = mmap(NULL, SHM_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    shm_region_t *shm = (shm_region_t *) region;
    memset(shm, 0, sizeof(*shm));  // صفر کردن ringها و داده

    pin_cpu(1);  // producer روی CPU1

    uint64_t *lat = malloc(sizeof(uint64_t) * ITER);
    if (!lat) {
        perror("malloc");
        return 1;
    }

    char payload[MSG_SIZE];
    memset(payload, 'P', MSG_SIZE);

    // کمی warm-up
    for (int i = 0; i < 1000; i++) {
        uint32_t idx = i % NUM_CHUNKS;
        memcpy(shm->umem.data[idx], payload, MSG_SIZE);

        while (ring_push(&shm->rx, idx) != 0) {
            // full → spin
        }
        uint32_t ack;
        while (ring_pop(&shm->comp, &ack) != 0) {
            // empty → spin
        }
    }

    // بنچمارک اصلی
    for (int i = 0; i < ITER; i++) {
        uint32_t idx = i % NUM_CHUNKS;
        memcpy(shm->umem.data[idx], payload, MSG_SIZE);

        uint64_t t0 = nsec_now();

        // درخواست → consumer
        while (ring_push(&shm->rx, idx) != 0) {
            // full → spin
        }

        // منتظر ack
        uint32_t ack;
        while (ring_pop(&shm->comp, &ack) != 0) {
            // empty → spin
        }

        uint64_t t1 = nsec_now();
        lat[i] = t1 - t0;
    }

    // مرتب‌سازی برای p50/p95/p99
    qsort(lat, ITER, sizeof(uint64_t), cmp_u64);

    uint64_t p50 = lat[(size_t)(ITER * 0.50)];
    uint64_t p95 = lat[(size_t)(ITER * 0.95)];
    uint64_t p99 = lat[(size_t)(ITER * 0.99)];

    // میانگین
    long double sum = 0.0;
    for (int i = 0; i < ITER; i++) {
        sum += lat[i];
    }
    long double avg = sum / ITER;

    printf("[pp_producer] RTT ns: p50=%" PRIu64 ", p95=%" PRIu64 ", p99=%" PRIu64 ", avg=%.1Lf\n",
           p50, p95, p99, avg);

    // ذخیره CSV برای فصل پنج
    const char *csv_path = "../../Data/internal_fastpath_pingpong.csv";
    FILE *f = fopen(csv_path, "w");
    if (!f) {
        perror("fopen csv");
        return 1;
    }

    fprintf(f, "iter,lat_ns\n");
    for (int i = 0; i < ITER; i++) {
        fprintf(f, "%d,%" PRIu64 "\n", i, lat[i]);
    }
    fclose(f);

    printf("[pp_producer] CSV written to %s\n", csv_path);

    free(lat);
    return 0;
}
