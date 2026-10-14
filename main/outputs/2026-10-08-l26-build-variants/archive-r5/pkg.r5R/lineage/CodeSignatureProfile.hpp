#pragma once
#include "CodeImageCache.hpp"
#include "FNameAbiProfile.hpp"
#include "SeedTable.h"
// Generated retained Townfall profile inputs; path is configured, not observed.
namespace rc::codeprofile {
namespace seeds=RC::Unreal::UnrealInitializer::SeedTable;
inline constexpr uint8_t kScoPrefix[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x58,0x02,0x00,0x00,0x48,0x8b,0x05,0x4d,0x14};
inline constexpr codecache::Pin kPins[]={
 {1,"fname_ctor_wchar",seeds::kFNameCtorWchar.label,fnameprofile::kPrefix,sizeof(fnameprofile::kPrefix)},
 {2,"static_construct_object_internal",seeds::kStaticConstructObject.label,kScoPrefix,sizeof(kScoPrefix)}
};
inline constexpr codecache::Profile kProfile{
 "/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin",
 seeds::kImageSha256.data(),194241424ull,0xbc602c8ull,seeds::kBaseObserved,kPins,2
};
}
