// win_api_shim.h -- Windows API import shim for the cap13 PE loader.
//
// Intended tiered policy (DESIGN/native-dll-loader.md section 5):
//   T1 MAP  memory / sync / time / thread / text / TLS-API emulation
//   T2 MAP  files + the GA path-shim rules (drive letters, backslashes,
//           Binaries/Win64 virtual-cwd, write confinement to the mod root)
//   T3 MAP  module registry (GetModuleHandle("UE4SS.dll") -> our surface)
//   T4 FAIL-CLOSED  EH/SEH runtime: precise diagnostic + mod stop, never
//           a silent no-op (cap13 acceptance requirement)
//   T5 REFUSE (load time, in the loader/census, not here) game-code
//           rewriting APIs and user32/gdi32 surfaces
//
// Most tiers remain candidate bodies or design. Raw bodies stay private;
// only explicitly reviewed integer rows can be published after sealing a
// context-bound Win64 wrapper. The public unguarded loader refuses real
// static TLS/GS; guarded preparation still needs a real segment backend.
// CAP13 proposal-only; design-grade.
#ifndef CAP13_WIN_API_SHIM_H
#define CAP13_WIN_API_SHIM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "pe_loader.h"

namespace cap13 {

// A mod-visible module handle ("HMODULE"): opaque index into the registry.
using WinModule = void*;

enum class BuiltinInstallStatus
{
    Ok,
    InvalidInput,
    Unsupported,
    SealFailed,
    AlreadyPublished,
    OutOfMemory
};

class WinApiShim : public ImportResolver
{
  public:
    struct Hooks
    {
        // Host logging sink (klog / UE4SS Output); level mirrors MBM_LOG_*.
        void (*log)(void* user, int level, const char* utf8) = nullptr;
        void* log_user = nullptr;
        // Fail-closed exit: stop THIS mod per NativeModAbi.h law 3.
        void (*stop_mod)(void* user, const char* why_utf8) = nullptr;
        void* stop_user = nullptr;
        // Optional recursive entry for LoadLibrary of additional PEs.
        // Null => LoadLibrary of unknown DLLs is refused at call time.
        WinModule (*load_pe)(void* user, const char* path) = nullptr;
        void* load_user = nullptr;
    };

    WinApiShim();

    struct Impl; // defined in win_api_shim.cpp; shared with the SysV impl
                 // functions in that file.
    void set_hooks(const Hooks& hooks);
    void set_mod_root(std::string utf8_root); // T2 confinement root

    // Register a module the way Windows would expose it:
    //  - "UE4SS.dll"  -> the ue4ss_compat export surface (design section 6)
    //  - mapped PE images registered by the adapter (mod -> mod imports)
    void register_module(const char* name, WinModule handle,
                         bool (*lookup)(WinModule, const char* symbol, void** out));

    // Reserve a mapped PE by the basename of its source path. Reject any
    // duplicate or built-in DLL name instead of replacing a live module.
    // The lookup receives its owner context and must refuse until that PE
    // has completed process attachment. A null symbol selects a PE ordinal.
    using MappedLookup = bool (*)(void* owner, const char* symbol,
                                  uint64_t ordinal, void** out,
                                  std::string* forwarder);
    bool register_mapped_module(const char* source_path, WinModule handle,
                                MappedLookup lookup, void* owner);
    // Caller must quiesce guest users of the module before removing it.
    bool unregister_mapped_module(const char* source_path, WinModule handle,
                                  void* owner);

    // Read-only namespace check for static dependency discovery. Presence
    // does not mean every symbol is callable; resolve still gates each row.
    bool has_module_surface(const char* dll_name);

    // The adapter registers each validated mapped DLL by its HMODULE/image
    // base before DllMain. A non-TLS module needs a callback that actually
    // suppresses its thread notifications; active static TLS always refuses
    // DisableThreadLibraryCalls. A duplicate handle is rejected.
    bool register_thread_notification_policy(WinModule module_handle,
        bool has_static_tls, bool (*disable)(void* user), void* user);
    // Remove a policy only after caller has quiesced all guest calls that
    // might already have copied its callback out of the registry lock.
    bool unregister_thread_notification_policy(WinModule module_handle);

    // Catalog raw SysV handlers. They are NOT guest-callable and remain
    // unresolved imports until a per-signature Win64 wrapper is published.
    void register_builtin_exports();

    // Stamp and publish one reviewed integer/pointer signature. The caller
    // owns writable_slot through process exit. seal must transition the
    // entire slot region to readable executable memory, including needed
    // target cache maintenance, and return false without publication on
    // failure. Neither this method nor the Mac host test proves PS5 sealing.
    using SealExecutable = bool (*)(void* memory, size_t bytes, void* user);
    BuiltinInstallStatus install_builtin_integer_wrapper(
        const char* dll_name, const char* symbol, void* writable_slot,
        size_t slot_bytes, SealExecutable seal, void* seal_user);

    // Optional platform backend for lazy installation during PE import
    // resolution. allocate_writable returns a region and its full size;
    // seal_executable must make it RX and perform target cache maintenance.
    // release is called on failed/duplicate installation. Published regions
    // remain owned by the platform until process exit. Callbacks must not
    // throw. No backend means reviewed imports remain unresolved.
    struct ExecutableMemoryBackend
    {
        bool (*allocate_writable)(size_t minimum_bytes, void** memory,
                                  size_t* region_bytes, void* user) = nullptr;
        SealExecutable seal_executable = nullptr;
        void (*release)(void* memory, size_t region_bytes, void* user) = nullptr;
        void* user = nullptr;
    };
    bool set_executable_backend(const ExecutableMemoryBackend& backend);

    // ImportResolver: exact match on (dll, symbol) or (dll, ordinal).
    // Reviewed built-ins install on demand if a backend is configured.
    // Unknown/unsupported => nullptr (loader reports the missing list).
    void* resolve(const char* dll_name, const char* symbol, uint64_t ordinal) override;

  private:
    Impl* m;
};

} // namespace cap13

#endif // CAP13_WIN_API_SHIM_H
