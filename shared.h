#ifndef DROP2_SHARED_H
#define DROP2_SHARED_H

#include <immintrin.h>
#include <stdint.h>

// Kaby Lake L1D: 32KiB, 8-way, 64-byte lines, 64 sets.
#define SET_STRIDE 4096u
#define LINE_SIZE 64u
#define NUM_SETS 64u

#define EVICTORS 9u
#define ONE_BURSTS 32u
#define ATTACK_BUF_SIZE (SET_STRIDE * EVICTORS)
#define PAGE_SIZE 4096u
#define ATTACK_BUF_PAGES (ATTACK_BUF_SIZE / PAGE_SIZE)

#define CHASE_LENGTH 8u
#define VICTIM_BUF_SIZE (SET_STRIDE * CHASE_LENGTH)

// 2048-cycle physical slots.
#define INTERVAL_BITS 11u
#define INTERVAL_CYCLES (1u << INTERVAL_BITS)
#define L1_THRESHOLD 70u

#define SAFE_SET 1u

static inline uint32_t rdtsc_lo(void)
{
    uint32_t lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo) : : "edx");
    return lo;
}

static inline uint32_t rdtscp_lo(void)
{
    uint32_t lo;
    __asm__ __volatile__("rdtscp" : "=a"(lo) : : "edx", "ecx");
    return lo;
}

static inline uint64_t rdtscp(void)
{
    unsigned aux;
    return __rdtscp(&aux);
}

#endif
