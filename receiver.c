#include "shared.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>

#define BURST_LENGTH 8000

typedef enum {
    STATE_WAITING,
    STATE_
} receiver_state_t;

static volatile uint8_t buf[L1_SIZE] __attribute__((aligned(SET_STRIDE)));

int main() {
    for (;;) {
        CYCLES c = 0;
        for (int i = 0; i < BURST_LENGTH; i++) {
            buf[0] = 0;

            sleep(3000);

            FENCE
            c += measure_one_block_access_time((uint64_t)buf);
            FENCE
        }

        bool ready = is_l1(c / BURST_LENGTH);
        printf("Resident? %d\n", ready);
    }
}