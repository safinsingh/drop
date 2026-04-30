#include "../shared.h"
// #include <stdio.h>

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

int main() {
    for (;;) {
        while (!attack_and_probe(buf, DATA_PACKET(TEST_CONST2), ACK));
        // printf("got ack\n");
        while (is_attacked(buf, ACK));
        // printf("got !ack\n");
    }
}