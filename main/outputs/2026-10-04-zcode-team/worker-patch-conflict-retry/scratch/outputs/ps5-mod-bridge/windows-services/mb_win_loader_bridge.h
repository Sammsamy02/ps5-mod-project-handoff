#ifndef MB_WIN_LOADER_BRIDGE_H
#define MB_WIN_LOADER_BRIDGE_H
#include "mb_win_tls_bridge.h"
#include "../windows-execution/pe_loader.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_WIN_LOADER_NAME_LIMIT 255u

typedef struct mb_win_loader_bridge_config {
    mb_pe_loader *loader;
    mb_win_thread_domain *domain;
    void *context;
    void (*lock)(void *);
    void (*unlock)(void *);
} mb_win_loader_bridge_config;
typedef struct mb_win_loader_bridge_session {
    mb_pe_loader_pin loader_pin;
    mb_win_thread_pin domain_pin;
    mb_pe_module *module;
    struct mb_win_loader_bridge_session *next;
    uint8_t active;
} mb_win_loader_bridge_session;
typedef enum mb_win_loader_bridge_status {
    MB_WIN_LOADER_BRIDGE_OK = 0, MB_WIN_LOADER_BRIDGE_INVALID_ARGUMENT,
    MB_WIN_LOADER_BRIDGE_BAD_STATE, MB_WIN_LOADER_BRIDGE_BUSY, MB_WIN_LOADER_BRIDGE_OWNER_ERROR
} mb_win_loader_bridge_status;
typedef struct mb_win_loader_runtime_context {
    mb_win_loader_bridge_session *loader;
    mb_win_runtime_bind_context *runtime;
} mb_win_loader_runtime_context;

/* Native public ABI; eight private Win64 thunks implement exact KERNEL32.dll
 * names LoadLibraryA/W, LoadLibraryExA/W, GetModuleHandleA/W, GetProcAddress,
 * FreeLibrary. DLL names fold ASCII case; symbol names are exact. Composed bind
 * validates original sessions/same module, tries mb_win_runtime_bind first and
 * falls back here only on UNRESOLVED. Only BOUND writes the address output.
 *
 * Zero/stable sessions pin both the native loader and thread domain. begin
 * requires module EMPTY; end requires original session and module EMPTY, and
 * retains unretired pins on failure for retry. A single installed configuration
 * stays immutable while sessions exist; all additional sessions must match.
 * All begin/end/bind/control calls are serialized by the loader control domain.
 * Designated loader service hooks may call begin/end under an already-held
 * loader mutex; these controls never lock it again. Ordinary API thunks take
 * that separate mutex around native loader operations. It must not be the heap/
 * thread-domain mutex. First install/final end require all calls retired;
 * unrelated calls may continue when other same-config sessions are added/ended.
 * No loader reentry from DLL entry, callbacks or source/service hooks is allowed
 * except the documented pin/session operations. Calls return normally without
 * exceptions/longjmp. All objects, mutex/context and code outlive all sessions.
 * Each calling thread enters the configured domain before executing DLL code.
 * Heap/TLS runtime sessions must use that same domain by caller contract.
 *
 * Names are bounded to 255 ASCII characters (A bytes or W uint16 code units).
 * Non-ASCII names and paths are unsupported; catalog matching/optional .dll
 * follows pe_loader. There is no ANSI codepage, filesystem search or main-image
 * handle. Ex accepts only hFile=NULL, flags=0. Proc pointers 1..65535 encode
 * ordinals; all higher addresses must be readable NUL-terminated symbol names.
 * Input pointers otherwise follow the normal C readable-buffer contract.
 * Handles are native image bases; no foreign handle is dereferenced. Proc lookup
 * caches executable export leases but gains no load reference: callers must
 * retain a load/dependency through every call and borrowed pointer use.
 *
 * Success preserves last-error. Declared failure policy: 126 module not found,
 * 127 procedure not found, 6 invalid handle, 87 invalid argument, 50 unsupported,
 * 8 capacity exhaustion, 1114 module/service failure, 170 busy/deferred cleanup.
 * Failed Free may have consumed the reference; retry the retained handle or use
 * native collect. Cleanup never discards source bytes, service sessions or pins.
 * Existing static-TLS/unwind/no-entry admission limits remain in force.
 */
mb_win_loader_bridge_status mb_win_loader_bridge_begin(mb_win_loader_bridge_session *,
    const mb_win_loader_bridge_config *, mb_pe_module *);
mb_win_loader_bridge_status mb_win_loader_bridge_end(mb_win_loader_bridge_session *);
mb_pe_bind_result mb_win_loader_bridge_bind(void *, const mb_pe_import *, uint64_t *);
mb_pe_bind_result mb_win_loader_runtime_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
