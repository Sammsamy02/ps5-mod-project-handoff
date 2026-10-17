#include <OwnedMappedScanSource.hpp>
namespace RC::Unreal::Platform {
OwnedMappedScanSource::OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot> owner,OwnedMappedImageFacts facts)
 :OwnedMappedScanSource(std::move(owner),facts,CandidateSelectionPolicy::OriginalOnly) {}
OwnedMappedScanSource::OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot> owner,OwnedMappedImageFacts facts,CandidateSelectionPolicy policy)
 :OwnedMappedScanSource(std::move(owner),facts,policy,{}) {}
OwnedMappedScanSource::OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot> owner,OwnedMappedImageFacts facts,CandidateSelectionPolicy policy,const std::vector<uint8_t>& serialized)
 :owner_(std::move(owner)),facts_(facts),policy_(policy) {
 if(policy!=CandidateSelectionPolicy::OriginalOnly && policy!=CandidateSelectionPolicy::CollectEvidenceOnly && policy!=CandidateSelectionPolicy::SelectUniqueAfterNoMatch)throw std::invalid_argument("Invalid owned mapped candidate selection policy");
 OwnedMappedImageCaller checked(owner_,facts_.format,facts_.text_base,facts_.image_base,facts_.eh_frame,facts_.eh_frame_hdr,facts_.pe_exception_directory);
 if(!serialized.empty())serialized_=OwnedSerializedElfSource::Create(checked,serialized);
}
auto OwnedMappedScanSource::collect_with_evidence(const PsScanConfig& config,const PsScanResults& previous) const noexcept -> OwnedMappedCandidateCollection {
 try {
  OwnedMappedImageCaller image(owner_,facts_.format,facts_.text_base,facts_.image_base,facts_.eh_frame,facts_.eh_frame_hdr,facts_.pe_exception_directory);
  auto out=collect_owned_mapped_candidates(image,config,previous,policy_);
  const auto& original=out.original.slots[10];
  if(serialized_ && policy_==CandidateSelectionPolicy::SelectUniqueAfterNoMatch && config.gameengine_tick && !out.original.image_error && original.status==MappedSlotStatus::NoMatch && original.endpoint_status==3) {
   uint64_t address=0;
   if(serialized_->tick(image,address)==0) {
    out.selected.results.gameengine_tick=reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    out.selected.available_mask|=1u<<10;out.selected.slots[10].status=MappedSlotStatus::Available;out.selected.slots[10].endpoint_status=0;
    out.selected_candidate_mask|=1u<<10;out.provenance[10]=MappedResultProvenance::TickSerializedElfWitness;
    out.selected.required_missing_mask=out.selected.requested_mask&MappedRequiredSlots&~out.selected.available_mask;
   }
  }
  return out;
 }catch(...){OwnedMappedCandidateCollection out{};out.policy=policy_;out.original=unavailable_owned_mapped(config,previous);out.selected=out.original;return out;}
}
auto OwnedMappedScanSource::collect(const PsScanConfig& config,const PsScanResults& previous) const noexcept -> OwnedMappedScanReport {
 return collect_with_evidence(config,previous).selected;
}
auto scan_owned_source(const std::shared_ptr<const OwnedMappedScanSource>& source,
    const std::shared_ptr<const ModuleSnapshot>& expected,PsCtx* ctx,PsScanResults* results) -> bool {
    if(!ctx || !results || !ctx->default_ || !ctx->normal || !ctx->verbose || !ctx->warning || !ctx->error)
        return false;
    // Keep the source and owner alive across callbacks; startup is serialized.
    const auto retained=source;
    const auto retained_expected=expected;
    auto report=unavailable_owned_mapped(ctx->config,*results);
    OwnedMappedCandidateCollection evidence{};
    if(retained && retained_expected && retained->source_owner()==retained_expected) {
        evidence=retained->collect_with_evidence(ctx->config,*results);
        report=evidence.selected;
    }
    *results=report.results;
    // collect's scoped lease has ended before any caller callback runs.
#if defined(__PROSPERO__) || defined(_WIN32)
    PsChar16 success[]=L"Owned mapped resolver pass completed";
    PsChar16 failure[]=L"Owned mapped source or required resolver unavailable";
#else
    PsChar16 success[]=u"Owned mapped resolver pass completed";
    PsChar16 failure[]=u"Owned mapped source or required resolver unavailable";
#endif
    // Each collect_with_evidence scope released the same read lease before
    // these callbacks. Provenance states static candidate selection, not ABI.
#if defined(__PROSPERO__) || defined(_WIN32)
    PsChar16 gu_message[]=L"Selected GUObjectArray diagnostic CFG address candidate; identity and ABI unverified";
    PsChar16 fname_message[]=L"Selected FName SysV argument-call address candidate; identity and ABI unverified";
    PsChar16 gm_message[]=L"Selected GMalloc DeleteFile SysV address candidate; identity and ABI unverified";
    PsChar16 tick_message[]=L"Selected serialized ELF Tick candidate with mapped witness; engine identity and ABI unverified";
#else
    PsChar16 gu_message[]=u"Selected GUObjectArray diagnostic CFG address candidate; identity and ABI unverified";
    PsChar16 fname_message[]=u"Selected FName SysV argument-call address candidate; identity and ABI unverified";
    PsChar16 gm_message[]=u"Selected GMalloc DeleteFile SysV address candidate; identity and ABI unverified";
    PsChar16 tick_message[]=u"Selected serialized ELF Tick candidate with mapped witness; engine identity and ABI unverified";
#endif
    if(evidence.selected_candidate_mask & evidence.selected.requested_mask & 1u)ctx->verbose(gu_message);
    if(evidence.selected_candidate_mask & evidence.selected.requested_mask & 4u)ctx->verbose(fname_message);
    if(evidence.selected_candidate_mask & evidence.selected.requested_mask & 8u)ctx->verbose(gm_message);
    if(evidence.selected_candidate_mask & evidence.selected.requested_mask & (1u<<10))ctx->verbose(tick_message);
    if(report.succeeded())ctx->normal(success);else ctx->error(failure);
    return report.succeeded();
}
}
