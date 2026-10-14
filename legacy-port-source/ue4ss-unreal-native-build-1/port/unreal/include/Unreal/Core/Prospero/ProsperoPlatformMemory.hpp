#pragma once

#include <Unreal/Core/GenericPlatform/GenericPlatformMemory.hpp>
namespace RC::Unreal {
// Reuse genuine stateless memory operations. Engine allocation remains FMemory/GMalloc.
using FProsperoPlatformMemory = FGenericPlatformMemory;
using FPlatformMemory = FProsperoPlatformMemory;
}
