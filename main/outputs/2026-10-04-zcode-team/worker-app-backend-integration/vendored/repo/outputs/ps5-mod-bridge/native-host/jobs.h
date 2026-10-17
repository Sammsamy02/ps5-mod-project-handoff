#ifndef MODBRIDGE_NEXUS_JOBS_H
#define MODBRIDGE_NEXUS_JOBS_H
#include <stddef.h>
#include <stdint.h>
#include "auth.h"
#include "analysis.h"
#include "download_store/store.h"

#define MB_NEXUS_JOBS_ABI_V1 1u
#define MB_NEXUS_JOBS_LIMIT 8u
#define MB_NEXUS_JOBS_INTENT_LIMIT 4096u
#define MB_NEXUS_JOBS_OUTPUT_LIMIT 98304u
#define MB_NEXUS_JOBS_ID_BYTES 32u
#define MB_NEXUS_JOBS_ID_CAPACITY 33u
#define MB_NEXUS_JOBS_KEY_LIMIT 80u
#define MB_NEXUS_JOBS_ARCHIVE_LIMIT UINT64_C(1073741824)
#define MB_NEXUS_JOBS_TIMEOUT_LIMIT_MS 300000u
#define MB_NEXUS_GENERATIONS_LIMIT 8u

typedef struct mb_nexus_jobs_context_v1 mb_nexus_jobs_context_v1;
typedef int (*mb_nexus_jobs_entropy_fn_v1)(void *, uint8_t output[32]);
typedef enum mb_nexus_jobs_status_v1 {
    MB_NEXUS_JOBS_OK = 0, MB_NEXUS_JOBS_BAD_ARGUMENT = 1,
    MB_NEXUS_JOBS_SESSION_REQUIRED = 2, MB_NEXUS_JOBS_AUTH_REQUIRED = 3,
    MB_NEXUS_JOBS_REFUSED = 4, MB_NEXUS_JOBS_INTENT_CONFLICT = 5,
    MB_NEXUS_JOBS_LIMIT_REACHED = 6, MB_NEXUS_JOBS_NOT_FOUND = 7,
    MB_NEXUS_JOBS_BUSY = 8, MB_NEXUS_JOBS_SOURCE_FAILED = 9,
    MB_NEXUS_JOBS_STORAGE_FAILED = 10, MB_NEXUS_JOBS_OUTPUT_TOO_SMALL = 11,
    MB_NEXUS_JOBS_INTERNAL_ERROR = 12,
    MB_NEXUS_JOBS_ACQUISITION_UNRESOLVED = 13
} mb_nexus_jobs_status_v1;
typedef struct mb_nexus_jobs_result_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_jobs_status_v1 status;
    uint32_t complete, processed, job_count, queued_count;
    size_t output_bytes;
} mb_nexus_jobs_result_v1;
typedef struct mb_nexus_jobs_create_params_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_auth_context_v1 *auth;
    int scratch_directory_fd;
    mb_archive_analysis_context_v1 execution_context;
    const uint8_t *ca_pem; size_t ca_bytes;
    /* Optional trusted CURLOPT_RESOLVE mapping, copied with its trailing NUL.
     * TLS verification remains mandatory; no caller URL or trust override. */
    const char *archive_resolve; size_t archive_resolve_bytes;
    uint32_t resolver_timeout_ms, download_timeout_ms, analysis_timeout_ms;
    uint32_t connect_timeout_ms;
    uint64_t archive_cap_bytes;
    void *entropy_context;
    mb_nexus_jobs_entropy_fn_v1 entropy_read32;
} mb_nexus_jobs_create_params_v1;
typedef struct mb_nexus_jobs_submit_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    const uint8_t *intent; size_t intent_bytes;
    char *output; size_t output_capacity;
} mb_nexus_jobs_submit_params_v1;
typedef struct mb_nexus_jobs_resume_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    const char *id; size_t id_bytes;
    const uint8_t *intent; size_t intent_bytes;
    char *output; size_t output_capacity;
} mb_nexus_jobs_resume_params_v1;
typedef struct mb_nexus_jobs_list_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    char *output; size_t output_capacity;
} mb_nexus_jobs_list_params_v1;
typedef struct mb_nexus_jobs_get_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *id; size_t id_bytes;
    char *output; size_t output_capacity;
} mb_nexus_jobs_get_params_v1;
/* Optional trusted captured-base profile. Borrowed immutable read-only backing
 * remains alive through successful jobs_free. Domain is an explicit source
 * admission binding, never target discovery or proof of console compatibility.
 * Base bytes are independently hashed against expected_sha256 at configuration.
 * Existing create layout and default analysis-only behavior remain unchanged. */
typedef struct mb_nexus_jobs_pak_profile_v1 {
    uint32_t struct_size, abi_version;
    int base_fd;
    uint32_t immutable_captured_base;
    uint64_t expected_bytes;
    uint8_t expected_sha256[32];
    const char *nexus_game;
    size_t nexus_game_bytes;
    uint32_t timeout_ms, reserved;
} mb_nexus_jobs_pak_profile_v1;

/* Captured immutable IoStore metadata, independently size/SHA admitted. The
 * full base UCAS is not opened: only its observed length is supplied. These
 * borrowed descriptors remain alive until successful jobs_free; source domain
 * binding is not automatic target discovery or engine/platform proof. */
typedef struct mb_nexus_jobs_iostore_profile_v1 {
    uint32_t struct_size, abi_version;
    int base_toc_fd, base_header_fd;
    uint32_t immutable_captured_base;
    uint64_t expected_toc_bytes, expected_header_bytes;
    uint8_t expected_toc_sha256[32], expected_header_sha256[32];
    uint64_t observed_base_ucas_bytes;
    const char *nexus_game;
    size_t nexus_game_bytes;
    uint32_t timeout_ms, reserved;
} mb_nexus_jobs_iostore_profile_v1;

#ifdef __cplusplus
#define MB_NEXUS_JOBS_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_JOBS_NOEXCEPT
#endif
/* Serialized process-lifetime queue; at most eight records, no eviction,
 * background thread or installation; optional configured store persists original archives. Root exclusively
 * owns the validated existing private0700 directory and its borrowed descriptor,
 * and keeps auth, checked entropy/context and the shared curl runtime alive until
 * successful free. Create owns CA/address copies; no credential getter or curl/
 * PSA init/cleanup occurs. Entropy succeeds only with32 fresh checked bytes.
 *
 * All specified spans, params/result/handle storage and this context's owned
 * storage must be valid, overflow-safe and disjoint. Unsupported layouts/known
 * aliases leave storage untouched. Otherwise failed outputs are fully wiped;
 * output_bytes=0, complete=0. Capacities1..98304, no size query or prefix.
 * Success outputs owning NUL-terminated JSON, length excluding NUL. submit/get
 * return one existing UI job object; list returns {"items":[...]}. Cookie-only
 * reads validate an existing local session; submit checks cookie/CSRF/Origin and
 * an unexpired private account before admission. IDs/status/idempotency are
 * cookie scoped; duplicate intents return the same record, conflicting source
 * fails. Input is a flat bounded JSON object with exactly kind/game/mod_id/
 * file_id/idempotency_key; canonical ASCII strings, positive signed63 integers,
 * unique keys. target_ref, nxm, extra fields/escapes/nesting are refused.
 *
 * run_one advances the oldest queued record synchronously, after root sends the
 * HTTP response. processed=1 means a record reached a stored terminal state,
 * including failure/needs_nexus_authorization. A successful no-work call has
 * processed=0. Queued cookie/CSRF/Origin and provider URI are erased after use;
 * only a cookie fingerprint remains for private status scoping. Download sends
 * no account headers. Fresh exclusive/no-follow0600 scratch names are generated
 * job IDs, never archive members; completed files are reopened O_RDONLY and
 * bound by device/inode/size/SHA before scan/group analysis. Scratch archives stay unchanged and private until free; failed scratch archives are removed.
 * A configured borrowed store publishes independently checked original bytes
 * before analysis. Public retention metadata explicitly reports saved/failed;
 * retention.metadata_state reports unknown/pending/saved/failed for the private
 * prerequisite capture. Sidecar failure never undoes a saved original. Direct
 * jobs have unknown metadata; raw metadata is never included in public output.
 * saved originals survive analysis refusal, scratch cleanup and jobs free. Public
 * reports establish analysis or configured private format preparation only;
 * runtime/installation remain false.
 *
 * Free(NULL-handle) is idempotent. Failure to prove/remove an owned scratch leaf
 * latches storage failure and retains the handle for a cleanup retry; new work
 * is refused. Free never unlinks a substituted file. Callbacks/syscalls/parser
 * work are cooperative, not hard interrupts; there is no progress polling while
 * the serial main loop is inside run_one. */
int mb_nexus_jobs_create_v1(const mb_nexus_jobs_create_params_v1 *,
    mb_nexus_jobs_context_v1 **, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Borrowed device-local archive store; configure once before any submission. */
int mb_nexus_jobs_configure_download_store_v1(mb_nexus_jobs_context_v1 *,
    mb_download_store *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_jobs_free_v1(mb_nexus_jobs_context_v1 **) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_jobs_submit_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_submit_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Atomic admission of a server-computed prerequisite acquisition. The existing
 * submit layouts and span/ownership rules apply. Intent has exactly game,
 * mod_ids (1..8 unique positive INT32 IDs), file_choices (0..8 unique mod_id /
 * positive signed63 file_id pairs), file_policy (explicit_only or
 * unique_current_primary), and idempotency_key (8..80 existing ASCII key rules).
 * Roots/choices are structurally canonicalized for same-cookie group replay;
 * changed intents conflict. Groups have a separate namespace and children have
 * no public single-job key. No caller plan, URL, credential or authority is read.
 *
 * A fresh group calls the actual auth planner under the separate auth context,
 * with one checked configured resolver budget covering discovery and admission.
 * Only a complete same-game ready closure can admit all 1..8 download_analyze
 * children into remaining global source slots. Non-ready plans return
 * ACQUISITION_UNRESOLVED; every refusal admits no child/receipt or scratch file.
 * All staging is private, with final session/account/storage/time gates before
 * bounded non-failing commit. Replay performs no discovery/entropy, even at cap.
 * The same fresh discovery captures bounded source/normalized prerequisites in
 * selected-source order. All identities, complete envelope and size bounds are
 * checked before admission; malformed capture admits nothing. Available bytes
 * are owned immutably per child through website-grant retries and are persisted
 * only after that child's original archive is saved. Null capture remains
 * unknown and never becomes a synthesized empty prerequisites object.
 *
 * Success returns {acquisition:{state:"accepted",game,idempotency_key,
 * source_job_ids,selected_sources,...}} with exact game/mod_id/file_id/job_id
 * mappings in prerequisite-first acquisition order. Accepted means queued;
 * current child states use existing job get/list. Compatibility/version remain
 * unresolved, download/runtime/install authority false; workers independently
 * authorize every download. No implicit generation or game load
 * order is inferred. There is no caller cancellation or hard interrupt. */
int mb_nexus_jobs_submit_acquisition_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_submit_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Same-owner website grant resume, using the common span/session/output rules.
 * ID is exactly32 lowercase hex. Intent is exactly {"nxm":string}, <=4096
 * bytes. Its documented subset accepts only nxm://game/mods/ID/files/ID with
 * exactly literal key/expires/user_id query names, once each. Values are percent
 * decoded (raw '+' becomes space and is refused); key is1..1024 ASCII33..126,
 * IDs/expiry canonical positive signed63. No fragment, userinfo, port, alternate
 * path, collection/OAuth URI or extra field is accepted. JSON string escapes
 * may encode ASCII; known duplicate keys and malformed encoding are refused.
 *
 * Only this cookie's needs_nexus_authorization source can resume; queued or
 * terminal records return REFUSED, foreign/missing IDs NOT_FOUND. Same ID/kind/
 * source/key/slot survives, even at full capacity. Scope/expired grants REFUSED;
 * malformed input BAD_ARGUMENT. Final session/account/storage/expiry/time gates
 * precede one bounded commit; failure changes neither record nor pending grant.
 * Success returns the existing public queued job, with the old error removed.
 * The decoded grant exists only in private volatile queued storage. Worker
 * consumes/erases it once and independently checks fresh live account identity,
 * expiry and fixed provider route. It clears its local grant before credential-
 * free archive transfer; all failure/free paths erase grants. No custom-scheme
 * interception, caller entitlement, public URI or installation is inferred. */
int mb_nexus_jobs_resume_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_resume_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_jobs_list_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_list_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_jobs_get_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_get_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_jobs_run_one_v1(mb_nexus_jobs_context_v1 *,
    mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Configure once before submission, when compiled with private preparation.
 * No replacement, game-path discovery or publication. Preparation intents use
 * kind=download_prepare_pak (default whole-bundle policy), or explicit
 * download_prepare_pak_component (extras retained/unhandled). Both keep the
 * same five source/idempotency fields. Missing/mismatched profile is unresolved.
 * kind=download_prepare_ini uses the same fields/profile for strict single
 * Engine.ini scalar SystemSettings preparation. Extra regular files or source
 * headers outside that section remain unhandled; no component/optional policy.
 * Its unique PS5 target/mount/seed derive from the captured base. The report
 * describes byte preparation, not CVar recognition/effect or console loading.
 * kind=download_prepare chooses a strict PAK or INI handler using the retained
 * filename inventory as a routing hint. The chosen core independently rescans
 * and validates the original archive; no automatic component/optional policy.
 * No single known handler reports additional_action_required without output.
 * Preparation is <=512MiB, separate from the <=1GiB analysis/download limit.
 * Its physical-order readback SHA256 is artifact identity, not platform proof. */
int mb_nexus_jobs_configure_pak_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_pak_profile_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Configure once before any submissions, independently of the PAK profile.
 * download_prepare_iostore uses the same five flat intent fields. Automatic
 * download_prepare selects IoStore before PAK when container names are present;
 * the strict core independently admits exactly one group, verifies empty PAK
 * and plaintext readme companions, and refuses material/unknown extras.
 * None/v8 only: five private inode-bound artifacts, including complete physical
 * sidecar readback against the write stream. No runtime/publication authority,
 * implicit batching, input remapping or asset/shader conversion. */
int mb_nexus_jobs_configure_iostore_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_iostore_profile_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
/* Explicit local generation over1..8 successful retained original downloads.
 * Uses the same params/result layouts and serialized context. Submit checks an
 * existing cookie/CSRF/Origin session (no new account network request), then
 * accepts exactly kind=prepare_iostore_generation, source_job_ids (ordered
 * array of unique32-lowerhex IDs), idempotency_key. Only this cookie's successful
 * downloaded/analyzed sources in one configured source domain may participate.
 * Missing/foreign IDs return NOT_FOUND, failed/mixed-domain sources REFUSED.
 * Repeated generation keys bind the ordered source IDs and immutable base;
 * changed selections conflict. Job/generation idempotency namespaces separate.
 *
 * Submit queues and returns one object; get/list are read-only. run_one advances
 * a queued generation when no source job is queued. All original archive leaves
 * are reopened no-follow read-only and size/inode/SHA verified before the batch
 * owner is invoked. No successful queue record is implicitly selected. Strict
 * chunk/package/path overlaps return a stored conflict, never an inferred winner.
 * Five outputs stay private with the same inode/physical readback cleanup rules.
 * There is no installation, prerequisite acquisition or target discovery here.
 * Generation result job_count/queued_count refer to this cookie's generations. */
int mb_nexus_generations_submit_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_submit_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_generations_list_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_list_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
int mb_nexus_generations_get_v1(mb_nexus_jobs_context_v1 *,
    const mb_nexus_jobs_get_params_v1 *, mb_nexus_jobs_result_v1 *) MB_NEXUS_JOBS_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_JOBS_NOEXCEPT
#endif
