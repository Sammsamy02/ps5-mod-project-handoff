#pragma once
#include <Unreal/UFunctionStructs.hpp>
#include <mutex>
#include <cstddef>
namespace RC::Unreal::ThunkDetail {
// Generic bounded prototype configuration, not a game layout fact or final budget.
inline constexpr size_t MaximumCapacity = 256;
enum class Phase { Pre, Post };
enum class Residence { Unpublished, RequiresCodeResidence };
struct TeardownSnapshot { Residence residence; size_t published_slots; size_t active_calls; };
std::recursive_mutex& RegistryMutex();
void ConfigureCapacity(size_t);
UnrealScriptFunctionData& Lookup(UFunction*);
UnrealScriptCallbackData* FindCallback(UFunction*, CallbackId);
CallbackId Register(UFunction*, Phase, const UnrealScriptFunctionCallable&, void*, UObject*);
bool Remove(UFunction*, CallbackId);
TeardownSnapshot BeginTeardown();
void Retire(UFunction*);
}
