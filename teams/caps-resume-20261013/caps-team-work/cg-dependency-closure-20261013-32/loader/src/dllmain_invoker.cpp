// dllmain_invoker.cpp -- invokes the mapped PE's entry point safely.
//
// For MSVC-linked DLLs the entry is _DllMainCRTStartup: one bridged call
// with DLL_PROCESS_ATTACH runs CRT init, static-duration ctors and atexit
// registration, then the author's DllMain (design 3f). This is why the
// loader calls AddressOfEntryPoint rather than hunting for a DllMain
// symbol.
//
// Detach policy (design 2/3f): images are never unmapped (retained-module
// law); DLL_PROCESS_DETACH is sent ONLY when the mod exported a working
// uninstall_mod and the host-driven teardown completed. A detach skipped is
// logged, never faked.
//
#include "dllmain_invoker.h"

#include "win64_bridge.h"

namespace cap13 {

namespace {
constexpr uint64_t DLL_PROCESS_ATTACH = 1;
constexpr uint64_t DLL_PROCESS_DETACH = 0;
} // namespace

// host (SysV) -> guest (Win64) DllMain(hinst=image base, reason, reserved).
// Signature is all-integer: inside the bridge envelope by construction.
EntryStatus invoke_dllmain_attach(const LoadResult& mod, uint64_t& out_entry_host)
{
    if (mod.entry_rva == 0) return EntryStatus::NoEntryPoint;
    void* entry = reinterpret_cast<uint8_t*>(mod.image) + mod.entry_rva;
    const uint64_t hinst = reinterpret_cast<uint64_t>(mod.image);
    out_entry_host = reinterpret_cast<uint64_t>(entry);
    const uint64_t rc = cap13_win64_call3(entry, hinst, DLL_PROCESS_ATTACH, 0);
    return rc != 0 ? EntryStatus::Ok : EntryStatus::ReturnedFalse;
}

// Detach: only after a confirmed clean teardown (see policy above).
void invoke_dllmain_detach_quiet(const LoadResult& mod)
{
    if (mod.entry_rva == 0) return;
    void* entry = reinterpret_cast<uint8_t*>(mod.image) + mod.entry_rva;
    (void)cap13_win64_call3(entry, reinterpret_cast<uint64_t>(mod.image), DLL_PROCESS_DETACH, 0);
}

} // namespace cap13
