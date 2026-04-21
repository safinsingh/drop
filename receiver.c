#include "shared.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_LISTEN,
    STATE_READ,
    STATE_ACK
} receiver_state_t;

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));
static char message_buf[512] = {0};

bool message_done(char* buf, int current_bit_index) {
    if (current_bit_index == 0 || current_bit_index % 8 != 0) return false;
    return buf[current_bit_index / 8 - 1] == '\0';
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    receiver_state_t state = STATE_LISTEN;

    int tmp;
    bool zero, one;
    int index = 0;

    debug_print("LISTEN");
    printf("receiving: ");

    for (;;) {
        switch (state) {
            case STATE_LISTEN:
                if (set_is_attacked(buf, REQ)) { debug_print("LISTEN->READ"); state = STATE_READ; }
                break;

            case STATE_READ:
                tmp  = set_is_attacked2(buf, DATA_0, DATA_1);
                zero = tmp & 0b10;
                one  = tmp & 0b01;

                if ((zero && one) || !(zero || one)) continue;
                if (one) set_nth_bit(message_buf, index, 1);
                index++;

                state = STATE_ACK; debug_print("READ->ACK");
                break;

            case STATE_ACK:
                if (!set_attack_and_probe(buf, ACK, REQ)) {
                    if (index % 8 == 0 && index != 0) printf("%c", message_buf[index / 8 - 1]);
                    if (message_done(message_buf, index)) { printf("receiving: "); }

                    state = STATE_LISTEN; debug_print("ACK->LISTEN");
                }
                break;
        }
    }
}