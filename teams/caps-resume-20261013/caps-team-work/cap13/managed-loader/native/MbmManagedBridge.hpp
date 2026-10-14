#pragma once
// CAP13 managed route — resident Mono host bridge. SOURCE STAGE ONLY.
// NOT an implementation of anything unproven: target compilation is guarded
// behind the cap10 admitted-port switches PLUS a cap13 resident-owner switch,
// so accidental host compilation is impossible; host-validation mode is a
// separate explicit switch (Mac rungs M1-M3, design section 5.1).
//
// Composition delta vs cap10 (design section 6d, staged for integrator
// review, nothing applied): cap10's MonoOwner is probe-shaped — its Lifetime
// runs mono_jit_cleanup before run_embedded_probe_once() returns. A mod host
// needs a RESIDENT runtime. This bridge mirrors cap10's MonoOwner
// admission-for-admission (one-shot claim, exact mono_set_dirs PREFIX
// semantics, MONO_AOT_MODE_INTERP, static corlib registration, owned-name
// dl-fallback, per-symbol dllmap inserts, image-identity checks) with the
// lifetime moved to explicit shutdown on the init thread, and runs the cap10
// Run/throw/recovery boundary as admission rung 1 inside that resident
// runtime. cap10 files are not modified.
//
// Discovery/admission authority: the integrator's discovery step (facts from
// the PE/CLR analyzer + dependency closure per design section 2c) supplies
// the admitted mod list. This bridge re-verifies byte lengths and image
// identity at open, but does NOT re-parse PE/CLR metadata.
#include <cstdint>

#if defined(MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION) && defined(__PROSPERO__)
#error "Host validation cannot be used for a target build"
#endif
#if !defined(MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION) && \
    (!defined(__PROSPERO__) || !defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) || \
     MODBRIDGE_CAP10_ADMITTED_MONO_PORT != 1 || \
     !defined(MODBRIDGE_CAP13_RESIDENT_OWNER) || MODBRIDGE_CAP13_RESIDENT_OWNER != 1)
#error "Requires the admitted cap10 Mono port AND the reviewed resident-owner composition"
#endif
#if !defined(MODBRIDGE_CAP13_CORLIB_DIR) || !defined(MODBRIDGE_CAP13_CONFIG_DIR)
#error "Package owner must provide actual matching runtime corlib/config directories (PREFIX semantics: lib prefix and etc prefix)"
#endif

namespace RC::Cap13Managed
{
    // One admitted mod, supplied by the discovery step. Bytes remain owned by
    // the caller until host_start returns; the bridge copies via
    // mono_image_open_from_data_with_name(need_copy=true).
    struct ModImageInput
    {
        const char* assembly_name;   // self-declared Assembly name (facts record)
        const char* sha8;            // first 8 hex of the admitted sha256
        const std::uint8_t* bytes;   // mod image bytes (read-only overlay copy)
        std::uint32_t size;          // byte length
    };

    // Diagnostic receipt surface — same tags/marker discipline as cap10
    // (diagnostic_event into klog; values are small unsigned fields).
    // extern "C" void diagnostic_event(const char*, unsigned int); // provided by host image

    // Real report. Numbers are receipt fields, not proof by themselves.
    struct StartReport
    {
        std::uint8_t  phase{};        // final Phase as uint8 (ManagedLifecycle.hpp)
        bool runtime_init{};          // resident interpreter + static corlib up
        bool probe_rung_passed{};     // cap10 Run/throw/recovery inside resident runtime
        std::uint8_t mods_admitted{}; // images actually loaded
        std::uint8_t mods_failed{};   // refused/failed after admission
        bool loader_invoked{};        // LoaderMain ran to completion
        bool loader_ok{};             // LoaderMain returned 0
        bool completed{};             // start reached steady Tick phase
    };

    struct TickReport
    {
        std::uint64_t ticks_dispatched{};
        bool thread_attached{};       // engine tick thread attached exactly once
        bool loader_threw{};          // Loader.Tick() itself threw (loader-level defect)
        bool accepted{};              // window discipline accepted the tick
    };

    struct ShutdownReport
    {
        bool unload_invoked{};
        bool images_closed{};
        bool runtime_cleaned{};       // mono_jit_cleanup on the init thread
        bool provider_unregistered{};
        bool completed{};
    };

    // Engine-surface binding (fail-closed). The integrator binds these to the
    // current #9 admitted native mod services; until bound, every engine call
    // returns MBM_CS_ENGINE_UNBOUND and mods observe a precise refusal.
    struct EngineSurface
    {
        // All mirror abi/MbmManagedAbi.h shapes; return MBM_CS_* codes.
        std::int32_t (*find_object)(const char* object_path, void** out_handle);
        std::int32_t (*read_number)(void* handle, const char* property_name, double* out_value);
        std::int32_t (*release)(void** handle);
    };
    // Bind once, before host_start. Passing {nullptr,nullptr,nullptr} unbinds
    // (fail-closed). Binding after start is refused.
    bool bind_engine_surface(const EngineSurface* surface);

    // Lifecycle entries (see ManagedLifecycle.hpp phase machine).
    // host_start: call ONCE from the admitted engine-ready callback (the
    // cap10 ready-admission discipline; not from an arbitrary thread).
    bool host_start(const ModImageInput* mods, std::uint32_t mod_count, StartReport* report) noexcept;
    // host_tick: call from the engine tick thread per engine tick, Tick phase only.
    bool host_tick(TickReport* report) noexcept;
    // host_shutdown: call ONCE from the host_start (init) thread at module exit.
    bool host_shutdown(ShutdownReport* report) noexcept;
}
