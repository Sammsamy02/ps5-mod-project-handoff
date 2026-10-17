#include <OwnedMappedCandidateSelection.hpp>
#include <algorithm>
#include <iterator>
namespace RC::Unreal::Platform {
namespace {
bool writable_slot(const OwnedMappedImageCaller& image,uint64_t address) {
 if(!image.permits_pointer_slot(address))return false;
 const auto& view=image.view();
 for(size_t i=0;i<view.segment_count;++i){const auto& s=view.segments[i];if(address>=s.runtime_address && address-s.runtime_address<=s.size && sizeof(void*)<=s.size-(address-s.runtime_address))return (s.permissions&(PS_MAPPED_READ|PS_MAPPED_WRITE))==(PS_MAPPED_READ|PS_MAPPED_WRITE);}
 return false;
}
CandidateAdmission status(int32_t s) {
 switch(s){case 0:return CandidateAdmission::Admitted;case 1:return CandidateAdmission::InvalidArguments;case 2:return CandidateAdmission::InvalidImage;case 3:return CandidateAdmission::NoMatch;case 4:return CandidateAdmission::Ambiguous;case 5:return CandidateAdmission::IncompleteOrRoleRefused;default:return CandidateAdmission::InvalidEvidence;}
}
}
auto collect_owned_mapped_candidates(const OwnedMappedImageCaller& image,const PsScanConfig& config,const PsScanResults& previous,CandidateSelectionPolicy policy) noexcept -> OwnedMappedCandidateCollection {
 OwnedMappedCandidateCollection out{};out.policy=policy;out.original=collect_owned_mapped(image,config,previous);out.selected=out.original;
 for(size_t slot=0;slot<13;++slot)if(out.original.available_mask&(1u<<slot))out.provenance[slot]=MappedResultProvenance::OriginalResolver;
 if(policy==CandidateSelectionPolicy::OriginalOnly)return out;
 if(policy!=CandidateSelectionPolicy::CollectEvidenceOnly && policy!=CandidateSelectionPolicy::SelectUniqueAfterNoMatch)return out;
 auto eligible=[&](size_t slot){const auto& s=out.original.slots[slot];return !out.original.image_error && s.status==MappedSlotStatus::NoMatch && s.endpoint_status==3;};
 auto select=[&](size_t slot,uint64_t address,MappedResultProvenance provenance,void*& field){
  if(policy!=CandidateSelectionPolicy::SelectUniqueAfterNoMatch)return;
  out.selected.available_mask|=1u<<slot;out.selected.slots[slot].status=MappedSlotStatus::Available;out.selected.slots[slot].endpoint_status=0;out.selected_candidate_mask|=1u<<slot;out.provenance[slot]=provenance;
  if(out.selected.slots[slot].requested)field=reinterpret_cast<void*>(static_cast<uintptr_t>(address));
 };
 auto& gu=out.object_array;gu.eligible=eligible(0);
 if(gu.eligible){gu.endpoint_status=ps_guobject_array_cfg_mapped_v1(&image.view(),&gu.raw);gu.admission=status(gu.endpoint_status);
  if(gu.endpoint_status==0){const auto& r=gu.raw;
   if(r.reserved || r.evidence_flags!=63 || !r.address || !r.allocation_entry || !r.free_entry || !r.allocation_call || !r.free_call || r.allocation_entry==r.free_entry)gu.admission=CandidateAdmission::InvalidEvidence;
   else if(!writable_slot(image,r.address) || !image.permits_code(r.allocation_entry) || !image.permits_code(r.free_entry) || !image.permits_code(r.allocation_call) || !image.permits_code(r.free_call))gu.admission=CandidateAdmission::RejectedRole;
   else select(0,r.address,MappedResultProvenance::ObjectArrayDiagnosticCFG,out.selected.results.g_uobject_array);
  }
 }
 auto& fname=out.fname;fname.eligible=eligible(2);
 if(fname.eligible){fname.endpoint_status=ps_fname_ctor_call_mapped_v1(&image.view(),&fname.raw);fname.admission=status(fname.endpoint_status);
  if(fname.endpoint_status==0){const auto& r=fname.raw;
   if(r.reserved || !r.address || !std::all_of(std::begin(r.anchor_call_counts),std::end(r.anchor_call_counts),[](uint32_t n){return n>0;}))fname.admission=CandidateAdmission::InvalidEvidence;
   else if(!image.permits_code(r.address))fname.admission=CandidateAdmission::RejectedRole;
   else select(2,r.address,MappedResultProvenance::FNameSysVArgumentCalls,out.selected.results.fname_ctor_wchar);
  }
 }
 auto& gm=out.gmalloc;gm.eligible=eligible(3);
 if(gm.eligible){gm.endpoint_status=ps_gmalloc_deletefile_candidate_mapped_v1(&image.view(),&gm.raw);gm.admission=status(gm.endpoint_status);
  if(gm.endpoint_status==0){const auto& r=gm.raw;
   if(r.reserved || !r.address || !r.direct_callee || !r.receiver_call || !r.proof_count || r.vtable_offset>256 || r.vtable_offset%8)gm.admission=CandidateAdmission::InvalidEvidence;
   else if(!writable_slot(image,r.address) || !image.permits_code(r.direct_callee) || !image.permits_code(r.receiver_call))gm.admission=CandidateAdmission::RejectedRole;
   else select(3,r.address,MappedResultProvenance::GMallocDeleteFileSysV,out.selected.results.gmalloc);
  }
 }
 out.selected.required_missing_mask=out.selected.requested_mask&MappedRequiredSlots&~out.selected.available_mask;
 return out;
}
}
