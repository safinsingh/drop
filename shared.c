#include <x86intrin.h>

// Rough (observed) latencies:
// L1: ~20 (0-25)
// L2: ~40 (25+)

int is_l1(int cycles) { return cycles < 25; }
int is_l2(int cycles) { return !is_l1(cycles); }

void sleep(int cycles) {
    for (int i = 0; i < cycles; i++) _mm_pause();
}

int rand_range(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}