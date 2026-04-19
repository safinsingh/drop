#include "stdint.h"
#include "util.h"

// Processor -> L1 || TLB -> L2 -> L3
//
// 64-byte lines, 4KiB pages
//
// L1: 32KiB, 8-way -- 64-bit address: T=48, I=6, O=6
// L2: 256KiB, 4-way
// L3: 8MiB, 16-way

// Rough (observed) latencies:
// L1: ~20 (0-25)
// L2: ~40 (25+)

#define KIB 1024
#define L1_SIZE 32 * KIB
#define L1_WAYS 8
#define SET_STRIDE (L1_SIZE / L1_WAYS)
#define SET_PRESSURE 1000
#define NUM_EVICTORS 128
#define RETRIES 5000
#define FENCE do { __asm__ __volatile__ ("lfence" ::: "memory"); } while (0);

volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS];

int rand_range(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

int main() {
    srand(time(NULL));
    CYCLES c1 = 0, c2 = 0;

    for (int j = 0; j < RETRIES; j++) {
        buf[0] = 0;

        FENCE
        c1 += measure_one_block_access_time((uint64_t)buf);
        FENCE

        for (int i = 0; i < SET_PRESSURE; i++) {
            buf[rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
        }

        FENCE
        c2 += measure_one_block_access_time((uint64_t)buf);
        FENCE
    }

    c1 /= RETRIES; c2 /= RETRIES;
    printf("cycles: %u, %u\n", c1, c2);
}
