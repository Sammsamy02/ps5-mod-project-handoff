#ifndef MB_WIN_MODULE_ENUM_H
#define MB_WIN_MODULE_ENUM_H
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Windows module enumeration / PSAPI family owner.
 *
 * Census-targeted KERNEL32.dll imports (baseline-dll-census.json):
 *   dwmapi.dll : FreeLibrary, GetModuleFileNameW, GetProcAddress, LoadLibraryW
 *   UE4SS.dll  : GetModuleHandleA/W, GetModuleHandleExW, GetModuleFileNameA/W,
 *                GetProcAddress, LoadLibraryA, LoadLibraryExA/W, FreeLibrary,
 *                K32EnumProcessModules, K32GetModuleBaseNameA,
 *                K32GetModuleInformation
 *
 * The owner keeps a fixed-capacity module table: ASCII basename, full file
 * name, image base (the HMODULE), image size, entry point, load reference
 * count, and a borrowed export table (exact-case ASCII name -> address).
 * Handles are actual image bases; NULL selects the executable-image record.
 * Module-name lookups fold ASCII case and follow the pe_loader key policy:
 * a query without a dot gains ".dll" (exact basename still tried first);
 * one trailing dot suppresses the addition. Symbol lookup is exact ASCII;
 * ordinals 1..export_count index the export table. Load increments one
 * reference per call; Free consumes one; static registrations stay in the
 * table at zero references (no unmap model). Loads of unknown names acquire
 * images only through the optional ops hook; without it they report
 * NOT_FOUND. All objects are externally serialized; no callback reenters
 * this owner. No ANSI codepage, filesystem search, ACL or foreign-handle
 * dereference exists. All calls return normally without exceptions/longjmp.
 */
#define MB_WIN_MODULE_NAME_LIMIT 255u
#define MB_WIN_MODULE_FILE_LIMIT 511u
#define MB_WIN_MODULE_MAX_MODULES 64u

#define MB_WIN_MODULE_FLAG_EXECUTABLE_IMAGE 1u
#define MB_WIN_MODULE_EX_FLAG_PIN 1u
#define MB_WIN_MODULE_EX_FLAG_UNCHANGED_REFCOUNT 2u
#define MB_WIN_MODULE_EX_FLAG_FROM_ADDRESS 4u

typedef enum mb_win_module_enum_status {
    MB_WIN_MODULE_ENUM_OK = 0, MB_WIN_MODULE_ENUM_INVALID_ARGUMENT,
    MB_WIN_MODULE_ENUM_BAD_STATE, MB_WIN_MODULE_ENUM_BUSY,
    MB_WIN_MODULE_ENUM_NOT_FOUND, MB_WIN_MODULE_ENUM_CAPACITY,
    MB_WIN_MODULE_ENUM_UNSUPPORTED, MB_WIN_MODULE_ENUM_OWNER_ERROR
} mb_win_module_enum_status;

typedef struct mb_win_module_export {
    const char *name; /* exact-case ASCII, NUL-terminated */
    void *address;
} mb_win_module_export;

typedef struct mb_win_module_record {
    uint8_t registered, executable_image, acquired;
    char name[MB_WIN_MODULE_NAME_LIMIT + 1];
    char file_name[MB_WIN_MODULE_FILE_LIMIT + 1];
    void *base, *entry_point, *source_token;
    uint32_t image_size, references;
    const mb_win_module_export *exports; /* borrowed, stable while active */
    size_t export_count;
} mb_win_module_record;

/* Exact x64 MODULEINFO shape: base 0/8, size_of_image 8/4, entry 16/8. */
typedef struct mb_win_module_info {
    void *base; uint32_t size_of_image; void *entry_point;
} mb_win_module_info;

/* Optional acquisition seam: acquire loads an unknown image (pe_loader route)
 * and must fill base/image_size plus a token; release disposes that token once
 * its final reference retires. Both return zero on success; a failed acquire
 * retains nothing; a failed release retains the record for free retry. */
typedef struct mb_win_module_enum_ops {
    void *context;
    int (*acquire)(void *, const char *ascii_name, void **base,
                   uint32_t *image_size, void **token);
    int (*release)(void *, void *base, void *token);
} mb_win_module_enum_ops;

typedef struct mb_win_module_enum {
    uint8_t active;
    mb_win_module_enum_ops ops;
    mb_win_module_record modules[MB_WIN_MODULE_MAX_MODULES];
} mb_win_module_enum;

typedef struct mb_win_module_enum_pin {
    struct mb_win_module_enum *owner;
    struct mb_win_module_enum_pin *next;
} mb_win_module_enum_pin;
typedef struct mb_win_module_enum_session {
    mb_win_module_enum_pin pin;
    struct mb_pe_module *module;
    struct mb_win_module_enum_session *next;
    uint8_t active;
} mb_win_module_enum_session;

mb_win_module_enum_status mb_win_module_enum_acquire_pin(mb_win_module_enum *,
    mb_win_module_enum_pin *);
mb_win_module_enum_status mb_win_module_enum_release_pin(mb_win_module_enum_pin *);
/* begin/end are the single-session primitives; the ABI unit owns the session
 * registry and re-exports them as mb_win_module_enum_abi_begin/end. */
mb_win_module_enum_status mb_win_module_enum_begin(mb_win_module_enum_session *,
    mb_win_module_enum *, struct mb_pe_module *);
mb_win_module_enum_status mb_win_module_enum_end(mb_win_module_enum_session *);

mb_win_module_enum_status mb_win_module_enum_register(mb_win_module_enum *,
    const mb_win_module_record *entry); /* references must be zero */
mb_win_module_enum_status mb_win_module_enum_find(mb_win_module_enum *,
    const char *ascii_name, void **base); /* NULL name -> executable image */
mb_win_module_enum_status mb_win_module_enum_find_ex(mb_win_module_enum *,
    uint32_t flags, const char *ascii_name_or_address, void **base);
mb_win_module_enum_status mb_win_module_enum_load(mb_win_module_enum *,
    const char *ascii_name, void **base);
mb_win_module_enum_status mb_win_module_enum_free(mb_win_module_enum *, void *handle);
mb_win_module_enum_status mb_win_module_enum_proc_name(mb_win_module_enum *,
    void *handle, const char *ascii_symbol, void **address);
mb_win_module_enum_status mb_win_module_enum_proc_ordinal(mb_win_module_enum *,
    void *handle, uint32_t ordinal, void **address);
mb_win_module_enum_status mb_win_module_enum_file_name(mb_win_module_enum *,
    void *handle, char *buffer, uint32_t size, uint32_t *written);
mb_win_module_enum_status mb_win_module_enum_base_name(mb_win_module_enum *,
    void *handle, char *buffer, uint32_t size, uint32_t *written);
mb_win_module_enum_status mb_win_module_enum_list(mb_win_module_enum *,
    void **bases, uint32_t capacity, uint32_t *needed); /* elements */
mb_win_module_enum_status mb_win_module_enum_info(mb_win_module_enum *,
    void *handle, mb_win_module_info *);
#ifdef __cplusplus
}
#endif
#endif
