// NativeModLoaderPolicy.hpp -- which entry a loaded native mod module offers (#9).
// Engine-free; works with any module owner exposing FindExport(std::string_view)
// that throws when the symbol is missing (RC::Platform::CppModModule does).
#pragma once
#include <Mod/NativeModAbi.h>

#include <string>
#include <string_view>

namespace RC::Platform::NativeMods
{
    enum class EntryKind : uint8_t
    {
        AbiV1,        // mbm_mod_entry_v1: supported
        LegacyCppAbi, // only upstream start_mod/uninstall_mod: refused (see reason)
        None,
    };

    struct EntrySelection
    {
        EntryKind kind{EntryKind::None};
        mbm_mod_entry_v1_fn entry{};
        std::string reason;
    };

    template <typename Module>
    auto select_entry(const Module& module) -> EntrySelection
    {
        EntrySelection out;
        try
        {
            if (void* p = module.FindExport(MBM_ENTRY_SYMBOL))
            {
                out.kind = EntryKind::AbiV1;
                out.entry = reinterpret_cast<mbm_mod_entry_v1_fn>(p);
                return out;
            }
        }
        catch (...)
        {
        }
        bool legacy = false;
        try
        {
            legacy = module.FindExport("start_mod") != nullptr;
        }
        catch (...)
        {
        }
        if (legacy)
        {
            out.kind = EntryKind::LegacyCppAbi;
            out.reason = "module exports start_mod but not " MBM_ENTRY_SYMBOL
                         ": it was built against the UE4SS C++ ABI, which this runtime does not export "
                         "(no cross-module C++); rebuild it with the PS5 Mod Bridge native mod SDK";
            return out;
        }
        out.reason = "module exports neither " MBM_ENTRY_SYMBOL " nor start_mod";
        return out;
    }
} // namespace RC::Platform::NativeMods
