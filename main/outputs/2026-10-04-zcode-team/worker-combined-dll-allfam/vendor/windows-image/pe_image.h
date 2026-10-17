#ifndef MB_PE_IMAGE_H
#define MB_PE_IMAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MB_PE_MAX_SECTIONS 96u
#define MB_PE_MAX_DIRECTORIES 16u
#define MB_PE_MAX_IMAGE_SIZE UINT32_C(1073741824)
#define MB_PE_MAX_ITEMS UINT32_C(262144)
#define MB_PE_MAX_STRING 4096u

typedef enum mb_pe_status {
    MB_PE_OK = 0, MB_PE_UNRESOLVED_IMPORTS, MB_PE_INVALID_ARGUMENT,
    MB_PE_ALIAS, MB_PE_TRUNCATED, MB_PE_BAD_FORMAT, MB_PE_UNSUPPORTED,
    MB_PE_LIMIT, MB_PE_BAD_RVA, MB_PE_OVERLAP, MB_PE_CAPACITY,
    MB_PE_RELOCATIONS_REQUIRED, MB_PE_RELOCATION_OVERFLOW,
    MB_PE_BINDER_ERROR, MB_PE_VISITOR_ERROR
} mb_pe_status;
typedef struct mb_pe_string { const char *data; size_t size; } mb_pe_string;
typedef struct mb_pe_directory { uint32_t rva, size; } mb_pe_directory;
typedef struct mb_pe_section {
    uint32_t rva, virtual_size, raw_offset, raw_size, mapped_size, characteristics;
} mb_pe_section;
enum mb_pe_requirement {
    MB_PE_REQUIRES_ENTRY = 1u, MB_PE_REQUIRES_TLS = 2u,
    MB_PE_REQUIRES_EXCEPTION = 4u, MB_PE_REQUIRES_DELAY_IMPORT = 8u,
    MB_PE_REQUIRES_LOAD_CONFIG = 16u, MB_PE_REQUIRES_CLR = 32u,
    MB_PE_REQUIRES_IMPORTS = 64u
};
typedef struct mb_pe_info {
    uint64_t preferred_base;
    uint32_t image_size, headers_size, entry_rva, section_alignment, file_alignment;
    uint32_t requirements, import_count, relocation_count, export_count, export_name_count;
    uint16_t section_count, characteristics, dll_characteristics;
    mb_pe_directory directories[MB_PE_MAX_DIRECTORIES];
    mb_pe_section sections[MB_PE_MAX_SECTIONS];
} mb_pe_info;
typedef struct mb_pe_import {
    mb_pe_string library, name; /* borrowed from immutable file; name empty for ordinal */
    uint32_t iat_rva;
    uint16_t ordinal, hint;
    uint8_t by_ordinal;
} mb_pe_import;
typedef struct mb_pe_error {
    mb_pe_status code;
    uint32_t rva, detail;
    mb_pe_import import; /* exact failing binding when available; borrowed */
} mb_pe_error;
typedef enum mb_pe_bind_result {
    MB_PE_BIND_UNRESOLVED = 0, MB_PE_BIND_BOUND = 1, MB_PE_BIND_ERROR = -1
} mb_pe_bind_result;
typedef mb_pe_bind_result (*mb_pe_binder)(void *, const mb_pe_import *, uint64_t *value);
typedef struct mb_pe_report {
    uint64_t logical_base;
    uint32_t relocations_applied, imports_bound, imports_unresolved;
    uint8_t image_prepared; /* bytes only, never execution readiness */
} mb_pe_report;
typedef struct mb_pe_export {
    uint32_t ordinal, rva;
    mb_pe_string name, forwarder; /* empty name for unnamed; forwarder is metadata */
} mb_pe_export;
typedef int (*mb_pe_export_visitor)(void *, const mb_pe_export *);
typedef int (*mb_pe_import_visitor)(void *, const mb_pe_import *);

/* All memory is caller-owned. File bytes must remain immutable through the call.
 * File, image capacity, info/report/error must be mutually disjoint. Callbacks
 * must not mutate them, reenter with them, retain transient record addresses, or
 * throw/longjmp. Callback string views remain valid with the original file.
 * Inspect/visit validate supported structures before publishing/callbacks.
 * Prepare additionally rejects overlapping relocation/IAT destinations (including
 * IAT terminators), using image storage as a temporary byte map before copying or
 * callbacks. A conflict or a late fatal error zeroes image_size
 * bytes and clears info/report. UNRESOLVED retains prepared bytes and info/report;
 * each unresolved IAT slot keeps its original value. NULL binder resolves none.
 * No allocation, code execution, library lookup, permission changes, or readiness
 * assertion. Requirements are present features for a future execution backend.
 */
mb_pe_status mb_pe_inspect(const void *, size_t, mb_pe_info *, mb_pe_error *);
mb_pe_status mb_pe_prepare(const void *, size_t, uint64_t logical_base,
    void *image, size_t capacity, mb_pe_binder, void *, mb_pe_info *, mb_pe_report *, mb_pe_error *);
mb_pe_status mb_pe_visit_exports(const void *, size_t, mb_pe_export_visitor, void *, mb_pe_error *);
/* Visits borrowed immutable import records only after the complete image has
 * passed inspect validation. A nonzero visitor result stops with VISITOR_ERROR
 * and records the stopping import. Callback contracts above apply. */
mb_pe_status mb_pe_visit_imports(const void *, size_t, mb_pe_import_visitor, void *, mb_pe_error *);
const char *mb_pe_status_name(mb_pe_status);
#ifdef __cplusplus
}
#endif
#endif
