#ifndef MB_PE_FILE_SOURCE_H
#define MB_PE_FILE_SOURCE_H
#include "pe_loader.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_pe_file_source_profile {
    const char *path;
    size_t max_bytes;
} mb_pe_file_source_profile;
typedef enum mb_pe_file_source_status {
    MB_PE_FILE_SOURCE_OK = 0, MB_PE_FILE_SOURCE_INVALID_ARGUMENT,
    MB_PE_FILE_SOURCE_OPEN_ERROR, MB_PE_FILE_SOURCE_IO_ERROR,
    MB_PE_FILE_SOURCE_NOT_REGULAR, MB_PE_FILE_SOURCE_SIZE_ERROR,
    MB_PE_FILE_SOURCE_NO_MEMORY
} mb_pe_file_source_status;
/* Native C ABI callbacks for mb_pe_loader_source_ops. The catalog entry's
 * source_context points to a stable profile, not a DLL-supplied search path.
 * max_bytes is positive and at most MB_PE_MAX_IMAGE_SIZE. Only regular files
 * are admitted; the final path component must not be a symlink. The caller
 * provides trusted profile paths and keeps the source file unchanged during
 * acquisition (e.g. an atomically published installer-owned artifact).
 *
 * Successful acquire returns an independent immutable malloc snapshot. It
 * remains valid even after later source replacement and until release(token).
 * This is file intake only; it makes no PE or execution-readiness assertion.
 * Failed acquire clears all three outputs. Outputs/profile/entry are disjoint.
 * The context argument is unused. Calls use native target header constants.
 * The adapter closes its descriptor once, without unsafe EINTR close retries;
 * descriptor-consumption semantics are those of its FreeBSD/macOS platform.
 * release accepts only the token returned by successful acquire, once.
 */
int mb_pe_file_source_acquire(void *, const mb_pe_loader_catalog_entry *,
    const void **file, size_t *size, void **token);
int mb_pe_file_source_release(void *, void *token);
#ifdef __cplusplus
}
#endif
#endif
