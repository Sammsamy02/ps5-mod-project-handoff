#include "SelectedThreadAuthorityAdapter.hpp"
#include "ThreadProfileCandidate.hpp"
#include <iostream>
static unsigned cache_calls{},getter_calls{};
extern "C" int bridge_code_cache_context(std::uint64_t,const rc::codecache::Metadata** p){++cache_calls;if(p)*p=nullptr;return 0;}
extern "C" std::int32_t scePthreadGetthreadid(void){++getter_calls;return 77;}
int main(){
 RC::Unreal::UnrealInitializer::Config c;unsigned failures=0;
 auto r=NativeThreadIntegration::prepare_selected_source(c,NativeThreadIntegration::kThreadCandidate,NativeThreadIntegration::kThreadCandidate.layout_digest,2000);
 const bool owner=!r.prepared.authority&&r.prepared.refusal==NativeThreadIntegration::Refusal::ImageOwner&&cache_calls==0&&getter_calls==0;
 std::cout<<(owner?"PASS ":"FAIL ")<<"actual Config absent owner refuses before cache/getter\n";failures+=!owner;
 auto wait=std::make_shared<RC::Unreal::UnrealInitializer::StartupWaitControl>();wait->IsCancelled=[](){return false;};c.StartupWait=wait;
 r=NativeThreadIntegration::prepare_selected_source(c,NativeThreadIntegration::kThreadCandidate,NativeThreadIntegration::kThreadCandidate.layout_digest,2000);
 const bool no_life=r.cooperative_cancellation_available&&r.mapping_lifetime_missing&&r.import_authority_missing&&!r.prepared.authority;
 std::cout<<(no_life?"PASS ":"FAIL ")<<"actual StartupWait does not manufacture mapping/import authority\n";failures+=!no_life;
 unsigned consumed=0,published=0;bool refused=false;
 try{NativeThreadIntegration::require_selected_authority(c,NativeThreadIntegration::kThreadCandidate,NativeThreadIntegration::kThreadCandidate.layout_digest,2000);++consumed;++published;}catch(const std::runtime_error& e){refused=std::string(e.what()).find("preparation refused before seed consumption/map publication")!=std::string::npos;}
 const bool gate=refused&&consumed==0&&published==0&&getter_calls==0;
 std::cout<<(gate?"PASS ":"FAIL ")<<"actual adapter throws before caller publication actions\n";failures+=!gate;
 std::cout<<"checks=3 failures="<<failures<<" actual-game-config=0 native-execution=0\n";return failures?1:0;
}
