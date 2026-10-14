#ifndef MB_ARCHIVE_CONFIG_CANDIDATE_SELECTOR_H
#define MB_ARCHIVE_CONFIG_CANDIDATE_SELECTOR_H

#include "intake.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1 1u
#define MB_ARCHIVE_CONFIG_SELECTOR_MAX_BASENAME 255u
#define MB_ARCHIVE_CONFIG_SELECTOR_MAX_SOURCE_BYTES UINT64_C(1048576)

enum mb_archive_config_selector_status_v1 {
    MB_ARCHIVE_CONFIG_SELECTOR_OK = 0,
    MB_ARCHIVE_CONFIG_SELECTOR_BAD_ARGUMENT = 1,
    MB_ARCHIVE_CONFIG_SELECTOR_INCOMPLETE = 2,
    MB_ARCHIVE_CONFIG_SELECTOR_INVALID_INVENTORY = 3,
    MB_ARCHIVE_CONFIG_SELECTOR_NO_CANDIDATE = 4,
    MB_ARCHIVE_CONFIG_SELECTOR_AMBIGUOUS = 5,
    MB_ARCHIVE_CONFIG_SELECTOR_SOURCE_TOO_LARGE = 6
};

typedef struct mb_archive_config_selector_profile_v1 {
    uint32_t struct_size, abi_version;
    const char *basename;
    uint32_t basename_bytes, reserved;
    uint64_t max_source_bytes;
    uint8_t target_token[32];
} mb_archive_config_selector_profile_v1;

typedef struct mb_archive_config_candidate_v1 {
    uint32_t struct_size, abi_version, status, complete;
    uint32_t candidate_count, row_index;
    uint64_t member_bytes;
    uint8_t member_sha256[32], source_sha256[32], inventory_sha256[32], target_token[32];
    uint32_t format_validated, platform_compatible, installation_supported, runtime_supported;
} mb_archive_config_candidate_v1;

/* The inventory, rows, and names are immutable and caller-owned for the call.
 * Every control/input/output span must be disjoint. Inputs must be a complete
 * MB_ARCHIVE_OK intake result. `basename` is one ASCII path component, and the
 * expected token must equal the inventory token. Result identity fields bind
 * the selection to that inventory/source/profile token; the token is not
 * target compatibility evidence. A BAD_ARGUMENT preflight return leaves
 * result storage untouched, including an invalid-cap/profile call whose
 * result aliases an input. Inspect the return code before result fields:
 * unchanged storage may contain a previous result. Admitted calls reset the
 * result before selection. No allocation, I/O, retained pointer, path
 * destination, parsing, transformation, installation, or runtime authority. */
int mb_archive_config_select_v1(
    const mb_archive_inventory_v1 *inventory,
    const mb_archive_row_v1 *rows, uint32_t row_capacity,
    const char *names, uint32_t name_capacity,
    const mb_archive_config_selector_profile_v1 *profile,
    mb_archive_config_candidate_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
