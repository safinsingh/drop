== drop ==

unidirectional chat app built on an L1 prime+probe side-channel
no syscalls (besides write for printf); hardware prefetcher enabled
implemented, tested, and tuned on an i7-8550u asus ux430unr
achieves ~70byte/sec throughput and ~99%+ accuracy!

inspired by chris fletcher's dead drop lab (https://cwfletcher.github.io/teaching/)