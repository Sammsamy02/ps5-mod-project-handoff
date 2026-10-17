#ifndef MB_WIN_TLS_BRIDGE_H
#define MB_WIN_TLS_BRIDGE_H
#include "mb_win_threads.h"
#include "mb_win_heap_bridge.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mb_win_tls_bridge_session {
    mb_win_thread_pin pin;
    mb_pe_module *module;
    struct mb_win_tls_bridge_session *next;
    uint8_t active;
} mb_win_tls_bridge_session;
typedef enum mb_win_tls_bridge_status {
    MB_WIN_TLS_BRIDGE_OK = 0, MB_WIN_TLS_BRIDGE_INVALID_ARGUMENT,
    MB_WIN_TLS_BRIDGE_BAD_STATE, MB_WIN_TLS_BRIDGE_BUSY, MB_WIN_TLS_BRIDGE_OWNER_ERROR
} mb_win_tls_bridge_status;
typedef struct mb_win_runtime_bind_context {
    mb_win_tls_bridge_session *tls;
    mb_win_heap_bridge_session *process;
} mb_win_runtime_bind_context;

/* Native C ABI public interface, with private Win64 import implementations.
 * Zero-initialize sessions; domain, modules, sessions and callback code remain
 * stationary, disjoint and alive until end succeeds. begin requires an EMPTY
 * module and acquires a stable domain pin, even if no thread is registered.
 * There is one installed domain. Concurrent sessions must name that same domain,
 * and a module can have only one TLS session. end requires the original session
 * and module EMPTY. BUSY or failed end retains the pin and installed domain;
 * module CLEANUP therefore cannot retire callable imports prematurely.
 *
 * All begin/end/bind operations are externally serialized. The installed domain
 * is immutable while any session lives, so unrelated imported calls may continue
 * while another module adds or retires its session. First installation/final
 * retirement require all calls quiescent; retiring a target requires its calls
 * ended and module EMPTY. Designated loader service hooks may run controls under
 * a separate loader mutex. Ordinary TLS calls can overlap: the domain methods
 * provide their own locking. Every calling thread must enter the domain before
 * calling Windows code and leave only after its Windows calls/borrowed pointers
 * end. Do not hold the domain mutex around TLS imports. No callback/exception or
 * longjmp may escape a call. End does not free pointed-to memory or TLS indices;
 * the application releases its indices and payloads explicitly. Domain close
 * additionally refuses outstanding indices and admitted threads.
 *
 * Exact supported pairs: KERNEL32.dll!GetLastError, SetLastError, TlsAlloc,
 * TlsFree, TlsGetValue, TlsSetValue (DLL ASCII case-insensitive, names exact).
 * Get/SetLastError use uint32, TLS indices use uint32, BOOL uses int32, pointers
 * use 64 bits. GetLastError preserves the stored value; TlsGetValue clears it on
 * success, even for a NULL slot. Other successful TLS operations preserve it.
 * Unresolved pairs/ordinal imports are UNRESOLVED, invalid views or inactive/
 * copied sessions are ERROR. Only BOUND writes the result. Binding requires
 * module LOADING or ACTIVE. Static PE TLS directories remain unsupported.
 *
 * runtime_bind requires both original active sessions refer to the same module.
 * It tries the existing process binder first, propagates ERROR/BOUND, and tries
 * TLS only on UNRESOLVED. Caller must configure the heap callbacks/Windows errno
 * against this same native domain; opaque callback context cannot prove that.
 * Both sessions must survive every call and failed module cleanup. The binding
 * context is borrowed only during bind; neither session may be copied or moved.
 */
mb_win_tls_bridge_status mb_win_tls_bridge_begin(mb_win_tls_bridge_session *,
    mb_win_thread_domain *, mb_pe_module *);
mb_win_tls_bridge_status mb_win_tls_bridge_end(mb_win_tls_bridge_session *);
mb_pe_bind_result mb_win_tls_bridge_bind(void *, const mb_pe_import *, uint64_t *);
mb_pe_bind_result mb_win_runtime_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
