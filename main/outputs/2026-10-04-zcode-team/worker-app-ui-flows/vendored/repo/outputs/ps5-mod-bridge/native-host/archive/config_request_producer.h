#ifndef MB_CONFIG_REQUEST_PRODUCER_H
#define MB_CONFIG_REQUEST_PRODUCER_H
#include "config_request.h"
#include <stddef.h>
#include <stdint.h>
#define MB_CONFIG_REQUEST_PRODUCER_ABI_V1 1u
enum mb_config_request_producer_status_v1 {
 MB_CONFIG_PRODUCER_OK=0, MB_CONFIG_PRODUCER_BAD_ARGUMENT=1,
 MB_CONFIG_PRODUCER_INVALID=2, MB_CONFIG_PRODUCER_LIMIT=3,
 MB_CONFIG_PRODUCER_BUFFER_TOO_SMALL=4, MB_CONFIG_PRODUCER_HASH_FAILED=5
};
/* Caller-owned immutable values. download_complete=1 asserts a separately
 * completed, owned raw-file receipt; this encoder performs no file/HTTP checks.
 * No selected-member identity or caller-chosen output path is accepted. Refs and
 * ID are bounded NUL-terminated strings; profile strings use explicit lengths. */
typedef struct mb_config_request_producer_params_v1 {
 uint32_t struct_size,abi_version,download_complete,role;
 uint64_t source_bytes,base_bytes,max_member_bytes,path_hash_seed;
 uint8_t source_sha256[32],base_sha256[32],target_token[32];
 char request_id[33],source_ref[MB_CONFIG_REF_BYTES],base_ref[MB_CONFIG_REF_BYTES];
 char context_ref[MB_CONFIG_REF_BYTES];
 uint32_t basename_bytes,target_entry_bytes,mount_bytes,section_count;
 char basename[MB_CONFIG_MAX_BASENAME_BYTES+1];
 char target_entry[MB_CONFIG_MAX_PATH_BYTES+1],mount[MB_CONFIG_MAX_PATH_BYTES+1];
 uint32_t section_bytes[MB_CONFIG_MAX_SECTIONS];
 char sections[MB_CONFIG_MAX_SECTIONS][MB_CONFIG_MAX_SECTION_BYTES+1];
} mb_config_request_producer_params_v1;
typedef struct mb_config_request_producer_result_v1 {
 uint32_t struct_size,abi_version,status,complete;
 uint32_t request_bytes,context_bytes;
 uint8_t context_sha256[32];
 uint32_t installation_supported,runtime_supported,game_acceptance_verified,reserved;
} mb_config_request_producer_result_v1;
/* Pure bounded C11 text/data encoding using the unchanged decoder grammar and
 * existing Mbed SHA256. No heap, retained pointers, I/O, network or publication.
 * Derives q-/p-/r-/claim refs from ID; emits exact ASCII/LF bytes (no extra NUL).
 * Caller provides valid disjoint immutable params and writable output/result
 * backing. NULL output with zero capacity is a permitted size query. Capacities
 * must not exceed the respective decoder caps. Alignment/span/alias/oversize-cap
 * preflight leaves ALL backing untouched; inspect return before result.
 * After admitted preflight, result is initialized; failed output buffers remain
 * untouched. BUFFER_TOO_SMALL supplies both required lengths/context digest.
 * Success copies both complete records only after roundtrip grammar validation.
 * Digests bind provenance, not authenticity. Complete means encoded preparation
 * intent; it grants no file-installation or engine/runtime authority. */
int mb_config_request_encode_v1(const mb_config_request_producer_params_v1 *params,
 uint8_t *request,size_t request_capacity,uint8_t *context,size_t context_capacity,
 mb_config_request_producer_result_v1 *result);
#define MB_CONFIG_REQUEST_PRODUCER_ABI_V2 2u
typedef struct mb_config_request_producer_params_v2 {
 uint32_t struct_size,abi_version;
 mb_config_request_producer_params_v1 common;
 uint32_t target_selection,platform_bytes;
 char platform[MB_CONFIG_MAX_PLATFORM_BYTES+1];
} mb_config_request_producer_params_v2;
typedef struct mb_config_request_producer_result_v2 {
 uint32_t struct_size,abi_version,status,complete;
 mb_config_request_producer_result_v1 common;
 uint32_t target_selection,platform_bytes;
 char platform[MB_CONFIG_MAX_PLATFORM_BYTES+1];
} mb_config_request_producer_result_v2;
/* Same pure encoding/output semantics as v1. Common target_entry and mount
 * arrays/lengths must be entirely zero; path_hash_seed must be zero. V2 omits
 * those fields and requests generic target+mount+seed discovery from verified
 * base PAK metadata. Only the explicit mode and bounded platform are added.
 * No selected-member or project/container-layout facts are input. */
int mb_config_request_encode_v2(const mb_config_request_producer_params_v2 *params,
 uint8_t *request,size_t request_capacity,uint8_t *context,size_t context_capacity,
 mb_config_request_producer_result_v2 *result);
#endif
