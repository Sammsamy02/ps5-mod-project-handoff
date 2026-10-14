#pragma once
// HOST-FIXTURE SHIM — Unreal/Hook.hpp (worker-menu-chain).
// Copied from the accepted worker-r3-ladder host-test shim
// (worker-l11-enqueue-ladder origin). ONE additive extension for the
// menu-chain fixture: `UnrealScriptFunctionCallableContext` — the empty
// carrier-parms context type in the REAL funcptr_marshal_hook signature.
// The fixture invokes the drain the way the R3 carrier post-hook would;
// parms/RESULT_DECL are never touched by the drain body (R3D 4.1/4.3).
// Minimal stand-in for the port's Hook API so the REAL staged regions
// (ensure_engine_tick_hooked / ensure_process_event_hooked / marshal_state)
// compile on macOS with __PROSPERO__ defined, exactly as the shipped PS5
// module compiles them. Registration success/refusal is a harness knob:
// in the W_FAILS_R_WORKS world Register*PreCallback returns ERROR_ID and
// stores nothing (the write-refused initializer world).

#include <cstdint>

namespace RC
{
    namespace Unreal
    {
        struct UObject;
        struct UFunction;
        struct UEngine;
        struct FName;
        struct UnrealScriptFunctionCallableContext
        {
        };

        namespace Hook
        {
            inline constexpr int32_t ERROR_ID = -1;
            using GlobalCallbackId = int32_t;

            template <typename T>
            struct TCallbackIterationData
            {
            };

            struct RegistrationParams
            {
                bool fire{};
                bool remove_after_call{};
                const wchar_t* mod_name{};
                const wchar_t* impl_name{};
            };

            using EngineTickCallback = auto (*)(TCallbackIterationData<void>&, UEngine*, float, bool) -> void;
            using ProcessEventCallback = auto (*)(TCallbackIterationData<void>&, UObject*, UFunction*, void*) -> void;
        } // namespace Hook
    } // namespace Unreal
} // namespace RC
