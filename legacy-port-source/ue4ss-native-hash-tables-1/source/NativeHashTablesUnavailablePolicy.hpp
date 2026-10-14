#pragma once
#include <cstdio>
#include <stdexcept>

// This private policy has no engine types, layouts, addresses or synchronization.
// Its own-data host controls verify only refusal behavior.
namespace RC::Unreal::HashTablesNativePolicy
{
    inline bool IsConfigured() { return false; }
    inline bool IsAvailable() { return false; }
    inline bool RunSelfTest() { return false; }
    inline void SetupGetAddress(void* Address)
    {
        if (Address)
        {
            std::fputs("[HashTables] Native hash optimization unavailable; candidate address ignored. Object lookups retain GUObjectArray.\n", stderr);
        }
    }
    [[noreturn]] inline void RefuseSingleton()
    {
        throw std::runtime_error{"Native FUObjectHashTables singleton provider is unavailable."};
    }
    [[noreturn]] inline void RefuseLock()
    {
        throw std::runtime_error{"Native FUObjectHashTables engine lock provider is unavailable."};
    }
    [[noreturn]] inline void RefuseUnlock()
    {
        throw std::runtime_error{"Native FUObjectHashTables engine unlock provider is unavailable."};
    }
}
