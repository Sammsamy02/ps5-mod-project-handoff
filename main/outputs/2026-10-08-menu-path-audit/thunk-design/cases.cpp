#include <iostream>
using namespace RC::Unreal;
void original(UObject*,FFrame& frame,void* result) { ++native_calls; trace.emplace_back("original-A"); last_original_frame=&frame; if(result) *static_cast<int*>(result)=17; }
void original_b(UObject*,FFrame&,void*) { ++foreign_calls; trace.emplace_back("original-B"); }
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 std::string scenario=argv[1];
 UFunction a,b; a.func=original; b.func=original_b;
 UObject context; FFrame frame; int result=0;
 a.RegisterPreHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("pre-A");},nullptr);
 a.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post-A");},nullptr);
 auto loaded=a.func;
 bool retired=false;
 if(scenario=="foreign_identity") {
  b.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("post-B");},nullptr);
  frame.current=&b;
 } else if(scenario=="retired_null_frame") {
  a.UnregisterAllHooks(); retired=true;
 } else if(scenario=="retired_then_registered") {
  a.UnregisterAllHooks(); retired=true;
  a.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*){trace.emplace_back("new-post-A");},nullptr);
  frame.current=&a;
 } else if(scenario=="reentrant_unregister") {
  frame.current=&a;
  Internal::GetHookedFunctionsMap().at(&a).AddPreCallback([&](UnrealScriptFunctionCallableContext&,void*){a.UnregisterAllHooks();});
  retired=true;
 } else if(scenario=="known_identity") {
  frame.current=&a;
 } else if(scenario!="null_identity") return 2;
 loaded(&context,frame,&result);
 bool expected=native_calls==1 && foreign_calls==0 && result==17 && last_original_frame==&frame;
 if(retired) {
  expected=expected && std::find(trace.begin(),trace.end(),"post-A")==trace.end() && std::find(trace.begin(),trace.end(),"new-post-A")==trace.end();
 } else expected=expected && trace==std::vector<std::string>{"pre-A","original-A","post-A"};
 if(!expected) { std::cerr<<"FAIL registration identity/original/retirement "<<scenario<<'\n'; return 1; }
 std::cout<<"PASS "<<scenario<<'\n';
}
