// lmf_to_native_loader_facts.cpp
//
// S1 conversion adapter implementation. Integer-only geometry: this TU never
// dereferences any reported module/mapping address (they are treated as
// integers, exactly like worker-1's supplier). The only pointers it forms are
// the ModuleImage base / readable-range pointers derived from those integers,
// which the downstream immutable chain validates without dereferencing at
// construction time either.
#include "lmf_to_native_loader_facts.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace RC::Platform {
namespace {

// Mirrors the lexical half of ValidateNativeProgramContext
// (NativeProgramContext.hpp) so a converted result can never be rejected there
// for a path reason the adapter could have caught. File EXISTENCE is
// deliberately not checked: that is a target-side fact MakeNativeProgramContext
// verifies itself.
[[nodiscard]] auto path_lexically_valid(const std::filesystem::path& p) -> bool {
    if (p.empty() || !p.is_absolute() || !p.has_filename()) return false;
    const auto filename = p.filename();
    if (filename == "." || filename == "..") return false;
    if (p.lexically_normal() != p) return false;
    const auto& bytes = p.native();
    return bytes.find('\0') == std::filesystem::path::string_type::npos;
}

[[nodiscard]] auto status_error(lmf::Status status) -> std::pair<LmfConversionError, const char*> {
    switch (status) {
        case lmf::Status::NotCaptured:
            return {LmfConversionError::SupplierNotCaptured,
                    "supplier has not captured facts (status NotCaptured); refusing conversion"};
        case lmf::Status::AlreadyReleased:
            return {LmfConversionError::SupplierAlreadyReleased,
                    "supplier loader reference was already released; refusing conversion"};
        case lmf::Status::InvalidArgument:
            return {LmfConversionError::SupplierInvalidArgument,
                    "supplier rejected capture arguments (status InvalidArgument); refusing conversion"};
        case lmf::Status::NoImageAnchor:
            return {LmfConversionError::SupplierNoImageAnchor,
                    "no image anchor (status NoImageAnchor); refusing conversion; dlopen(NULL) is never a fallback"};
        case lmf::Status::RetainFailed:
            return {LmfConversionError::SupplierRetainFailed,
                    "loader retain failed (status RetainFailed): zero facts; refusing conversion"};
        case lmf::Status::ModuleInfoNotObtained:
            return {LmfConversionError::SupplierModuleInfoNotObtained,
                    "module info not obtained (status ModuleInfoNotObtained): degraded retention+identity only; "
                    "may not be presented as full facts"};
        case lmf::Status::ModuleInfoRejected:
            return {LmfConversionError::SupplierModuleInfoRejected,
                    "kernel module info violated pinned ABI rules (status ModuleInfoRejected); refusing conversion"};
        case lmf::Status::RetentionProbeFailed:
            return {LmfConversionError::SupplierRetentionProbeFailed,
                    "retention re-probe failed (status RetentionProbeFailed); refusing conversion"};
        default:
            return {LmfConversionError::SupplierStatusNotFullFacts,
                    "supplier status is not Ok; only captured full facts convert"};
    }
}

struct ResolvedExtent {
    uint64_t base = 0;  // inclusive
    uint64_t end = 0;   // exclusive
};

// Clamp observed mappings (integers only) into [extent.base, extent.end),
// sort by start, drop empties, merge overlaps/adjacency. Returns false when
// nothing usable remains.
[[nodiscard]] auto derive_readable_ranges(const lmf::LiveModuleFactsSnapshot& snapshot,
                                          const ResolvedExtent& extent,
                                          std::vector<std::pair<uint64_t, uint64_t>>& out) -> bool {
    out.clear();
    const size_t count = std::min<size_t>(snapshot.anchor_mapping_count, lmf::kMaxAnchorMappings);
    for (size_t i = 0; i < count; ++i) {
        const auto& mapping = snapshot.anchor_mappings[i];
        if (mapping.end <= mapping.start) continue;  // empty or inverted: unusable
        // comp6 Diff A (worker-d2-textbase-design): readable_ranges are LEASED
        // (ReadLease requires R per page, OwnedMemory.cpp:126) and SCANNED;
        // only mappings the kernel reports readable (kve_protection & 1 — the
        // same bit the bridge's PLH pre-registration and the window-anchor
        // coverage test use) may enter. Exec-only text stays out.
        if ((mapping.protection & 1u) == 0) continue;
        const uint64_t start = std::max<uint64_t>(mapping.start, extent.base);
        const uint64_t finish = std::min<uint64_t>(mapping.end, extent.end);
        if (finish <= start) continue;  // does not intersect the module image extent
        out.emplace_back(start, finish);
    }
    if (out.empty()) return false;
    std::sort(out.begin(), out.end());
    std::vector<std::pair<uint64_t, uint64_t>> merged;
    merged.reserve(out.size());
    for (const auto& range : out) {
        if (!merged.empty() && range.first <= merged.back().second) {
            merged.back().second = std::max(merged.back().second, range.second);
        } else {
            merged.push_back(range);
        }
    }
    out.swap(merged);
    return !out.empty();
}

[[nodiscard]] auto convert_impl(const lmf::LiveModuleFactsSnapshot& snapshot,
                                std::shared_ptr<const void> lifetime_token,
                                const std::filesystem::path& module_file_path,
                                const LmfConversionOptions& options,
                                LmfConversionResult& result) -> void {
    result = LmfConversionResult{};

    // Caller-supplied UE4SS module path argument: required, absolute, normalized.
    if (!path_lexically_valid(module_file_path)) {
        result.error = LmfConversionError::ModuleFilePathInvalid;
        result.error_message = "module_file_path must be an absolute normalized file path with a filename and no NUL";
        return;
    }

    // Status gate: ONLY Ok (full facts) converts. IN-GAME UPDATE (checkpoint
    // zcode_ingame_v1_results): one explicit opt-in exception — a
    // ModuleInfoNotObtained snapshot whose identity is VmScan-derived
    // (VMMAP spans + in-memory ELF scan, the observed primary in-game route)
    // may convert behind allow_vm_scan_base_as_module_image_base so the scan
    // result can plant images[MainExe].base. Every other degraded status is
    // still refused verbatim.
    const bool vm_scan_eligible =
        snapshot.identity_source == lmf::IdentitySource::VmScan &&
        snapshot.vm_scan.chosen && snapshot.vm_scan.chosen_base != 0;
    const bool vm_scan_conversion =
        snapshot.status == lmf::Status::ModuleInfoNotObtained &&
        options.allow_vm_scan_base_as_module_image_base && vm_scan_eligible;
    if (snapshot.status != lmf::Status::Ok && !vm_scan_conversion) {
        const auto [code, message] = status_error(snapshot.status);
        result.error = code;
        result.error_message = message;
        return;
    }

    // Authoritative sysctl executable path: empty -> refuse, never fabricate,
    // never fall back to the link-map image name. Resolution B opt-in
    // (allow_configured_roots_for_missing_executable_path): an empty OBSERVED
    // path converts with the field left EMPTY — directory derivations move to
    // the caller-supplied configured roots, enforced by
    // ValidateNativeProgramContext. Never fabricated here.
    const char* observed_path = snapshot.executable_path;
    const bool observed_path_present = observed_path != nullptr && observed_path[0] != '\0';
    std::filesystem::path executable_path{};
    if (observed_path_present) {
        executable_path = std::filesystem::path{std::string{observed_path}};
        if (!path_lexically_valid(executable_path)) {
            result.error = LmfConversionError::GameExecutablePathInvalid;
            result.error_message = "observed executable path is not an absolute normalized file path";
            return;
        }
    } else if (!options.allow_configured_roots_for_missing_executable_path) {
        result.error = LmfConversionError::GameExecutablePathMissing;
        result.error_message = "observed executable path is empty; refusing to fabricate game_executable_path";
        return;
    }

    // Observed mappings are mandatory: ModuleSnapshot requires non-empty ranges.
    if (snapshot.anchor_mapping_count == 0) {
        result.error = LmfConversionError::NoReadableMappings;
        result.error_message = "snapshot has no observed mappings; ModuleSnapshot requires non-empty readable ranges";
        return;
    }

    // Resolve the module image extent (integer geometry only).
    ResolvedExtent extent{};
    if (snapshot.module_info.obtained) {
        extent.base = snapshot.module_info.image_span_begin;
        extent.end = snapshot.module_info.image_span_end;
        result.image_base_provenance = LmfImageBaseProvenance::ModuleInfoSpan;
    } else if (vm_scan_conversion) {
        // IN-GAME UPDATE: the scan's chosen base is a span-validated in-memory
        // ELF header address (provenance "v2_elf_base_scan"); the extent is
        // the PT_LOAD vaddr extent (mode-1 bias: header at span start, vaddrs
        // relative to it — identical to how the scan resolves the dynamic
        // address under mode 1; mode 2 extents are already absolute).
        const uint64_t base = snapshot.vm_scan.chosen_base;
        const uint64_t extent_bytes = snapshot.vm_scan.chosen_extent;
        if (extent_bytes == 0 ||
            extent_bytes > UINT64_MAX - base) {
            result.error = LmfConversionError::InvalidModuleImageSpan;
            result.error_message = "vm-scan image span is empty or unrepresentable";
            return;
        }
        extent.base = base;
        extent.end = base + extent_bytes;
        result.image_base_provenance = LmfImageBaseProvenance::VmScanBase;
    } else {
        // Ok snapshot without module info: weaker-provenance observed base is
        // allowed ONLY behind the explicit opt-in flag (default: refuse).
        if (!options.allow_observed_base_as_module_image_base) {
            result.error = LmfConversionError::ObservedBaseNotPermitted;
            result.error_message = "module info absent; observed-base conversion requires the explicit weaker-provenance opt-in";
            return;
        }
        const uint64_t base = reinterpret_cast<uintptr_t>(snapshot.observed_base);
        if (base == 0) {
            result.error = LmfConversionError::InvalidModuleImageSpan;
            result.error_message = "observed base is null; cannot resolve the module image extent";
            return;
        }
        extent.base = base;
        extent.end = base;
        const size_t count = std::min<size_t>(snapshot.anchor_mapping_count, lmf::kMaxAnchorMappings);
        for (size_t i = 0; i < count; ++i) {
            const auto& mapping = snapshot.anchor_mappings[i];
            if (mapping.start <= base && base < mapping.end && mapping.end > extent.end)
                extent.end = mapping.end;
        }
        result.image_base_provenance = LmfImageBaseProvenance::ObservedBaseOptIn;
    }
    if (extent.end <= extent.base || extent.end - extent.base > static_cast<uint64_t>(PTRDIFF_MAX)) {
        result.error = LmfConversionError::InvalidModuleImageSpan;
        result.error_message = "module image span is empty or unrepresentable";
        return;
    }

    std::vector<std::pair<uint64_t, uint64_t>> clamped;
    if (!derive_readable_ranges(snapshot, extent, clamped)) {
        result.error = LmfConversionError::InvalidReadableMappings;
        result.error_message = "no observed mapping interval intersects the module image extent with valid geometry";
        return;
    }

    // Assemble the immutable loader facts.
    NativeLoaderFacts facts{};
    facts.module_file_path = module_file_path.lexically_normal();
    facts.game_executable_path = executable_path;
    facts.mapped.format = PS_MAPPED_IMAGE_ELF64;
    // text_base NEVER supplied: unresolved by design (see header + REPORT.md).
    facts.mapped.text_base = 0;
    // DEVIATION (actual ABI wins): PsScanAbi.h documents image_base as
    // "PE actual image base; ELF zero" and PsMappedImageCaller.hpp enforces
    // ELF64 image_base == 0, so the resolved base lives in the result record
    // and in ModuleImage geometry instead of facts.mapped.image_base.
    facts.mapped.image_base = 0;
    facts.mapped.eh_frame = PsMappedExtent{0, 0};
    facts.mapped.eh_frame_hdr = PsMappedExtent{0, 0};
    facts.mapped.pe_exception_directory = PsMappedExtent{0, 0};

    facts.modules.lifetime = std::move(lifetime_token);
    facts.modules.monolithic = true;  // one MainExe image: PS5 UE shipping is monolithic
    auto& image = facts.modules.images[ScanTarget::MainExe];
    image.base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(extent.base));
    image.size = static_cast<size_t>(extent.end - extent.base);
    image.readable_ranges.resize(clamped.size());
    for (size_t i = 0; i < clamped.size(); ++i) {
        image.readable_ranges[i] = {};
        image.readable_ranges[i].data = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(clamped[i].first));
        image.readable_ranges[i].size = static_cast<size_t>(clamped[i].second - clamped[i].first);
    }
    // find_export intentionally left unset: ModuleSnapshot::FindExport throws
    // std::logic_error until a real export resolver is registered downstream.

    result.converted = true;
    result.error = LmfConversionError::None;
    result.error_message = "";
    result.facts = std::move(facts);
    result.resolved_image_base = extent.base;
    result.resolved_image_end = extent.end;
}

}  // namespace

auto TryMakeNativeLoaderFactsFromSupplier(const lmf::RetainedLiveModuleFactsSupplier& supplier,
                                          const std::filesystem::path& module_file_path,
                                          const LmfConversionOptions& options) noexcept -> LmfConversionResult {
    LmfConversionResult result{};
    try {
        // Non-owning alias: the empty-owner aliasing constructor yields a
        // non-null shared_ptr that never deletes anything. The supplier MUST
        // outlive the whole scan lease; release() only after it ends.
        std::shared_ptr<const void> lifetime_token{std::shared_ptr<const void>{},
                                                   static_cast<const void*>(&supplier)};
        convert_impl(supplier.facts(), std::move(lifetime_token), module_file_path, options, result);
    } catch (...) {
        result = LmfConversionResult{};
        result.error = LmfConversionError::InternalFailure;
        result.error_message = "unexpected internal failure during conversion";
    }
    return result;
}

auto TryMakeNativeLoaderFactsFromSupplier(std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier> supplier,
                                          const std::filesystem::path& module_file_path,
                                          const LmfConversionOptions& options) noexcept -> LmfConversionResult {
    LmfConversionResult result{};
    if (!supplier) {
        result.error = LmfConversionError::SupplierInvalidArgument;
        result.error_message = "owning conversion requires a non-null supplier shared_ptr";
        return result;
    }
    try {
        // Real aliasing conversion: the token shares the supplier's control
        // block, so the facts genuinely retain the supplier (and its loader
        // reference) until the facts and every snapshot built from them are
        // gone.
        std::shared_ptr<const void> lifetime_token = supplier;
        convert_impl(supplier->facts(), std::move(lifetime_token), module_file_path, options, result);
    } catch (...) {
        result = LmfConversionResult{};
        result.error = LmfConversionError::InternalFailure;
        result.error_message = "unexpected internal failure during conversion";
    }
    return result;
}

auto MakeNativeLoaderFactsFromSupplier(const lmf::RetainedLiveModuleFactsSupplier& supplier,
                                       const std::filesystem::path& module_file_path,
                                       const LmfConversionOptions& options) -> NativeLoaderFacts {
    const auto result = TryMakeNativeLoaderFactsFromSupplier(supplier, module_file_path, options);
    if (!result.converted) throw std::invalid_argument(result.error_message);
    return result.facts;
}

}  // namespace RC::Platform

// comp6-drybuild rehearsal marker (worker-comp6-drybuild): proves THIS staged
// adapter copy (D2 Diff A + Resolution B content) reached the composed object.
namespace RC::Platform { namespace comp6_drybuild {
auto adapter_staging_marker() -> const char* {
    return "comp6-drybuild lmf_to_native_loader_facts.cpp staging override (D2 Diff A + Resolution B) reached codegen";
}
}}
