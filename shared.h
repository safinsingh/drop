#include <stdint.h>

// kaby lake l1 d$:
// size=32KiB, ways=8, linesz=64 (sets=64, set stride=4096)

#define SET_STRIDE 4096
#define LINE_SIZE 64
#define NUM_SETS 64

#define EVICTORS 16
#define ATTACK_BUF_SIZE (SET_STRIDE * EVICTORS)
#define PAGE_SIZE 4096
#define ATTACK_BUF_PAGES (ATTACK_BUF_SIZE / PAGE_SIZE)

#define CHASE_LENGTH 8
#define CHASE_CANDIDATES 16
#define VICTIM_BUF_SIZE (SET_STRIDE * CHASE_CANDIDATES)

#define L1_THRESHOLD 180
#define MISS_THRESHOLD 5


// set 0=used, 1-63=unused. used for prefaulting.
#define SAFE_SET 1

uint32_t rdtsc_lo();
uint32_t rdtscp_lo();
