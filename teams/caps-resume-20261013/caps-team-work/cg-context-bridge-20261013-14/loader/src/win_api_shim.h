// win_api_shim.h -- Windows API import shim for the cap13 PE loader.
//
// Tiered policy (DESIGN/native-dll-loader.md section 5):
//   T1 MAP  memory / sync / time / thread / text / TLS-API emulation
//   T2 MAP  files + the GA path-shim rules (drive letters, backslashes,
//           Binaries/Win64 virtual-cwd, write confinement to the mod root)
//   T3 MAP  module registry (GetModuleHandle("UE4SS.dll") -> our surface)
//   T4 FAIL-CLOSED  EH/SEH runtime: precise diagnostic + mod stop, never
//           a silent no-op (cap13 acceptance requirement)
//   T5 REFUSE (load time, in the loader/census, not here) game-code
//           rewriting APIs and user32/gdi32 surfaces
//
// The shim bodies model selected Win32 signatures. Raw bodies remain private
// until a per-signature guest-callable wrapper is supplied and checked.
// CAP13 proposal-only; design-grade.
#ifndef CAP13_WIN_API_SHIM_H
#define CAP13_WIN_API_SHIM_H

#include <cstdint>
#include <string>
#include <unordered_map>

#include "pe_loader.h"

namespace cap13 {

// A mod-visible module handle ("HMODULE"): opaque index into the registry.
using WinModule = void*;

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

    // Catalog raw SysV handlers. They are NOT guest-callable and remain
    // unresolved imports until a per-signature Win64 wrapper is published.
    void register_builtin_exports();

    // Call only after generating and validating a guest-callable wrapper
    // that also supplies the handler's hidden Impl* context. Unknown rows,
    // null pointers, and malformed names refuse. Publication does not by
    // itself prove the wrapper's behavior; the caller owns that gate.
    bool publish_builtin_wrapper(const char* dll_name, const char* symbol,
                                 void* win64_entry);

    // ImportResolver: exact match on (dll, symbol) or (dll, ordinal).
    // Unknown => nullptr (the loader refuses with the full missing list).
    void* resolve(const char* dll_name, const char* symbol, uint64_t ordinal) override;

  private:
    Impl* m;
};

} // namespace cap13

#endif // CAP13_WIN_API_SHIM_H
