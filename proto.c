#include "proto.h"
#include <stdlib.h>

static inline uint8_t raw_get(volatile uint8_t *raw, uint32_t slot)
{
    return raw[raw_offset(slot)];
}

uint32_t raw_offset(uint32_t slot)
{
    uint32_t page = slot / RAW_SETS_PER_PAGE;
    uint32_t set = 1u + (slot % RAW_SETS_PER_PAGE);
    return page * PAGE_SIZE + set * LINE_SIZE;
}

void fill_preamble(uint8_t preamble[PREAMBLE_BITS])
{
    uint32_t x = 0x3ffu;
    for (uint32_t i = 0; i < PREAMBLE_BITS; i++) {
        preamble[i] = (uint8_t)(x & 1u);
        uint32_t feedback = ((x >> 0) ^ (x >> 3)) & 1u;
        x = (x >> 1) | (feedback << 9);
    }
}

void fill_payload_sync(uint8_t sync[PAYLOAD_SYNC_BITS])
{
    uint32_t x = 0x7fu;
    for (uint32_t i = 0; i < PAYLOAD_SYNC_BITS; i++) {
        sync[i] = (uint8_t)(x & 1u);
        uint32_t feedback = ((x >> 0) ^ (x >> 1)) & 1u;
        x = (x >> 1) | (feedback << 6);
    }
}

void put_u16(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)v;
    buf[1] = (uint8_t)(v >> 8);
}

void put_u32(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)v;
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
}

uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

uint32_t get_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static int64_t preamble_stats(volatile uint8_t *raw,
                              const uint8_t preamble[PREAMBLE_BITS],
                              uint32_t off,
                              uint32_t *ones_sum,
                              uint32_t *zeros_sum,
                              uint32_t *n1,
                              uint32_t *n0)
{
    uint32_t os = 0;
    uint32_t zs = 0;
    uint32_t oc = 0;
    uint32_t zc = 0;

    for (uint32_t i = 0; i < PREAMBLE_BITS; i++) {
        uint32_t sample = raw_get(raw, off + i);
        if (preamble[i]) {
            os += sample;
            oc++;
        } else {
            zs += sample;
            zc++;
        }
    }

    *ones_sum = os;
    *zeros_sum = zs;
    *n1 = oc;
    *n0 = zc;
    return (int64_t)os * zc - (int64_t)zs * oc;
}

static void add_candidate(proto_candidate_t candidates[MAX_CANDIDATES], uint32_t *count,
                          uint32_t off, int64_t score,
                          uint32_t ones_sum, uint32_t zeros_sum,
                          uint32_t n1, uint32_t n0)
{
    if (score < PREAMBLE_SCORE_MIN)
        return;

    if (*count < MAX_CANDIDATES) {
        candidates[*count] = (proto_candidate_t){off, score, ones_sum, zeros_sum, n1, n0};
        (*count)++;
        return;
    }

    uint32_t min_idx = 0;
    int64_t min_score = candidates[0].score;
    for (uint32_t i = 1; i < MAX_CANDIDATES; i++) {
        if (candidates[i].score < min_score) {
            min_idx = i;
            min_score = candidates[i].score;
        }
    }
    if (score > min_score)
        candidates[min_idx] = (proto_candidate_t){off, score, ones_sum, zeros_sum, n1, n0};
}

static int cmp_candidate_desc(const void *a, const void *b)
{
    const proto_candidate_t *ca = (const proto_candidate_t *)a;
    const proto_candidate_t *cb = (const proto_candidate_t *)b;
    if (ca->score < cb->score)
        return 1;
    if (ca->score > cb->score)
        return -1;
    return 0;
}

static uint32_t collect_candidates(volatile uint8_t *raw,
                                   const uint8_t preamble[PREAMBLE_BITS],
                                   proto_candidate_t candidates[MAX_CANDIDATES],
                                   int64_t *best_score)
{
    uint32_t count = 0;
    uint32_t max_off = ACQUIRE_SLOTS - PREAMBLE_BITS - HEADER_SLOTS;
    uint32_t os0, zs0, n10, n00;
    uint32_t os1, zs1, n11, n01;
    int64_t prev2 = preamble_stats(raw, preamble, 0, &os0, &zs0, &n10, &n00);
    int64_t prev1 = preamble_stats(raw, preamble, 1, &os1, &zs1, &n11, &n01);
    *best_score = prev2 > prev1 ? prev2 : prev1;

    for (uint32_t off = 2; off <= max_off; off++) {
        uint32_t os, zs, n1, n0;
        int64_t cur = preamble_stats(raw, preamble, off, &os, &zs, &n1, &n0);
        if (cur > *best_score)
            *best_score = cur;
        if (prev1 >= prev2 && prev1 > cur)
            add_candidate(candidates, &count, off - 1u, prev1, os1, zs1, n11, n01);
        prev2 = prev1;
        prev1 = cur;
        os1 = os;
        zs1 = zs;
        n11 = n1;
        n01 = n0;
    }
    if (prev1 >= prev2)
        add_candidate(candidates, &count, max_off, prev1, os1, zs1, n11, n01);

    qsort(candidates, count, sizeof(candidates[0]), cmp_candidate_desc);
    return count;
}

static uint8_t classify_sum(uint32_t sum, uint32_t reps, const proto_candidate_t *cand)
{
    uint64_t lhs = (uint64_t)sum * 2u * cand->n0 * cand->n1;
    uint64_t rhs = (uint64_t)reps *
                   ((uint64_t)cand->ones_sum * cand->n0 +
                    (uint64_t)cand->zeros_sum * cand->n1);
    return lhs > rhs;
}

static void decode_header(volatile uint8_t *raw, uint32_t bit_base,
                          const proto_candidate_t *cand, uint8_t header[HEADER_BYTES])
{
    for (uint32_t byte = 0; byte < HEADER_BYTES; byte++) {
        uint8_t v = 0;
        for (uint32_t bit = 0; bit < 8u; bit++) {
            uint32_t header_bit = byte * 8u + bit;
            uint32_t rep_base = bit_base + header_bit * HEADER_REPS;
            uint32_t sum = 0;
            for (uint32_t rep = 0; rep < HEADER_REPS; rep++)
                sum += raw_get(raw, rep_base + rep);
            if (classify_sum(sum, HEADER_REPS, cand))
                v |= (uint8_t)(1u << bit);
        }
        header[byte] = v;
    }
}

static int64_t payload_sync_stats(volatile uint8_t *raw, uint32_t bit_base,
                                  const uint8_t sync[PAYLOAD_SYNC_BITS],
                                  uint32_t *ones_sum,
                                  uint32_t *zeros_sum,
                                  uint32_t *n1,
                                  uint32_t *n0)
{
    uint32_t os = 0;
    uint32_t zs = 0;
    uint32_t oc = 0;
    uint32_t zc = 0;
    for (uint32_t i = 0; i < PAYLOAD_SYNC_BITS; i++) {
        uint32_t sample = raw_get(raw, bit_base + i);
        if (sync[i]) {
            os += sample;
            oc++;
        } else {
            zs += sample;
            zc++;
        }
    }
    *ones_sum = os;
    *zeros_sum = zs;
    *n1 = oc;
    *n0 = zc;
    return (int64_t)os * zc - (int64_t)zs * oc;
}

uint32_t proto_collect_headers(volatile uint8_t *raw, proto_decode_state_t *state)
{
    uint8_t preamble[PREAMBLE_BITS];
    uint8_t header[HEADER_BYTES];
    proto_candidate_t candidates[MAX_CANDIDATES];

    fill_preamble(preamble);
    *state = (proto_decode_state_t){
        .min_sync_base = UINT32_MAX,
        .best_sync = INT64_MIN,
    };
    state->candidate_count = collect_candidates(raw, preamble, candidates, &state->best_score);

    for (uint32_t i = 0; i < state->candidate_count; i++) {
        for (int32_t delta = -4; delta <= 4; delta++) {
            if (delta < 0 && candidates[i].off < (uint32_t)(-delta))
                continue;
            uint32_t off = (uint32_t)((int32_t)candidates[i].off + delta);
            if (off + PREAMBLE_BITS + HEADER_SLOTS > ACQUIRE_SLOTS)
                continue;

            proto_candidate_t cand = {0};
            cand.off = off;
            cand.score = preamble_stats(raw, preamble, off,
                                        &cand.ones_sum, &cand.zeros_sum,
                                        &cand.n1, &cand.n0);
            if (cand.score < PREAMBLE_SCORE_MIN || cand.ones_sum <= cand.zeros_sum)
                continue;

            uint32_t hdr_base = off + PREAMBLE_BITS;
            decode_header(raw, hdr_base, &cand, header);
            uint32_t magic = get_u32(&header[0]);
            uint16_t got_len = get_u16(&header[4]);
            uint16_t inv_len = get_u16(&header[6]);
            if (magic != FRAME_MAGIC || (uint16_t)~got_len != inv_len || got_len > MSG_BYTES)
                continue;

            uint32_t sync_base = hdr_base + HEADER_SLOTS + HEADER_GUARD_SLOTS;
            if (state->header_count < MAX_CANDIDATES) {
                state->headers[state->header_count++] = (proto_header_candidate_t){
                    .cand = cand,
                    .frame_off = off,
                    .sync_base = sync_base,
                    .len = got_len,
                    .sync_score = INT64_MIN,
                };
                if (sync_base + PAYLOAD_SYNC_BITS > state->max_sync_end)
                    state->max_sync_end = sync_base + PAYLOAD_SYNC_BITS;
                if (sync_base < state->min_sync_base)
                    state->min_sync_base = sync_base;
            }
        }
    }

    if (state->header_count == 0)
        return 0;

    for (uint32_t i = 0; i < state->header_count; i++) {
        if (state->headers[i].sync_base != state->min_sync_base)
            continue;
        state->threshold = state->headers[i].cand;
        state->frame_off = state->headers[i].frame_off;
        state->payload_start = state->headers[i].sync_base + PAYLOAD_SYNC_BITS;
        state->len = state->headers[i].len;
        break;
    }
    return state->header_count;
}

void proto_choose_payload(volatile uint8_t *raw, proto_decode_state_t *state)
{
    uint8_t sync[PAYLOAD_SYNC_BITS];

    fill_payload_sync(sync);

    uint32_t best_header = 0;
    int64_t best_sync = INT64_MIN;
    for (uint32_t i = 0; i < state->header_count; i++) {
        state->headers[i].sync_score = payload_sync_stats(raw, state->headers[i].sync_base, sync,
                                                          &state->headers[i].sync_ones_sum,
                                                          &state->headers[i].sync_zeros_sum,
                                                          &state->headers[i].sync_n1,
                                                          &state->headers[i].sync_n0);
        if (state->headers[i].sync_score > best_sync) {
            best_sync = state->headers[i].sync_score;
            best_header = i;
        }
    }

    state->threshold = state->headers[best_header].cand;
    state->threshold.ones_sum = state->headers[best_header].sync_ones_sum;
    state->threshold.zeros_sum = state->headers[best_header].sync_zeros_sum;
    state->threshold.n1 = state->headers[best_header].sync_n1;
    state->threshold.n0 = state->headers[best_header].sync_n0;
    state->frame_off = state->headers[best_header].frame_off;
    state->payload_start = state->headers[best_header].sync_base + PAYLOAD_SYNC_BITS;
    state->len = state->headers[best_header].len;
    state->best_sync = best_sync;
}

uint32_t proto_payload_target(const proto_decode_state_t *state)
{
    return state->payload_start + state->len * 8u;
}

void proto_decode_payload(volatile uint8_t *raw, const proto_decode_state_t *state, uint8_t *out)
{
    for (uint32_t byte = 0; byte < state->len; byte++) {
        uint8_t v = 0;
        for (uint32_t bit = 0; bit < 8u; bit++) {
            uint32_t sample = raw_get(raw, state->payload_start + byte * 8u + bit);
            if (classify_sum(sample, 1u, &state->threshold))
                v |= (uint8_t)(1u << bit);
        }
        out[byte] = v;
    }
}
