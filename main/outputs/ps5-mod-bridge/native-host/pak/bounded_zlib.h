#ifndef BOUNDED_ZLIB_EXPERIMENT_H
#define BOUNDED_ZLIB_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>
#include "zlib.h"

#define BZ_MAX_BLOCK_BYTES ((size_t)1048576)
#define BZ_MAX_ARENA_BYTES ((size_t)65536)
#define BZ_MAX_STEP_BYTES 4096U

typedef enum {
    BZ_OK = 0,
    BZ_INVALID_ARGUMENT,
    BZ_MALFORMED,
    BZ_TRUNCATED,
    BZ_SIZE_MISMATCH,
    BZ_TRAILING_INPUT,
    BZ_UNSUPPORTED_DICTIONARY,
    BZ_ALLOCATION_FAILURE,
    BZ_RESOURCE_LIMIT,
    BZ_INTERNAL_ERROR
} bz_status;

typedef struct {
    size_t max_input_bytes;
    size_t max_output_bytes;
    size_t max_arena_bytes;
    unsigned input_per_step;
    unsigned output_per_step;
} bz_limits;

/* One caller-owned aggregate budget may cover many blocks. Reservations,
 * including those of failed operations, are never refunded or reset here.
 * Work is conservative offered/zeroed byte units, not elapsed CPU time. */
typedef struct {
    uint64_t work_limit;
    uint64_t step_limit;
    uint64_t allocation_limit;
    uint64_t work_used;
    uint64_t steps_used;
    uint64_t allocations_used;
} bz_budget;

/* Typed storage belongs to the caller and is cleared after decode. The fields
 * are implementation state, never a public partial decoded result. */
typedef struct {
    z_stream stream;
    uint8_t *arena;
    size_t arena_capacity;
    size_t arena_used;
    bz_budget *budget;
    int allocation_failed;
    int budget_exhausted;
    uint8_t overflow_byte;
} bz_state;

typedef struct {
    bz_status status;
    int complete;
    size_t decoded_size;
} bz_result;

bz_limits bz_default_limits(void);
bz_budget bz_default_budget(void);

/* Input, output's entire capacity, arena, state and budget must be disjoint
 * readable/writable objects of the stated sizes (input is immutable). No
 * pointer is retained. Output may be inspected/published only when complete.
 * Preflight rejection leaves buffers untouched; after decoding starts every
 * failure clears output[0..expected_size), arena and state. Output's capacity
 * beyond expected_size is untouched. One zlib stream only; no trailing input.
 * Cooperative finite budgets act between upstream calls, not as an interrupt.
 * Build adapter/upstream with Z_SOLO, NO_GZIP, ZLIB_CONST and, for the caller
 * byte-arena storage overlays, the tested Clang -fno-strict-aliasing profile.
 */
bz_result bz_decode(const uint8_t *input, size_t input_size,
                    size_t expected_size, uint8_t *output,
                    size_t output_capacity, uint8_t *arena,
                    size_t arena_capacity, bz_state *state,
                    bz_budget *budget, bz_limits limits);

#endif
