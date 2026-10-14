#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fmt { template<class... T> std::string format(const char* text, T&&...) { return text; } }
namespace Helper::Integer { template<class T, class S> T to(S n) { return static_cast<T>(n); } }
namespace Unreal {
struct FName {
    FName(unsigned = 0, unsigned = 0) {}
    int GetComparisonIndex() const { return 1; }
    std::string ToString() const { return "IntProperty"; }
};
inline std::string to_string(const std::string& s) { return s; }
enum { CPF_Parm=1, CPF_OutParm=2, CPF_ReturnParm=4, CPF_ConstParm=8 };
enum class EFieldIterationFlags { IncludeDeprecated };
struct FArrayProperty {}; struct FStructProperty {};
struct FClass { FName GetFName() const { return {}; } };
struct FProperty {
    int offset=0, element=4, count=1, flags=CPF_Parm;
    bool HasAnyPropertyFlags(int f) const { return flags & f; }
    int GetOffset_Internal() const { return offset; }
    int GetElementSize() const { return element; }
    int GetArrayDim() const { return count; }
    FClass GetClass() const { return {}; }
    std::string GetName() const { return "Value"; }
    template<class T> bool IsA() const { return false; }
};
struct UFunction;
inline int engine_calls=0, pushes=0;
struct UObject {
    template<class T> bool IsA() const { return true; }
    void ProcessEvent(UFunction*, void*);
};
struct UFunction : UObject {
    uint16_t bytes=4, ret=0xffff;
    uint8_t parms=1;
    std::vector<FProperty*> props;
    uint16_t GetParmsSize() const { return bytes; }
    uint8_t GetNumParms() const { return parms; }
    uint16_t GetReturnValueOffset() const { return ret; }
    bool HasChildren() const { return !props.empty(); }
};
void UObject::ProcessEvent(UFunction*, void*) { ++engine_calls; }
template<class T> struct TFieldRange {
    UFunction* function;
    TFieldRange(UFunction* f, EFieldIterationFlags) : function(f) {}
    auto begin() { return function->props.begin(); }
    auto end() { return function->props.end(); }
};
}
using Unreal::to_string;
namespace LuaMadeSimple { struct Lua; }
namespace RC::LuaType {
struct UE4SSBaseObject { bool derives_from_function() const { return false; } };
struct UFunction : UE4SSBaseObject {
    Unreal::UFunction* value; Unreal::UObject* base;
    Unreal::UFunction* get_remote_cpp_object() { return value; }
    Unreal::UObject* get_base() { return base; }
};
struct UObject : UE4SSBaseObject { Unreal::UObject* get_remote_cpp_object() { return nullptr; } };
enum class Operation { Set, GetNonTrivialLocal };
}
namespace LuaMadeSimple {
struct Lua {
    mutable int consumed=0;
    int supplied;
    RC::LuaType::UFunction& function;
    mutable RC::LuaType::UE4SSBaseObject colon_self;
    mutable RC::LuaType::UObject object;
    bool is_userdata() const { return consumed < 2; }
    template<class T> T& get_userdata(int=1, bool peek=false) const {
        if (!peek) ++consumed;
        if constexpr (std::is_same_v<T, RC::LuaType::UFunction>) return function;
        else if constexpr (std::is_same_v<T, RC::LuaType::UObject>) return object;
        else return colon_self;
    }
    void discard_value() const { ++consumed; }
    int get_stack_size() const { return supplied; }
    bool is_table() const { return false; }
    [[noreturn]] void throw_error(const std::string& text) const { throw std::runtime_error(text); }
};
}
namespace RC::LuaType {
#include "actual_frame.inc"
struct FNativeUFunctionCall { void StoreOutTableReference(Unreal::FProperty*) {} struct {} OutParameters; };
struct PusherParams {
    Operation operation;
    const LuaMadeSimple::Lua& lua;
    Unreal::UObject* base;
    void* data;
    Unreal::FProperty* property;
};
using Pusher = void (*)(const PusherParams&);
namespace StaticState { inline std::unordered_map<int, Pusher> m_property_value_pushers; }
// This file includes the byte-for-byte selected pre-call implementation body.
// Only the post-ProcessEvent result/out-param suffix is outside this harness.
#include "actual_pre_call.inc"
}

int main(int argc, char** argv) {
    if (argc != 2) return 99;
    std::string name=argv[1];
    Unreal::FProperty a, b;
    Unreal::UFunction function;
    function.props={&a};
    int supplied=1;
    bool reject=false;
    if (name=="small") {}
    else if (name=="boundary") { function.bytes=512; a.offset=508; }
    else if (name=="empty") { function.bytes=0; function.parms=0; function.props={}; supplied=0; }
    else if (name=="nonparm_ignored") { b.offset=-10; b.element=-3; b.flags=0; function.props.push_back(&b); }
    else if (name=="array_extent") { a.count=2; function.bytes=8; }
    else if (name=="return_extent") { b.offset=4; b.flags=Unreal::CPF_Parm|Unreal::CPF_ReturnParm; function.props.push_back(&b); function.bytes=8; function.parms=2; function.ret=4; }
    else if (name=="oversized") { function.bytes=513; reject=true; }
    else if (name=="max_size") { function.bytes=65535; reject=true; }
    else if (name=="beyond_declared") { a.offset=4; reject=true; }
    else if (name=="straddles_capacity") { function.bytes=512; a.offset=510; reject=true; }
    else if (name=="negative_offset") { a.offset=-1; reject=true; }
    else if (name=="negative_element") { a.element=-1; reject=true; }
    else if (name=="zero_element") { a.element=0; reject=true; }
    else if (name=="negative_count") { a.count=-1; reject=true; }
    else if (name=="zero_count") { a.count=0; reject=true; }
    else if (name=="large_product") { a.element=0x7fffffff; a.count=0x7fffffff; reject=true; }
    else if (name=="late_bad_parameter") { b.offset=4; function.props.push_back(&b); function.parms=2; supplied=2; reject=true; }
    else if (name=="bad_return") { b.offset=4; b.flags=Unreal::CPF_Parm|Unreal::CPF_ReturnParm; function.props.push_back(&b); function.parms=2; function.ret=4; reject=true; }
    else return 98;
    RC::LuaType::UFunction object;
    object.value=&function;
    object.base=&function;
    LuaMadeSimple::Lua lua{0, supplied, object};
    RC::LuaType::FNativeUFunctionCall invocation;
    RC::LuaType::StaticState::m_property_value_pushers.emplace(1, +[](const RC::LuaType::PusherParams& p) {
        ++Unreal::pushes;
        // One actual integer argument write. A bad original offset exposes its
        // real fixed stack-buffer bounds in the ASan diagnostic controls.
        const uint32_t value=0x12345678;
        std::memcpy(p.data, &value, sizeof(value));
    });
    bool refused=false;
    try { RC::LuaType::call_ufunction_from_lua_impl(lua, invocation); }
    catch (const std::runtime_error&) { refused=true; }
    bool pass=reject ? (refused && Unreal::pushes==0 && Unreal::engine_calls==0)
                     : (!refused && Unreal::engine_calls==1 && Unreal::pushes==(supplied==0 ? 0 : 1));
    std::cout << name << " refused=" << refused << " pushes=" << Unreal::pushes
              << " engine=" << Unreal::engine_calls << " expected_refusal=" << reject
              << " pass=" << pass << '\n';
    return pass ? 0 : 1;
}
