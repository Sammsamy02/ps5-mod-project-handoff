#include <memory>
#include "cases.basic.cpp"
void throws_original(UObject*,FFrame&,void*) { ++native_calls; throw std::runtime_error("original failure"); }
struct OwnedCapture { bool* destroyed; ~OwnedCapture() { *destroyed=true; } };
extern "C" int bridge_native_code_residence_required(void) ;
int main(int argc,char** argv) {
 if(argc!=2)return 2;std::string scenario=argv[1];
 if(scenario=="query"||scenario=="callback_query"||scenario=="unregister_absent"||scenario=="empty_pre"||scenario=="empty_post"||scenario=="empty_pre_instance"||scenario=="empty_post_instance"||scenario=="copy_refusal"||scenario=="capacity256")return basic_case(argc,argv);
 UFunction a,b,c;a.func=original;b.func=original;c.func=original;FFrame frame;UObject context;bool pass=false;
 auto noop=[](UnrealScriptFunctionCallableContext&,void*){};
 if(scenario=="configured_limit") {
  ThunkDetail::ConfigureCapacity(2);a.RegisterPostHook(noop,nullptr);b.RegisterPostHook(noop,nullptr);
  try{c.RegisterPostHook(noop,nullptr);}catch(std::exception&){pass=c.func==original&&c.stores==0;}
 } else if(scenario=="invalid_capacity") {
  int refusals=0;for(size_t limit:{size_t(0),size_t(257)})try{ThunkDetail::ConfigureCapacity(limit);}catch(std::exception&){++refusals;}
  pass=refusals==2&&Internal::GetHookedFunctionsMap().empty()&&a.stores==0;
 } else if(scenario=="late_capacity") {
  a.RegisterPostHook(noop,nullptr);
  try{ThunkDetail::ConfigureCapacity(1);}catch(std::exception&){pass=true;}
  b.RegisterPostHook(noop,nullptr);pass=pass&&Internal::GetHookedFunctionsMap().size()==2;
 } else if(scenario=="last_unregister") {
  auto id=a.RegisterPostHook(noop,nullptr);auto loaded=a.func;
  pass=a.UnregisterHook(id)&&a.func==original&&Internal::GetHookedFunctionsMap().empty();
  loaded(&context,frame,nullptr);pass=pass&&native_calls==1;
 } else if(scenario=="teardown_unpublished") {
  auto state=ThunkDetail::BeginTeardown();
  pass=state.residence==ThunkDetail::Residence::Unpublished&&state.published_slots==0&&state.active_calls==0&&a.stores==0;
  try{a.RegisterPostHook(noop,nullptr);pass=false;}catch(std::exception&){}
 } else if(scenario=="teardown_preserves_original") {
  int callbacks=0;a.RegisterPostHook([&](auto&,void*){++callbacks;},nullptr);auto loaded=a.func;
  auto state=ThunkDetail::BeginTeardown();loaded(&context,frame,nullptr);
  pass=state.residence==ThunkDetail::Residence::RequiresCodeResidence&&state.published_slots==1&&native_calls==1&&callbacks==0&&a.stores==1;
 } else if(scenario=="phase_add_post"||scenario=="phase_add_pre") {
  int additions=0;bool added=false;
  a.RegisterPreHook([&](auto&,void*){if(!added){added=true;if(scenario=="phase_add_post")a.RegisterPostHook([&](auto&,void*){++additions;},nullptr);else a.RegisterPreHook([&](auto&,void*){++additions;},nullptr);}},nullptr);
  a.func(&context,frame,nullptr);
  pass=additions==(scenario=="phase_add_post"?1:0);
  a.func(&context,frame,nullptr);pass=pass&&additions==(scenario=="phase_add_post"?2:1);
 } else if(scenario=="residence_unpublished"||scenario=="residence_published") {
  if(!bridge_native_code_residence_required)return 1;
  if(scenario=="residence_published")a.RegisterPostHook(noop,nullptr);
  int expected=scenario=="residence_published"?1:0;
  pass=bridge_native_code_residence_required()==expected&&bridge_native_code_residence_required()==expected;
  try{b.RegisterPostHook(noop,nullptr);pass=false;}catch(std::exception&){}
  if(expected){a.func(&context,frame,nullptr);pass=pass&&native_calls==1;}
 } else if(scenario=="teardown_active_capture") {
  bool destroyed=false,inside=false;auto capture=std::make_shared<OwnedCapture>();capture->destroyed=&destroyed;
  a.RegisterPostHook([capture](auto&,void*){},nullptr);capture.reset();
  a.RegisterPreHook([&](auto&,void*){auto state=ThunkDetail::BeginTeardown();inside=!destroyed&&state.active_calls==1&&state.residence==ThunkDetail::Residence::RequiresCodeResidence;},nullptr);
  a.func(&context,frame,nullptr);auto after=ThunkDetail::BeginTeardown();
  pass=inside&&destroyed&&after.active_calls==0&&after.residence==ThunkDetail::Residence::RequiresCodeResidence&&native_calls==1;
 } else if(scenario=="retired_capture_cleanup") {
  bool destroyed=false;auto capture=std::make_shared<OwnedCapture>();capture->destroyed=&destroyed;
  a.func=throws_original;
  a.RegisterPostHook([capture](auto&,void*){},nullptr);capture.reset();
  a.RegisterPreHook([&](auto&,void*){a.UnregisterAllHooks();},nullptr);
  auto loaded=a.func;loaded(&context,frame,nullptr);
  pass=destroyed&&Internal::GetHookedFunctionsMap().empty()&&native_calls==1;
 } else return 2;
 if(!pass){std::cerr<<"FAIL "<<scenario<<'\n';return 1;}std::cout<<"PASS "<<scenario<<'\n';return 0;
}
