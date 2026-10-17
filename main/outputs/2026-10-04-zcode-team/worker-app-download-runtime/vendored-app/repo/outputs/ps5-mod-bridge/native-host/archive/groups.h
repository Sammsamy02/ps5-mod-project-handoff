#ifndef MODBRIDGE_NATIVE_ARCHIVE_GROUPS_H
#define MODBRIDGE_NATIVE_ARCHIVE_GROUPS_H

#include "intake.h"

#define MB_ARCHIVE_GROUPS_ABI_V1 1u
#define MB_ARCHIVE_GROUPS_MAX_GROUPS MB_ARCHIVE_MAX_ENTRIES
#define MB_ARCHIVE_GROUPS_MAX_MEMBERS MB_ARCHIVE_MAX_ENTRIES
#define MB_ARCHIVE_GROUPS_MAX_KEY_BYTES MB_ARCHIVE_MAX_NAME_BYTES
#define MB_ARCHIVE_GROUPS_MAX_PATH_WORK MB_ARCHIVE_MAX_PATH_WORK

enum mb_archive_groups_status_v1 {
    MB_ARCHIVE_GROUPS_OK = 0,
    MB_ARCHIVE_GROUPS_BAD_ARGUMENT = 1,
    MB_ARCHIVE_GROUPS_INCOMPLETE_INVENTORY = 2,
    MB_ARCHIVE_GROUPS_LIMIT = 3,
    MB_ARCHIVE_GROUPS_DUPLICATE_ROLE = 4,
    MB_ARCHIVE_GROUPS_INVALID_ROW = 5
};

enum mb_archive_group_kind_v1 {
    MB_ARCHIVE_GROUP_PAK = 1,
    MB_ARCHIVE_GROUP_IOSTORE = 2
};

enum mb_archive_group_status_v1 {
    MB_ARCHIVE_GROUP_STATUS_CANDIDATE = 1,
    MB_ARCHIVE_GROUP_STATUS_UNRESOLVED = 2
};

enum mb_archive_group_role_v1 {
    MB_ARCHIVE_GROUP_ROLE_PAK = 1,
    MB_ARCHIVE_GROUP_ROLE_UCAS = 2,
    MB_ARCHIVE_GROUP_ROLE_UTOC = 3
};

enum mb_archive_group_reason_v1 {
    MB_ARCHIVE_GROUP_REASON_MEMBER_MISSING = 1u << 0,
    MB_ARCHIVE_GROUP_REASON_CASE_AMBIGUITY = 1u << 1,
    MB_ARCHIVE_GROUP_REASON_PARTITION_UNRESOLVED = 1u << 2
};

typedef struct mb_archive_group_member_v1 {
    uint32_t row_index;
    uint32_t role;
} mb_archive_group_member_v1;

typedef struct mb_archive_group_v1 {
    uint32_t parent_offset, parent_length;
    uint32_t stem_offset, stem_length;
    uint32_t member_begin, member_count;
    uint32_t kind, status, reason_flags, members_complete;
    uint32_t validation_unverified;
    uint32_t installation_supported, runtime_supported, format_validated;
    uint32_t reserved;
} mb_archive_group_v1;

typedef struct mb_archive_groups_result_v1 {
    uint32_t struct_size, abi_version, status, complete;
    uint32_t group_count, member_count, key_bytes, reserved;
    uint64_t path_work_used;
    uint32_t installation_supported, runtime_supported, format_validated, reserved2;
    uint8_t source_sha256[32], inventory_sha256[32], target_token[32];
} mb_archive_groups_result_v1;

#ifdef __cplusplus
extern "C" {
#endif

/* Pure metadata grouping over one complete intake result. The caller asserts
 * that inventory, rows and names remain immutable for the call; this assertion
 * is not independently authenticated. Parent and stem are copied to key_bytes.
 * Members retain original row indices and role codes, not archive-path-based
 * filesystem destinations. The result identity tuple binds the deterministic
 * group/member output to this exact source, inventory and target. No separate
 * group ID is created.
 *
 * Only regular rows are considered. Group order is bytewise parent then exact
 * stem; member order is pak, ucas, utoc. Recognized suffixes are .pak/.utoc/
 * .ucas with ASCII-only case folding. Case-folded stem collisions, missing
 * required companions, and a same-parent `_s<digits>.ucas` beside a base UTOC
 * make affected groups unresolved. Duplicate roles at one exact key fail the
 * operation, matching the committed host grouping error. These names remain
 * candidates only: format validity, compatibility, install and runtime are
 * unverified/false.
 *
 * All pointers, capacities, alignments and known aliases are checked before
 * any write. A BAD_ARGUMENT return is no-write. After preflight, any failure
 * clears the entire supplied output arrays, key arena, and sort scratch; only
 * OK publishes a complete grouping result. The caller must discard its prior
 * result/backing on every non-OK return. Input inventory must have exact ABI,
 * MB_ARCHIVE_OK status, complete==1 and installation_supported==0.
 */
int mb_archive_groups_v1(
    const mb_archive_inventory_v1 *inventory,
    const mb_archive_row_v1 *rows, uint32_t row_capacity,
    const char *names, uint32_t name_capacity,
    uint32_t *sort_indices, uint32_t sort_capacity,
    mb_archive_group_v1 *groups, uint32_t group_capacity,
    mb_archive_group_member_v1 *members, uint32_t member_capacity,
    char *key_bytes, uint32_t key_capacity,
    mb_archive_groups_result_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
