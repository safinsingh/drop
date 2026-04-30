#include "../shared.h"
#include <stdio.h>

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

int main() {
    for (;;) {
        while (!is_attacked(buf, REQ));
        // printf("got req, getting data\n");
        mask_t data = which_attacked(buf) >> 2;
        mask_t diff = data ^ TEST_CONST2;
        if (diff) { printf("diff: "); print_binary64(diff); printf("\n"); }
        // printf("got data, sending ack\n");
        while (attack_and_probe(buf, BIT(ACK), REQ));
        // printf("got !req\n");
        // if (is_attacked(buf, 0)) printf("abc\n");
    }
}