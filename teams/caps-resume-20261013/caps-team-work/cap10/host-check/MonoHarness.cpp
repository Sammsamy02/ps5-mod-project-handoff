// Real matching Mono HOST VALIDATION. No UE4SS readiness or strict owner patch claim.
#include "InvocationBoundary.hpp"
#include "ProbeImage.hpp"
#include <mono/mini/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/image.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/object.h>
#include "MonoGcRegionApi.hpp"
#include <cstdio>
#include <cstdlib>
extern "C" void cap10_host_begin(std::int32_t, std::int32_t);
extern "C" std::int32_t cap10_host_read(std::int32_t);
namespace RC::Cap10Observation
{
    // HOST-ONLY observer: actual C calls in this native host, not engine readiness.
    static bool active;
    static WindowKind kind;
    static std::int32_t cookie;
    static std::uint64_t sequence;
    static int identity;
    bool begin(WindowKind requested, std::int32_t expected, WindowToken* token) noexcept
    {
        if (active || !token) return false;
        active = true; kind = requested; cookie = expected;
        *token = {&identity, ++sequence};
        cap10_host_begin(requested == WindowKind::RunImports ? 1 : 0, expected);
        return true;
    }
    bool finish(WindowToken token, Observation* out) noexcept
    {
        if (!active || !out || token.provider_tls != &identity || token.serial != sequence) return false;
        active = false;
        *out = {};
        out->kind = kind;out->expected_cookie = cookie;
        out->abi_entries = static_cast<std::uint8_t>(cap10_host_read(0));
        out->abi_exits = static_cast<std::uint8_t>(cap10_host_read(5));
        out->probe_entries = static_cast<std::uint8_t>(cap10_host_read(1));
        out->probe_exits = static_cast<std::uint8_t>(cap10_host_read(6));
        out->probe_cookie = cap10_host_read(3);out->probe_reply = cap10_host_read(4);
        out->abi_result = cap10_host_read(7);out->probe_status = cap10_host_read(8);out->probe_reply_valid = cap10_host_read(9) == 1;
        out->accepted = cap10_host_read(2) == 0 && (kind == WindowKind::NoNativeCalls ?
            out->abi_entries == 0 && out->abi_exits == 0 && out->probe_entries == 0 && out->probe_exits == 0 :
            out->abi_entries == 1 && out->abi_exits == 1 && out->probe_entries == 1 && out->probe_exits == 1 &&
            out->abi_result == 1 && out->probe_status == 0 && out->probe_reply_valid && out->probe_cookie == cookie && out->probe_reply == cookie);
        return out->accepted;
    }
    void abort(WindowToken token) noexcept
    {
        if (token.provider_tls == &identity && token.serial == sequence) active = false;
    }
}
static void need(bool ok, const char* why) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1); } }
int main(int argc, char** argv)
{
    need(argc == 5, "matching runtime prefixes and host-only config probe/input");
    mono_set_dirs(argv[1], argv[2]);
    mono_config_parse(nullptr);
    // Stock MDK default execution tests genuine public APIs/GC/helper semantics.
    // INTERP-only target depends on additional AOT trampolines and is NOT asserted.
    MonoDomain* domain = mono_jit_init_version("Cap10.RealMonoHost", "v4.0.30319");
    need(domain != nullptr, "genuine initialized Mono domain");
    // Exercises real System.Native-backed file I/O after relocated config parsing.
    // MONO_PATH is absent, so a bad assembly prefix cannot be silently masked.
    MonoAssembly* config_assembly = mono_domain_assembly_open(domain, argv[3]);
    need(config_assembly != nullptr, "host-only config probe assembly");
    MonoClass* config_class = mono_class_from_name(mono_assembly_get_image(config_assembly), "", "ConfigProbe");
    MonoMethod* read_bytes = config_class ? mono_class_get_method_from_name(config_class, "ReadByteCount", 1) : nullptr;
    need(read_bytes != nullptr, "host-only File.ReadAllBytes method");
    MonoString* file_path = mono_string_new(domain, argv[4]);
    void* config_args[] = {file_path};
    MonoObject* config_exception = nullptr;
    MonoObject* config_result = mono_runtime_invoke(read_bytes, nullptr, config_args, &config_exception);
    need(!config_exception && config_result && mono_object_get_class(config_result) == mono_get_int32_class(),
         "real relocated System.Native-backed file I/O");
    need(*static_cast<std::int32_t*>(mono_object_unbox(config_result)) == sizeof(RC::Cap10Host::ProbeImage::bytes),
         "read actual Probe byte count");
    MonoImageOpenStatus status = MONO_IMAGE_OK;
    MonoImage* image = mono_image_open_from_data_full(
        reinterpret_cast<char*>(const_cast<std::uint8_t*>(RC::Cap10Host::ProbeImage::bytes)),
        sizeof(RC::Cap10Host::ProbeImage::bytes), true, &status, false);
    need(image && status == MONO_IMAGE_OK, "actual compiler-bound image");
    MonoAssembly* assembly = mono_assembly_load_from_full(image, "Probe.dll", &status, false);
    need(assembly && status == MONO_IMAGE_OK && mono_assembly_get_image(assembly) == image, "actual loaded image association");
    // __Internal intentionally uses this HOST executable's exports. Not production resolver policy.
    MonoClass* probe = mono_class_from_name(image, "ModBridge.Cap10", "Probe");
    need(probe != nullptr, "actual Probe class");
    MonoMethod* run = mono_class_get_method_from_name(probe,"Run",1);
    MonoMethod* deliberate = mono_class_get_method_from_name(probe,"ThrowForHostBoundaryTest",0);
    MonoMethod* abi = mono_class_get_method_from_name(probe,"AbiVersion",0);
    need(run && deliberate && abi, "actual resolved methods");
    void* marker{};void* gc = mono_threads_enter_gc_unsafe_region(&marker);
    need(RC::Cap10Boundary::signatures_admitted(run, deliberate), "actual static signatures");
    need(!RC::Cap10Boundary::signatures_admitted(abi, deliberate), "wrong Run arity refused");
    need(!RC::Cap10Boundary::signatures_admitted(run, run), "wrong Throw signature refused");
    // Actual helper nests its public unsafe scope inside this real outer unsafe scope.
    auto result = RC::Cap10Boundary::invoke_boundaries(image,run,deliberate,0x3456789,0x456789a);
    need(result.completed && result.first.int32_valid && result.deliberate.exact_exception && result.recovery.int32_valid,
         "actual Run/throw/recovery/native calls via production boundary helper");
    mono_threads_exit_gc_unsafe_region(gc, &marker);
    mono_image_close(image);
    mono_jit_cleanup(domain);
    std::printf("{\"verdict\":\"REAL_MONO_HELPER_HOST_PASS\",\"assembly_sha256\":\"%s\",\"real_managed_runs\":2,\"real_managed_exceptions\":1,\"signature_negative_cases\":2,\"nested_gc_public_pair\":true,\"strict_resolver_proof\":false,\"interpreter_only_proof\":false,\"townfall_proof\":false}\n",RC::Cap10Host::ProbeImage::sha256);
}
