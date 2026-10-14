#pragma once
// Explicit HOST MODEL only. The production wrapper/callback implementation is
// extracted verbatim; these engine-facing storage and dispatch seams do not
// establish native FFrame, GNatives, or UFunction ABI compatibility.
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdlib>
#include <alloca.h>
#define STR(x) x
#define RC_UE_API
using uint8 = uint8_t;
using uint64 = uint64_t;
enum class LogLevel { Warning, Error };
inline int diagnostic_calls{};
namespace Output {
template <LogLevel Level = LogLevel::Warning, typename... T>
void send(const char*, T&&...) { ++diagnostic_calls; }
}
inline const char* ensure_str(const char* s) { return s; }
namespace RC::Unreal {
class UObject {};
class FField {};
class FProperty;
class UFunction;
class FFrame;
struct UnrealScriptCallbackData;
struct FOutParmRec { FProperty* Property{}; uint8* PropAddr{}; FOutParmRec* NextOutParm{}; };
using NativeHandler = void(*)(UObject*, FFrame&, void*);
inline void* GNatives_Internal{};
inline std::vector<std::string> trace;
inline int native_calls{}, foreign_calls{}, lookup_calls{}, frame_allocations{}, frame_frees{}, below425{};
inline FFrame* last_original_frame{};
inline uint8* last_original_code{};
inline constexpr unsigned CPF_Parm=1, CPF_ReturnParm=2, CPF_OutParm=4;
enum class EFieldIterationFlags { IncludeDeprecated };
namespace Version { inline bool IsBelow(int, int) { return below425; } }
class FProperty : public FField {
public:
 unsigned flags{}; int offset{}; int initializations{}, destructions{}, copies{};
 uint8* modeled_out_address{};
 bool HasAnyPropertyFlags(unsigned f) { return (flags & f) != 0; }
 template<typename T> T* ContainerPtrToValuePtr(uint8* p) { return reinterpret_cast<T*>(p+offset); }
 void InitializeValue(uint8* p) { ++initializations; *p=0; }
 void DestroyValue(uint8*) { ++destructions; }
 void CopyCompleteValue(uint8* to, uint8* from) { ++copies; *to=*from; }
};
class FFrame {
public:
 UFunction* current{}, *node{}; UObject* object{}; uint8* code{}, *locals{}, *recent{};
 FFrame* previous{}; FField* chain{}; FOutParmRec* out{}; int code_reads{}, steps{};
 UFunction*& CurrentNativeFunction() { return current; }
 UFunction*& Node() { return node; }
 UObject*& Object() { return object; }
 uint8*& Code() { ++code_reads; return code; }
 uint8*& Locals() { return locals; }
 FFrame*& PreviousFrame() { return previous; }
 FField*& PropertyChainForCompiledIn() { return chain; }
 FOutParmRec*& OutParms() { return out; }
 uint8*& MostRecentPropertyAddress() { return recent; }
 void StepCompiledIn(uint8* p, FProperty* property) { ++steps; *p=13; recent=property->modeled_out_address; }
 static FFrame* MallocAndMemsetNewFrame() { ++frame_allocations; return new FFrame; }
};
namespace FMemory { inline void Free(FFrame* p) { ++frame_frees; delete p; } }
class UFunction {
public:
 NativeHandler func{}; int stores{}; std::vector<FProperty*> properties; FField* children{}; int parms_size{16};
 std::string GetFullName() { return "HOST UFunction"; }
 std::string GetPathName() { return "HOST UFunction"; }
 NativeHandler& GetFunc() { return func; }
 NativeHandler GetFuncPtr() { return func; }
 void SetFuncPtr(NativeHandler p) { ++stores; func=p; }
 struct Children { FField* field; FField* Get() { return field; } };
 Children GetChildren() { return {children}; }
 FField* GetChildProperties() { return children; }
 int GetParmsSize() { return parms_size; }
 class UnrealScriptFunctionData& GetFunctionHookData();
 void UnregisterAllHooks();
 bool UnregisterHook(int32_t);
 UnrealScriptCallbackData* GetFunctionHookCallbackData(int32_t);
 int32_t RegisterPreHook(const std::function<void(class UnrealScriptFunctionCallableContext&,void*)>&,void*);
 int32_t RegisterPreHookForInstance(const std::function<void(class UnrealScriptFunctionCallableContext&,void*)>&,void*,UObject*);
 int32_t RegisterPostHookForInstance(const std::function<void(class UnrealScriptFunctionCallableContext&,void*)>&,void*,UObject*);
 int32_t RegisterPostHook(const std::function<void(class UnrealScriptFunctionCallableContext&,void*)>&,void*);
};
template<typename T> struct TFieldRange {
 std::vector<T*>& items;
 TFieldRange(UFunction* f, EFieldIterationFlags) : items(f->properties) {}
 auto begin() { return items.begin(); } auto end() { return items.end(); }
};
namespace UObjectGlobals {
 template<typename T> T StaticFindObject(void*,void*,const char*) { ++lookup_calls; return nullptr; }
}
}
