# Generic engine/format dispatch design

Design candidate only. Root reports the current generic PAK library committed at `6ae9339`; this task read its contracts and the existing planner without Git operations. No implementation, build, runtime probe or product change was performed.

The app must select different games without game/mod-ID branches or per-mod recipes. Finished operations run on PS5, without a PC, phone, Windows tool or AI service supplying a runtime dependency. This design adds one small, native, read-only format-dispatch seam. It does not approve assets, engine bindings or installation merely because an archive is readable.

## Choice and existing contracts

Use a **sealed registry of built-in native adapters**, bounded byte probes, and a typed wrapper around the existing PAK operation. Registry entries describe format operations; engine/build acceptance remains evidence supplied by the target/capability owner.

Two alternatives were considered. Extending the planner's backend JSON alone would leave byte probing, lifetimes and execution ownership unspecified. A dynamically loaded plugin system would add module authentication, ABI and unloading work before the first useful dispatch test. The sealed registry gives deterministic selection and a reusable boundary with no downloaded code execution.

This preserves the existing contracts:

- [backend.h:4](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/include/modbridge/backend.h:4) is a future C backend ABI, with owning JSON release and module lifetime rules; it does not implement dynamic loading. This proposal does not change that ABI.
- [planner.cpp:80](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:80) requires validated, scoped, matching target evidence. [planner.cpp:231](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:231) rejects unsupported descriptor fields and treats analyzer backends as analysis only. [planner.cpp:246](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:246) keeps ambiguous selection unresolved.
- [pak_candidate.h:123](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/pak/pak_candidate.h:123) defines an immutable, caller-owned, bounded operation. Its proof covers supported classic-v11 metadata and optionally one selected payload, with installation, authenticity and game-acceptance flags zero ([pak_candidate.h:111](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/pak/pak_candidate.h:111)).
- [DDS requirements:46](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/dds.cpp:46) already distinguish a PS5 writer from verified game/build acceptance. The [first-test audit](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/game-independent-first-test-audit.md) found generic provider selection, but actual PS5 title/build selection remains unfinished.

## Four separate decisions

Wire schema version is `1`. Canonical state strings are `supported`, `unsupported`, `unverified`; display labels are Supported, Unsupported, Unverified. Every scoped record has `{state, reason, scope}`, where `reason` is a constant code and `scope` is an array of explicit proof tags, never a free-form support claim.

| Record | Meaning of Supported | Initial behavior |
| --- | --- | --- |
| `format_candidate` | This source satisfied the requested adapter operation and its exact format/profile proof scope. | A matching probe is Unverified. Actual successful classic-v11 validation may be Supported for `classic_v11_metadata`, plus `selected_payload` only when that requested payload completed. |
| `engine_build` | Trusted evidence binds the required engine behavior to the selected PS5 title/build fingerprints and context. | Unverified (`target_evidence_missing`). Neither a Nexus domain nor a PAK version identifies the installed engine/build. |
| `asset_platform` | Every required asset/runtime operation for the assessed scope has PS5-native implementation and matching acceptance evidence. | Unverified (`platform_acceptance_missing`). A required Windows executable/hook/tool is Unsupported (`windows_runtime_unavailable`). Known PC-only asset requirements with no PS5 writer are Unsupported; undecoded asset semantics remain Unverified. |
| `install_authority` | A separate verified writer has an identified target, confined destinations, transaction/rollback policy and applicable user authorization. | Unsupported (`install_operation_unavailable`). No registry or format proof grants writes. |

`operation_state` rates only the requested operation. `inspect_metadata` can be Supported while asset compatibility is Unverified and installation is Unsupported. `compatibility_state` rates the assessed components' engine/build plus asset/runtime acceptance: any demonstrated unsupported requirement makes it Unsupported; missing evidence makes it Unverified; Supported requires complete semantic inventory and matching acceptance for every required component in the stated scope. It does not include installation authority. V1's PAK wrapper cannot produce Supported compatibility.

Unknown formats, encrypted PAK profiles, IoStore and unsupported versions/codecs have Unsupported **format operations under the current registry**. This means no implemented route; it does not assert that another format is inherently impossible. An opaque filename/extension hint may explain `no_format_adapter`; it cannot establish a format identification or engine version. Windows tools may still be inventoried as bytes, but their required runtime operation is Unsupported.

Malformed data and digest mismatch give Unsupported for that source's requested format proof (`invalid_archive` / `integrity_failed`), retaining the actual child status/reason. Cancellation, unavailable/changed source, insufficient capture, resource exhaustion or ambiguous adapters give Unverified. A failed operation never publishes partial supported scope. No fallback to another adapter follows validation failure.

## Small type/API contract

Proposed follow-up files are `native-host/format/dispatch.h`, `dispatch.c`, and `pak_adapter.c`; this document creates none of them. All ABI-v1 structures begin with `uint32_t struct_size, abi_version`; exact v1 sizes and zero reserved fields are checked. There is no serialized pointer, engine pointer, game ID or mod ID in this interface.

| Type | Concrete contents and ownership |
| --- | --- |
| `mb_adapter_desc_v1` | Fixed, NUL-terminated/zero-padded ASCII arrays: `adapter_id[64]`, `adapter_version[32]`, `format_id[64]`, `profile_id[96]`; `uint32_t operation_mask, reserved`. IDs use `[a-z0-9_.-]+`; versions are exact numeric dotted versions. Operation bits are metadata inspection=1 and selected-payload validation=2; other bits are invalid in v1. |
| `mb_adapter_registration_v1` | One descriptor and a stateless trusted `probe` function pointer. Only the application's built-in registration table supplies this; Nexus records/mod manifests cannot register callbacks, modules or addresses. |
| `mb_adapter_registry_v1` | Typed caller-owned storage for at most 16 copied descriptors/function pointers, count, nonzero `uint64_t generation`, and trusted process `execution_context` (`host_fixture` or `ps5_native`). Sealing checks all entries/IDs before publication and sorts by ID. Empty registry is valid. It is immutable until process termination; replacing it requires a new generation and invalidates old keys. |
| `mb_probe_input_v1` | `uint64_t logical_size, snapshot_token, tail_offset`; borrowed immutable `prefix`/`tail` pointers with `uint32_t` byte counts, each at most 4096. Prefix begins at offset0; tail ends at `logical_size`. Input may contain partial windows, but only complete required windows permit a conclusive match/no-match. Source tokens bind a caller-asserted immutable snapshot, not authenticity. |
| `mb_probe_budget_v1` | Caller-owned `uint64_t remaining_steps, used_steps`, initialized once with at most131072 steps and shared across every registry callback. Charge each probe byte inspection/comparison before it occurs; exhaustion ends dispatch. |
| `mb_probe_claim_v1` | Owning scalars: disposition `no_match`, `candidate`, `unsupported_profile`, `incomplete` or `invalid_archive`; constant reason and requested operation mask. No Supported state and no input pointer can be returned by a probe. |
| `mb_dispatch_result_v1` | Owning copied descriptor; explicit `adapter_present` (0/1), slot and registry generation; source size/token, operation/context, operation/compatibility states and four scoped records. Scope is represented by validated bits in C and the fixed tag array on the wire. Optional selection uses `want_selected` plus index, not a magic sentinel. Child status/reason and source/work counters are diagnostic scalars. `runtime_supported`, `installation_supported`, `game_acceptance_verified` and `authenticity_verified` remain zero in the probe/PAK route. |

Proposed signatures (design declarations, not implementation):

```c
typedef mb_dispatch_code (*mb_probe_fn_v1)(
    const mb_probe_input_v1 *input, uint32_t operation,
    mb_probe_budget_v1 *shared_budget, mb_probe_claim_v1 *claim);

mb_dispatch_code mb_registry_seal_v1(
    uint64_t generation, uint32_t execution_context,
    const mb_adapter_registration_v1 *trusted_entries, uint32_t count,
    mb_adapter_registry_v1 *owned_registry);

mb_dispatch_code mb_dispatch_probe_v1(
    const mb_adapter_registry_v1 *registry,
    const mb_probe_input_v1 *input, uint32_t operation,
    mb_probe_budget_v1 *shared_budget, mb_dispatch_result_v1 *result);

pak_status mb_pak_adapter_validate_v1(
    const mb_adapter_registry_v1 *registry,
    const mb_dispatch_result_v1 *candidate,
    const pak_source_v1 *source, const pak_candidate_limits_v1 *limits,
    uint32_t want_selected, uint32_t selected_file_index,
    const pak_candidate_buffers_v1 *buffers, bz_budget *codec_budget,
    pak_graph_cancel_fn cancelled, void *cancel_context,
    mb_dispatch_result_v1 *result);

int mb_core_format_report_json(
    const mb_dispatch_result_v1 *result,
    char *output, size_t capacity, size_t *length);
```

`mb_dispatch_code` is an API status (`ok`, `bad_argument`, `invalid_registry`, `resource_limit`, `internal_error`), separate from metadata support. A valid operation can return `ok` with Unsupported/Unverified metadata. A request names exactly one operation (1 or2), not the combined descriptor mask3. The typed PAK wrapper requires operation1 with `want_selected=0,index=0`, or operation2 with `want_selected=1` and a validated selected-row index; a mode mismatch is a preflight error. It returns the actual `pak_status` as well as a complete dispatch receipt. Known input/output aliases or invalid output backing fail preflight without writes; after valid preflight, failure clears owned positive scope/flags and candidate outputs while retaining bounded constant diagnostics and consumed budgets. Results contain no borrowed pointer; callbacks retain no input, workspace or cancellation pointer.

The first descriptor is `adapter_id="format.unreal.pak.classic_v11"`, `adapter_version="1.0.0"`, `format_id="unreal_pak"`, `profile_id="classic_v11_unencrypted_stored_sha1_v1"`, operation mask3. Its probe recognizes only the actual retained classic-v11 footer envelope (221 bytes, magic at+17, LE version at+21); encrypted/other known versions are rejected for this route. It does not reproduce primary/directory parsing. Short capture is Unverified, not a fabricated match.

Each callback is attempted at most once over the same capture/budget. If every attempted probe conclusively returns `no_match`, dispatch gives Unsupported (`no_format_adapter`); exactly one candidate gives Unverified (`candidate_requires_validation`); conflicting claims or multiple candidates give Unverified (`ambiguous_adapter`). A sole `unsupported_profile`/`invalid_archive` claim gives Unsupported for that identified route. Incomplete, exhausted or callback-error probes prevent a conclusive selection. Registry order cannot resolve ambiguity. A conclusive negative is never a reason to try a permissive fallback.

The PAK wrapper verifies the candidate's registry generation/slot/descriptor/profile, operation and source size/token before calling the existing validator exactly once. Requested selection must match the receipt's output binding. Only actual `PAK_STATUS_OK`, `PAK_CANDIDATE_STAGE_COMPLETE`, complete metadata and (when requested) complete selected payload publish format Supported. Mismatched binding yields Unverified with `result_binding_mismatch`; no stale success is reused. A preflight error leaves prior bytes untouched, so the caller must treat that output as unpublished for the new attempt. Host-fixture receipts do not establish a PS5-native capability; compile/link-only artifacts produce no runtime Supported receipt.

**First implementation slice:** seal/probe plus `mb_core_format_report_json`, with the built-in classic-v11 probe and actual scalar result wiring. The formatter consumes the owning result; it never probes, reads a provider or retries an operation during a size query. Use the existing core C bridge's whole NUL-terminated output/status convention, a required `length`, an8192-byte report ceiling and constant reason/scope strings. Publish this owning report in the inventory entry's `facts.adapter_dispatch`; do not rename existing job stages or `compatibility="unresolved"` fields. A matching first-slice receipt has `operation_state="unverified"`, `format_candidate.reason="candidate_requires_validation"`, both engine/asset states Unverified, install authority Unsupported and every runtime/install assertion false. The typed PAK wrapper is the immediately following bounded connection to the already implemented validator, not a new validator or requirement to build an engine runtime first.

## Application staging and evidence projection

1. At integration, native transfer/quarantine ownership supplies an immutable source snapshot and, when known, its verified full-source SHA256. The dispatcher cannot manufacture authenticity from declared SHA1s or an opaque token. Provider/domain/mod/file IDs remain provenance outside the adapter key.
2. The job owner captures prefix/tail once: at most8192 requested bytes and two reads of at most4096 (coalesce a small source when possible). One outer charged provider covers these reads and the later candidate call. Its byte/call limits are the selected candidate limits plus8192 bytes/two calls, checked for overflow before operations; requests remain within the existing candidate limit. It reserves full requests and failed attempts before calling the underlying provider, never resets/refunds, and never sums/subtracts child counters. The existing candidate's inner limits remain additional bounds. Probes themselves perform no source I/O.
3. Probe, then invoke the one selected typed native wrapper. Its existing caller-owned directory/name tables and optional raw payload sink remain private until whole success. The app can hand a successful selected sink to an independent asset assessor synchronously; generic dispatch retains no body pointer. Names remain engine metadata, not install destinations.
4. Retain the owning scoped receipt and independently assessed target/asset records. UI title/mod cards show `compatibility_state`; operation details may say “Classic v11 bytes validated” or “Selected payload checked.” They never display a title as supported from archive integrity alone. The separate [title inventory design](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/generic-ps5-title-inventory-design.md) resolves `{snapshot_id,title_key}` against an owned title snapshot; that reference is not an adapter-selection input, and identity-only records leave planner fingerprints empty.

Project the format descriptor into the current planner as `mode="analyzer"`, `kinds=["unreal_pak"]`, with empty target-fingerprint requirements and profile/operation data inside allowed `metadata`/`provenance` fields. Format facts belong in the inventory; they do not satisfy `translation.unreal_pak`, `translation.unreal_asset`, a PS5 writer or game-process runtime requirements. The existing analyzer-only rejection keeps it out of preparation steps. Do not add unsupported fields to the strict backend schema or translate wire `supported` directly into capability `status="validated"`.

Trusted engine/build/asset evidence continues to use existing `id`, `version`, `status`, `context`, `scope`, `target_fingerprints` and applicable `source_hash`. Complete capability matching remains with the planner/evidence owner. A new game/build changes that target context; source parsing evidence may be reused only at its target-independent scope. Cache identity includes adapter/version/profile/operation and proof context in existing backend/version/options/evidence fields, plus real source, dependency and target identities ([cache.cpp:21](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/cache.cpp:21)). Missing full source identity means no reusable source-proof cache entry. Per-mod instructions cannot alter adapter choice, native runtime requirements or target fingerprints.

## Focused next fixture checks

One bounded probe/report experiment followed by the typed PAK connection is enough; no engine roadmap or installation writer is part of it. Use meaningful compiled RED/GREEN fixtures:

- Both retained real unencrypted repak v11 inputs: probe remains Unverified; actual metadata/selected validation advances only the corresponding format scope. Compatibility stays Unverified, install authority Unsupported, all authority flags zero. Empty inventory succeeds only for metadata, with no selected-payload scope.
- Changed bytes or changed source token after probing: digest/provider failure cannot become Supported or switch adapters. Corrupt late metadata, truncated selected payload and source-short controls clear published scope/sink as the existing operation requires.
- Unknown complete bytes, encrypted/unsupported-version envelope and IoStore hints: Unsupported current operation, no PAK invocation. A required Windows runtime is Unsupported at asset/runtime assessment even when its containing PAK validates; unknown engine/build evidence stays Unverified.
- Duplicate registry IDs/ABI or reserved-field errors, two candidate callbacks, insufficient capture, exact/one-short probe and outer read budgets, cancellation, stale registry generation, operation/selection mismatch and known aliases: no accidental selection, no reset/refund and no stale positive result. Independent sources with different opaque provider/game labels must have identical byte-driven selection.
- Projection check: a successful format receipt cannot satisfy translation, game acceptance or install requirements. A target/build mismatch rejects otherwise validated capability evidence; host-fixture evidence cannot satisfy a PS5-native operation.
- First-slice report check: actually invoke the probe and serializer, parse the returned JSON, and assert the four exact states/reasons and false authority flags. Whole-output sizing/short-buffer handling must not re-run a probe or consume its budget again; malformed result fields are rejected rather than printed as a new capability claim.

Native C-caller compile/link may follow that focused host experiment without execution. Actual native invocation, asset/platform acceptance and any writer promotion remain separately evidenced operations. This candidate defines the next reusable seam; it does not claim every Nexus mod, engine variant or game is supported.
