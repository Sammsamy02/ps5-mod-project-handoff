#include <Unreal/FString.hpp>
#include "CandidateBindingData.hpp"
#include "ExtendedCodeProfile.hpp"
#include <cstdint>
#include <type_traits>
namespace NativeNameThread {
using NameFunction=const RC::Unreal::FString&(*)(std::uint32_t);
extern "C" __attribute__((used)) const std::uint64_t NAME_ABI_CENSUS[]={
 sizeof(std::uint32_t),alignof(std::uint32_t),sizeof(NameFunction),alignof(NameFunction),
 sizeof(RC::Unreal::FString),alignof(RC::Unreal::FString),
 std::is_member_function_pointer_v<NameFunction>,
 std::is_same_v<std::invoke_result_t<NameFunction,std::uint32_t>,const RC::Unreal::FString&>,
 sizeof(kPrefix),sizeof(kExtendedPins)/sizeof(kExtendedPins[0])};
}
