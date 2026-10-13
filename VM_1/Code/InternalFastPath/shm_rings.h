#ifndef SHM_RINGS_H
#define SHM_RINGS_H

#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#define CACHELINE   64

// پیام‌ها تا سقف 64KB
#define CHUNK_SIZE  65536
#define NUM_CHUNKS  1024

// اندازهٔ رینگ (قدرت دو)
#define RING_SIZE   1024
#define RING_MASK   (RING_SIZE - 1)

typedef struct {
    atomic_uint head;    // producer index
    atomic_uint tail;    // consumer index
    uint32_t buf[RING_SIZE];
} shm_ring_t;

// UMEM مشترک
typedef struct {
    uint8_t data[NUM_CHUNKS][CHUNK_SIZE];
} shm_umem_t;

// ساختار اصلی shared-memory
typedef struct {
    shm_umem_t umem;
    shm_ring_t fill;   // free buffers → producer alloc
    shm_ring_t rx;     // producer → consumer
    shm_ring_t comp;   // consumer → producer (ack)
} shm_region_t;

// -------------------------------------------------------
//          Primitiveهای رینگ (SPSC، اتمیک واقعی)
// -------------------------------------------------------

static inline void ring_init(shm_ring_t *r) {
    atomic_store_explicit(&r->head, 0, memory_order_relaxed);
    atomic_store_explicit(&r->tail, 0, memory_order_relaxed);
}

static inline int ring_is_empty(const shm_ring_t *r) {
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    return head == tail;
}

static inline int ring_is_full(const shm_ring_t *r) {
    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    return ((head + 1) & RING_MASK) == tail;
}

static inline int ring_push(shm_ring_t *r, uint32_t v) {
    uint32_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint32_t next = (head + 1) & RING_MASK;
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);

    if (next == tail) return -1; // full

    r->buf[head] = v;
    atomic_store_explicit(&r->head, next, memory_order_release);
    return 0;
}

static inline int ring_pop(shm_ring_t *r, uint32_t *out) {
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);

    if (tail == head) return -1; // empty

    *out = r->buf[tail];
    uint32_t next = (tail + 1) & RING_MASK;
    atomic_store_explicit(&r->tail, next, memory_order_release);
    return 0;
}

#endif
