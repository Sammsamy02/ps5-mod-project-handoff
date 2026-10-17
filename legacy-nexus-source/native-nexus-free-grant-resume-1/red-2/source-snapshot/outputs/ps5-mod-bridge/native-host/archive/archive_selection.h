#ifndef MB_IOSTORE_ARCHIVE_SELECTION_H
#define MB_IOSTORE_ARCHIVE_SELECTION_H
#include "prepared.h"
#include "pak_primary_capture.h"

#define MB_IOSTORE_ARCHIVE_SELECTION_ABI_V1 1u
#define MB_IOSTORE_ARCHIVE_MAX_PROBES 17u
#define MB_IOSTORE_ARCHIVE_MAX_DOCUMENTS 16u
#define MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES 65536u
#define MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES (1024u*1024u)
#define MB_IOSTORE_ARCHIVE_PROBE_WORKSPACE_BYTES 65536u
#define MB_IOSTORE_ARCHIVE_PRIMARY_CAPTURE_BYTES 65536u

typedef enum mb_iostore_archive_status_v1 {
 MB_IOSTORE_ARCHIVE_OK=0, MB_IOSTORE_ARCHIVE_BAD_ARGUMENT,
 MB_IOSTORE_ARCHIVE_INCOMPLETE, MB_IOSTORE_ARCHIVE_GROUP_REJECTED,
 MB_IOSTORE_ARCHIVE_NO_CANDIDATE, MB_IOSTORE_ARCHIVE_SELECTION_REQUIRED,
 MB_IOSTORE_ARCHIVE_UNRESOLVED, MB_IOSTORE_ARCHIVE_UNSUPPORTED_EXTRA,
 MB_IOSTORE_ARCHIVE_PROBES_REQUIRED, MB_IOSTORE_ARCHIVE_PROBE_MISMATCH,
 MB_IOSTORE_ARCHIVE_MATERIAL_PAK, MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK,
 MB_IOSTORE_ARCHIVE_INVALID_DOCUMENT, MB_IOSTORE_ARCHIVE_LIMIT,
 MB_IOSTORE_ARCHIVE_HASH_ERROR
} mb_iostore_archive_status_v1;
typedef enum mb_iostore_archive_role_v1 {
 MB_IOSTORE_ARCHIVE_ROLE_UTOC=1, MB_IOSTORE_ARCHIVE_ROLE_UCAS,
 MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION, MB_IOSTORE_ARCHIVE_ROLE_DOCUMENT
} mb_iostore_archive_role_v1;
typedef struct mb_iostore_archive_probe_v1 {
 uint32_t row_index,reserved;
 const uint8_t *data;size_t bytes; /* Full immutable materialized member bytes. */
} mb_iostore_archive_probe_v1;
typedef struct mb_iostore_archive_member_v1 {
 uint32_t row_index,role,probe_required,probe_verified;
 uint64_t bytes;uint8_t sha256[32];
 uint32_t omitted_from_iostore_job,reserved;
} mb_iostore_archive_member_v1;
typedef struct mb_iostore_archive_workspace_v1 {
 uint32_t struct_size,abi_version;
 mb_archive_prepared_workspace_v1 grouping;
 uint8_t *probe_workspace;size_t probe_workspace_bytes; /* Exactly64KiB. */
 uint8_t *primary_capture;size_t primary_capture_bytes; /* Exactly64KiB. */
} mb_iostore_archive_workspace_v1;
typedef struct mb_iostore_archive_selection_v1 {
 uint32_t struct_size,abi_version,status,selection_complete;
 uint32_t preparation_ready,group_count,candidate_count,member_count;
 uint32_t toc_row,ucas_row,pak_row,document_count;
 uint32_t probes_required,probes_verified,omitted_empty_paks,omitted_documents;
 uint32_t problem_row,group_inner_status,pak_inner_status,pak_inner_reason;
 uint64_t path_work_used,source_bytes;
 uint8_t source_sha256[32],inventory_sha256[32],target_token[32];
 uint32_t payload_verified,authenticity_verified,runtime_supported,installation_supported;
} mb_iostore_archive_selection_v1;

/* Minimal pure metadata/content adapter. Calls the existing group classifier,
 * with one immutable completed source/inventory. Exactly one resolved IoStore
 * group is admitted; >1 valid groups returns SELECTION_REQUIRED, never first.
 * Any unresolved companion or standalone PAK group refuses. Unknown regular
 * rows refuse; directories are retained inventory metadata only. No paths are
 * destinations and no per-mod names/IDs/sizes/expected member hashes are input.
 *
 * TOC/UCAS roles come ONLY from groups.c. Same-group PAK is retained as an
 * explicit probe member. Only readme[optional safe ASCII suffix].txt/.md leaf
 * names are document candidates; content must be bounded valid UTF8 plaintext
 * (TAB/CR/LF allowed, NUL/other controls refused). Unknown text roles refuse.
 * First call with no probes may return PROBES_REQUIRED with selection_complete1
 * and preparation_ready0. Members[] and materialize_rows[] include EVERY
 * regular member of the admitted scope, including pending probes. They feed
 * the existing owning materializer; bytes/identities derive from inventory.
 * A second call with the exact probe rows rederives scope and SHA256 binds full
 * companion/doc bytes. Extra/duplicate/TOC/UCAS probes refuse. Only verified
 * empty v11 PAK metadata with contiguous zero-payload layout is omitted; any
 * declared files/tombstones/nonempty index refuses MATERIAL_PAK/UNSUPPORTED_PAK.
 * No material PAK bytes or extra runtime files are silently discarded.
 *
 * Caller owns immutable coherent inventory/probe backing and private workspace.
 * Known invalid controls/caps/aliases leave all backing unchanged. Admitted
 * refusals clear member/materialization outputs except PROBES_REQUIRED publishes
 * the explicit pending descriptor. Group scratch is not persistent authority.
 * No heap/files/network/runtime/installation. Probe workspace/capture may be
 * mutated after admission. No whole TOC/UCAS compatibility/authenticity proof.
 */
mb_iostore_archive_status_v1 mb_iostore_archive_select_v1(
 const mb_archive_prepared_view_v1 *view,
 const mb_iostore_archive_probe_v1 *probes,uint32_t probe_count,
 const mb_iostore_archive_workspace_v1 *workspace,
 uint32_t *materialize_rows,uint32_t row_capacity,
 mb_iostore_archive_member_v1 *members,uint32_t member_capacity,
 mb_iostore_archive_selection_v1 *result);
#endif
