#include <stdint.h>

uint32_t rdtsc_lo() {
    uint32_t lo;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo) : : "edx");
    return lo;
}

// performs load serialization. slight cycle count improvement over lfence + rdtsc
uint32_t rdtscp_lo() {
    uint32_t lo;
    __asm__ __volatile__ ("rdtscp" : "=a" (lo) : : "edx", "ecx");
    return lo;
}
