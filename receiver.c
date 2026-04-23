#include "shared.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));
static uint8_t msg[MSG_LEN] = {0};

// duff-style zero-byte detection (ignore top byte)
bool has_nul_byte(uint64_t v) {
    v |= 0xFF00000000000000ULL;
    return ((v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL) != 0;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    for (;;) {
        while (!is_attacked(buf, REQ));
        printf("receiving: ");

        for (int i = 0; i < MSG_LEN - BYTES_PER_PACKET; i += BYTES_PER_PACKET) {
            mask_t which = which_attacked(buf) >> 2;
            memcpy((void*)&msg[i], (uint8_t*)&which, BYTES_PER_PACKET); // little-endian
            msg[i + BYTES_PER_PACKET] = '\0';
            printf("%s", &msg[i]);

            while (attack_and_probe(buf, BIT(ACK), REQ));
            // invariant: if we don't break, REQ will be held high since data is left
            if (has_nul_byte(which)) break;
        }
    }
}