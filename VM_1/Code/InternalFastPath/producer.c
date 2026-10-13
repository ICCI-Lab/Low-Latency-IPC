#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

#include "shm_rings.h"

#define SHM_NAME   "/ipc_fastpath"
#define SHM_SIZE   sizeof(shm_region_t)
#define MSG_SIZE   64

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

int main() {
    printf("[Producer] starting...\n");

    // Create shared-memory or open if exists
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
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

    pin_cpu(1);  // producer on CPU1

    char msg[MSG_SIZE];
    memset(msg, 'A', MSG_SIZE);

    for (int i = 0; i < 10000; i++) {

        uint32_t idx = i % NUM_CHUNKS;
        memcpy(shm->umem.data[idx], msg, MSG_SIZE);

        while (ring_push(&shm->rx, idx) != 0) {
            // ring full → spin
        }
    }

    printf("[Producer] finished.\n");
    return 0;
}
