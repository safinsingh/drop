#include <stdint.h>
#include <stdbool.h>
#include "util.h"

// Processor -> L1 || TLB -> L2 -> L3
//
// 64-byte lines, 4KiB pages
//
// L1: 32KiB, 8-way -- 64-bit address: T=48, I=6, O=6
// L2: 256KiB, 4-way
// L3: 8MiB, 16-way

#ifndef _SHARED_H
#define _SHARED_H

#define KIB 1024
#define L1_SIZE 32 * KIB
#define L1_WAYS 8
#define L1_SETS 64
#define LINE_SIZE 64
#define SET_STRIDE (L1_SIZE / L1_WAYS)
#define NUM_EVICTORS 256
#define BYTES_PER_PACKET 7
#define MSG_LEN 1029 // 7 | 1029
#define BIT(x) ((uint64_t)1 << (x))
#define TEST_BIT(x, b) (((x) >> (b)) & 0b1)
#define DATA_PACKET(x) ((x << 2) | BIT(REQ))
#define WHICH_MIN 2
#define WHICH_MAX (WHICH_MIN + BYTES_PER_PACKET * 8)
#define TODO 0

#define TEST_CONST 0b1011101101001110101010111100100UL
#define TEST_CONST2 0b0000000001111000011110010110000101111010011110010111011101111001UL

#ifdef DEBUG
    #define debug_print(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
    #define debug_print(fmt, ...)
#endif

// wire (=== cache set) mapping:
// 0:1   = REQ, ACQ (for 4-phase handshake)
// 2:57  = 7 bytes of data
// 57:63 = unused
#define DATA_MASK 0x00FFFFFFFFFFFFFFULL
typedef uint64_t mask_t;
typedef enum {
    REQ = 0,
    ACK
} msg_t;

// Rough (observed) latencies:
// L1: ~20 (0-30)
// L2: ~40 (30+)
inline bool is_l1(int cycles) { return cycles < 30; }
inline bool is_l2(int cycles) { return !is_l1(cycles); }

// IN UR FACE DCU-IP PREFETCHER!!!
static inline int rand_line_offset() {
    static uint32_t state = 42424242;
    state *= 0x9E3779B1; 
    return state >> 26;
}
static inline uint64_t set_index(int set) { return set * LINE_SIZE + rand_line_offset(); }
static inline uint64_t set_index_no_offset(int set) { return set * LINE_SIZE; }
static inline CYCLES set_access_time(volatile uint8_t* buf, int set) { return measure_one_block_access_time((uint64_t)&buf[set_index(set)]); }

void spin(int cycles);
void print_binary64(uint64_t n);

bool is_attacked(volatile uint8_t* buf, int set);
mask_t which_attacked(volatile uint8_t* buf);
bool attack_and_probe(volatile uint8_t* buf, mask_t attack_sets, int probe_set);

#endif