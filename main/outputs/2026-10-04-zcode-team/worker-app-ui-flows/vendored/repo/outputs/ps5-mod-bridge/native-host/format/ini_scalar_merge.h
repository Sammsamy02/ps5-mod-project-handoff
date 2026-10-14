#ifndef PM_INI_SCALAR_MERGE_H
#define PM_INI_SCALAR_MERGE_H

#include <stddef.h>
#include <stdint.h>

#define PM_INI_MAX_INPUT_BYTES (1024u * 1024u)
#define PM_INI_MAX_OUTPUT_BYTES (2u * PM_INI_MAX_INPUT_BYTES)
#define PM_INI_MAX_LINES 16384u
#define PM_INI_MAX_LINE_BYTES 16384u
#define PM_INI_MAX_SOURCES 16u
#define PM_INI_MAX_SECTIONS 64u
#define PM_INI_MAX_SECTION_BYTES 255u
#define PM_INI_MAX_WINNERS 4096u
#define PM_INI_HASH_SLOTS 8192u
#define PM_INI_WORKSPACE_CEILING (1024u * 1024u)

typedef struct { const uint8_t *data; size_t size; } pm_ini_bytes;

typedef enum {
    PM_INI_OK = 0, PM_INI_INVALID_ARGUMENT, PM_INI_INPUT_LIMIT,
    PM_INI_SECTION_LIMIT, PM_INI_OVERLAP, PM_INI_INVALID_UTF8, PM_INI_NUL,
    PM_INI_LINE_SYNTAX, PM_INI_HEADER_SYNTAX, PM_INI_SCALAR_SYNTAX,
    PM_INI_ARRAY_UNSUPPORTED, PM_INI_SOURCE_EMPTY, PM_INI_WINNER_LIMIT,
    PM_INI_OUTPUT_LIMIT, PM_INI_NEW_SECTION_BOUNDARY
} pm_ini_status;

typedef struct {
    pm_ini_status status;
    size_t output_bytes, required_output_bytes;
    uint32_t source_assignments, source_duplicates, scalar_winners;
    uint32_t replacements, additions, base_duplicates_removed;
    uint32_t output_lines;
    uint8_t bom_preserved, preferred_crlf;
} pm_ini_result;

/* Internal scratch records are exposed only to give callers an aligned,
 * typed, fixed-size object. They contain offsets/indices, never pointers.
 * Callers must not inspect or modify them during a merge. */
typedef struct {
    uint32_t offset, raw_size, body_size, key_offset, value_offset;
    uint16_t left_size, key_size, value_size;
    int16_t section;
    uint8_t newline_size;
} pm_ini_scratch_line;

typedef struct {
    uint32_t body_offset, body_size, key_offset, value_offset, ordinal;
    int32_t base_last;
    uint16_t key_size, value_size, base_count;
    uint8_t section, source, emitted;
} pm_ini_scratch_winner;

typedef struct {
    pm_ini_scratch_line lines[PM_INI_MAX_LINES];
    pm_ini_scratch_winner winners[PM_INI_MAX_WINNERS];
    uint16_t hash_slots[PM_INI_HASH_SLOTS];
    uint16_t order[PM_INI_MAX_WINNERS], sort_temp[PM_INI_MAX_WINNERS];
    int32_t section_last[PM_INI_MAX_SECTIONS];
    uint32_t line_count, winner_count, ordinal;
    uint8_t bom_size, preferred_crlf;
} pm_ini_workspace;

/* Pure C11: no allocation, filesystem, network or engine access. Immutable
 * input bytes, view tables and selected names must be disjoint from output,
 * result and workspace; those writable ranges must also be mutually disjoint.
 * An overlap refusal leaves every writable range untouched. Other refusals
 * may change scratch/result, but output is written only after full validation
 * and exact capacity measurement. All pointer ranges must be caller-valid.
 * No input pointers are retained after return. This is an explicit scalar
 * syntax subset, not a full Unreal configuration/schema interpreter. */
pm_ini_status pm_ini_merge(const pm_ini_bytes *base,
                          const pm_ini_bytes *sources, size_t source_count,
                          const pm_ini_bytes *sections, size_t section_count,
                          pm_ini_workspace *workspace,
                          uint8_t *output, size_t output_capacity,
                          pm_ini_result *result);

const char *pm_ini_status_name(pm_ini_status status);

#endif
