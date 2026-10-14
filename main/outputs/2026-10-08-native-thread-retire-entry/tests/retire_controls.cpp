#include "NativeThreadAuthorityRetire.h"
#include "ThreadNameRuntime.hpp"
#include "CandidateBindingData.hpp"
#include <OwnedMappedScanSource.hpp>
#include <future>
#include <iostream>
#include <cstring>
using namespace NativeNameThread;
extern int pre_constructor_result;
namespace {std::uintptr_t fixture_base{};std::atomic<unsigned> calls{};}
namespace NativeNameThread {
std::int32_t current_native_id(){++calls;return 77;}
const void* call_native_name(std::uintptr_t entry,std::uint32_t id){
 if(entry!=fixture_base+kAccessor||id!=77)throw 1;
 return reinterpret_cast<const void*>(fixture_base+kGameNameStorage);
}
}
struct Fixture {
 std::shared_ptr<std::array<std::uint8_t,64>> bytes=std::make_shared<std::array<std::uint8_t,64>>();
 std::atomic<bool> stopped{false};RC::Unreal::UnrealInitializer::Config config{};
 rc::codecache::Metadata metadata{};
 Fixture(){
  using namespace RC::Unreal::Platform;fixture_base=reinterpret_cast<std::uintptr_t>(bytes->data());
  ModuleInput input{};input.lifetime=bytes;input.monolithic=true;
  input.images.emplace(RC::ScanTarget::MainExe,ModuleImage{bytes->data(),kImageExtent,{{bytes->data(),bytes->size()}}});
  config.ModuleSource=ModuleSnapshot::Create(std::move(input));
  config.NativeScanSource=std::make_shared<OwnedMappedScanSource>(config.ModuleSource);
  auto cancellation=std::make_shared<RC::Unreal::UnrealInitializer::StartupWaitControl>();
  cancellation->IsCancelled=[this]{return stopped.load();};config.StartupWait=cancellation;
  // Metadata/API are explicit lifecycle fixtures, not fresh native authority.
  metadata.ready=true;metadata.generation=1;metadata.base=fixture_base;metadata.size=kImageSize;
  metadata.extent=kImageExtent;metadata.start_ms=1000;metadata.deadline_ms=61000;
  std::memcpy(metadata.sha256,kImageSha,65);metadata.count=1;metadata.segment_count=2;
  metadata.segments[0]={0,0x4000,0x7479b1c,0x7479b1c,1,0};
  metadata.segments[1]={0x913c000,0x9140000,0x1cc5f2,0x4f7418,6,7};
  auto&e=metadata.entries[0];e.id=kNamePin.id;e.name=kNamePin.name;e.linked=kAccessor;
  e.runtime=fixture_base+kAccessor;e.size=e.collected=kNamePin.size;e.segment=0;
  std::memcpy(e.bytes,kNamePin.bytes,kNamePin.size);
 }
 void prepare(){stopped=false;prepare_runtime(config,&metadata,kCandidateBindingData,kCandidateBindingData.layout_digest,2000,true);}
};
int main(){unsigned checks=0,failures=0;Fixture f;
 auto check=[&](const char* name,auto fn){++checks;bool ok=false;try{ok=fn();}catch(...){}std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;retire_runtime();};
 check("preconstructor refuses with -1",[]{return pre_constructor_result==-1;});
 check("initialized absent owner completes with zero",[]{return bridge_native_thread_authority_retire()==0;});
 check("repeated absent owner calls complete",[]{for(int i=0;i<64;++i)if(bridge_native_thread_authority_retire()!=0)return false;return true;});
 check("same-thread ticket refuses and cancels",[&]{f.prepare();auto ticket=acquire_runtime().ticket;if(!ticket)return false;auto n=calls.load();bool ok=bridge_native_thread_authority_retire()==1&&!acquire_runtime().ticket&&calls.load()==n;ticket.reset();return ok&&bridge_native_thread_authority_retire()==0;});
 check("repeated self refusal retains owner until ticket release",[&]{f.prepare();auto ticket=acquire_runtime().ticket;if(!ticket)return false;bool ok=true;for(int i=0;i<8;++i)ok=ok&&bridge_native_thread_authority_retire()==1;ticket.reset();return ok&&bridge_native_thread_authority_retire()==0&&acquire_runtime().refusal==Refusal::Owner;});
 check("other-thread outstanding ticket delays successful retirement",[&]{
  f.prepare();std::promise<bool> got;std::promise<void> release;auto wait=release.get_future();
  auto worker=std::async(std::launch::async,[&]{auto ticket=acquire_runtime().ticket;got.set_value(bool(ticket));wait.wait();ticket.reset();});
  if(!got.get_future().get()){release.set_value();worker.get();return false;}
  std::promise<void> entered;auto finish=std::async(std::launch::async,[&]{entered.set_value();return bridge_native_thread_authority_retire();});
  entered.get_future().wait();bool waiting=finish.wait_for(std::chrono::milliseconds(30))==std::future_status::timeout;
  release.set_value();worker.get();return waiting&&finish.get()==0&&acquire_runtime().refusal==Refusal::Owner;
 });
 check("owned cancellation still permits explicit retirement",[&]{f.prepare();f.stopped=true;return !acquire_runtime().ticket&&bridge_native_thread_authority_retire()==0;});
 check("new owner after completed retirement then repeated complete",[&]{f.prepare();auto ticket=acquire_runtime().ticket;if(!ticket)return false;ticket.reset();if(bridge_native_thread_authority_retire()!=0)return false;for(int i=0;i<64;++i)if(bridge_native_thread_authority_retire()!=0)return false;return true;});
 std::cout<<"main-checks="<<checks<<" failures="<<failures<<" plus-postconstructor=1 native-execution=0\n"<<std::flush;
 return failures?1:0;
}
