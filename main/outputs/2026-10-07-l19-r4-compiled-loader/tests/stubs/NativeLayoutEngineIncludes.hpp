#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <functional>
#define STR(s) s
namespace RC::File {using StringType=std::string;}
namespace RC::Unreal {
struct BitfieldInfo {uint8_t position,length,storage;bool operator==(const BitfieldInfo&)const=default;};
struct UObject{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;}; struct FFrame; struct FProperty;
struct FName {uint64_t value;}; struct FFieldVariant {void* container; bool legacyIsObject;};
enum class EPropertyFlags:uint64_t {};
struct FArrayProperty {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FBoolProperty {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FEnumProperty {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FField {FFieldVariant GetOwnerVariant(); FProperty* GetNextFieldAsProperty(); FName GetFName();inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FFieldClass {uint64_t GetCastFlags();inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FProperty {EPropertyFlags& GetPropertyFlags(); inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FStructProperty {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FUObjectArray {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct FUObjectItem {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct TUObjectArray {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct UClass {uint64_t& GetClassCastFlags();inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct UField {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct UFunction {std::function<void(UObject*,FFrame&,void* const)>*& GetFunc(); uint32_t& GetFunctionFlags();inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct UObjectBase {inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
struct UStruct {int16_t& GetMinAlignment(); UStruct*& GetSuperStruct();inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;};
static_assert(sizeof(FFieldVariant)==16);
}

namespace RC::Unreal {struct UEngine{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};struct AGameModeBase{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};}
