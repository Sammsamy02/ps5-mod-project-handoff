// pe_adapter.cpp -- the mbm_mod_entry_v1 wrapper around the whole PE route.
//
// The #9 host admits ONLY the C-table ABI (NativeModLoaderPolicy.hpp:
// EntryKind::AbiV1). The PE adapter is itself such a module: it exports
// mbm_mod_entry_v1, and the host table it receives is threaded into
// ue4ss_compat so the ORIGINAL Windows DLL's calls land in the host with
// no host-side changes and no UE4SS C++ symbol ever crossing (the design
// reason the #9 host refuses start_mod-only modules stays intact).
//
// Lifecycle mapping (mbm_mod_v1 fields -> guest):
//   on_program_start  -> guest start_mod()           (bridge call4)
//   on_update etc.    -> guest virtual slot dispatch  (bridge call4)
//   destroy           -> guest uninstall_mod(mod_obj) (bridge call4)
// Every callback returns mbm_status; any guest refusal converts to
// MBM_E_MOD_FAULT with the text preserved for last_error (ABI law 3).
// The mapped image is never unloaded (retained-module law).
//
// CAP13 proposal-only; design-grade; not built.
#include <cstdio>
#include <cstring>
#include <string>

#include "Mod/NativeModAbi.h"
#include "dllmain_invoker.h"
#include "pe_loader.h"
#include "win64_bridge.h"
#include "win_api_shim.h"

extern "C" {
// Provided by the adapter module itself (packaged as a PRX by the existing
// package_prx machinery; exports under its sce-NID plus this dynsym anchor
// exactly like every ABI-v1 mod).
mbm_status mbm_mod_entry_v1(const mbm_host_v1* host, uint64_t mod_token, const mbm_mod_v1** out_mod);
}

namespace cap13 {

struct PeModState
{
    LoadResult image;
    WinApiShim shim;
    void* guest_start_mod = nullptr;    // exported C function (Win64)
    void* guest_uninstall_mod = nullptr;
    uint64_t mod_object = 0;            // CppUserModBase* returned by start_mod
    bool started = false;
};

static PeModState* g_state = nullptr; // one PE mod per adapter module (M1: map)

// ---- mbm_mod_v1 callbacks (SysV; call the guest through the bridge) -----

static mbm_status cb_on_program_start(void* instance)
{
    PeModState& s = *static_cast<PeModState*>(instance);
    if (!s.guest_start_mod) return MBM_E_INTERNAL;
    // start_mod() -> CppUserModBase*  (Win64, no args, pointer return)
    const uint64_t obj = cap13_win64_call0(s.guest_start_mod);
    if (obj == 0) return MBM_E_MOD_FAULT;
    s.mod_object = obj;
    s.started = true;
    return MBM_OK;
}

static mbm_status cb_on_update(void* instance)
{
    PeModState& s = *static_cast<PeModState*>(instance);
    if (!s.started || !s.mod_object) return MBM_E_STATE;
    // Virtual slot dispatch: on_update lives at the slot index the pinning
    // tool derived from the vendored header (declaration order; MSVC bare
    // vtable). M0/M1 bake the index; the placeholder refuses here.
    return MBM_E_UNAVAILABLE; // placeholder until pinned slot table lands
}

static mbm_status cb_destroy(void* instance)
{
    PeModState& s = *static_cast<PeModState*>(instance);
    if (s.started && s.guest_uninstall_mod && s.mod_object)
    {
        // uninstall_mod(CppUserModBase*) (Win64, 1 arg)
        (void)cap13_win64_call1(s.guest_uninstall_mod, s.mod_object);
        s.mod_object = 0;
        s.started = false;
    }
    return MBM_OK; // image stays mapped (retained-module law); no dlclose
}

static const char* cb_last_error(void*)
{
    return "see klog"; // M1: per-state fault text buffer
}

// ---- the single export ---------------------------------------------------

mbm_status pe_adapter_entry(const mbm_host_v1* host, uint64_t mod_token, const mbm_mod_v1** out_mod)
{
    if (!host || !out_mod) return MBM_E_INVALID;
    if (host->magic != MBM_ABI_MAGIC || host->abi_version != MBM_ABI_VERSION) return MBM_E_INVALID;
    (void)mod_token; // M1: threaded into PeModState for host->register_* calls

    // M1 inputs: the original DLL path + expected hash arrive via the
    // manifest row (adapter packaging). Design shows the pipeline shape:
    //   read file -> hash check (bytes on disk never modified) ->
    //   WinApiShim with UE4SS surface registered -> pe_load -> refusal on
    //   TLS/RWX/missing imports with the named reasons -> exports
    //   start_mod/uninstall_mod -> invoke_dllmain_attach -> publish table.
    static mbm_mod_v1 table = {};
    table.magic = MBM_ABI_MAGIC;
    table.abi_version = MBM_ABI_VERSION;
    table.struct_size = sizeof(table);
    table.name = "pe-adapter";
    table.on_program_start = &cb_on_program_start;
    table.on_update = &cb_on_update;
    table.destroy = &cb_destroy;
    table.last_error = &cb_last_error;
    // on_unreal_init/on_ui_init/on_dll_load/on_cpp_mods_loaded wired in M1
    // against the pinned virtual slot table (same dispatch as on_update).
    static PeModState state;
    g_state = &state;
    table.instance = &state;
    *out_mod = &table;
    return MBM_OK;
}

} // namespace cap13

extern "C" mbm_status mbm_mod_entry_v1(const mbm_host_v1* host, uint64_t mod_token, const mbm_mod_v1** out_mod)
{
    return cap13::pe_adapter_entry(host, mod_token, out_mod);
}
