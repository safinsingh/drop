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

void spin(int cycles) {
    for (int i = 0; i < cycles; i++) _mm_pause();
}

// buf must be  aligned to SET_STRIDE
//              of a size === 0 (mod SET_STRIDE)
bool is_attacked(volatile uint8_t* buf, int set) {
    register int misses = 0;
    for (int i = 0; i < 50; i++) {
        buf[set_index(set)] = 0;
        spin(400); // ~ 1/popcount(attack bits!) we go with worst-case.
        CYCLES t = set_access_time(buf, set);
        if (is_l2(t)) misses++;
    }
    uint64_t cond = misses >= 30;
    #ifdef DEBUG
    if (cond) printf("misses: %d\n", misses);
    #endif
    return cond;
}

mask_t which_attacked(volatile uint8_t* buf) {
    int misses[L1_SETS] = {0};
    for (int i = WHICH_MIN; i < WHICH_MAX; i++) {
        register int m = 0;
        for (int j = 0; j < 15; j++) {
            buf[set_index(i)] = 0;
            spin(100);
            CYCLES t = set_access_time(buf, i);
            if (is_l2(t)) m++;
        }
        misses[i] = m;
    }

    #ifdef DEBUG
    uint64_t avg_attacked_misses = 0;
    uint64_t avg_relaxed_misses  = 0;
    mask_t packet = DATA_PACKET(TEST_CONST2);
    uint8_t popcnt = _mm_popcnt_u64(packet);
    for (int i = WHICH_MIN; i < WHICH_MAX; i++) {
        if (TEST_BIT(packet, i)) avg_attacked_misses += misses[i];
        else avg_relaxed_misses += misses[i];
    }
    avg_attacked_misses /= popcnt; avg_relaxed_misses /= (64 - popcnt);
    uint32_t needs_print = 0;
    #endif

    #ifdef DEBUG
    struct { int i, misses; uint64_t expected, got; } mismatches[L1_SETS];
    int n_mismatch = 0;
    #endif

    uint64_t ret = 0;
    for (int i = WHICH_MIN; i < WHICH_MAX; i++) {
        uint64_t cond = misses[i] >= 9;
        ret |= cond << i;

        #ifdef DEBUG
        if (cond ^ TEST_BIT(packet, i)) {
            mismatches[n_mismatch].i = i;
            mismatches[n_mismatch].misses = misses[i];
            mismatches[n_mismatch].expected = TEST_BIT(packet, i);
            mismatches[n_mismatch].got = cond;
            n_mismatch++;
            needs_print = true;
        }
        #endif
    }

    #ifdef DEBUG
    if (needs_print) {
        for (int x = 0; x < n_mismatch; x++) {
            printf("misses[%d] = %d (expected %lu, got %lu)\n",
                   mismatches[x].i, mismatches[x].misses,
                   mismatches[x].expected, mismatches[x].got);
        }
        printf("avg attacked #misses: %lu\n", avg_attacked_misses);
        printf("avg passive  #misses: %lu\n", avg_relaxed_misses);
    }
    #endif

    return ret;
}

// fused probe/attack to sustain pressure
bool attack_and_probe(volatile uint8_t* buf, mask_t attack_sets, int probe_set) {
    int num_attacked = _mm_popcnt_u64(attack_sets);
    int evictors = 5500 / num_attacked; // per set, per round
    int misses = 0;

    for (int i = 0; i < 50; i++) {
        buf[set_index(probe_set)] = 0;

        for (int j = 0; j < evictors; j++) {
            int evictor = (i * evictors + j) & (NUM_EVICTORS - 1);

            for (uint64_t m = attack_sets; m > 0; m &= m - 1) {
                int k = __builtin_ctzll(m);
                buf[set_index_no_offset(k) + evictor * SET_STRIDE] = 0;
            }
        }

        CYCLES t = set_access_time(buf, probe_set);
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