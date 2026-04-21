#include <stdint.h>
#include <string.h>
#include "util.h"
#include "shared.h"

typedef enum {
    STATE_INPUT,
    STATE_REQ_HI,
    STATE_REQ_LO
} sender_state_t;

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

static char message_buf[512] = {0};

int main() {
    setvbuf(stdout, NULL, _IOLBF, 0);
    sender_state_t state = STATE_INPUT;

    int bit;
    int index = 0;
    int len;

    debug_print("INPUT");

    for (;;) {
        switch (state) {
            case STATE_INPUT:
                printf("> ");
                fgets(message_buf, sizeof(message_buf), stdin);
                // send null-terminator
                index = 0; len = (strlen(message_buf) + 1) * 8;
                state = STATE_REQ_HI; debug_print("INPUT->REQ_HI");
                break;

            case STATE_REQ_HI:
                bit = get_nth_bit(message_buf, index) ? DATA_1 : DATA_0;
                if (set_attack_and_probe2(buf, REQ, bit, ACK)) { debug_print("REQ_HI->REQ_LO"); state = STATE_REQ_LO; }
                break;

            case STATE_REQ_LO:
                if (!set_is_attacked(buf, ACK)) {
                    if (++index == len) {
                        state = STATE_INPUT; debug_print("REQ_LO->INPUT");
                        printf("sent: %s", message_buf);
                    }
                    else { state = STATE_REQ_HI; debug_print("REQ_LO->REQ_HI"); }
                }
                break;
        }
    }
}
