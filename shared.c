#include <immintrin.h>
#include <stdbool.h>
#include "util.h"
#include "shared.h"

// hand-tuned constants; ~accurate when other thread is busy-priming set
#define UNI_PROBE_ROUNDS 4000
#define WHICH_ROUNDS 5000
#define AP_ROUNDS 4000
#define PROBE_SLEEP 4000

// Rough (observed) latencies:
// L1: ~20 (0-30)
// L2: ~40 (30+)
bool is_l1(int cycles) { return cycles < 30; }
bool is_l2(int cycles) { return !is_l1(cycles); }

void sleep(int cycles) {
    // TODO: replace with blocking I/O
    for (int i = 0; i < cycles; i++);
}

// buf must be  aligned to SET_STRIDE
//              of a size === 0 (mod SET_STRIDE)
bool is_attacked(volatile uint8_t* buf, int set) {
    int hits = 0;
    for (int i = 0; i < UNI_PROBE_ROUNDS; i++) {
        buf[set * LINE_SIZE] = 0;
        sleep(PROBE_SLEEP);
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[set * LINE_SIZE]));
        if (is_l2(t)) hits++;
    }
    return hits > UNI_PROBE_ROUNDS / 2;
}

mask_t which_attacked(volatile uint8_t* buf) {
    uint64_t hits[L1_SETS] = {0};
    for (int i = 0; i < WHICH_ROUNDS; i++) {
        for (int j = 0; j < L1_SETS; j++) {
            buf[j * LINE_SIZE] = 0;
        }
        sleep(TODO);
        for (int j = 0; j < L1_SETS; j++) {
            CYCLES t = measure_one_block_access_time((uint64_t)(&buf[j * LINE_SIZE]));
            if (is_l2(t)) hits[j]++;
        }
    }
    uint64_t ret = 0;
    for (int i = 0; i < L1_SETS; i++) {
        ret |= (hits[i] > WHICH_ROUNDS / 2) << i;
    }
    return ret;
}

// fused probe/attack to sustain pressure
bool attack_and_probe(volatile uint8_t* buf, mask_t attack_sets, int probe_set) {
    int num_attacked = _mm_popcnt_u64(attack_sets);
    int evictors = PROBE_SLEEP / num_attacked; // per set, per round
    int hits = 0;

    for (int i = 0; i < AP_ROUNDS; i++) {
        buf[probe_set * LINE_SIZE] = 0;

        for (int j = 0; j < evictors; j++) {
            for (int k = 0; k < 64; k++) {
                if ((attack_sets >> k) & 0b1) {
                    // only touch lines in the [1, NUM_EVICTORS] * SET_STRIDE range
                    int index = (i * evictors + j) % (NUM_EVICTORS - 1) + 1;
                    buf[k * LINE_SIZE + index * SET_STRIDE] = 0;
                }
            }
        }
        
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE]));
        if (is_l2(t)) hits++;
    }
    return hits > AP_ROUNDS / 2;
}
