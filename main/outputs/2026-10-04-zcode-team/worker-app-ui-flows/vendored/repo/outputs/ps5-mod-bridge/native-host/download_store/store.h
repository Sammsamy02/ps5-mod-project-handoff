#ifndef MB_DOWNLOAD_STORE_H
#define MB_DOWNLOAD_STORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_download_store mb_download_store;
#define MB_DOWNLOAD_STORE_METADATA_LIMIT (128u * 1024u)
enum mb_download_store_status {
  MB_DOWNLOAD_STORE_OK = 0,
  MB_DOWNLOAD_STORE_BAD_ARGUMENT = 1,
  MB_DOWNLOAD_STORE_STORAGE_FAILED = 2,
  MB_DOWNLOAD_STORE_INTEGRITY_FAILED = 3,
  MB_DOWNLOAD_STORE_LIMIT_REACHED = 4,
  MB_DOWNLOAD_STORE_NOT_FOUND = 5,
  MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL = 6,
  MB_DOWNLOAD_STORE_PATH_UNAVAILABLE = 7
};
/* Directory fd is borrowed; it must name an existing owner-private 0700
 * directory. Root is optional, canonical absolute and checked against fd before
 * path output. JSON output is NUL terminated. On errors output is empty. Single
 * caller at a time. */
int mb_download_store_open(int directory_fd, const char *absolute_root,
                           mb_download_store **out);
int mb_download_store_publish(mb_download_store *, int source_fd,
                              const char *game, uint64_t mod_id,
                              uint64_t file_id, uint64_t expected_bytes,
                              const char *sha256, char *output,
                              size_t capacity);
int mb_download_store_list(mb_download_store *, char *output, size_t capacity);
/* Native trusted callers only: emits archive_path. Never expose to browser. */
int mb_download_store_lookup(mb_download_store *, const char *download_id,
                             char *output, size_t capacity);
/* Trusted native authenticated metadata capture only; never browser input.
 * An immutable valid JSON object, 1..128KiB, is bound to the already-published
 * archive manifest and its checked filesystem identity. Semantics remain the
 * registration adapter's responsibility. Exact byte replay succeeds; different
 * bytes or a corrupt/binding-mismatched sidecar return INTEGRITY_FAILED.
 * Archive contents are not rehashed here; final registration lookup does that.
 * No archive manifest/public list fields change. Close preserves sidecars. */
int mb_download_store_publish_metadata(mb_download_store *, const char *download_id,
                                       const char *metadata, size_t metadata_bytes);
/* Returns only the original metadata bytes, NUL terminated. Missing capture is
 * NOT_FOUND with empty output, never a synthesized complete/empty object.
 * Capacity must include the terminator; all failures leave output empty. */
int mb_download_store_lookup_metadata(mb_download_store *, const char *download_id,
                                      char *output, size_t capacity);
void mb_download_store_free(mb_download_store *);
#ifdef __cplusplus
}
#endif
#endif
