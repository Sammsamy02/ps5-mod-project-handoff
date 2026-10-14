#include <NativeLoaderContext.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstring>
#include "MappedFixture.hpp"
using namespace RC::Platform;
using namespace RC::Unreal::Platform;
static size_t checks{};
#define CHECK(x) do {++checks;if(!(x))throw std::runtime_error("owner check: " #x);}while(false)
struct Fixture {
 std::vector<uint8_t> file;
 std::shared_ptr<PLH::OwnedMemory::OwnedMapping> memory;
 static constexpr uint64_t lo=0x410000,hi=0x911200;
 struct Load{size_t off,va,n,memory;unsigned flags;};
 std::vector<Load> loads;bool sce{};size_t image_size{};
 explicit Fixture(const char* path):memory(std::make_shared<PLH::OwnedMemory::OwnedMapping>(hi-lo)) {
  std::ifstream in(path,std::ios::binary);file={std::istreambuf_iterator<char>(in),{}};CHECK(file.size()==0x5200);
  sce=file[7]==9;
  auto value=[&](size_t at,size_t n){uint64_t v=0;std::memcpy(&v,file.data()+at,n);return v;};
  for(size_t i=0;i<value(56,2);++i){const auto p=64+i*56;if(value(p,4)!=1||!value(p+4,4))continue;
   loads.push_back({size_t(value(p+8,8)),size_t(value(p+16,8)),size_t(value(p+32,8)),size_t(value(p+40,8)),unsigned(value(p+4,4))});image_size=std::max(image_size,size_t(loads.back().va+loads.back().memory-lo));}
  std::memset(data(),0,memory->size());for(auto& l:loads)std::memcpy(data()+l.va-lo,file.data()+l.off,l.n);
  for(size_t i=0;i<41;++i){uint64_t dst,value;std::memcpy(&dst,file.data()+0x4000+i*24,8);std::memcpy(&value,file.data()+0x4010+i*24,8);value=address(value);std::memcpy(data()+dst-lo,&value,8);}
  for(auto& l:loads){bool ok=false;PLH::MemAccessor{}.mem_protect(address(l.va),l.memory,PLH::R|((l.flags&2)&&!(sce&&l.va==0x810000)?PLH::W:PLH::UNSET)|(l.flags&1?PLH::X:PLH::UNSET),ok);CHECK(ok);}
 }
 auto data() const->uint8_t*{return static_cast<uint8_t*>(memory->data());}
 auto address(uint64_t linked) const->uint64_t{return reinterpret_cast<uintptr_t>(data())+linked-lo;}
 auto facts(const char* path) const->NativeLoaderFacts{
  NativeLoaderFacts f{};f.module_file_path=f.game_executable_path=path;f.modules.lifetime=memory;f.modules.monolithic=true;
  std::vector<RC::ReadableRange> ranges;for(auto& l:loads)ranges.push_back({data()+l.va-lo,l.memory});
  f.modules.images.emplace(RC::ScanTarget::MainExe,ModuleImage{data(),image_size,std::move(ranges)});
  f.mapped={PS_MAPPED_IMAGE_ELF64,address(lo),0,{},{},{}};f.scan_policy=CandidateSelectionPolicy::SelectUniqueAfterNoMatch;return f;
 }
};
static unsigned verbose_count{},normal_count{};static uint64_t callback_address{};
static void quiet(PsChar16*){}
static void verbose(PsChar16*){CHECK(PLH::OwnedMemory::query(callback_address));++verbose_count;}
static void normal(PsChar16*){CHECK(PLH::OwnedMemory::query(callback_address));++normal_count;}
extern "C" int owned_tick_owner_controls(const char* path) {
 try {
  Fixture f(path);// Ordinary writable globals are intentionally different from file bytes.
  f.data()[0x810000-Fixture::lo+0xf00]=0xa5;
  auto ctx=MakeNativeProgramContext(f.facts(path),f.file);PsScanConfig config{};config.gameengine_tick=true;
  auto r=ctx.scanner_source->collect_with_evidence(config);
  CHECK(r.original.slots[10].endpoint_status==3);CHECK(r.original.slots[10].status==MappedSlotStatus::NoMatch);
  CHECK(r.selected.results.gameengine_tick==reinterpret_cast<void*>(f.address(Fixture::lo+0x300)));
  CHECK(r.selected_candidate_mask==(1u<<10));CHECK(r.provenance[10]==MappedResultProvenance::TickSerializedElfWitness);
  auto original=MakeNativeProgramContext(f.facts(path));auto before=original.scanner_source->collect_with_evidence(config);
  CHECK(!before.selected.results.gameengine_tick && !before.selected_candidate_mask);
  auto default_facts=f.facts(path);default_facts.scan_policy=CandidateSelectionPolicy::OriginalOnly;
  auto default_ctx=MakeNativeProgramContext(std::move(default_facts),f.file);CHECK(!default_ctx.scanner_source->collect(config).results.gameengine_tick);
  PsScanResults previous{};previous.gameengine_tick=&checks;
  auto disabled=ctx.scanner_source->collect_with_evidence({},previous);CHECK(disabled.selected.results.gameengine_tick==&checks && !disabled.selected_candidate_mask);
  callback_address=f.address(Fixture::lo);PsCtx scan{quiet,normal,verbose,quiet,quiet,config};PsScanResults result{};
  CHECK(scan_owned_source(ctx.scanner_source,ctx.module_source,&scan,&result));CHECK(result.gameengine_tick==r.selected.results.gameengine_tick && verbose_count==1 && normal_count==1);
  CHECK(!scan_owned_source(ctx.scanner_source,original.module_source,&scan,&result));CHECK(!result.gameengine_tick);
  // Caller bytes may change after admission; the source's private copy remains.
  f.file.assign(16,0);CHECK(ctx.scanner_source->collect(config).results.gameengine_tick==r.selected.results.gameengine_tick);
  // Only positively used mutable proof slots are compared, on every scan.
  uint64_t* slot=reinterpret_cast<uint64_t*>(f.data()+0x810000-Fixture::lo+0x300+12*8);const auto saved=*slot;*slot=0;
  CHECK(!ctx.scanner_source->collect(config).results.gameengine_tick);*slot=saved;
  CHECK(ctx.scanner_source->collect(config).results.gameengine_tick==r.selected.results.gameengine_tick);
  auto* header=f.data()+0x810000-Fixture::lo+0x300-16;*header=1;CHECK(!ctx.scanner_source->collect(config).results.gameengine_tick);*header=0;
  // The optional source cannot replace genuine original success/ambiguity/error.
  const auto controls=std::filesystem::path(path).parent_path()/"controls";
  for(const char* name:{"unique.elf","ambiguous.elf"}){
   const auto control=controls/name;MappedFixture old(control.c_str());old.protect();std::ifstream input(control,std::ios::binary);std::vector<uint8_t> file{std::istreambuf_iterator<char>(input),{}};
   auto snapshot=old.context().module_source;OwnedMappedImageFacts facts{PS_MAPPED_IMAGE_ELF64,old.address(old.text),0,old.frame_extent(),old.header_extent(),{}};
   OwnedMappedScanSource source(snapshot,facts,CandidateSelectionPolicy::SelectUniqueAfterNoMatch,file);auto record=source.collect_with_evidence(config);
   CHECK(record.original.slots[10].endpoint_status==(std::strcmp(name,"unique.elf")==0?0:4));CHECK(!record.selected_candidate_mask);CHECK(record.selected.results.gameengine_tick==record.original.results.gameengine_tick);
   facts.eh_frame.size=1;OwnedMappedScanSource malformed(snapshot,facts,CandidateSelectionPolicy::SelectUniqueAfterNoMatch,file);auto failed=malformed.collect_with_evidence(config);CHECK(failed.original.slots[10].endpoint_status==5);CHECK(!failed.selected_candidate_mask && !failed.selected.results.gameengine_tick);
  }
  {
   const auto sce_path=std::filesystem::path(path).parent_path()/"owned-tick-sce.elf";Fixture sce(sce_path.c_str());
   // Loader-managed imported slot and unrelated global/BSS differ from disk.
   uint64_t imported=0x1234567812345678;std::memcpy(sce.data()+0x850008-Fixture::lo,&imported,8);sce.data()[0x850500-Fixture::lo]=0xa5;
   auto context=MakeNativeProgramContext(sce.facts(sce_path.c_str()),sce.file);auto got=context.scanner_source->collect_with_evidence(config);
   CHECK(got.selected.results.gameengine_tick==reinterpret_cast<void*>(sce.address(Fixture::lo+0x300)));CHECK(got.provenance[10]==MappedResultProvenance::TickSerializedElfWitness);
  }
  for(unsigned mode=0;mode<4;++mode){
   Fixture owned(path);auto supplied=owned.facts(path);auto file=owned.file;
   if(mode==0)file[0]=0;
   if(mode==1)file[0x700]^=1; // File's witnessed Tick instruction differs from actual RX.
   if(mode==2)supplied.modules.images.at(RC::ScanTarget::MainExe).size-=1;
   if(mode==3){bool ok=false;PLH::MemAccessor{}.mem_protect(owned.address(Fixture::lo),0x1400,PLH::R|PLH::W,ok);CHECK(ok);}
   bool refused=false;try{(void)MakeNativeProgramContext(std::move(supplied),file);}catch(const std::invalid_argument&){refused=true;}CHECK(refused);
  }
  // A changed actual immutable instruction invalidates an already-bound source.
  {
   Fixture owned(path);auto context=MakeNativeProgramContext(owned.facts(path),owned.file);bool ok=false;
   PLH::MemAccessor{}.mem_protect(owned.address(Fixture::lo),0x1400,PLH::R|PLH::W,ok);CHECK(ok);owned.data()[0x300]^=1;
   PLH::MemAccessor{}.mem_protect(owned.address(Fixture::lo),0x1400,PLH::R|PLH::X,ok);CHECK(ok);
   CHECK(!context.scanner_source->collect(config).results.gameengine_tick);
  }
  std::weak_ptr<PLH::OwnedMemory::OwnedMapping> weak;
  std::shared_ptr<const OwnedMappedScanSource> kept;
  {Fixture owned(path);weak=owned.memory;auto context=MakeNativeProgramContext(owned.facts(path),owned.file);kept=context.scanner_source;}
  CHECK(!weak.expired());CHECK(kept->collect(config).results.gameengine_tick);kept.reset();CHECK(weak.expired());
  std::printf("owned Tick source: %zu assertions passed\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"after %zu assertions: %s\n",checks,e.what());return 91;}
}
