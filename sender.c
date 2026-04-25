#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "shared.h"

static volatile uint8_t buf[SET_STRIDE * NUM_EVICTORS] __attribute__((aligned(SET_STRIDE)));
static uint8_t msg[MSG_LEN] = {0};

int main() {
    for (;;) {
        printf("> ");
        if (fgets((char*)msg, sizeof(msg), stdin) == NULL) continue;
        int len = strlen((char*)msg) + 1; // send null-terminator

        clock_t begin = clock();
        for (int i = 0; i < len; i += BYTES_PER_PACKET) {
            uint64_t data = *((uint64_t*)(msg + i)) & DATA_MASK;
            while (!attack_and_probe(buf, (data << 2) | BIT(REQ), ACK));
            // deassert REQ & wait for receiver to drop ACK
            while (is_attacked(buf, ACK));
        }
        clock_t end = clock();
        double time = (double)(end - begin) / CLOCKS_PER_SEC;
        double thpt = (double)len / time;

        printf("wrote %d bytes in %.3lf sec (%.3lf bytes/sec)\n", len, time, thpt);
    }
}
