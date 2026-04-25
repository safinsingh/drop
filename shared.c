#include <immintrin.h>
#include <popcntintrin.h>
#include <stdbool.h>
#include "util.h"
#include "shared.h"
#include <syscall.h>
#include <unistd.h>

// each function contains hand-tuned constants; ~accurate when other thread is busy-priming set
// note that these constants must be tuned per-device. this was tested on:
// asus ux430unr, i7-8550u, plugged in

// Rough (observed) latencies:
// L1: ~20 (0-30)
// L2: ~40 (30+)
bool is_l1(int cycles) { return cycles < 30; }
bool is_l2(int cycles) { return !is_l1(cycles); }

void spin(int cycles) {
    // TODO: replace with blocking I/O
    for (int i = 0; i < cycles; i++) _mm_pause();
}

// IN UR FACE DCU-IP PREFETCHER!!!
static inline int rand_line_offset() {
    return rand() % (LINE_SIZE - 1);
}

// buf must be  aligned to SET_STRIDE
//              of a size === 0 (mod SET_STRIDE)
bool is_attacked(volatile uint8_t* buf, int set) {
    int misses = 0;
    for (int i = 0; i < 50; i++) {
        buf[set * LINE_SIZE + rand_line_offset()] = 0;
        spin(400); // ~ 1/popcount(attack bits!) we go with worst-case.
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[set * LINE_SIZE + rand_line_offset()]));
        if (is_l2(t)) misses++;
    }
    return misses > 30;
}

mask_t which_attacked(volatile uint8_t* buf) {
    int misses[L1_SETS] = {0};
    for (int j = 0; j < L1_SETS; j++) {
        for (int i = 0; i < 50; i++) {
            buf[j * LINE_SIZE + rand_line_offset()] = 0;
            spin(500);
            CYCLES t = measure_one_block_access_time((uint64_t)(&buf[j * LINE_SIZE + rand_line_offset()]));
            if (is_l2(t)) misses[j]++;
        }
    }

    #ifdef DEBUG
    uint64_t avg_attacked_misses = 0;
    uint64_t avg_relaxed_misses  = 0;
    uint8_t popcnt = _mm_popcnt_u64(TEST_CONST);
    for (int i = 0; i < L1_SETS; i++) {
        if (TEST_BIT(TEST_CONST, i)) avg_attacked_misses += misses[i];
        else avg_relaxed_misses += misses[i];
    }
    avg_attacked_misses /= popcnt; avg_relaxed_misses /= (64 - popcnt);
    printf("avg attacked #misses: %lu\n", avg_attacked_misses);
    printf("avg passive  #misses: %lu\n", avg_relaxed_misses);
    #endif
    
    uint64_t ret = 0;
    for (int i = 0; i < L1_SETS; i++) {
        uint64_t cond = misses[i] > 35;

        // missed thresh
        #ifdef DEBUG
        if (cond ^ TEST_BIT(TEST_CONST, i)) printf("misses[%d] = %d\n", i, misses[i]);
        #endif

        ret |= cond << i;
    }
    return ret;
}

// fused probe/attack to sustain pressure
bool attack_and_probe(volatile uint8_t* buf, mask_t attack_sets, int probe_set) {
    int num_attacked = _mm_popcnt_u64(attack_sets);
    int evictors = 4000 / num_attacked; // per set, per round
    int misses = 0;

    for (int i = 0; i < 50; i++) {
        buf[probe_set * LINE_SIZE + rand_line_offset()] = 0;

        for (int j = 0; j < evictors; j++) {
            for (int k = 0; k < 64; k++) {
                if ((attack_sets >> k) & 0b1) {
                    // only touch lines in the [1, NUM_EVICTORS] * SET_STRIDE range
                    int index = (i * evictors + j) % (NUM_EVICTORS - 1) + 1;
                    buf[k * LINE_SIZE + index * SET_STRIDE] = 0;
                }
            }
        }
        
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE + rand_line_offset()]));
        if (is_l2(t)) misses++;
    }
    return misses > 30;
}

void print_binary64(uint64_t n) {
    for (int i = 63; i >= 0; i--) {
        uint64_t mask = (uint64_t)1 << i;
        if (n & mask) {
            putchar('1');
        } else {
            putchar('0');
        }
    }
}