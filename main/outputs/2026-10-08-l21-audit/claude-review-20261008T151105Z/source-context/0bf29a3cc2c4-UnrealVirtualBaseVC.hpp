#pragma once

#include <bit>

#include <ASMHelper/ASMHelper.hpp>
#include <Zydis/Zydis.h>

#define PARAMS(...) __VA_ARGS__
#define ARGS(...)  __VA_ARGS__

// Vtable entries are code addresses, not portable pointer-to-member objects.
// Preserve the Microsoft member-call ABI; native Itanium-family targets use
// the exact explicit receiver entry signature, including compiler sret handling.
#if defined(_MSC_VER)
#define RC_UE_VIRTUAL_FUNCTION_NO_PARAMS(class_name, return_type) return_type(class_name::*)() const
#define RC_UE_VIRTUAL_FUNCTION_PARAMS(class_name, return_type, ...) return_type(class_name::*)(__VA_ARGS__) const
#define RC_UE_VIRTUAL_CALL_NO_PARAMS(func) (this->*func)()
#define RC_UE_VIRTUAL_CALL_PARAMS(func, ...) (this->*func)(__VA_ARGS__)
#else
#define RC_UE_VIRTUAL_FUNCTION_NO_PARAMS(class_name, return_type) return_type(*)(const class_name*)
#define RC_UE_VIRTUAL_FUNCTION_PARAMS(class_name, return_type, ...) return_type(*)(const class_name*, __VA_ARGS__)
#define RC_UE_VIRTUAL_CALL_NO_PARAMS(func) func(this)
#define RC_UE_VIRTUAL_CALL_PARAMS(func, ...) func(this, __VA_ARGS__)
#endif

#define IMPLEMENT_UNREAL_VIRTUAL_WRAPPER_NO_PARAMS(class_name, function_name, return_type) \
static const auto offset = []() { \
    auto it = VTableLayoutMap.find(STR(#function_name)); \
    if (it == VTableLayoutMap.end())                                                         \
    {                                                                                              \
        throw std::runtime_error{"Virtual " #class_name "::" #function_name " is unavailable, possibly unsupported in engine version"}; \
    }                                                                                           \
    return it->second; \
}(); \
std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(this)); \
auto func = std::bit_cast<RC_UE_VIRTUAL_FUNCTION_NO_PARAMS(class_name, return_type)>(*std::bit_cast<void**>(vtable + offset));\
if (!func) \
{ \
throw std::runtime_error{"Function '" #function_name "' not available"}; \
}                                                                          \
return RC_UE_VIRTUAL_CALL_NO_PARAMS(func);

#define IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(class_name, function_name, return_type, params, args) \
static const auto offset = []() { \
    auto it = VTableLayoutMap.find(STR(#function_name)); \
    if (it == VTableLayoutMap.end())                                                         \
    {                                                                                              \
        throw std::runtime_error{"Virtual " #class_name "::" #function_name " is unavailable, possibly unsupported in engine version"}; \
    }\
    return it->second; \
}(); \
std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(this)); \
auto func = std::bit_cast<RC_UE_VIRTUAL_FUNCTION_PARAMS(class_name, return_type, params)>(*std::bit_cast<void**>(vtable + offset)); \
if (!func) \
{ \
throw std::runtime_error{"Function '" #function_name "' not available"}; \
}                                                                          \
return RC_UE_VIRTUAL_CALL_PARAMS(func, args);

#define IMPLEMENT_UNREAL_VIRTUAL_WRAPPER_MULTI_NAME_NO_PARAMS(class_name, function_name, return_type, ...) \
static const auto offset = []() { \
    auto it = VTableLayoutMap.end(); \
    for (const auto& name : {STR(#function_name), __VA_ARGS__}) \
    { \
        it = VTableLayoutMap.find(name); \
        if (it != VTableLayoutMap.end()) break; \
    } \
    if (it == VTableLayoutMap.end()) \
    { \
        throw std::runtime_error{"Virtual " #class_name "::" #function_name " is unavailable, possibly unsupported in engine version"}; \
    } \
    return it->second; \
}(); \
std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(this)); \
auto func = std::bit_cast<RC_UE_VIRTUAL_FUNCTION_NO_PARAMS(class_name, return_type)>(*std::bit_cast<void**>(vtable + offset)); \
if (!func) \
{ \
    throw std::runtime_error{"Function '" #function_name "' not available"}; \
} \
return RC_UE_VIRTUAL_CALL_NO_PARAMS(func);

#define IMPLEMENT_UNREAL_VIRTUAL_WRAPPER_MULTI_NAME(class_name, function_name, return_type, params, args, ...) \
static const auto offset = []() { \
    auto it = VTableLayoutMap.end(); \
    for (const auto& name : {STR(#function_name), __VA_ARGS__}) \
    { \
        it = VTableLayoutMap.find(name); \
        if (it != VTableLayoutMap.end()) break; \
    } \
    if (it == VTableLayoutMap.end()) \
    { \
        throw std::runtime_error{"Virtual " #class_name "::" #function_name " is unavailable, possibly unsupported in engine version"}; \
    } \
    return it->second; \
}(); \
std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(this)); \
auto func = std::bit_cast<RC_UE_VIRTUAL_FUNCTION_PARAMS(class_name, return_type, params)>(*std::bit_cast<void**>(vtable + offset)); \
if (!func) \
{ \
    throw std::runtime_error{"Function '" #function_name "' not available"}; \
} \
return RC_UE_VIRTUAL_CALL_PARAMS(func, args);

#define GET_ADDRESS_OF_UNREAL_VIRTUAL(class_name, function_name, instance) \
[&instance]() -> void* {                                                   \
    static const auto offset = []() { \
        auto it = class_name::VTableLayoutMap.find(STR(#function_name));                        \
        if (it == class_name::VTableLayoutMap.end())                                       \
        {                                                                      \
            throw std::runtime_error{"Virtual " #class_name "::" #function_name " is unavailable, possibly unsupported in engine version"}; \
        }\
        return it->second; \
    }(); \
    std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(instance)); \
    return ASM::resolve_function_address_from_potential_jmp(*std::bit_cast<void**>(vtable + offset));\
}()

#define OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(class_name, function_name, instance) \
[&instance]() -> void* {                                                   \
    static const auto offset = []() -> uint32_t { \
        auto it = class_name::VTableLayoutMap.find(STR(#function_name));                        \
        if (it == class_name::VTableLayoutMap.end())                                       \
        {                                                                      \
            return 0; \
        }\
        return it->second; \
    }(); \
    if (offset == 0) \
    { \
        return nullptr; \
    } \
    std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(instance)); \
    return ASM::resolve_function_address_from_potential_jmp(*std::bit_cast<void**>(vtable + offset));\
}()

namespace RC::Unreal
{
    class UnrealVirtualBaseVC
    {
    public:
        virtual ~UnrealVirtualBaseVC() = default;

    public:
        virtual auto set_virtual_offsets() -> void = 0;
    };
}


