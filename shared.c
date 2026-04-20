#include <immintrin.h>
#include <stdbool.h>
#include "util.h"
#include "shared.h"

// hand-tuned constants; ~accurate when other thread is busy-priming set
#define BURST_REPETITIONS 4000
#define BURST_SLEEP 4000

// Rough (observed) latencies:
// L1: ~20 (0-30)
// L2: ~40 (30+)
bool is_l1(int cycles) { return cycles < 30; }
bool is_l2(int cycles) { return !is_l1(cycles); }

void sleep(int cycles) {
    for (int i = 0; i < cycles; i++);
}

// buf must be  aligned to SET_STRIDE
//              of a size === 0 (mod SET_STRIDE)
bool set_is_attacked(volatile uint8_t* buf, int set) {
    int hits = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[set * LINE_SIZE] = 0;
        sleep(BURST_SLEEP);
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[set * LINE_SIZE]));
        if (is_l2(t)) hits++;
    }
    return hits > BURST_REPETITIONS / 2;
}

// fused to check two sets at once
// set1->MSB, set2->LSB
int set_is_attacked2(volatile uint8_t* buf, int set1, int set2) {
    int h1 = 0, h2 = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[set1 * LINE_SIZE] = 0;
        buf[set2 * LINE_SIZE] = 0;
        sleep(BURST_SLEEP);
        CYCLES t1 = measure_one_block_access_time((uint64_t)(&buf[set1 * LINE_SIZE]));
        CYCLES t2 = measure_one_block_access_time((uint64_t)(&buf[set2 * LINE_SIZE]));
        if (is_l2(t1)) h1++;
        if (is_l2(t2)) h2++;
    }
    int r1 = h1 > BURST_REPETITIONS / 2;
    int r2 = h2 > BURST_REPETITIONS / 2;
    return (r1 << 1) | r2;
}

// buf must be  aligned to SET_STRIDE
//              of size SET_STRIDE * NUM_EVICTORS
void set_attack(volatile uint8_t* buf, int set) {
    for (int j = 1; j < NUM_EVICTORS; j++) {
        buf[set * LINE_SIZE + j * SET_STRIDE] = 0;
    }
}

// fused probe/attack to sustain pressure
bool set_attack_and_probe(volatile uint8_t* buf, int attack_set, int probe_set) {
    int hits = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[probe_set * LINE_SIZE] = 0;
        set_attack(buf, attack_set);
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE]));
        if (is_l2(t)) hits++;
    }
    return hits > BURST_REPETITIONS / 2;
}

bool set_attack_and_probe2(volatile uint8_t* buf, int attack_set1, int attack_set2, int probe_set) {
    int hits = 0;
    for (int i = 0; i < BURST_REPETITIONS; i++) {
        buf[probe_set * LINE_SIZE] = 0;
        set_attack(buf, attack_set1);
        set_attack(buf, attack_set2);
        CYCLES t = measure_one_block_access_time((uint64_t)(&buf[probe_set * LINE_SIZE]));
        if (is_l2(t)) hits++;
    }
    return hits > BURST_REPETITIONS / 2;
}
