#ifndef MB_MOD_SERVICE_REGISTER_BRIDGE_H
#define MB_MOD_SERVICE_REGISTER_BRIDGE_H
#include "read_bridge.h"
#include "../download_store/store.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_MOD_SERVICE_REGISTRATION_METADATA_LIMIT (128u * 1024u)
#define MB_MOD_SERVICE_REGISTRATION_REQUEST_LIMIT (144u * 1024u)
/* Trusted native caller only. The owner serializes this complete operation
 * against other backend/store mutations and keeps both borrowed owners alive.
 * title_id is an explicit selected association, PPSA/CUSA followed by 1..16
 * digits; spelling is not evidence of a game build or installed/running game.
 * No game.status inference, provider request, preparation or installation.
 *
 * Input spans are immutable, valid and disjoint from output storage. No NUL is
 * required. download_id is exactly dl- plus 64 lowercase hex digits. Optional
 * metadata is absent only for NULL/0; otherwise it is <=128KiB of exact JSON:
 * {source:{name?,author?,version?},requirements:<normalized complete object>}.
 * Identity and the private archive path come only from a full store lookup.
 * NULL/0 metadata loads an available immutable capture from the durable store.
 * A missing capture sends requirements:null, never synthesized empty arrays;
 * a corrupt/unreadable capture refuses the operation instead of downgrading it.
 * Explicit native metadata is validated directly and takes precedence.
 *
 * service.describe must advertise this contract, registration and records.list.
 * All replies are <=256KiB (MB_MOD_SERVICE_RESPONSE_LIMIT), depth<=64 and
 * <=200000 parser events. records.list accepts <=256 unique mod records.
 * Exact existing title/source identity returns a no-op receipt; it does not
 * claim requirements are known because records.list omits them. Conflicts and
 * malformed/duplicate records refuse registration. One full store lookup after
 * the read-only backend queries checks integrity before replay or registration.
 * Backend-owned replies are released exactly once, including rejected replies.
 *
 * Success allocates a NUL-terminated app-owned allowlisted receipt; caller frees.
 * The receipt never contains archive_path, raw requirements, or generic backend
 * echoes. Every failure sets output=NULL and output_bytes=0. Returns 200 success,
 * 400 invalid input/metadata, 404 missing download, 409 conflict/integrity failure,
 * 502 invalid/refused backend reply, or 503 unavailable/allocation failure.
 * A failed/invalid reply after registration has an unknown mutation outcome;
 * retry reconciles through records.list before considering another mutation. */
int mb_mod_service_register_download(const struct mb_mod_service_bridge *,
    mb_download_store *, const char *download_id, size_t download_id_bytes,
    const char *title_id, size_t title_id_bytes,
    const char *metadata, size_t metadata_bytes, char **output, size_t *output_bytes);
#ifdef __cplusplus
}
#endif
#endif
