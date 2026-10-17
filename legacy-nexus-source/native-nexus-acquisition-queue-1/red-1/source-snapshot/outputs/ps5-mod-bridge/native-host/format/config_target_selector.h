#ifndef MB_UNREAL_CONFIG_TARGET_SELECTOR_H
#define MB_UNREAL_CONFIG_TARGET_SELECTOR_H

#include "pak_stream_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_UNREAL_CONFIG_TARGET_SELECTOR_ABI_V1 1u
#define MB_UNREAL_CONFIG_TARGET_SELECTOR_MAX_FILES MB_PAK_STREAM_MAX_FILES
#define MB_UNREAL_CONFIG_TARGET_SELECTOR_MAX_PLATFORM_BYTES 32u

typedef enum mb_unreal_config_target_status_v1 {
    MB_UNREAL_CONFIG_TARGET_OK = 0,
    MB_UNREAL_CONFIG_TARGET_BAD_ARGUMENT = 1,
    MB_UNREAL_CONFIG_TARGET_INVALID_INPUT = 2,
    MB_UNREAL_CONFIG_TARGET_LIMIT = 3,
    MB_UNREAL_CONFIG_TARGET_NO_PROJECT_ROOT = 4,
    MB_UNREAL_CONFIG_TARGET_AMBIGUOUS_PROJECT_ROOT = 5,
    MB_UNREAL_CONFIG_TARGET_NO_PLATFORM_TARGET = 6,
    MB_UNREAL_CONFIG_TARGET_AMBIGUOUS_PLATFORM_TARGET = 7
} mb_unreal_config_target_status_v1;

typedef struct mb_unreal_config_target_profile_v1 {
    uint32_t struct_size, abi_version;
    const uint8_t *platform;
    uint32_t platform_bytes, reserved;
} mb_unreal_config_target_profile_v1;

typedef struct mb_unreal_config_target_result_v1 {
    uint32_t struct_size, abi_version;
    mb_unreal_config_target_status_v1 status;
    uint32_t complete;
    /* Matching anchor/target rows; any duplicate or case alias is counted and
     * refused rather than selecting the first row. */
    uint32_t root_count, target_count;
    uint32_t target_row_index, target_path_bytes;
    uint8_t target_path[MB_PAK_STREAM_MAX_PATH_BYTES];
} mb_unreal_config_target_result_v1;

/* Pure path-layout candidate selector. The file array and platform span are
 * immutable, caller-owned records from a validated logical inventory. The
 * selector recognizes exactly ROOT/Config/DefaultEngine.ini anchors and
 * ROOT/Platforms/PLATFORM/Config/PLATFORMEngine.ini targets, with ASCII-folded
 * segment matching. The reserved `Engine` root is excluded. Success requires
 * exactly one anchor row and one target row; duplicate/case-alias rows refuse.
 * The original target path bytes and row index are copied into the owning
 * result. This identifies only a path candidate, not config load precedence,
 * platform compatibility, installation, or runtime support.
 *
 * Result, file descriptors, profile, platform and admitted path spans must be
 * disjoint. BAD_ARGUMENT and over-limit count preflights leave result storage
 * untouched; admitted calls clear result before validating/scanning paths.
 * Callers must keep files/profile immutable through return and inspect status
 * before using target_path. No heap or I/O. */
mb_unreal_config_target_status_v1 mb_unreal_config_target_select_v1(
    const mb_pak_stream_file_v1 *files, uint32_t file_count,
    const mb_unreal_config_target_profile_v1 *profile,
    mb_unreal_config_target_result_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
