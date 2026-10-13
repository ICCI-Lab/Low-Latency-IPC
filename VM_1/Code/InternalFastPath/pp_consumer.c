#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

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

int main(void) {
    printf("[pp_consumer] starting...\n");

    // producer باید shared-memory را ساخته باشد
    int fd = shm_open(SHM_NAME, O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
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

    pin_cpu(2);   // consumer pinned on CPU2

    for (int i = 0; i < ITER; i++) {

        uint32_t idx;

        // منتظر پیام
        while (ring_pop(&shm->rx, &idx) != 0) {
            // empty → spin
        }

        // ack → producer
        while (ring_push(&shm->comp, idx) != 0) {
            // full → spin
        }
    }

    printf("[pp_consumer] finished.\n");
    return 0;
}
