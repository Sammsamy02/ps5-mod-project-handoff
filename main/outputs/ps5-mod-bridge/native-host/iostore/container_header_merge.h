#ifndef CCH_CONTAINER_HEADER_MERGE_H
#define CCH_CONTAINER_HEADER_MERGE_H

#include <stddef.h>
#include <stdint.h>

#define CCH_HEADER_BYTES_CAP (64u * 1024u * 1024u)
#define CCH_MAX_PACKAGES 262144u
#define CCH_MAX_REFS (2u * CCH_MAX_PACKAGES)
#define CCH_MAX_RANGES (2u * CCH_MAX_PACKAGES + 1u)

struct cch_package_ref {
    uint64_t package_id;
    uint32_t source_index;
    uint8_t source; /* 0=base, 1=mod */
    uint8_t reserved[3];
};

struct cch_byte_range {
    uint32_t begin;
    uint32_t end;
};

struct cch_workspace {
    struct cch_package_ref *package_refs;
    size_t package_ref_capacity;
    struct cch_byte_range *ranges;
    size_t range_capacity;
};

enum cch_status {
    CCH_OK = 0,
    CCH_OUTPUT_TOO_SMALL = 1,
    CCH_INVALID = 2,
    CCH_UNSUPPORTED = 3
};

struct cch_result {
    size_t required_bytes;
    size_t output_bytes;
    uint32_t base_package_count;
    uint32_t mod_package_count;
    uint32_t merged_package_count;
    uint32_t replaced_package_count;
    uint32_t added_package_count;
    uint64_t base_container_id;
    uint64_t mod_container_id;
    uint8_t container_id_preserved;
};

/*
 * Inputs and caller storage must be disjoint. A null output with capacity zero
 * performs a full validation/measure pass and returns CCH_OUTPUT_TOO_SMALL on
 * valid input. Reinvoke with required_bytes to emit. No heap or retained data.
 */
enum cch_status cch_merge_v5(const uint8_t *base, size_t base_bytes,
                            const uint8_t *mod, size_t mod_bytes,
                            struct cch_workspace *workspace,
                            uint8_t *output, size_t output_capacity,
                            struct cch_result *result);

#endif
