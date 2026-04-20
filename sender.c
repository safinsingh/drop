#include <stdint.h>
#include "util.h"
#include "shared.h"

typedef enum {
    STATE_IDLE,
    STATE_REQ_HI,
    STATE_REQ_LO
} sender_state_t;

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

uint8_t message = 0b10101010;

int main() {
    srand(time(NULL));
    sender_state_t state = STATE_IDLE;

    int bit;
    int index = 0;

    puts("IDLING");

    for (;;) {
        switch (state) {
            case STATE_IDLE:
                if (index++ == 8) goto done;
                puts("IDLING->REQHI");
                state = STATE_REQ_HI;
                break;

            case STATE_REQ_HI:
                bit = ((message >> index) & 0b1) ? DATA_1 : DATA_0;
                if (set_attack_and_probe2(buf, REQ, bit, ACK)) { puts("REQHI->REQLO"); state = STATE_REQ_LO; }
                break;
            
            case STATE_REQ_LO:
                if (!set_is_attacked(buf, ACK)) { puts("REQLO->IDLING"); state = STATE_IDLE; }
                break;
        }
    }

done:
    puts("done");
}
