#ifndef MB_CONFIG_REQUEST_H
#define MB_CONFIG_REQUEST_H
#include <stddef.h>
#include <stdint.h>
#define MB_CONFIG_REQUEST_ABI_V1 1u
#define MB_CONFIG_REQUEST_MAX_BYTES 1024u
#define MB_CONFIG_CONTEXT_MAX_BYTES 32768u
#define MB_CONFIG_REF_BYTES 41u
#define MB_CONFIG_MAX_SOURCE_BYTES UINT64_C(134217728)
#define MB_CONFIG_MAX_BASE_BYTES UINT64_C(536870912)
#define MB_CONFIG_MAX_MEMBER_BYTES UINT64_C(1048576)
#define MB_CONFIG_MAX_PATH_BYTES 1024u
#define MB_CONFIG_MAX_BASENAME_BYTES 255u
#define MB_CONFIG_MAX_SECTIONS 64u
#define MB_CONFIG_MAX_SECTION_BYTES 255u

enum mb_config_request_status_v1 {
 MB_CONFIG_REQUEST_OK=0, MB_CONFIG_REQUEST_BAD_ARGUMENT=1,
 MB_CONFIG_REQUEST_INVALID=2, MB_CONFIG_REQUEST_LIMIT=3,
 MB_CONFIG_REQUEST_CONTEXT_MISMATCH=4
};
enum mb_config_role_v1 { MB_CONFIG_ROLE_SCALAR_INI=1 };
typedef struct mb_config_request_ref_v1 {
 uint32_t struct_size,abi_version,status,complete;
 uint8_t context_sha256[32];
 char request_id[33],source_ref[MB_CONFIG_REF_BYTES],base_ref[MB_CONFIG_REF_BYTES];
 char context_ref[MB_CONFIG_REF_BYTES],quarantine_ref[40],output_ref[48];
 char receipt_ref[48],claim_ref[40];
} mb_config_request_ref_v1;
typedef struct mb_config_request_v1 {
 uint32_t struct_size,abi_version,status,complete;
 uint64_t source_bytes,base_bytes,max_member_bytes,path_hash_seed;
 uint8_t source_sha256[32],base_sha256[32],context_sha256[32],target_token[32];
 char request_id[33],source_ref[MB_CONFIG_REF_BYTES],base_ref[MB_CONFIG_REF_BYTES];
 char context_ref[MB_CONFIG_REF_BYTES],quarantine_ref[40],output_ref[48];
 char receipt_ref[48],claim_ref[40];
 uint32_t role,basename_bytes,target_entry_bytes,mount_bytes,section_count;
 char basename[MB_CONFIG_MAX_BASENAME_BYTES+1];
 char target_entry[MB_CONFIG_MAX_PATH_BYTES+1],mount[MB_CONFIG_MAX_PATH_BYTES+1];
 uint32_t section_bytes[MB_CONFIG_MAX_SECTIONS];
 char sections[MB_CONFIG_MAX_SECTIONS][MB_CONFIG_MAX_SECTION_BYTES+1];
 uint32_t installation_supported,runtime_supported,game_acceptance_verified,reserved;
} mb_config_request_v1;
/* Owning pure C11 decode only: no I/O, allocation or retained pointers.
 * Exact ordered printable ASCII/LF records; no unknown fields/trailing data.
 * Ref decode validates the entire envelope. Before full context binding the
 * caller may open only context_ref under its already checked inbox FD.
 * Full decode requires caller-computed SHA256 of those exact immutable context
 * bytes; equality binds provenance, not authenticity, target or install authority.
 * Known pointer/span/alignment/alias preflight and oversize byte spans are no-write.
 * Inspect return first (old output may remain). Admitted parse failure clears
 * positive output. All backing must be valid, immutable and mutually disjoint.
 * Logical target/mount strings are PAK metadata; never filesystem destinations.
 * A complete request describes preparation, never installation/game execution. */
int mb_config_request_ref_decode_v1(const uint8_t *request,size_t size,
 mb_config_request_ref_v1 *out);
int mb_config_request_decode_v1(const uint8_t *request,size_t request_size,
 const uint8_t *context,size_t context_size,const uint8_t observed_context_sha256[32],
 mb_config_request_v1 *out);
#define MB_CONFIG_REQUEST_ABI_V2 2u
#define MB_CONFIG_MAX_PLATFORM_BYTES 32u
enum mb_config_target_selection_v2 { MB_CONFIG_TARGET_UNREAL_PROJECT_PLATFORM=1 };
typedef struct mb_config_request_v2 {
 uint32_t struct_size,abi_version,status,complete;
 mb_config_request_v1 common;
 uint32_t target_selection,platform_bytes;
 char platform[MB_CONFIG_MAX_PLATFORM_BYTES+1];
} mb_config_request_v2;
/* MBCFGREQ1 envelope with strictly ordered MBCFGCTX2. Target selection is
 * explicit; platform is 1..32 ASCII alnum/_/-. No project path, mount or path
 * hash seed is accepted: those common fields remain zero for discovery later.
 * Common owning ABI stays v1; outer ABI is v2. Same no-write preflight and
 * provenance-only/preparation-only semantics as v1. V1 rejects v2 contexts. */
int mb_config_request_decode_v2(const uint8_t *request,size_t request_size,
 const uint8_t *context,size_t context_size,const uint8_t observed_context_sha256[32],
 mb_config_request_v2 *out);
#endif
