#include "../shared.h"
// #include <stdio.h>

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

int main() {
    for (;;) {
        attack_and_probe(buf, TEST_CONST2, 0);
        // printf("hi!: %d\n", line0);
    }
}