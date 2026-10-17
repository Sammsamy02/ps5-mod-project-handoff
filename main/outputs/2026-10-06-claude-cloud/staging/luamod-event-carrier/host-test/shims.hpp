#pragma once
// Minimal stand-ins for the port types the extracted functions touch.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <fmt/xchar.h>
#define STR(x) L##x
using CharType = wchar_t;
using StringType = std::wstring;
enum class LoopAction { Continue, Break };
enum class LogLevel { Default, Warning, Error };
namespace Output {
inline std::vector<std::wstring> log;
template <LogLevel L = LogLevel::Default, typename... A> void send(const wchar_t* f, A&&... a) {
    log.push_back(fmt::format(fmt::runtime(std::wstring_view{f}), std::forward<A>(a)...));
}
}
namespace Unreal {
enum FNameFind { FNAME_Find };
struct FName { std::wstring s; FName() = default; FName(const wchar_t* x, FNameFind) : s(x) {} explicit FName(std::wstring x) : s(std::move(x)) {}
               bool Equals(const FName& o) const { return s == o.s; } };
enum class EFunctionFlags : uint32_t { FUNC_Native = 0x400 };
struct UClass;
struct UObject { UClass* cls{}; FName name; std::wstring full; virtual ~UObject() = default;
                 UClass* GetClassPrivate() const { return cls; } FName GetNamePrivate() const { return name; }
                 FName GetFName() const { return name; } std::wstring GetFullName() const { return full; } };
struct UStruct : UObject { UStruct* super{}; UStruct* GetSuperStruct() const { return super; } };
struct UClass : UStruct {};
using HookFn = void (*)(int&, void*);
struct UFunction : UStruct {
    void* func{}; uint32_t flags{}; std::vector<HookFn> post_hooks; int next_id{0}; bool refuse_hook{false};
    void* GetFuncPtr() const { return func; }
    bool HasAnyFunctionFlags(EFunctionFlags f) const { return flags & static_cast<uint32_t>(f); }
    int32_t RegisterPostHook(HookFn fn, void*) { if (refuse_hook) return -1; post_hooks.push_back(fn); return next_id++; }
};
namespace UObjectGlobals {
inline std::vector<UObject*> all;
template <typename T> T StaticFindObject(void*, void*, const StringType& path) {
    for (auto* o : all) if (o->full == path) return static_cast<T>(o);
    return nullptr;
}
inline void ForEachUObject(const std::function<LoopAction(void*, int32_t, int32_t)>& cb) {
    int32_t i = 0; for (auto* o : all) if (cb(o, 0, i++) == LoopAction::Break) break;
}
}
inline bool pump_pending = true;
inline bool R3MarshalPumpPending() { return pump_pending; }
}
// The drain stand-in: the real funcptr_marshal_hook stores last_fire on the game thread.
using UnrealScriptFunctionCallableContext = int;
