#pragma once
#include "ThreadNameProfile.hpp"
#include "CodeSignatureProfile.hpp"
namespace NativeNameThread {
// Existing FName/SCO pins are preserved; cache capacity4 already permits3.
inline constexpr rc::codecache::Pin kExtendedPins[]={rc::codeprofile::kPins[0],rc::codeprofile::kPins[1],kNamePin};
inline constexpr rc::codecache::Profile kExtendedCodeProfile{
 rc::codeprofile::kProfile.path,rc::codeprofile::kProfile.sha256,
 rc::codeprofile::kProfile.size,rc::codeprofile::kProfile.extent,
 rc::codeprofile::kProfile.base,kExtendedPins,3};
}
