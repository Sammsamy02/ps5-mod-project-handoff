// lmf_to_native_loader_facts.hpp
//
// S1 conversion adapter (ZCode team 2026-10-04, PS5 mod-manager / UE4SS
// critical path). Converts a retained live-module facts snapshot from
// worker-1's `lmf::RetainedLiveModuleFactsSupplier` into the immutable loader
// context `RC::Platform::NativeLoaderFacts`
// (.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp).
//
// Hard conversion contract (from the controller brief + worker-1 REPORT.md
// "Typed contract for the controller", cross-checked against the ACTUAL
// immutable headers; deviations are listed in REPORT.md):
//   * Convert ONLY when supplier.facts().status == Ok (full facts).
//     RetainFailed / NoImageAnchor / ModuleInfoNotObtained (degraded
//     retention+identity) and every other status are REFUSED — degraded
//     observations may never be presented as validated engine facts.
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
//     obtained. The rtld observed_base may substitute ONLY behind the
//     explicit opt-in flag allow_observed_base_as_module_image_base
//     (default: refuse); such a conversion is surfaced with
//     LmfImageBaseProvenance::ObservedBaseOptIn (weaker provenance).
//   * mapped.text_base is NEVER supplied: it stays at its default 0
//     (unresolved). Identifying the executable segment requires parsing
//     mapped ELF program headers, a dereference worker-1's supplier
//     intentionally never performs. Downstream, OwnedMappedImageCaller
//     requires a contained, registered readable+executable text_base, so
//     MakeNativeProgramContext cannot complete until a target-verified
//     text_base is supplied by a future owner (host-proven; REPORT.md).
//   * readable_ranges are derived (integer arithmetic only, never
//     dereferenced) from the snapshot's observed mappings, clamped and
//     coalesced into the module image extent. No mappings -> refuse
//     (ModuleSnapshot requires non-empty ranges).
//   * ModuleInput.lifetime is a shared_ptr aliasing the supplier itself:
//     the supplier owns the genuine RTLD_NOLOAD loader reference and MUST
//     OUTLIVE the entire scan lease; call supplier.release() only AFTER the
//     lease ends. The by-reference overloads below create a NON-OWNING
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

#include <NativeLoaderContext.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>

#include "live_module_facts.hpp"

namespace RC::Platform {

enum class LmfConversionError : uint8_t {
    None = 0,
    // Supplier status gate: only Ok (full facts) may convert.
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
};

struct LmfConversionOptions {
    // Explicit opt-in, default REFUSE: allow the rtld observed_base to resolve
    // the module image base when the Ok snapshot lacks obtained module info.
    // This never converts a supplier whose status itself is not Ok.
    bool allow_observed_base_as_module_image_base = false;
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
