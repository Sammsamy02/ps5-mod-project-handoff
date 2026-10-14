#pragma once

#include <Unreal/Core/GenericPlatform/GenericPlatformMisc.hpp>
#define UE_DEBUG_BREAK_IMPL() PLATFORM_BREAK()
namespace RC::Unreal {
struct FProsperoPlatformMisc : FGenericPlatformMisc {
    static FORCEINLINE void MemoryBarrier() {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }
};
using FPlatformMisc = FProsperoPlatformMisc;
}
