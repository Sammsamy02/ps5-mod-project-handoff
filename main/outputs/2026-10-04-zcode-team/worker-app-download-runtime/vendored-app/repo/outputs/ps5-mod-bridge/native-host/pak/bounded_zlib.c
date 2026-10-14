#include "bounded_zlib.h"
#include <limits.h>

_Static_assert(sizeof(bz_state) <= 512, "typed state must remain bounded");

typedef struct { uintptr_t begin, end; } byte_range;

static int range_for(const void *pointer, size_t bytes, byte_range *range) {
    const uintptr_t start = (uintptr_t)pointer;
    if ((bytes && !pointer) || bytes > UINTPTR_MAX - start) return 0;
    range->begin = start;
    range->end = start + bytes;
    return 1;
}

static int overlap(byte_range a, byte_range b) {
    return a.begin != a.end && b.begin != b.end &&
           a.begin < b.end && b.begin < a.end;
}

static void clear_bytes(void *pointer, size_t count) {
    volatile uint8_t *bytes = (volatile uint8_t *)pointer;
    for (size_t i = 0; i < count; ++i) bytes[i] = 0;
}

static int reserve(bz_budget *budget, uint64_t work, uint64_t steps,
                   uint64_t allocations) {
    if (budget->work_used > budget->work_limit ||
        budget->steps_used > budget->step_limit ||
        budget->allocations_used > budget->allocation_limit ||
        work > budget->work_limit - budget->work_used ||
        steps > budget->step_limit - budget->steps_used ||
        allocations > budget->allocation_limit - budget->allocations_used)
        return 0;
    budget->work_used += work;
    budget->steps_used += steps;
    budget->allocations_used += allocations;
    return 1;
}

static voidpf arena_allocate(voidpf opaque, uInt items, uInt size) {
    bz_state *state = (bz_state *)opaque;
    if (!reserve(state->budget, 0, 0, 1)) {
        state->budget_exhausted = 1;
        return Z_NULL;
    }
    if (!size || !items || (size_t)items > SIZE_MAX / (size_t)size) {
        state->allocation_failed = 1;
        return Z_NULL;
    }
    const size_t bytes = (size_t)items * (size_t)size;
    const size_t alignment = _Alignof(max_align_t);
    const uintptr_t next = (uintptr_t)state->arena + state->arena_used;
    const size_t remainder = (size_t)(next % alignment);
    const size_t padding = remainder ? alignment - remainder : 0;
    const size_t remaining = state->arena_capacity - state->arena_used;
    if (padding > remaining || bytes > remaining - padding) {
        state->allocation_failed = 1;
        return Z_NULL;
    }
    if (!reserve(state->budget, (uint64_t)bytes, 0, 0)) {
        state->budget_exhausted = 1;
        return Z_NULL;
    }
    uint8_t *allocation = state->arena + state->arena_used + padding;
    state->arena_used += padding + bytes;
    clear_bytes(allocation, bytes);
    return allocation;
}

static void arena_release(voidpf opaque, voidpf address) {
    /* No heap or reuse while zlib holds state. The transaction clears the
     * entire arena only after inflateEnd has released all its references. */
    (void)opaque;
    (void)address;
}

static bz_status allocation_status(const bz_state *state) {
    return state->budget_exhausted ? BZ_RESOURCE_LIMIT :
           state->allocation_failed ? BZ_ALLOCATION_FAILURE : BZ_INTERNAL_ERROR;
}

bz_limits bz_default_limits(void) {
    bz_limits limits = { BZ_MAX_BLOCK_BYTES, BZ_MAX_BLOCK_BYTES,
                         BZ_MAX_ARENA_BYTES, 256, 256 };
    return limits;
}

bz_budget bz_default_budget(void) {
    bz_budget budget = { 33554432, 32768, 64, 0, 0, 0 };
    return budget;
}

bz_result bz_decode(const uint8_t *input, size_t input_size,
                    size_t expected_size, uint8_t *output,
                    size_t output_capacity, uint8_t *arena,
                    size_t arena_capacity, bz_state *state,
                    bz_budget *budget, bz_limits limits) {
    bz_result result = { BZ_INVALID_ARGUMENT, 0, 0 };
    if (!state || !budget || !arena || !arena_capacity ||
        (input_size && !input) || (output_capacity && !output) ||
        expected_size > output_capacity ||
        !limits.input_per_step || !limits.output_per_step ||
        limits.input_per_step > BZ_MAX_STEP_BYTES ||
        limits.output_per_step > BZ_MAX_STEP_BYTES)
        return result;
    if (limits.max_input_bytes > BZ_MAX_BLOCK_BYTES ||
        limits.max_output_bytes > BZ_MAX_BLOCK_BYTES ||
        limits.max_arena_bytes > BZ_MAX_ARENA_BYTES ||
        input_size > limits.max_input_bytes ||
        expected_size > limits.max_output_bytes ||
        output_capacity > BZ_MAX_BLOCK_BYTES ||
        arena_capacity > limits.max_arena_bytes) {
        result.status = BZ_RESOURCE_LIMIT;
        return result;
    }
    byte_range ranges[5];
    if (!range_for(input, input_size, &ranges[0]) ||
        !range_for(output, output_capacity, &ranges[1]) ||
        !range_for(arena, arena_capacity, &ranges[2]) ||
        !range_for(state, sizeof(*state), &ranges[3]) ||
        !range_for(budget, sizeof(*budget), &ranges[4]))
        return result;
    for (size_t i = 0; i < 5; ++i)
        for (size_t j = i + 1; j < 5; ++j)
            if (overlap(ranges[i], ranges[j])) return result;

    /* Reserve all transaction setup, cleanup and possible output rollback
     * before touching buffers. Two library steps cover init and eventual end,
     * even if an inner step or allocation exhausts the aggregate budget. */
    const uint64_t cleanup_work = (uint64_t)arena_capacity * 2 +
                                 (uint64_t)sizeof(*state) * 2 + expected_size;
    if (!reserve(budget, cleanup_work, 2, 0)) {
        result.status = BZ_RESOURCE_LIMIT;
        return result;
    }
    clear_bytes(state, sizeof(*state));
    clear_bytes(arena, arena_capacity);
    state->arena = arena;
    state->arena_capacity = arena_capacity;
    state->budget = budget;
    state->stream.zalloc = arena_allocate;
    state->stream.zfree = arena_release;
    state->stream.opaque = state;
    const int initialized = inflateInit2(&state->stream, 15);
    if (initialized != Z_OK) {
        result.status = initialized == Z_MEM_ERROR ? allocation_status(state) : BZ_INTERNAL_ERROR;
    } else {
        size_t consumed = 0;
        size_t produced = 0;
        for (;;) {
            size_t in_offer = input_size - consumed;
            if (in_offer > limits.input_per_step) in_offer = limits.input_per_step;
            const int overflow_probe = produced == expected_size;
            size_t out_offer = overflow_probe ? 1 : expected_size - produced;
            if (out_offer > limits.output_per_step) out_offer = limits.output_per_step;
            if (!reserve(budget, (uint64_t)in_offer + out_offer, 1, 0)) {
                result.status = BZ_RESOURCE_LIMIT;
                break;
            }
            state->stream.next_in = input_size ? input + consumed : Z_NULL;
            state->stream.avail_in = (uInt)in_offer;
            state->stream.next_out = overflow_probe ? &state->overflow_byte : output + produced;
            state->stream.avail_out = (uInt)out_offer;
            const int code = inflate(&state->stream, Z_NO_FLUSH);
            if (state->stream.avail_in > in_offer || state->stream.avail_out > out_offer) {
                result.status = BZ_INTERNAL_ERROR;
                break;
            }
            const size_t took = in_offer - state->stream.avail_in;
            const size_t wrote = out_offer - state->stream.avail_out;
            consumed += took;
            if (overflow_probe && wrote) {
                result.status = BZ_SIZE_MISMATCH;
                break;
            }
            produced += wrote;
            if (code == Z_STREAM_END) {
                result.status = produced != expected_size ? BZ_SIZE_MISMATCH :
                                consumed != input_size ? BZ_TRAILING_INPUT : BZ_OK;
                break;
            }
            if (code == Z_NEED_DICT) {
                result.status = BZ_UNSUPPORTED_DICTIONARY;
                break;
            }
            if (code == Z_MEM_ERROR) {
                result.status = allocation_status(state);
                break;
            }
            if (code == Z_DATA_ERROR) {
                result.status = BZ_MALFORMED;
                break;
            }
            if (code != Z_OK && code != Z_BUF_ERROR) {
                result.status = BZ_INTERNAL_ERROR;
                break;
            }
            if (!took && !wrote) {
                result.status = consumed == input_size ? BZ_TRUNCATED : BZ_MALFORMED;
                break;
            }
        }
        if (inflateEnd(&state->stream) != Z_OK) result.status = BZ_INTERNAL_ERROR;
    }
    if (result.status != BZ_OK) clear_bytes(output, expected_size);
    clear_bytes(arena, arena_capacity);
    clear_bytes(state, sizeof(*state));
    if (result.status == BZ_OK) {
        result.complete = 1;
        result.decoded_size = expected_size;
    }
    return result;
}
