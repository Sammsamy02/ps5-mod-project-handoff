// lmf_to_native_loader_facts.hpp
//
// S1 conversion adapter (ZCode team 2026-10-04, PS5 mod-manager / UE4SS
// critical path). Converts a retained live-module facts snapshot from
// worker-1's `lmf::RetainedLiveModuleFactsSupplier` into the immutable loader
// context `RC::Platform::NativeLoaderFacts`
// (.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp).
//
// IN-GAME UPDATE (worker-supplier-ingame-update, checkpoint
// zcode_ingame_v1_results, 2026-10-04): a second, explicitly-opt-in identity
// route is accepted. When the supplier's identity came from the VMMAP spans +
// in-memory ELF base scan (IdentitySource::VmScan, vm_scan.chosen), the
// adapter may plant images[MainExe].base from the scan result behind
// LmfConversionOptions::allow_vm_scan_base_as_module_image_base. This mirrors
// the observed in-game reality (exec-handle raw 0, DT_DEBUG dead, module-info
// domain mismatch; KERN_PROC_VMMAP the only proven source).
//
// Hard conversion contract (from the controller brief + worker-1 REPORT.md
// "Typed contract for the controller", cross-checked against the ACTUAL
// immutable headers; deviations are listed in REPORT.md):
//   * Convert ONLY when supplier.facts().status == Ok (full facts), OR —
//     new, explicit opt-in — a ModuleInfoNotObtained snapshot whose identity
//     is VmScan-derived (see above). RetainFailed / NoImageAnchor and every
//     other status are REFUSED — degraded observations may never be
//     presented as validated engine facts.
//   * game_executable_path <- snapshot.executable_path (sysctl
//     KERN_PROC_PATHNAME, authoritative). Empty -> refuse, never fabricate,
//     never silently fall back to the link-map image name.
//   * module_file_path is the caller-supplied UE4SS module path argument:
//     required, must be an absolute normalized file path without NUL.
//   * mapped.format = PS_MAPPED_IMAGE_ELF64 (this target's actual ABI).
//   * mapped.image_base: the ABI header PsScanAbi.h documents image_base as
//     "PE actual image base; ELF zero", and PsMappedImageCaller.hpp enforces
//     ELF64 image_base == 0 (throws otherwise). DEVIATION from the brief:
//     the adapter therefore stores 0 in mapped.image_base and instead
//     (a) records the resolved base in LmfConversionResult::
//     resolved_image_base with its provenance, and (b) plants it as the
//     geometric base of ModuleInput.images[MainExe], which is where the
//     immutable chain actually consumes it.
//   * The resolved base is snapshot.module_info.image_span_begin when
//     obtained. The VMMAP scan chosen_base may resolve it ONLY behind the
//     explicit opt-in flag allow_vm_scan_base_as_module_image_base for a
//     VmScan-identity snapshot (provenance LmfImageBaseProvenance::
//     VmScanBase); the rtld observed_base remains available ONLY behind the
//     separate, older opt-in (ObservedBaseOptIn).
//   * mapped.text_base is NEVER supplied: it stays at its default 0
//     (unresolved). Identifying the executable segment requires parsing
//     mapped ELF program headers; the SUPPLIER now does span-validated phdr
//     parsing for identity, but the adapter does not relay a text base
//     without a target-verified joined fact (unchanged gate).
//   * readable_ranges are derived (integer arithmetic only, never
//     dereferenced) from the snapshot's observed mappings, clamped and
//     coalesced into the module image extent. No mappings -> refuse
//     (ModuleSnapshot requires non-empty ranges).
//   * ModuleInput.lifetime is a shared_ptr aliasing the supplier itself:
//     the supplier owns the genuine RTLD_NOLOAD loader reference when one
//     exists (VmScan captures own none — degraded, recorded) and MUST
//     OUTLIVE the entire scan lease; call supplier.release() only AFTER
//     the lease ends. The by-reference overloads below create a NON-OWNING
//     alias (the empty-owner aliasing constructor), so this lifetime rule is
//     purely a caller obligation — use the shared_ptr overload for a
//     genuine owning alias.
//   * find_export is left unset (the supplier provides no export resolver):
//     ModuleSnapshot::FindExport then throws std::logic_error("Native module
//     export resolver is not configured") on any downstream call. Documented,
//     not papered over.
//   * input_source stays empty (no Input::PlatformInputSource is available
//     from the supplier); scan_policy keeps NativeLoaderFacts' default
//     CandidateSelectionPolicy::OriginalOnly.
#ifndef LMF_TO_NATIVE_LOADER_FACTS_HPP
#define LMF_TO_NATIVE_LOADER_FACTS_HPP

// comp6-drybuild staging override: quoted include binds NativeLoaderContext.hpp
// from THIS staging dir (same-dir rule) so the intercept chain below reaches the
// D2 patched headers instead of the pinned -I frozen copies.
#include "NativeLoaderContext.hpp"
#ifndef WCD_DRYBUILD_STAGING_PS_HEADER
#error "comp6-drybuild: staging PsMappedImageCaller.hpp (Diff B) did not win include resolution"
#endif
#ifndef WCD_DRYBUILD_STAGING_OWNED_HEADER
#error "comp6-drybuild: staging OwnedMappedImageCaller.hpp (Diff C) did not win include resolution"
#endif

#include <cstdint>
#include <filesystem>
#include <memory>

#include "live_module_facts.hpp"

namespace RC::Platform {

enum class LmfConversionError : uint8_t {
    None = 0,
    // Supplier status gate: only Ok (full facts) — or, behind the explicit
    // vm-scan opt-in, a VmScan-identity ModuleInfoNotObtained — may convert.
    SupplierNotCaptured,
    SupplierAlreadyReleased,
    SupplierInvalidArgument,
    SupplierNoImageAnchor,
    SupplierRetainFailed,
    SupplierModuleInfoNotObtained,
    SupplierModuleInfoRejected,
    SupplierRetentionProbeFailed,
    SupplierStatusNotFullFacts,
    // Refusals about the observed executable path (sysctl, authoritative).
    GameExecutablePathMissing,
    GameExecutablePathInvalid,
    // Refusals about the caller-supplied UE4SS module path argument.
    ModuleFilePathInvalid,
    // Refusals about the image facts.
    ObservedBaseNotPermitted,
    NoReadableMappings,
    InvalidModuleImageSpan,
    InvalidReadableMappings,
    // Defensive: allocation/filesystem failure inside the adapter.
    InternalFailure,
};

enum class LmfImageBaseProvenance : uint8_t {
    None = 0,
    // sceKernelGetModuleInfo checked image span (min segment address).
    ModuleInfoSpan,
    // Explicit opt-in fallback: rtld-reported anchor base, weaker provenance.
    ObservedBaseOptIn,
    // Explicit opt-in (in-game update): VMMAP span + in-memory ELF scan
    // chosen base (checkpoint zcode_ingame_v1_results; the observed primary
    // identity route in-game).
    VmScanBase,
};

struct LmfConversionOptions {
    // Explicit opt-in, default REFUSE: allow the rtld observed_base to resolve
    // the module image base when the Ok snapshot lacks obtained module info.
    // This never converts a supplier whose status itself is not Ok.
    bool allow_observed_base_as_module_image_base = false;
    // Explicit opt-in, default REFUSE (in-game update): allow the VMMAP+
    // in-memory-ELF-scan chosen base (IdentitySource::VmScan) to resolve the
    // module image base of a ModuleInfoNotObtained snapshot. Without it, a
    // VmScan-identity snapshot converts never — the same refusal as before
    // this update.
    bool allow_vm_scan_base_as_module_image_base = false;
    // Explicit opt-in, default REFUSE (Resolution B, 2026-10-04): allow an
    // EMPTY observed executable path to convert, leaving NativeLoaderFacts::
    // game_executable_path empty, when the caller supplies explicit configured
    // roots on the facts. ValidateNativeProgramContext then requires
    // configured_game_directory. Refusal unchanged when false; a NON-empty
    // observed path is unaffected either way.
    bool allow_configured_roots_for_missing_executable_path = false;
};

struct LmfConversionResult {
    LmfConversionError error = LmfConversionError::None;
    const char* error_message = "";  // static storage; stable per error code
    bool converted = false;
    NativeLoaderFacts facts{};  // valid only when converted == true
    // Provenance record for the resolved module image base. This value is NOT
    // duplicated into facts.mapped.image_base (ELF64 requires zero there).
    LmfImageBaseProvenance image_base_provenance = LmfImageBaseProvenance::None;
    uint64_t resolved_image_base = 0;  // inclusive
    uint64_t resolved_image_end = 0;   // exclusive
    explicit operator bool() const noexcept { return converted; }
};

// Primary entry: result/error wrapper. Never throws; on any internal
// std::exception it reports LmfConversionError::InternalFailure. On refusal,
// `facts` is left value-initialized (no partial output).
[[nodiscard]] auto TryMakeNativeLoaderFactsFromSupplier(
    const lmf::RetainedLiveModuleFactsSupplier& supplier,
    const std::filesystem::path& module_file_path,
    const LmfConversionOptions& options = {}) noexcept -> LmfConversionResult;

// Owning variant: the supplier is retained by a real shared_ptr, so the
// ModuleInput.lifetime alias inside the returned facts genuinely keeps the
// supplier (and its loader reference) alive as long as the facts — or any
// ModuleSnapshot/scanner built from them — exist.
[[nodiscard]] auto TryMakeNativeLoaderFactsFromSupplier(
    std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier> supplier,
    const std::filesystem::path& module_file_path,
    const LmfConversionOptions& options = {}) noexcept -> LmfConversionResult;

// Throwing form named by the controller brief: returns the converted facts or
// throws std::invalid_argument with the refusal's error_message.
[[nodiscard]] auto MakeNativeLoaderFactsFromSupplier(
    const lmf::RetainedLiveModuleFactsSupplier& supplier,
    const std::filesystem::path& module_file_path,
    const LmfConversionOptions& options = {}) -> NativeLoaderFacts;

}  // namespace RC::Platform

#endif  // LMF_TO_NATIVE_LOADER_FACTS_HPP
