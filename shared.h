#include <stdint.h>

// kaby lake l1 d$:
// size=32KiB, ways=8, linesz=64 (sets=64, set stride=4096)

#define SET_STRIDE 4096
#define LINE_SIZE 64
#define NUM_SETS 64

#define EVICTORS 9
#define ATTACK_BUF_SIZE (SET_STRIDE * EVICTORS)
#define PAGE_SIZE 4096
#define ATTACK_BUF_PAGES (ATTACK_BUF_SIZE / PAGE_SIZE)

#define CHASE_LENGTH 8
#define VICTIM_BUF_SIZE (SET_STRIDE * CHASE_LENGTH)

// fgets-style framed message: fixed-size byte buffer
#define MSG_BYTES 16
#define MSG_BITS (MSG_BYTES * 8)
#define MSG_SLOTS MSG_BITS

// 2048-cycle interval
#define INTERVAL_BITS 11
// f(interval)
#define L1_THRESHOLD 70
#define MISS_THRESHOLD 2

// set 0=used, 1-63=unused. used for prefaulting.
#define SAFE_SET 1

static inline uint32_t rdtsc_lo() {
    uint32_t lo;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo) : : "edx");
    return lo;
}

// performs load serialization. slight cycle count improvement over lfence + rdtsc.
static inline uint32_t rdtscp_lo() {
    uint32_t lo;
    __asm__ __volatile__ ("rdtscp" : "=a" (lo) : : "edx", "ecx");
    return lo;
}
