#ifndef MB_WIN_FILEIO_H
#define MB_WIN_FILEIO_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.h; that original file is unchanged). */
#include <stdint.h>
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Microsoft-documented x64 widths (widths are contracts):
 * HANDLE 8, BOOL 4, DWORD 4, FILETIME 8/4 (two DWORDs, so FILETIME-bearing
 * Windows structs align 4), WIN32_FIND_DATAW 592 (align 4),
 * WIN32_FILE_ATTRIBUTE_DATA 36 (align 4), BY_HANDLE_FILE_INFORMATION 52
 * (align 4), CREATEFILE2_EXTENDED_PARAMETERS 32 (align 8),
 * FILE_BASIC_INFO 40 (align 8), FILE_STANDARD_INFO 24 (align 8). */
typedef struct mb_win_fileio { void *state; } mb_win_fileio;
typedef struct mb_win_fileio_pin { mb_win_fileio *owner; struct mb_win_fileio_pin *next; } mb_win_fileio_pin;
/* root: host directory the controller dedicates to this owner. Every mod
 * path is resolved inside it (see the namespace rule in the block comment
 * below); no host path supplied by the mod is ever opened directly. */
typedef struct mb_win_fileio_config { mb_win_thread_domain *domain; const char *root; size_t capacity; } mb_win_fileio_config;
typedef enum mb_win_fileio_status {
    MB_WIN_FILEIO_OK=0, MB_WIN_FILEIO_INVALID_ARGUMENT, MB_WIN_FILEIO_BAD_STATE,
    MB_WIN_FILEIO_BUSY, MB_WIN_FILEIO_NATIVE_ERROR
} mb_win_fileio_status;
/* Microsoft-documented constants used by this adapter. */
#define MB_WIN_GENERIC_READ UINT32_C(0x80000000)
#define MB_WIN_GENERIC_WRITE UINT32_C(0x40000000)
#define MB_WIN_FILE_SHARE_READ UINT32_C(0x1)
#define MB_WIN_FILE_SHARE_WRITE UINT32_C(0x2)
#define MB_WIN_FILE_SHARE_DELETE UINT32_C(0x4)
#define MB_WIN_CREATE_NEW UINT32_C(1)
#define MB_WIN_CREATE_ALWAYS UINT32_C(2)
#define MB_WIN_OPEN_EXISTING UINT32_C(3)
#define MB_WIN_OPEN_ALWAYS UINT32_C(4)
#define MB_WIN_TRUNCATE_EXISTING UINT32_C(5)
#define MB_WIN_FILE_ATTRIBUTE_READONLY UINT32_C(0x1)
#define MB_WIN_FILE_ATTRIBUTE_DIRECTORY UINT32_C(0x10)
#define MB_WIN_FILE_ATTRIBUTE_NORMAL UINT32_C(0x80)
#define MB_WIN_FILE_FLAG_BACKUP_SEMANTICS UINT32_C(0x02000000)
#define MB_WIN_FILE_FLAG_OVERLAPPED UINT32_C(0x40000000)
#define MB_WIN_INVALID_HANDLE_VALUE ((void *)(uintptr_t)UINT64_MAX)
typedef struct mb_win_filetime { uint32_t low, high; } mb_win_filetime;
typedef struct mb_win_w32_file_attribute_data { /* WIN32_FILE_ATTRIBUTE_DATA */
    uint32_t attributes;
    mb_win_filetime creation, last_access, last_write;
    uint32_t size_high, size_low;
} mb_win_w32_file_attribute_data;
typedef struct mb_win_by_handle_file_information { /* BY_HANDLE_FILE_INFORMATION */
    uint32_t attributes;
    mb_win_filetime creation, last_access, last_write;
    uint32_t volume_serial, size_high, size_low, links, index_high, index_low;
} mb_win_by_handle_file_information;
typedef struct mb_win_w32_find_data { /* WIN32_FIND_DATAW */
    uint32_t attributes;
    mb_win_filetime creation, last_access, last_write;
    uint32_t size_high, size_low, reserved0, reserved1;
    uint16_t name[260];
    uint16_t alternate[14];
} mb_win_w32_find_data;
typedef struct mb_win_createfile2_extended { /* CREATEFILE2_EXTENDED_PARAMETERS */
    uint32_t size, file_attributes, file_flags, security_qos_flags;
    const void *security_attributes;
    const void *template_file;
} mb_win_createfile2_extended;
typedef struct mb_win_file_basic_info { uint64_t creation, last_access, last_write, change; uint32_t attributes; } mb_win_file_basic_info;
typedef struct mb_win_file_standard_info { uint64_t allocation_size, end_of_file; uint32_t links; uint8_t delete_pending, directory; } mb_win_file_standard_info;
/* Windows last-error codes used by this adapter: documented unless the
 * REPORT marks the specific path adapter-policy. */
#define MB_WIN_ERROR_FILE_NOT_FOUND UINT32_C(2)
#define MB_WIN_ERROR_PATH_NOT_FOUND UINT32_C(3)
#define MB_WIN_ERROR_ACCESS_DENIED UINT32_C(5)
#define MB_WIN_ERROR_INVALID_HANDLE UINT32_C(6)
#define MB_WIN_ERROR_NOT_ENOUGH_MEMORY UINT32_C(8)
#define MB_WIN_ERROR_NO_MORE_FILES UINT32_C(18)
#define MB_WIN_ERROR_SHARING_VIOLATION UINT32_C(32)
#define MB_WIN_ERROR_NOT_SUPPORTED UINT32_C(50)
#define MB_WIN_ERROR_INVALID_PARAMETER UINT32_C(87)
#define MB_WIN_ERROR_INSUFFICIENT_BUFFER UINT32_C(122)
#define MB_WIN_ERROR_INVALID_NAME UINT32_C(123)
#define MB_WIN_ERROR_FILENAME_EXCED_RANGE UINT32_C(206)
#define MB_WIN_ERROR_GEN_FAILURE UINT32_C(31)
#define MB_WIN_ERROR_CALL_NOT_IMPLEMENTED UINT32_C(120)
#define MB_WIN_ERROR_ALREADY_EXISTS UINT32_C(183)
#define MB_WIN_ERROR_BROKEN_PIPE UINT32_C(109) /* documented for pipes; this pass has no pipe objects, so it is never raised */
/* Zeroed stationary owner. capacity bounds simultaneously-open handles
 * (file AND find handles share one table). Domain/code outlive owner,
 * sessions and calls. Owner pins the domain; close refuses service pins or
 * any open handle. Every calling native thread must already be admitted to
 * the domain.
 *
 * VIRTUAL NAMESPACE RULE (exact): the controller supplies one host
 * directory at open time. Every mod-supplied path is UTF-16 and is
 * interpreted RELATIVE TO THAT ROOT as follows. Both '/' and '\' separate
 * components. Leading separators are stripped (the path is root-relative);
 * a drive/mount prefix (any ':' before the first separator) is rejected
 * with 87 (adapter-policy containment refusal; Windows would accept it);
 * '..' components are rejected with 87 (containment refusal; Windows would
 * normalize); '.' and empty components are dropped; trailing separators
 * are dropped. Components longer than 255 UTF-16 units fail with 123
 * ERROR_INVALID_NAME (policy); a normalized path longer than 32767 units
 * fails with 206 ERROR_FILENAME_EXCED_RANGE (documented bound). Matching
 * inside the namespace is case-insensitive ASCII (documented NTFS default;
 * per-directory case sensitivity is not modeled and
 * FIND_FIRST_EX_CASE_SENSITIVE fails fast). Host symlinks anywhere under
 * the root are refused with 5 ERROR_ACCESS_DENIED (containment policy);
 * the controller must dedicate a real directory. No host file descriptor
 * ever leaks: Windows HANDLE values are opaque 64-bit tokens of this
 * owner's table; INVALID_HANDLE_VALUE (-1) and 0 are never issued.
 *
 * Semantics: ReadFile/WriteFile are SYNCHRONOUS on per-handle offsets
 * (pread/pwrite; each handle has an independent file pointer, as on
 * Windows). Overlapped I/O is out of scope this pass: FILE_FLAG_OVERLAPPED
 * or a non-NULL lpOverlapped fails fast with 50 ERROR_NOT_SUPPORTED
 * (policy) instead of faking success. Sizes are bounded: transfers above
 * 1 GiB fail with 87 (policy). Share modes are enforced per normalized
 * path (read/write/delete conflicts fail with 32 ERROR_SHARING_VIOLATION,
 * documented). CREATE_ALWAYS/OPEN_ALWAYS on an existing file succeed and
 * set last error 183 ERROR_ALREADY_EXISTS exactly as Microsoft documents;
 * every OTHER success preserves the thread's last error. FindFirstFile
 * wildcards '*' and '?' are matched case-insensitively against long names
 * only: the 8.3 short-name emulation quirk ('*.*' matching extensionless
 * names) is NOT modeled (policy); '.' and '..' are returned when the
 * pattern matches them, as on Windows (documented). The current directory
 * is fixed at the namespace root '\' (SetCurrentDirectory is not census
 * observed); GetCurrentDirectoryW reports it. GetFinalPathNameByHandleW
 * returns '\\?\vfs' + '\' + normalized path (policy format), VOLUME_NAME_DOS
 * only. AreFileApisANSI returns TRUE (policy: fixed, matching the common
 * ANSI code page state; SetFileApisToOEM is not implemented). Times are
 * host stat times converted to FILETIME; negative host times clamp to 0
 * (policy). BY_HANDLE volume serial/index are opaque values derived from
 * host ids (policy). SetFileInformationByHandle and the change
 * notification family are census-observed but NOT implemented this pass:
 * they fail loudly with 120 ERROR_CALL_NOT_IMPLEMENTED (policy) and must
 * never fake success. CloseHandle is a separate census family (mutex
 * slice); this owner exposes mb_win_fileio_close_handle for that future
 * owner to delegate file-handle closing, and never binds the CloseHandle
 * import itself. Success/try APIs never fake results; failed calls write
 * no outputs unless the API documents writing them. No cancellation,
 * longjmp or exception escapes an operation. Not process-shared.
 */
mb_win_fileio_status mb_win_fileio_open(mb_win_fileio *,const mb_win_fileio_config *,int *);
mb_win_fileio_status mb_win_fileio_close(mb_win_fileio *,int *);
mb_win_fileio_status mb_win_fileio_acquire_pin(mb_win_fileio *,mb_win_fileio_pin *);
mb_win_fileio_status mb_win_fileio_release_pin(mb_win_fileio_pin *);
mb_win_thread_domain *mb_win_fileio_domain(mb_win_fileio *);
/* File-handle close service for the (separate) CloseHandle owner. */
int32_t mb_win_fileio_close_handle(mb_win_fileio *,void *handle);
/* Microsoft-documented KERNEL32 entry points (16 implemented this pass).
 * Like the InitOnce owner, every op takes its owning object first; the
 * ms_abi bridge supplies the single installed owner. */
void *mb_win_fileio_create_file_w(mb_win_fileio *,const uint16_t *file_name,uint32_t desired_access,uint32_t share_mode,const void *security_attributes,uint32_t disposition,uint32_t flags_and_attributes,const void *template_file);
void *mb_win_fileio_create_file_2(mb_win_fileio *,const uint16_t *file_name,uint32_t desired_access,uint32_t share_mode,uint32_t disposition,const mb_win_createfile2_extended *extended_parameters);
int32_t mb_win_fileio_read_file(mb_win_fileio *,void *handle,void *buffer,uint32_t bytes_to_read,uint32_t *bytes_read,const void *overlapped);
int32_t mb_win_fileio_write_file(mb_win_fileio *,void *handle,const void *buffer,uint32_t bytes_to_write,uint32_t *bytes_written,const void *overlapped);
void *mb_win_fileio_find_first_file_w(mb_win_fileio *,const uint16_t *file_pattern,mb_win_w32_find_data *find_data);
void *mb_win_fileio_find_first_file_ex_w(mb_win_fileio *,const uint16_t *file_pattern,int32_t info_level,void *find_data,int32_t search_op,const void *search_filter,uint32_t additional_flags);
int32_t mb_win_fileio_find_next_file_w(mb_win_fileio *,void *handle,mb_win_w32_find_data *find_data);
int32_t mb_win_fileio_find_close(mb_win_fileio *,void *handle);
int32_t mb_win_fileio_are_file_apis_ansi(mb_win_fileio *);
int32_t mb_win_fileio_get_file_attributes_ex_w(mb_win_fileio *,const uint16_t *file_name,int32_t information_level,void *information);
int32_t mb_win_fileio_get_file_information_by_handle(mb_win_fileio *,void *handle,mb_win_by_handle_file_information *information);
int32_t mb_win_fileio_get_file_information_by_handle_ex(mb_win_fileio *,void *handle,int32_t information_class,void *information,uint32_t size);
int32_t mb_win_fileio_create_directory_w(mb_win_fileio *,const uint16_t *path_name,const void *security_attributes);
int32_t mb_win_fileio_delete_file_w(mb_win_fileio *,const uint16_t *file_name);
uint32_t mb_win_fileio_get_current_directory_w(mb_win_fileio *,uint32_t buffer_length,uint16_t *buffer);
uint32_t mb_win_fileio_get_final_path_name_by_handle_w(mb_win_fileio *,void *handle,uint16_t *buffer,uint32_t buffer_length,uint32_t flags);
/* Census-observed but explicitly unsupported this pass (fail-fast stubs). */
int32_t mb_win_fileio_set_file_information_by_handle(mb_win_fileio *,void *handle,int32_t information_class,const void *information,uint32_t size);
void *mb_win_fileio_find_first_change_notification_w(mb_win_fileio *,const uint16_t *path_name,int32_t watch_subtree,uint32_t notify_filter);
int32_t mb_win_fileio_find_next_change_notification(mb_win_fileio *,void *handle);
int32_t mb_win_fileio_find_close_change_notification(mb_win_fileio *,void *handle);
#ifdef __cplusplus
}
#endif
#endif
