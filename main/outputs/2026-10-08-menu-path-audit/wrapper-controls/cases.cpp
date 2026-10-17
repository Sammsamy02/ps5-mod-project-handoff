#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame& stack, void*) {
 ++native_calls; trace.emplace_back("original"); last_original_frame=&stack; last_original_code=stack.code;
 if (stack.locals) stack.locals[1]=29;
 if (stack.out && stack.out->PropAddr) *stack.out->PropAddr=29;
}
void foreign(UObject*, FFrame&, void*) { ++foreign_calls; }
int failures{};
void require(bool b,const char* name) { if (!b) { ++failures; std::cerr << "FAIL " << name << '\n'; } }
int main(int argc,char** argv) {
 if (argc!=2) return 2;
 const std::string scenario=argv[1];
 UFunction function; function.func=original;
 FFrame frame; UObject object;
 auto& map=Internal::GetHookedFunctionsMap();
 auto install=[&] {
  function.RegisterPreHook([](UnrealScriptFunctionCallableContext&,void*) { trace.emplace_back("pre"); },nullptr);
  function.RegisterPostHook([](UnrealScriptFunctionCallableContext&,void*) { trace.emplace_back("post"); },nullptr);
 };
 uint8 storage[32]{};
 if (scenario=="null_null" || scenario=="null_bogus" || scenario=="unknown_bogus" || scenario=="unknown_foreign") {
  if (scenario=="null_bogus" || scenario=="unknown_bogus") frame.code=reinterpret_cast<uint8*>(1);
  if (scenario=="unknown_bogus" || scenario=="unknown_foreign") frame.current=reinterpret_cast<UFunction*>(0x101);
  UFunction other; other.func=foreign;
  if (scenario=="unknown_foreign") { auto* ptr=&other; std::memcpy(storage,&ptr,sizeof(ptr)); frame.code=storage+8; }
  Internal::UnrealScriptFunctionHook(&object,frame,nullptr);
  require(native_calls==0 && foreign_calls==0 && lookup_calls==0,"unknown frame cannot identify safe original dispatch");
  require(frame.code_reads==0,"unknown frame is refused before reading Code or Code-8");
  require(diagnostic_calls>0,"unknown frame emits refusal diagnostic");
 } else if (scenario=="script_prefix_delivery") {
  install(); auto* ptr=&function; std::memcpy(storage,&ptr,sizeof(ptr)); frame.code=storage+8;
  function.func(&object,frame,nullptr);
  require(native_calls==1 && trace==std::vector<std::string>{"pre","original","post"},"legitimate modeled script prefix must retain original callback delivery");
 } else if (scenario=="known_null_original" || scenario=="known_self_original") {
  map.emplace(std::piecewise_construct,std::forward_as_tuple(&function),std::forward_as_tuple(scenario=="known_self_original"?Internal::UnrealScriptFunctionHook:nullptr));
  frame.current=&function;
  Internal::UnrealScriptFunctionHook(&object,frame,nullptr);
  require(native_calls==0 && diagnostic_calls>0,"missing or recursive saved original is refused");
 } else {
  install(); frame.current=&function;
  if (scenario=="known_code_without_gnatives") frame.code=reinterpret_cast<uint8*>(1);
  FProperty regular; regular.flags=CPF_Parm; regular.offset=0;
  FProperty out; out.flags=CPF_Parm|CPF_OutParm; out.offset=1;
  FProperty returned; returned.flags=CPF_Parm|CPF_ReturnParm; returned.offset=2;
  uint8 out_value=0; out.modeled_out_address=&out_value;
  FOutParmRec out_rec{&out,&out_value,nullptr};
  if (scenario=="known_conversion" || scenario=="known_conversion_legacy") {
   GNatives_Internal=reinterpret_cast<void*>(1); frame.code=storage; frame.out=&out_rec;
   function.properties={&regular,&out,&returned}; below425=scenario=="known_conversion_legacy";
  }
  if (scenario=="pre_exception") {
   map.at(&function).AddPreCallback([](UnrealScriptFunctionCallableContext&,void*) { throw std::runtime_error("pre failure"); });
  }
  function.func(&object,frame,nullptr);
  require(native_calls==1 && trace==std::vector<std::string>{"pre","original","post"},"known saved original executes once between callbacks");
  require(!map.at(&function).bIsMidExecution,"known normal completion clears mid-execution");
  require(lookup_calls==0,"known path does not perform unchecked ExecuteUbergraph lookup");
  if (GNatives_Internal) {
   require(frame.steps==2 && regular.initializations==1 && out.initializations==1 && returned.initializations==0,"actual wrapper converts only input/out parms");
   require(frame.code==storage+1 && out_value==29 && out.copies==1,"actual wrapper advances code and copies out result back");
   require(regular.destructions==1 && out.destructions==1 && frame_allocations==1 && frame_frees==1,"actual wrapper destroys modeled parameters and temporary frame");
   require(last_original_frame!=&frame,"GNatives seam selects converted modeled frame");
  } else {
   require(last_original_frame==&frame && frame_allocations==0 && frame_frees==0,"no GNatives conversion preserves original frame");
  }
 }
 if (failures) return 1;
 std::cout << "PASS " << scenario << '\n';
}
