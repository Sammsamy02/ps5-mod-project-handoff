#include <Unreal/FString.hpp>
#include <cstdint>
#include <memory>
extern "C" int32_t scePthreadGetthreadid(void);
namespace NativeNameThread {
using NativeNameFunction=const RC::Unreal::FString&(*)(std::uint32_t);
static_assert(sizeof(NativeNameFunction)==8);
static_assert(sizeof(RC::Unreal::FString)==16);
std::int32_t current_native_id(){return scePthreadGetthreadid();}
const void* call_native_name(std::uintptr_t entry,std::uint32_t id){
 const auto function=reinterpret_cast<NativeNameFunction>(entry);
 // Native reference-return ABI is pointer in RAX; no returned FString read.
 return std::addressof(function(id));
}
}
