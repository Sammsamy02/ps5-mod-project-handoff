#pragma once

#ifdef _WIN32
#include <Unreal/UnrealInitializer.hpp>
#else
#include <File/File.hpp>

// File cache implementation data is not a dependency of reflection/configuration declarations.
// Consumers which create or inspect CacheInfo must include this header and a real File backend.
namespace RC::Unreal::UnrealInitializer
{
    struct CacheInfo
    {
        File::Handle GameExeFile;
        bool bShouldUseCache;
        bool bShouldSerializeCache;
    };
}

#endif
