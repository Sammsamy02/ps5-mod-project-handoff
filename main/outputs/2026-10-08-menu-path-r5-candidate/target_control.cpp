#include <Unreal/CoreUObject/UObject/Class.hpp>
#include "ScriptFunctionStorage.hpp"
#include <type_traits>
#include <utility>
using namespace RC::Unreal;
using Storage=std::remove_reference_t<decltype(std::declval<UFunction&>().GetFunc())>;
static_assert(std::is_pointer_v<Storage>);
static_assert(sizeof(Storage)==sizeof(UnrealScriptFunction));
static_assert(__atomic_always_lock_free(sizeof(Storage), nullptr));
extern "C" const unsigned long long ScriptFuncStorageFacts[]={sizeof(Storage),alignof(Storage),sizeof(UnrealScriptFunction),__atomic_always_lock_free(sizeof(Storage),nullptr),sizeof(UnrealScriptFunctionData)};
extern "C" UnrealScriptFunction AtomicProbeRead(Storage& storage){return ScriptFuncStorage::Load<UnrealScriptFunction>(storage);}
extern "C" void AtomicProbeWrite(Storage& storage,UnrealScriptFunction value){ScriptFuncStorage::Store(storage,value);}
extern "C" bool AtomicProbeReplace(Storage& storage,UnrealScriptFunction expected,UnrealScriptFunction value){return ScriptFuncStorage::Replace(storage,expected,value);}
