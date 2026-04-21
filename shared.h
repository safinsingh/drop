#include <stdint.h>
#include <stdbool.h>

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
#define LINE_SIZE 64
#define SET_STRIDE (L1_SIZE / L1_WAYS)
#define FENCE do { __asm__ __volatile__ ("lfence" ::: "memory"); } while (0);
#define NUM_EVICTORS 300

#ifdef DEBUG
    #define debug_print(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
    #define debug_print(fmt, ...)
#endif

// wire <=> cache set primed
typedef enum {
    // Sender -> Receiver
    REQ,
    ACK,
    DATA_0,
    DATA_1
} msg_t;

bool is_l1(int cycles);
bool is_l2(int cycles);
void sleep(int cycles);

void set_attack(volatile uint8_t* buf, int set);

bool set_is_attacked(volatile uint8_t* buf, int set);
int set_is_attacked2(volatile uint8_t* buf, int set1, int set2);

bool set_attack_and_probe(volatile uint8_t* buf, int attack_set, int probe_set);
bool set_attack_and_probe2(volatile uint8_t* buf, int attack_set1, int attack_set2, int probe_set);

int get_nth_bit(char* buf, int n);
void set_nth_bit(char* buf, int n, int value);

#endif