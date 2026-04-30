== drop ==

unidirectional chat app built on an L1 prime+probe covert channel
no syscalls (besides write for printf); hardware prefetcher enabled
implemented, tested, and tuned on an i7-8550u asus ux430unr
achieves ~1kB/sec throughput and ~100% accuracy!

inspired by chris fletcher's dead drop lab (https://cwfletcher.github.io/teaching/)
