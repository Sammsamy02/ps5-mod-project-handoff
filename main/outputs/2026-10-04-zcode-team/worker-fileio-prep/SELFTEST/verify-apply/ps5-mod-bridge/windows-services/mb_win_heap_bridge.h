#ifndef MB_WIN_HEAP_BRIDGE_H
#define MB_WIN_HEAP_BRIDGE_H
#include "mb_win_heap.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mb_win_heap_bridge_config {
    mb_win_heap *heap;
    void *context;
    void (*lock)(void *);
    void (*unlock)(void *);
} mb_win_heap_bridge_config;
typedef struct mb_win_heap_bridge_session {
    mb_win_heap_pin pin;
    mb_pe_module *module;
    struct mb_win_heap_bridge_session *next;
    uint8_t active;
} mb_win_heap_bridge_session;
typedef enum mb_win_heap_bridge_status {
    MB_WIN_HEAP_BRIDGE_OK = 0, MB_WIN_HEAP_BRIDGE_INVALID_ARGUMENT,
    MB_WIN_HEAP_BRIDGE_BAD_STATE, MB_WIN_HEAP_BRIDGE_BUSY, MB_WIN_HEAP_BRIDGE_OWNER_ERROR
} mb_win_heap_bridge_status;

/* Native C ABI public surface; only the private import thunks use Win64 ABI.
 * This bridge installs one process-wide configuration. Every session holds an
 * owner pin from before module open until after module close succeeds. Additional
 * sessions must use the identical heap/context/lock/unlock configuration. A
 * module can have only one session. Zero-initialize sessions and keep sessions,
 * modules, heap and callback context stationary and disjoint for their lifetime.
 * The config is copied; source storage need not survive begin.
 *
 * All begin/end/bind control operations are externally serialized with each
 * other. Installed configuration is immutable while any session lives, so adding
 * or retiring other modules permits unrelated imported calls to continue. First
 * installation and final retirement require all calls quiescent; retiring a
 * target module requires its calls ended and module EMPTY. Designated loader
 * service hooks may run controls under a separate loader mutex. Ordinary calls
 * may run concurrently: each holds the supplied lock while accessing the owner.
 * Native heap operations must use that same lock while installed. Callbacks must
 * return normally, never reenter the bridge/owner, throw or longjmp. This is an
 * explicit process service, not automatic Windows loader/TLS/CRT initialization.
 *
 * begin requires an EMPTY module and active heap. bind accepts only its original
 * active session with a LOADING or ACTIVE module. Retain the session across
 * failed module cleanup and entry callbacks. end requires module EMPTY; it
 * refuses to retire the last session while heap allocations remain. Failed end
 * retains ownership and configuration. No thunk or borrowed errno pointer may
 * escape these lifetimes. Every thread must prepare the owner's valid per-thread
 * int32 Windows errno slot BEFORE calling DLL code; there is no fallback slot.
 * Context, callback code, the heap owner and this bridge code remain alive until
 * every session ends. No configuration swap is allowed with a session installed.
 *
 * Heap binder supports exact name pairs (DLL ASCII case-insensitive):
 * api-ms-win-crt-heap-l1-1-0.dll: malloc, calloc, realloc, free
 * api-ms-win-crt-runtime-l1-1-0.dll: _errno
 * Other pairs/ordinals are UNRESOLVED; invalid views/session are ERROR. Only
 * BOUND writes the output. process_bind additionally tries mb_win_crt_bind on
 * UNRESOLVED, providing the existing stateless CRT services through this session.
 */
mb_win_heap_bridge_status mb_win_heap_bridge_begin(mb_win_heap_bridge_session *,
    const mb_win_heap_bridge_config *, mb_pe_module *);
mb_win_heap_bridge_status mb_win_heap_bridge_end(mb_win_heap_bridge_session *);
mb_pe_bind_result mb_win_heap_bridge_bind(void *, const mb_pe_import *, uint64_t *);
mb_pe_bind_result mb_win_process_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
