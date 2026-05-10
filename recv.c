#include "proto.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef TRACE
#define TRACEF(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACEF(...) ((void)0)
#endif

typedef struct {
    // Current pointer-chase node for the hot prime+probe measurement loop.
    volatile uintptr_t *head;
    // Last observed 2048-cycle interval index.
    uint64_t prev_interval;
    // Number of slow pointer chases accumulated in the current interval.
    uint32_t misses;
} capture_t;

static void capture_until(volatile uint8_t *raw, uint32_t *pos, uint32_t target,
                          capture_t *cap)
{
    while (*pos < target) {
        uint32_t start = rdtsc_lo();
#pragma GCC unroll 8
        for (uint32_t i = 0; i < CHASE_LENGTH; i++)
            cap->head = (volatile uintptr_t *)*cap->head;
        uint64_t end = rdtscp();

        uint32_t lat = (uint32_t)end - start;
        if (lat >= L1_THRESHOLD && lat < INTERVAL_CYCLES)
            cap->misses++;

        uint64_t ci = end >> INTERVAL_BITS;
        if (ci > cap->prev_interval) {
            raw[raw_offset((*pos)++)] = cap->misses > 255u ? 255u : (uint8_t)cap->misses;
            cap->misses = 0;

            uint64_t skipped = ci - cap->prev_interval - 1u;
            while (skipped-- && *pos < target)
                raw[raw_offset((*pos)++)] = 0;
            cap->prev_interval = ci;
        }
    }
}

int main(void)
{
    static volatile uint8_t raw[RAW_STORE_SIZE] __attribute__((__aligned__(SET_STRIDE)));
    static uint8_t out[MSG_BYTES];

    mlockall(MCL_CURRENT | MCL_FUTURE);

    for (uint32_t page = 0; page < RAW_PAGES; page++)
        raw[page * PAGE_SIZE + SAFE_SET * LINE_SIZE] = 0;

    volatile uintptr_t victim_buf[VICTIM_BUF_SIZE / sizeof(uintptr_t)] __attribute__((__aligned__(SET_STRIDE)));
    size_t stride = SET_STRIDE / sizeof(uintptr_t);
    for (uint32_t i = 0; i < CHASE_LENGTH; i++)
        victim_buf[i * stride] = (uintptr_t)&victim_buf[((i + 1u) % CHASE_LENGTH) * stride];
    _mm_mfence();

    capture_t cap = {
        .head = &victim_buf[0],
        .prev_interval = __rdtsc() >> INTERVAL_BITS,
        .misses = 0,
    };
    uint32_t pos = 0;
    capture_until(raw, &pos, ACQUIRE_SLOTS, &cap);

    uint64_t scan_start = __rdtsc();
    proto_decode_state_t state;
    if (proto_collect_headers(raw, &state) == 0) {
        uint64_t scan_end = __rdtsc();
        TRACEF("recv candidates=%u best=%lld header=0 scan_slots=%llu\n",
               state.candidate_count, (long long)state.best_score,
               (unsigned long long)((scan_end - scan_start) >> INTERVAL_BITS));
        return 2;
    }

    uint64_t scan_end = __rdtsc();
    int32_t guard_slack = (int32_t)state.min_sync_base - (int32_t)pos -
                          (int32_t)((scan_end - scan_start) >> INTERVAL_BITS);
    if (guard_slack < 0) {
        TRACEF("recv candidates=%u headers=%u best=%lld guard_slack=%d\n",
               state.candidate_count, state.header_count,
               (long long)state.best_score, guard_slack);
        return 2;
    }
    capture_until(raw, &pos, state.max_sync_end, &cap);
    proto_choose_payload(raw, &state);

    uint32_t target = proto_payload_target(&state);
    if (target > MAX_CAPTURE_SLOTS)
        return 2;
    capture_until(raw, &pos, target, &cap);

    proto_decode_payload(raw, &state, out);
#ifdef TRACE
    TRACEF("recv off=%u candidates=%u headers=%u best=%lld sync=%lld len=%u scan_slots=%llu guard_slack=%d\n",
           state.frame_off, state.candidate_count, state.header_count,
           (long long)state.best_score, (long long)state.best_sync, state.len,
           (unsigned long long)((scan_end - scan_start) >> INTERVAL_BITS),
           guard_slack);
#else
    (void)state;
    (void)scan_start;
    (void)scan_end;
    (void)guard_slack;
#endif

    if (write(1, out, state.len) < 0)
        return 1;
    return 0;
}
