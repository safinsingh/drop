#include <immintrin.h>
#include <stdbool.h>
#include "util.h"
#include "shared.h"

// hand-tuned constants; ~accurate when other thread is busy-priming set
#define BURST_REPETITIONS 10000
#define BURST_SLEEP 4000
#define SET_PRESSURE 512

// Rough (observed) latencies:
// L1: ~20 (0-25)
// L2: ~40 (25+)
bool is_l1(int cycles) { return cycles < 25; }
bool is_l2(int cycles) { return !is_l1(cycles); }

void sleep(int cycles) {
    for (int i = 0; i < cycles; i++);
}

int rand_range(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

// buf must be  aligned to SET_STRIDE
//              of a size === 0 (mod SET_STRIDE)
bool set_is_attacked(volatile uint8_t* buf, int set) {
    CYCLES c = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[set * LINE_SIZE] = 0;

        sleep(BURST_SLEEP);

        FENCE
        c += measure_one_block_access_time((uint64_t)(&buf[set * LINE_SIZE]));
        FENCE
    }

    return is_l2(c / BURST_REPETITIONS);
}

// fused to check two sets at once
// set1->MSB, set2->LSB
bool set_is_attacked2(volatile uint8_t* buf, int set1, int set2) {
    CYCLES c1 = 0;
    CYCLES c2 = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[set1 * LINE_SIZE] = 0;
        buf[set2 * LINE_SIZE] = 0;

        sleep(BURST_SLEEP);

        FENCE
        c1 += measure_one_block_access_time((uint64_t)(&buf[set1 * LINE_SIZE]));
        FENCE
        c2 += measure_one_block_access_time((uint64_t)(&buf[set2 * LINE_SIZE]));
        FENCE
    }

    return (is_l2(c1 / BURST_REPETITIONS) << 1) | is_l2(c2 / BURST_REPETITIONS);
}

// buf must be  aligned to SET_STRIDE
//              of size SET_STRIDE * NUM_EVICTORS
void set_attack(volatile uint8_t* buf, int set) {
    for (int i = 0; i < SET_PRESSURE; i++) {
        buf[set * LINE_SIZE + rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
    }
}

// fused probe/attack to sustain pressure
bool set_attack_and_probe(volatile uint8_t* buf, int attack_set, int probe_set) {
    CYCLES c = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[attack_set * LINE_SIZE + rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
    
        buf[probe_set * LINE_SIZE] = 0;
        sleep(BURST_SLEEP);
        FENCE
        c += measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE]));
        FENCE
    }

    return is_l2(c / BURST_REPETITIONS);
}

bool set_attack_and_probe2(volatile uint8_t* buf, int attack_set1, int attack_set2, int probe_set) {
    CYCLES c = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[attack_set1 * LINE_SIZE + rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
        buf[attack_set2 * LINE_SIZE + rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
    
        buf[probe_set * LINE_SIZE] = 0;
        sleep(BURST_SLEEP);
        FENCE
        c += measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE]));
        FENCE
    }

    return is_l2(c / BURST_REPETITIONS);
}


