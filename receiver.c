#include "shared.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_LISTEN,
    STATE_READ,
    STATE_ACK_HI
} receiver_state_t;

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

int main() {
    srand(time(NULL));
    receiver_state_t state = STATE_LISTEN;

    int tmp;
    bool zero, one;
    int index = 0;
    uint8_t data = 0;

    puts("LISTENING");

    for (;;) {
        switch (state) {
            case STATE_LISTEN:
                if (set_is_attacked(buf, REQ)) { puts("LISTENING->READING"); state = STATE_READ; }
                break;

            case STATE_READ:
                tmp  = set_is_attacked2(buf, DATA_0, DATA_1);
                zero = tmp & 0b10;
                one  = tmp & 0b01;

                if ((zero && one) || !(zero || one)) continue;
                else if (one) data |= 1 << index++;

                puts("READING->ACKING");
                state = STATE_ACK_HI;
                break;

            case STATE_ACK_HI:
                if (!set_attack_and_probe(buf, ACK, REQ)) { puts("ACKING->LISTENING"); state = STATE_LISTEN; }
                break;
        }
    }
}