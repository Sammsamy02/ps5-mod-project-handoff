#pragma once
// CAP13 managed route — lifecycle phase machine and framework mapping table.
// SOURCE STAGE ONLY: header consumed by MbmManagedBridge.cpp; nothing here
// builds or applies. See DESIGN/managed-dll-route.md sections 2d, 3d and 6c.
#include <cstdint>

#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION)
namespace RC::Cap13Managed
{
    // Resident-host phase machine. One direction only; no phase is ever
    // re-entered (Boot → Probe → Load → Tick → Shutdown/Failed/Quiesced).
    // Tick never overlaps admission: the engine lifecycle guarantees the
    // ready callback completes before the first tick arrives (same ready-
    // callback admission discipline as cap10).
    enum class Phase : std::uint8_t
    {
        NotStarted,   // host_start not called (or refused)
        Boot,         // resident runtime init (interpreter, static corlib)
        Probe,        // admission rung 1: cap10 Run/throw/recovery must pass
        Load,         // ModSdk + ModLoader + admitted mod images
        Tick,         // steady state: engine tick → Loader.Tick()
        Shutdown,     // OnUnload quiesce + runtime teardown (init thread)
        Failed,       // any stage refused; no mods loaded or ticks run
        Quiesced      // clean shutdown: no live callbacks; assemblies resident
    };

    // Framework classification mirrored from the discovery facts record.
    // The native discovery step owns the decision; this table owns what
    // lifecycle each shape WOULD get and what it is refused for today.
    enum class Framework : std::uint8_t { Mbmsdk, Standalone, Bepinex, Melonloader, Unknown };

    enum class Support : std::uint8_t { ActiveV1, GatedNoShim, Refused };

    struct FrameworkPolicy
    {
        Framework kind;
        Support support;
        const char* lifecycle;    // human-readable mapping
        const char* required;     // missing backing, precisely named
        const char* refusal;      // exact refusal reason code text
    };

    // The honest framework/API integration surface. "GatedNoShim" rows are
    // DESIGNED, NOT SHIPPED: admitting a BepInEx-shape mod without the
    // UnityEngine/backing layer would be a fake success (never-fabricate
    // law). The mapping exists so a future shim rung needs no new loader
    // design — only new admitted backing assemblies and a policy flip.
    inline const FrameworkPolicy* policy_for(Framework kind)
    {
        static const FrameworkPolicy table[] = {
            { Framework::Mbmsdk, Support::ActiveV1,
              "OnLoad(ModHost) -> OnTick() -> OnUnload()",
              "mscorlib (static) + Mbm.ModSdk (host-shipped)", nullptr },
            { Framework::Standalone, Support::Refused,
              "none (reference-only assembly)",
              "nothing; admitted only as a dependency of another admitted mod",
              "standalone mod with no recognized lifecycle and no referencer" },
            { Framework::Bepinex, Support::GatedNoShim,
              "ctor -> Awake -> [Start/Update/OnDestroy]",
              "BepInEx.dll shim + 0Harmony handling + UnityEngine backing + "
              "game managed assembly (Unreal game: none of these exist)",
              "bepinex-shape: unsatisfiable references (named at admission)" },
            { Framework::Melonloader, Support::GatedNoShim,
              "OnInitializeMelon -> OnUpdate/OnLateUpdate -> OnApplicationQuit",
              "MelonLoader.dll shim + UnityEngine backing / Il2Cpp interop "
              "(Unreal game: none of these exist)",
              "melonloader-shape: unsatisfiable references (named at admission)" },
            { Framework::Unknown, Support::Refused,
              "none",
              "unclassified reference set",
              "unknown framework: exact unmet reference list at admission" },
        };
        for (const FrameworkPolicy* p = table; p < table + 5; ++p)
            if (p->kind == kind) return p;
        return table + 4; // Unknown
    }

    // Tick policy (v1): single dispatch point, post-engine-update phase,
    // one Loader.Tick() per engine tick, on the engine tick thread only.
    // Per-mod tick isolation happens managed-side (design 3d); native
    // isolates only the loader entry itself. No per-mod threads, no timers,
    // no rescheduling — recurring schedules are explicitly out of v1.
    struct TickPolicy
    {
        static const bool per_mod_threads = false;
        static const bool native_per_mod_isolation = false; // loader owns it
        static const std::uint32_t tick_failure_limit_managed = 3; // mirrored in ModLoader.cs
    };
}
#endif
