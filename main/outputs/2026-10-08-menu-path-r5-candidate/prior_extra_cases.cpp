#include <thread>
#include <memory>
#include <array>
void throwing_original(UObject*, FFrame&,void*) { ++native_calls; throw std::runtime_error("original throws"); }
UnrealScriptFunctionData& admit(UFunction& f) { f.RegisterPostHook([](auto&,void*){},nullptr); return f.GetFunctionHookData(); }
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 std::string scenario=argv[1];
 if(scenario=="null_identity"||scenario=="foreign_identity"||scenario=="retired_null_frame"||scenario=="retired_then_registered"||scenario=="reentrant_unregister"||scenario=="known_identity") return basic_case(argc,argv);
 UFunction a,b; a.func=original; b.func=original_b; FFrame frame; UObject context; int result=0;
 bool pass=false;
 auto& map=Internal::GetHookedFunctionsMap();
 if(scenario=="capacity_no_reuse") {
  ThunkDetail::ConfigureCapacity(8);
  std::array<UFunction,9> functions;
  for(auto& f:functions)f.func=original;
  for(int i=0;i<8;++i)admit(functions[i]);
  auto refuse=[&]{try{admit(functions[8]);return false;}catch(std::runtime_error&){return true;}};
  pass=refuse()&&map.size()==8&&functions[8].func==original&&functions[8].stores==0;
  functions[0].UnregisterAllHooks();
  pass=pass&&refuse()&&map.size()==7&&functions[8].stores==0;
 } else if(scenario=="null_original"||scenario=="recursive_original"||scenario=="gnatives_refusal") {
  if(scenario=="null_original")a.func=nullptr;
  if(scenario=="recursive_original")a.func=Internal::UnrealScriptFunctionHook;
  if(scenario=="gnatives_refusal")GNatives_Internal=reinterpret_cast<void*>(1);
  auto before=a.func;
  try{admit(a);}catch(std::runtime_error&){pass=a.func==before&&a.stores==0&&map.empty();}
 } else if(scenario=="other_owned_thunk_refusal") {
  admit(a);b.func=a.func;
  try{admit(b);}catch(std::runtime_error&){pass=b.func==a.func&&b.stores==0&&map.size()==1;}
 } else if(scenario=="intervening_replacement") {
  a.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post-A");},nullptr);
  auto loaded=a.func;a.func=original_b;a.UnregisterAllHooks();loaded(&context,frame,&result);
  pass=a.func==original_b&&a.stores==1&&native_calls==1&&foreign_calls==0&&trace==std::vector<std::string>{"original-A"};
 } else if(scenario=="nested_retirement") {
  auto& data=admit(a);
  bool nested=false;
  data.AddPreCallback([&](UnrealScriptFunctionCallableContext&,void*) {
   trace.emplace_back("pre");
   if(!nested){nested=true;a.func(&context,frame,&result);if(!data.bIsMidExecution)throw std::runtime_error("outer lifetime lost");a.UnregisterAllHooks();}
  });
  data.AddPostCallback([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post");});
  a.func(&context,frame,&result);
  pass=native_calls==2&&!data.bIsMidExecution&&map.empty()&&trace==std::vector<std::string>{"pre","pre","original-A","post","original-A"};
 } else if(scenario=="original_exception_lifetime") {
  a.func=throwing_original;auto& data=admit(a);
  data.AddPostCallback([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post");});
  a.func(&context,frame,&result);
  pass=native_calls==1&&trace.empty()&&!data.bIsMidExecution&&diagnostic_calls>0;
 } else if(scenario=="gnatives_later_original_only") {
  a.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post");},nullptr);
  GNatives_Internal=reinterpret_cast<void*>(1);frame.code=reinterpret_cast<uint8*>(1);a.func(&context,frame,&result);
  pass=native_calls==1&&trace==std::vector<std::string>{"original-A"}&&frame.code_reads==0;
 } else if(scenario=="concurrent_registration_dispatch") {
  auto& data=admit(a);data.AddPostCallback([](UnrealScriptFunctionCallableContext&,void*){});
  auto loaded=a.func;
  std::array<UFunction,6> others;for(auto& f:others)f.func=original_b;
  std::thread register_thread([&]{for(auto& f:others)f.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){},nullptr);});
  for(int i=0;i<2000;++i)loaded(&context,frame,&result);
  register_thread.join();pass=native_calls==2000&&map.size()==7&&!data.bIsMidExecution;
 } else return 2;
 if(!pass){std::cerr<<"FAIL "<<scenario<<'\n';return 1;}std::cout<<"PASS "<<scenario<<'\n';return 0;
}
