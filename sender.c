#include <stdint.h>
#include "util.h"
#include "shared.h"

#define SET_PRESSURE 1000
#define NUM_EVICTORS 128
#define RETRIES 5000

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));

// set in [0, 64]
static inline void prime_set(int set) {
    for (int i = 0; i < SET_PRESSURE; i++) {
        buf[set * LINE_SIZE + rand_range(1, NUM_EVICTORS - 1) * SET_STRIDE] = 0;
    }
}

int main() {
    srand(time(NULL));
    CYCLES c1 = 0, c2 = 0;

    for (;;) {
        prime_set(0);
    }
}
