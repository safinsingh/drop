#include <stdint.h>
#include <string.h>
#include "util.h"
#include "shared.h"

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));
static uint8_t msg[MSG_LEN] = {0};

int main() {
    for (;;) {
        printf("> ");
        fgets((char*)msg, sizeof(msg), stdin);
        int len = strlen((char*)msg) + 1; // send null-terminator

        for (int i = 0; i < len; i += BYTES_PER_PACKET) {
            uint64_t data = *((uint64_t*)(msg + i)) & DATA_MASK;
            while (!attack_and_probe(buf, (data << 2) | BIT(REQ), ACK));
            sleep(TODO); // receiver must see that REQ has been deasserted
        }
        printf("sent: %s", msg);
    }
}
