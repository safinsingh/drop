# drop

Blazing-fast SMT-based L1 prime+probe covert channel achieving 1 Mbps throughput and <0.3% BER.

Tuned and tested on an i7-8550u with Hardware Prefetching enabled in an Asus UX430UNR.

Only syscalls used: write (printf) and mlockall (reduce page fault noise).

## How?

Receiver times a pointer chase while sender hammers set 0 in a loop in 2048-cycle windows. To align clocks, sender transmits a fixed preamble + message header while receiver scans for preamble against captured windows. Once the preamble is found, the data begins at a fixed interval offset from this preamble, and is prepended by a fixed magic in case of interval misalignment. Miss-per-interval <-> 0/1 mapping thresholds are determined online based on preamble statistics. Protocol framing designed in collaboration with Codex.

For short messages, the preamble+header+synchronization gaps dominate latency. As message length goes to infinity, bandwidth approaches 1 bit / 2048 cycles, or 2 Mbps on a 4 GHz part. For reference, SOTA is ~10 Mbps, and probably not on an undervolted laptop like mine!

## Previous

For my previous implementation, which used a 4-phase handshake to synchronize clients, check out [safinsingh/drop-old](https://github.com/safinsingh/drop-old).
