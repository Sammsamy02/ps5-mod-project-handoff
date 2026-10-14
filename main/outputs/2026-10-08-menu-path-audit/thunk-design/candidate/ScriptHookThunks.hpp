#pragma once
#include <Unreal/UFunctionStructs.hpp>
#include <mutex>
#include <cstddef>
namespace RC::Unreal::ThunkDetail {
// Generic bounded prototype configuration, not a game layout fact or final budget.
inline constexpr size_t Capacity = 8;
std::recursive_mutex& RegistryMutex();
UnrealScriptFunctionData& Install(UFunction*);
void Retire(UFunction*);
}
