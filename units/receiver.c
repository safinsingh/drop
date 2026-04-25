#include "../shared.h"
#include <stdio.h>

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

int main() {
    for (;;) {
        // attack_and_probe(buf, ~CONST, 1);
        // mask_t which = which_attacked(buf);
        // printf("diff: "); print_binary64(which ^ CONST); printf("\n");
        if (is_attacked(buf, 0)) printf("abc\n");
    }
}