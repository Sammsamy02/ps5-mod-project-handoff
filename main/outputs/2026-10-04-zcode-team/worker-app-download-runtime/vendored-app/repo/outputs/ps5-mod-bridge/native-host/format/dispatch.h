#ifndef MODBRIDGE_FORMAT_DISPATCH_H
#define MODBRIDGE_FORMAT_DISPATCH_H

#include <stddef.h>
#include <stdint.h>
#include "../core_bridge.h"
#include "../pak/pak_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_FORMAT_ABI_VERSION 1u
#define MB_FORMAT_MAX_ADAPTERS 16u
#define MB_FORMAT_MAX_CAPTURE_BYTES 4096u
#define MB_FORMAT_MAX_PROBE_STEPS UINT64_C(131072)
#define MB_FORMAT_REPORT_LIMIT 8192u
#define MB_FORMAT_REGISTRY_SEALED UINT32_C(0x4d424631)
#define MB_FORMAT_OP_METADATA 1u
#define MB_FORMAT_OP_SELECTED_PAYLOAD 2u
#define MB_FORMAT_SCOPE_CLASSIC_V11_METADATA 1u
#define MB_FORMAT_SCOPE_SELECTED_PAYLOAD 2u

typedef enum mb_dispatch_code {
    MB_DISPATCH_OK=0, MB_DISPATCH_BAD_ARGUMENT, MB_DISPATCH_INVALID_REGISTRY,
    MB_DISPATCH_RESOURCE_LIMIT, MB_DISPATCH_INTERNAL_ERROR
} mb_dispatch_code;
typedef enum mb_support_state {
    MB_FORMAT_UNVERIFIED=0, MB_FORMAT_SUPPORTED=1, MB_FORMAT_UNSUPPORTED=2
} mb_support_state;
typedef enum mb_execution_context {
    MB_FORMAT_HOST_FIXTURE=1, MB_FORMAT_PS5_NATIVE=2
} mb_execution_context;
typedef enum mb_probe_disposition {
    MB_PROBE_NO_MATCH=0, MB_PROBE_CANDIDATE, MB_PROBE_UNSUPPORTED_PROFILE,
    MB_PROBE_INCOMPLETE, MB_PROBE_INVALID_ARCHIVE
} mb_probe_disposition;
typedef enum mb_format_reason {
    MB_FORMAT_REASON_NONE=0, MB_FORMAT_CANDIDATE_REQUIRES_VALIDATION,
    MB_FORMAT_VALIDATED_CANDIDATE, MB_FORMAT_NO_ADAPTER,
    MB_FORMAT_AMBIGUOUS_ADAPTER, MB_FORMAT_PROFILE_UNAVAILABLE,
    MB_FORMAT_ENCRYPTED_PROFILE_UNAVAILABLE, MB_FORMAT_INVALID_ARCHIVE,
    MB_FORMAT_INTEGRITY_FAILED, MB_FORMAT_INPUT_UNAVAILABLE,
    MB_FORMAT_SOURCE_CHANGED, MB_FORMAT_WORK_LIMIT, MB_FORMAT_CANCELLED,
    MB_FORMAT_TARGET_EVIDENCE_MISSING, MB_FORMAT_PLATFORM_ACCEPTANCE_MISSING,
    MB_FORMAT_INSTALL_OPERATION_UNAVAILABLE, MB_FORMAT_RESULT_BINDING_MISMATCH,
    MB_FORMAT_INTERNAL_FAILURE
} mb_format_reason;

typedef struct mb_adapter_desc_v1 {
    uint32_t struct_size, abi_version;
    char adapter_id[64], adapter_version[32], format_id[64], profile_id[96];
    uint32_t operation_mask, reserved;
} mb_adapter_desc_v1;
typedef struct mb_probe_input_v1 {
    uint32_t struct_size, abi_version;
    uint64_t logical_size, snapshot_token, tail_offset;
    const uint8_t *prefix; uint32_t prefix_bytes, reserved;
    const uint8_t *tail; uint32_t tail_bytes, reserved2;
} mb_probe_input_v1;
typedef struct mb_probe_budget_v1 {
    uint32_t struct_size, abi_version;
    uint64_t remaining_steps, used_steps;
} mb_probe_budget_v1;
typedef struct mb_probe_claim_v1 {
    uint32_t struct_size, abi_version;
    uint32_t disposition, reason, operation_mask, reserved;
} mb_probe_claim_v1;
typedef mb_dispatch_code (*mb_probe_fn_v1)(const mb_probe_input_v1 *, uint32_t,
    mb_probe_budget_v1 *, mb_probe_claim_v1 *);
typedef struct mb_adapter_registration_v1 {
    uint32_t struct_size, abi_version;
    mb_adapter_desc_v1 descriptor;
    mb_probe_fn_v1 probe;
} mb_adapter_registration_v1;
typedef struct mb_adapter_registry_v1 {
    uint32_t struct_size, abi_version;
    uint64_t generation;
    uint32_t execution_context, count, sealed, reserved;
    mb_adapter_registration_v1 entries[MB_FORMAT_MAX_ADAPTERS];
} mb_adapter_registry_v1;
typedef struct mb_scoped_state_v1 {
    uint32_t struct_size, abi_version;
    uint32_t state, reason, scope, reserved;
} mb_scoped_state_v1;
typedef struct mb_dispatch_result_v1 {
    uint32_t struct_size, abi_version;
    uint64_t registry_generation, logical_size, snapshot_token, probe_steps;
    mb_adapter_desc_v1 descriptor;
    uint32_t adapter_present, adapter_slot, operation, execution_context;
    uint32_t operation_state, compatibility_state, probe_disposition, reserved;
    mb_scoped_state_v1 format_candidate, engine_build, asset_platform, install_authority;
    uint32_t want_selected, selected_file_index, metadata_complete, selected_payload_complete;
    uint32_t child_status, child_stage, child_reason, read_calls;
    uint64_t requested_read_bytes, decoded_bytes;
    uint32_t runtime_supported, installation_supported, authenticity_verified, game_acceptance_verified;
} mb_dispatch_result_v1;

/* Trusted stateless built-ins only. Registry owns copied metadata/function
 * pointers, is immutable after seal, and outlives calls. No plugin loading. */
mb_dispatch_code mb_registry_seal_v1(uint64_t generation, uint32_t execution_context,
    const mb_adapter_registration_v1 *entries, uint32_t count, mb_adapter_registry_v1 *registry);
int mb_format_registry_valid_v1(const mb_adapter_registry_v1 *registry);
int mb_format_result_valid_v1(const mb_dispatch_result_v1 *result);
/* Callback helper: charges BEFORE examining source bytes. Budget is shared,
 * unreplenished across callbacks. Callback backing/behavior is trusted. */
mb_dispatch_code mb_probe_charge_v1(mb_probe_budget_v1 *budget, uint64_t steps);
mb_dispatch_code mb_dispatch_probe_v1(const mb_adapter_registry_v1 *registry,
    const mb_probe_input_v1 *input, uint32_t operation,
    mb_probe_budget_v1 *budget, mb_dispatch_result_v1 *result);
mb_adapter_registration_v1 mb_pak_adapter_registration_v1(void);

/* Invoke only a bound candidate. Preflight errors write nothing. A source/
 * registry binding mismatch publishes an unverified receipt but invokes no
 * provider and leaves candidate buffers untouched; those are not valid output.
 * After entering the candidate, its existing failure-erasure contract applies.
 * No input/workspace pointer is retained. No heap or authority is granted.
 * Caller owns valid disjoint storage and immutable provider/cancel backing. */
pak_status mb_pak_adapter_validate_v1(const mb_adapter_registry_v1 *registry,
    const mb_dispatch_result_v1 *candidate, const pak_source_v1 *source,
    const pak_candidate_limits_v1 *limits, uint32_t want_selected,
    uint32_t selected_file_index, const pak_candidate_buffers_v1 *buffers,
    bz_budget *codec_budget, pak_graph_cancel_fn cancelled, void *cancel_context,
    mb_dispatch_result_v1 *result);
/* Whole owning JSON report. No probe/source access on serialization/size query.
 * Returns existing MB_CORE_* codes. length excludes NUL; short output is empty.
 * Known aliases/invalid backing write nothing. Only owning produced receipts
 * may be serialized; this is not an untrusted JSON-to-capability entry point. */
int mb_core_format_report_json(const mb_dispatch_result_v1 *result,
    char *output, size_t capacity, size_t *length);

#ifdef __cplusplus
}
#endif
#endif
