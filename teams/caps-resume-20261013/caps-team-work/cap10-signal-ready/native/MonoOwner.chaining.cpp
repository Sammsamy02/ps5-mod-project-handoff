#if defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION) && defined(__PROSPERO__)
#error "Host validation cannot be used for a target build"
#endif
#if !defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION) && (!defined(__PROSPERO__) || !defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) || MODBRIDGE_CAP10_ADMITTED_MONO_PORT != 1)
#error "Real target Mono interpreter/GC/TLS/EH port and owner admission required"
#endif
#if !defined(MODBRIDGE_CAP10_CORLIB_DIR) || !defined(MODBRIDGE_CAP10_CONFIG_DIR)
#error "Package owner must provide actual matching runtime corlib/config directories"
#endif
#include "MonoOwner.hpp"
#include "ProbeImage.hpp"
#include <mono/mini/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/image.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/mono-config.h>
#include <mono/utils/mono-dl-fallback.h>
#include <atomic>
#include <chrono>
#include <cstring>

extern "C" std::int32_t modbridge_cap10_abi_version() noexcept;
extern "C" std::int32_t modbridge_cap10_runtime_probe(std::int32_t, std::int32_t*) noexcept;
extern "C" void diagnostic_event(const char*, unsigned int);
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_STATIC_CORLIB)
extern "C" void* mono_aot_module_mscorlib_info;
#endif
namespace RC::Cap10Host
{
    namespace
    {
        std::atomic_bool claimed{false};
        // Direct same-image bindings; no arbitrary callback pointer or caller stack retained.
        struct Provider { std::atomic_bool load{}, abi{}, probe{}; };
        constexpr char library_name[] = "modbridge-owned:cap10-c328dca11447b2c41ab1580582b957691";
        constexpr char image_name[] = "modbridge-cap10-probe-c328dca11447b2c41ab1580582b957691.dll";
        void* load(const char* name, int, char**, void* user)
        {
            if (!name || std::strcmp(name, library_name) != 0) return nullptr;
            auto* provider = static_cast<Provider*>(user);
            provider->load = true;
            return provider;
        }
        void* symbol(void* handle, const char* name, char**, void* user)
        {
            if (!name || handle != user) return nullptr;
            auto* provider = static_cast<Provider*>(user);
            if (std::strcmp(name, "modbridge_cap10_abi_version") == 0)
            {
                provider->abi = true;
                return reinterpret_cast<void*>(&modbridge_cap10_abi_version);
            }
            if (std::strcmp(name, "modbridge_cap10_runtime_probe") == 0)
            {
                provider->probe = true;
                return reinterpret_cast<void*>(&modbridge_cap10_runtime_probe);
            }
            return nullptr;
        }
        void* close(void*, void*) { return nullptr; }
        // Only used by this owning thread; Mono must finish all uses before destruction.
        struct Lifetime
        {
            Provider provider{};
            MonoDomain* domain{};
            MonoImage* image{};
            MonoDlFallbackHandler* handler{};
            ~Lifetime() noexcept
            {
                if (image) mono_image_close(image); // Assembly retains its own image reference.
                if (domain) mono_jit_cleanup(domain); // Actual owning init thread; no outer GC scope alive.
                // Cleanup may close cached native modules; handler/user must outlive it.
                if (handler) mono_dl_fallback_unregister(handler);
            }
        };
        void emit(const char* tag, unsigned value) noexcept
        {
            try { diagnostic_event(tag, value); } catch (...) {}
        }
        void emit_result(const Result& r, std::int32_t first, std::int32_t recovery) noexcept
        {
            emit("CAP10_STAGE", static_cast<unsigned>(r.stage));
            emit("CAP10_COOKIE1", static_cast<unsigned>(first));
            emit("CAP10_COOKIE2", static_cast<unsigned>(recovery));
            emit("CAP10_IMAGE", r.exact_image);
            emit("CAP10_RESOLVE", (r.resolver_load ? 1u : 0u) | (r.resolver_abi ? 2u : 0u) | (r.resolver_probe ? 4u : 0u));
            emit("CAP10_RUN", r.calls.first.passed);
            emit("CAP10_THROW", r.calls.deliberate.passed);
            emit("CAP10_RECOVER", r.calls.recovery.passed);
            emit("CAP10_DONE", r.completed);
            emit("CAP10_R1_CALLS", r.calls.first.native.abi_entries | (r.calls.first.native.abi_exits << 8) |
                 (r.calls.first.native.probe_entries << 16) | (r.calls.first.native.probe_exits << 24));
            emit("CAP10_R2_CALLS", r.calls.recovery.native.abi_entries | (r.calls.recovery.native.abi_exits << 8) |
                 (r.calls.recovery.native.probe_entries << 16) | (r.calls.recovery.native.probe_exits << 24));
            emit("CAP10_T_CALLS", r.calls.deliberate.native.abi_entries | (r.calls.deliberate.native.abi_exits << 8) |
                 (r.calls.deliberate.native.probe_entries << 16) | (r.calls.deliberate.native.probe_exits << 24));
        }
    }
    Result run_embedded_probe_once() noexcept
    {
        Result out{};
        // Admission is a real runtime guard, not a fake owner-ready boolean.
        Cap10Observation::WindowToken admission{};
        Cap10Observation::Observation empty{};
        if (!Cap10Observation::begin(Cap10Observation::WindowKind::NoNativeCalls, 0, &admission) ||
            !Cap10Observation::finish(admission, &empty)) return out;
        bool expected = false;
        if (!claimed.compare_exchange_strong(expected, true)) return out;
        if (mono_get_root_domain()) { out.stage = Stage::ExistingRuntime; return out; }
        const auto clock = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::int32_t first_cookie = static_cast<std::int32_t>((clock & 0x3fffffff) + 1);
        const std::int32_t recovery_cookie = first_cookie ^ 0x40000000;
        struct EmitOnExit
        {
            Result& result;
            std::int32_t first, recovery;
            ~EmitOnExit() noexcept { emit_result(result, first, recovery); }
        } emit_on_exit{out, first_cookie, recovery_cookie};
        try
        {
            Lifetime owner; // Does not leave actual callback or its owning native thread.
            // Mono appends mono/4.5 and mono/config to these prefixes itself.
            mono_set_dirs(MODBRIDGE_CAP10_CORLIB_DIR, MODBRIDGE_CAP10_CONFIG_DIR);
            mono_config_parse(nullptr); // Load the relocated runtime's global dllmap.
            mono_set_signal_chaining(true);
            mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP);
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_STATIC_CORLIB)
            // The generated symbol holds the file-info pointer; do not pass its address.
            mono_aot_register_module(static_cast<void**>(mono_aot_module_mscorlib_info));
#endif
            owner.domain = mono_jit_init_version("ModBridge.Cap10", "v4.0.30319");
            if (!owner.domain) { out.stage = Stage::InitFailed; return out; }
            out.initialized = true;
            // mono_jit_init_version creates/attaches the current root-domain thread.
            // No unsafe mono_thread_current probe and no extra attach/detach ownership.
            owner.handler = mono_dl_fallback_register(load, symbol, close, &owner.provider);
            if (!owner.handler) { out.stage = Stage::ResolverFailed; return out; }
            MonoImageOpenStatus status = MONO_IMAGE_OK;
            // A named in-memory image also supplies filename metadata used by
            // the AOT loader. The bytes still come solely from ProbeImage.
            owner.image = mono_image_open_from_data_with_name(
                reinterpret_cast<char*>(const_cast<std::uint8_t*>(ProbeImage::bytes)),
                sizeof(ProbeImage::bytes), true, &status, false, image_name);
            if (!owner.image || status != MONO_IMAGE_OK) { out.stage = Stage::ImageFailed; return out; }
            MonoImage* image = owner.image;
            MonoAssembly* assembly = mono_assembly_load_from_full(image, image_name, &status, false);
            if (!assembly || status != MONO_IMAGE_OK || mono_assembly_get_image(assembly) != image)
            { out.stage = Stage::AssemblyIdentityFailed; return out; }
            out.exact_image = true; // Loaded image is that copied from the compiler-bound byte array.
            mono_dllmap_insert(image, "__Internal", "modbridge_cap10_abi_version", library_name, "modbridge_cap10_abi_version");
            mono_dllmap_insert(image, "__Internal", "modbridge_cap10_runtime_probe", library_name, "modbridge_cap10_runtime_probe");
            MonoClass* probe = mono_class_from_name(image, "ModBridge.Cap10", "Probe");
            MonoMethod* run = probe ? mono_class_get_method_from_name(probe, "Run", 1) : nullptr;
            MonoMethod* throw_method = probe ? mono_class_get_method_from_name(probe, "ThrowForHostBoundaryTest", 0) : nullptr;
            if (!run || !throw_method) { out.stage = Stage::MethodsFailed; return out; }
            out.calls = Cap10Boundary::invoke_boundaries(image, run, throw_method, first_cookie, recovery_cookie);
            out.stage = Stage::Invoked;
            out.resolver_load = owner.provider.load;
            out.resolver_abi = owner.provider.abi;
            out.resolver_probe = owner.provider.probe;
            out.completed = out.calls.completed && out.resolver_load && out.resolver_abi && out.resolver_probe;
            // Actual cleanup runs before the observation result is emitted/callback returns.
        }
        catch (...) { out.completed = false; } // Not a Mono fatal/assert/OS-fault catch.
        return out;
    }
}
