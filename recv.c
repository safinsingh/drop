#include "shared.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

int main(int argc, char**argv) {
    uint64_t start_tsc = strtoull(argv[1], 0, 10);

    mlockall(MCL_CURRENT | MCL_FUTURE);

    volatile uintptr_t victim_buf[VICTIM_BUF_SIZE / sizeof(uintptr_t)] __attribute__((__aligned__(SET_STRIDE)));
    unsigned aux;

    size_t stride = SET_STRIDE / sizeof(uintptr_t);
    for (int i = 0; i < CHASE_LENGTH; i++)
        victim_buf[i * stride] = (uintptr_t)&victim_buf[((i + 1) % CHASE_LENGTH) * stride];
    _mm_mfence();

    volatile uintptr_t* head = &victim_buf[0];

    // chase iv -2 to land chain in L1, idle iv -1 (sender seeds there)
    while (__rdtsc() < start_tsc - (2ULL << INTERVAL_BITS));
    uint64_t boundary = start_tsc - (1ULL << INTERVAL_BITS);
    uint64_t now;
    do {
#pragma GCC unroll 8
        for (int i = 0; i < CHASE_LENGTH; i++) head = (volatile uintptr_t*)*head;
        now = __rdtscp(&aux);
    } while (now < boundary);
    while (__rdtsc() < start_tsc);

    uint8_t slots[MSG_SLOTS];
    for (uint32_t i = 0; i < MSG_SLOTS; i++) slots[i] = 0;

    uint64_t start_interval = start_tsc >> INTERVAL_BITS;
    uint64_t prev_interval = __rdtsc() >> INTERVAL_BITS;
    uint32_t misses = 0;
    uint32_t pos = (uint32_t)(prev_interval - start_interval);
    if (pos > MSG_SLOTS) pos = MSG_SLOTS;
    while (pos < MSG_SLOTS) {
        uint32_t start = rdtsc_lo();
#pragma GCC unroll 8
        for (int i = 0; i < CHASE_LENGTH; i++) head = (volatile uintptr_t*)*head;
        uint64_t end = __rdtscp(&aux);
        uint32_t lat = (uint32_t)end - start;
        if (lat >= L1_THRESHOLD) misses++;
        uint64_t ci = end >> INTERVAL_BITS;
        if (ci > prev_interval) {
            slots[pos++] = misses >= MISS_THRESHOLD;
            misses = 0;
            // catch up if interrupt skipped intervals
            uint64_t skip = ci - prev_interval - 1;
            for (uint64_t k = 0; k < skip && pos < MSG_SLOTS; k++) slots[pos++] = 0;
            prev_interval = ci;
        }
    }

    uint8_t out[MSG_BYTES];
    for (uint32_t b = 0; b < MSG_BYTES; b++) {
        uint8_t byte = 0;
        for (uint32_t k = 0; k < 8; k++)
            if (slots[b * 8 + k]) byte |= (1u << k);
        out[b] = byte;
    }
    printf("msg: ");
    for (int i = 0; i < MSG_BYTES; i++) {
        uint8_t c = out[i];
        if (c >= 0x20 && c < 0x7f) putchar(c);
        else if (c == 0) putchar('.');
        else printf("\\x%02x", c);
    }
    putchar('\n');
}
