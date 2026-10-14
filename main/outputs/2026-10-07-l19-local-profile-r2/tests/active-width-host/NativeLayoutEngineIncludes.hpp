#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <functional>
#define STR(s) s
namespace RC::File {using StringType=std::string;}
namespace RC::Unreal {
struct BitfieldInfo {uint8_t position,length,storage;};
struct UObject;struct FFrame;
struct UFunction {std::function<void(UObject*,FFrame&,void* const)>*& GetFunc(); inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct UStruct {int16_t& GetMinAlignment();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct UClass {uint64_t& GetClassCastFlags();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct FFieldClass {uint64_t GetCastFlags();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct FProperty {inline static std::unordered_map<std::string,uint32_t>VTableLayoutMap;};
}
