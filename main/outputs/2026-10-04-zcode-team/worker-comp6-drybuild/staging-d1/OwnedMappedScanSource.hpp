#pragma once
#include "OwnedMappedCandidateSelection.hpp"  // comp6-drybuild: quoted -> staging intercept copy
#include <memory>
#include "OwnedSerializedElfSource.hpp"  // comp6-drybuild: quoted -> staging intercept copy
namespace RC::Unreal::Platform {
struct OwnedMappedImageFacts {
    uint32_t format{};
    uint64_t text_base{};
    uint64_t image_base{};
    PsMappedExtent eh_frame{};
    PsMappedExtent eh_frame_hdr{};
    PsMappedExtent pe_exception_directory{};
};
// The loader supplies facts for an already-owned image. Construction and each
// scan require registered readable storage; this class never discovers a process
// or registers external memory. The source retains its owner after each scan.
class OwnedMappedScanSource final {
    std::shared_ptr<const ModuleSnapshot> owner_;
    OwnedMappedImageFacts facts_;
    const CandidateSelectionPolicy policy_;
    std::shared_ptr<const OwnedSerializedElfSource> serialized_;
public:
    OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot>,OwnedMappedImageFacts);
    OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot>,OwnedMappedImageFacts,CandidateSelectionPolicy);
    OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot>,OwnedMappedImageFacts,CandidateSelectionPolicy,const std::vector<uint8_t>& serialized_elf);
    auto selection_policy() const noexcept -> CandidateSelectionPolicy { return policy_; }
    auto collect_with_evidence(const PsScanConfig&,const PsScanResults& previous={}) const noexcept -> OwnedMappedCandidateCollection;
    auto source_owner() const noexcept -> const std::shared_ptr<const ModuleSnapshot>& { return owner_; }
    auto collect(const PsScanConfig&,const PsScanResults& previous={}) const noexcept -> OwnedMappedScanReport;
};
// Explicit source entry for focused owned-image tests and the actual ps_scan
// binding. Callback pointers follow PsScanAbi.h. No callback runs under a lease.
auto scan_owned_source(const std::shared_ptr<const OwnedMappedScanSource>&,
    const std::shared_ptr<const ModuleSnapshot>& expected,PsCtx*,PsScanResults*) -> bool;
}
