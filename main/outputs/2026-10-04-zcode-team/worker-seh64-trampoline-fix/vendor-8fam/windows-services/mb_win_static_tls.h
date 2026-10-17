#ifndef MB_WIN_STATIC_TLS_H
#define MB_WIN_STATIC_TLS_H
/* VENDORED ADAPTED COPY (origin: new work; structure follows the
 * windows-services family owner pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.h and the CRT startup family of
 * outputs/2026-10-04-zcode-team/worker-crt-startup-implement/sources;
 * those files are unchanged). */
#include "mb_win_threads.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* PE32+ image TLS directory (winnt.h IMAGE_TLS_DIRECTORY64) raw fields. */
typedef struct mb_win_static_tls_values {
    uint64_t start_address_of_raw_data; /* VA of the .tls template start */
    uint64_t end_address_of_raw_data;   /* VA of the template end */
    uint64_t address_of_index;          /* VA of the module DWORD index slot */
    uint64_t address_of_callbacks;      /* VA of NULL-terminated callback array */
    uint32_t size_of_zero_fill;
    uint32_t characteristics;           /* carried opaque, never validated */
} mb_win_static_tls_values;

/* Documented Windows callback reasons (winnt.h DLL_PROCESS_ATTACH family). */
#define MB_WIN_STATIC_TLS_DLL_PROCESS_DETACH UINT32_C(0)
#define MB_WIN_STATIC_TLS_DLL_PROCESS_ATTACH UINT32_C(1)
#define MB_WIN_STATIC_TLS_DLL_THREAD_ATTACH  UINT32_C(2)
#define MB_WIN_STATIC_TLS_DLL_THREAD_DETACH  UINT32_C(3)
#define MB_WIN_STATIC_TLS_NO_INDEX UINT32_MAX

/* Owner-only registration record resolved from a directory: one per TLS-
 * bearing image. Caller-owned, stationary, zero-initialized before register.
 * resolved host addresses are computed by register from host_base and
 * preferred_base; raw template/index slot/callback array stay borrowed from
 * the mapped image and must remain mapped until unregister succeeds. */
typedef struct mb_win_static_tls mb_win_static_tls; /* completed below */
typedef struct mb_win_static_tls_module {
    mb_win_static_tls_values values;
    void *host_base;        /* mapped image base passed to callbacks */
    uint64_t preferred_base;/* link-time ImageBase for VA translation */
    uint8_t *raw_data;      /* resolved host template start */
    size_t raw_bytes;       /* end - start, must be >= 0 */
    uint32_t *index_slot;   /* resolved host AddressOfIndex DWORD */
    uintptr_t *callbacks;   /* resolved host callback array (may be empty) */
    mb_win_static_tls *owner;
    uint32_t tls_index;     /* MB_WIN_STATIC_TLS_NO_INDEX when unregistered */
    size_t live_blocks;     /* per-thread blocks currently allocated */
    struct mb_win_static_tls_module *next;
} mb_win_static_tls_module;

struct mb_win_static_tls { void *state; }; /* completes the forward typedef */
typedef struct mb_win_static_tls_pin { mb_win_static_tls *owner; struct mb_win_static_tls_pin *next; } mb_win_static_tls_pin;
typedef struct mb_win_static_tls_config { mb_win_thread_domain *domain; } mb_win_static_tls_config;
typedef enum mb_win_static_tls_status {
    MB_WIN_STATIC_TLS_OK = 0, MB_WIN_STATIC_TLS_INVALID_ARGUMENT,
    MB_WIN_STATIC_TLS_BAD_STATE, MB_WIN_STATIC_TLS_BUSY, MB_WIN_STATIC_TLS_NATIVE_ERROR
} mb_win_static_tls_status;

/* Static PE TLS for one open thread domain: directory parsing, per-thread
 * block allocation and callback dispatch for TLS-bearing retained images.
 * Freestanding C11; the only memory operations are the domain's
 * mb_win_threads_allocate/release (malloc-backed host side) and byte loops.
 *
 * Census authority (worker-dll-mod-retention/baseline-dll-census.json):
 * UE4SS.dll carries the directory (728 raw bytes, zero fill 0,
 * characteristics 0x500000, 2 callbacks, VAs 0x180c956f0/0x180c95a10);
 * dwmapi.dll has tls.directory_present=false. The family supports both
 * shapes and a zero-entry callback array.
 *
 * Zero-initialize the owner; open pins the domain exactly like an InitOnce
 * owner and close refuses while sessions, service pins, registered modules
 * or live per-thread blocks remain. Open/close and register/unregister are
 * externally serialized controls; attach/detach for distinct threads may
 * overlap and synchronize only on the domain mutex for block counting.
 * Register/unregister and every attach/detach must run on a domain-admitted
 * current thread (they consume the dynamic TLS index pool), never while
 * holding mb_win_threads_lock (the underlying TLS calls lock internally).
 *
 * Directory parsing is little-endian x64 only: 40 bytes -> 8 fields. The
 * parsed values stay raw PE (link-time preferred VAs); register resolves
 * host = host_base + (value - preferred_base) and refuses unresolved or
 * degenerate records (end < start, null index slot or callback array,
 * non-admitted thread, already-registered record). characteristics is
 * carried but never validated (census 0x500000 is section-align echo).
 *
 * Index management follows the Windows loader shape: register assigns ONE
 * dynamic index from the same domain pool as TlsAlloc and writes it into
 * the image's AddressOfIndex DWORD immediately (load time, before any
 * attach), so inlined __tls_index reads observe it. A module has at most
 * one record; one dynamic index per record is consumed until unregister.
 * Unregister requires zero live blocks, then frees the index back (which
 * NULLs that slot on all live threads) and leaves the image's index DWORD
 * untouched (stale, unread - documented Windows-compatible model choice).
 *
 * Per-thread blocks are exactly raw_bytes + size_of_zero_fill bytes. The
 * first allocation for a thread copies the mapped template then zero-fills
 * the remainder; the block pointer is stored through the domain dynamic
 * slot for the module index, so isolation, reuse-after-free cleanliness
 * and thread-exit slot drop are the mb_win_threads guarantees. Blocks are
 * per-thread and only the owning thread touches its bytes after publish.
 *
 * Callback dispatch reads the callback array live from mapped memory at
 * dispatch (it is relocation-updated), invokes every non-NULL entry with
 * (host_base, reason, reserved), and never dispatches under the domain
 * mutex. Attach dispatch runs ascending (first entry first); detach runs
 * descending, matching the documented Windows loader order. Reasons:
 * process attach 1 before the entry adapter calls DllMain; thread attach 2
 * after block allocation; thread detach 3 before block release; process
 * detach 0 before block release. Reserved is passed as NULL: this family
 * does not emulate a TEB. Attach calls return 0 on success, -1 on
 * failure (unregistered record or unadmitted thread); detach is void and
 * never fails. A failed attach leaves allocated state consistent (attach
 * is idempotent per thread: a second attach keeps the existing block and
 * dispatches again, exactly like Windows re-notify).
 *
 * Documented divergences from the Windows loader (no parity claim): blocks
 * for threads that existed before registration appear lazily at first
 * access or attach (Windows allocates eagerly for existing threads without
 * thread-attach callbacks; content is identical); there is no loader lock,
 * no DLL_MAIN-callback ordering enforcement beyond the documented call
 * order above (the entry adapter owns ordering), and no callback exception
 * trapping: an escaping callback/longjmp is a contract violation.
 */
/* worker-static-tls-7fam (vendored adaptation): the composition installs the
 * win64cc dispatch boundary (TLS callbacks in a mapped image are Windows x64
 * code; see TLS-ADMISSION-DESIGN.md section 5). fn receives the raw callback
 * address read live from the mapped array plus the module's host base and
 * reason; the default dispatcher (installed state) preserves the original
 * direct plain-C call for standalone family tests. Returns the previous
 * dispatcher; not thread-safe with concurrent dispatch (composition
 * contract: install before any module loads, single-threaded setup). */
typedef void (*mb_win_static_tls_dispatch_fn)(void *callback, void *host_base, uint32_t reason);
mb_win_static_tls_dispatch_fn mb_win_static_tls_set_dispatch(mb_win_static_tls_dispatch_fn fn);
mb_win_static_tls_status mb_win_static_tls_open(mb_win_static_tls *, const mb_win_static_tls_config *, int *);
mb_win_static_tls_status mb_win_static_tls_close(mb_win_static_tls *, int *);
mb_win_static_tls_status mb_win_static_tls_acquire_pin(mb_win_static_tls *, mb_win_static_tls_pin *);
mb_win_static_tls_status mb_win_static_tls_release_pin(mb_win_static_tls_pin *);
mb_win_thread_domain *mb_win_static_tls_domain(mb_win_static_tls *);
/* Parse 40 little-endian bytes of IMAGE_TLS_DIRECTORY64 into values.
 * 0 on success, -1 for a null pointer or out-of-range end < start. */
int mb_win_static_tls_parse(const void *directory64, mb_win_static_tls_values *);
mb_win_static_tls_status mb_win_static_tls_register(mb_win_static_tls *, mb_win_static_tls_module *,
    const mb_win_static_tls_values *, void *host_base, uint64_t preferred_base);
mb_win_static_tls_status mb_win_static_tls_unregister(mb_win_static_tls *, mb_win_static_tls_module *);
/* Registered record for a mapped image base (mb_pe_module::image), or NULL.
 * Externally serialized with register/unregister. */
mb_win_static_tls_module *mb_win_static_tls_module_for(mb_win_static_tls *, const void *host_base);
int mb_win_static_tls_process_attach(mb_win_static_tls_module *);
int mb_win_static_tls_thread_attach(mb_win_static_tls_module *);
void mb_win_static_tls_thread_detach(mb_win_static_tls_module *);
void mb_win_static_tls_process_detach(mb_win_static_tls_module *);
/* Calling thread's block; lazily allocates template+zero-fill with no
 * callbacks (pre-existing-thread path); NULL when unregistered or the
 * allocation or TLS publish fails. */
void *mb_win_static_tls_block(mb_win_static_tls_module *);
#ifdef __cplusplus
}
#endif
#endif
