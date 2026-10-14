#include <OwnedMappedScanSource.hpp>
#include <polyhook2/MemAccessor.hpp>
#include "fixture_symbols.hpp"
#include <cstdio>
#include <stdexcept>
using namespace RC::Unreal::Platform;
static size_t checks{};
#define CHECK(x) do {++checks;if(!(x))throw std::runtime_error("owned selection check: " #x);} while(false)
static PsScanConfig all(){return {true,true,true,true,true,true,true,true,true,true,true,true,true};}
static auto facts(MappedFixture& f){return OwnedMappedImageFacts{PS_MAPPED_IMAGE_ELF64,f.address(f.text),0,f.frame_extent(),f.header_extent(),{}};}
static auto source(MappedFixture& f,CandidateSelectionPolicy policy){f.protect();return std::make_shared<OwnedMappedScanSource>(f.context().module_source,facts(f),policy);}
static unsigned verbose_count,normal_count,error_count;static bool callback_bad;static uint64_t callback_owned_address;
static void quiet(PsChar16*){}
static void verbose(PsChar16* text){if(!text||!PLH::OwnedMemory::query(callback_owned_address))callback_bad=true;++verbose_count;}
static void normal(PsChar16* text){if(!text||!PLH::OwnedMemory::query(callback_owned_address))callback_bad=true;++normal_count;}
static void error(PsChar16* text){if(!text||!PLH::OwnedMemory::query(callback_owned_address))callback_bad=true;++error_count;}
static bool original_same(const OwnedMappedScanReport&a,const OwnedMappedScanReport&b){return std::memcmp(&a.results,&b.results,sizeof(a.results))==0 && a.requested_mask==b.requested_mask&&a.available_mask==b.available_mask&&a.required_missing_mask==b.required_missing_mask&&a.image_error==b.image_error&&[&]{for(size_t i=0;i<13;++i)if(a.slots[i].status!=b.slots[i].status||a.slots[i].endpoint_status!=b.slots[i].endpoint_status||a.slots[i].requested!=b.slots[i].requested)return false;return true;}();}
extern "C" int owned_selection_controls(const char* root){
 try {
 auto path=[&](const char*n){return std::string(root)+"/"+n+".elf";};
 for(unsigned allocation=0;allocation<2;++allocation){
  auto p=path("hybrid");MappedFixture f(p.c_str());auto symbols=fixture_symbols(p.c_str());f.protect();
  auto default_source=std::make_shared<OwnedMappedScanSource>(f.context().module_source,facts(f));CHECK(default_source->selection_policy()==CandidateSelectionPolicy::OriginalOnly);
  auto before=default_source->collect_with_evidence(all());CHECK(before.original.required_missing_mask==13 && !before.selected.succeeded());CHECK(original_same(before.original,before.selected));CHECK(before.object_array.endpoint_status==-1 && before.fname.endpoint_status==-1 && before.gmalloc.endpoint_status==-1);
  auto evidence=source(f,CandidateSelectionPolicy::CollectEvidenceOnly)->collect_with_evidence(all());CHECK(original_same(before.original,evidence.original));CHECK(original_same(evidence.original,evidence.selected));CHECK(!evidence.selected_candidate_mask);
  CHECK(evidence.object_array.endpoint_status==0&&evidence.fname.endpoint_status==0&&evidence.gmalloc.endpoint_status==0);
  auto opted=source(f,CandidateSelectionPolicy::SelectUniqueAfterNoMatch);auto selected=opted->collect_with_evidence(all());CHECK(original_same(before.original,selected.original));CHECK(selected.original.required_missing_mask==13 && selected.selected.required_missing_mask==0 && selected.selected.succeeded());CHECK(selected.selected_candidate_mask==13);
  CHECK(selected.selected.results.g_uobject_array==f.data()+symbols.at("candidate_gu"));CHECK(selected.selected.results.fname_ctor_wchar==f.data()+symbols.at("candidate_ctor"));CHECK(selected.selected.results.gmalloc==f.data()+symbols.at("candidate_malloc"));
  CHECK(selected.provenance[0]==MappedResultProvenance::ObjectArrayDiagnosticCFG && selected.provenance[2]==MappedResultProvenance::FNameSysVArgumentCalls && selected.provenance[3]==MappedResultProvenance::GMallocDeleteFileSysV);CHECK(selected.provenance[4]==MappedResultProvenance::OriginalResolver && selected.provenance[6]==MappedResultProvenance::OriginalResolver);
  CHECK(selected.object_array.raw.allocation_entry==f.address(symbols.at("gu_alloc")) && selected.object_array.raw.free_entry==f.address(symbols.at("gu_free")));CHECK(selected.gmalloc.raw.vtable_offset==16 && selected.gmalloc.raw.proof_count>0);for(auto count:selected.fname.raw.anchor_call_counts)CHECK(count>0);
  unsigned marker{};PsScanResults previous{};previous.g_uobject_array=previous.fname_ctor_wchar=previous.gmalloc=&marker;previous.engine_version={3,9};previous.build_configuration=1;previous.stats=2;
  auto disabled=opted->collect_with_evidence({},previous);CHECK(disabled.selected_candidate_mask==13 && disabled.selected.requested_mask==0);CHECK(std::memcmp(&disabled.selected.results,&previous,sizeof(previous))==0);
  callback_owned_address=f.address(0);verbose_count=normal_count=error_count=0;callback_bad=false;PsCtx ctx{quiet,normal,verbose,quiet,error,all()};PsScanResults result{};
  CHECK(scan_owned_source(opted,opted->source_owner(),&ctx,&result));CHECK(verbose_count==3 && normal_count==1 && error_count==0 && !callback_bad);CHECK(result.g_uobject_array==selected.selected.results.g_uobject_array);
  verbose_count=normal_count=error_count=0;CHECK(!scan_owned_source(default_source,default_source->source_owner(),&ctx,&result));CHECK(!verbose_count && !normal_count && error_count==1);
 }
 for(const auto name:{"original-gu","original-gu-ambiguous"}){
  auto p=path(name);MappedFixture f(p.c_str());auto s=source(f,CandidateSelectionPolicy::SelectUniqueAfterNoMatch);auto r=s->collect_with_evidence(all());CHECK(!r.object_array.eligible && r.object_array.endpoint_status==-1 && !r.object_array.raw.address && !(r.selected_candidate_mask&1));
  if(std::string(name)=="original-gu"){CHECK(r.original.slots[0].status==MappedSlotStatus::Available && r.provenance[0]==MappedResultProvenance::OriginalResolver);}
  else{CHECK(r.original.slots[0].status==MappedSlotStatus::Ambiguous && r.original.slots[0].endpoint_status==4 && r.selected.required_missing_mask==1 && !r.selected.succeeded());}
  CHECK(r.selected.slots[0].status==r.original.slots[0].status && r.selected.results.g_uobject_array==r.original.results.g_uobject_array);
 }
 for(const auto name:{"ambiguous-gu","incomplete-gu","ambiguous-fname","ambiguous-malloc","incomplete-malloc-competitor"}){
  auto p=path(name);MappedFixture f(p.c_str());auto r=source(f,CandidateSelectionPolicy::SelectUniqueAfterNoMatch)->collect_with_evidence(all());const size_t slot=std::string(name).find("fname")!=std::string::npos?2:std::string(name).find("malloc")!=std::string::npos?3:0;const auto& admission=slot==0?r.object_array.admission:slot==2?r.fname.admission:r.gmalloc.admission;CHECK(admission==(std::string(name).find("incomplete")!=std::string::npos?CandidateAdmission::IncompleteOrRoleRefused:CandidateAdmission::Ambiguous));CHECK(!(r.selected_candidate_mask&(1u<<slot)) && r.selected.required_missing_mask&(1u<<slot));CHECK(r.original.slots[slot].status==MappedSlotStatus::NoMatch && r.selected.slots[slot].status==MappedSlotStatus::NoMatch);
 }
 {
  auto p=path("hybrid");MappedFixture f(p.c_str());f.protect();auto bad=facts(f);bad.eh_frame.size=1;auto s=std::make_shared<OwnedMappedScanSource>(f.context().module_source,bad,CandidateSelectionPolicy::SelectUniqueAfterNoMatch);auto r=s->collect_with_evidence(all());CHECK(r.original.slots[2].endpoint_status==5 && r.original.slots[2].status==MappedSlotStatus::OperandOrMetadataError);CHECK(!r.fname.eligible && r.fname.endpoint_status==-1 && !(r.selected_candidate_mask&4));CHECK(r.selected.slots[2].status==r.original.slots[2].status && !r.selected.succeeded());
 }
 {
  auto p=path("hybrid");MappedFixture f(p.c_str());for(auto& l:f.loads)l.flags&=~2u;auto r=source(f,CandidateSelectionPolicy::SelectUniqueAfterNoMatch)->collect_with_evidence(all());CHECK(r.object_array.admission==CandidateAdmission::NoMatch && r.object_array.endpoint_status==3);CHECK(r.gmalloc.admission==CandidateAdmission::IncompleteOrRoleRefused && r.gmalloc.endpoint_status==5);CHECK(!(r.selected_candidate_mask&9) && (r.selected.required_missing_mask&9)==9);
 }
 std::printf("genuine17-endpoint candidate/source integration: %zu owned assertions passed; original authority, immutable policy, dual provenance, lease-safe callbacks\n",checks);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"selection control exception: %s\n",e.what());return 91;}catch(...){return 92;}
}
