// CAP13 managed route — resident Mono host bridge. SOURCE STAGE ONLY.
// Design: DESIGN/managed-dll-route.md sections 3 and 6c. Nothing in this
// file has been compiled or run. Mirrors cap10/native/MonoOwner.cpp +
// InvocationBoundary.cpp admission discipline with the LIFETIME DELTA
// documented in the header (resident runtime; teardown at explicit shutdown
// on the init thread).
//
// Preconditions that remain integrator/console proof (never claimed here):
// genuine target Mono port (GC/TLS/EH/signals), matching corlib+config at
// the admitted dirs, the strict owned-name loader patch inside that runtime,
// single Mono owner per process, the in-game Townfall Probe pass, and an
// engine surface actually bound to #9's admitted services.
#include "MbmManagedBridge.hpp"
#include "ManagedLifecycle.hpp"
#include "MbmLoaderImage.hpp"             // staged hash-pinned host images; FAILS CLOSED until regenerated
// cap10 sources are consumed read-only from their reviewed locations; the
// integrator supplies these include paths (cap10 MAC-INTEGRATION-HANDOFF).
#include "ProbeImage.hpp"                 // cap10 embedded Probe image (rung 1)
#include "Cap10InvocationObservation.hpp" // cap10 same-image observer header
#include "InvocationBoundary.hpp"         // cap10 RC::Cap10Boundary declarations
#include "../../abi/MbmManagedAbi.h"
#include <mono/mini/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/image.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/object.h>
#include <mono/metadata/threads.h>
#include <mono/utils/mono-dl-fallback.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>

extern "C" void diagnostic_event(const char*, unsigned int);
extern "C" std::int32_t modbridge_cap10_abi_version() noexcept;
extern "C" std::int32_t modbridge_cap10_runtime_probe(std::int32_t, std::int32_t*) noexcept;
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP13_STATIC_CORLIB)
extern "C" void* mono_aot_module_mscorlib_info; // generated symbol; VALUE is the info pointer
#endif

namespace RC::Cap13Managed
{
    namespace
    {
        // ---- diagnostics (receipt fields; never proof by themselves) ----
        void emit(const char* tag, unsigned value) noexcept
        {
            try { diagnostic_event(tag, value); } catch (...) {}
        }

        // ---- shared resident state (single owner per lifecycle contract) ----
        constexpr std::uint32_t max_mods = 64;
        const char* mod_names[max_mods]{};
        std::atomic_bool start_claimed{false};
        std::atomic_bool bind_claimed{false};
        std::atomic<Phase> phase{Phase::NotStarted};
        std::atomic<std::uint64_t> ticks_dispatched{0};
        std::atomic<std::uint32_t> mods_admitted{0};
        std::atomic<std::uint32_t> mod_name_count{0};
        std::atomic<std::uint64_t> boot_serial{1};
        Lifetime* lifetime{}; // set by host_start; consumed by host_shutdown

        // ---- frozen v1 ABI provider (abi/MbmManagedAbi.h shapes) ----
        std::int32_t host_abi_version() { return MBM_CS_ABI_VERSION; }

        std::int32_t host_log(std::int32_t level, const char* message)
        {
            if (!message || level < 0 || level > 3) return MBM_CS_BAD_ARGUMENT;
            // Marker receipt: level + first character only. Full-text managed
            // logging binds through the integrator's logger, never assumed.
            emit("CAP13_MLOG", (static_cast<unsigned>(level) << 8) |
                               (static_cast<unsigned>(message[0]) & 0xFFu));
            return MBM_CS_OK;
        }

        std::int32_t host_query(std::int32_t key, std::int64_t* out_value)
        {
            if (!out_value) return MBM_CS_BAD_ARGUMENT;
            *out_value = 0;
            switch (key)
            {
                case MBM_QUERY_TICK_COUNT:
                    *out_value = static_cast<std::int64_t>(ticks_dispatched.load());
                    return MBM_CS_OK;
                case MBM_QUERY_MOD_COUNT:
                    *out_value = static_cast<std::int64_t>(mods_admitted.load());
                    return MBM_CS_OK;
                case MBM_QUERY_BOOT_ID:
                    *out_value = static_cast<std::int64_t>(boot_serial.load());
                    return MBM_CS_OK;
                default:
                    return MBM_CS_NOT_FOUND;
            }
        }

        std::int32_t host_mod_assembly(std::int32_t index, char* buffer,
                                       std::int32_t* len_inout)
        {
            if (!len_inout || index < 0 ||
                index >= static_cast<std::int32_t>(mod_name_count.load()))
                return MBM_CS_BAD_ARGUMENT;
            const char* name = mod_names[static_cast<unsigned>(index)];
            if (!name) return MBM_CS_NOT_FOUND;
            const std::int32_t needed =
                static_cast<std::int32_t>(std::strlen(name)) + 1;
            if (!buffer || *len_inout < needed)
            {
                *len_inout = needed; // MBM_CS_BUFFER_TOO_SMALL contract
                return MBM_CS_BUFFER_TOO_SMALL;
            }
            std::memcpy(buffer, name, static_cast<std::size_t>(needed));
            return MBM_CS_OK;
        }

        // Engine-surface binding (fail-closed until the integrator binds #9).
        std::atomic<const EngineSurface*> engine_surface{nullptr};

        std::int32_t engine_find_object(const char* path, void** out_handle)
        {
            if (!out_handle) return MBM_CS_BAD_ARGUMENT;
            *out_handle = nullptr;
            const EngineSurface* surface = engine_surface.load();
            if (!surface || !surface->find_object) return MBM_CS_ENGINE_UNBOUND;
            return surface->find_object(path, out_handle);
        }
        std::int32_t engine_read_number(void* handle, const char* prop, double* out_value)
        {
            if (!out_value || !handle) return MBM_CS_BAD_ARGUMENT;
            *out_value = 0.0;
            const EngineSurface* surface = engine_surface.load();
            if (!surface || !surface->read_number) return MBM_CS_ENGINE_UNBOUND;
            return surface->read_number(handle, prop, out_value);
        }
        std::int32_t engine_release(void** handle)
        {
            if (!handle) return MBM_CS_BAD_ARGUMENT;
            const EngineSurface* surface = engine_surface.load();
            if (!surface || !surface->release) return MBM_CS_ENGINE_UNBOUND;
            return surface->release(handle);
        }

        // ---- provider symbol resolution ----
        // cap13's provider answers the frozen cap13 ABI plus the two cap10
        // probe symbols (the rung-1 boundary needs them resolved inside this
        // runtime, exactly as cap10's owner resolved them). Integration note:
        // if the composed build's cap10 callback patch also registers a
        // fallback for these, reconcile to ONE answering source at review —
        // mono_dl_fallback_register PREPENDS, so later registration is
        // consulted first (cap10 review section 3 finding).
        struct AbiEntry { const char* symbol; std::atomic_bool admitted; };
        AbiEntry abi_table[] = {
            {"mbm_cs_abi_version", {}},
            {"mbm_cs_log", {}},
            {"mbm_cs_host_query", {}},
            {"mbm_cs_mod_assembly", {}},
            {"mbm_cs_engine_find_object", {}},
            {"mbm_cs_engine_read_number", {}},
            {"mbm_cs_engine_release", {}},
        };
        AbiEntry cap10_probe_symbols[] = {
            {"modbridge_cap10_abi_version", {}},
            {"modbridge_cap10_runtime_probe", {}},
        };
        constexpr char owned_library[] = "modbridge-owned:cap13-abi-v1";

        void* resolve_abi_symbol(const char* name)
        {
            if (std::strcmp(name, "mbm_cs_abi_version") == 0)
                return reinterpret_cast<void*>(&host_abi_version);
            if (std::strcmp(name, "mbm_cs_log") == 0)
                return reinterpret_cast<void*>(&host_log);
            if (std::strcmp(name, "mbm_cs_host_query") == 0)
                return reinterpret_cast<void*>(&host_query);
            if (std::strcmp(name, "mbm_cs_mod_assembly") == 0)
                return reinterpret_cast<void*>(&host_mod_assembly);
            if (std::strcmp(name, "mbm_cs_engine_find_object") == 0)
                return reinterpret_cast<void*>(&engine_find_object);
            if (std::strcmp(name, "mbm_cs_engine_read_number") == 0)
                return reinterpret_cast<void*>(&engine_read_number);
            if (std::strcmp(name, "mbm_cs_engine_release") == 0)
                return reinterpret_cast<void*>(&engine_release);
            if (std::strcmp(name, "modbridge_cap10_abi_version") == 0)
                return reinterpret_cast<void*>(&modbridge_cap10_abi_version);
            if (std::strcmp(name, "modbridge_cap10_runtime_probe") == 0)
                return reinterpret_cast<void*>(&modbridge_cap10_runtime_probe);
            return nullptr; // unknown import -> DllNotFoundException names it
        }

        struct Provider { std::atomic_bool load{}; };
        void* provider_load(const char* name, int, char**, void* user)
        {
            if (!name || std::strcmp(name, owned_library) != 0) return nullptr;
            static_cast<Provider*>(user)->load = true;
            return user;
        }
        void* provider_symbol(void* handle, const char* name, char**, void* user)
        {
            if (!name || handle != user) return nullptr;
            for (AbiEntry& entry : abi_table)
                if (std::strcmp(name, entry.symbol) == 0)
                {
                    entry.admitted = true;
                    return resolve_abi_symbol(name);
                }
            for (AbiEntry& entry : cap10_probe_symbols)
                if (std::strcmp(name, entry.symbol) == 0)
                {
                    entry.admitted = true;
                    return resolve_abi_symbol(name);
                }
            return nullptr;
        }
        void* provider_close(void*, void*) { return nullptr; }

        // ---- resident lifetime (heap-owned; teardown at host_shutdown) ----
        struct Lifetime
        {
            Provider provider{};
            MonoDomain* domain{};
            MonoImage* probe_image{};   // rung-1 image, closed FIRST at teardown
            MonoImage* modsdk_image{};
            MonoImage* loader_image{};
            MonoImage* mod_images[max_mods]{};
            MonoMethod* loader_main{};
            MonoMethod* loader_tick{};
            MonoMethod* loader_unload{};
            MonoDlFallbackHandler* handler{}; // registered for the RESIDENT lifetime
            void* tick_thread{}; // mono_thread_attach result; v1: never detached
            ~Lifetime() noexcept
            {
                // Teardown order mirrors cap10's Lifetime: images first
                // (assembly retains its own image reference), then
                // mono_jit_cleanup on the owning (init) thread, then provider
                // unregister LAST — cleanup may touch the module cache and
                // the handler/user must outlive it (cap10 MonoOwner comment).
                if (probe_image) mono_image_close(probe_image);
                for (std::uint32_t i = 0; i < max_mods; ++i)
                    if (mod_images[i]) mono_image_close(mod_images[i]);
                if (modsdk_image) mono_image_close(modsdk_image);
                if (loader_image) mono_image_close(loader_image);
                if (domain) mono_jit_cleanup(domain);
                if (handler) mono_dl_fallback_unregister(handler);
            }
        };

        // ---- signature admission, modeled on cap10 signature_matches ----
        bool signature_static(MonoMethod* method, std::uint32_t param_count,
                              std::uint32_t return_type)
        {
            std::uint32_t implementation{};
            if (!(mono_method_get_flags(method, &implementation) & MONO_METHOD_ATTR_STATIC))
                return false;
            MonoMethodSignature* signature = mono_method_signature(method);
            if (!signature || mono_signature_is_instance(signature) ||
                mono_signature_explicit_this(signature) ||
                mono_signature_get_call_conv(signature) != MONO_CALL_DEFAULT ||
                mono_signature_get_param_count(signature) != param_count) return false;
            MonoType* returned = mono_signature_get_return_type(signature);
            if (!returned || mono_type_is_byref(returned) ||
                mono_type_get_type(returned) != return_type) return false;
            void* iterator = nullptr;
            for (std::uint32_t i = 0; i < param_count; ++i)
            {
                MonoType* argument = mono_signature_get_params(signature, &iterator);
                if (!argument || mono_type_is_byref(argument) ||
                    mono_type_get_type(argument) != MONO_TYPE_I4) return false;
            }
            return mono_signature_get_params(signature, &iterator) == nullptr;
        }
        // LoaderMain(int)->int is the cap10 "Run" shape; Tick()/Unload()
        // void() is the cap10 "ThrowForHostBoundaryTest" shape. All admitted
        // with genuine public metadata APIs before any invocation.
        bool admit_loader_signatures(MonoMethod* main_method, MonoMethod* tick_method,
                                     MonoMethod* unload_method)
        {
            return main_method && tick_method && unload_method &&
                   signature_static(main_method, 1, MONO_TYPE_I4) &&
                   signature_static(tick_method, 0, MONO_TYPE_VOID) &&
                   signature_static(unload_method, 0, MONO_TYPE_VOID);
        }

        void insert_abi_dllmaps(MonoImage* image)
        {
            // Per-symbol inserts — the exact proven cap10 form. (A NULL-func
            // catch-all exists in the Mono API; its semantics are unpinned
            // here and deliberately unused. Design section 8, item 2.)
            for (const AbiEntry& entry : abi_table)
                mono_dllmap_insert(image, "__Internal", entry.symbol,
                                   owned_library, entry.symbol);
        }

        MonoImage* open_admitted_image(Lifetime& owner, const ModImageInput& input,
                                       MonoImageOpenStatus& status)
        {
            if (!input.assembly_name || !input.sha8 || !input.bytes || input.size == 0 ||
                std::strlen(input.assembly_name) > 96 || std::strlen(input.sha8) != 8)
                return nullptr;
            // Owned image name carries the admission sha — the cap10 naming
            // discipline that keeps managed images unambiguous.
            char image_name[128];
            std::snprintf(image_name, sizeof(image_name), "modbridge-cap13-%s-%s.dll",
                          input.sha8, input.assembly_name);
            MonoImage* image = mono_image_open_from_data_with_name(
                reinterpret_cast<char*>(const_cast<std::uint8_t*>(input.bytes)),
                input.size, true, &status, false, image_name);
            if (!image || status != MONO_IMAGE_OK) return nullptr;
            MonoAssembly* assembly =
                mono_assembly_load_from_full(image, image_name, &status, false);
            if (!assembly || status != MONO_IMAGE_OK ||
                mono_assembly_get_image(assembly) != image)
            {
                mono_image_close(image);
                return nullptr;
            }
            insert_abi_dllmaps(image);
            (void)owner;
            return image;
        }
    }

    bool bind_engine_surface(const EngineSurface* surface)
    {
        bool expected = false;
        if (!bind_claimed.compare_exchange_strong(expected, true)) return false;
        engine_surface.store(surface);
        return true;
    }

    bool host_start(const ModImageInput* mods, std::uint32_t mod_count,
                    StartReport* report) noexcept
    {
        StartReport out{};
        if (!report) return false;
        bool expected = false;
        if (!start_claimed.compare_exchange_strong(expected, true)) return false;
        if (mods && mod_count > max_mods)
        {
            emit("CAP13_STAGE", 0xF0);
            return false; // claim consumed: oversize admission is fatal to start
        }
        Lifetime* owner_raw = new Lifetime(); // heap: the runtime must OUTLIVE this call
        std::unique_ptr<Lifetime> failure_guard(owner_raw);
        Lifetime& owner = *owner_raw;
        try
        {
            phase.store(Phase::Boot);
            // Same initialization sequence and PREFIX semantics as cap10's
            // MonoOwner (mono appends mono/4.5 and mono/config itself).
            mono_set_dirs(MODBRIDGE_CAP13_CORLIB_DIR, MODBRIDGE_CAP13_CONFIG_DIR);
            mono_config_parse(nullptr);
            mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP);
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP13_STATIC_CORLIB)
            // The generated symbol holds the file-info pointer; do not pass
            // its address (cap10 MonoOwner comment, kept verbatim).
            mono_aot_register_module(static_cast<void**>(mono_aot_module_mscorlib_info));
#endif
            owner.domain = mono_jit_init_version("ModBridge.Cap13", "v4.0.30319");
            if (!owner.domain) { phase.store(Phase::Failed); emit("CAP13_STAGE", 1u); return false; }
            out.runtime_init = true;
            owner.handler = mono_dl_fallback_register(provider_load, provider_symbol,
                                                      provider_close, &owner.provider);
            if (!owner.handler) { phase.store(Phase::Failed); emit("CAP13_STAGE", 2u); return false; }

            // ---- admission rung 1: cap10 Run/throw/recovery in THIS runtime ----
            phase.store(Phase::Probe);
            MonoImageOpenStatus status = MONO_IMAGE_OK;
            owner.probe_image = mono_image_open_from_data_with_name(
                reinterpret_cast<char*>(const_cast<std::uint8_t*>(ProbeImage::bytes)),
                sizeof(ProbeImage::bytes), true, &status,
                false, "modbridge-cap10-probe-c328dca11447b2c41ab1580582b957691.dll");
            if (!owner.probe_image || status != MONO_IMAGE_OK)
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 3u); return false; }
            MonoAssembly* probe_assembly = mono_assembly_load_from_full(
                owner.probe_image, "modbridge-cap10-probe", &status, false);
            if (!probe_assembly || status != MONO_IMAGE_OK ||
                mono_assembly_get_image(probe_assembly) != owner.probe_image)
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 4u); return false; }
            // The same two per-symbol inserts cap10's owner performed.
            mono_dllmap_insert(owner.probe_image, "__Internal",
                "modbridge_cap10_abi_version", owned_library, "modbridge_cap10_abi_version");
            mono_dllmap_insert(owner.probe_image, "__Internal",
                "modbridge_cap10_runtime_probe", owned_library, "modbridge_cap10_runtime_probe");
            MonoClass* probe = mono_class_from_name(owner.probe_image, "ModBridge.Cap10", "Probe");
            MonoMethod* run = probe ?
                mono_class_get_method_from_name(probe, "Run", 1) : nullptr;
            MonoMethod* throw_method = probe ?
                mono_class_get_method_from_name(probe, "ThrowForHostBoundaryTest", 0) : nullptr;
            if (!run || !throw_method ||
                !Cap10Boundary::signatures_admitted(run, throw_method))
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 5u); return false; }
            const std::uint64_t clock = boot_serial.load() ^ 0x5A5A0000;
            const std::int32_t first = static_cast<std::int32_t>((clock & 0x3FFFFFFF) + 1);
            Cap10Boundary::Report calls = Cap10Boundary::invoke_boundaries(
                owner.probe_image, run, throw_method, first, first ^ 0x40000000);
            out.probe_rung_passed = calls.completed;
            emit("CAP13_PROBE", calls.completed ? 1u : 0u);
            if (!calls.completed) { phase.store(Phase::Failed); emit("CAP13_STAGE", 6u); return false; }

            // ---- resident load: ModSdk, ModLoader, then admitted mods ----
            phase.store(Phase::Load);
            // Host-shipped images are hash-pinned; the staged placeholders
            // FAIL CLOSED (size 0) until regenerated from a real compile.
            owner.modsdk_image = open_admitted_image(owner, ModSdkImage::input(), status);
            if (!owner.modsdk_image)
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 7u); return false; }
            owner.loader_image = open_admitted_image(owner, ModLoaderImage::input(), status);
            if (!owner.loader_image)
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 8u); return false; }

            for (std::uint32_t i = 0; i < mod_count; ++i)
            {
                MonoImage* image = open_admitted_image(owner, mods[i], status);
                if (!image)
                {
                    out.mods_failed++;
                    emit("CAP13_MOD_REFUSE", i);
                    continue;
                }
                owner.mod_images[i] = image;
                mod_names[i] = mods[i].assembly_name;
                mod_name_count.store(i + 1);
                mods_admitted.store(i + 1);
                out.mods_admitted++;
            }

            MonoClass* loader_class =
                mono_class_from_name(owner.loader_image, "Mbm.ModLoader", "Loader");
            owner.loader_main = loader_class ?
                mono_class_get_method_from_name(loader_class, "LoaderMain", 1) : nullptr;
            owner.loader_tick = loader_class ?
                mono_class_get_method_from_name(loader_class, "Tick", 0) : nullptr;
            owner.loader_unload = loader_class ?
                mono_class_get_method_from_name(loader_class, "Unload", 0) : nullptr;
            if (!admit_loader_signatures(owner.loader_main, owner.loader_tick,
                                         owner.loader_unload))
            { phase.store(Phase::Failed); emit("CAP13_STAGE", 9u); return false; }

            std::int32_t cookie = static_cast<std::int32_t>(
                ((boot_serial.load() * 31 + mods_admitted.load()) & 0x3FFFFFFF)) + 1;
            void* params[] = {&cookie};
            MonoObject* exception = nullptr;
            // GC-unsafe region pair around every runtime_invoke (cap10 rule);
            // the raw-return slot stays on this admitted native stack.
            void* stack_marker{};
            void* gc_cookie = mono_threads_enter_gc_unsafe_region(&stack_marker);
            MonoObject* volatile result =
                mono_runtime_invoke(owner.loader_main, nullptr, params, &exception);
            const bool threw = exception != nullptr; // checked before any use of result
            mono_threads_exit_gc_unsafe_region(gc_cookie, &stack_marker);
            out.loader_invoked = true;
            if (threw || !result)
            { emit("CAP13_LOADER", 0u); phase.store(Phase::Failed); emit("CAP13_STAGE", 10u); return false; }
            std::int32_t loader_status = -100;
            std::memcpy(&loader_status, mono_object_unbox(result), sizeof(loader_status));
            out.loader_ok = loader_status == 0;
            emit("CAP13_LOADER", static_cast<unsigned>(loader_status) & 0xFFFFu);
            if (!out.loader_ok) { phase.store(Phase::Failed); emit("CAP13_STAGE", 11u); return false; }

            phase.store(Phase::Tick);
            out.completed = true;
            emit("CAP13_STAGE", static_cast<unsigned>(Phase::Tick));
            *report = out;
            failure_guard.release(); // RESIDENT: teardown moves to host_shutdown
            lifetime = &owner;
            return true;
        }
        catch (...)
        {
            phase.store(Phase::Failed);
            emit("CAP13_STAGE", 0xFEu);
            return false; // failure_guard tears the half-built runtime down
        }
    }

    bool host_tick(TickReport* report) noexcept
    {
        TickReport out{};
        Lifetime* owner = lifetime; // single writer/reader per lifecycle contract
        if (!owner || phase.load() != Phase::Tick) return false;
        try
        {
            if (!owner->tick_thread)
            {
                owner->tick_thread = mono_thread_attach(owner->domain);
                out.thread_attached = owner->tick_thread != nullptr;
                if (!owner->tick_thread) return false;
            }
            void* stack_marker{};
            void* gc_cookie = mono_threads_enter_gc_unsafe_region(&stack_marker);
            MonoObject* exception = nullptr;
            (void)mono_runtime_invoke(owner->loader_tick, nullptr, nullptr, &exception);
            mono_threads_exit_gc_unsafe_region(gc_cookie, &stack_marker);
            // Per-MOD exceptions are isolated managed-side (Loader.Tick). A
            // throw HERE means the loader loop itself failed: receipt + stop
            // dispatching, never spin on a broken loader.
            out.loader_threw = exception != nullptr;
            ticks_dispatched.fetch_add(1);
            out.ticks_dispatched = ticks_dispatched.load();
            out.accepted = !out.loader_threw;
            emit("CAP13_TICK", static_cast<unsigned>(ticks_dispatched.load() & 0xFFFFFFu));
            if (out.loader_threw) emit("CAP13_EXC", 1u);
            if (report) *report = out;
            return out.accepted;
        }
        catch (...) { return false; }
    }

    bool host_shutdown(ShutdownReport* report) noexcept
    {
        ShutdownReport out{};
        Lifetime* owner = lifetime;
        if (!owner || phase.load() != Phase::Tick) return false;
        phase.store(Phase::Shutdown);
        try
        {
            if (owner->loader_unload)
            {
                void* stack_marker{};
                void* gc_cookie = mono_threads_enter_gc_unsafe_region(&stack_marker);
                MonoObject* exception = nullptr;
                (void)mono_runtime_invoke(owner->loader_unload, nullptr, nullptr, &exception);
                mono_threads_exit_gc_unsafe_region(gc_cookie, &stack_marker);
                out.unload_invoked = exception == nullptr; // per-mod isolation is managed-side
            }
            // v1 honesty: the tick thread is never detached (single tick
            // thread, process-lifetime); mono_jit_cleanup below runs on the
            // init thread per the cap10 owner precondition.
            for (std::uint32_t i = 0; i < max_mods; ++i) mod_names[i] = nullptr;
            mod_name_count.store(0);
            mods_admitted.store(0);
            lifetime = nullptr;
            delete owner; // images -> mono_jit_cleanup (init thread) -> unregister
            out.images_closed = true;
            out.runtime_cleaned = true;
            out.provider_unregistered = true;
            out.completed = true;
            phase.store(Phase::Quiesced);
            emit("CAP13_DONE", 1u);
            if (report) *report = out;
            return true;
        }
        catch (...)
        {
            phase.store(Phase::Failed);
            emit("CAP13_DONE", 0u);
            return false;
        }
    }
}
