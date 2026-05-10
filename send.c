#include "shared.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include <sys/mman.h>
#include <time.h>

int main(int argc, char**argv) {
    uint64_t start_tsc = strtoull(argv[1], 0, 10);

    static uint8_t msg[MSG_BYTES] = {0};
    if (fgets((char*)msg, sizeof(msg), stdin) == NULL) return 1;
    size_t n = strlen((char*)msg);
    if (n > 0 && msg[n-1] == '\n') msg[n-1] = 0;

    mlockall(MCL_CURRENT | MCL_FUTURE);

    volatile uint8_t attack_buf[ATTACK_BUF_SIZE] __attribute__((__aligned__(SET_STRIDE)));
    for (int i = 0; i < ATTACK_BUF_PAGES; i++) attack_buf[i * PAGE_SIZE + SAFE_SET * LINE_SIZE];
    _mm_lfence();

    // seed evictors into L1 in iv -1; recv chases iv -2 only (stagger-RS)
    uint64_t seed_at = start_tsc - (1ULL << INTERVAL_BITS);
    while (__rdtsc() < seed_at);
    for (int i = 0; i < EVICTORS; i++) attack_buf[i * SET_STRIDE];
    while (__rdtsc() < start_tsc);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // catch up if interrupt delayed us past start_tsc
    uint64_t start_interval = start_tsc >> INTERVAL_BITS;
    uint64_t prev_interval = __rdtsc() >> INTERVAL_BITS;
    uint32_t shift = (uint32_t)(prev_interval - start_interval);
    while (shift < MSG_BITS) {
        uint8_t byte = msg[shift >> 3];
        if ((byte >> (shift & 7)) & 1) {
            for (int i = 0; i < EVICTORS; i++) attack_buf[i * SET_STRIDE];
            for (int i = 0; i < EVICTORS; i++) attack_buf[i * SET_STRIDE];
        }
        unsigned aux;
        uint64_t cur_interval = __rdtscp(&aux) >> INTERVAL_BITS;
        if (cur_interval > prev_interval) {
            shift += (uint32_t)(cur_interval - prev_interval);
            prev_interval = cur_interval;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    fprintf(stderr, "wrote %d bytes in %.3fms (%.3f bytes/sec)\n",
            MSG_BYTES, sec * 1e3, MSG_BYTES / sec);
}
