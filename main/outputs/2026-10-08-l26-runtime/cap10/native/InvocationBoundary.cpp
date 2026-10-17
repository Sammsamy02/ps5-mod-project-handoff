// Same-image production boundary, exercised on matching Mono host only.
#if defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION) && defined(__PROSPERO__)
#error "Host validation cannot be used for a target build"
#endif
#if !defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION) && (!defined(__PROSPERO__) || !defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) || MODBRIDGE_CAP10_ADMITTED_MONO_PORT != 1)
#error "Disabled: genuine same-image Mono port/owner/rooting admission is required"
#endif
#include "InvocationBoundary.hpp"
#include <mono/metadata/appdomain.h>
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/blob.h>
#include <mono/metadata/object.h>
#if defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION)
#include "MonoGcRegionApi.hpp"
#else
#include <mono/utils/mono-threads-api.h>
#endif
#include <cstring>

namespace RC::Cap10Boundary
{
    namespace
    {
        // This instance MUST be a native stack local of the admitted attached thread.
        // The actual port scans live native locals/registers through all Mono calls.
        struct GcRegion
        {
            void* stack_marker{};
            void* cookie{};
            GcRegion() { cookie = mono_threads_enter_gc_unsafe_region(&stack_marker); }
            GcRegion(const GcRegion&) = delete;
            GcRegion& operator=(const GcRegion&) = delete;
            ~GcRegion() noexcept { mono_threads_exit_gc_unsafe_region(cookie, &stack_marker); }
        };
        struct PinnedHandle
        {
            std::uint32_t value{};
            bool owned{};
            explicit PinnedHandle(MonoObject* object)
            {
                value = mono_gchandle_new(object, true);
                owned = true; // Numeric zero is NOT an invented allocation-failure sentinel.
            }
            PinnedHandle(const PinnedHandle&) = delete;
            PinnedHandle& operator=(const PinnedHandle&) = delete;
            ~PinnedHandle() noexcept { if (owned) mono_gchandle_free(value); }
            MonoObject* target() const { return mono_gchandle_get_target(value); }
        };
        struct NativeWindow
        {
            Cap10Observation::WindowToken token{};
            bool open{};
            NativeWindow(Cap10Observation::WindowKind kind, std::int32_t cookie)
                : open(Cap10Observation::begin(kind, cookie, &token)) {}
            NativeWindow(const NativeWindow&) = delete;
            NativeWindow& operator=(const NativeWindow&) = delete;
            ~NativeWindow() noexcept { if (open) Cap10Observation::abort(token); }
            bool finish(Cap10Observation::Observation& observed) noexcept
            {
                if (!open) return false;
                const bool accepted = Cap10Observation::finish(token, &observed);
                open = false; // Matching finish consumes even a rejected observation.
                return accepted;
            }
        };
        bool signature_matches(MonoMethod* method, bool run)
        {
            std::uint32_t implementation{};
            if (!(mono_method_get_flags(method, &implementation) & MONO_METHOD_ATTR_STATIC)) return false;
            MonoMethodSignature* signature = mono_method_signature(method);
            if (!signature || mono_signature_is_instance(signature) || mono_signature_explicit_this(signature) ||
                mono_signature_get_call_conv(signature) != MONO_CALL_DEFAULT ||
                mono_signature_get_param_count(signature) != (run ? 1u : 0u)) return false;
            MonoType* returned = mono_signature_get_return_type(signature);
            if (!returned || mono_type_is_byref(returned) ||
                mono_type_get_type(returned) != (run ? MONO_TYPE_I4 : MONO_TYPE_VOID)) return false;
            void* iterator = nullptr;
            if (run)
            {
                MonoType* argument = mono_signature_get_params(signature, &iterator);
                if (!argument || mono_type_is_byref(argument) || mono_type_get_type(argument) != MONO_TYPE_I4) return false;
            }
            return mono_signature_get_params(signature, &iterator) == nullptr;
        }
        bool invoke_run(MonoMethod* method, std::int32_t cookie, MonoClass* int32_class,
                        RunReport& out)
        {
            void* params[] = {&cookie};
            MonoObject* exception = nullptr;
            NativeWindow window{Cap10Observation::WindowKind::RunImports, cookie};
            if (!window.open) return false;
            // Genuine port must keep this native raw-return slot rooted until handle acquisition.
            MonoObject* volatile result = mono_runtime_invoke(method, nullptr, params, &exception);
            const bool threw = exception != nullptr; // Exception checked before any use of result.
            out.native_accepted = window.finish(out.native);
            out.invoked = true;
            out.had_exception = threw;
            if (threw || !out.native_accepted) return false;
            if (!result) return false;
            PinnedHandle pinned{result};
            MonoObject* target = pinned.target();
            if (!target || mono_object_get_class(target) != int32_class) return false;
            void* storage = mono_object_unbox(target);
            if (!storage) return false;
            std::memcpy(&out.value, storage, sizeof(out.value));
            out.int32_valid = true;
            out.passed = out.value == 0;
            return out.passed;
        }
        bool invoke_throw(MonoMethod* method, MonoClass* expected_exception, ThrowReport& out)
        {
            MonoObject* exception = nullptr;
            NativeWindow window{Cap10Observation::WindowKind::NoNativeCalls, 0};
            if (!window.open) return false;
            // Never inspect the returned object when an exception is reported.
            (void)mono_runtime_invoke(method, nullptr, nullptr, &exception);
            const bool threw = exception != nullptr;
            out.native_accepted = window.finish(out.native);
            out.invoked = true;
            out.had_exception = threw;
            if (!threw || !out.native_accepted) return false;
            // Initial raw-to-handle rooting is a genuine native-stack scanning precondition.
            PinnedHandle pinned{exception};
            MonoObject* target = pinned.target();
            out.exact_exception = target && mono_object_get_class(target) == expected_exception;
            out.passed = out.exact_exception;
            return out.passed;
        }
    }
    bool signatures_admitted(MonoMethod* run, MonoMethod* throw_method)
    {
        return run && throw_method && signature_matches(run, true) && signature_matches(throw_method, false);
    }
    Report invoke_boundaries(MonoImage* image, MonoMethod* run, MonoMethod* throw_method,
                             std::int32_t first_cookie, std::int32_t recovery_cookie) noexcept
    {
        Report out{};
        if (!image || !run || !throw_method || first_cookie == 0 || recovery_cookie == 0 ||
            first_cookie == recovery_cookie) return out;
        try
        {
            GcRegion gc;
            // Valid initialized-runtime/attachment/signature ownership precedes these APIs.
            MonoClass* probe = mono_class_from_name(image, "ModBridge.Cap10", "Probe");
            if (!probe || mono_class_get_image(probe) != image ||
                mono_method_get_class(run) != probe || mono_method_get_class(throw_method) != probe)
                return out;
            const char* run_name = mono_method_get_name(run);
            const char* throw_name = mono_method_get_name(throw_method);
            if (!run_name || !throw_name || std::strcmp(run_name, "Run") != 0 ||
                std::strcmp(throw_name, "ThrowForHostBoundaryTest") != 0) return out;
            // Exact static signatures are checked using genuine pinned public metadata APIs.
            if (!signatures_admitted(run, throw_method)) return out;
            MonoClass* int32_class = mono_get_int32_class();
            MonoImage* corlib = mono_get_corlib();
            if (!int32_class || !corlib) return out;
            MonoClass* expected_exception = mono_class_from_name(corlib, "System", "InvalidOperationException");
            if (!expected_exception) return out;
            out.inputs_associated = true;
            if (!invoke_run(run, first_cookie, int32_class, out.first)) return out;
            if (!invoke_throw(throw_method, expected_exception, out.deliberate)) return out;
            if (!invoke_run(run, recovery_cookie, int32_class, out.recovery)) return out;
            out.completed = true;
        }
        catch (...) { out.unexpected_cpp_exception = true; }
        // C++ cleanup only: Mono fatal/assert/OS faults and async teardown are not caught.
        return out;
    }
}
