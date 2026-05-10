#include "proto.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>

#define SLOT_GUARD_CYCLES 96u

#ifdef TRACE
#define TRACEF(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACEF(...) ((void)0)
#endif

static inline void send_one(volatile uint8_t *attack_buf)
{
    for (uint32_t r = 0; r < ONE_BURSTS; r++) {
#pragma GCC unroll 9
        for (uint32_t i = 0; i < EVICTORS; i++)
            attack_buf[i * SET_STRIDE];
#pragma GCC unroll 9
        for (uint32_t i = 0; i < EVICTORS; i++)
            attack_buf[i * SET_STRIDE];
    }
}

static inline uint8_t byte_bit(const uint8_t *buf, uint32_t bit)
{
    return (uint8_t)((buf[bit >> 3] >> (bit & 7u)) & 1u);
}

static inline uint32_t send_slot(volatile uint8_t *attack_buf, uint64_t interval, uint8_t bit)
{
    uint64_t slot_start = interval << INTERVAL_BITS;
    uint64_t slot_stop = slot_start + INTERVAL_CYCLES - SLOT_GUARD_CYCLES;
    uint64_t now;

    while ((now = __rdtsc()) < slot_start)
        ;

    if (now >= slot_stop)
        return 1u;
    if (bit)
        send_one(attack_buf);
    while (__rdtsc() < slot_stop)
        ;
    return 0;
}

int main(void)
{
    static uint8_t preamble[PREAMBLE_BITS];
    static uint8_t sync[PAYLOAD_SYNC_BITS];
    static uint8_t msg[MSG_BYTES];
    static uint8_t header[HEADER_BYTES];
    fill_preamble(preamble);
    fill_payload_sync(sync);

    size_t n = fread(msg, 1, sizeof(msg), stdin);
    if (n > 0 && msg[n - 1] == '\n')
        n--;

    put_u32(&header[0], FRAME_MAGIC);
    put_u16(&header[4], (uint16_t)n);
    put_u16(&header[6], (uint16_t)~(uint16_t)n);

    mlockall(MCL_CURRENT | MCL_FUTURE);

    volatile uint8_t attack_buf[ATTACK_BUF_SIZE] __attribute__((__aligned__(SET_STRIDE)));
    for (uint32_t i = 0; i < ATTACK_BUF_PAGES; i++)
        attack_buf[i * PAGE_SIZE + SAFE_SET * LINE_SIZE];
    _mm_lfence();

    uint32_t late = 0;
    uint64_t interval = (__rdtsc() >> INTERVAL_BITS) + 2u;
    struct timespec start_ts;
    struct timespec end_ts;

    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    for (uint32_t i = 0; i < PREAMBLE_BITS; i++, interval++)
        late += send_slot(attack_buf, interval, preamble[i]);

    for (uint32_t bit = 0; bit < HEADER_BITS; bit++) {
        uint8_t v = byte_bit(header, bit);
        for (uint32_t rep = 0; rep < HEADER_REPS; rep++, interval++)
            late += send_slot(attack_buf, interval, v);
    }

    for (uint32_t i = 0; i < HEADER_GUARD_SLOTS; i++, interval++)
        late += send_slot(attack_buf, interval, 0);

    for (uint32_t i = 0; i < PAYLOAD_SYNC_BITS; i++, interval++)
        late += send_slot(attack_buf, interval, sync[i]);

    for (uint32_t bit = 0; bit < (uint32_t)n * 8u; bit++, interval++)
        late += send_slot(attack_buf, interval, byte_bit(msg, bit));

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    double elapsed_sec = (double)(end_ts.tv_sec - start_ts.tv_sec) +
                         (double)(end_ts.tv_nsec - start_ts.tv_nsec) / 1e9;
    double kbps = elapsed_sec > 0.0 ? ((double)n * 8.0) / (elapsed_sec * 1000.0) : 0.0;
    fprintf(stderr, "wrote %zu bytes in %.3fms (%.3f Kbps)\n",
            n, elapsed_sec * 1e3, kbps);

    TRACEF("sent %zu bytes slots=%u late=%u\n",
           n, PREAMBLE_BITS + HEADER_SLOTS + HEADER_GUARD_SLOTS +
           PAYLOAD_SYNC_BITS + (uint32_t)n * 8u, late);
    (void)late;
    return 0;
}
