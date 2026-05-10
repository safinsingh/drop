#include <immintrin.h>
#include <stdio.h>
int main() { printf("%llu\n", (unsigned long long)__rdtsc()); return 0; }
