#include "shared.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    volatile uintptr_t victim_buf[VICTIM_BUF_SIZE]  __attribute__((__aligned__(SET_STRIDE)));
    uint8_t counter = 0;

    // set up circular chase + warm l1
    size_t stride = SET_STRIDE / sizeof(uintptr_t);
    for (int i = 0; i < CHASE_CANDIDATES; i++)
        victim_buf[i * stride] =
            (uintptr_t)&victim_buf[((i + 1) % CHASE_CANDIDATES) * stride];
    _mm_mfence();

    volatile uintptr_t* head = &victim_buf[0];
    for (;;) {
        uint32_t start = rdtsc_lo();
        for (int i = 0; i < CHASE_LENGTH; i++) head = (volatile uintptr_t*)*head;
        // CHASE_LENGTH loads MUST finish
        uint32_t elapsed = rdtscp_lo() - start;

        counter <<= 1; counter += elapsed > L1_THRESHOLD;
        if (__builtin_popcount(counter) >= MISS_THRESHOLD) {
            return 0;
        }
    }
}
