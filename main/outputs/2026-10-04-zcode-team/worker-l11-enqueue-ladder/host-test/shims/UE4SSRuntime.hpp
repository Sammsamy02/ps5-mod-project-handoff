#pragma once
// HOST-TEST SHIM — UE4SSRuntime.hpp (worker-l11-enqueue-ladder).
// The availability wrappers in the staged source call
// UE4SSRuntime::IsEngineTickAvailable/IsProcessEventAvailable. The REAL
// functions report AOB/vtable address availability — NOT registrability
// (R3D 4.5 trap). The harness knob mirrors that semantics: in
// W_FAILS_R_WORKS the address IS assigned (vtable fallback) so these return
// true while registration (Unreal::Hook) is refused.

namespace RC
{
    struct UE4SSRuntime
    {
        static auto IsEngineTickAvailable() -> bool;
        static auto IsProcessEventAvailable() -> bool;
    };
} // namespace RC
