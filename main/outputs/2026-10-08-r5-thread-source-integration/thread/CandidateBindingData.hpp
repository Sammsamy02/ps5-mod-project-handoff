#pragma once
#include "NativeThreadNameAdapter.hpp"
#include "RuntimeBuildProfile.hpp"
namespace NativeNameThread {
inline constexpr Data kCandidateBindingData{kImageSha,rc::runtimeprofile::kDigest,
 kImageSize,kImageExtent,kAccessor,kGameNameStorage,&kNamePin};
}
