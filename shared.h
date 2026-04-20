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

int is_l1(int cycles);
int is_l2(int cycles);
void sleep(int cycles);
int rand_range(int min, int max);

#endif