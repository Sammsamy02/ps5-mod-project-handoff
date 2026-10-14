#include <array>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*,FFrame&,void*) { ++native_calls; }
struct ThrowingCopy {
 static inline bool fail{};
 ThrowingCopy()=default;
 ThrowingCopy(const ThrowingCopy&) { if(fail)throw std::runtime_error("callable copy refused"); }
 void operator()(UnrealScriptFunctionCallableContext&,void*) const {}
};
int basic_case(int argc,char** argv) {
 if(argc!=2)return 2;
 const std::string scenario=argv[1];UFunction a;a.func=original;
 auto& map=Internal::GetHookedFunctionsMap();bool refused=false;
 if(scenario=="query") {
  try{a.GetFunctionHookData();}catch(std::exception&){refused=true;}
 } else if(scenario=="callback_query") {
  auto* data=a.GetFunctionHookCallbackData(123);refused=data==nullptr;
 } else if(scenario=="unregister_absent") {
  refused=!a.UnregisterHook(123);
 } else if(scenario=="empty_pre"||scenario=="empty_post"||scenario=="empty_pre_instance"||scenario=="empty_post_instance") {
  UnrealScriptFunctionCallable empty;
  try {
   if(scenario=="empty_pre")a.RegisterPreHook(empty,nullptr);
   if(scenario=="empty_post")a.RegisterPostHook(empty,nullptr);
   if(scenario=="empty_pre_instance")a.RegisterPreHookForInstance(empty,nullptr,nullptr);
   if(scenario=="empty_post_instance")a.RegisterPostHookForInstance(empty,nullptr,nullptr);
  }catch(std::exception&){refused=true;}
 } else if(scenario=="copy_refusal") {
  UnrealScriptFunctionCallable callable=ThrowingCopy{};ThrowingCopy::fail=true;
  try{a.RegisterPostHook(callable,nullptr);}catch(std::exception&){refused=true;}
 } else if(scenario=="capacity256") {
  std::array<UFunction,257> f;for(auto& x:f)x.func=original;
  int successes=0;for(int i=0;i<256;++i){try{f[i].RegisterPostHook([](auto&,void*){},nullptr);++successes;}catch(std::exception&){break;}}
  try{f[256].RegisterPostHook([](auto&,void*){},nullptr);}catch(std::exception&){refused=true;}
  bool pass=successes==256&&refused&&f[256].func==original&&f[256].stores==0&&map.size()==256;
  if(!pass){std::cerr<<"FAIL capacity256 successes="<<successes<<'\n';return 1;}return 0;
 } else return 2;
 bool pass=refused&&a.func==original&&a.stores==0&&map.empty();
 if(!pass){std::cerr<<"FAIL refusal/query changed Func/map "<<scenario<<" stores="<<a.stores<<" map="<<map.size()<<'\n';return 1;}
 std::cout<<"PASS "<<scenario<<'\n';return 0;
}
