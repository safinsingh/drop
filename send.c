#include "shared.h"
#include <stdint.h>
#include <immintrin.h>

int main() {
    volatile uint8_t attack_buf[ATTACK_BUF_SIZE] __attribute__((__aligned__(SET_STRIDE)));

    // pre-fault all attack_buf pages; only touch safe set so as not to provoke recv
    for (int i = 0; i < ATTACK_BUF_PAGES; i++) attack_buf[i * PAGE_SIZE + SAFE_SET * LINE_SIZE];
    _mm_lfence();

    // move lines mapping to set 0 into L2
    for (int i = 0; i < EVICTORS; i++) _mm_prefetch(&attack_buf[i * SET_STRIDE], _MM_HINT_T1);
    _mm_lfence();

    for (;;) {
        for (int i = 0; i < EVICTORS; i++) attack_buf[i * SET_STRIDE];
    }
}
