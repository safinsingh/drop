#ifndef DROP2_PROTO_H
#define DROP2_PROTO_H

#include "shared.h"
#include <stdint.h>

// Fast framed stream: protected acquisition/header, raw payload at 1 bit/slot.
#define MSG_BYTES 8192u
#define PREAMBLE_BITS 511u
#define HEADER_BYTES 8u
#define HEADER_BITS (HEADER_BYTES * 8u)
#define HEADER_REPS 9u
#define HEADER_SLOTS (HEADER_BITS * HEADER_REPS)
#define HEADER_GUARD_SLOTS 15360u
#define PAYLOAD_SYNC_BITS 127u
#define ACQUIRE_MARGIN_SLOTS 4096u
#define ACQUIRE_SLOTS (ACQUIRE_MARGIN_SLOTS + PREAMBLE_BITS + HEADER_SLOTS)
#define MAX_CAPTURE_SLOTS (ACQUIRE_SLOTS + HEADER_GUARD_SLOTS + PAYLOAD_SYNC_BITS + MSG_BYTES * 8u + 2048u)

#define FRAME_MAGIC 0x314c4350u
#define PREAMBLE_SCORE_MIN 20000
#define MAX_CANDIDATES 128u

// Raw per-slot counts are stored only in cache sets 1..63, never set 0.
#define RAW_SETS_PER_PAGE (NUM_SETS - 1u)
#define RAW_PAGES ((MAX_CAPTURE_SLOTS + RAW_SETS_PER_PAGE - 1u) / RAW_SETS_PER_PAGE)
#define RAW_STORE_SIZE (RAW_PAGES * PAGE_SIZE)

typedef struct {
    // Candidate frame start, measured in raw 2048-cycle sample slots.
    uint32_t off;
    // Preamble correlation score: larger means 1-slots looked hotter than 0-slots.
    int64_t score;
    // Soft-sample totals over known preamble-1 and preamble-0 positions.
    uint32_t ones_sum;
    uint32_t zeros_sum;
    // Counts of known 1 and 0 bits in the preamble window.
    uint32_t n1;
    uint32_t n0;
} proto_candidate_t;

typedef struct {
    // Preamble candidate whose repeated header decoded cleanly.
    proto_candidate_t cand;
    // Frame start offset; retained for trace/debug output.
    uint32_t frame_off;
    // Expected start of the payload sync marker in raw sample slots.
    uint32_t sync_base;
    // Payload byte count decoded from the header.
    uint32_t len;
    // Sync correlation score; used to choose among valid header candidates.
    int64_t sync_score;
    // Soft-sample totals over known sync-1 and sync-0 positions.
    uint32_t sync_ones_sum;
    uint32_t sync_zeros_sum;
    // Counts of known 1 and 0 bits in the sync window.
    uint32_t sync_n1;
    uint32_t sync_n0;
} proto_header_candidate_t;

typedef struct {
    proto_candidate_t threshold;
    proto_header_candidate_t headers[MAX_CANDIDATES];
    uint32_t frame_off;
    uint32_t payload_start;
    uint32_t len;
    uint32_t candidate_count;
    uint32_t header_count;
    uint32_t max_sync_end;
    uint32_t min_sync_base;
    int64_t best_score;
    int64_t best_sync;
} proto_decode_state_t;

uint32_t raw_offset(uint32_t slot);
void fill_preamble(uint8_t preamble[PREAMBLE_BITS]);
void fill_payload_sync(uint8_t sync[PAYLOAD_SYNC_BITS]);
void put_u16(uint8_t *buf, uint16_t v);
void put_u32(uint8_t *buf, uint32_t v);
uint16_t get_u16(const uint8_t *buf);
uint32_t get_u32(const uint8_t *buf);
uint32_t proto_collect_headers(volatile uint8_t *raw, proto_decode_state_t *state);
void proto_choose_payload(volatile uint8_t *raw, proto_decode_state_t *state);
uint32_t proto_payload_target(const proto_decode_state_t *state);
void proto_decode_payload(volatile uint8_t *raw, const proto_decode_state_t *state, uint8_t *out);

#endif
