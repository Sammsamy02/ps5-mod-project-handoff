// runtime_start_bridge.cpp — the module_start -> composed-runtime wiring.
//
// Consumed interfaces (all DEFINED in the composition-4 composed object,
// verified against ue4ss-native-supplier-composition-4/run-3/runtime.map,
// runtime sha256 45dd9901db7d626e63b8ac813bd8bf09f00938b1c5ecfa5f726ff7c9ff075af5):
//   _ZN3lmf27make_sce_live_module_os_opsEPv
//   _ZN3lmf31RetainedLiveModuleFactsSupplier...  (capture/validate/release)
//   _ZN2RC8Platform36TryMakeNativeLoaderFactsFromSupplierENSt3__110shared_ptrIKN3lmf31RetainedLiveModuleFactsSupplierEEERKNS1_4__fs10filesystem4pathERKNS0_20LmfConversionOptionsE
//   _ZN2RC8Platform24MakeNativeProgramContextENS0_17NativeLoaderFactsE
//   _ZN9modbridge6native28StartActivatedRuntimeWithAckEN2RC8Platform20NativeProgramContextENSt3__110unique_ptrI14mfa_activationNS0_17ActivationDeleterEEENS4_17basic_string_viewIcNS4_11char_traitsIcEEEEPKc
//   _ZN9modbridge6native19StopRuntimeWithAckERNS_10unique_ptrINS0_12RuntimeOwnerENS1_11default_deleteIS3_EEEE
//
// Signatures are taken from the ACTUAL composed-unit headers
// (ue4ss-native-tick-file-owner-1/include + worker-query-s2-arm
// integration-patch new-units), which are the same files the composition
// compiled these units from (run-3 commands.json argv template).

#include "runtime_start_bridge.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

#include "activated_mod_source.hpp"
#include "live_module_facts.hpp"
// r2 (controller revision, 2026-10-04): the complete LiveModuleOsOps definition
#include "live_module_os_ops.hpp"
#include "lmf_to_native_loader_facts.hpp"
#include "NativeLoaderContext.hpp"
#include "runtime_ack_supplier.hpp"
#include "runtime_owner.hpp"
#include "ue4ss_backend.hpp"

// Declared only in live_module_os_ops_sce.cpp (no header); mangled symbol
// verified present in the composed runtime.map (see file top).
namespace lmf
{
struct LiveModuleOsOps;
LiveModuleOsOps make_sce_live_module_os_ops(void *context) noexcept;
}

namespace
{
// The one process-lifetime owner. module_start/module_stop are serialized by
// runtime_module_entry.c's entry_guard, so no additional locking is required.
auto owner_slot() -> std::unique_ptr<modbridge::native::RuntimeOwner>&
{
    static std::unique_ptr<modbridge::native::RuntimeOwner> owner;
    return owner;
}

// Bounded printable-ASCII copy: control bytes and non-ASCII become '?', the
// result is always NUL-terminated (detail may be null / capacity 0).
void copy_detail(char *detail, unsigned detail_capacity, const char *text) noexcept
{
    if (detail == nullptr || detail_capacity == 0u)
    {
        return;
    }
    unsigned used = 0u;
    if (text != nullptr)
    {
        while (*text != '\0' && used + 1u < detail_capacity)
        {
            const char c = *text++;
            detail[used++] = (c >= 0x20 && c <= 0x7e) ? c : '?';
        }
    }
    detail[used] = '\0';
}

void copy_detail_or_empty(char *detail, unsigned detail_capacity) noexcept
{
    copy_detail(detail, detail_capacity, "");
}

// Firmware profile per worker-1-live-supplier REPORT "Typed contract for the
// controller": {352, 4, fw=0 (unknown, recorded honestly),
// bridge_flag=false (unproven handle-domain equality), "eboot.bin"}.
auto prospero_live_firmware_profile() noexcept -> lmf::LiveFirmwareProfile
{
    lmf::LiveFirmwareProfile profile{};
    profile.module_info_abi_size = 352u;
    profile.module_segment_capacity = 4u;
    profile.fw_version = 0u;  // unknown: never guessed here
    profile.allow_loader_handle_as_module_handle = false;
    const char *suffix = "eboot.bin";
    unsigned i = 0u;
    for (; suffix[i] != '\0' && i + 1u < sizeof(profile.exe_name_suffix); ++i)
    {
        profile.exe_name_suffix[i] = suffix[i];
    }
    profile.exe_name_suffix[i] = '\0';
    return profile;
}
}  // namespace

extern "C" int modbridge_runtime_stop(void)
{
    auto &slot = owner_slot();
    if (!slot)
    {
        return 0;  // nothing started; nothing to join
    }
    bool stopped = false;
    try
    {
        // Loader control thread (module_stop), never the init thread.
        // Emits at most one 'stopped' ack line, then destroys the owner.
        stopped = modbridge::native::StopRuntimeWithAck(slot);
    }
    catch (...)
    {
        slot.reset();
        return MRS_ERR_STOP_EXN;
    }
    return stopped ? 0 : 1;
}

extern "C" int modbridge_runtime_start_with_activation(const char *module_file_path,
                                                       const char *mod_root,
                                                       const char *title_id,
                                                       struct mfa_activation *activation,
                                                       char *detail,
                                                       unsigned detail_capacity)
{
    auto &slot = owner_slot();
    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
    {
        copy_detail(detail, detail_capacity, "bad arguments");
        return MRS_ERR_INTERNAL;
    }
    if (slot)
    {
        copy_detail(detail, detail_capacity, "runtime already started");
        return MRS_ERR_INTERNAL;
    }

    // Adopt the activation unconditionally: the ActivatedMods deleter
    // (ActivationDeleter -> mfa_release) runs on EVERY exit path below, so
    // ownership always lands in exactly one place and nothing leaks on
    // failure. A null activation (not reachable from the C entry, which only
    // calls after a successful mfa_activate) is consumed and refused by the
    // prepare call's own activation validation.
    modbridge::native::ActivatedMods mods{activation};

    // ---- Stage 1: live module facts -> NativeLoaderFacts (noexcept capture,
    // conversion wrapper records refusals instead of throwing).
    try
    {
        lmf::LiveModuleOsOps ops = lmf::make_sce_live_module_os_ops(nullptr);
        auto supplier = std::make_shared<lmf::RetainedLiveModuleFactsSupplier>(
            ops, prospero_live_firmware_profile());
        supplier->capture();  // VMMAP-first; facts().status records the outcome

        // Owning conversion overload: ModuleInput.lifetime aliases the
        // supplier (and any retained loader reference) into the facts.
        const std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier> retained = supplier;
        RC::Platform::LmfConversionOptions options{};
        // In-game primary identity route (checkpoint zcode_ingame_v1_results):
        // VMMAP span + in-memory ELF base scan. Explicit opt-in, default is
        // refusal; without it a VmScan-identity snapshot never converts.
        options.allow_vm_scan_base_as_module_image_base = true;

        const RC::Platform::LmfConversionResult converted =
            RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
                retained, std::filesystem::path(module_file_path), options);
        if (!converted)
        {
            copy_detail(detail, detail_capacity, converted.error_message);
            return MRS_ERR_CONVERSION;
        }

        // ---- Stage 2: NativeProgramContext build + validation. Throws on
        // refused/invalid facts or missing regular files (both paths must
        // exist NOW, while /data is still reachable in module_start).
        try
        {
            RC::Platform::NativeProgramContext context =
                RC::Platform::MakeNativeProgramContext(std::move(converted.facts));

            // ---- Stage 3: THE composition chain, exactly as
            // INTEGRATION-CHECKLIST STEP 6 orders it:
            //   InstallDefaultEngineBindings() once,
            //   RuntimeAckConfig born from mod_root (S1/S2/S3),
            //   PrepareActivatedUE4SSRuntime (opens the single RuntimeAckSink
            //     and builds the NativeModSource from the activation),
            //   owner->start() (serialized init thread; returns immediately).
            try
            {
                slot = modbridge::native::StartActivatedRuntimeWithAck(
                    std::move(context), std::move(mods), std::string_view{title_id},
                    mod_root);
                if (!slot)
                {
                    copy_detail(detail, detail_capacity,
                                "StartActivatedRuntimeWithAck returned null owner");
                    return MRS_ERR_START;
                }
                copy_detail_or_empty(detail, detail_capacity);
                return MRS_OK;
            }
            catch (const std::exception &error)
            {
                copy_detail(detail, detail_capacity, error.what());
                return MRS_ERR_START;
            }
            catch (...)
            {
                copy_detail(detail, detail_capacity, "unknown exception in prepare/start");
                return MRS_ERR_START;
            }
        }
        catch (const std::exception &error)
        {
            copy_detail(detail, detail_capacity, error.what());
            return MRS_ERR_CONTEXT;
        }
        catch (...)
        {
            copy_detail(detail, detail_capacity, "unknown exception building program context");
            return MRS_ERR_CONTEXT;
        }
    }
    catch (const std::exception &error)
    {
        copy_detail(detail, detail_capacity, error.what());
        return MRS_ERR_CONVERSION;
    }
    catch (...)
    {
        copy_detail(detail, detail_capacity, "unknown exception capturing loader facts");
        return MRS_ERR_CONVERSION;
    }
}
