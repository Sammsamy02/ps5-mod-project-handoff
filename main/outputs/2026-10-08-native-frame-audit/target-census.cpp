#include <Unreal/FFrame.hpp>
#include <Unreal/Script.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <type_traits>
#include <utility>
using namespace RC::Unreal;
extern "C" { extern const unsigned long long FrameFacts[] = {
sizeof(void*),
alignof(void*),
sizeof(bool),
sizeof(EExprToken),
sizeof(void*),
sizeof(FNativeFuncPtr),
alignof(FNativeFuncPtr),
sizeof(UnrealScriptFunction),
sizeof(std::remove_reference_t<decltype(std::declval<UFunction>().GetFunc())>),
sizeof(void (UObject::*)(FFrame&,void*)),
sizeof(FFrame),
sizeof(FOutputDevice),
alignof(FOutputDevice),
sizeof(FOutParmRec),
alignof(FOutParmRec),
__builtin_offsetof(FOutParmRec,Property),
__builtin_offsetof(FOutParmRec,PropAddr),
__builtin_offsetof(FOutParmRec,NextOutParm),
sizeof(FFrame_50_AndBelow),
alignof(FFrame_50_AndBelow),
__builtin_offsetof(FFrame_50_AndBelow,Node),
__builtin_offsetof(FFrame_50_AndBelow,Object),
__builtin_offsetof(FFrame_50_AndBelow,Code),
__builtin_offsetof(FFrame_50_AndBelow,Locals),
__builtin_offsetof(FFrame_50_AndBelow,MostRecentProperty),
__builtin_offsetof(FFrame_50_AndBelow,MostRecentPropertyAddress),
__builtin_offsetof(FFrame_50_AndBelow,FlowStack),
__builtin_offsetof(FFrame_50_AndBelow,PreviousFrame),
__builtin_offsetof(FFrame_50_AndBelow,OutParms),
__builtin_offsetof(FFrame_50_AndBelow,PropertyChainForCompiledIn),
__builtin_offsetof(FFrame_50_AndBelow,CurrentNativeFunction),
__builtin_offsetof(FFrame_50_AndBelow,bArrayContextFailed),
sizeof(FFrame_51_AndAbove),
alignof(FFrame_51_AndAbove),
__builtin_offsetof(FFrame_51_AndAbove,Node),
__builtin_offsetof(FFrame_51_AndAbove,Object),
__builtin_offsetof(FFrame_51_AndAbove,Code),
__builtin_offsetof(FFrame_51_AndAbove,Locals),
__builtin_offsetof(FFrame_51_AndAbove,MostRecentProperty),
__builtin_offsetof(FFrame_51_AndAbove,MostRecentPropertyAddress),
__builtin_offsetof(FFrame_51_AndAbove,MostRecentPropertyContainer),
__builtin_offsetof(FFrame_51_AndAbove,FlowStack),
__builtin_offsetof(FFrame_51_AndAbove,PreviousFrame),
__builtin_offsetof(FFrame_51_AndAbove,OutParms),
__builtin_offsetof(FFrame_51_AndAbove,PropertyChainForCompiledIn),
__builtin_offsetof(FFrame_51_AndAbove,CurrentNativeFunction),
__builtin_offsetof(FFrame_51_AndAbove,PreviousTrackingFrame),
__builtin_offsetof(FFrame_51_AndAbove,bArrayContextFailed),
__builtin_offsetof(FFrame_51_AndAbove,bAbortingExecution),
sizeof(FlowStackType),
std::is_member_function_pointer_v<FNativeFuncPtr>,
std::is_same_v<UnrealScriptFunction,FNativeFuncPtr>
}; }
extern "C" void CallRawNative(FNativeFuncPtr fn,UObject* ctx,FFrame& frame,void* result) {fn(ctx,frame,result);}
extern "C" void CallMemberNative(void(UObject::*fn)(FFrame&,void*),UObject* ctx,FFrame& frame,void* result) {(ctx->*fn)(frame,result);}
