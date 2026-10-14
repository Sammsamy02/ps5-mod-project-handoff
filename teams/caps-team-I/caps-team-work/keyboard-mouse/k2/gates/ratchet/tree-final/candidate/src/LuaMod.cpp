#if defined(__PROSPERO__)
#include "NativePakMountEntry.hpp"
#include "L23GameThreadProbe.hpp"
#endif
#include <L22Diagnostics/Logging.hpp>
#include "ThreadNameRuntime.hpp"
#define NOMINMAX

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <DynamicOutput/Output.hpp>
#include <ExceptionHandling.hpp>
#include <Helpers/Format.hpp>
#include <Helpers/String.hpp>
#include <Input/Handler.hpp>
#include <LuaLibrary.hpp>
#include <LuaFNameBridge.hpp> // [LFB] ONE hand Lua-string -> engine-name converter (agent-EY)
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <LuaType/LuaAActor.hpp>
#include <LuaType/LuaCustomProperty.hpp>
#include <LuaType/LuaFName.hpp>
#include <LuaType/LuaFText.hpp>
#include <LuaType/LuaUnrealString.hpp>
#include <LuaType/LuaFOutputDevice.hpp>
#include <LuaType/LuaModRef.hpp>
#include <LuaType/LuaUClass.hpp>
#include <LuaType/LuaUObject.hpp>
#include <LuaType/LuaFURL.hpp>
#include <LuaType/LuaThreadId.hpp>
#include <Mod/CppMod.hpp>
#include <Mod/LuaMod.hpp>
#pragma warning(disable : 4005)
#include <GUI/Dumpers.hpp>
#include <UE4SSProgram.hpp>
#include <JMapGenerator/JMapGenerator.hpp>
#include <USMapGenerator/Generator.hpp>

// Local override of normalize_path_for_lua: the shared String.hpp version
// routes through wstring_convert on this target, which has the proven
// codecvt stride bug (4-byte-unit helpers with 2-byte wchar_t). All mod
// paths are pure ASCII; use the filesystem path's narrow string directly.
namespace modbridge {
    auto normalize_path_for_lua(const std::filesystem::path& path) -> std::string
    {
        std::string utf8_path = path.string();
        std::replace(utf8_path.begin(), utf8_path.end(), '\\', '/');
        return utf8_path;
    }
}
using modbridge::normalize_path_for_lua;

#ifdef __PROSPERO__
// [LFB-M] agent-FM (2026-10-10, Claude round-3 reverse directive): engine
// wide text -> Lua string for the hook/callback name echoes in this TU
// (LoadMap Error FStrings, ULocalPlayerExec/CallFunctionByNameWithName
// TCHAR commands, ProcessConsoleExec command StringTypes and their split
// parts). Every one of these was `to_string(...)` (the Helpers wide codec —
// the seam that carried the interleaved-NUL corruption). Now: hand bounded
// unit count (no wcslen), then the ONE bridge reverse loop (wide_to_narrow
// / push_fstring_to_lua; these log the 'fname-narrow'/'fstring-rev' family
// markers). No wide libc anywhere.
namespace modbridge {
    auto lfb_set_string_from_wide(const LuaMadeSimple::Lua& lua, const Unreal::TCHAR* wide) -> void
    {
        if (!wide)
        {
            lua.set_string("");
            return;
        }
        size_t units = 0;
        while (units < LuaType::LuaFNameBridge::kMaxPushUnits && wide[units] != 0)
        {
            ++units;
        }
        char narrow[LuaType::LuaFNameBridge::kMaxPushUtf8Bytes + 1];
        LuaType::LuaFNameBridge::wide_to_narrow(wide, units, narrow, sizeof(narrow));
        lua.set_string(narrow);
    }

    auto lfb_set_string_from_stringtype(const LuaMadeSimple::Lua& lua, const StringType& wide) -> void
    {
        char narrow[LuaType::LuaFNameBridge::kMaxPushUtf8Bytes + 1];
        LuaType::LuaFNameBridge::wide_to_narrow(wide.data(), wide.size(), narrow, sizeof(narrow));
        lua.set_string(narrow);
    }

    auto lfb_narrow_from_stringtype(const StringType& wide) -> std::string
    {
        char narrow[LuaType::LuaFNameBridge::kMaxPushUtf8Bytes + 1];
        LuaType::LuaFNameBridge::wide_to_narrow(wide.data(), wide.size(), narrow, sizeof(narrow));
        return std::string{narrow};
    }

    auto lfb_narrow_from_wide(const Unreal::TCHAR* wide) -> std::string
    {
        if (!wide)
        {
            return std::string{};
        }
        size_t units = 0;
        while (units < LuaType::LuaFNameBridge::kMaxPushUnits && wide[units] != 0)
        {
            ++units;
        }
        char narrow[LuaType::LuaFNameBridge::kMaxPushUtf8Bytes + 1];
        LuaType::LuaFNameBridge::wide_to_narrow(wide, units, narrow, sizeof(narrow));
        return std::string{narrow};
    }
}
using modbridge::lfb_set_string_from_wide;
using modbridge::lfb_set_string_from_stringtype;
using modbridge::lfb_narrow_from_stringtype;
using modbridge::lfb_narrow_from_wide;
#endif

#include <Unreal/Core/HAL/Platform.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/FURL.hpp>
#include <Unreal/FWorldContext.hpp>
#include <Unreal/FOutputDevice.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/PackageName.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Core/Containers/FUtf8String.hpp>
#include <Unreal/Core/Containers/FAnsiString.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/CoreUObject/UObject/FUtf8StrProperty.hpp>
#include <Unreal/TypeChecker.hpp>
#include <Unreal/UAssetRegistry.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UGameViewportClient.hpp>
#include <Unreal/UKismetSystemLibrary.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UPackage.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <UnrealCustom/CustomProperty.hpp>
#include <UE4SSRuntime.hpp>
#include <Unreal/UnrealInitializer.hpp>

#if PLATFORM_WINDOWS
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#endif
#pragma warning(default : 4005)

// Shared composition diagnostic sink (diagnostic.c, in-link C symbol) - the
// same declaration UE4SSProgram.cpp carries. Used by the [l25] runtime-phase
// tags (marshal first-invocation / throttled pump + queue-depth liveness).
extern "C" void diagnostic_event(const char* tag, unsigned int value);

#ifdef _WIN32
#define UE4SS_LUAMOD_PRINTF printf_s
#else
#define UE4SS_LUAMOD_PRINTF std::printf
#endif

#ifdef __PROSPERO__
// [CAP09] defined in src/Mod/CppMod.cpp (NativeModEngineServices.inc).
namespace RC::Platform { void NativeModsGameThreadFrame(int source) noexcept; }
// [K2] defined in src/UE4SSProgram.cpp (KbmEngineKeysRuntime.inc): engine-keys sampler, never throws.
namespace RC::Platform { void KbmGameThreadFrame() noexcept; }
#endif

namespace RC
{
    namespace Unreal::UObjectGlobals
    {
        // [R9-NOTIFY] UObjectGlobals.cpp (declared early: used by NotifyOnNewObject registration)
        auto r9_index_notify_arm() -> void;
        auto r9_index_notify_disarm() -> void;
    }
#if defined(__PROSPERO__)
    namespace L23GameThreadProbe {
        extern "C" int32_t scePthreadGetthreadid(void);
        static State state;
        static uint64_t now_ms() noexcept {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }
        static int callback(lua_State* L) noexcept {
            try {
                // [L27][EV string-seam] (agent-EV 2026-10-09, Claude task 4):
                // LUA-ORIGINATED round trips through the ACTUAL registered
                // bindings, on the game thread, before anything else. The
                // existing init battery only exercises C++ literals, which
                // never touch the narrow->wide conversion seam — exactly why
                // it passed while every runtime Lua query truncated to one
                // character ('[FFO] query-fallback ts:P'). Each probe is a
                // tiny pcall'd Lua chunk pushing a REAL Lua string through
                // lua_tostring -> ensure_str -> FindFirstOf/StaticFindObject
                // -> FName construction seam. Own '[ST] lua-*' lines, PASS =
                // call ok AND non-nil; evidence only, never a throw, and
                // never blocking the drain.
                // [LFB] (agent-EY 2026-10-09, Claude items 6-7): extended to
                // FindAllOf + the Lua FName() constructor on the SAME names;
                // every probe now flows through the ONE hand bridge
                // (lua_tolstring + hand UTF-8->UTF-16 -> slot-2 ctor), so
                // the '[LUA-FN] in=.. id=.. decoded=.. OK/MISMATCH' lines are
                // the per-probe construction evidence. The chunk literal and
                // the [LUA-FN]/[LFB] markers are the PRX composition-grep
                // strings (UTF-8 chunk bytes + UTF-16LE marker literals) —
                // the previous build's Lua self-test was composed out; grep
                // after every build.
                try {
                    static const char* k_seam_chunk =
                        "local ok1,v1=pcall(FindFirstOf,'PlayerController') "
                        "local ok2,v2=pcall(FindFirstOf,'ItemDatabaseSubsystem') "
                        "local ok3,v3=pcall(StaticFindObject,'/Script/Engine.PlayerController') "
                        "local ok4,v4=pcall(StaticFindObject,'/Script/UMG.Default__WidgetBlueprintLibrary') "
                        "local ok5,v5=pcall(FindAllOf,'PlayerController') "
                        "local ok6,v6=pcall(FName,'PlayerController') "
                        "local ok7,v7=pcall(FName,'ItemDatabaseSubsystem') "
                        "return ok1,v1~=nil,ok2,v2~=nil,ok3,v3~=nil,ok4,v4~=nil,ok5,v5~=nil,ok6,v6~=nil,ok7,v7~=nil";
                    const int top = lua_gettop(L);
                    if (luaL_loadbuffer(L, k_seam_chunk, strlen(k_seam_chunk), "seam_probe") == LUA_OK &&
                        lua_pcall(L, 0, 14, 0) == LUA_OK && lua_gettop(L) - top == 14)
                    {
                        const auto st_line = [&](const char* probe, int ok_idx, int have_idx) {
                            // Negative indices are stable: nothing touches the
                            // stack between the pcall and these reads.
                            const bool ok = lua_toboolean(L, ok_idx) != 0;
                            const bool have = lua_toboolean(L, have_idx) != 0;
                            L22Diagnostics::observe([&] {
                                Output::send(STR("[ST] lua-{} {}{}\n"), ensure_str(probe), ok && have ? STR("PASS") : STR("FAIL"),
                                              ok ? (have ? STR("") : STR(" (nil)")) : STR(" (call error)"));
                            });
                        };
                        st_line("ffo PlayerController", -14, -13);
                        st_line("ffo ItemDatabaseSubsystem", -12, -11);
                        st_line("sfo /Script/Engine.PlayerController", -10, -9);
                        st_line("sfo /Script/UMG.Default__WidgetBlueprintLibrary", -8, -7);
                        st_line("fao PlayerController", -6, -5);
                        st_line("fname PlayerController", -4, -3);
                        st_line("fname ItemDatabaseSubsystem", -2, -1);
                    }
                    else
                    {
                        L22Diagnostics::observe([] { Output::send<LogLevel::Error>(STR("[ST] lua-seam probe chunk failed\n")); });
                    }
                    lua_settop(L, top);
                }
                catch (...) { L22Diagnostics::observe([] { Output::send<LogLevel::Error>(STR("[ST] lua-seam probe exception\n")); }); }
                const int32_t tid=scePthreadGetthreadid();
                const bool genuine=Unreal::IsInGameThreadRaw();
                const auto result=state.ran(now_ms(),tid,genuine);
                L22Diagnostics::observe([&] {
                    if(result.accepted) Output::send(STR("[l23] game-thread self-test ran thread={} drain={}ms late={}\n"),result.thread,result.elapsed,result.late);
                    else Output::send(STR("[l23] game-thread self-test callback refused thread={} genuine={} state={}\n"),tid,genuine,static_cast<int>(state.status()));
                });
            } catch(...) { /* A diagnostic must not throw across the Lua C ABI. */ }
            return 0;
        }
        static int request(lua_State* L) {
            // Run all potentially raising Lua operations inside the caller's
            // pcall. No C++ owner lives on this Lua-longjmp frame.
            lua_getglobal(L,"ExecuteInGameThread");
            if(!lua_isfunction(L,-1)) return luaL_error(L,"ExecuteInGameThread is unavailable");
            lua_pushcfunction(L,callback);
            lua_call(L,1,0);
            return 0;
        }
        static void Queue(const void* owner,lua_State* L) noexcept {
            try {
                // Same lock as the production dispatcher: no concurrent drain
                // can race the queued marker or the protected main-state call.
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                if(!state.begin(owner,now_ms(),scePthreadGetthreadid())) return;
                const int top=lua_gettop(L);
                lua_pushcfunction(L,request);
                const int result=lua_pcall(L,0,0,0);
                if(result==LUA_OK) {
                    state.queued();
                    L22Diagnostics::observe([&]{Output::send(STR("[l23] game-thread self-test queued via ExecuteInGameThread deadline=30000ms\n"));});
                } else {
                    state.failed();
                    L22Diagnostics::observe([&]{const char* msg=lua_tostring(L,-1);Output::send(STR("[l23] game-thread self-test enqueue failed: {}\n"),ensure_str(msg?msg:"non-string Lua error"));});
                }
                lua_settop(L,top);
            } catch(...) { state.failed(); }
        }
        static void Cancel(const void* owner) noexcept {
            try { if(state.cancel(owner)) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test cancelled before Lua owner teardown\n"));}); }
            catch(...) {}
        }
        void Poll() noexcept {
            try { if(state.timeout(now_ms())) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test NOT DRAINED within 30000ms; startup continues; callback remains owned by the normal queue\n"));}); }
            catch(...) {}
        }
    }
#endif

    namespace Unreal
    {
        // =================================================================================
        // [R3] FuncPtr-marshal bridges (DIFF 1; R3-TIER-DESIGN 4.4a/4.4b). Defined in the
        // replaced initializer unit (DIFF 2). Deliberately declared here rather than in a
        // shared header: touching Unreal/UnrealInitializer.hpp would force a tree-wide
        // recompile. The definitions exist only under __PROSPERO__; every reference below
        // is inside #ifdef __PROSPERO__ so non-PROSPERO builds never odr-use them.
        // Signatures are part of the DIFF 1/DIFF 2 contract and must not drift.
        // =================================================================================
        auto R3MarshalPublishGameThreadIdentity() -> bool;
        auto R3MarshalPumpPending() -> bool;
    }

    LuaMadeSimple::Lua* LuaStatics::console_executor{};
    bool LuaStatics::console_executor_enabled{};

    auto get_mod_ref(const LuaMadeSimple::Lua& lua) -> LuaMod*
    {
        if (lua_getglobal(lua.get_lua_state(), "ModRef") == LUA_TNIL)
        {
            lua.throw_error("[get_mod_ref] Tried retrieving 'ModRef' global variable but it was nil, please do not override this global");
        }

        // Explicitly using the top of the stack (-1) since that's where 'getglobal' puts stuff
        auto& lua_object = lua.get_userdata<LuaType::LuaModRef>(-1);
        return lua_object.get_remote_cpp_object();
    }

    // [B5] get_mod_ref for call sites that hold LuaMod::m_thread_actions_mutex.
    // Lua is compiled as C on this port, so lua.throw_error (luaL_error) is a
    // longjmp that skips the lock guard's destructor and leaks the mutex for the
    // rest of the session. A missing 'ModRef' is therefore reported here with a
    // C++ exception (same text as get_mod_ref). Use it ONLY inside a function
    // registered through register_function: process_lua_function's TRY catches
    // the exception, the guard has already been released by then, and TRY raises
    // the same Lua error afterwards.
    // [R9-GUARD] RAII for LuaMod::actions_lock()/actions_unlock(): a throw between the
    // two (e.g. bad_alloc in emplace/insert) must not leave the per-mod action mutex held
    // (B6c review: caught exceptions would otherwise freeze the async loop and the game).
    struct ActionsLockGuard
    {
        LuaMod* mod;
        explicit ActionsLockGuard(LuaMod* m) : mod(m) { mod->actions_lock(); }
        ~ActionsLockGuard() { mod->actions_unlock(); }
        ActionsLockGuard(const ActionsLockGuard&) = delete;
        auto operator=(const ActionsLockGuard&) -> ActionsLockGuard& = delete;
    };

    static auto get_mod_ref_cxx(const LuaMadeSimple::Lua& lua) -> LuaMod*
    {
        if (lua_getglobal(lua.get_lua_state(), "ModRef") == LUA_TNIL)
        {
            throw std::runtime_error("[get_mod_ref] Tried retrieving 'ModRef' global variable but it was nil, please do not override this global");
        }

        // Explicitly using the top of the stack (-1) since that's where 'getglobal' puts stuff
        auto& lua_object = lua.get_userdata<LuaType::LuaModRef>(-1);
        return lua_object.get_remote_cpp_object();
    }

    // [B5-opt] Registry::get_function_ref with a C++ exception instead of
    // lua.throw_error, for NATIVE callers (not registered Lua functions) that hold
    // LuaMod::m_thread_actions_mutex and already catch std::exception. When no Lua
    // pcall is active, throw_error throws C++ anyway; when the caller is nested
    // inside a Lua pcall (a hook fired from a Lua-initiated engine call) it is a
    // longjmp over the lock guard. Same stack effect as the original
    // (rawgeti, then discard the non-function on failure).
    static auto get_function_ref_cxx(const LuaMadeSimple::Lua& lua, int32_t registry_index) -> void
    {
        if (lua.rawgeti(LUA_REGISTRYINDEX, registry_index) != LUA_TFUNCTION)
        {
            lua.discard_value();
            throw std::runtime_error("[Lua::Registry::get_function_ref] Ref was not function");
        }
    }

    // =====================================================================================
    // [REFUSED-LOG] (round8 refused-hooks track; generic, no mod/game names) ONE uniform line
    // for every hook/registration this port refuses (the Lua call throws) or accepts but can
    // never deliver (the call returns normally, the callback is never invoked):
    //
    //   [REFUSED] kind=<Lua API> target='<UFunction path|class|event|command|->' mod='<mod|*>' reason=<code> effect=<throws|accepted-undelivered|queued-pending|never-delivered>
    //
    // Why: the B4 launch had no log line at all for the minimap's 3 refused RegisterHook calls
    // and for 18 self-test registrations that returned "registered"/ret=nil but never fire.
    // Formatting uses narrow literals + one-char widening (the B6 emit_line technique), so it
    // does not depend on ensure_str / the short-wchar codec. The helper never throws and holds
    // no lock; it is safe to call immediately before lua.throw_error (it returns first, so no
    // C++ object of the caller is skipped by a longjmp that it did not already skip).
    // =====================================================================================
    namespace
    {
    namespace refused_log
    {
        // Gate state. Written at startup / by the one-shot LoadMap arm; read at registration time.
        inline std::atomic<bool> eager_detour_block_skipped{false}; // on_program_start: R3MarshalPumpPending() branch taken
        inline std::atomic<int> loadmap_state{0};                   // 0 eager block ran, 1 deferred arm pending, 2 armed, 3 arm refused
        inline std::atomic<int> world_route_state{0};               // [WORLD-EV] object-index route: 0 pending, 1 live, 2 refused
        inline std::atomic<uint32_t> total{0};                      // every refusal/undelivered registration, even past the line cap
        constexpr uint32_t kMaxLines = 256;

        enum class Family { EagerDetour, LoadMap, CustomEvent, WorldEvent, BeginPlay, EndPlay };

        inline auto ascii_from(const StringType& s, char* out, size_t n) -> void
        {
            size_t i = 0;
            for (; i + 1 < n && i < s.size(); ++i)
            {
                const auto c = static_cast<unsigned long>(s[i]);
                out[i] = (c >= 0x20 && c < 0x7f && c != '\'') ? static_cast<char>(c) : '?';
            }
            out[i] = '\0';
        }

        inline auto emit(const char* kind, const StringType& target, const StringType& mod, const char* reason, const char* effect) -> void
        {
            const auto n = total.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n > kMaxLines + 1) { return; }
            char t[300]; char m[100]; char line[640];
            ascii_from(target, t, sizeof(t));
            ascii_from(mod, m, sizeof(m));
            if (n == kMaxLines + 1)
            {
                std::snprintf(line, sizeof(line), "[REFUSED] kind=log target='-' mod='*' reason=line-cap-%u-reached effect=suppressed", kMaxLines);
            }
            else
            {
                std::snprintf(line, sizeof(line), "[REFUSED] kind=%s target='%s' mod='%s' reason=%s effect=%s", kind, t, m, reason, effect);
            }
            StringType wide{};
            for (const char* p = line; *p != '\0'; ++p)
            {
                wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p)));
            }
            L22Diagnostics::observe([&]() { Output::send(STR("{}\n"), wide); });
        }

        // Mod name of the calling Lua state, never throws, leaves the Lua stack as it found it.
        inline auto lua_rawgetfield_compat(lua_State* L, const char* k) -> int
        {
            lua_pushstring(L, k);
            return lua_rawget(L, -2);
        }

        inline auto mod_name_of(const LuaMadeSimple::Lua& lua) -> StringType
        {
            StringType out{STR("?")};
            lua_State* L = lua.get_lua_state();
            const int top = lua_gettop(L);
            try
            {
                lua_pushglobaltable(L);
                if (lua_rawgetfield_compat(L, "ModRef") == LUA_TUSERDATA)
                {
                    auto& ref = lua.get_userdata<LuaType::LuaModRef>(-1);
                    if (auto* m = ref.get_remote_cpp_object()) { out = StringType{m->get_name()}; }
                }
            }
            catch (void*)
            {
                // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
                // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
                throw;
            }
            catch (...)
            {
                out = StringType{STR("?")};
            }
            lua_settop(L, top);
            return out;
        }

        // nullptr when the family can be delivered right now, else the reason code.
        inline auto undelivered_reason(Family f) -> const char*
        {
            switch (f)
            {
            case Family::EagerDetour:
                return eager_detour_block_skipped.load(std::memory_order_acquire) ? "detour-block-skipped-r3-marshal" : nullptr;
            case Family::LoadMap:
                // [WORLD-EV] delivered by an armed detour (state 2), by the eager block (not skipped),
                // or else by the data-only object-index route; a gap only once that route is refused.
                if (loadmap_state.load(std::memory_order_acquire) == 2 || !eager_detour_block_skipped.load(std::memory_order_acquire)) { return nullptr; }
                return world_route_state.load(std::memory_order_acquire) == 2 ? "world-event-route-refused" : nullptr;
            case Family::WorldEvent:
                // [WORLD-EV] InitGameState / BeginPlay: object-index route in the skipped-detour world.
                if (!eager_detour_block_skipped.load(std::memory_order_acquire)) { return nullptr; }
                return world_route_state.load(std::memory_order_acquire) == 2 ? "world-event-route-refused" : nullptr;
            case Family::BeginPlay:
                // [WORLD-EV] delivered, but only PARTIALLY (actors of streamed levels / World Partition
                // cells are not reported): stays a counted gap until the coverage is console-proven.
                if (!eager_detour_block_skipped.load(std::memory_order_acquire)) { return nullptr; }
                return world_route_state.load(std::memory_order_acquire) == 2 ? "world-event-route-refused" : "beginplay-partial-streamed-unreported";
            case Family::EndPlay:
                // [WORLD-EV] no data-only route: the object index sees an actor go only after it is
                // freed (no live actor to hand to Lua); ReceiveEndPlay Func swaps would use up the
                // script-thunk slots the pump carriers need.
                return eager_detour_block_skipped.load(std::memory_order_acquire) ? "endplay-no-data-only-route" : nullptr;
            case Family::CustomEvent:
            {
                const bool plsf = Unreal::UObject::ProcessLocalScriptFunctionInternal.is_ready() && Unreal::Version::IsAtLeast(4, 22);
                const bool pi = Unreal::UObject::ProcessInternalInternal.is_ready() && Unreal::Version::IsBelow(4, 22);
                return (plsf || pi) ? nullptr : "process-local-script-function-unavailable";
            }
            }
            return nullptr;
        }

        // The Lua call is about to throw. Call this BEFORE lua.throw_error.
        inline auto throws(const LuaMadeSimple::Lua& lua, const char* kind, const StringType& target, const char* reason) -> void
        {
            emit(kind, target, mod_name_of(lua), reason, "throws");
        }

        // The Lua call is about to return normally. Logs only when the callback can never (or not yet) fire.
        inline auto accepted(const LuaMadeSimple::Lua& lua, const char* kind, const StringType& target, Family f) -> void
        {
            if (const char* why = undelivered_reason(f))
            {
                // [WORLD-EV] LoadMap is no longer "queued-pending": its only remaining reason is a refused route
                emit(kind, target, mod_name_of(lua), why,
                     f == Family::BeginPlay && world_route_state.load(std::memory_order_acquire) != 2 ? "partial-delivered" : "accepted-undelivered");
            }
        }

        // [WORLD-EV] one NON-counting line per registration kind (id 0..31): how the PS5 delivery
        // of this hook differs from PC. Silent in the detour world and once the route is refused.
        inline auto route_note(unsigned id, const char* kind, const char* timing) -> void
        {
            static std::atomic<uint32_t> s_noted{0};
            if (id >= 32 || !eager_detour_block_skipped.load(std::memory_order_acquire) || world_route_state.load(std::memory_order_acquire) == 2) { return; }
            if ((s_noted.fetch_or(1u << id, std::memory_order_relaxed) & (1u << id)) != 0) { return; }
            char line[512];
            std::snprintf(line, sizeof(line), "[WORLD-EV] registration kind=%s route=object-index timing=%s", kind, timing);
            StringType wide{};
            for (const char* p = line; *p != '\0'; ++p)
            {
                wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p)));
            }
            L22Diagnostics::observe([&]() { Output::send(STR("{}\n"), wide); });
        }
    }
    }

    static auto get_function_name_without_prefix(const StringType& function_full_name) -> StringType
    {
        static constexpr StringViewType function_prefix{STR("Function ")};
        if (auto prefix_pos = function_full_name.find(function_prefix); prefix_pos != function_full_name.npos)
        {
            return function_full_name.substr(prefix_pos + function_prefix.size());
        }
        else
        {
            return function_full_name;
        }
    }

    struct LuaUnrealScriptFunctionData
    {
        Unreal::CallbackId pre_callback_id;
        Unreal::CallbackId post_callback_id;
        Unreal::UFunction* unreal_function;
        const Mod* mod;
        const LuaMadeSimple::Lua& lua;
        const int lua_callback_ref;
        const int lua_post_callback_ref;
        const int lua_thread_ref;

        bool has_return_value{};
        // Will be non-nullptr if the UFunction has a return value
        Unreal::FProperty* return_property{};
        std::atomic<bool> scheduled_for_removal{false};

        LuaUnrealScriptFunctionData(Unreal::CallbackId pre_id,
                                    Unreal::CallbackId post_id,
                                    Unreal::UFunction* func,
                                    const Mod* m,
                                    const LuaMadeSimple::Lua& l,
                                    int cb_ref,
                                    int post_cb_ref,
                                    int thread_ref)
            : pre_callback_id(pre_id), post_callback_id(post_id), unreal_function(func), mod(m), lua(l),
              lua_callback_ref(cb_ref), lua_post_callback_ref(post_cb_ref), lua_thread_ref(thread_ref)
        {
        }
    };
    static std::vector<std::unique_ptr<LuaUnrealScriptFunctionData>> g_hooked_script_function_data{};

    static auto lua_unreal_script_function_hook_pre(Unreal::UnrealScriptFunctionCallableContext context, void* custom_data) -> void
    {
        TRY([&]() {
            // Fetch the data corresponding to this UFunction
            auto& lua_data = *static_cast<LuaUnrealScriptFunctionData*>(custom_data);

            // Check if this hook has been scheduled for removal (Lua state may be invalid)
            if (lua_data.scheduled_for_removal) return;

            // Use the stored registry index to put a Lua function on the Lua stack
            // This is the function that was provided by the Lua call to "RegisterHook"
            lua_data.lua.registry().get_function_ref(lua_data.lua_callback_ref);

            // Set up the first param (context / this-ptr)
            // TODO: Check what happens if a static UFunction is hooked since they don't have any context
            static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
            LuaType::RemoteUnrealParam::construct(lua_data.lua, &context.Context, s_object_property_name);

            // Attempt at dynamically fetching the params
            const auto FunctionBeingExecuted = lua_data.unreal_function;
            uint16_t return_value_offset = FunctionBeingExecuted->GetReturnValueOffset();

            // 'ReturnValueOffset' is 0xFFFF if the UFunction return type is void
            lua_data.has_return_value = return_value_offset != 0xFFFF;

            uint8_t num_unreal_params = FunctionBeingExecuted->GetNumParms();
            if (lua_data.has_return_value)
            {
                // Subtract one from the number of params if there's a return value
                // This is because Unreal treats the return value as a param, and it's included in the 'NumParms' member variable
                --num_unreal_params;
            }

            bool has_properties_to_process = lua_data.has_return_value || num_unreal_params > 0;
            if (has_properties_to_process && (context.TheStack.Locals() || context.TheStack.OutParms()))
            {
                // int32_t current_param_offset{};

                for (Unreal::FProperty* func_prop : Unreal::TFieldRange<Unreal::FProperty>(FunctionBeingExecuted, Unreal::EFieldIterationFlags::IncludeDeprecated))
                {
                    // Skip this property if it's not a parameter
                    if (!func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                    {
                        continue;
                    }

                    // Skip if this property corresponds to the return value
                    if (lua_data.has_return_value && func_prop->GetOffset_Internal() == return_value_offset)
                    {
                        lua_data.return_property = func_prop;
                        continue;
                    }

                    Unreal::FName property_type = func_prop->GetClass().GetFName();
                    int32_t name_comparison_index = property_type.GetComparisonIndex();

                    if (LuaType::StaticState::m_property_value_pushers.contains(name_comparison_index))
                    {
                        // Non-typed pointer to the current parameter value
                        void* data{};
                        if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_OutParm))
                        {
                            data = Unreal::FindOutParamValueAddress(context.TheStack, func_prop);
                        }
                        else
                        {
                            data = func_prop->ContainerPtrToValuePtr<void>(context.TheStack.Locals());
                        }

                        // Keeping track of where in the 'Locals' array the next property is
                        // current_param_offset += func_prop->GetSize();

                        // Set up a call to a handler for this type of Unreal property (the param)
                        // The FName is being used as a key for an unordered_map which has the types & corresponding handlers filled right after the dll is injected
                        const LuaType::PusherParams pusher_params{.operation = LuaType::Operation::GetParam,
                                                                  .lua = lua_data.lua,
                                                                  .base = nullptr,
                                                                  .data = data,
                                                                  .property = func_prop};
                        LuaType::StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
                    }
                    else
                    {
                        lua_data.lua.throw_error(fmt::format(
                                "[unreal_script_function_hook] Tried accessing unreal property without a registered handler. Property type '{}' not supported.",
                                to_string(property_type.ToString())));
                    }
                }
            }

            // Call the Lua function with the correct number of parameters & return values
            // Increasing the 'num_params' by one to account for the 'this / context' param
            lua_data.lua.call_function(num_unreal_params + 1, 1);

            // The params for the Lua script will be 'userdata' and they will have get/set functions
            // Use these functions in the Lua script to access & mutate the parameter values

            // After the Lua function has been executed you should call the original function
            // This will execute any internal UE4 scripting functions & native functions depending on the type of UFunction
            // The API will automatically call the original function
            // This function continues in 'lua_unreal_script_function_hook_post' which executes immediately after the original function gets called
        });
    }

    static auto lua_unreal_script_function_hook_post(Unreal::UnrealScriptFunctionCallableContext context, void* custom_data) -> void
    {
        // Fetch the data corresponding to this UFunction
        auto& lua_data = *static_cast<LuaUnrealScriptFunctionData*>(custom_data);

        // Returns true if a hooks were removed.
        auto remove_if_scheduled = [&] -> bool {
            if (lua_data.scheduled_for_removal)
            {
                const auto function_name_no_prefix = get_function_name_without_prefix(lua_data.unreal_function->GetFullName());

                Output::send<LogLevel::Verbose>(STR("Unregistering native pre-hook ({}) for {}\n"), lua_data.pre_callback_id, function_name_no_prefix);
                lua_data.unreal_function->UnregisterHook(lua_data.pre_callback_id);
                luaL_unref(lua_data.lua.get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_callback_ref);

                Output::send<LogLevel::Verbose>(STR("Unregistering native post-hook ({}) for {}\n"), lua_data.post_callback_id, function_name_no_prefix);
                lua_data.unreal_function->UnregisterHook(lua_data.post_callback_id);
                if (lua_data.lua_post_callback_ref != -1)
                {
                    luaL_unref(lua_data.lua.get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_post_callback_ref);
                }

                const auto mod = get_mod_ref(lua_data.lua);
                luaL_unref(mod->lua().get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_thread_ref);
                std::erase_if(g_hooked_script_function_data, [&](const std::unique_ptr<LuaUnrealScriptFunctionData>& elem) {
                    return elem.get() == &lua_data;
                });

                return true;
            }
            else
            {
                return false;
            }
        };

        // Removes pre & post-hook callbacks if UnregisterHook was called in the pre-callback.
        if (remove_if_scheduled())
        {
            return;
        }

        TRY([&]() {
            auto process_return_value = [&]() {
                // If 'nil' exists on the Lua stack, that means that the UFunction expected a return value but the Lua script didn't return anything
                // So we can simply clean the stack and let the UFunction decide the return value on its own
                if (lua_data.lua.is_nil())
                {
                    lua_data.lua.discard_value();
                }
                else if (lua_data.lua.get_stack_size() > 0 && lua_data.has_return_value && lua_data.return_property && context.RESULT_DECL)
                {
                    // Fetch the return value from Lua if the UFunction expects one
                    // If no return value exists then assume that the Lua script didn't want to override the original
                    // Keep in mind that the if this was a Blueprint UFunction then the entire byte-code will already have executed
                    // That means that changing the return value here won't affect the script itself
                    // If this was a native UFunction then changing the return value here will have the desired effect

                    Unreal::FName property_type_name = lua_data.return_property->GetClass().GetFName();
                    int32_t name_comparison_index = property_type_name.GetComparisonIndex();

                    if (LuaType::StaticState::m_property_value_pushers.contains(name_comparison_index))
                    {
                        const LuaType::PusherParams pusher_params{.operation = LuaType::Operation::Set,
                                                                  .lua = lua_data.lua,
                                                                  .base = static_cast<Unreal::UObject*>(context.RESULT_DECL),
                                                                  .data = context.RESULT_DECL,
                                                                  .property = lua_data.return_property};
                        LuaType::StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
                    }
                    else
                    {
                        // If the type wasn't supported then we simply clean the Lua stack, output a warning and then do nothing
                        lua_data.lua.discard_value();

                        auto parameter_type_name = property_type_name.ToString();
                        auto parameter_name = lua_data.return_property->GetName();

                        Output::send(
                                STR("Tried altering return value of a hooked UFunction without a registered handler for return type Return property '{}' of type "
                                    "'{}' not supported."),
                                parameter_name,
                                parameter_type_name);
                    }
                }
            };

            if (lua_data.lua_post_callback_ref != -1)
            {
                // Use the stored registry index to put a Lua function on the Lua stack
                // This is the function that was provided by the Lua call to "RegisterHook"
                lua_data.lua.registry().get_function_ref(lua_data.lua_post_callback_ref);

                // Set up the first param (context / this-ptr)
                static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                LuaType::RemoteUnrealParam::construct(lua_data.lua, &context.Context, s_object_property_name);

                // Attempt at dynamically fetching the params
                const auto FunctionBeingExecuted = lua_data.unreal_function;
                uint16_t return_value_offset = FunctionBeingExecuted->GetReturnValueOffset();

                // 'ReturnValueOffset' is 0xFFFF if the UFunction return type is void
                lua_data.has_return_value = return_value_offset != 0xFFFF;

                uint8_t num_unreal_params = FunctionBeingExecuted->GetNumParms();
                if (lua_data.has_return_value)
                {
                    // Subtract one from the number of params if there's a return value
                    // This is because Unreal treats the return value as a param, and it's included in the 'NumParms' member variable
                    --num_unreal_params;

                    // Set up the return value param so that Lua can access the original return value
                    auto return_property = FunctionBeingExecuted->GetReturnProperty();
                    auto return_property_type = return_property->GetClass().GetFName();
                    int32_t name_comparison_index = return_property_type.GetComparisonIndex();
                    if (LuaType::StaticState::m_property_value_pushers.contains(name_comparison_index))
                    {
                        const LuaType::PusherParams pusher_params{.operation = LuaType::Operation::GetParam,
                                                                  .lua = lua_data.lua,
                                                                  .base = nullptr,
                                                                  .data = context.RESULT_DECL,
                                                                  .property = return_property};
                        LuaType::StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
                    }
                }

                bool has_properties_to_process = lua_data.has_return_value || num_unreal_params > 0;
                if (has_properties_to_process && context.TheStack.Locals())
                {
                    for (Unreal::FProperty* func_prop : Unreal::TFieldRange<Unreal::FProperty>(FunctionBeingExecuted, Unreal::EFieldIterationFlags::IncludeDeprecated))
                    {
                        // Skip this property if it's not a parameter
                        if (!func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                        {
                            continue;
                        }

                        // Skip if this property corresponds to the return value
                        if (lua_data.has_return_value && func_prop->GetOffset_Internal() == return_value_offset)
                        {
                            lua_data.return_property = func_prop;
                            continue;
                        }

                        Unreal::FName property_type = func_prop->GetClass().GetFName();
                        int32_t name_comparison_index = property_type.GetComparisonIndex();

                        if (LuaType::StaticState::m_property_value_pushers.contains(name_comparison_index))
                        {
                            // Non-typed pointer to the current parameter value
                            void* data{};
                            if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_OutParm))
                            {
                                // For out params (including ref params), get the modified value from OutParms
                                data = Unreal::FindOutParamValueAddress(context.TheStack, func_prop);
                            }
                            else
                            {
                                // For regular input params, read from Locals
                                data = func_prop->ContainerPtrToValuePtr<void>(context.TheStack.Locals());
                            }

                            // Keeping track of where in the 'Locals' array the next property is
                            // current_param_offset += func_prop->GetSize();

                            // Set up a call to a handler for this type of Unreal property (the param)
                            // The FName is being used as a key for an unordered_map which has the types & corresponding handlers filled right after the dll is injected
                            const LuaType::PusherParams pusher_params{.operation = LuaType::Operation::GetParam,
                                                                      .lua = lua_data.lua,
                                                                      .base = nullptr,
                                                                      .data = data,
                                                                      .property = func_prop};
                            LuaType::StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
                        }
                        else
                        {
                            lua_data.lua.throw_error(fmt::format(
                                    "[unreal_script_function_hook] Tried accessing unreal property without a registered handler. Property type '{}' not supported.",
                                    to_string(property_type.ToString())));
                        }
                    }
                }

                // Call the Lua function with the correct number of parameters & return values
                // Increasing the 'num_params' by one to account for the 'this / context' param
                // Increasing it again if there's a return value because we store that as the second param
                lua_data.lua.call_function(num_unreal_params + (lua_data.has_return_value ? 2 : 1), 1);
            }

            // Processing potential return values from both callbacks.
            // Stack pos 1: return value from callback 1 (nil if nothing returned)
            // Stack pos 2: return value from callback 2
            // We will always have at leaste two return values, either one can be nil, and we need to process both in case one isn't nil.
            process_return_value();
            process_return_value();
        });

        // Removes pre & post-hook callbacks if UnregisterHook was called in the post-hook callback.
        // Deliberately outside the TRY so the hooks still get removed when the callback throws.
        remove_if_scheduled();
    }

    static auto register_input_globals(const LuaMadeSimple::Lua& lua) -> void
    {
        LuaMadeSimple::Lua::Table key_table = lua.prepare_new_table();
        key_table.add_pair("LEFT_MOUSE_BUTTON", static_cast<uint32_t>(Input::Key::LEFT_MOUSE_BUTTON));
        key_table.add_pair("RIGHT_MOUSE_BUTTON", static_cast<uint32_t>(Input::Key::RIGHT_MOUSE_BUTTON));
        key_table.add_pair("CANCEL", static_cast<uint32_t>(Input::Key::CANCEL));
        key_table.add_pair("MIDDLE_MOUSE_BUTTON", static_cast<uint32_t>(Input::Key::MIDDLE_MOUSE_BUTTON));
        key_table.add_pair("XBUTTON_ONE", static_cast<uint32_t>(Input::Key::XBUTTON_ONE));
        key_table.add_pair("XBUTTON_TWO", static_cast<uint32_t>(Input::Key::XBUTTON_TWO));
        key_table.add_pair("BACKSPACE", static_cast<uint32_t>(Input::Key::BACKSPACE));
        key_table.add_pair("TAB", static_cast<uint32_t>(Input::Key::TAB));
        key_table.add_pair("CLEAR", static_cast<uint32_t>(Input::Key::CLEAR));
        key_table.add_pair("RETURN", static_cast<uint32_t>(Input::Key::RETURN));
        key_table.add_pair("PAUSE", static_cast<uint32_t>(Input::Key::PAUSE));
        key_table.add_pair("CAPS_LOCK", static_cast<uint32_t>(Input::Key::CAPS_LOCK));
        key_table.add_pair("IME_KANA", static_cast<uint32_t>(Input::Key::IME_KANA));
        key_table.add_pair("IME_HANGUEL", static_cast<uint32_t>(Input::Key::IME_HANGUEL));
        key_table.add_pair("IME_HANGUL", static_cast<uint32_t>(Input::Key::IME_HANGUL));
        key_table.add_pair("IME_ON", static_cast<uint32_t>(Input::Key::IME_ON));
        key_table.add_pair("IME_JUNJA", static_cast<uint32_t>(Input::Key::IME_JUNJA));
        key_table.add_pair("IME_FINAL", static_cast<uint32_t>(Input::Key::IME_FINAL));
        key_table.add_pair("IME_HANJA", static_cast<uint32_t>(Input::Key::IME_HANJA));
        key_table.add_pair("IME_KANJI", static_cast<uint32_t>(Input::Key::IME_KANJI));
        key_table.add_pair("IME_OFF", static_cast<uint32_t>(Input::Key::IME_OFF));
        key_table.add_pair("ESCAPE", static_cast<uint32_t>(Input::Key::ESCAPE));
        key_table.add_pair("IME_CONVERT", static_cast<uint32_t>(Input::Key::IME_CONVERT));
        key_table.add_pair("IME_NONCONVERT", static_cast<uint32_t>(Input::Key::IME_NONCONVERT));
        key_table.add_pair("IME_ACCEPT", static_cast<uint32_t>(Input::Key::IME_ACCEPT));
        key_table.add_pair("IME_MODECHANGE", static_cast<uint32_t>(Input::Key::IME_MODECHANGE));
        key_table.add_pair("SPACE", static_cast<uint32_t>(Input::Key::SPACE));
        key_table.add_pair("PAGE_UP", static_cast<uint32_t>(Input::Key::PAGE_UP));
        key_table.add_pair("PAGE_DOWN", static_cast<uint32_t>(Input::Key::PAGE_DOWN));
        key_table.add_pair("END", static_cast<uint32_t>(Input::Key::END));
        key_table.add_pair("HOME", static_cast<uint32_t>(Input::Key::HOME));
        key_table.add_pair("LEFT_ARROW", static_cast<uint32_t>(Input::Key::LEFT_ARROW));
        key_table.add_pair("UP_ARROW", static_cast<uint32_t>(Input::Key::UP_ARROW));
        key_table.add_pair("RIGHT_ARROW", static_cast<uint32_t>(Input::Key::RIGHT_ARROW));
        key_table.add_pair("DOWN_ARROW", static_cast<uint32_t>(Input::Key::DOWN_ARROW));
        key_table.add_pair("SELECT", static_cast<uint32_t>(Input::Key::SELECT));
        key_table.add_pair("PRINT", static_cast<uint32_t>(Input::Key::PRINT));
        key_table.add_pair("EXECUTE", static_cast<uint32_t>(Input::Key::EXECUTE));
        key_table.add_pair("PRINT_SCREEN", static_cast<uint32_t>(Input::Key::PRINT_SCREEN));
        key_table.add_pair("INS", static_cast<uint32_t>(Input::Key::INS));
        key_table.add_pair("DEL", static_cast<uint32_t>(Input::Key::DEL));
        key_table.add_pair("HELP", static_cast<uint32_t>(Input::Key::HELP));
        key_table.add_pair("ZERO", static_cast<uint32_t>(Input::Key::ZERO));
        key_table.add_pair("ONE", static_cast<uint32_t>(Input::Key::ONE));
        key_table.add_pair("TWO", static_cast<uint32_t>(Input::Key::TWO));
        key_table.add_pair("THREE", static_cast<uint32_t>(Input::Key::THREE));
        key_table.add_pair("FOUR", static_cast<uint32_t>(Input::Key::FOUR));
        key_table.add_pair("FIVE", static_cast<uint32_t>(Input::Key::FIVE));
        key_table.add_pair("SIX", static_cast<uint32_t>(Input::Key::SIX));
        key_table.add_pair("SEVEN", static_cast<uint32_t>(Input::Key::SEVEN));
        key_table.add_pair("EIGHT", static_cast<uint32_t>(Input::Key::EIGHT));
        key_table.add_pair("NINE", static_cast<uint32_t>(Input::Key::NINE));
        key_table.add_pair("A", static_cast<uint32_t>(Input::Key::A));
        key_table.add_pair("B", static_cast<uint32_t>(Input::Key::B));
        key_table.add_pair("C", static_cast<uint32_t>(Input::Key::C));
        key_table.add_pair("D", static_cast<uint32_t>(Input::Key::D));
        key_table.add_pair("E", static_cast<uint32_t>(Input::Key::E));
        key_table.add_pair("F", static_cast<uint32_t>(Input::Key::F));
        key_table.add_pair("G", static_cast<uint32_t>(Input::Key::G));
        key_table.add_pair("H", static_cast<uint32_t>(Input::Key::H));
        key_table.add_pair("I", static_cast<uint32_t>(Input::Key::I));
        key_table.add_pair("J", static_cast<uint32_t>(Input::Key::J));
        key_table.add_pair("K", static_cast<uint32_t>(Input::Key::K));
        key_table.add_pair("L", static_cast<uint32_t>(Input::Key::L));
        key_table.add_pair("M", static_cast<uint32_t>(Input::Key::M));
        key_table.add_pair("N", static_cast<uint32_t>(Input::Key::N));
        key_table.add_pair("O", static_cast<uint32_t>(Input::Key::O));
        key_table.add_pair("P", static_cast<uint32_t>(Input::Key::P));
        key_table.add_pair("Q", static_cast<uint32_t>(Input::Key::Q));
        key_table.add_pair("R", static_cast<uint32_t>(Input::Key::R));
        key_table.add_pair("S", static_cast<uint32_t>(Input::Key::S));
        key_table.add_pair("T", static_cast<uint32_t>(Input::Key::T));
        key_table.add_pair("U", static_cast<uint32_t>(Input::Key::U));
        key_table.add_pair("V", static_cast<uint32_t>(Input::Key::V));
        key_table.add_pair("W", static_cast<uint32_t>(Input::Key::W));
        key_table.add_pair("X", static_cast<uint32_t>(Input::Key::X));
        key_table.add_pair("Y", static_cast<uint32_t>(Input::Key::Y));
        key_table.add_pair("Z", static_cast<uint32_t>(Input::Key::Z));
        key_table.add_pair("LEFT_WIN", static_cast<uint32_t>(Input::Key::LEFT_WIN));
        key_table.add_pair("RIGHT_WIN", static_cast<uint32_t>(Input::Key::RIGHT_WIN));
        key_table.add_pair("APPS", static_cast<uint32_t>(Input::Key::APPS));
        key_table.add_pair("SLEEP", static_cast<uint32_t>(Input::Key::SLEEP));
        key_table.add_pair("NUM_ZERO", static_cast<uint32_t>(Input::Key::NUM_ZERO));
        key_table.add_pair("NUM_ONE", static_cast<uint32_t>(Input::Key::NUM_ONE));
        key_table.add_pair("NUM_TWO", static_cast<uint32_t>(Input::Key::NUM_TWO));
        key_table.add_pair("NUM_THREE", static_cast<uint32_t>(Input::Key::NUM_THREE));
        key_table.add_pair("NUM_FOUR", static_cast<uint32_t>(Input::Key::NUM_FOUR));
        key_table.add_pair("NUM_FIVE", static_cast<uint32_t>(Input::Key::NUM_FIVE));
        key_table.add_pair("NUM_SIX", static_cast<uint32_t>(Input::Key::NUM_SIX));
        key_table.add_pair("NUM_SEVEN", static_cast<uint32_t>(Input::Key::NUM_SEVEN));
        key_table.add_pair("NUM_EIGHT", static_cast<uint32_t>(Input::Key::NUM_EIGHT));
        key_table.add_pair("NUM_NINE", static_cast<uint32_t>(Input::Key::NUM_NINE));
        key_table.add_pair("MULTIPLY", static_cast<uint32_t>(Input::Key::MULTIPLY));
        key_table.add_pair("ADD", static_cast<uint32_t>(Input::Key::ADD));
        key_table.add_pair("SEPARATOR", static_cast<uint32_t>(Input::Key::SEPARATOR));
        key_table.add_pair("SUBTRACT", static_cast<uint32_t>(Input::Key::SUBTRACT));
        key_table.add_pair("DECIMAL", static_cast<uint32_t>(Input::Key::DECIMAL));
        key_table.add_pair("DIVIDE", static_cast<uint32_t>(Input::Key::DIVIDE));
        key_table.add_pair("F1", static_cast<uint32_t>(Input::Key::F1));
        key_table.add_pair("F2", static_cast<uint32_t>(Input::Key::F2));
        key_table.add_pair("F3", static_cast<uint32_t>(Input::Key::F3));
        key_table.add_pair("F4", static_cast<uint32_t>(Input::Key::F4));
        key_table.add_pair("F5", static_cast<uint32_t>(Input::Key::F5));
        key_table.add_pair("F6", static_cast<uint32_t>(Input::Key::F6));
        key_table.add_pair("F7", static_cast<uint32_t>(Input::Key::F7));
        key_table.add_pair("F8", static_cast<uint32_t>(Input::Key::F8));
        key_table.add_pair("F9", static_cast<uint32_t>(Input::Key::F9));
        key_table.add_pair("F10", static_cast<uint32_t>(Input::Key::F10));
        key_table.add_pair("F11", static_cast<uint32_t>(Input::Key::F11));
        key_table.add_pair("F12", static_cast<uint32_t>(Input::Key::F12));
        key_table.add_pair("F13", static_cast<uint32_t>(Input::Key::F13));
        key_table.add_pair("F14", static_cast<uint32_t>(Input::Key::F14));
        key_table.add_pair("F15", static_cast<uint32_t>(Input::Key::F15));
        key_table.add_pair("F16", static_cast<uint32_t>(Input::Key::F16));
        key_table.add_pair("F17", static_cast<uint32_t>(Input::Key::F17));
        key_table.add_pair("F18", static_cast<uint32_t>(Input::Key::F18));
        key_table.add_pair("F19", static_cast<uint32_t>(Input::Key::F19));
        key_table.add_pair("F20", static_cast<uint32_t>(Input::Key::F20));
        key_table.add_pair("F21", static_cast<uint32_t>(Input::Key::F21));
        key_table.add_pair("F22", static_cast<uint32_t>(Input::Key::F22));
        key_table.add_pair("F23", static_cast<uint32_t>(Input::Key::F23));
        key_table.add_pair("F24", static_cast<uint32_t>(Input::Key::F24));
        key_table.add_pair("NUM_LOCK", static_cast<uint32_t>(Input::Key::NUM_LOCK));
        key_table.add_pair("SCROLL_LOCK", static_cast<uint32_t>(Input::Key::SCROLL_LOCK));
        key_table.add_pair("BROWSER_BACK", static_cast<uint32_t>(Input::Key::BROWSER_BACK));
        key_table.add_pair("BROWSER_FORWARD", static_cast<uint32_t>(Input::Key::BROWSER_FORWARD));
        key_table.add_pair("BROWSER_REFRESH", static_cast<uint32_t>(Input::Key::BROWSER_REFRESH));
        key_table.add_pair("BROWSER_STOP", static_cast<uint32_t>(Input::Key::BROWSER_STOP));
        key_table.add_pair("BROWSER_SEARCH", static_cast<uint32_t>(Input::Key::BROWSER_SEARCH));
        key_table.add_pair("BROWSER_FAVORITES", static_cast<uint32_t>(Input::Key::BROWSER_FAVORITES));
        key_table.add_pair("BROWSER_HOME", static_cast<uint32_t>(Input::Key::BROWSER_HOME));
        key_table.add_pair("VOLUME_MUTE", static_cast<uint32_t>(Input::Key::VOLUME_MUTE));
        key_table.add_pair("VOLUME_DOWN", static_cast<uint32_t>(Input::Key::VOLUME_DOWN));
        key_table.add_pair("VOLUME_UP", static_cast<uint32_t>(Input::Key::VOLUME_UP));
        key_table.add_pair("MEDIA_NEXT_TRACK", static_cast<uint32_t>(Input::Key::MEDIA_NEXT_TRACK));
        key_table.add_pair("MEDIA_PREV_TRACK", static_cast<uint32_t>(Input::Key::MEDIA_PREV_TRACK));
        key_table.add_pair("MEDIA_STOP", static_cast<uint32_t>(Input::Key::MEDIA_STOP));
        key_table.add_pair("MEDIA_PLAY_PAUSE", static_cast<uint32_t>(Input::Key::MEDIA_PLAY_PAUSE));
        key_table.add_pair("LAUNCH_MAIL", static_cast<uint32_t>(Input::Key::LAUNCH_MAIL));
        key_table.add_pair("LAUNCH_MEDIA_SELECT", static_cast<uint32_t>(Input::Key::LAUNCH_MEDIA_SELECT));
        key_table.add_pair("LAUNCH_APP1", static_cast<uint32_t>(Input::Key::LAUNCH_APP1));
        key_table.add_pair("LAUNCH_APP2", static_cast<uint32_t>(Input::Key::LAUNCH_APP2));
        key_table.add_pair("OEM_ONE", static_cast<uint32_t>(Input::Key::OEM_ONE));
        key_table.add_pair("OEM_PLUS", static_cast<uint32_t>(Input::Key::OEM_PLUS));
        key_table.add_pair("OEM_COMMA", static_cast<uint32_t>(Input::Key::OEM_COMMA));
        key_table.add_pair("OEM_MINUS", static_cast<uint32_t>(Input::Key::OEM_MINUS));
        key_table.add_pair("OEM_PERIOD", static_cast<uint32_t>(Input::Key::OEM_PERIOD));
        key_table.add_pair("OEM_TWO", static_cast<uint32_t>(Input::Key::OEM_TWO));
        key_table.add_pair("OEM_THREE", static_cast<uint32_t>(Input::Key::OEM_THREE));
        key_table.add_pair("OEM_FOUR", static_cast<uint32_t>(Input::Key::OEM_FOUR));
        key_table.add_pair("OEM_FIVE", static_cast<uint32_t>(Input::Key::OEM_FIVE));
        key_table.add_pair("OEM_SIX", static_cast<uint32_t>(Input::Key::OEM_SIX));
        key_table.add_pair("OEM_SEVEN", static_cast<uint32_t>(Input::Key::OEM_SEVEN));
        key_table.add_pair("OEM_EIGHT", static_cast<uint32_t>(Input::Key::OEM_EIGHT));
        key_table.add_pair("OEM_102", static_cast<uint32_t>(Input::Key::OEM_102));
        key_table.add_pair("IME_PROCESS", static_cast<uint32_t>(Input::Key::IME_PROCESS));
        key_table.add_pair("PACKET", static_cast<uint32_t>(Input::Key::PACKET));
        key_table.add_pair("ATTN", static_cast<uint32_t>(Input::Key::ATTN));
        key_table.add_pair("CRSEL", static_cast<uint32_t>(Input::Key::CRSEL));
        key_table.add_pair("EXSEL", static_cast<uint32_t>(Input::Key::EXSEL));
        key_table.add_pair("EREOF", static_cast<uint32_t>(Input::Key::EREOF));
        key_table.add_pair("PLAY", static_cast<uint32_t>(Input::Key::PLAY));
        key_table.add_pair("ZOOM", static_cast<uint32_t>(Input::Key::ZOOM));
        key_table.add_pair("PA1", static_cast<uint32_t>(Input::Key::PA1));
        key_table.add_pair("OEM_CLEAR", static_cast<uint32_t>(Input::Key::OEM_CLEAR));
        key_table.make_global("Key");

        LuaMadeSimple::Lua::Table modifier_key_table = lua.prepare_new_table();
        modifier_key_table.add_pair("SHIFT", 0x10);
        modifier_key_table.add_pair("CONTROL", 0x11);
        modifier_key_table.add_pair("ALT", 0x12);
        /*modifier_key_table.add_pair("LEFT_SHIFT", 0xA0);
        modifier_key_table.add_pair("RIGHT_SHIFT", 0xA1);
        modifier_key_table.add_pair("LEFT_CONTROL", 0xA2);
        modifier_key_table.add_pair("RIGHT_CONTROL", 0xA3);
        modifier_key_table.add_pair("LEFT_ALT", 0xA4);
        modifier_key_table.add_pair("RIGHT_ALT", 0xA5);*/
        modifier_key_table.make_global("ModifierKey");
    }

    static auto register_object_flags(const LuaMadeSimple::Lua& lua) -> void
    {
        LuaMadeSimple::Lua::Table object_flags_table = lua.prepare_new_table();
        object_flags_table.add_pair("RF_NoFlags", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NoFlags));
        object_flags_table.add_pair("RF_Public", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_Public));
        object_flags_table.add_pair("RF_Standalone", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_Standalone));
        object_flags_table.add_pair("RF_MarkAsNative", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_MarkAsNative));
        object_flags_table.add_pair("RF_Transactional", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_Transactional));
        object_flags_table.add_pair("RF_ClassDefaultObject",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_ClassDefaultObject));
        object_flags_table.add_pair("RF_ArchetypeObject", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_ArchetypeObject));
        object_flags_table.add_pair("RF_Transient", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_Transient));
        object_flags_table.add_pair("RF_MarkAsRootSet", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_MarkAsRootSet));
        object_flags_table.add_pair("RF_TagGarbageTemp", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_TagGarbageTemp));
        object_flags_table.add_pair("RF_NeedInitialization",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NeedInitialization));
        object_flags_table.add_pair("RF_NeedLoad", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NeedLoad));
        object_flags_table.add_pair("RF_KeepForCooker", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_KeepForCooker));
        object_flags_table.add_pair("RF_NeedPostLoad", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NeedPostLoad));
        object_flags_table.add_pair("RF_NeedPostLoadSubobjects",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NeedPostLoadSubobjects));
        object_flags_table.add_pair("RF_NewerVersionExists",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NewerVersionExists));
        object_flags_table.add_pair("RF_BeginDestroyed", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_BeginDestroyed));
        object_flags_table.add_pair("RF_FinishDestroyed", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_FinishDestroyed));
        object_flags_table.add_pair("RF_BeingRegenerated", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_BeingRegenerated));
        object_flags_table.add_pair("RF_DefaultSubObject", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_DefaultSubObject));
        object_flags_table.add_pair("RF_WasLoaded", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_WasLoaded));
        object_flags_table.add_pair("RF_TextExportTransient",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_TextExportTransient));
        object_flags_table.add_pair("RF_LoadCompleted", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_LoadCompleted));
        object_flags_table.add_pair("RF_InheritableComponentTemplate",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_InheritableComponentTemplate));
        object_flags_table.add_pair("RF_DuplicateTransient",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_DuplicateTransient));
        object_flags_table.add_pair("RF_StrongRefOnFrame", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_StrongRefOnFrame));
        object_flags_table.add_pair("RF_NonPIEDuplicateTransient",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_NonPIEDuplicateTransient));
        object_flags_table.add_pair("RF_Dynamic", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_Dynamic));
        object_flags_table.add_pair("RF_WillBeLoaded", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_WillBeLoaded));
        object_flags_table.add_pair("RF_HasExternalPackage",
                                    static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_HasExternalPackage));
        object_flags_table.add_pair("RF_AllFlags", static_cast<std::underlying_type_t<Unreal::EObjectFlags>>(Unreal::EObjectFlags::RF_AllFlags));
        object_flags_table.make_global("EObjectFlags");

        LuaMadeSimple::Lua::Table object_internal_flags_table = lua.prepare_new_table();
        object_internal_flags_table.add_pair("ReachableInCluster",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::ReachableInCluster));
        object_internal_flags_table.add_pair("ClusterRoot",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::ClusterRoot));
        object_internal_flags_table.add_pair("Native", static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::Native));
        object_internal_flags_table.add_pair("Async", static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::Async));
        object_internal_flags_table.add_pair("AsyncLoading",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::AsyncLoading));
        object_internal_flags_table.add_pair("Unreachable",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::Unreachable));
        object_internal_flags_table.add_pair("PendingKill",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::PendingKill));
        object_internal_flags_table.add_pair("RootSet", static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::RootSet));
        object_internal_flags_table.add_pair("GarbageCollectionKeepFlags",
                                             static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(
                                                     Unreal::EInternalObjectFlags::GarbageCollectionKeepFlags));
        object_internal_flags_table.add_pair("AllFlags", static_cast<std::underlying_type_t<Unreal::EInternalObjectFlags>>(Unreal::EInternalObjectFlags::AllFlags));
        object_internal_flags_table.make_global("EInternalObjectFlags");
    }

    static auto register_efindname(const LuaMadeSimple::Lua& lua) -> void
    {
        LuaMadeSimple::Lua::Table efindname_table = lua.prepare_new_table();
        efindname_table.add_pair("FNAME_Find", static_cast<std::underlying_type_t<Unreal::EFindName>>(Unreal::EFindName::FNAME_Find));
        efindname_table.add_pair("FNAME_Add", static_cast<std::underlying_type_t<Unreal::EFindName>>(Unreal::EFindName::FNAME_Add));
        efindname_table.add_pair("FNAME_Replace_Not_Safe_For_Threading",
                                 static_cast<std::underlying_type_t<Unreal::EFindName>>(Unreal::EFindName::FNAME_Replace_Not_Safe_For_Threading));
        efindname_table.make_global("EFindName");
    }

    // BEGIN native Lua buffer helpers
    namespace
    {
        auto native_lua_push_text_call(lua_State* L) -> int
        {
            const auto* text = static_cast<const std::string_view*>(lua_touserdata(L, 1));
            lua_pushlstring(L, text->data(), text->size());
            return 1;
        }

        auto native_lua_push_text(lua_State* L, std::string_view text) -> int
        {
            // Zero-upvalue C functions and light userdata do not allocate.
            // Protect the string allocation so callers' C++ owners unwind
            // normally even when Lua reports an out-of-memory error.
            lua_pushcfunction(L, native_lua_push_text_call);
            lua_pushlightuserdata(L, &text);
            return lua_pcall(L, 1, 1, 0);
        }

        struct NativeLuaReader
        {
            std::string_view bytes;
            bool initial_newline;
        };

        auto native_lua_reader(lua_State*, void* context, size_t* size) -> const char*
        {
            auto& reader = *static_cast<NativeLuaReader*>(context);
            if (reader.initial_newline)
            {
                reader.initial_newline = false;
                *size = 1;
                return "\n";
            }
            *size = reader.bytes.size();
            const char* bytes = reader.bytes.data();
            reader.bytes = {};
            return *size ? bytes : nullptr;
        }

        auto native_lua_chunk(lua_State* L, std::string_view bytes, const char* chunk, const char* mode = nullptr) -> int
        {
            // Match luaL_loadfilex's BOM/shebang and source-line behavior. A
            // binary chunk must begin with its signature, without the newline.
            if (bytes.starts_with("\xef\xbb\xbf")) bytes.remove_prefix(3);
            bool newline = false;
            if (!bytes.empty() && bytes.front() == '#')
            {
                const auto end = bytes.find('\n');
                bytes = end == bytes.npos ? std::string_view{} : bytes.substr(end + 1);
                newline = bytes.empty() || bytes.front() != LUA_SIGNATURE[0];
            }
            NativeLuaReader reader{bytes, newline};
            // lua_load protects parser failures internally. The reader is POD;
            // the caller retains the NativeFileBuffer until this returns.
            return lua_load(L, native_lua_reader, &reader, chunk, mode);
        }

        auto native_lua_file(lua_State* L, const Platform::NativeModSource* source, std::string_view scripts,
                             const char* filename, size_t filename_size, const char* mode) -> int
        {
            // Errors are copied out before pushing Lua values: no C++ owner
            // survives a possible Lua allocation error or a later lua_error.
            char error[4608]{};
            {
                try
                {
                    if (!source) throw std::invalid_argument("native Lua source is unavailable");
                    if (!filename) throw std::invalid_argument("stdin is unavailable in native memory mode");
                    if (!filename_size || filename_size > 4096) throw std::invalid_argument("native Lua filename is empty or too long");
                    const std::string_view name(filename, filename_size);
                    // Relative names use this mod's Scripts/scripts directory.
                    // Canonical ue4ss/... keys are exact; source.read rejects
                    // absolute paths, parent components, NUL and Windows paths.
                    const std::string path = name.starts_with("ue4ss/") ? std::string(name) : std::string(scripts) + "/" + std::string(name);
                    const auto buffer = source->read(path);
                    if (buffer)
                    {
                        const std::string chunk = "@" + path;
                        return native_lua_chunk(L, buffer->bytes, chunk.c_str(), mode);
                    }
                    std::snprintf(error, sizeof(error), "cannot open native Lua file '%.*s': file is not retained",
                                  static_cast<int>(name.size()), name.data());
                }
                catch (const std::exception& exception)
                {
                    std::snprintf(error, sizeof(error), "cannot open native Lua file: %s", exception.what());
                }
                catch (void*)
                {
                    // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
                    // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
                    throw;
                }
                catch (...)
                {
                    std::snprintf(error, sizeof(error), "cannot open native Lua file: source lookup failed");
                }
            }
            lua_pushstring(L, error);
            return LUA_ERRFILE;
        }

        auto native_lua_loadfile(lua_State* L) -> int
        {
            size_t filename_size = 0, scripts_size = 0;
            const char* filename = luaL_optlstring(L, 1, nullptr, &filename_size);
            const char* mode = luaL_optstring(L, 2, nullptr);
            const int environment = lua_isnone(L, 3) ? 0 : 3;
            const auto* source = static_cast<const Platform::NativeModSource*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* scripts = lua_tolstring(L, lua_upvalueindex(2), &scripts_size);
            const int status = native_lua_file(L, source, std::string_view(scripts, scripts_size), filename, filename_size, mode);
            if (status == LUA_OK)
            {
                if (environment)
                {
                    lua_pushvalue(L, environment);
                    if (!lua_setupvalue(L, -2, 1)) lua_pop(L, 1);
                }
                return 1;
            }
            luaL_pushfail(L);
            lua_insert(L, -2);
            return 2;
        }

        auto native_lua_dofile_continue(lua_State* L, int, lua_KContext) -> int
        {
            return lua_gettop(L) - 1;
        }

        auto native_lua_dofile(lua_State* L) -> int
        {
            size_t filename_size = 0, scripts_size = 0;
            const char* filename = luaL_optlstring(L, 1, nullptr, &filename_size);
            lua_settop(L, 1);
            const auto* source = static_cast<const Platform::NativeModSource*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* scripts = lua_tolstring(L, lua_upvalueindex(2), &scripts_size);
            if (native_lua_file(L, source, std::string_view(scripts, scripts_size), filename, filename_size, nullptr) != LUA_OK)
                return lua_error(L);
            // Only borrowed pointers/POD remain. Yield and runtime errors can
            // cross this frame without bypassing C++ owned-buffer destructors.
            lua_callk(L, 0, LUA_MULTRET, 0, native_lua_dofile_continue);
            return native_lua_dofile_continue(L, 0, 0);
        }

        // Return -1 with a Lua error on the stack for a malformed existing
        // module. The caller raises it after these C++ owners have unwound.
        auto native_lua_module(lua_State* L, const Platform::NativeModSource& source, const std::string& scripts_path) -> int
        {
            if (lua_type(L, 1) != LUA_TSTRING)
            {
                return native_lua_push_text(L, "module name must be a string") == LUA_OK ? 1 : -1;
            }
            size_t name_size = 0;
            const char* name = lua_tolstring(L, 1, &name_size);
            std::string relative(name, name_size);
            bool valid = !relative.empty() && relative.size() <= 4096;
            for (char& c : relative)
            {
                const auto byte = static_cast<unsigned char>(c);
                if (byte < 0x20 || byte == 0x7f || c == '\\' || c == ':') valid = false;
                if (c == '.') c = '/';
            }
            if (!valid || relative.front() == '/' || relative.back() == '/' || relative.find("//") != relative.npos)
            {
                return native_lua_push_text(L, "invalid native Lua module name") == LUA_OK ? 1 : -1;
            }
            const std::string shared = "ue4ss/Mods/shared/";
            const std::vector<std::string> paths{
                scripts_path + "/" + relative + ".lua",
                scripts_path + "/" + relative + "/init.lua",
                shared + relative + ".lua",
                shared + relative + "/init.lua",
                shared + relative + "/" + relative + ".lua",
            };
            std::string missing;
            for (const auto& path : paths)
            {
                const auto buffer = source.read(path);
                if (!buffer)
                {
                    missing += "\n\tno native Lua file '" + path + "'";
                    continue;
                }
                const std::string chunk = "@" + path;
                // NativeFileBuffer holds the immutable owner through parsing.
                // Return the chunk directly: Lua require supplies module name
                // and loader data, and owns standard package.loaded caching.
                if (native_lua_chunk(L, buffer->bytes, chunk.c_str()) != LUA_OK) return -1;
                return native_lua_push_text(L, path) == LUA_OK ? 2 : -1;
            }
            return native_lua_push_text(L, missing) == LUA_OK ? 1 : -1;
        }
    }
    // END native Lua buffer helpers

    LuaMod::LuaMod(UE4SSProgram& program, StringType&& mod_name, StringType&& mod_path)
        : Mod(program, std::move(mod_name), std::move(mod_path))
#if defined(__PROSPERO__)
        , m_native_source(program.native_mod_source())
#endif
        , m_lua(LuaMadeSimple::new_state())
    {
        if (m_native_source)
        {
            const auto path = modbridge::normalize_path_for_lua(m_mod_path);
            const auto& mods = m_native_source->lua_mods();
            const auto found = std::find_if(mods.begin(), mods.end(), [&](const auto& mod) { return mod.path == path; });
            if (found == mods.end())
            {
                Output::send<LogLevel::Error>(STR("Mod is absent from native Lua source: {}\n"), ensure_str(m_mod_path));
                set_installable(false);
                return;
            }
            m_scripts_path = std::filesystem::u8path(found->scripts_path);
            return;
        }
        // First check for "Scripts" (capital S)
        std::filesystem::path scripts_path = m_mod_path / STR("Scripts");

        // If not found, try with lowercase "scripts"
        if (!std::filesystem::exists(scripts_path))
        {
            std::filesystem::path alt_scripts_path = m_mod_path / STR("scripts");
            if (std::filesystem::exists(alt_scripts_path))
            {
                scripts_path = alt_scripts_path;
            }
        }

        m_scripts_path = scripts_path;

        if (!std::filesystem::exists(m_scripts_path))
        {
            Output::send<LogLevel::Error>(STR("Mod path doesn't exist {}\n"), ensure_str(m_scripts_path));
            set_installable(false);
            return;
        }
    }

    auto LuaMod::global_uninstall() -> void
    {
    }

    static bool is_first_mod_to_start = true;
    template <typename PropertyType>
    auto add_property_type_table(const LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua::Table& property_types_table, std::string_view property_type_name) -> void
    {
        property_types_table.add_key(property_type_name.data());

        auto property_type_table = lua.prepare_new_table();
        property_type_table.add_pair("Name", property_type_name.data());

        try
        {
            if (is_first_mod_to_start)
            {
                Output::send(STR("[Lua] Exposing size of '{}' as 0x{:X}\n"), ensure_str(typeid(PropertyType).name()), PropertyType::StaticValueSize());
            }
            property_type_table.add_pair("Size", PropertyType::StaticValueSize());
        }
        catch (std::runtime_error&)
        {
            if (is_first_mod_to_start)
            {
                Output::send<LogLevel::Warning>(
                        STR("[Lua] Failed to expose size of '{}', internal value invalid, manually set this value in your Lua mod if you intend to use it.\n"),
                        ensure_str(typeid(PropertyType).name()));
            }
            property_type_table.add_pair("Size", 0);
        }

        // property_type_table.add_pair("Size", PropertyType::size);
        //  This should be a lightuserdata instead of a reinterpret_cast to int64_t
        //  This is not very safe at all, what if the pointer is too large for a signed 64-bit integer ?
        property_type_table.add_pair("FFieldClassPointer", static_cast<int64_t>(PropertyType::StaticClass().HashObject()));
        // TODO: Figure out if the static object pointer is needed
        property_type_table.add_pair("StaticPointer", 0);

        property_types_table.fuse_pair();

        property_type_table.make_local();
    }

    // Private helper: Ensures hook thread exists and returns the registry reference (or LUA_REFNIL if already exists)
    static int ensure_hook_thread_exists(LuaMod* mod)
    {
        if (mod->m_hook_lua == nullptr)
        {
            // First use - create new thread and anchor it in the registry
            mod->m_hook_lua = &mod->lua().new_thread();
            int thread_ref = luaL_ref(mod->lua().get_lua_state(), LUA_REGISTRYINDEX);
            return thread_ref;
        }

        // Thread already exists and is already anchored
        return LUA_REFNIL;
    }

    // Returns the hook lua thread for immediate use (doesn't need registry reference management)
    auto static get_hook_lua(LuaMod* mod) -> LuaMadeSimple::Lua*
    {
        ensure_hook_thread_exists(mod);
        return mod->m_hook_lua;
    }

    // Returns hook state with registry reference (for persistent hooks that need cleanup)
    // auto static make_hook_state(Mod* mod, const LuaMadeSimple::Lua& lua)->std::shared_ptr<LuaMadeSimple::Lua>
    auto static make_hook_state(LuaMod* mod) -> std::pair<LuaMadeSimple::Lua*, int>
    {
        int thread_ref = ensure_hook_thread_exists(mod);
        return {mod->m_hook_lua, thread_ref};

        // Make the hook thread (which is just a separate Lua stack) be a global in its parent.
        // This is needed because otherwise it will be GCd when we don't want it to.
        // lua_setglobal(lua.get_lua_state(), "HookThread");

        // Commenting out until we switch to lua_newstate instead of lua_newthread.
        // For the switch to happen, we need to be able to move or copy Lua types across lua_states which we can't do yet.
        /*
        mod->m_hook_lua->register_function("RegisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.throw_error("'RegisterHook' is not allowed from the game thread");
            return 0;
        });

        mod->m_hook_lua->register_function("NotifyOnNewObject", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.throw_error("'NotifyOnNewObject' is not allowed in the game thread");
            return 0;
        });
        //*/
        //}
    }

    auto static make_async_state(LuaMod* mod, const LuaMadeSimple::Lua& lua) -> void
    {
        if (!mod->m_async_lua)
        {
            mod->m_async_lua = &lua.new_thread();

            // Make the hook thread (which is just a separate Lua stack) be a global in its parent.
            // This is needed because otherwise it will be GCd when we don't want it to.
            lua_setglobal(lua.get_lua_state(), "AsyncThread");

            // Commenting out until we switch to lua_newstate instead of lua_newthread.
            // For the switch to happen, we need to be able to move or copy Lua types across lua_states which we can't do yet.
            /*
            mod->m_hook_lua->register_function("RegisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.throw_error("'RegisterHook' is not allowed from the game thread");
                return 0;
            });

            mod->m_hook_lua->register_function("NotifyOnNewObject", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.throw_error("'NotifyOnNewObject' is not allowed in the game thread");
                return 0;
            });
            //*/
        }
    }

    auto static make_main_state(LuaMod* mod, const LuaMadeSimple::Lua& lua) -> void
    {
        if (!mod->m_main_lua)
        {
            mod->m_main_lua = &lua.new_thread();

            // Make the hook thread (which is just a separate Lua stack) be a global in its parent.
            // This is needed because otherwise it will be GCd when we don't want it to.
            lua_setglobal(lua.get_lua_state(), "MainThread");

            // Commenting out until we switch to lua_newstate instead of lua_newthread.
            // For the switch to happen, we need to be able to move or copy Lua types across lua_states which we can't do yet.
            /*
            mod->m_hook_lua->register_function("RegisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.throw_error("'RegisterHook' is not allowed from the game thread");
                return 0;
            });

            mod->m_hook_lua->register_function("NotifyOnNewObject", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.throw_error("'NotifyOnNewObject' is not allowed in the game thread");
                return 0;
            });
            //*/
        }
    }
    auto static register_all_property_types(const LuaMadeSimple::Lua& lua) -> void
    {
        auto property_types_table = lua.prepare_new_table();

        add_property_type_table<Unreal::FObjectProperty>(lua, property_types_table, "ObjectProperty");
        add_property_type_table<Unreal::FObjectPtrProperty>(lua, property_types_table, "ObjectPtrProperty");
        add_property_type_table<Unreal::FInt8Property>(lua, property_types_table, "Int8Property");
        add_property_type_table<Unreal::FInt16Property>(lua, property_types_table, "Int16Property");
        add_property_type_table<Unreal::FIntProperty>(lua, property_types_table, "IntProperty");
        add_property_type_table<Unreal::FInt64Property>(lua, property_types_table, "Int64Property");
        add_property_type_table<Unreal::FByteProperty>(lua, property_types_table, "ByteProperty");
        add_property_type_table<Unreal::FUInt16Property>(lua, property_types_table, "UInt16Property");
        add_property_type_table<Unreal::FUInt32Property>(lua, property_types_table, "UInt32Property");
        add_property_type_table<Unreal::FUInt64Property>(lua, property_types_table, "UInt64Property");
        add_property_type_table<Unreal::FNameProperty>(lua, property_types_table, "NameProperty");
        add_property_type_table<Unreal::FFloatProperty>(lua, property_types_table, "FloatProperty");
        // add_property_type_table<Unreal::FStrProperty>(lua, property_types_table, "StrProperty");
        add_property_type_table<Unreal::FBoolProperty>(lua, property_types_table, "BoolProperty");
        add_property_type_table<Unreal::FArrayProperty>(lua, property_types_table, "ArrayProperty");
        add_property_type_table<Unreal::FSetProperty>(lua, property_types_table, "SetProperty");
        add_property_type_table<Unreal::FMapProperty>(lua, property_types_table, "MapProperty");
        add_property_type_table<Unreal::FStructProperty>(lua, property_types_table, "StructProperty");
        add_property_type_table<Unreal::FClassProperty>(lua, property_types_table, "ClassProperty");
        add_property_type_table<Unreal::FWeakObjectProperty>(lua, property_types_table, "WeakObjectProperty");
        if (Unreal::Version::IsAtLeast(4, 15))
        {
            add_property_type_table<Unreal::FEnumProperty>(lua, property_types_table, "EnumProperty");
        }
        add_property_type_table<Unreal::FTextProperty>(lua, property_types_table, "TextProperty");
        add_property_type_table<Unreal::FStrProperty>(lua, property_types_table, "StrProperty");
        if (Unreal::Version::IsAtLeast(5, 6))
        {
            add_property_type_table<Unreal::FUtf8StrProperty>(lua, property_types_table, "Utf8StrProperty");
        }

        is_first_mod_to_start = false;

        property_types_table.make_global("PropertyTypes");
    }

    auto LuaMod::setup_custom_module_loader(const LuaMadeSimple::Lua* lua_state) -> void
    {
        lua_State* L = lua_state->get_lua_state();
    
        // Initialize ue4ss_loaded_modules table
        lua_newtable(L);
        lua_setglobal(L, "ue4ss_loaded_modules");
    
        // Get package.searchers table
        lua_getglobal(L, "package");
        if (!lua_istable(L, -1))
        {
            Output::send<LogLevel::Error>(STR("package table not found\n"));
            lua_pop(L, 1);
            return;
        }
    
        lua_getfield(L, -1, "searchers");
        if (!lua_istable(L, -1))
        {
            Output::send<LogLevel::Error>(STR("package.searchers table not found\n"));
            lua_pop(L, 2);
            return;
        }
        if (m_native_source)
        {
            // Keep package.preload first, and expose only our buffer searcher.
            // Standard Lua/C file searchers must not fall back to target disk.
            lua_createtable(L, 2, 0);
            lua_rawgeti(L, -2, 1);
            if (!lua_isfunction(L, -1))
            {
                lua_pop(L, 4);
                throw std::runtime_error("Native Lua loader requires package.preload searcher");
            }
            lua_rawseti(L, -2, 1);
            lua_pushlightuserdata(L, this);
            lua_pushcclosure(L, custom_module_searcher, 1);
            lua_rawseti(L, -2, 2);
            lua_setfield(L, -3, "searchers");
            lua_pop(L, 2);
            // File closures borrow this LuaMod's retained source. Their path
            // upvalue is Lua-owned and remains valid across execution/yield.
            const std::string* scripts = nullptr;
            {
                const auto path = modbridge::normalize_path_for_lua(m_mod_path);
                for (const auto& mod : m_native_source->lua_mods())
                    if (mod.path == path) { scripts = &mod.scripts_path; break; }
            }
            if (!scripts) throw std::runtime_error("Native Lua mod scripts path is unavailable");
            constexpr luaL_Reg file_functions[]{{"loadfile", native_lua_loadfile}, {"dofile", native_lua_dofile}};
            for (const auto& entry : file_functions)
            {
                lua_pushlightuserdata(L, const_cast<Platform::NativeModSource*>(m_native_source.get()));
                lua_pushlstring(L, scripts->data(), scripts->size());
                lua_pushcclosure(L, entry.func, 2);
                lua_setglobal(L, entry.name);
            }
            return;
        }

        // Insert our searcher at position 1
        // First, shift existing searchers up
        lua_Integer len = luaL_len(L, -1);
        for (lua_Integer i = len; i >= 1; i--)
        {
            lua_geti(L, -1, i);     // Get searchers[i]
            lua_seti(L, -2, i + 1); // Set searchers[i+1] = searchers[i]
        }
    
        // Push the LuaMod instance as a light userdata (our upvalue)
        lua_pushlightuserdata(L, this);
    
        // Create the C closure with one upvalue
        lua_pushcclosure(L, custom_module_searcher, 1);
    
        // Set searchers[1] = our new closure
        lua_seti(L, -2, 1);
    
        lua_pop(L, 2); // Clean up stack: searchers, package
    }

    // Static C function for the module searcher
    int LuaMod::custom_module_searcher(lua_State* L)
    {
        auto* native_mod = static_cast<LuaMod*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (native_mod && native_mod->m_native_source)
        {
            int result = -1;
            try
            {
                result = native_lua_module(L, *native_mod->m_native_source, modbridge::normalize_path_for_lua(native_mod->m_scripts_path));
            }
            catch (const std::exception& error)
            {
                native_lua_push_text(L, error.what());
            }
            catch (void*)
            {
                // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
                // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
                throw;
            }
            catch (...)
            {
                native_lua_push_text(L, "Native Lua module loading failed");
            }
            // No C++ automatic owners are alive across Lua's error longjmp.
            return result < 0 ? lua_error(L) : result;
        }
        const char* module_name = luaL_checkstring(L, 1);
        if (!module_name)
        {
            lua_pushstring(L, "module name is required");
            return 1;
        }
        
        // Get the LuaMod* from the upvalue at index 1
        auto* lua_mod = static_cast<LuaMod*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!lua_mod)
        {
            lua_pushstring(L, "custom searcher is missing its C++ context");
            return 1;
        }
        
        // Get paths directly from the C++ object and convert to UTF-8 strings for Lua
        const auto& mods_dir = lua_mod->get_program().get_mods_directory();
        const auto& mod_name = lua_mod->get_name();
        const auto& scripts_path = lua_mod->get_scripts_path();
        
        std::string mods_path_str = modbridge::normalize_path_for_lua(mods_dir);
        std::string mod_name_str = to_utf8_string(mod_name);
        std::string scripts_path_str = modbridge::normalize_path_for_lua(scripts_path);
        
        // Try different path combinations
        std::vector<std::string> paths_to_try = {
            scripts_path_str + "/" + module_name + ".lua",
            mods_path_str + "/shared/" + module_name + ".lua",
            mods_path_str + "/shared/" + module_name + "/" + module_name + ".lua"
        };
        
        // Try each path
        std::string attempted_paths_str;
        for (const auto& path : paths_to_try)
        {
            // Convert to wide string for Windows filesystem operations
            std::wstring wide_path;
            try
            {
                wide_path = utf8_to_wpath(path);
            }
            catch (const std::exception&)
            {
                attempted_paths_str += "\n\t" + path + " (encoding error)";
                continue;
            }
            
            if (!std::filesystem::exists(wide_path))
            {
                attempted_paths_str += "\n\t" + path;
                continue;
            }
            
            // Check if already loaded
            lua_getglobal(L, "ue4ss_loaded_modules");
            lua_pushstring(L, path.c_str());
            lua_gettable(L, -2);
            
            if (!lua_isnil(L, -1))
            {
                // Already loaded, return the cached function
                lua_remove(L, -2); // Remove ue4ss_loaded_modules table
                return 1;
            }
            
            lua_pop(L, 2); // Pop nil and ue4ss_loaded_modules
            
            // Try to load the file
            std::ifstream file(wide_path, std::ios::binary);
            if (!file.is_open())
            {
                attempted_paths_str += "\n\t" + path + " (cannot open)";
                continue;
            }
            
            // Get file size and read content
            file.seekg(0, std::ios::end);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size))
            {
                attempted_paths_str += "\n\t" + path + " (read error)";
                continue;
            }
            file.close();
            
            // Create chunk name for debugging
            std::string chunk_name = "@" + path;
            
            // Load the script as a function that returns the module
            std::string module_wrapper = "return function()\n" + std::string(buffer.data(), buffer.size()) + "\nend";
            
            if (luaL_loadbuffer(L, module_wrapper.c_str(), module_wrapper.size(), chunk_name.c_str()) != LUA_OK)
            {
                attempted_paths_str += "\n\t" + path + " (syntax error: " + lua_tostring(L, -1) + ")";
                lua_pop(L, 1); // Pop error message
                continue;
            }
            
            // Execute to get the loader function
            if (lua_pcall(L, 0, 1, 0) != LUA_OK)
            {
                attempted_paths_str += "\n\t" + path + " (execution error: " + lua_tostring(L, -1) + ")";
                lua_pop(L, 1); // Pop error message
                continue;
            }
            
            // Cache the loaded module
            lua_getglobal(L, "ue4ss_loaded_modules");
            lua_pushstring(L, path.c_str());
            lua_pushvalue(L, -3); // Copy the function
            lua_settable(L, -3);
            lua_pop(L, 1); // Pop ue4ss_loaded_modules
            
            // Return the loader function
            return 1;
        }
        
        // Module not found
        std::string error_msg = "module '" + std::string(module_name) + "' not found:" + attempted_paths_str;
        lua_pushstring(L, error_msg.c_str());
        return 1;
    }

    auto LuaMod::setup_lua_require_paths(const LuaMadeSimple::Lua& lua) const -> void
    {
        try
        {
            auto* lua_state = m_lua.get_lua_state();
            lua_getglobal(lua_state, "package");

            if (m_native_source)
            {
                lua_pushliteral(lua_state, "");
                lua_setfield(lua_state, -2, "path");
                lua_pushliteral(lua_state, "");
                lua_setfield(lua_state, -2, "cpath");
                lua_pop(lua_state, 1);
                return;
            }

            // Get current paths
            lua_getfield(lua_state, -1, "path");
            std::string current_paths = lua_tostring(lua_state, -1);
            lua_pop(lua_state, 1);

            auto mods_dir = m_program.get_mods_directory();
            auto mod_name = get_name();

            // Create normalized UTF-8 paths with forward slashes
            std::string mods_dir_utf8 = modbridge::normalize_path_for_lua(mods_dir);
            std::string mod_name_utf8 = to_utf8_string(mod_name);
            std::string scripts_path_utf8 = modbridge::normalize_path_for_lua(m_scripts_path);

            // Create path strings with forward slashes for Lua
            std::string script_path = fmt::format(";{}/?.lua", scripts_path_utf8);
            std::string shared_path = fmt::format(";{}/shared/?.lua", mods_dir_utf8);
            std::string shared_nested_path = fmt::format(";{}/shared/?/?.lua", mods_dir_utf8);

            current_paths.append(script_path);
            current_paths.append(shared_path);
            current_paths.append(shared_nested_path);

            lua_pushstring(lua_state, current_paths.c_str());
            lua_setfield(lua_state, -2, "path");

            // Now set up cpath similarly
            lua_getfield(lua_state, -1, "cpath");
            std::string current_cpaths = lua_tostring(lua_state, -1);
            lua_pop(lua_state, 1);

            // Create cpath strings
            std::string script_dll_path = fmt::format(";{}/?.dll", scripts_path_utf8);
            std::string mod_dll_path = fmt::format(";{}/{}/?/?.dll", mods_dir_utf8, mod_name_utf8);

            current_cpaths.append(script_dll_path);
            current_cpaths.append(mod_dll_path);

            lua_pushstring(lua_state, current_cpaths.c_str());
            lua_setfield(lua_state, -2, "cpath");

            lua_pop(lua_state, 1); // Pop package table
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("Exception in setup_lua_require_paths: {}\n"), ensure_str(e.what()));
            throw;
        }
    }

    auto LuaMod::get_object_names(const Unreal::UObject* object) -> std::vector<Unreal::FName>
    {
        std::vector<Unreal::FName> names{};
        for (auto ptr = object; ptr; ptr = ptr->GetOuterPrivate())
        {
            names.emplace_back(ptr->GetNamePrivate());
        }
        return names;
    }

    auto LuaMod::find_function_hook_data(std::vector<FunctionHookData>& container, Unreal::FName in_name) -> FunctionHookData*
    {
        for (auto it = container.begin(); it != container.end(); ++it)
        {
            if (it->names.size() >= 1 && in_name.Equals(it->names[0]))
            {
                return &*it;
            }
        }
        return nullptr;
    }

    auto LuaMod::find_function_hook_data(std::vector<FunctionHookData>& container, const Unreal::UObject* object) -> FunctionHookData*
    {
        return find_function_hook_data(container, get_object_names(object));
    }

    auto LuaMod::find_function_hook_data(std::vector<FunctionHookData>& container, const std::vector<Unreal::FName>& in_name) -> FunctionHookData*
    {
        for (auto it = container.begin(); it != container.end(); ++it)
        {
            auto found = true;
            if (in_name.size() != it->names.size())
            {
                continue;
            }
            for (std::size_t index = 0; index < it->names.size(); ++index)
            {
                const auto& name = it->names[index];
                if (!name.Equals(in_name[index]))
                {
                    found = false;
                }
            }
            if (found)
            {
                return &*it;
            }
        }
        return nullptr;
    }

    auto LuaMod::remove_function_hook_data(std::vector<FunctionHookData>& container, StringViewType in_name) -> void
    {
        // [L27][ET nul-throw-armor] FIX 1 (Lua→C++ seam): the FName ctor
        // takes const CharType* and scans to NUL — an owned, explicitly-sized
        // copy of the view before construction, so a slice view can never be
        // over-read into the following path text. (In-tree callers currently
        // pass owned StringTypes; this seals the seam itself.)
        remove_function_hook_data(container, Unreal::FName(in_name.empty() ? StringType{} : StringType{in_name.data(), in_name.size()}, Unreal::FNAME_Add));
    }

    auto LuaMod::remove_function_hook_data(std::vector<FunctionHookData>& container, Unreal::FName in_name) -> void
    {
        for (auto it = container.begin(); it != container.end(); ++it)
        {
            if (it->names.size() >= 1 && it->names[0] == in_name)
            {
                container.erase(it);
                break;
            }
        }
    }

    auto LuaMod::remove_function_hook_data(std::vector<FunctionHookData>& container, const Unreal::UObject* object) -> void
    {
        remove_function_hook_data(container, get_object_names(object));
    }

    auto LuaMod::remove_function_hook_data(std::vector<FunctionHookData>& container, const std::vector<Unreal::FName>& in_name) -> void
    {
        for (auto it = container.begin(); it != container.end(); ++it)
        {
            auto found = true;
            if (in_name.size() != it->names.size())
            {
                continue;
            }
            for (std::size_t index = 0; index < it->names.size(); ++index)
            {
                const auto& name = it->names[index];
                if (name != in_name[index])
                {
                    found = false;
                }
            }
            if (found)
            {
                container.erase(it);
            }
        }
    }

    // [CAP05-CRASHTEST-BEGIN]
    // cap05 (crash capture) - deliberate, opt-in crash used ONLY by the cap05
    // launch to prove that a crash is captured and symbolized end to end. Generic:
    // no game, mod or engine names; no detour; no new import; no new include.
    // Nothing happens unless the caller passes the exact confirmation token, so no
    // downloaded mod can trip it by accident (no UE4SS mod knows this function).
    //   "segv": 32-bit store to virtual address 0xC08. Page 0 is never mapped in a
    //           user process, so this always faults (SIGSEGV, fault VA 0xC08) with
    //           RIP inside modbridge_cap05_crashtest_fault.
    //   "trap": __builtin_trap (ud2) -> SIGILL with RIP inside the same function.
    namespace cap05_crashtest
    {
        inline volatile uintptr_t fault_va = 0xC08;   // volatile: the store cannot be proven away
        inline constexpr std::string_view kToken = "CRASH-THE-GAME-NOW";

        [[gnu::noinline]] [[gnu::used]] void modbridge_cap05_crashtest_fault(int kind)
        {
            if (kind == 1)
            {
                *reinterpret_cast<volatile uint32_t*>(fault_va) = 0xC0DE05u;
            }
            else if (kind == 2)
            {
                __builtin_trap();
            }
        }

        // Returns a message when NOTHING happened (refused, or the fault did not
        // fire). For a valid request it does not return: the process crashes.
        inline const char* dispatch(std::string_view kind, std::string_view token)
        {
            if (token != kToken)
                return "ModBridgeCrashTest refused: wrong or missing confirmation token (nothing happened)";
            if (kind == "segv")
            {
                modbridge_cap05_crashtest_fault(1);
                return "ModBridgeCrashTest: the segv store did not fault (unexpected)";
            }
            if (kind == "trap")
            {
                modbridge_cap05_crashtest_fault(2);
                return "ModBridgeCrashTest: the trap did not fire (unexpected)";
            }
            return "ModBridgeCrashTest refused: kind must be \"segv\" or \"trap\" (nothing happened)";
        }
    } // namespace cap05_crashtest
    // [CAP05-CRASHTEST-END]

    auto static setup_lua_global_functions_internal(const LuaMadeSimple::Lua& lua, Mod::IsTrueMod is_true_mod) -> void
    {
        lua.register_function("print", LuaLibrary::global_print);
        lua.register_function("LoadExport", LuaLibrary::load_export);

        lua.register_function("CreateInvalidObject", [](const LuaMadeSimple::Lua& lua) -> int {
            LuaType::auto_construct_object(lua, nullptr);
            return 1;
        });

        lua.register_function("StaticFindObject", [](const LuaMadeSimple::Lua& lua) -> int {
            // Stack size @ the start of the function is the same as the number of params
            int32_t stack_size = lua.get_stack_size();

            if (stack_size <= 0)
            {
                lua.throw_error("Function 'StaticFindObject' cannot be called with 0 parameters.");
            }

            std::string error_overload_not_found{R"(
No overload found for function 'StaticFindObject'.
Overloads:
#1: StaticFindObject(string name)
#2: StaticFindObject(UClass* Class, UObject* InOuter, string name, bool ExactClass = false))"};

            // Overload #1
            // P1: string name
            // Ignores any params after P1
            if (lua.is_string())
            {
                // [LFB] Claude item 2: the ONE hand converter owns this Lua
                // string (lua_tolstring + hand UTF-8->UTF-16; no ensure_str,
                // no std::wstring, no wide libc). The per-part FNAME_Find
                // constructions inside StaticFindObject stay on the engine
                // path (Claude item 9).
                const LuaType::LuaFNameBridge::Bridge name{lua.get_lua_state()};
                Unreal::UObject* object = Unreal::UObjectGlobals::StaticFindObject(nullptr, nullptr, name.view());

                // Construct a Lua object of type 'UObject'
                // Auto constructing is nullptr safe
                LuaType::auto_construct_object(lua, object);

                return 1;
            }

            // Overload #2
            // P1: UClass* Class
            // P2: UObject* InOuter
            // P3: string Name
            // P4: bool ExactClass = false
            // Full definition of StaticFindObject, including default values
            // Ignores any params after P4
            if (stack_size < 3)
            {
                // No overload found for function 'StaticFindObject'. Overloads are:
                lua.throw_error(error_overload_not_found);
            }

            Unreal::UClass* param_class{};
            Unreal::UObject* param_in_outer{};
            StringType param_name{};
            bool param_exact_class{};

            // P1 (Class), userdata
            if (lua.is_userdata())
            {
                auto& lua_object = lua.get_userdata<LuaType::UClass>();
                param_class = lua_object.get_remote_cpp_object();
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            // P2 (InOuter), userdata
            if (lua.is_userdata())
            {
                auto& lua_object = lua.get_userdata<LuaType::UObject>();
                param_in_outer = lua_object.get_remote_cpp_object();
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            // P3 (Name), string
            if (lua.is_string())
            {
                // [LFB] ONE hand converter (same route as overload #1).
                const LuaType::LuaFNameBridge::Bridge name{lua.get_lua_state()};
                param_name = name.string();
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            // P4 (ExactClass), bool = false
            if (lua.is_bool())
            {
                param_exact_class = lua.get_bool();
            }
            // There's no error if P4 isn't a bool, simply ignore all parameters after P3

            Unreal::UObject* object = Unreal::UObjectGlobals::StaticFindObject(param_class, param_in_outer, param_name, param_exact_class);

            // Construct a Lua object of type 'UObject'
            // Auto constructing is nullptr safe
            LuaType::auto_construct_object(lua, object);

            return 1;
        });

        lua.register_function("FindFirstOf", [](const LuaMadeSimple::Lua& lua) -> int {
            // Stack size @ the start of the function is the same as the number of params
            int32_t stack_size = lua.get_stack_size();

            if (stack_size <= 0)
            {
                lua.throw_error("Function 'FindFirstOf' cannot be called with 0 parameters.");
            }

            std::string error_overload_not_found{R"(
No overload found for function 'FindFirstOf'.
Overloads:
#1: FindFirstOf(string short_class_name))"};

            // Overload #1
            // P1: string short_name
            // Ignores any params after P1
            if (lua.is_string())
            {
                // [LFB] Claude items 1-2: the Lua class-name string goes
                // through the ONE hand converter (lua_tolstring + hand
                // UTF-8->UTF-16 -> slot-2 ctor DIRECT, image 0x23D590) and
                // the FName overload of FindFirstOf. Prospero keeps the CP
                // Find-first discipline (Add only on NAME_None); host keeps
                // the upstream default Add semantics.
                const LuaType::LuaFNameBridge::Bridge name{lua.get_lua_state()};
#if defined(__PROSPERO__)
                Unreal::FName class_name = name.to_fname(Unreal::FNAME_Find);
                if (class_name.IsNone())
                {
                    class_name = name.to_fname(Unreal::FNAME_Add);
                }
#else
                const Unreal::FName class_name = name.to_fname(Unreal::FNAME_Add);
#endif
                Unreal::UObject* object = Unreal::UObjectGlobals::FindFirstOf(class_name);

                // Construct a Lua object of type 'UObject'
                // Auto constructing is nullptr safe
                LuaType::auto_construct_object(lua, object);

                return 1;
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            return 0;
        });

        lua.register_function("FindAllOf", [](const LuaMadeSimple::Lua& lua) -> int {
            // Stack size @ the start of the function is the same as the number of params
            int32_t stack_size = lua.get_stack_size();

            if (stack_size <= 0)
            {
                lua.throw_error("Function 'FindAllOf' cannot be called with 0 parameters.");
            }

            std::string error_overload_not_found{R"(
No overload found for function 'FindAllOf'.
Overloads:
#1: FindAllOf(string short_class_name))"};

            // Overload #1
            // P1: string short_name
            // Ignores any params after P1
            if (lua.is_string())
            {
                constexpr int32_t elements_to_reserve = 40;

                std::vector<Unreal::UObject*> found_unreal_objects;

                // Reserving some space because FindAllOf is likely to find lots of objects
                found_unreal_objects.reserve(elements_to_reserve);

                // [LFB] Claude items 1-2: same ONE hand converter route as
                // FindFirstOf (direct slot-2 ctor, Prospero Find-first +
                // Add-fallback, host Add), then the FName overload walk.
                const LuaType::LuaFNameBridge::Bridge name{lua.get_lua_state()};
#if defined(__PROSPERO__)
                Unreal::FName class_name = name.to_fname(Unreal::FNAME_Find);
                if (class_name.IsNone())
                {
                    class_name = name.to_fname(Unreal::FNAME_Add);
                }
#else
                const Unreal::FName class_name = name.to_fname(Unreal::FNAME_Add);
#endif
                Unreal::UObjectGlobals::FindAllOf(class_name, found_unreal_objects);

                if (!found_unreal_objects.empty())
                {
                    LuaMadeSimple::Lua::Table table = lua.prepare_new_table(elements_to_reserve);

                    for (size_t count{}; const auto& unreal_object : found_unreal_objects)
                    {
                        // Increasing the count first, this is to accommodate the one-index based tables of Lua
                        ++count;

                        table.add_key(count);

                        // Construct a Lua version of a UObject
                        // It will be at the top of the Lua stack and can act as the value of a key/value pair if fuse_pair() is called
                        LuaType::auto_construct_object(lua, unreal_object);
                        table.fuse_pair();
                    }

                    table.make_local();
                }
                else
                {
                    lua.set_nil();
                }

                return 1;
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            // This code isn't executed
            // Lua will error out in the else statement above
            // This is purely to shut the compiler up
            lua.set_nil();
            return 1;
        });

        // [CAP05-CRASHTEST] ModBridgeCrashTest(kind, token) -> false, message when
        // refused (nothing happened); for a valid request the game crashes on purpose.
        // Never throws and never raises a Lua error (no longjmp over C++ objects).
        lua.register_function("ModBridgeCrashTest", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string kind{};
            std::string token{};
            if (lua.is_string()) kind = std::string{lua.get_string()};   // get_string removes index 1
            if (lua.is_string()) token = std::string{lua.get_string()};
            if (token == cap05_crashtest::kToken && (kind == "segv" || kind == "trap"))
            {
                try { Output::send<LogLevel::Warning>(STR("[CAP05-CRASHTEST] deliberate crash requested by a diagnostic mod; crashing now (opt-in crash-capture test, not a bug)\n")); } catch (...) {}
            }
            const char* msg = cap05_crashtest::dispatch(kind, token);
            lua.set_bool(false);
            lua.set_string(std::string_view{msg});
            return 2;
        });

        // [IS-LUA-ALLOC] Claude round-13 item 1: expose the Lua allocator
        // state to mods so the self-test can bracket every run with exact
        // before/after numbers (gc_kb per THIS state; live/allocs/fails are
        // process-wide across all Lua states). Read-only; never errors.
        // [REFUSED-LOG] number of [REFUSED] events so far (refused calls + registrations that can never fire).
        // Lets the self-test's GAPS line count real gaps. Read-only; never errors.
        lua.register_function("GetRefusedHookCount", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(static_cast<int64_t>(refused_log::total.load(std::memory_order_relaxed)));
            return 1;
        });

        lua.register_function("GetLuaMemory", [](const LuaMadeSimple::Lua& lua) -> int {
            const auto snap = LuaMadeSimple::query_lua_memory(lua.get_lua_state());
            lua.set_string(std::format("gc_kb={} live_kb={} blocks={} allocs={} fails={} fail_o={} fail_n={} arena_cap_kb={} arena_used_kb={} arena_ext_kb={} errs={} cxx={} be={} be_states={} libc_states={} be_kb={} be_fails={}",
                                       snap.gc_kb,
                                       snap.live_bytes / 1024,
                                       snap.live_blocks,
                                       snap.total_alloc_calls,
                                       snap.failed_calls,
                                       snap.last_fail_osize,
                                       snap.last_fail_nsize,
                                       snap.arena_capacity_bytes / 1024,
                                       snap.arena_used_bytes / 1024,
                                       snap.arena_external_bytes / 1024,
                                       snap.raised_errors,
                                       snap.cxx_exceptions,
                                       snap.be_installed != 0 ? "gmalloc" : "libc",
                                       snap.be_states,
                                       snap.libc_states,
                                       snap.be_live_bytes / 1024,
                                       snap.be_failed_calls));
            return 1;
        });

        if (is_true_mod == Mod::IsTrueMod::Yes)
        {
            lua.register_function("IsKeyBindRegistered", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'IsKeyBindRegistered'.
Overloads:
#1: IsKeyBindRegistered(integer key)
#2: IsKeyBindRegistered(integer key, table modifier_key_integers))"};

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                auto key_from_lua = lua.get_integer();
                if (key_from_lua < std::numeric_limits<uint8_t>::min() || key_from_lua > std::numeric_limits<uint8_t>::max())
                {
                    lua.throw_error("Parameter #1 for function 'IsKeyBindRegistered' must be an integer between 0 and 255");
                }
                auto key_to_check = static_cast<Input::Key>(key_from_lua);

                const auto mod = get_mod_ref(lua);

                if (lua.is_table())
                {
                    Input::Handler::ModifierKeyArray modifier_keys{};

                    uint8_t table_counter{};
                    lua.for_each_in_table([&](LuaMadeSimple::LuaTableReference table) -> bool {
                        if (!table.value.is_integer())
                        {
                            lua.throw_error(
                                    "Lua function 'IsKeyBindRegistered', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        int64_t full_integer = table.value.get_integer();
                        if (full_integer < std::numeric_limits<uint8_t>::min() || full_integer > std::numeric_limits<uint8_t>::max())
                        {
                            lua.throw_error(
                                    "Lua function 'IsKeyBindRegistered', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        modifier_keys[table_counter++] = static_cast<Input::ModifierKey>(table.value.get_integer());

                        return false;
                    });

                    if (table_counter > 0)
                    {
                        lua.set_bool(mod->m_program.is_keydown_event_registered(key_to_check, modifier_keys));
                    }
                    else
                    {
                        lua.set_bool(mod->m_program.is_keydown_event_registered(key_to_check));
                    }
                }
                else
                {
                    lua.set_bool(mod->m_program.is_keydown_event_registered(key_to_check));
                }

                return 1;
            });

            lua.register_function("RegisterKeyBindAsync", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'RegisterKeyBindAsync'.
Overloads:
#1: RegisterKeyBindAsync(integer key)
#2: RegisterKeyBindAsync(integer key, table modifier_key_integers))"};

                Mod* mod = get_mod_ref(lua);

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                int64_t key_from_lua = lua.get_integer();
                if (key_from_lua < std::numeric_limits<uint8_t>::min() || key_from_lua > std::numeric_limits<uint8_t>::max())
                {
                    lua.throw_error("Parameter #1 for function 'RegisterKeyBindAsync' must be an integer between 0 and 255");
                }

                Input::Key key_to_register = static_cast<Input::Key>(key_from_lua);

                const auto lua_keybind_callback_lambda = [](const LuaMadeSimple::Lua& lua, const int callback_register_index) -> void {
                    try
                    {
                        lua.registry().get_function_ref(callback_register_index);
                        lua.call_function(0, 0);
                    }
                    catch (std::runtime_error& e)
                    {
                        Output::send<LogLevel::Error>(STR("{}\n"), ensure_str(lua.handle_error(e.what())));
                    }
                };

                if (lua.is_function())
                {
                    // Overload #1
                    // P1: Key to register
                    // P2: Callback

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const int32_t lua_callback_registry_index = lua.registry().make_ref();

                    // Taking 'lua_callback_registry_index' by copy here to ensure its survival
                    // Using a 'custom_data' of 1 to signify that this keydown event was created by a mod
                    mod->m_program.register_keydown_event(
                            key_to_register,
                            [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                            },
                            1, mod);
                }
                else if (lua.is_table())
                {
                    // Overload #2
                    // P1: Key to register
                    // P2: Table of modifier keys
                    // P3: Callback

                    Input::Handler::ModifierKeyArray modifier_keys{};

                    uint8_t table_counter{};
                    lua.for_each_in_table([&](LuaMadeSimple::LuaTableReference table) -> bool {
                        if (!table.value.is_integer())
                        {
                            lua.throw_error(
                                    "Lua function 'RegisterKeyBindAsync', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        int64_t full_integer = table.value.get_integer();
                        if (full_integer < std::numeric_limits<uint8_t>::min() || full_integer > std::numeric_limits<uint8_t>::max())
                        {
                            lua.throw_error(
                                    "Lua function 'RegisterKeyBindAsync', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        modifier_keys[table_counter++] = static_cast<Input::ModifierKey>(table.value.get_integer());

                        return false;
                    });

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const auto lua_callback_registry_index = lua.registry().make_ref();

                    if (table_counter > 0)
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                modifier_keys,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                                },
                                1, mod);
                    }
                    else
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                                },
                                1, mod);
                    }
                }
                else
                {
                    lua.throw_error(error_overload_not_found);
                }

                return 0;
            });

            lua.register_function("RegisterKeyBind", [](const LuaMadeSimple::Lua& lua) -> int {
        try
        {
                std::string error_overload_not_found{R"(
No overload found for function 'RegisterKeyBind'.
Overloads:
#1: RegisterKeyBind(integer key)
#2: RegisterKeyBind(integer key, table modifier_key_integers))"};

                Mod* mod = get_mod_ref(lua);

                if (!lua.is_integer())
                {
                    L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "RegisterKeyBind");
                }

                int64_t key_from_lua = lua.get_integer();
                if (key_from_lua < std::numeric_limits<uint8_t>::min() || key_from_lua > std::numeric_limits<uint8_t>::max())
                {
                    L22Diagnostics::throw_lua_error(lua, "Parameter #1 for function 'RegisterKeyBind' must be an integer between 0 and 255", "RegisterKeyBind");
                }

                Input::Key key_to_register = static_cast<Input::Key>(key_from_lua);

                const auto lua_keybind_callback_lambda = [](const LuaMadeSimple::Lua& lua, const int callback_register_index, Input::Key callback_key) -> void {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] keybind callback entry key={} ref={}\n"), static_cast<int>(callback_key), callback_register_index);
                    });
                    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                    try
                    {
                        get_function_ref_cxx(lua, callback_register_index);
                        lua.call_function(0, 0);
                    }
                    catch (std::runtime_error& e)
                    {
                        Output::send<LogLevel::Error>(STR("{}\n"), ensure_str(lua.handle_error(e.what())));
                    }
                };

                if (lua.is_function())
                {
                    // Overload #1
                    // P1: Key to register
                    // P2: Callback

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const int32_t lua_callback_registry_index = lua.registry().make_ref();

                    // Taking 'lua_callback_registry_index' by copy here to ensure its survival
                    // Using a 'custom_data' of 1 to signify that this keydown event was created by a mod
                    mod->m_program.register_keydown_event(
                            key_to_register,
                            [&lua, lua_callback_registry_index, lua_keybind_callback_lambda, key_to_register]() {
                                lua_keybind_callback_lambda(lua, lua_callback_registry_index, key_to_register);
                            },
                            1, mod);
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] RegisterKeyBind returned key={} modifiers=0 (registration call returned)\n"), static_cast<int>(key_to_register));
                    });
                }
                else if (lua.is_table())
                {
                    // Overload #2
                    // P1: Key to register
                    // P2: Table of modifier keys
                    // P3: Callback

                    Input::Handler::ModifierKeyArray modifier_keys{};

                    uint8_t table_counter{};
                    lua.for_each_in_table([&](LuaMadeSimple::LuaTableReference table) -> bool {
                        if (!table.value.is_integer())
                        {
                            L22Diagnostics::throw_lua_error(lua, "Lua function 'RegisterKeyBind', overload #2, requires a table of 1-byte large integers as the second parameter", "RegisterKeyBind");
                        }

                        int64_t full_integer = table.value.get_integer();
                        if (full_integer < std::numeric_limits<uint8_t>::min() || full_integer > std::numeric_limits<uint8_t>::max())
                        {
                            L22Diagnostics::throw_lua_error(lua, "Lua function 'RegisterKeyBind', overload #2, requires a table of 1-byte large integers as the second parameter", "RegisterKeyBind");
                        }

                        modifier_keys[table_counter++] = static_cast<Input::ModifierKey>(table.value.get_integer());

                        return false;
                    });

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const auto lua_callback_registry_index = lua.registry().make_ref();

                    if (table_counter > 0)
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                modifier_keys,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda, key_to_register]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index, key_to_register);
                                },
                                1, mod);
                    }
                    else
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda, key_to_register]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index, key_to_register);
                                },
                                1, mod);
                    }
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] RegisterKeyBind returned key={} modifiers={} (registration call returned)\n"), static_cast<int>(key_to_register), table_counter);
                        for (uint8_t index = 0; index < table_counter; ++index)
                            Output::send(STR("[l22] RegisterKeyBind modifier[{}]={} key={}\n"), index, static_cast<int>(modifier_keys[index]), static_cast<int>(key_to_register));
                    });
                }
                else
                {
                    L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "RegisterKeyBind");
                }

                return 0;
            
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("RegisterKeyBind", error.what());
            throw;
        }
        catch (void*)
        {
            // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
            // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
            throw;
        }
        catch (...)
        {
            L22Diagnostics::error("RegisterKeyBind", "non-standard C++ exception");
            throw;
        }
    });

            lua.register_function("UnregisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                std::string error_overload_not_found{R"(
No overload found for function 'UnregisterHook'.
Overloads:
#1: UnregisterHook(string UFunction_Name, integer PreCallbackId, integer PostCallbackId))"};

                if (!lua.is_string())
                {
                    throw std::runtime_error(error_overload_not_found);
                }

                // [LFB] ONE hand converter for the Lua path name (was
                // ensure_str(lua.get_string())).
                const LuaType::LuaFNameBridge::Bridge hook_name{lua.get_lua_state()};
                auto function_name_no_prefix = get_function_name_without_prefix(hook_name.string());

                Unreal::UFunction* unreal_function = Unreal::UObjectGlobals::StaticFindObject<Unreal::UFunction*>(nullptr, nullptr, function_name_no_prefix);
                if (!unreal_function)
                {
                    refused_log::throws(lua, "UnregisterHook", function_name_no_prefix, "ufunction-not-found");
                    throw std::runtime_error(std::format("Tried to unregister a hook with Lua function 'UnregisterHook' but no UFunction with the specified name "
                                                "was found.\n>FunctionName: {}",
                                                to_string(function_name_no_prefix)));
                }

                if (!lua.is_integer())
                {
                    throw std::runtime_error(error_overload_not_found);
                }
                const auto pre_id = lua.get_integer();

                if (!lua.is_integer())
                {
                    throw std::runtime_error(error_overload_not_found);
                }
                const auto post_id = lua.get_integer();

                if (pre_id > std::numeric_limits<int32_t>::max())
                {
                    throw std::runtime_error(std::format("Tried to unregister a hook with Lua function 'UnregisterHook' but the PreCallbackId supplied was too "
                                                "large (>int32)\n>FunctionName: {}",
                                                to_string(function_name_no_prefix)));
                }

                if (post_id > std::numeric_limits<int32_t>::max())
                {
                    throw std::runtime_error(std::format("Tried to unregister a hook with Lua function 'UnregisterHook' but the PostCallbackId supplied was too "
                                                "large (>int32)\n>FunctionName: {}",
                                                to_string(function_name_no_prefix)));
                }


                auto func_ptr = unreal_function->GetFunc();
                if (func_ptr && func_ptr != Unreal::UObject::ProcessInternalInternal.get_function_address() &&
                    unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
                {
                    const auto hook_data = std::ranges::find_if(g_hooked_script_function_data, [&](const std::unique_ptr<LuaUnrealScriptFunctionData>& elem) {
                        return elem->post_callback_id == post_id && elem->pre_callback_id == pre_id;
                    });
                    if (hook_data != g_hooked_script_function_data.end())
                    {
                        hook_data->get()->scheduled_for_removal = true;
                    }
                }
                else if (func_ptr && func_ptr == Unreal::UObject::ProcessInternalInternal.get_function_address() &&
                         !unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
                {
                    if (auto data_ptr = LuaMod::find_function_hook_data(LuaMod::m_script_hook_callbacks, unreal_function); data_ptr)
                    {
                        Output::send<LogLevel::Verbose>(STR("Unregistering script hook with id: {}, FunctionName: {}\n"), post_id, function_name_no_prefix);
                        auto& registry_indexes = data_ptr->callback_data.registry_indexes;
                        for (auto& registry_index : registry_indexes)
                        {
                            if (post_id == registry_index.second.identifier)
                            {
                                registry_index.second.lua_index = -1;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    std::string error_message{"Was unable to unregister a hook with Lua function 'UnregisterHook', information:\n"};
                    error_message.append(fmt::format("FunctionName: {}\n", to_string(function_name_no_prefix)));
                    error_message.append(fmt::format("UFunction::Func: {}\n", std::bit_cast<void*>(func_ptr)));
                    error_message.append(fmt::format("ProcessInternal: {}\n", Unreal::UObject::ProcessInternalInternal.get_function_address()));
                    error_message.append(
                            fmt::format("FUNC_Native: {}\n", static_cast<uint32_t>(unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))));
                    refused_log::throws(lua, "UnregisterHook", function_name_no_prefix,
                                        !func_ptr ? "ufunction-func-null"
                                                  : (unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native) ? "native-func-is-processinternal"
                                                                                                                               : "bp-script-function-needs-processinternal-hook"));
                    throw std::runtime_error(error_message);
                }

                return 0;
            });

            lua.register_function("DumpAllObjects", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                const Mod* mod = get_mod_ref(lua);
                if (!mod)
                {
                    lua.throw_error("Couldn't dump objects and properties because the pointer to 'Mod' was nullptr");
                }
                UE4SSProgram::dump_all_objects_and_properties(mod->m_program.get_object_dumper_output_directory() + STR("\\") +
                                                              UE4SSProgram::m_object_dumper_file_name);
                return 0;
            });

            lua.register_function("GenerateSDK", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                const Mod* mod = get_mod_ref(lua);
                if (!mod)
                {
                    lua.throw_error("Couldn't generate SDK because the pointer to 'Mod' was nullptr");
                }
                File::StringType working_dir{mod->m_program.get_working_directory()};
                mod->m_program.generate_cxx_headers(working_dir + STR("\\CXXHeaderDump"));
                return 0;
            });

            lua.register_function("GenerateLuaTypes", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                const Mod* mod = get_mod_ref(lua);
                if (!mod)
                {
                    lua.throw_error("Couldn't generate lua types because the pointer to 'Mod' was nullptr");
                }
                File::StringType working_dir{mod->m_program.get_working_directory()};
                UE4SSProgram::get_program().generate_lua_types(working_dir + STR("\\Mods\\shared\\types"));
                return 0;
            });

            lua.register_function("GenerateUHTCompatibleHeaders", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                const Mod* mod = get_mod_ref(lua);
                mod->m_program.generate_uht_compatible_headers();
                return 0;
            });

            lua.register_function("DumpStaticMeshes", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                GUI::Dumpers::call_generate_static_mesh_file();
                return 0;
            });

            lua.register_function("DumpAllActors", []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
                GUI::Dumpers::call_generate_all_actor_file();
                return 0;
            });

            lua.register_function("DumpUSMAP", [](const LuaMadeSimple::Lua& lua) -> int {
                bool include_blueprint_types{true};
                if (lua.is_bool())
                {
                    include_blueprint_types = lua.get_bool();
                }
                OutTheShade::generate_usmap(include_blueprint_types);
                return 0;
            });

            lua.register_function("DumpJMAP", [](const LuaMadeSimple::Lua& lua) -> int {
                bool include_blueprint_types{true};
                if (lua.is_bool())
                {
                    include_blueprint_types = lua.get_bool();
                }
                bool skip_property_values{false};
                if (lua.is_bool())
                {
                    skip_property_values = lua.get_bool();
                }
                JMapGenerator::generate_jmap(include_blueprint_types, skip_property_values);
                return 0;
            });
        }

        lua.register_function("StaticConstructObject", [](const LuaMadeSimple::Lua& lua) -> int {
        try
        {
            std::string error_overload_not_found{R"(
No overload found for function 'StaticConstructObject'.
Overloads:
#1: StaticConstructObject(
                            UClass Class,
                            UObject Outer,
                            FName Name, #Optional
                            EObjectFlags Flags, #Optional
                            EInternalObjectFlags InternalSetFlags, #Optional
                            bool CopyTransientsFromClassDefaults, #Optional
                            bool AssumeTemplateIsArchetype, #Optional
                            UObject Template, #Optional
                            FObjectInstancingGraph InstanceGraph, #Optional
                            UPackage ExternalPackage, #Optional
                            void SubobjectOverrides #Optional

))"};

            // For now, we're assuming that if there's userdata, that userdata is of the correct underlying type
            if (!lua.is_userdata())
            {
                L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "StaticConstructObject");
            }
            Unreal::UClass* param_class = lua.get_userdata<LuaType::UClass>().get_remote_cpp_object();

            if (!lua.is_userdata())
            {
                L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "StaticConstructObject");
            }
            Unreal::UObject* param_outer = lua.get_userdata<LuaType::UObject>().get_remote_cpp_object();

            Unreal::FName param_name;
            if (lua.is_userdata())
            {
                param_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
            }
            else if (lua.is_integer())
            {
                param_name = Unreal::FName(lua.get_integer());
            }
            else
            {
                param_name = Unreal::FName(static_cast<int64_t>(0));
            }

            Unreal::EObjectFlags param_set_flags{};
            if (lua.is_integer())
            {
                param_set_flags = static_cast<Unreal::EObjectFlags>(lua.get_integer());
            }

            Unreal::EInternalObjectFlags param_internal_set_flags{};
            if (lua.is_integer())
            {
                param_internal_set_flags = static_cast<Unreal::EInternalObjectFlags>(lua.get_integer());
            }

            // The rest are all optional parameters
            bool param_copy_transients_from_class_defaults{};
            if (lua.is_bool())
            {
                param_copy_transients_from_class_defaults = lua.get_bool();
            }

            bool param_assume_template_is_archetype{};
            if (lua.is_bool())
            {
                param_assume_template_is_archetype = lua.get_bool();
            }

            Unreal::UObject* param_template{};
            if (lua.is_userdata())
            {
                param_template = lua.get_userdata<LuaType::UObject>().get_remote_cpp_object();
            }

            // Change this to userdata if support for 'FObjectInstancingGraph' is ever added
            void* param_instance_graph{};
            if (lua.is_integer())
            {
                param_instance_graph = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer()));
            }

            // Change this to userdata if support for 'UPackage' is ever added
            void* param_external_package{};
            if (lua.is_integer())
            {
                param_external_package = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer()));
            }

            // void* param_subobject_overrides{};
            // if (lua.is_integer()) { param_subobject_overrides = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer())); }

            Unreal::FStaticConstructObjectParameters params{param_class, param_outer};
            params.Name = param_name;
            params.SetFlags = param_set_flags;
            params.InternalSetFlags = param_internal_set_flags;
            params.bCopyTransientsFromClassDefaults = param_copy_transients_from_class_defaults;
            params.bAssumeTemplateIsArchetype = param_assume_template_is_archetype;
            params.Template = param_template;
            params.InstanceGraph = static_cast<struct RC::Unreal::FObjectInstancingGraph*>(param_instance_graph);
            params.ExternalPackage = static_cast<class RC::Unreal::UPackage*>(param_external_package);
            Unreal::UObject* created_object = Unreal::UObjectGlobals::StaticConstructObject(params);
            if (created_object)
            {
                // [B6] rate limit: this line fired thousands of times per minute when a mod
                // builds UI (B4 launch: 5341 lines). Log the first 20, then one count line per 1000.
                static std::atomic<uint64_t> s_constructed_count{0};
                const uint64_t constructed_n = s_constructed_count.fetch_add(1, std::memory_order_relaxed) + 1;
                if (constructed_n <= 20)
                {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] widget/object constructed via StaticConstructObject class={} object={} (non-null return)\n"),
                                     static_cast<void*>(param_class), static_cast<void*>(created_object));
                    });
                }
                else if (constructed_n % 1000 == 0)
                {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[B6-RATE] StaticConstructObject constructed total={} (per-object lines after the first 20 suppressed)\n"), constructed_n);
                    });
                }
            }
            else
            {
                L22Diagnostics::error("StaticConstructObject Lua", "returned null; preserving invalid UObject result");
            }

            LuaType::UObject::construct(lua, created_object);

            return 1;
        
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("StaticConstructObject", error.what());
            throw;
        }
        catch (void*)
        {
            // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
            // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
            throw;
        }
        catch (...)
        {
            L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception");
            throw;
        }
    });

        lua.register_function("RegisterCustomProperty", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterCustomProperty'.
Overloads:
#1: RegisterCustomProperty(table PropertyInfo))"};

            if (!lua.is_table())
            {
                lua.throw_error(error_overload_not_found);
            }

            struct PropertyTypeInfo
            {
                std::string_view name{};
                int32_t size{-1};
                void* ffieldclass_pointer{};
                void* static_pointer{};

                auto is_valid() -> bool
                {
                    if (size < 0)
                    {
                        return false;
                    }
                    if (!ffieldclass_pointer)
                    {
                        return false;
                    }
                    // if (!static_pointer) { return false; }

                    return true;
                }
            };

            struct PropertyInfo
            {
                StringType name{};
                PropertyTypeInfo type{}; // Figure out what to do here, it shouldn't be just a string
                StringType belongs_to_class{};
                int32_t offset_internal{-1};
                int32_t element_size{-1}; // Is this required for trivial types like integers and floats ?

                // ArrayProperty
                PropertyTypeInfo array_inner{};

                bool offset_internal_is_table{};

                // Only one of these booleans can be true
                bool is_array_property{};

                auto set_is_array_property() -> void
                {
                    // Check here if any incompatible booleans have been set, and throw error if so
                    is_array_property = true;
                }

                auto is_missing_values() -> bool
                {
                    if (name.empty())
                    {
                        return true;
                    }
                    if (!type.is_valid())
                    {
                        return true;
                    }
                    if (belongs_to_class.empty())
                    {
                        return true;
                    }
                    if (!offset_internal_is_table && offset_internal < 0)
                    {
                        return true;
                    }
                    // if (element_size < 0) { return true; }

                    if (is_array_property && !array_inner.is_valid())
                    {
                        return true;
                    }

                    return false;
                }
            };

            PropertyInfo property_info{};

            auto lua_table = lua.get_table();

            auto verify_and_convert_int64_to_int32 = [&](std::string_view field_name,
                                                         std::string_view second_field_name = {},
                                                         std::string_view third_field_name = {},
                                                         bool* has_error = nullptr) -> int32_t {
                int64_t integer;

                if (second_field_name.empty())
                {
                    // Ignore the third field name if the second one isn't set
                    integer = lua_table.get_int_field(field_name, has_error);
                }
                else if (third_field_name.empty())
                {
                    // If the second field name is set but the third isn't, then we have two layers to the table
                    integer = lua_table.get_table_field(field_name, has_error).get_int_field(second_field_name, has_error);
                }
                else
                {
                    // If both the second field name and the third field name is set, then we have three layers to the table
                    integer = lua_table.get_table_field(field_name, has_error).get_table_field(second_field_name, has_error).get_int_field(third_field_name, has_error);
                }

                if (integer < std::numeric_limits<int32_t>::min() || integer > std::numeric_limits<int32_t>::max())
                {
                    std::string error_field_names;

                    if (second_field_name.empty())
                    {
                        error_field_names = fmt::format("{}", field_name);
                    }
                    else if (third_field_name.empty())
                    {
                        error_field_names = fmt::format("{}.{}", field_name, second_field_name);
                    }
                    else
                    {
                        error_field_names = fmt::format("{}.{}.{}", field_name, second_field_name, third_field_name);
                    }

                    lua.throw_error(fmt::format(
                            "Parameter #1 for function 'RegisterCustomProperty'. The table value for key '{}' is outside the range of a 32-bit integer",
                            error_field_names));
                }

                return static_cast<int32_t>(integer);
            };

            // Always required, for all property types
            // [LFB] ONE hand converter (memory-source variant over the table
            // fields; was ensure_str on each).
            {
                auto field = lua_table.get_string_field("Name");
                property_info.name = LuaType::LuaFNameBridge::Bridge{field.data(), field.size()}.string();
            }
            property_info.type.name = lua_table.get_table_field("Type").get_string_field("Name");
            property_info.type.size = verify_and_convert_int64_to_int32("Type", "Size");
            property_info.type.ffieldclass_pointer = reinterpret_cast<void*>(lua_table.get_table_field("Type").get_int_field("FFieldClassPointer"));
            property_info.type.static_pointer = reinterpret_cast<void*>(lua_table.get_table_field("Type").get_int_field("StaticPointer"));
            {
                auto field = lua_table.get_string_field("BelongsToClass");
                property_info.belongs_to_class = LuaType::LuaFNameBridge::Bridge{field.data(), field.size()}.string();
            }

            std::string oi_property_name;
            int32_t oi_relative_offset{};

            bool error_while_getting_offset_internal{};
            property_info.offset_internal = verify_and_convert_int64_to_int32("OffsetInternal", "", "", &error_while_getting_offset_internal);

            if (error_while_getting_offset_internal)
            {
                // Failed to get integer from table
                // This means that we may have a table instead of an integer

                oi_property_name = lua_table.get_table_field("OffsetInternal").get_string_field("Property");
                oi_relative_offset = verify_and_convert_int64_to_int32("OffsetInternal", "RelativeOffset");

                property_info.offset_internal_is_table = true;
            }

            // Only required for ArrayProperty
            if (property_info.type.name == "ArrayProperty")
            {
                if (!lua_table.does_field_exist("ArrayProperty"))
                {
                    lua.throw_error("Parameter #1 for function 'RegisterCustomProperty'. The table entry 'ArrayProperty' is missing.");
                }
                else
                {
                    property_info.set_is_array_property();
                    property_info.array_inner.name = lua_table.get_table_field("ArrayProperty").get_table_field("Type").get_string_field("Name");
                    property_info.array_inner.size = verify_and_convert_int64_to_int32("ArrayProperty", "Type", "Size");
                    property_info.array_inner.ffieldclass_pointer =
                            reinterpret_cast<void*>(lua_table.get_table_field("ArrayProperty").get_table_field("Type").get_int_field("FFieldClassPointer"));
                    property_info.array_inner.static_pointer =
                            reinterpret_cast<void*>(lua_table.get_table_field("ArrayProperty").get_table_field("Type").get_int_field("StaticPointer"));
                }
            }

            if (property_info.is_missing_values())
            {
                lua.throw_error("Parameter #1 for function 'RegisterCustomProperty'. The table is missing required fields.");
            }

            Unreal::UClass* belongs_to_class = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, property_info.belongs_to_class);
            if (!belongs_to_class)
            {
                lua.throw_error("Tried to 'RegisterCustomProperty' but 'BelongsToClass' could not be found");
            }

            if (property_info.offset_internal_is_table)
            {
                // [LFB] ONE hand converter, slot-2 ctor direct (was
                // ensure_str + FString-ctor seam).
                auto name = LuaType::LuaFNameBridge::Bridge{oi_property_name.data(), oi_property_name.size()}.to_fname(Unreal::FNAME_Add);
                Unreal::FProperty* oi_property = belongs_to_class->FindProperty(name);
                if (!oi_property)
                {
                    lua.throw_error(fmt::format("Was unable to find property '{}' in class '{}' for use for relative Offset_Internal",
                                                oi_property_name,
                                                to_string(property_info.belongs_to_class)));
                }

                property_info.offset_internal = oi_property->GetOffset_Internal() + oi_relative_offset;
            }

            if (property_info.type.size == 0)
            {
                lua.throw_error(fmt::format("The size for property '{}' was unknown. Custom sizes are unsupported but will likely be supported in the future.",
                                            property_info.type.name));
            }

            if (property_info.is_array_property && property_info.array_inner.size == 0)
            {
                lua.throw_error(
                        fmt::format("The size for inner property '{}' was unknown. Custom sizes are unsupported but will likely be supported in the future.",
                                    property_info.array_inner.name));
            }

            LuaType::LuaCustomProperty::StaticStorage::property_list.add(
                    property_info.name,
                    Unreal::CustomArrayProperty::construct(property_info.offset_internal,
                                                           belongs_to_class,
                                                           static_cast<Unreal::UClass*>(property_info.type.ffieldclass_pointer),
                                                           static_cast<Unreal::FProperty*>(property_info.array_inner.ffieldclass_pointer),
                                                           property_info.is_array_property ? property_info.array_inner.size : property_info.type.size

                                                           ));

            UE4SS_LUAMOD_PRINTF("Registered Custom Property\n");
            UE4SS_LUAMOD_PRINTF("PropertyInfo {\n");
            UE4SS_LUAMOD_PRINTF("\tName: %ls\n", FromCharTypePtr<wchar_t>(property_info.name.c_str()));
            UE4SS_LUAMOD_PRINTF("\tType {\n");
            UE4SS_LUAMOD_PRINTF("\t\tName: %s\n", property_info.type.name.data());
            UE4SS_LUAMOD_PRINTF("\t\tSize: 0x%X\n", property_info.type.size);
            UE4SS_LUAMOD_PRINTF("\t\tFFieldClassPointer: 0x%p\n", property_info.type.ffieldclass_pointer);
            UE4SS_LUAMOD_PRINTF("\t\tStaticPointer: 0x%p\n", property_info.type.static_pointer);
            UE4SS_LUAMOD_PRINTF("\t}\n");
            UE4SS_LUAMOD_PRINTF("\tBelongsToClass: %ls\n", FromCharTypePtr<wchar_t>(property_info.belongs_to_class.c_str()));
            UE4SS_LUAMOD_PRINTF("\tOffsetInternal: 0x%X\n", property_info.offset_internal);

            if (property_info.is_array_property)
            {
                UE4SS_LUAMOD_PRINTF("\tArrayProperty {\n");
                UE4SS_LUAMOD_PRINTF("\t\tType {\n");
                UE4SS_LUAMOD_PRINTF("\t\t\tName: %s\n", property_info.array_inner.name.data());
                UE4SS_LUAMOD_PRINTF("\t\t\tSize: 0x%X\n", property_info.array_inner.size);
                UE4SS_LUAMOD_PRINTF("\t\t\tFFieldClassPointer: %p\n", property_info.array_inner.ffieldclass_pointer);
                UE4SS_LUAMOD_PRINTF("\t\t\tStaticPointer: %p\n", property_info.array_inner.static_pointer);
                UE4SS_LUAMOD_PRINTF("\t\t}\n");
                UE4SS_LUAMOD_PRINTF("\t}\n");
            }

            UE4SS_LUAMOD_PRINTF("}\n");

            return 0;
        });

        lua.register_function("ForEachUObject", [](const LuaMadeSimple::Lua& lua) -> int {
            // [FOU][IU-GROUP-B] Claude r13 item 5 / self-test call 133.
            // The inherited loop called call_function(3, 1) and never discarded
            // the requested result: this is a global function (no get_userdata
            // removal happens), the callback stays at stack index 1, and the
            // single result slot leaked per visited object. On this game's
            // large GUObjectArray the Lua stack grows to LUAI_MAXSTACK and the
            // whole call fails with the observed 'iterate-err' (164ms run 1 /
            // 23ms run 2, CLAUDE-SELFTEST-RESULTS). Upstream UEnum.ForEachName
            // already uses the discard pattern copied here. Hardening beyond
            // parity, all bounded: lua_checkstack guard and per-object
            // construction failures skip (first 3 logged) instead of aborting
            // the walk.
            // [B3 review] The callback's return value is IGNORED, as upstream
            // does: the staged "return true breaks the walk" was dropped (a PC
            // mod whose callback returns true must not stop early on PS5), and
            // the result slot is discarded by restoring the per-iteration stack
            // height instead of a fixed index (correct even with extra args).
            int32_t fou_construct_failures{};
            int32_t fou_visited{};
            Unreal::UObjectGlobals::ForEachUObject([&](void* object, int32_t chunk_index, int32_t object_index) {
                lua_State* fou_lua_state = lua.get_lua_state();
                if (!lua_checkstack(fou_lua_state, 8))
                {
                    Output::send<LogLevel::Error>(STR("[FOU] ForEachUObject: Lua stack exhausted after {} objects; stopping walk\n"), fou_visited);
                    return LoopAction::Break;
                }
                const int fou_baseline = lua_gettop(fou_lua_state);
                // Duplicate the Lua function so that we can use it in subsequent iterations of this loop (call_function pops the function from the stack)
                lua_pushvalue(fou_lua_state, 1);

                bool fou_constructed{};
                try
                {
                    // Set the 'Object' parameter for the Lua function (P1)
                    LuaType::auto_construct_object(lua, static_cast<Unreal::UObject*>(object));
                    fou_constructed = true;
                }
                catch (const std::exception& e)
                {
                    ++fou_construct_failures;
                    if (fou_construct_failures <= 3)
                    {
                        Output::send<LogLevel::Warning>(STR("[FOU] ForEachUObject: object construction failed ({}); skipping object\n"), ensure_str(e.what()));
                    }
                }

                if (!fou_constructed)
                {
                    lua_settop(fou_lua_state, fou_baseline);
                    return LoopAction::Continue;
                }

                // Set the 'ChunkIndex' parameter for the Lua function (P2)
                lua.set_integer(chunk_index);

                // Set the 'ObjectIndex' parameter for the Lua function (P3)
                lua.set_integer(object_index);

                lua.call_function(3, 1);
                ++fou_visited;

                // There's a 'nil' on the stack because we told Lua that we expect a return value.
                // Lua will put 'nil' on the stack if the Lua function doesn't explicitly return anything.
                // Discard it (back to the height before this iteration), otherwise the Lua stack grows
                // by one slot per object and overflows on large object arrays.
                lua_settop(fou_lua_state, fou_baseline);
                return LoopAction::Continue;
            });
            if (fou_construct_failures > 0)
            {
                Output::send<LogLevel::Warning>(STR("[FOU] ForEachUObject: {} object constructions failed ({} visited)\n"), fou_construct_failures, fou_visited);
            }
            return 0;
        });

        lua.register_function("NotifyOnNewObject", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'NotifyOnNewObject'.
Overloads:
#1: NotifyOnNewObject(string UClassName, LuaFunction Callback))"};

            if (!lua.is_string())
            {
                lua.throw_error(error_overload_not_found);
            }

            // [LFB] ONE hand converter owns the Lua class-name string (was
            // ensure_str); the per-part FNAME_Find/Add constructions below
            // stay on the engine seam path (Claude item 9).
            auto class_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.string();

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto [hook_lua, thread_ref] = make_hook_state(mod);

            // Duplicate the Lua function to the top of the stack for lua_xmove and luaL_ref
            lua_pushvalue(lua.get_lua_state(), 1);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            const auto func_ref = hook_lua->registry().make_ref();

            if (class_name.contains(STR(' ')))
            {
                lua.throw_error(fmt::format("Param #1 for NotifyOnNewObject cannot contain spaces; Param value: '{}'", to_utf8_string(class_name)));
            }

            const auto name_parts = explode_by_occurrence(class_name, STR('.'));
            if (name_parts.size() < 2)
            {
                lua.throw_error(fmt::format("Param #1 for NotifyOnNewObject must contain at least two parts; Param value: '{}'", to_utf8_string(class_name)));
            }

            auto class_fname = Unreal::FName(name_parts.back(), Unreal::FNAME_Find);
            if (class_fname == Unreal::NAME_None)
            {
                class_fname = Unreal::FName(name_parts.back(), Unreal::FNAME_Add);
            }

            auto class_outer_fname = Unreal::FName(name_parts.front(), Unreal::FNAME_Find);
            if (class_outer_fname == Unreal::NAME_None)
            {
                class_outer_fname = Unreal::FName(name_parts.front(), Unreal::FNAME_Add);
            }

            LuaMod::m_static_construct_object_lua_callbacks.emplace_back(LuaMod::LuaCancellableCallbackData{
                LuaMod::m_next_static_construct_callback_id++,
                hook_lua,
                class_fname,
                class_outer_fname,
                func_ref,
                thread_ref
            });

            Output::send<LogLevel::Verbose>(STR("[NotifyOnNewObject] Registered notification for {}\n"), class_name);
            Unreal::UObjectGlobals::r9_index_notify_arm(); // report only objects created after this registration

            // [R9-NOTIFY] delivered from the object index on the PS5 (no longer a silent refusal)
            return 0;
        });

        lua.register_function("RegisterCustomEvent", [](const LuaMadeSimple::Lua& lua) -> int {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterCustomEvent'.
Overloads:
#1: RegisterCustomEvent(string EventName, LuaFunction Callback))"};

            if (!lua.is_string())
            {
                throw std::runtime_error(error_overload_not_found);
            }

            // [LFB] ONE hand converter: Lua event-name string -> UTF-16 (was
            // ensure_str); the FNAME_Add constructions below run on the
            // hand-converted text.
            auto event_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.string();

            if (!lua.is_function())
            {
                throw std::runtime_error(error_overload_not_found);
            }

            auto mod = get_mod_ref_cxx(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();
            if (!LuaMod::find_function_hook_data(LuaMod::m_custom_event_callbacks, Unreal::FName(event_name, Unreal::FNAME_Add)))
            {
                LuaMod::m_custom_event_callbacks.emplace_back(LuaMod::FunctionHookData{
                        {Unreal::FName(event_name, Unreal::FNAME_Add)},
                        LuaMod::LuaCallbackData{
                                .lua = hook_lua,
                                .instance_of_class = nullptr,
                                .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, {lua_callback_registry_index}}},
                        }});
            }

            refused_log::accepted(lua, "RegisterCustomEvent", event_name, refused_log::Family::CustomEvent);
            return 0;
        });

        lua.register_function("UnregisterCustomEvent", [](const LuaMadeSimple::Lua& lua) -> int {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            std::string error_overload_not_found{R"(
No overload found for function 'UnregisterCustomEvent'.
Overloads:
#1: UnregisterCustomEvent(string EventName))"};

            if (!lua.is_string())
            {
                throw std::runtime_error(error_overload_not_found);
            }
            // [LFB] ONE hand converter (was ensure_str).
            auto custom_event_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.string();

            LuaMod::remove_function_hook_data(LuaMod::m_custom_event_callbacks, custom_event_name);

            return 0;
        });

        lua.register_function("RegisterLoadMapPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterLoadMapPreHook'.
Overloads:
#1: RegisterLoadMapPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the lua function (it also pops it off the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_load_map_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}}});
            }

            refused_log::accepted(lua, "RegisterLoadMapPreHook", StringType{STR("-")}, refused_log::Family::LoadMap);
            refused_log::route_note(0, "RegisterLoadMapPreHook", "after-the-load(PC:before);World=invalid,Engine=invalid,URL=nil;return-ignored");
            return 0;
        });

        lua.register_function("RegisterLoadMapPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterLoadMapPostHook'.
Overloads:
#1: RegisterLoadMapPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the lua function (it also pops it off the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_load_map_post_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}}});
            }

            refused_log::accepted(lua, "RegisterLoadMapPostHook", StringType{STR("-")}, refused_log::Family::LoadMap);
            refused_log::route_note(1, "RegisterLoadMapPostHook", "when-the-load-window-closes(>=3s-after-the-last-load-object);Engine=invalid,URL=nil;return-ignored");
            return 0;
        });

        lua.register_function("RegisterInitGameStatePreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterInitGameStatePreHook'.
Overloads:
#1: RegisterInitGameStatePreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_init_game_state_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });
            }

            refused_log::accepted(lua, "RegisterInitGameStatePreHook", StringType{STR("-")}, refused_log::Family::WorldEvent);
            refused_log::route_note(2, "RegisterInitGameStatePreHook", "after-InitGameState-ran(PC:before);when-the-load-window-closes");
            return 0;
        });

        lua.register_function("RegisterInitGameStatePostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterInitGameStatePostHook'.
Overloads:
#1: RegisterInitGameStatePostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_init_game_state_post_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });
            }

            refused_log::accepted(lua, "RegisterInitGameStatePostHook", StringType{STR("-")}, refused_log::Family::WorldEvent);
            refused_log::route_note(3, "RegisterInitGameStatePostHook", "when-the-load-window-closes");
            return 0;
        });

        lua.register_function("RegisterBeginPlayPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterBeginPlayPreHook'.
Overloads:
#1: RegisterBeginPlayPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_begin_play_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });
            }

            refused_log::accepted(lua, "RegisterBeginPlayPreHook", StringType{STR("-")}, refused_log::Family::BeginPlay);
            refused_log::route_note(4, "RegisterBeginPlayPreHook", "after-the-event(PC:before);spawned:>=50ms-after-spawn;map-load-actors:when-the-load-window-closes;PARTIAL:streamed-level-actors-not-reported(counted-gap)");
            return 0;
        });

        lua.register_function("RegisterBeginPlayPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterBeginPlayPostHook'.
Overloads:
#1: RegisterBeginPlayPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            { // [WORLD-EV] the game-thread delivery reads this vector under the same lock
            std::lock_guard<std::recursive_mutex> world_ev_guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_begin_play_post_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });
            }

            refused_log::accepted(lua, "RegisterBeginPlayPostHook", StringType{STR("-")}, refused_log::Family::BeginPlay);
            refused_log::route_note(5, "RegisterBeginPlayPostHook", "spawned:>=50ms-after-spawn;map-load-actors:when-the-load-window-closes;PARTIAL:streamed-level-actors-not-reported(counted-gap)");
            return 0;
        });

        lua.register_function("RegisterEndPlayPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterEndPlayPreHook'.
Overloads:
#1: RegisterEndPlayPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            LuaMod::m_end_play_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });

            refused_log::accepted(lua, "RegisterEndPlayPreHook", StringType{STR("-")}, refused_log::Family::EndPlay); // [WORLD-EV] stays refused
            return 0;
        });

        lua.register_function("RegisterEndPlayPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterEndPlayPostHook'.
Overloads:
#1: RegisterEndPlayPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);

            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const int32_t lua_callback_registry_index = hook_lua->registry().make_ref();

            LuaMod::m_end_play_post_callbacks.emplace_back(LuaMod::LuaCallbackData{
                    .lua = hook_lua,
                    .instance_of_class = nullptr,
                    .registry_indexes = {std::pair<const LuaMadeSimple::Lua*, LuaMod::LuaCallbackData::RegistryIndex>{hook_lua, lua_callback_registry_index}},
            });

            refused_log::accepted(lua, "RegisterEndPlayPostHook", StringType{STR("-")}, refused_log::Family::EndPlay); // [WORLD-EV] stays refused
            return 0;
        });

        lua.register_function("IterateGameDirectories", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IterateGameDirectories'.
Overloads:
#1: IterateGameDirectories())"};

            std::filesystem::path game_executable_directory = UE4SSProgram::get_program().get_game_executable_directory();
            auto game_content_dir = game_executable_directory.parent_path().parent_path() / "Content";
            auto game_name = game_executable_directory.parent_path().parent_path().filename();
            auto game_root_directory = game_executable_directory.parent_path().parent_path().parent_path();
            if (!std::filesystem::exists(game_content_dir))
            {
                bool igd_flat_layout{};
#ifdef __PROSPERO__
                // [B3][IGD] Self-test call 134. A PS5 package has no Binaries/<Platform>
                // level: the executable sits directly in the app root (/app0/eboot.bin)
                // and the project directory is a child of it (/app0/<Project>/Content).
                // So the classic <Root>/<Game>/Binaries/Win64 derivation lands on
                // "/Content" and always missed. Same result shape as upstream: the
                // root is the app root, and the ONE child directory (not "Engine")
                // that contains a Content directory is the game directory, which the
                // walk below names "Game". Names compare case-insensitively (package
                // paths are often lower-case). Zero or several candidates keep
                // upstream's nil + warning (no guessing). error_code calls only.
                const auto igd_iequals = [](const std::string& a, const char* b) -> bool {
                    size_t i = 0;
                    for (; i < a.size() && b[i] != '\0'; ++i)
                    {
                        const char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
                        const char cb = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
                        if (ca != cb) { return false; }
                    }
                    return i == a.size() && b[i] == '\0';
                };
                const auto igd_has_content_child = [&](const std::filesystem::path& dir) -> bool {
                    std::error_code ec{};
                    std::filesystem::directory_iterator sub{dir, ec};
                    const std::filesystem::directory_iterator sub_end{};
                    while (!ec && sub != sub_end)
                    {
                        std::error_code entry_ec{};
                        if (sub->is_directory(entry_ec) && !entry_ec && igd_iequals(sub->path().filename().string(), "content")) { return true; }
                        sub.increment(ec);
                    }
                    return false;
                };
                std::error_code igd_ec{};
                std::filesystem::directory_iterator igd_it{game_executable_directory, igd_ec};
                const std::filesystem::directory_iterator igd_end{};
                int igd_candidates{};
                std::filesystem::path igd_game_name{};
                while (!igd_ec && igd_it != igd_end)
                {
                    std::error_code entry_ec{};
                    if (igd_it->is_directory(entry_ec) && !entry_ec)
                    {
                        const auto child_name = igd_it->path().filename();
                        if (!igd_iequals(child_name.string(), "engine") && igd_has_content_child(igd_it->path()))
                        {
                            ++igd_candidates;
                            igd_game_name = child_name;
                        }
                    }
                    igd_it.increment(igd_ec);
                }
                if (!igd_ec && igd_candidates == 1 && game_executable_directory != game_executable_directory.root_path())
                {
                    igd_flat_layout = true;
                    game_name = igd_game_name;
                    game_root_directory = game_executable_directory;
                }
#endif
                if (!igd_flat_layout)
                {
                    Output::send<LogLevel::Warning>(STR("IterateGameDirectories: Could not locate the root directory because the directory structure is unknown "
                                                        "(not <RootGamePath>/Game/Binaries/Win64)\n"));
                    lua.set_nil();
                    return 1;
                }
            }

            // A C function only gets LUA_MINSTACK (20) free slots and each level below holds two of them until
            // fuse_pair() runs, so a deep tree pushes past the end of the stack. api_incr_top's check is compiled
            // out in release, so it corrupts the heap instead of erroring.
            constexpr int32_t lua_stack_slots_per_level = 8;
            constexpr size_t max_directory_depth = 64;

            auto directories_table = lua.prepare_new_table();

            bool reported_depth_limit{};
            // Link targets on the current path. Every cycle goes through a link, so this breaks them all without
            // resolving the non-link entries.
            std::vector<std::filesystem::path> followed_links{};

            std::function<void(const std::filesystem::path&, LuaMadeSimple::Lua::Table&, size_t)> iterate_directory =
                    [&](const std::filesystem::path& directory, LuaMadeSimple::Lua::Table& current_directory_table, size_t depth) {
                        try
                        {
                            if (!lua_checkstack(lua.get_lua_state(), lua_stack_slots_per_level))
                            {
                                Output::send<LogLevel::Error>(STR("IterateGameDirectories: Could not grow the Lua stack while iterating {}, "
                                                                  "stopping traversal here\n"),
                                                              directory.wstring());
                                return;
                            }

                            if (depth >= max_directory_depth)
                            {
                                if (!reported_depth_limit)
                                {
                                    reported_depth_limit = true;
                                    Output::send<LogLevel::Warning>(STR("IterateGameDirectories: Reached the maximum directory depth of {} at {}. "
                                                                        "Directories below this are not included in the result.\n"),
                                                                    max_directory_depth,
                                                                    directory.wstring());
                                }
                            }
                            else
                            {
                                std::error_code ec;
                                for (const auto& item :
                                     std::filesystem::directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied, ec))
                                {
                                    std::error_code item_ec;
                                    if (!item.is_directory(item_ec) || item_ec)
                                    {
                                        continue;
                                    }

                                    bool following_link{};
                                    if (item.is_symlink(item_ec) && !item_ec)
                                    {
                                        auto link_target = std::filesystem::canonical(item.path(), item_ec);
                                        if (item_ec)
                                        {
                                            // Can't check an unresolvable link for a cycle, so don't descend into it.
                                            continue;
                                        }

                                        if (std::find(followed_links.begin(), followed_links.end(), link_target) != followed_links.end())
                                        {
                                            continue;
                                        }

                                        followed_links.emplace_back(std::move(link_target));
                                        following_link = true;
                                    }

                                    try
                                    {
                                        auto path = item.path().filename();

                                        // Set key to "Game" if this is the game directory, otherwise use the actual name
                                        std::string table_key;
                                        if (path == game_name)
                                        {
                                            table_key = "Game";
                                        }
                                        else
                                        {
                                            // TODO: When UE5 String conversion is implemented, replace with StringCast<ANSICHAR>
                                            table_key = to_utf8_string(path);
                                        }

                                        current_directory_table.add_key(table_key.c_str());
                                        auto next_directory_table = lua.prepare_new_table();

                                        // Recursively iterate the subdirectory
                                        iterate_directory(item.path(), next_directory_table, depth + 1);
                                        current_directory_table.fuse_pair();
                                    }
                                    catch (const std::exception& e)
                                    {
                                        Output::send<LogLevel::Error>(STR("Error processing directory entry: {}\n"), to_wstring(e.what()));
                                    }

                                    if (following_link)
                                    {
                                        followed_links.pop_back();
                                    }
                                }

                                if (ec)
                                {
                                    Output::send<LogLevel::Error>(STR("Error iterating directory {}: {}\n"), directory.wstring(), to_wstring(ec.message()));
                                }
                            }

                            auto meta_table = lua.prepare_new_table();

                            lua_pushcfunction(lua.get_lua_state(), [](lua_State* lua_state) -> int {
                                return TRY([&] {
                                    const auto& lua = LuaMadeSimple::Lua(lua_state);
                                    std::string name{};
                                    if (!lua.is_string(2))
                                    {
                                        return 0;
                                    }
                                    name = lua.get_string(2);

                                    if (name == "__name")
                                    {
                                        lua_getmetatable(lua_state, 1);
                                        lua_pushliteral(lua_state, "__name");
                                        lua_rawget(lua_state, 2);
                                        lua.discard_value();
                                        lua.discard_value();
                                        if (!lua.is_string())
                                        {
                                            throw std::runtime_error{"Couldn't find '__name' for directory entry."};
                                        }
                                        return 1;
                                    }
                                    else if (name == "__absolute_path")
                                    {
                                        lua_getmetatable(lua_state, 1);
                                        lua_pushliteral(lua_state, "__absolute_path");
                                        lua_rawget(lua_state, 2);
                                        lua.discard_value();
                                        lua.discard_value();
                                        if (!lua.is_string())
                                        {
                                            throw std::runtime_error{"Couldn't find '__absolute_path' for directory entry."};
                                        }
                                        return 1;
                                    }
                                    else if (name == "__files")
                                    {
                                        lua_getmetatable(lua_state, 1);
                                        lua_pushliteral(lua_state, "__absolute_path");
                                        lua_rawget(lua_state, 2);
                                        lua.discard_value();
                                        lua.discard_value();
                                        if (!lua.is_string())
                                        {
                                            throw std::runtime_error{"Couldn't find '__absolute_path' for directory entry."};
                                        }

                                        const auto path_str = lua.get_string();
                                        std::wstring path_wstr;

                                        // Try to convert the path string to wstring for filesystem operations
                                        try
                                        {
                                            path_wstr = RC::to_wstring(path_str);
                                        }
                                        catch (const std::exception&)
                                        {
                                            // If conversion fails, reconstruct path as basic conversion
                                            path_wstr.reserve(path_str.size());
                                            for (char c : path_str)
                                            {
                                                path_wstr.push_back(static_cast<wchar_t>(c));
                                            }

                                            // Check if the path exists first
                                            std::error_code path_ec;
                                            if (!std::filesystem::exists(path_wstr, path_ec))
                                            {
                                                // Path doesn't exist, try to reconstruct it
                                                if (path_str.find("LogicMods") != std::string::npos)
                                                {
                                                    // Get the game executable directory
                                                    std::filesystem::path game_exec_dir = UE4SSProgram::get_program().get_game_executable_directory();

                                                    // Navigate to content directory
                                                    std::filesystem::path content_dir = game_exec_dir;
                                                    content_dir = content_dir.parent_path(); // Up to Binaries
                                                    content_dir = content_dir.parent_path(); // Up to Game
                                                    content_dir /= "Content";

                                                    if (std::filesystem::exists(content_dir))
                                                    {
                                                        auto logic_mods_dir = content_dir / "Paks/LogicMods";

                                                        // Check if it exists or try to create it
                                                        if (!std::filesystem::exists(logic_mods_dir))
                                                        {
                                                            std::error_code dir_ec;
                                                            auto paks_dir = content_dir / "Paks";
                                                            if (!std::filesystem::exists(paks_dir))
                                                            {
                                                                std::filesystem::create_directory(paks_dir, dir_ec);
                                                            }
                                                            std::filesystem::create_directory(logic_mods_dir, dir_ec);
                                                        }

                                                        if (std::filesystem::exists(logic_mods_dir))
                                                        {
                                                            path_wstr = logic_mods_dir.wstring();
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        auto files_table = lua.prepare_new_table();
                                        auto index = 1;

                                        try
                                        {
                                            std::error_code ec;
                                            if (std::filesystem::exists(path_wstr, ec))
                                            {
                                                for (const auto& item :
                                                     std::filesystem::directory_iterator(path_wstr, std::filesystem::directory_options::skip_permission_denied, ec))
                                                {
                                                    try
                                                    {
                                                        std::error_code item_ec;
                                                        if (item.is_directory(item_ec) || item_ec)
                                                        {
                                                            continue;
                                                        }

                                                        if (!lua_checkstack(lua_state, 5))
                                                        {
                                                            Output::send<LogLevel::Error>(STR("Error iterating files in {}: could not grow the Lua stack\n"),
                                                                                          path_wstr);
                                                            break;
                                                        }

                                                        files_table.add_key(index);
                                                        auto file_table = lua.prepare_new_table();

                                                        // Create safe strings for filenames and paths
                                                        // TODO: When UE5 String conversion is implemented, replace with StringCast<ANSICHAR>
                                                        std::string safe_filename = to_utf8_string(item.path().filename());
                                                        std::string safe_path = to_utf8_string(item.path());

                                                        file_table.add_pair("__name", safe_filename.c_str());
                                                        file_table.add_pair("__absolute_path", safe_path.c_str());
                                                        files_table.fuse_pair();

                                                        // Only advance when something was added, otherwise skipped
                                                        // directories leave holes and '#files' stops being usable.
                                                        ++index;
                                                    }
                                                    catch (const std::exception& e)
                                                    {
                                                        Output::send<LogLevel::Error>(STR("Error processing file: {}\n"), to_wstring(e.what()));
                                                    }
                                                }
                                            }

                                            if (ec)
                                            {
                                                Output::send<LogLevel::Error>(STR("Error iterating files in {}: {}\n"), path_wstr, to_wstring(ec.message()));
                                            }
                                        }
                                        catch (const std::exception& e)
                                        {
                                            Output::send<LogLevel::Error>(STR("Error iterating files: {}\n"), to_wstring(e.what()));
                                        }

                                        return 1;
                                    }
                                    else
                                    {
                                        lua.set_nil();
                                        return 1;
                                    }
                                });
                            });

                            lua_setfield(lua.get_lua_state(), -2, "__index");

                            // Set metadata using safe string conversion
                            // TODO: When UE5 String conversion is implemented, replace with StringCast<ANSICHAR>
                            std::string safe_filename = to_utf8_string(directory.filename());
                            std::string safe_path = to_utf8_string(directory);

                            meta_table.add_pair("__name", safe_filename.c_str());
                            meta_table.add_pair("__absolute_path", safe_path.c_str());
                            lua_setmetatable(lua.get_lua_state(), -2);
                        }
                        catch (const std::exception& e)
                        {
                            Output::send<LogLevel::Error>(STR("Exception in iterate_directory: {}\n"), to_wstring(e.what()));
                        }
                    };

            try
            {
                iterate_directory(game_root_directory, directories_table, 0);
            }
            catch (const std::exception& e)
            {
                Output::send<LogLevel::Error>(STR("Exception in IterateGameDirectories: {}\n"), to_wstring(e.what()));
                lua.set_nil();
                return 1;
            }

            return 1;
        });

        lua.register_function("CreateLogicModsDirectory", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'CreateLogicModsDirectory'.
Overloads:
#1: CreateLogicModsDirectory())"};
            try
            {
                std::filesystem::path game_executable_directory = UE4SSProgram::get_program().get_game_executable_directory();
                auto game_content_dir = game_executable_directory.parent_path().parent_path() / "Content";
                if (!std::filesystem::exists(game_content_dir))
                {
                    lua.throw_error("CreateLogicModsDirectory: Could not locate the \"Content\" directory because the directory structure is unknown (not "
                                    "<RootGamePath>/Game/Content)\n");
                }

                auto logic_mods_dir = game_content_dir / "Paks/LogicMods";

                std::error_code ec;
                if (std::filesystem::exists(logic_mods_dir, ec))
                {
                    Output::send<LogLevel::Warning>(
                            STR("CreateLogicModsDirectory: \"LogicMods\" directory already exists. Cancelling creation of new directory.\n"));
                    lua.set_bool(true);
                    return 1;
                }

                // Try to create the Paks directory first if it doesn't exist
                auto paks_dir = game_content_dir / "Paks";
                if (!std::filesystem::exists(paks_dir, ec))
                {
                    ec.clear();
                    bool paks_created = std::filesystem::create_directory(paks_dir, ec);
                    if (!paks_created || ec)
                    {
                        Output::send<LogLevel::Error>(STR("CreateLogicModsDirectory: Failed to create Paks directory: {}\n"), to_wstring(ec.message()));
                        // Try to continue anyway
                    }
                }

                // Now create the LogicMods directory
                ec.clear();
                bool created = std::filesystem::create_directory(logic_mods_dir, ec);

                if (!created || ec)
                {
                    Output::send<LogLevel::Error>(STR("CreateLogicModsDirectory: Error creating directory: {}\n"), to_wstring(ec.message()));

                    // Check if the directory exists despite the error (might happen with Unicode paths)
                    ec.clear();
                    if (std::filesystem::exists(logic_mods_dir, ec))
                    {
                        lua.set_bool(true);
                        return 1;
                    }

                    lua.throw_error("CreateLogicModsDirectory: Unable to create \"LogicMods\" directory. Try creating manually.\n");
                }

                Output::send<LogLevel::Warning>(STR("CreateLogicModsDirectory: LogicMods directory created.\n"));

                lua.set_bool(true);
                return 1;
            }
            catch (const std::exception& e)
            {
                Output::send<LogLevel::Error>(STR("Exception in CreateLogicModsDirectory: {}\n"), to_wstring(e.what()));
                lua.throw_error(e.what());
                return 0;
            }
        });

        lua.register_function("ExecuteAsync", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteAsync'.
Overloads:
#1: ExecuteAsync(LuaFunction Callback))"};

            auto mod = get_mod_ref(lua);

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            const int32_t lua_function_ref = lua.registry().make_ref();

            {
                const ActionsLockGuard actions_guard{mod};
                mod->m_pending_actions.emplace_back(LuaMod::AsyncAction{lua_function_ref, LuaMod::ActionType::Immediate});
            }

            return 0;
        });

        lua.register_function("ExecuteWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteWithDelay'.
Overloads:
#1: ExecuteWithDelay(integer DelayInMilliseconds, LuaFunction Callback))"};

            if (!lua.is_integer())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            int64_t delay = lua.get_integer();

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            const int32_t lua_function_ref = lua.registry().make_ref();

            auto mod = get_mod_ref(lua);

            {
                const ActionsLockGuard actions_guard{mod};
                mod->m_pending_actions.emplace_back(LuaMod::AsyncAction{
                        lua_function_ref,
                        LuaMod::ActionType::Delayed,
                        std::chrono::steady_clock::now(),
                        delay,
                });
            }
            return 0;
        });

        lua.register_function("LoopAsync", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoopAsync'.
Overloads:
#1: LoopAsync(integer DelayInMilliseconds, LuaFunction Callback))"};

            if (!lua.is_integer())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            int64_t delay = lua.get_integer();

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            const int32_t lua_function_ref = lua.registry().make_ref();

            auto mod = get_mod_ref(lua);

            {
                const ActionsLockGuard actions_guard{mod};
                mod->m_pending_actions.emplace_back(LuaMod::AsyncAction{
                        lua_function_ref,
                        LuaMod::ActionType::Loop,
                        std::chrono::steady_clock::now(),
                        delay,
                });
            }

            return 0;
        });

        lua.register_function("RegisterProcessConsoleExecPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterProcessConsoleExecPreHook'.
Overloads:
#1: RegisterProcessConsoleExecPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_process_console_exec_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterProcessConsoleExecPreHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterProcessConsoleExecPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterProcessConsoleExecPostHook'.
Overloads:
#1: RegisterProcessConsoleExecPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_process_console_exec_post_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterProcessConsoleExecPostHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterCallFunctionByNameWithArgumentsPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterCallFunctionByNameWithArgumentsPreHook'.
Overloads:
#1: RegisterCallFunctionByNameWithArgumentsPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_call_function_by_name_with_arguments_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterCallFunctionByNameWithArgumentsPreHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterCallFunctionByNameWithArgumentsPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterCallFunctionByNameWithArgumentsPostHook'.
Overloads:
#1: RegisterCallFunctionByNameWithArgumentsPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_call_function_by_name_with_arguments_post_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterCallFunctionByNameWithArgumentsPostHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterULocalPlayerExecPreHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterULocalPlayerExecPreHook'.
Overloads:
#1: RegisterULocalPlayerExecPreHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_local_player_exec_pre_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterULocalPlayerExecPreHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterULocalPlayerExecPostHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterULocalPlayerExecPostHook'.
Overloads:
#1: RegisterULocalPlayerExecPostHook(LuaFunction Callback))"};

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto mod = get_mod_ref(lua);
            auto hook_lua = get_hook_lua(mod);
            callback = &LuaMod::m_local_player_exec_post_callbacks.emplace_back(LuaMod::LuaCallbackData{hook_lua, nullptr, {}});
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(hook_lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterULocalPlayerExecPostHook", StringType{STR("-")}, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterConsoleCommandGlobalHandler", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterConsoleCommandGlobalHandler'.
Overloads:
#1: RegisterConsoleCommandGlobalHandler(string CommandName, LuaFunction Callback))"};

            if (!lua.is_string())
            {
                throw std::runtime_error{error_overload_not_found};
            }
#ifdef __PROSPERO__
            // [LFB] agent-FD (Claude Q1 extension): the console-command name
            // keys the dispatch map matched against engine console command
            // names — a Lua string -> engine-name-shaped StringType push.
            // ONE hand converter (was ensure_str); same .string() shape as
            // the other routed StringType sites (RegisterCustomEvent etc.).
            auto command_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.string();
#else
            auto command_name = ensure_str(lua.get_string());
#endif

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto iter = LuaMod::m_global_command_lua_callbacks.find(command_name);
            if (iter == LuaMod::m_global_command_lua_callbacks.end())
            {
                auto mod = get_mod_ref(lua);
                auto hook_lua = get_hook_lua(mod);
                callback = &LuaMod::m_global_command_lua_callbacks.emplace(command_name, LuaMod::LuaCallbackData{hook_lua, nullptr, {}}).first->second;
            }
            else
            {
                callback = &iter->second;
            }
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(&lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterConsoleCommandGlobalHandler", command_name, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("RegisterConsoleCommandHandler", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RegisterConsoleCommandHandler'.
Overloads:
#1: RegisterConsoleCommandHandler(string CommandName, LuaFunction Callback))"};

            if (!lua.is_string())
            {
                throw std::runtime_error{error_overload_not_found};
            }
#ifdef __PROSPERO__
            // [LFB] agent-FD (Claude Q1 extension): same ONE-converter route
            // as RegisterConsoleCommandGlobalHandler above (was ensure_str).
            auto command_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.string();
#else
            auto command_name = ensure_str(lua.get_string());
#endif

            if (!lua.is_function())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            LuaMod::LuaCallbackData* callback = nullptr;
            auto iter = LuaMod::m_custom_command_lua_pre_callbacks.find(command_name);
            if (iter == LuaMod::m_custom_command_lua_pre_callbacks.end())
            {
                auto mod = get_mod_ref(lua);
                auto hook_lua = get_hook_lua(mod);
                callback = &LuaMod::m_custom_command_lua_pre_callbacks.emplace(command_name, LuaMod::LuaCallbackData{hook_lua, nullptr, {}}).first->second;
            }
            else
            {
                callback = &iter->second;
            }
            lua_xmove(lua.get_lua_state(), callback->lua->get_lua_state(), 1);
            const int32_t lua_function_ref = callback->lua->registry().make_ref();
            callback->registry_indexes.emplace_back(&lua, LuaMod::LuaCallbackData::RegistryIndex{lua_function_ref});
            refused_log::accepted(lua, "RegisterConsoleCommandHandler", command_name, refused_log::Family::EagerDetour);
            return 0;
        });

        lua.register_function("LoadAsset", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoadAsset'.
Overloads:
#1: LoadAsset(string AssetPathAndName))"};

            if (!Unreal::IsInGameThread())
            {
                throw std::runtime_error{"Function 'LoadAsset' can only be called from within the game thread"};
            }

            if (!lua.is_string())
            {
                throw std::runtime_error{error_overload_not_found};
            }
            // [LFB] ONE hand converter, slot-2 ctor direct (was ensure_str +
            // FString-ctor seam); FNAME_Add kept.
            auto asset_path_and_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.to_fname(Unreal::FNAME_Add);

            auto* asset_registry = static_cast<Unreal::UAssetRegistry*>(Unreal::UAssetRegistryHelpers::GetAssetRegistry().ObjectPointer);
            if (!asset_registry)
            {
                throw std::runtime_error{"Did not load assets because asset_registry was nullptr\n"};
            }

            Unreal::UObject* loaded_asset{};
            bool was_asset_found{};
            bool did_asset_load{};
            Unreal::FAssetData asset_data = asset_registry->GetAssetByObjectPath(asset_path_and_name);
            if ((Unreal::Version::IsAtMost(5, 0) && asset_data.ObjectPath().GetComparisonIndex()) || asset_data.PackageName().GetComparisonIndex())
            {
                was_asset_found = true;
                loaded_asset = Unreal::UAssetRegistryHelpers::GetAsset(asset_data);
                if (loaded_asset)
                {
                    did_asset_load = true;
                    Output::send(STR("Asset loaded\n"));
                }
                else
                {
                    Output::send(STR("Asset was found but not loaded, could be a package\n"));
                }
            }

            LuaType::auto_construct_object(lua, loaded_asset);
            lua.set_bool(was_asset_found);
            lua.set_bool(did_asset_load);
            return 3;
        });

        lua.register_function("FindObject", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'FindObject'.
Overloads:
#1: FindObject(UClass InClass, UObject|UClass InOuter, string Name, bool ExactClass)
#2: FindObject(string|FName|nil ClassName, string|FName|nil ObjectShortName, EObjectFlags RequiredFlags, EObjectFlags BannedFlags)
#3: FindObject(UClass|nil Class, string|FName|nil ObjectShortName, EObjectFlags RequiredFlags, EObjectFlags BannedFlags))"};

            if (!lua.is_string() && !lua.is_userdata() && !lua.is_nil())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            Unreal::FName object_class_name{};
            Unreal::UClass* in_class{};
            bool could_be_in_class{};
            if (lua.is_string())
            {
                // [LFB] ONE hand converter, slot-2 ctor direct (was
                // ensure_str + FString-ctor seam); FNAME_Add kept.
                object_class_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.to_fname(Unreal::FNAME_Add);
            }
            else if (lua.is_userdata())
            {
                // The API is a bit awkward, we have to tell it to preserve the stack
                // That way, when we call 'get_userdata' again with a more specific type, there's still something to actually get
                auto& userdata = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
                if (std::string_view{userdata.get_object_name()} == "UClass")
                {
                    in_class = lua.get_userdata<LuaType::UClass>().get_remote_cpp_object();
                    could_be_in_class = true;
                    object_class_name = in_class->GetNamePrivate();
                }
                else if (std::string_view{userdata.get_object_name()} == "FName")
                {
                    object_class_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
                }
                else
                {
                    throw std::runtime_error{error_overload_not_found};
                }
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
                could_be_in_class = true;
            }
            else
            {
                throw std::runtime_error{error_overload_not_found};
            }

            if (!lua.is_string() && !lua.is_userdata() && !lua.is_integer() && !lua.is_nil())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            Unreal::FName object_short_name{};
            Unreal::UObject* in_outer{};
            bool could_be_in_outer{};
            bool could_be_object_short_name{};
            if (lua.is_string())
            {
                // [LFB] ONE hand converter, slot-2 ctor direct (was
                // ensure_str + FString-ctor seam); FNAME_Add kept.
                object_short_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.to_fname(Unreal::FNAME_Add);
                could_be_object_short_name = true;
            }
            else if (lua.is_userdata())
            {
                // The API is a bit awkward, we have to tell it to preserve the stack
                // That way, when we call 'get_userdata' again with a more specific type, there's still something to actually get
                auto& userdata = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
                std::string_view lua_object_name = userdata.get_object_name();
                // TODO: Redo when there's a bette way of checking whether a lua object is derived from UObject
                if (lua_object_name == "UObject" || lua_object_name == "UWorld" || lua_object_name == "AActor" || lua_object_name == "UClass")
                {
                    if (lua_object_name == "UClass")
                    {
                        in_outer = lua.get_userdata<LuaType::UClass>().get_remote_cpp_object();
                    }
                    else
                    {
                        in_outer = lua.get_userdata<LuaType::UObject>().get_remote_cpp_object();
                    }
                    could_be_in_outer = true;
                }
                else if (lua_object_name == "FName")
                {
                    object_short_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
                    could_be_object_short_name = true;
                }
                else
                {
                    throw std::runtime_error{error_overload_not_found};
                }
            }
            else if (lua.is_integer())
            {
                if (lua.get_integer() == -1)
                {
                    in_outer = Unreal::UObjectGlobals::ANY_PACKAGE;
                    could_be_in_outer = true;
                }
                else
                {
                    throw std::runtime_error{error_overload_not_found};
                }
            }
            else if (lua.is_nil())
            {
                could_be_in_outer = true;
                could_be_object_short_name = true;
                lua.discard_value();
            }
            else
            {
                throw std::runtime_error{error_overload_not_found};
            }

            int32_t required_flags{Unreal::EObjectFlags::RF_NoFlags};
            std::string in_name{};
            bool could_be_in_name{};
            if (lua.is_string())
            {
                in_name = lua.get_string();
                could_be_in_name = true;
            }
            else if (lua.is_integer())
            {
                required_flags = static_cast<int32_t>(lua.get_integer());
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }

            int32_t banned_flags{Unreal::EObjectFlags::RF_NoFlags};
            bool exact_class{};
            bool could_be_exact_class{};
            if (lua.is_bool())
            {
                exact_class = lua.get_bool();
                could_be_exact_class = true;
            }
            else if (lua.is_integer())
            {
                banned_flags = static_cast<int32_t>(lua.get_integer());
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }

            if (could_be_in_class && could_be_in_outer && could_be_in_name)
            {
                // [LFB] ONE hand converter over the already-consumed positional
                // string (memory-source variant; was ensure_str(in_name)).
                const LuaType::LuaFNameBridge::Bridge in_name_bridge{in_name.data(), in_name.size()};
                LuaType::auto_construct_object(lua, Unreal::UObjectGlobals::FindObject(in_class, in_outer, in_name_bridge.string(), exact_class));
            }
            else
            {
                if (could_be_exact_class || !could_be_object_short_name)
                {
                    throw std::runtime_error{error_overload_not_found};
                }
                LuaType::auto_construct_object(lua, Unreal::UObjectGlobals::FindObject(object_class_name, object_short_name, required_flags, banned_flags));
            }
            return 1;
        });

        lua.register_function("FindObjects", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'FindObjects'.
Overloads:
#1: FindObjects(integer NumObjectsToFind, string|FName|nil ClassName, string|FName|nil ObjectShortName, EObjectFlags RequiredFlags, EObjectFlags BannedFlags, bool bExactClass)
#2: FindObjects(integer NumObjectsToFind, UClass|nil Class, string|FName|nil ObjectShortName, EObjectFlags RequiredFlags, EObjectFlags BannedFlags, bool bExactClass))"};

            int32_t num_objects_to_find{};
            if (lua.is_integer())
            {
                if (num_objects_to_find < 0)
                {
                    throw std::runtime_error{error_overload_not_found};
                }
                num_objects_to_find = static_cast<int32_t>(lua.get_integer());
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }
            else
            {
                throw std::runtime_error{error_overload_not_found};
            }

            if (!lua.is_string() && !lua.is_userdata() && !lua.is_nil())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            Unreal::FName object_class_name{};
            bool object_class_name_supplied{true};
            if (lua.is_string())
            {
                // [LFB] ONE hand converter, slot-2 ctor direct (was
                // ensure_str + FString-ctor seam); FNAME_Add kept.
                object_class_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.to_fname(Unreal::FNAME_Add);
            }
            else if (lua.is_userdata())
            {
                // The API is a bit awkward, we have to tell it to preserve the stack
                // That way, when we call 'get_userdata' again with a more specific type, there's still something to actually get
                auto& userdata = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
                if (std::string_view{userdata.get_object_name()} == "UClass")
                {
                    object_class_name = lua.get_userdata<LuaType::UClass>().get_remote_cpp_object()->GetNamePrivate();
                }
                else if (std::string_view{userdata.get_object_name()} == "FName")
                {
                    object_class_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
                }
                else
                {
                    throw std::runtime_error{error_overload_not_found};
                }
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
                object_class_name_supplied = false;
            }
            else
            {
                throw std::runtime_error{error_overload_not_found};
            }

            if (!lua.is_string() && !lua.is_userdata() && !lua.is_nil())
            {
                throw std::runtime_error{error_overload_not_found};
            }

            Unreal::FName object_short_name{};
            if (lua.is_string())
            {
                // [LFB] ONE hand converter, slot-2 ctor direct (was
                // ensure_str + FString-ctor seam); FNAME_Add kept.
                object_short_name = LuaType::LuaFNameBridge::Bridge{lua.get_lua_state()}.to_fname(Unreal::FNAME_Add);
            }
            else if (lua.is_userdata())
            {
                // The API is a bit awkward, we have to tell it to preserve the stack
                // That way, when we call 'get_userdata' again with a more specific type, there's still something to actually get
                auto& userdata = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
                if (std::string_view{userdata.get_object_name()} == "FName")
                {
                    object_short_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
                }
                else
                {
                    throw std::runtime_error{error_overload_not_found};
                }
            }
            else if (lua.is_nil())
            {
                if (!object_class_name_supplied)
                {
                    error_overload_not_found.append("\nBoth param #1 and param #2 cannot be nil");
                    throw std::runtime_error{error_overload_not_found};
                }
                else
                {
                    lua.discard_value();
                }
            }
            else
            {
                throw std::runtime_error{error_overload_not_found};
            }

            int32_t required_flags{Unreal::EObjectFlags::RF_NoFlags};
            if (lua.is_integer())
            {
                required_flags = static_cast<int32_t>(lua.get_integer());
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }

            int32_t banned_flags{Unreal::EObjectFlags::RF_NoFlags};
            if (lua.is_integer())
            {
                banned_flags = static_cast<int32_t>(lua.get_integer());
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }

            bool exact_class{true};
            if (lua.is_integer())
            {
                exact_class = lua.get_integer();
            }
            else if (lua.is_bool())
            {
                exact_class = lua.get_bool();
            }
            else if (lua.is_nil())
            {
                lua.discard_value();
            }

            std::vector<Unreal::UObject*> objects_found{};
            Unreal::UObjectGlobals::FindObjects(static_cast<size_t>(num_objects_to_find),
                                                object_class_name,
                                                object_short_name,
                                                objects_found,
                                                required_flags,
                                                banned_flags,
                                                exact_class);

            auto table = lua.prepare_new_table(static_cast<int32_t>(objects_found.size()));
            for (size_t i = 0; i < objects_found.size(); ++i)
            {
                table.add_key(i + 1);
                LuaType::auto_construct_object(lua, objects_found[i]);
                table.fuse_pair();
            }

            return 1;
        });

        lua.register_function("GetCurrentThreadId", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetCurrentThreadId'.
Overloads:
#1: GetCurrentThreadId())"};

            LuaType::ThreadId::construct(lua, std::this_thread::get_id());

            return 1;
        });

        lua.register_function("GetMainModThreadId", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetMainModThreadId'.
Overloads:
#1: GetMainModThreadId())"};

            const auto mod = get_mod_ref(lua);
            LuaType::ThreadId::construct(lua, mod->get_main_thread_id());

            return 1;
        });

        lua.register_function("GetAsyncThreadId", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetAsyncThreadId'.
Overloads:
#1: GetAsyncThreadId())"};

            const auto mod = get_mod_ref(lua);
            LuaType::ThreadId::construct(lua, mod->get_async_thread_id());

            return 1;
        });

        lua.register_function("GetGameThreadId", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetGameThreadId'.
Overloads:
#1: GetGameThreadId())"};

            LuaType::ThreadId::construct(lua, Unreal::GetGameThreadId());

            return 1;
        });

        lua.register_function("IsInMainModThread", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsInMainModThread'.
Overloads:
#1: IsInMainModThread())"};

            const auto mod = get_mod_ref(lua);
            lua.set_bool(std::this_thread::get_id() == mod->get_main_thread_id());

            return 1;
        });

        lua.register_function("IsInAsyncThread", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsInAsyncThread'.
Overloads:
#1: IsInAsyncThread())"};

            const auto mod = get_mod_ref(lua);
            lua.set_bool(std::this_thread::get_id() == mod->get_async_thread_id());

            return 1;
        });

        lua.register_function("IsInGameThread", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsInGameThread'.
Overloads:
#1: IsInGameThread())"};

            lua.set_bool(std::this_thread::get_id() == Unreal::GetGameThreadId());

            return 1;
        });
    }

    auto LuaMod::setup_lua_global_functions(const LuaMadeSimple::Lua& lua) const -> void
    {
        setup_lua_global_functions_internal(lua, IsTrueMod::Yes);
    }

#ifdef __PROSPERO__
    namespace
    {
        // This protects Lua action state, not arbitrary engine code or memory.
        // Uninstall already uses the same mutex through lua_close.
        thread_local bool NativeLuaActionDispatching{};
        Unreal::Hook::GlobalCallbackId NativeProcessEventHookId{Unreal::Hook::ERROR_ID};

        class NativeLuaActionDispatchScope
        {
            std::unique_lock<std::recursive_mutex> Lock{LuaMod::m_thread_actions_mutex};
            bool PreviousProcessing{LuaMod::m_is_processing_actions};
            bool OwnsDispatch{!PreviousProcessing && !NativeLuaActionDispatching};
        public:
            NativeLuaActionDispatchScope()
            {
                if (OwnsDispatch)
                {
                    LuaMod::m_is_processing_actions = true;
                    NativeLuaActionDispatching = true;
                }
            }
            ~NativeLuaActionDispatchScope()
            {
                if (OwnsDispatch)
                {
                    LuaMod::m_is_processing_actions = PreviousProcessing;
                    NativeLuaActionDispatching = false;
                }
            }
            NativeLuaActionDispatchScope(const NativeLuaActionDispatchScope&) = delete;
            auto owns_dispatch() const noexcept -> bool { return OwnsDispatch; }
        };
    }
#endif

    // [R3-L10LM] File-local executor tag for the FuncPtr-marshal drain (this
    // rebase edition, worker-luamod-rebase 2026-10-04). DIFF-1's SettingsManager.hpp
    // enum append is deliberately NOT landed in this edition: the appended
    // enumerator's value would be 2 (ProcessEvent=0, EngineTick=1 in the base
    // enum, appended last per DIFF-1 hunk 1), so a file-local constexpr with
    // that exact value is assignment-/comparison-/template-argument-compatible
    // WITHOUT touching the shared header - zero additional recompile set. The
    // named Lua table key below is a string, not the C++ identifier, unchanged.
    static constexpr auto kFuncPtrMarshalExecutor = static_cast<GameThreadExecutionMethod>(2);

    // =====================================================================================
    // [R3] FuncPtr-marshal state (DIFF 1, worker-r3-diffs-authoring 2026-10-04;
    // R3-TIER-DESIGN 4/5.2). The R3-compatible game-thread pump: ONE carrier UFunction
    // ("the calibration-selected carrier", R3D 4.2) is R3-hooked via RegisterPostHook
    // (UFunction.cpp:91-101 -> GetFunctionHookData :78-89: save current FuncPtr as the
    // original, then SetFuncPtr(&Internal::UnrealScriptFunctionHook) - ONE aligned
    // pointer store into already-RW heap memory, zero syscalls). Its post-callback body
    // (funcptr_marshal_hook below) drains the SAME m_game_thread_actions queue
    // process_event_hook drains. Arming is parameterized by the carrier's FName id
    // ([L26][P2] the former arm_funcptr_marshal_drain ranked-native arm was removed;
    // arming is now exclusively arm_event_carrier_locked in the event-carrier
    // family). File scope by design: no LuaMod.hpp change,
    // so no additional unit recompiles.
    // =====================================================================================
    static std::atomic<bool> m_funcptr_marshal_armed{false};
    static int32_t m_funcptr_marshal_callback_id{-1};
    static Unreal::FName m_funcptr_marshal_carrier_id{};
    [[maybe_unused]] static bool m_funcptr_marshal_identity_published{false};
    [[maybe_unused]] static bool m_funcptr_marshal_pump_pending_logged{false};

    // Backend-reality predicate (R3D 4.5): "carrier armed" module state, NOT an AOB
    // address is_ready() - the ExecuteInGameThread/WithDelay availability trap of R3D 3
    // (address-availability is not arming) cannot recur through this path.
    static auto is_funcptr_marshal_armed() -> bool
    {
        return m_funcptr_marshal_armed.load(std::memory_order_acquire);
    }

#ifdef __PROSPERO__
    // [R3-LADDER] (worker-r3-ladder 2026-10-05; RUNBOOK l11-marshal-integration
    // 5.1 item 1, the WithDelay-family remainder) Usable-pump predicate for the
    // ladder re-order. The marshal is the game-thread pump only while the rev-B
    // pairing law holds: the initializer deferred its EngineTick registrations
    // (R3MarshalPumpPending, DIFF 2). armed && !pump_pending is that pairing
    // contradiction - a marshal armed while the initializer owns the pump -
    // and is treated as a PROVABLY DEAD drain: the ExecuteInGameThread /
    // WithDelay-family ladders refuse loudly instead of silently queueing onto
    // it or silently arming a second EngineTick pump. Deliberately NOT a
    // "drain succeeded" test: between arming and the first carrier invocation
    // the identity publication is legitimately pending, and queueing then is
    // the designed lazy semantics (R3D 4.4a); publication refusal remains the
    // drain body's own fail-closed gate (funcptr_marshal_hook).
    static auto r3_marshal_lazy_arm() -> void; // defined after the calibration consumer
    static auto funcptr_marshal_usable() -> bool
    {
        // [claude-cloud 2026-10-06 event-carrier] pump_pending means the initializer
        // deferred every EngineTick registration, so the marshal is the ONLY possible
        // game-thread pump in this world whether or not a carrier has armed yet. Routing
        // to it while unarmed is the designed lazy semantics (R3D 4.4a: closures await
        // arming); falling through to the EngineTick/ProcessEvent ladders instead strands
        // closures behind refused detours. Arming is (re)attempted here, rate-limited.
        if (!Unreal::R3MarshalPumpPending())
        {
            return false;
        }
        r3_marshal_lazy_arm();
        return true;
    }

    // [claude-cloud 2026-10-06 event-carrier] Blueprint-override carrier family state.
    static std::mutex m_funcptr_marshal_carrier_mutex{};
    static std::vector<Unreal::UFunction*> m_funcptr_marshal_event_carriers{};
    static std::atomic<int64_t> m_funcptr_marshal_last_fire_ms{0};
    static std::atomic<int64_t> m_funcptr_marshal_last_scan_ms{0};
    static auto r3_now_ms() -> int64_t
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
#endif

    // [R9-EIGT] One exception that escapes TRY (e.g. std::bad_alloc thrown by
    // TRY's own "Error: {}" log line once the heap is exhausted) used to skip
    // the "= false" below and leave m_is_currently_executing_game_action TRUE
    // forever: from then on every ExecuteInGameThread / delayed / loop action is
    // only deferred, never run (launch 2: pump alive to the end, depth 2 -> 41,
    // no drain after decoded:3473). The guard restores the flag on every exit,
    // including unwinding. Normal-path behavior is unchanged (PC semantics).
    // =====================================================================================
    // [R9-PERF] measurement build (reviewer, launch 4): one line per second, game thread only,
    // fixed storage (no allocation per frame). Attributes the stutter to our runtime or a mod.
    //   frame: interval between game-thread pump (carrier) invocations ~ frame time
    //   pe:    process_event_hook calls on the game thread, dispatch-lock takes, total time
    //   pump:  funcptr_marshal_hook total time
    //   run:   game-thread Lua actions (ExecuteInGameThread / delayed / loops) by Lua state
    // State -> mod names are printed once per mod as [PERF-MAP].
    // =====================================================================================
    namespace Unreal::UObjectGlobals
    {
        auto r9_take_walk_stats(uint64_t& calls, uint64_t& ns) -> void; // [R9-PERF] UObjectGlobals.cpp
        auto r9_take_index_stats(uint64_t& del_calls, uint64_t& del_ticks, uint64_t& sync_calls, uint64_t& sync_ticks, uint64_t& watch, uint64_t& cuts) -> void;
    }
    namespace r9perf
    {
        inline auto now_ns() -> int64_t
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        struct StateSlot
        {
            const void* state;
            int64_t ns;
            uint32_t calls;
        };
        inline int64_t pe_calls{0}, pe_locks{0}, pe_ns{0}, pump_calls{0}, pump_ns{0};
        inline int64_t run_ns{0}, run_calls{0}, run_other_ns{0}, run_max_ns{0};
        inline StateSlot slots[16]{};
        inline int nslots{0};
        inline int64_t frame_last_ns{0}, frame_max_ns{0}, frame_sum_ns{0}, frames{0}, frames_over_40ms{0}, window_start_ns{0};
        inline uint64_t tsc_window_start{0};

        inline auto add_run(const void* state, int64_t ns) -> void
        {
            run_ns += ns;
            ++run_calls;
            if (ns > run_max_ns) { run_max_ns = ns; }
            for (int i = 0; i < nslots; ++i)
            {
                if (slots[i].state == state) { slots[i].ns += ns; ++slots[i].calls; return; }
            }
            if (nslots < 16) { slots[nslots++] = StateSlot{state, ns, 1}; return; }
            run_other_ns += ns;
        }

        inline auto emit_and_reset(int64_t now) -> void
        {
            try
            {
                const double win_ms = (now - window_start_ns) / 1e6;
                char narrow[1024];
                size_t used = 0;
                narrow[0] = '\0';
                for (int i = 0; i < nslots && used + 64 < sizeof(narrow); ++i)
                {
                    const int w = std::snprintf(narrow + used, sizeof(narrow) - used, " %p=%.1fms/%u", slots[i].state, slots[i].ns / 1e6, slots[i].calls);
                    if (w > 0) { used += static_cast<size_t>(w); }
                }
                StringType states{};
                for (const char* c = narrow; *c != '\0'; ++c)
                {
                    states.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*c)));
                }
                uint64_t walk_calls{0}, walk_ns{0};
                Unreal::UObjectGlobals::r9_take_walk_stats(walk_calls, walk_ns);
                // per Lua API cost: cycle ticks -> ms using this window's ticks per ns
                const uint64_t tsc_now = __builtin_readcyclecounter();
                const double ticks_per_ns = (tsc_window_start != 0 && now > window_start_ns) ? static_cast<double>(tsc_now - tsc_window_start) / static_cast<double>(now - window_start_ns) : 0.0;
                tsc_window_start = tsc_now;
                char fn_raw[640];
                uint64_t fn_total_ticks{0};
                LuaMadeSimple::take_function_stats(fn_raw, sizeof(fn_raw), 6, fn_total_ticks);
                // rewrite " name=ticks/calls" as " name=ms/calls"
                char fn_ms[720];
                size_t fused = 0;
                fn_ms[0] = '\0';
                for (const char* q = fn_raw; *q != '\0' && fused + 48 < sizeof(fn_ms);)
                {
                    const char* eq = q;
                    while (*eq != '\0' && *eq != '=') { ++eq; }
                    if (*eq != '=') { break; }
                    unsigned long long ticks = 0;
                    const char* sl = eq + 1;
                    while (*sl >= '0' && *sl <= '9') { ticks = ticks * 10 + static_cast<unsigned long long>(*sl - '0'); ++sl; }
                    if (*sl != '/') { break; }
                    const char* end = sl + 1;
                    while (*end >= '0' && *end <= '9') { ++end; }
                    const int w = std::snprintf(fn_ms + fused, sizeof(fn_ms) - fused, "%.*s=%.1fms/%.*s", static_cast<int>(eq - q), q,
                                                ticks_per_ns > 0 ? ticks / ticks_per_ns / 1e6 : -1.0, static_cast<int>(end - sl - 1), sl + 1);
                    if (w <= 0) { break; }
                    fused += static_cast<size_t>(w);
                    q = end;
                }
                StringType fn_wide{};
                for (const char* q = fn_ms; *q != '\0'; ++q) { fn_wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*q))); }
                const double fn_total_ms = ticks_per_ns > 0 ? fn_total_ticks / ticks_per_ns / 1e6 : -1.0;
                const auto tk = [&](uint64_t ticks) -> double { return ticks_per_ns > 0 ? ticks / ticks_per_ns / 1e6 : -1.0; };
                uint64_t del_calls{0}, del_ticks{0}, sync_calls{0}, sync_ticks{0}, idx_watch{0}, idx_cut{0};
                Unreal::UObjectGlobals::r9_take_index_stats(del_calls, del_ticks, sync_calls, sync_ticks, idx_watch, idx_cut);
                const uint64_t p_calls = LuaType::r9prop::take(LuaType::r9prop::calls), p_hits = LuaType::r9prop::take(LuaType::r9prop::hits);
                const double p_bridge = tk(LuaType::r9prop::take(LuaType::r9prop::t_bridge)), p_fname = tk(LuaType::r9prop::take(LuaType::r9prop::t_fname));
                const double p_custom = tk(LuaType::r9prop::take(LuaType::r9prop::t_custom)), p_find = tk(LuaType::r9prop::take(LuaType::r9prop::t_find));
                const double p_handle = tk(LuaType::r9prop::take(LuaType::r9prop::t_handle));
                Output::send(STR("[PERF] win_ms={:.0f} frames={} frame_avg_ms={:.1f} frame_max_ms={:.1f} frames_over_40ms={} | pe_calls={} pe_locks={} pe_ms={:.1f} | pump_calls={} pump_ms={:.1f} | run_calls={} run_ms={:.1f} run_max_ms={:.1f} other_ms={:.1f} | walks={} walk_ms={:.1f} walk_avg_ms={:.2f} | api_ms={:.1f}{} | del_calls={} del_ms={:.1f} idx_syncs={} idx_ms={:.1f} idx_watch={} idx_cut={} | prop calls={} cache_hits={} bridge_ms={:.1f} fname_ms={:.1f} custom_ms={:.1f} find_ms={:.1f} handle_ms={:.1f} |{}\n"),
                             win_ms, frames, frames ? frame_sum_ns / 1e6 / static_cast<double>(frames) : 0.0, frame_max_ns / 1e6, frames_over_40ms,
                             pe_calls, pe_locks, pe_ns / 1e6, pump_calls, pump_ns / 1e6, run_calls, run_ns / 1e6, run_max_ns / 1e6, run_other_ns / 1e6,
                             walk_calls, walk_ns / 1e6, walk_calls ? walk_ns / 1e6 / static_cast<double>(walk_calls) : 0.0, fn_total_ms, fn_wide, del_calls, tk(del_ticks), sync_calls, tk(sync_ticks), idx_watch, idx_cut,
                             p_calls, p_hits, p_bridge, p_fname, p_custom, p_find, p_handle, states);
            }
            catch (...)
            {
            }
            pe_calls = pe_locks = pe_ns = pump_calls = pump_ns = 0;
            run_ns = run_calls = run_other_ns = run_max_ns = 0;
            nslots = 0;
            frame_max_ns = frame_sum_ns = frames = frames_over_40ms = 0;
            window_start_ns = now;
        }

        // called once per pump invocation on the game thread
        inline auto frame_tick() -> void
        {
            const int64_t now = now_ns();
            if (window_start_ns == 0) { window_start_ns = now; }
            if (frame_last_ns != 0)
            {
                const int64_t d = now - frame_last_ns;
                ++frames;
                frame_sum_ns += d;
                if (d > frame_max_ns) { frame_max_ns = d; }
                if (d > 40'000'000) { ++frames_over_40ms; }
            }
            frame_last_ns = now;
            if (now - window_start_ns >= 1'000'000'000) { emit_and_reset(now); }
        }

        struct TimedScope
        {
            int64_t& calls;
            int64_t& total;
            int64_t t0;
            TimedScope(int64_t& c, int64_t& t) : calls(c), total(t), t0(now_ns()) { ++calls; }
            ~TimedScope() { total += now_ns() - t0; }
        };
    } // namespace r9perf

    class GameActionExecutingGuard
    {
      public:
        GameActionExecutingGuard() noexcept { LuaMod::m_is_currently_executing_game_action = true; }
        ~GameActionExecutingGuard() { LuaMod::m_is_currently_executing_game_action = false; }
        GameActionExecutingGuard(const GameActionExecutingGuard&) = delete;
        auto operator=(const GameActionExecutingGuard&) -> GameActionExecutingGuard& = delete;
    };

    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void
    {
        std::vector<LuaMod::SimpleLuaAction> to_run{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (actions.empty())
            {
                return;
            }
            to_run = std::move(actions);
            actions.clear();
        }

        std::vector<LuaMod::SimpleLuaAction> deferred{};
        for (auto& lua_data : to_run)
        {
            if (LuaMod::m_is_currently_executing_game_action)
            {
                deferred.emplace_back(std::move(lua_data));
                continue;
            }

            const int64_t run_t0 = r9perf::now_ns();
            const void* const run_state = lua_data.lua->get_lua_state();
            try
            {
                const GameActionExecutingGuard executing_guard{};
                TRY([&]() {
                    get_function_ref_cxx(*lua_data.lua, lua_data.lua_action_function_ref);
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] ExecuteInGameThread drained state={} ref={} (callback entry)\n"),
                                     static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                    });
                    lua_data.lua->call_function(0, 0);
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] ExecuteInGameThread callback returned state={} ref={}\n"),
                                     static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                    });
                });
                luaL_unref(lua_data.lua->get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_action_function_ref);
                r9perf::add_run(run_state, r9perf::now_ns() - run_t0);
            }
            catch (...)
            {
                // [R9-EIGT] an exception that escaped TRY must not drop the rest of to_run
                // or the deferred callbacks; release this action's ref and continue.
                luaL_unref(lua_data.lua->get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_action_function_ref);
                try { Output::send<LogLevel::Error>(STR("[R9-EIGT] game-thread action threw; continuing\n")); } catch (...) {}
            }
        }

        if (!deferred.empty())
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            actions.insert(
                actions.end(),
                std::make_move_iterator(deferred.begin()),
                std::make_move_iterator(deferred.end()));
        }
    }

    // Process pending NotifyOnNewObject callbacks that were queued from StaticConstructObject
    // This MUST run on the game thread because Lua is NOT thread-safe
    namespace Unreal::UObjectGlobals
    {
        auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj, bool (*BudgetExpired)()) -> bool; // [R9-NOTIFY] UObjectGlobals.cpp
        auto r9_index_disabled() -> bool; // [R11-WORLD] UObjectGlobals.cpp
        auto r9_index_snapshot_live(std::vector<int32_t>& OutIdx, std::vector<void*>& OutObj, bool (*Want)(Unreal::UClass*)) -> bool; // [EARLY-4] UObjectGlobals.cpp
    }

    // [R9-NOTIFY] NotifyOnNewObject producer for the PS5 (the StaticConstructObject seam is not
    // armed here, so registrations were accepted but never delivered). New objects come from the
    // delete-listener-fed object index; matching is the seam's own rule (class or any super with the
    // registered name AND outer name); delivery is the existing pending queue below, on the game
    // thread. Deliberate difference from PC: the callback runs on the next pump (<= one frame after
    // construction) instead of inside StaticConstructObject.
    // [R9-PACE] the game-thread pump runs on game event carriers, not once per frame (launch 10: ~800-2900
    // calls per second), so per-call budgets multiply. Each budgeted step instead gets a fixed amount of
    // time per ~16 ms window (about one frame), however often the pump fires; a call that finds its
    // window's budget used up skips the step (the work waits for the next window, nothing is dropped).
    struct R9Pacer
    {
        std::chrono::steady_clock::time_point window_start{};
        std::chrono::steady_clock::duration spent{};
        std::chrono::steady_clock::time_point call_start{};
        uint64_t skipped{0};
        auto begin(std::chrono::microseconds per_window, std::chrono::steady_clock::time_point& deadline) -> bool
        {
            const auto now = std::chrono::steady_clock::now();
            if (now - window_start >= std::chrono::microseconds(16667)) { window_start = now; spent = {}; }
            if (spent >= per_window) { ++skipped; return false; }
            call_start = now;
            deadline = now + (per_window - spent);
            return true;
        }
        auto end() -> void { spent += std::chrono::steady_clock::now() - call_start; }
    };
    struct R9PaceEnd
    {
        R9Pacer& pacer;
        ~R9PaceEnd() { pacer.end(); }
    };
    static R9Pacer s_r9_pace_watch{}, s_r9_pace_match{}, s_r9_pace_exec{};

    // =====================================================================================
    // [WORLD-EV] (round10 HOOKS track r2; generic for every UE game, no game/mod names, no new
    // offsets) Data-only delivery of the Lua map-load / InitGameState / BeginPlay hooks
    // (RegisterLoadMapPre/PostHook, RegisterInitGameStatePre/PostHook,
    // RegisterBeginPlayPre/PostHook). On PC these are code detours; on PS5 game code is
    // execute-only, so the detour block is skipped and the registrations used to be accepted
    // but never delivered. Route: the same delete-listener-fed object index that already
    // serves NotifyOnNewObject ([R9-NOTIFY]) reports objects that became live since the last
    // pump. Each new object's class chain is compared (pointer equality) with four engine
    // classes resolved once by path (/Script/Engine.Actor, .GameModeBase, .GameStateBase,
    // .World) and checked to be real classes (meta class named "Class", GameModeBase and
    // GameStateBase derive from Actor, World does not). A new GameStateBase means "a map
    // finished loading" (UEngine::LoadMap spawns one per load, after the GameMode, inside
    // the load); a new GameModeBase means "InitGameState ran"; a new Actor means "BeginPlay
    // ran".
    // One map load is NOT one index batch: objects that reuse freed slots are found by the
    // budgeted watch re-check, spread over many collects. So events are tied to a map by the
    // actor's Level (its outer), not by the batch:
    //   * the GameState's outer is the new map's persistent level. When it appears, LoadMap
    //     pre is queued at once and a "map window" opens for that level.
    //   * GameModes and actors whose level is not the current map's level wait in a hold list
    //     (max 5 s). If a GameState of their level appears in time they join its map window;
    //     otherwise GameModes / spawned actors are delivered and package-loaded (RF_WasLoaded)
    //     actors of another level are counted as streamed_unreported (streamed level / World
    //     Partition cell: their BeginPlay waits for the level to become visible, which this
    //     route cannot see).
    //   * the map window collects that level's GameModes and actors; it closes when no
    //     GameMode / loaded actor of the level arrived for 3 s (or 30 s after it opened, or
    //     when the next map's GameState / GameMode appears). Then InitGameState, BeginPlay and
    //     LoadMap post are queued in PC order.
    //   * after the window closed, spawned actors of the current level are queued at once;
    //     loaded ones wait in the hold list (a new map may reuse the level's address) and,
    //     if no GameState claims them, are delivered late (late_after_post counter).
    // Delivery is on the game thread, from the existing pump (process_pending_notify_on_new_
    // object_callbacks, called by every drain), time-paced (~1 ms per ~16 ms window,
    // [R9-PACE]) and contained per callback (one mod's Lua error never stops the others).
    // Honest timing differences vs PC (also in the one-line registration note):
    //   * everything is delivered AFTER the event; "pre" hooks also run after the event.
    //     LoadMap pre comes at the first pump after the GameState is seen; InitGameState,
    //     BeginPlay (of the load) and LoadMap post >= 3 s after the last load object is seen.
    //   * LoadMap args: Engine = invalid object (not looked up), World = invalid for pre (the
    //     old world is already gone) / the new world for post, URL = nil, PendingGame =
    //     invalid object, Error = "". Return values are ignored (the load already happened).
    //   * BeginPlay is PARTIAL (still counted as a gap until console-proven): actors of
    //     streamed levels are not reported; actors created and destroyed between two pumps
    //     are missed; actors spawned outside the current level wait up to 5 s.
    //   * an object that dies (or whose slot now holds another class) before delivery is
    //     skipped, never handed to Lua.
    // EndPlay stays REFUSED (counted): by the time the index sees an actor gone it is already
    // freed, so there is no live actor to hand to Lua, and the Blueprint ReceiveEndPlay
    // Func-swap route would consume script-thunk slots the pump carriers need.
    // FAIL CLOSED: a missing / non-class engine class or a switched-off object index turns the
    // route off with one [WORLD-EV] line + one counted [REFUSED] line per registered hook kind.
    // [R11-WORLD] an index that is merely not usable YET (no delete seen: launch 12 took ~2.5 min,
    // until the first level change) is waited for: one "pending" line at 60 s, live when usable.
    // =====================================================================================
    namespace world_ev
    {
        enum class Kind : uint8_t { LoadMapPre, InitGameState, BeginPlay, LoadMapPost };
        struct Ev // 24 bytes
        {
            Unreal::UObject* obj;
            Unreal::UStruct* cls; // class at observe time: re-checked at delivery (slot reuse)
            int32_t index;
            Kind kind;
        };
        struct Held // waits (max kHoldMs) for a GameState of its level
        {
            Ev ev;
            Unreal::UObject* level;
            int64_t t_ms;
            bool loaded;
        };
        static Unreal::UStruct* s_actor{nullptr};
        static Unreal::UStruct* s_game_mode{nullptr};
        static Unreal::UStruct* s_game_state{nullptr};
        static Unreal::UStruct* s_world{nullptr};
        static bool s_wanted{false}; // set by the feed under m_thread_actions_mutex (game thread)
        static bool s_classes_ok{false};
        static bool s_class_check_failed{false};
        static bool s_index_ok{false};
        static int s_resolve_tries{0};
        static int64_t s_first_wanted_ms{0};
        static bool s_pending_logged{false}; // [R11-WORLD]
        static int64_t s_last_resolve_ms{0};
        // events waiting for their map (FIFO from s_hold_pos)
        static std::vector<Held> s_hold{};
        static size_t s_hold_pos{0};
        // the current map (persistent level from its GameState) and its open load window
        static Unreal::UObject* s_map_level{nullptr};
        static bool s_window_open{false};
        static Ev s_window_gs{};
        static int64_t s_window_open_ms{0}, s_window_last_ms{0};
        static uint64_t s_window_joined{0};
        static std::vector<Ev> s_win_gm{}, s_win_actor{};
        // ordered delivery queue (FIFO from s_queue_pos)
        static std::vector<Ev> s_queue{};
        static size_t s_queue_pos{0};
        // actors spawned during play in the current map wait kSettleMs before BeginPlay delivery (a nested
        // carrier pump inside SpawnActor must never hand a half-constructed actor to a mod); t_ms = seen time
        static std::vector<Held> s_settle{};
        static size_t s_settle_pos{0};
        constexpr int64_t kSettleMs = 50;
        constexpr int kMaxResolveTries = 3;
        constexpr int64_t kHoldMs = 5000;       // hold list: max wait for a GameState of the same level
        constexpr int64_t kQuietMs = 3000;      // map window closes after this long without a load object
        constexpr int64_t kMaxWindowMs = 30000; // ... or this long after it opened
        constexpr size_t kMaxPending = size_t{1} << 15; // hold + window + queue (<= ~1.5 MB; libc heap is small)
        static uint64_t s_maps{0}, s_gms{0}, s_actors{0}, s_delivered{0}, s_skipped_dead{0}, s_dropped{0}, s_streamed_unreported{0}, s_late_after_post{0};
        static uint32_t s_errors{0};
        static R9Pacer s_pace{};
        static bool s_first_kind[4]{true, true, true, true};
        // [EARLY-4] startup replay. On PC UE4SS the mods start during engine init, before the first
        // map, so a hook registered in main.lua sees that map's LoadMap / InitGameState / BeginPlay.
        // On PS5 the loader injects the runtime late (launch 14: ~69 s after the game process started),
        // and objects alive when the index is first used are never reported as new. So, once, when
        // the route first goes live AND world hooks were registered during startup (mods' main scripts and
        // any ExecuteInGameThread callback that ran before the first pump after the start gate), every
        // live Actor that already existed is fed
        // through observe() exactly like a new one, BEFORE any newer object: the map that was already
        // loaded (or loading) is delivered in the normal order (LoadMap pre -> InitGameState ->
        // BeginPlay -> LoadMap post), ahead of every later map. Objects of the unprocessed new batch are
        // excluded (they are observed live), so no object is fed twice. The route never goes live before
        // startup closes (feed + note_index check s_startup_closed), so nothing is delivered live while
        // mods are still starting and then replayed again. More than one live GameState
        // (a map change in progress) = ambiguous order: the replay is skipped, loudly (fail closed).
        // Not filtered: actors already marked PendingKill/Garbage but not yet collected. That internal
        // flag bit is engine-version dependent and has no two-source layout value yet (cap #4 NOTES).
        static std::atomic<bool> s_mods_started{false}; // set once by UE4SSProgram after the start gate
        static bool s_startup_closed{false};
        static bool s_startup_wanted{false};
        static int s_replay_state{0}; // 0 idle, 1 requested, 2 running, 3 finished/skipped
        static std::vector<int32_t> s_rp_idx{};
        static std::vector<void*> s_rp_obj{};
        static size_t s_rp_pos{0};
        static uint64_t s_rp_fed{0};
        static int64_t s_rp_start_ms{0};
        static bool s_in_replay{false};
        static bool s_first_map_noted{false};

        static auto any_world_callbacks() -> bool // caller holds m_thread_actions_mutex
        {
            return !LuaMod::m_load_map_pre_callbacks.empty() || !LuaMod::m_load_map_post_callbacks.empty() ||
                   !LuaMod::m_init_game_state_pre_callbacks.empty() || !LuaMod::m_init_game_state_post_callbacks.empty() ||
                   !LuaMod::m_begin_play_pre_callbacks.empty() || !LuaMod::m_begin_play_post_callbacks.empty();
        }

        // Caller holds m_thread_actions_mutex. True only in the world where the detour block was
        // skipped (PS5 R3 marshal) and the route is not refused.
        static auto wanted_locked() -> bool
        {
            return refused_log::eager_detour_block_skipped.load(std::memory_order_acquire) &&
                   refused_log::world_route_state.load(std::memory_order_acquire) != 2 && any_world_callbacks();
        }

        template <typename V>
        static auto release(V& v) -> void // drop a big buffer instead of keeping its capacity
        {
            if (v.capacity() > 8192) { V{}.swap(v); } else { v.clear(); }
        }

        // Never throws (an allocation failure is counted as a drop instead of leaving the pump).
        template <typename T>
        static auto push_safe(std::vector<T>& v, const T& e) -> bool
        {
            try
            {
                v.push_back(e);
                return true;
            }
            catch (...)
            {
                ++s_dropped;
                return false;
            }
        }

        // FIFO compaction: the delivered / expired head never grows without bound.
        template <typename T>
        static auto compact(std::vector<T>& v, size_t& pos) -> void
        {
            if (pos >= v.size()) { release(v); pos = 0; }
            else if (pos >= 4096 && pos * 2 >= v.size())
            {
                v.erase(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(pos));
                pos = 0;
            }
        }

        static auto pending() -> size_t
        {
            return (s_hold.size() - s_hold_pos) + s_win_gm.size() + s_win_actor.size() + (s_queue.size() - s_queue_pos) +
                   (s_settle.size() - s_settle_pos);
        }

        static auto enqueue(const Ev& e) -> bool
        {
            compact(s_queue, s_queue_pos);
            return push_safe(s_queue, e);
        }

        static auto queue_actor_event(const Ev& e) -> void // InitGameState / BeginPlay, counted when queued
        {
            if (!enqueue(e)) { return; }
            if (e.kind == Kind::InitGameState) { ++s_gms; } else { ++s_actors; }
        }

        static auto reset_batch() -> void // game thread, lock held (feed / refuse): forget hold + window
        {
            if (s_replay_state == 1 || s_replay_state == 2) // [EARLY-4] nobody listens / route refused: no replay
            {
                s_replay_state = 3;
                release(s_rp_idx); release(s_rp_obj); s_rp_pos = 0;
                try { Output::send<LogLevel::Warning>(STR("[EARLY] stage=startup-replay decision=aborted fed={} (route refused or no world hooks left)\n"), s_rp_fed); } catch (...) {}
            }
            release(s_hold); s_hold_pos = 0;
            release(s_settle); s_settle_pos = 0;
            release(s_win_gm); release(s_win_actor);
            s_window_open = false;
            s_map_level = nullptr;
        }

        static auto close_window(const char* why, int64_t now) -> void // lock held
        {
            if (!s_window_open) { return; }
            s_window_open = false;
            const size_t gms = s_win_gm.size(), actors = s_win_actor.size();
            for (const auto& e : s_win_gm) { queue_actor_event(e); }
            for (const auto& e : s_win_actor) { queue_actor_event(e); }
            (void)enqueue(Ev{s_window_gs.obj, s_window_gs.cls, s_window_gs.index, Kind::LoadMapPost}); // map events are never capped
            release(s_win_gm); release(s_win_actor);
            try
            {
                StringType wide{};
                for (const char* p = why; *p != '\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                Output::send(STR("[WORLD-EV] map load window closed after {} ms (reason={}): gamemodes={} actors={} held_joined={} -> InitGameState, BeginPlay, LoadMap post queued\n"),
                             now - s_window_open_ms, wide, gms, actors, s_window_joined);
            }
            catch (...)
            {
            }
        }

        static auto add_to_window(const Ev& e) -> void // lock held
        {
            (void)push_safe(e.kind == Kind::InitGameState ? s_win_gm : s_win_actor, e);
        }

        // A new GameState: its level is the new map's persistent level.
        static auto on_game_state(const Ev& gs, Unreal::UObject* level, int64_t now) -> void // lock held
        {
            if (s_window_open && level == s_map_level && gs.obj == s_window_gs.obj && gs.index == s_window_gs.index) { return; } // same GameState again: no second map
            close_window("next-map", now);
            while (s_settle_pos < s_settle.size()) { queue_actor_event(s_settle[s_settle_pos++].ev); } // old map's spawns first
            compact(s_settle, s_settle_pos);
            s_map_level = level;
            s_window_open = true;
            s_window_gs = gs;
            s_window_open_ms = s_window_last_ms = now;
            s_window_joined = 0;
            ++s_maps;
            (void)enqueue(Ev{gs.obj, gs.cls, gs.index, Kind::LoadMapPre}); // map events are never capped
            // held events of this level (seen before its GameState) join the window, in arrival order
            size_t w = s_hold_pos;
            for (size_t r = s_hold_pos; r < s_hold.size(); ++r)
            {
                if (s_hold[r].level == level)
                {
                    add_to_window(s_hold[r].ev);
                    ++s_window_joined;
                    if (s_hold[r].loaded || s_hold[r].ev.kind == Kind::InitGameState) { s_window_last_ms = now; }
                }
                else
                {
                    s_hold[w++] = s_hold[r];
                }
            }
            s_hold.resize(w);
            compact(s_hold, s_hold_pos);
            try
            {
                Unreal::UObject* world = level ? level->GetOuterPrivate() : nullptr;
                const bool world_ok = world && static_cast<Unreal::UStruct*>(world->GetClassPrivate()) == s_world;
                Output::send(STR("[WORLD-EV] map change seen (object index): world='{}' held_joined={} -> LoadMap pre queued; this map's InitGameState/BeginPlay/LoadMap post follow when its load window closes (PS5: all after the load)\n"),
                             world_ok ? world->GetName() : StringType{STR("?")}, s_window_joined);
                if (!s_first_map_noted)
                {
                    s_first_map_noted = true;
                    Output::send(STR("[EARLY] stage=first-map-seen t_ms={} src={} world='{}'\n"), r3_now_ms(),
                                 s_in_replay ? STR("replay") : STR("live"), world_ok ? world->GetName() : StringType{STR("?")});
                }
            }
            catch (...)
            {
            }
        }

        static auto emit_counted(const char* reason, const char* effect) -> void;
        static auto refuse(const char* reason) -> void
        {
            if (refused_log::world_route_state.exchange(2, std::memory_order_acq_rel) == 2) { return; }
            try
            {
                StringType wide{};
                for (const char* p = reason; *p != '\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                Output::send<LogLevel::Error>(STR("[WORLD-EV] route refused: reason={} (LoadMap/InitGameState/BeginPlay Lua hooks stay undelivered; everything else unaffected)\n"), wide);
            }
            catch (...)
            {
            }
            emit_counted(reason, "accepted-undelivered");
            reset_batch();
            release(s_queue); s_queue_pos = 0;
        }

        // one counted line per registered hook kind: registrations made while the route was
        // still pending were not counted as gaps at registration time
        static auto emit_counted(const char* reason, const char* effect) -> void
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const struct { const char* kind; size_t n; } kinds[] = {
                    {"RegisterLoadMapPreHook", LuaMod::m_load_map_pre_callbacks.size()},
                    {"RegisterLoadMapPostHook", LuaMod::m_load_map_post_callbacks.size()},
                    {"RegisterInitGameStatePreHook", LuaMod::m_init_game_state_pre_callbacks.size()},
                    {"RegisterInitGameStatePostHook", LuaMod::m_init_game_state_post_callbacks.size()},
                    {"RegisterBeginPlayPreHook", LuaMod::m_begin_play_pre_callbacks.size()},
                    {"RegisterBeginPlayPostHook", LuaMod::m_begin_play_post_callbacks.size()},
            };
            for (const auto& k : kinds)
            {
                if (k.n == 0) { continue; }
                char target[96];
                std::snprintf(target, sizeof(target), "object-index-world-events registrations=%u", static_cast<unsigned>(k.n));
                StringType wide_target{};
                for (const char* p = target; *p != '\0'; ++p) { wide_target.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                refused_log::emit(k.kind, wide_target, StringType{STR("*")}, reason, effect);
            }
        }

        static auto chain_has(Unreal::UStruct* s, Unreal::UStruct* base) -> bool
        {
            int depth = 0;
            for (Unreal::UStruct* c = s; c && depth < 64; c = c->GetSuperStruct(), ++depth)
            {
                if (c == base) { return true; }
            }
            return false;
        }

        static auto is_class_object(Unreal::UStruct* s) -> bool // its own class is the engine's "Class"
        {
            Unreal::UClass* meta = s ? s->GetClassPrivate() : nullptr;
            return meta && meta->GetName() == StringType{STR("Class")};
        }

        // Game thread, from the feed, when the route is wanted. Resolves the four engine classes
        // (once; retried every 2 s) and applies the fail-closed rules (classes: 3 tries / 60 s; index:
        // refused only when switched off, otherwise waited for).
        static auto ensure_ready() -> bool
        {
            const int state = refused_log::world_route_state.load(std::memory_order_acquire);
            if (state == 1) { return true; } // live: nothing to do (called on every pump)
            if (state == 2) { return false; }
            const int64_t now = r3_now_ms();
            if (s_first_wanted_ms == 0) { s_first_wanted_ms = now; }
            if (!s_classes_ok && (s_last_resolve_ms == 0 || now - s_last_resolve_ms >= 2000))
            {
                s_last_resolve_ms = now;
                ++s_resolve_tries;
                try
                {
                    s_actor = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.Actor")});
                    s_game_mode = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.GameModeBase")});
                    s_game_state = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.GameStateBase")});
                    s_world = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(nullptr, nullptr, StringType{STR("/Script/Engine.World")});
                    const bool found = s_actor && s_game_mode && s_game_state && s_world;
                    // the path lookup ignores the class: prove these are the real classes, else not found
                    s_classes_ok = found && is_class_object(s_actor) && is_class_object(s_game_mode) && is_class_object(s_game_state) &&
                                   is_class_object(s_world) && s_game_mode != s_actor && s_game_state != s_actor && s_game_mode != s_game_state &&
                                   chain_has(s_game_mode, s_actor) && chain_has(s_game_state, s_actor) && !chain_has(s_world, s_actor) &&
                                   !chain_has(s_game_mode, s_game_state) && !chain_has(s_game_state, s_game_mode);
                    s_class_check_failed = found && !s_classes_ok;
                }
                catch (...)
                {
                    s_classes_ok = false;
                }
                if (!s_classes_ok) { s_actor = s_game_mode = s_game_state = s_world = nullptr; }
                if (!s_classes_ok && s_resolve_tries >= kMaxResolveTries)
                {
                    // native engine classes exist from boot or never: more full-array lookups cannot help
                    refuse(s_class_check_failed ? "engine-class-check-failed" : "engine-class-lookup-failed");
                    return false;
                }
            }
            if (s_classes_ok && s_index_ok)
            {
                if (refused_log::world_route_state.exchange(1, std::memory_order_acq_rel) == 0)
                {
                    try { Output::send(STR("[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved and class-checked (tries={}, waited_ms={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump\n"), s_resolve_tries, now - s_first_wanted_ms); } catch (...) {}
                    try { Output::send(STR("[EARLY] stage=world-route-live t_ms={} waited_ms={}\n"), now, now - s_first_wanted_ms); } catch (...) {} // [EARLY-4]
                }
                return true;
            }
            static int64_t s_last_disabled_check_ms{0}; // takes the index lock: at most once a second
            if (s_classes_ok && now - s_last_disabled_check_ms >= 1000)
            {
                s_last_disabled_check_ms = now;
                if (Unreal::UObjectGlobals::r9_index_disabled())
                {
                    refuse("object-index-disabled");
                    return false;
                }
            }
            if (now - s_first_wanted_ms >= 60000)
            {
                if (!s_classes_ok)
                {
                    refuse(s_class_check_failed ? "engine-class-check-failed" : "engine-class-lookup-failed");
                }
                else if (!s_pending_logged)
                {
                    s_pending_logged = true;
                    try { Output::send<LogLevel::Warning>(STR("[WORLD-EV] route pending: object index not usable yet (delete listener not yet proven live); LoadMap/InitGameState/BeginPlay Lua hooks deliver once it is\n")); } catch (...) {}
                    // [R12-WORLD] counted like a refusal while pending, so a route that never goes live is never silent
                    emit_counted("object-index-not-usable-yet-60s", "accepted-pending");
                }
            }
            return false;
        }

        // After each collect (outside the lock). Going live here, before the batch is matched,
        // means the very first batch after the index becomes usable is already observed.
        static auto note_index(bool usable) -> void
        {
            if (!usable) { return; }
            s_index_ok = true;
            // [EARLY-4] same rule as the feed: the route never goes live before startup closes
            if (s_wanted && s_startup_closed && refused_log::world_route_state.load(std::memory_order_acquire) == 0) { (void)ensure_ready(); }
        }

        // Caller holds m_thread_actions_mutex; object is live and pointer-checked by the feed.
        static auto observe(Unreal::UObject* object, int32_t object_index) -> void
        {
            if (refused_log::world_route_state.load(std::memory_order_relaxed) != 1) { return; }
            // a NEW object at the current map level's address: that level was replaced
            if (object == s_map_level && !s_window_open) { s_map_level = nullptr; }
            bool is_actor{false}, is_gm{false}, is_gs{false};
            int depth = 0;
            Unreal::UStruct* const cls = object->GetClassPrivate();
            for (Unreal::UStruct* c = cls; c && depth < 64; c = c->GetSuperStruct(), ++depth)
            {
                if (c == s_actor) { is_actor = true; break; } // Actor is the last class of interest up the chain
                if (c == s_game_state) { is_gs = true; }
                else if (c == s_game_mode) { is_gm = true; }
            }
            if (!is_actor) { return; }
            if (object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_ClassDefaultObject | Unreal::RF_ArchetypeObject))) { return; }
            const int64_t now = r3_now_ms();
            Unreal::UObject* const level = object->GetOuterPrivate();
            if (is_gs) { on_game_state(Ev{object, cls, object_index, Kind::LoadMapPre}, level, now); } // map events are never capped
            const bool want_gm = is_gm && (!LuaMod::m_init_game_state_pre_callbacks.empty() || !LuaMod::m_init_game_state_post_callbacks.empty());
            const bool want_bp = !LuaMod::m_begin_play_pre_callbacks.empty() || !LuaMod::m_begin_play_post_callbacks.empty();
            const bool loaded = object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_WasLoaded));
            const bool in_window = s_window_open && level == s_map_level;
            if (is_gm && !in_window)
            {
                // a GameMode with no open load window: a new map load is under way (also catches a
                // same-map reload that reuses the old level address); until its GameState arrives,
                // its actors wait in the hold list
                close_window("next-map", now);
                s_map_level = nullptr;
            }
            if (!want_gm && !want_bp) { return; }
            if (pending() + 2 > kMaxPending) { s_dropped += (want_gm ? 1 : 0) + (want_bp ? 1 : 0); return; }
            const Ev gm_ev{object, cls, object_index, Kind::InitGameState};
            const Ev bp_ev{object, cls, object_index, Kind::BeginPlay};
            if (in_window)
            {
                if (loaded || is_gm) { s_window_last_ms = now; }
                if (want_gm) { add_to_window(gm_ev); }
                if (want_bp) { add_to_window(bp_ev); }
                return;
            }
            if (!is_gm && !loaded && level != nullptr && level == s_map_level)
            {
                if (want_bp) // spawned during play in the current map: settle first (see s_settle)
                {
                    compact(s_settle, s_settle_pos);
                    (void)push_safe(s_settle, Held{bp_ev, level, now, false});
                }
                return;
            }
            if (want_gm) { (void)push_safe(s_hold, Held{gm_ev, level, now, loaded}); }
            if (want_bp) { (void)push_safe(s_hold, Held{bp_ev, level, now, loaded}); }
        }

        // [EARLY-4] every feed call until startup closes (game thread, m_thread_actions_mutex held):
        // a world hook registered before the first feed AFTER the start gate counts as a startup
        // registration (mods' main scripts, and ExecuteInGameThread callbacks, which run before the
        // feed in the same pump: the PS5 equivalent of "before the first map"). A pump that fires
        // while mods are still starting keeps the window open.
        static auto note_feed_locked() -> void
        {
            if (s_startup_closed) { return; }
            if (any_world_callbacks()) { s_startup_wanted = true; }
            if (!s_mods_started.load(std::memory_order_acquire)) { return; }
            s_startup_closed = true;
            try
            {
                Output::send(STR("[EARLY] stage=startup-hooks t_ms={} loadmap_pre={} loadmap_post={} initgamestate={} beginplay={}\n"), r3_now_ms(),
                             LuaMod::m_load_map_pre_callbacks.size(), LuaMod::m_load_map_post_callbacks.size(),
                             LuaMod::m_init_game_state_pre_callbacks.size() + LuaMod::m_init_game_state_post_callbacks.size(),
                             LuaMod::m_begin_play_pre_callbacks.size() + LuaMod::m_begin_play_post_callbacks.size());
            }
            catch (...)
            {
            }
        }

        // Snapshot predicate (runs under the index lock, once per distinct class of a live object).
        static auto replay_want(Unreal::UClass* cls) -> bool
        {
            return cls != nullptr && s_actor != nullptr && chain_has(static_cast<Unreal::UStruct*>(cls), s_actor);
        }

        // Decided once: the route is live and startup has closed. Startup registrations -> replay requested.
        static auto maybe_request_replay() -> void
        {
            if (s_replay_state != 0 || !s_startup_closed || refused_log::world_route_state.load(std::memory_order_acquire) != 1) { return; }
            s_replay_state = s_startup_wanted ? 1 : 3;
            try { Output::send(STR("[EARLY] stage=replay-request t_ms={} startup_hooks={} replay={}\n"), r3_now_ms(), s_startup_wanted ? 1 : 0,
                               s_replay_state == 1 ? STR("requested") : STR("not-startup")); } catch (...) {}
        }

        // Game thread, NOT holding m_thread_actions_mutex (takes the index lock). pending_idx[from..]
        // are new objects collected but not matched yet: they are observed live, never replayed.
        static auto maybe_start_replay(const std::vector<int32_t>& pending_idx, size_t from) -> void
        {
            maybe_request_replay();
            if (s_replay_state != 1) { return; }
            s_replay_state = 2;
            s_rp_pos = 0;
            s_rp_fed = 0;
            s_rp_start_ms = r3_now_ms();
            size_t baseline{0}, excluded{0}, gamestates{0};
            bool ok{false};
            try
            {
                ok = Unreal::UObjectGlobals::r9_index_snapshot_live(s_rp_idx, s_rp_obj, &replay_want);
                if (ok)
                {
                    baseline = s_rp_idx.size();
                    std::vector<int32_t> unmatched{};
                    if (from < pending_idx.size()) { unmatched.assign(pending_idx.begin() + static_cast<std::ptrdiff_t>(from), pending_idx.end()); }
                    std::sort(unmatched.begin(), unmatched.end());
                    size_t w = 0;
                    for (size_t r = 0; r < s_rp_idx.size(); ++r)
                    {
                        if (std::binary_search(unmatched.begin(), unmatched.end(), s_rp_idx[r])) { ++excluded; continue; }
                        s_rp_idx[w] = s_rp_idx[r];
                        s_rp_obj[w] = s_rp_obj[r];
                        ++w;
                    }
                    s_rp_idx.resize(w);
                    s_rp_obj.resize(w);
                    for (void* p : s_rp_obj)
                    {
                        auto* o = static_cast<Unreal::UObject*>(p);
                        if (o->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_ClassDefaultObject | Unreal::RF_ArchetypeObject))) { continue; }
                        if (chain_has(static_cast<Unreal::UStruct*>(o->GetClassPrivate()), s_game_state)) { ++gamestates; }
                    }
                }
            }
            catch (...)
            {
                ok = false;
            }
            const char* decision = !ok ? "skip-index-unusable" : (gamestates > 1 ? "skip-ambiguous" : (s_rp_idx.empty() ? "none" : "replay"));
            if (!ok || gamestates > 1 || s_rp_idx.empty())
            {
                release(s_rp_idx); release(s_rp_obj);
                s_replay_state = 3;
            }
            try
            {
                StringType wide{};
                for (const char* p = decision; *p != '\0'; ++p) { wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p))); }
                const int64_t t = r3_now_ms();
                if (gamestates > 1 || !ok)
                {
                    Output::send<LogLevel::Warning>(STR("[EARLY] stage=startup-replay t_ms={} decision={} candidates={} baseline={} excluded_new={} gamestates={} snapshot_ms={}\n"),
                                                    t, wide, s_rp_idx.size(), baseline, excluded, gamestates, t - s_rp_start_ms);
                }
                else
                {
                    Output::send(STR("[EARLY] stage=startup-replay t_ms={} decision={} candidates={} baseline={} excluded_new={} gamestates={} snapshot_ms={}\n"),
                                 t, wide, s_rp_idx.size(), baseline, excluded, gamestates, t - s_rp_start_ms);
                }
            }
            catch (...)
            {
            }
        }

        // Game thread, m_thread_actions_mutex held. Feeds replayed actors through observe() under the
        // matcher's budget. true = nothing (left) to replay: newer objects may be matched now.
        static auto replay_step(std::chrono::steady_clock::time_point deadline) -> bool
        {
            if (s_replay_state != 2) { return true; }
            s_in_replay = true;
            while (s_rp_pos < s_rp_idx.size())
            {
                if ((s_rp_pos & 63) == 0 && s_rp_pos > 0 && std::chrono::steady_clock::now() > deadline) { s_in_replay = false; return false; }
                const int32_t idx = s_rp_idx[s_rp_pos];
                void* const expected = s_rp_obj[s_rp_pos];
                ++s_rp_pos;
                Unreal::FUObjectItem* item = Unreal::FUObjectArray::IndexToObject(idx);
                if (!item || item->IsUnreachable()) { continue; }
                Unreal::UObject* object = item->GetUObject();
                if (!object || static_cast<void*>(object) != expected) { continue; } // freed / slot reused since the snapshot
                observe(object, idx);
                ++s_rp_fed;
            }
            s_in_replay = false;
            s_replay_state = 3;
            release(s_rp_idx); release(s_rp_obj); s_rp_pos = 0;
            try
            {
                const int64_t t = r3_now_ms();
                Output::send(STR("[EARLY] stage=startup-replay-done t_ms={} fed={} took_ms={} window_open={} held={}\n"), t, s_rp_fed, t - s_rp_start_ms,
                             s_window_open ? 1 : 0, s_hold.size() - s_hold_pos);
            }
            catch (...)
            {
            }
            return true;
        }

        static auto replay_running() -> bool { return s_replay_state == 2; }

        // Lock held, every pump while the route is live: close a quiet window, expire the hold list.
        static auto tick() -> void
        {
            if (refused_log::world_route_state.load(std::memory_order_relaxed) != 1) { return; }
            if (!s_window_open && s_hold_pos >= s_hold.size() && s_settle_pos >= s_settle.size()) { return; }
            const int64_t now = r3_now_ms();
            while (s_settle_pos < s_settle.size() && now - s_settle[s_settle_pos].t_ms >= kSettleMs)
            {
                queue_actor_event(s_settle[s_settle_pos++].ev); // alive + class re-checked at delivery
            }
            compact(s_settle, s_settle_pos);
            if (s_window_open && now - s_window_last_ms >= kQuietMs) { close_window("quiet", now); }
            else if (s_window_open && now - s_window_open_ms >= kMaxWindowMs) { close_window("max-30s", now); }
            while (s_hold_pos < s_hold.size() && now - s_hold[s_hold_pos].t_ms >= kHoldMs)
            {
                const Held& h = s_hold[s_hold_pos++];
                if (h.loaded && h.ev.kind == Kind::BeginPlay)
                {
                    // package-loaded and no GameState claimed it: the current map's level -> late, else streamed
                    if (h.level != nullptr && h.level == s_map_level) { ++s_late_after_post; queue_actor_event(h.ev); }
                    else { ++s_streamed_unreported; }
                }
                else
                {
                    queue_actor_event(h.ev); // GameMode / spawned actor without a map: delivered after the wait
                }
            }
            compact(s_hold, s_hold_pos);
        }

        static auto kind_name(Kind k) -> StringType
        {
            switch (k)
            {
            case Kind::LoadMapPre: return StringType{STR("LoadMapPre")};
            case Kind::InitGameState: return StringType{STR("InitGameState")};
            case Kind::BeginPlay: return StringType{STR("BeginPlay")};
            case Kind::LoadMapPost: return StringType{STR("LoadMapPost")};
            }
            return StringType{STR("?")};
        }

        static auto short_name(Unreal::UObject* o) -> StringType
        {
            StringType n{};
            if (!o) { return n; }
            try { n = o->GetName(); } catch (...) { return StringType{}; }
            if (n.size() > 64) { n.resize(64); }
            for (auto& c : n) { if (c < 32 || c > 126) { c = '?'; } }
            return n;
        }

        // One contained Lua call (plain try/catch; the Lua stack is always restored). push_args
        // pushes the arguments after the function.
        template <typename PushArgs>
        static auto call_one(const LuaMadeSimple::Lua& lua, int32_t ref, int nargs, Kind k, PushArgs&& push_args) -> void
        {
            lua_State* L = lua.get_lua_state();
            const int top = lua_gettop(L);
            try
            {
                get_function_ref_cxx(lua, ref);
                push_args();
                lua.call_function(nargs, 0);
                lua_settop(L, top);
                ++s_delivered;
            }
            catch (const std::exception& e)
            {
                lua_settop(L, top);
                if (++s_errors <= 20)
                {
                    try { Output::send<LogLevel::Error>(STR("[WORLD-EV] callback error (#{}, kind={}, mod={}): {}\n"), s_errors, kind_name(k), refused_log::mod_name_of(lua), ensure_str(e.what())); } catch (...) {}
                }
            }
            catch (...)
            {
                lua_settop(L, top);
                if (++s_errors <= 20)
                {
                    try { Output::send<LogLevel::Error>(STR("[WORLD-EV] callback threw a non-standard exception (#{}, kind={})\n"), s_errors, kind_name(k)); } catch (...) {}
                }
            }
        }

        static auto snapshot(const std::vector<LuaMod::LuaCallbackData>& v, std::vector<std::pair<const LuaMadeSimple::Lua*, int32_t>>& out) -> void
        {
            for (const auto& cb : v)
            {
                for (const auto& [lua_ptr, registry_index] : cb.registry_indexes)
                {
                    if (lua_ptr) { out.emplace_back(lua_ptr, registry_index.lua_index); }
                }
            }
        }

        static auto report_stats() -> void
        {
            static auto s_last_report = std::chrono::steady_clock::now();
            static uint64_t s_last_sum{0};
            const auto now = std::chrono::steady_clock::now();
            if (now - s_last_report < std::chrono::seconds(10)) { return; }
            s_last_report = now;
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const uint64_t sum = s_maps + s_gms + s_actors + s_delivered + s_skipped_dead + s_dropped + s_streamed_unreported + s_late_after_post + s_errors;
            if (sum == s_last_sum) { return; }
            s_last_sum = sum;
            try
            {
                Output::send(STR("[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} late_after_post={} held={} settling={} window_open={} dropped={} errors={} queued={} paced_skips={}\n"),
                             s_maps, s_gms, s_actors, s_delivered, s_skipped_dead, s_streamed_unreported, s_late_after_post,
                             s_hold.size() - s_hold_pos, s_settle.size() - s_settle_pos, s_window_open ? 1 : 0, s_dropped, s_errors,
                             s_queue.size() - s_queue_pos, s_pace.skipped);
            }
            catch (...)
            {
            }
        }

        // Game thread, every pump (inside process_pending_notify_on_new_object_callbacks).
        static auto deliver() -> void
        {
            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                tick();
                if (s_queue_pos >= s_queue.size()) { return; }
            }
            std::chrono::steady_clock::time_point deadline{};
            if (!s_pace.begin(std::chrono::microseconds(1000), deadline)) { return; }
            const R9PaceEnd pace_end{s_pace};
            // Same lock discipline as the [R9-NOTIFY] executor: held across the Lua calls so a
            // mod uninstall (erases callbacks under this lock) can never free a state mid-call.
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            static std::vector<std::pair<const LuaMadeSimple::Lua*, int32_t>> s_pre{}, s_post{};
            static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
            size_t n = 0;
            while (s_queue_pos < s_queue.size())
            {
                if (n > 0 && std::chrono::steady_clock::now() > deadline) { break; }
                ++n;
                const Ev ev = s_queue[s_queue_pos++];
                Unreal::FUObjectItem* item = Unreal::FUObjectArray::IndexToObject(ev.index);
                // same slot, same address, still reachable, not being destroyed, and still the class
                // it had when observed (a reused slot/address with another class is never handed on)
                const bool alive = item && item->GetUObject() == ev.obj && !item->IsUnreachable() &&
                                   !ev.obj->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_BeginDestroyed | Unreal::RF_FinishDestroyed)) &&
                                   static_cast<Unreal::UStruct*>(ev.obj->GetClassPrivate()) == ev.cls;
                const bool is_map = ev.kind == Kind::LoadMapPre || ev.kind == Kind::LoadMapPost;
                // A dead actor / game mode is never handed to Lua. A map event still fires (the
                // load did happen); only its World argument becomes invalid.
                if (!alive && !is_map) { ++s_skipped_dead; continue; }
                // A LoadMap detour that really armed delivers itself: never twice.
                if (is_map && refused_log::loadmap_state.load(std::memory_order_acquire) == 2) { continue; }
                s_pre.clear();
                s_post.clear();
                switch (ev.kind)
                {
                case Kind::LoadMapPre: snapshot(LuaMod::m_load_map_pre_callbacks, s_pre); break;
                case Kind::LoadMapPost: snapshot(LuaMod::m_load_map_post_callbacks, s_pre); break;
                case Kind::InitGameState:
                    snapshot(LuaMod::m_init_game_state_pre_callbacks, s_pre);
                    snapshot(LuaMod::m_init_game_state_post_callbacks, s_post);
                    break;
                case Kind::BeginPlay:
                    snapshot(LuaMod::m_begin_play_pre_callbacks, s_pre);
                    snapshot(LuaMod::m_begin_play_post_callbacks, s_post);
                    break;
                }
                const auto ki = static_cast<size_t>(ev.kind);
                if (s_first_kind[ki] && (!s_pre.empty() || !s_post.empty()))
                {
                    s_first_kind[ki] = false;
                    try { Output::send(STR("[WORLD-EV] first delivery kind={} callbacks={} object='{}'\n"), kind_name(ev.kind), s_pre.size() + s_post.size(), short_name(alive ? ev.obj : nullptr)); } catch (...) {}
                    try { Output::send(STR("[EARLY] stage=first-delivery kind={} t_ms={}\n"), kind_name(ev.kind), r3_now_ms()); } catch (...) {} // [EARLY-4]
                }
                if (is_map)
                {
                    Unreal::UObject* engine{nullptr};
                    Unreal::UObject* pending_game{nullptr};
                    Unreal::UObject* world{nullptr};
                    if (ev.kind == Kind::LoadMapPost && alive)
                    {
                        // GameState -> its Level -> that Level's World (class-checked; else invalid)
                        Unreal::UObject* level = ev.obj->GetOuterPrivate();
                        Unreal::UObject* w = level ? level->GetOuterPrivate() : nullptr;
                        if (w && static_cast<Unreal::UStruct*>(w->GetClassPrivate()) == s_world) { world = w; }
                    }
                    for (const auto& [lua_ptr, ref] : s_pre)
                    {
                        const auto& lua = *lua_ptr;
                        call_one(lua, ref, 5, ev.kind, [&]() {
                            LuaType::RemoteUnrealParam::construct(lua, &engine, s_object_property_name);
                            LuaType::RemoteUnrealParam::construct(lua, &world, s_object_property_name);
                            lua.set_nil(); // URL: not available on this route
                            LuaType::RemoteUnrealParam::construct(lua, &pending_game, s_object_property_name);
                            lua.set_string(std::string_view{});
                        });
                    }
                    if (ev.kind == Kind::LoadMapPost)
                    {
                        try { Output::send(STR("[WORLD-EV] LoadMap post delivered: world='{}' callbacks={}\n"), short_name(world), s_pre.size()); } catch (...) {}
                    }
                    continue;
                }
                Unreal::UObject* context = ev.obj;
                for (const auto& [lua_ptr, ref] : s_pre)
                {
                    const auto& lua = *lua_ptr;
                    call_one(lua, ref, 1, ev.kind, [&]() { LuaType::RemoteUnrealParam::construct(lua, &context, s_object_property_name); });
                }
                for (const auto& [lua_ptr, ref] : s_post)
                {
                    const auto& lua = *lua_ptr;
                    call_one(lua, ref, 1, ev.kind, [&]() { LuaType::RemoteUnrealParam::construct(lua, &context, s_object_property_name); });
                }
            }
            compact(s_queue, s_queue_pos);
        }
    } // namespace world_ev

    // [EARLY-4] called once by UE4SSProgram right after the "[L25] start gate" line (every mod's main
    // script has returned): the startup-registration window closes at the next feed.
    auto early4_mods_started() noexcept -> void
    {
        world_ev::s_mods_started.store(true, std::memory_order_release);
    }

    static auto r9_feed_notify_from_index() -> void
    {
        // backlog of new objects, matched under a per-frame budget (~1 ms) so a level load that creates
        // ~100k objects is spread over frames instead of stacked into one (reviewer, launch 7)
        static std::vector<int32_t> s_backlog_idx{};
        static std::vector<void*> s_backlog_obj{};
        static size_t s_backlog_pos{0};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            world_ev::s_wanted = world_ev::wanted_locked(); // [WORLD-EV] map/InitGameState/BeginPlay hooks also read the index
            world_ev::note_feed_locked(); // [EARLY-4] startup registrations = those present at the first feed after the start gate
            if (LuaMod::m_static_construct_object_lua_callbacks.empty() && !world_ev::s_wanted)
            {
                Unreal::UObjectGlobals::r9_index_notify_disarm(); // cheap: stop collecting while nobody listens
                s_backlog_idx.clear();
                s_backlog_obj.clear();
                s_backlog_pos = 0;
                world_ev::reset_batch();
                return;
            }
        }
        // [WORLD-EV] outside the lock (class lookups). [EARLY-4] never before startup closes (first feed after
        // the start gate): objects matched while mods are still starting are not delivered live, the startup
        // replay delivers them once instead (a live delivery + the replay would deliver that map twice).
        if (world_ev::s_wanted && world_ev::s_startup_closed) { (void)world_ev::ensure_ready(); }
        world_ev::maybe_start_replay(s_backlog_idx, s_backlog_pos); // [EARLY-4] went live above: unmatched backlog stays live-only
        if (s_backlog_pos >= s_backlog_idx.size())
        {
            s_backlog_idx.clear();
            s_backlog_obj.clear();
            s_backlog_pos = 0;
            Unreal::UObjectGlobals::r9_index_notify_arm(); // no-op when armed; re-arms after a racing disarm
            // [R9-IDX] ~0.5 ms per ~16 ms window for re-checking freed slots ([R9-PACE]): a huge unload can
            // never cost more than that per frame; FindFirstOf/FindAllOf still sync exactly
            static std::chrono::steady_clock::time_point s_watch_budget_end{};
            if (!s_r9_pace_watch.begin(std::chrono::microseconds(500), s_watch_budget_end)) { return; }
            bool r9_collected{false};
            {
                const R9PaceEnd r9_pace_end{s_r9_pace_watch};
                r9_collected = Unreal::UObjectGlobals::r9_index_collect_new(s_backlog_idx, s_backlog_obj,
                                                                            +[]() -> bool { return std::chrono::steady_clock::now() > s_watch_budget_end; });
            }
            world_ev::note_index(r9_collected); // [WORLD-EV]
            if (r9_collected) { world_ev::maybe_start_replay(s_backlog_idx, 0); } // [EARLY-4] went live in note_index: this batch is live-only
            if (!r9_collected)
            {
                static bool s_unusable_logged{false};
                if (!s_unusable_logged)
                {
                    s_unusable_logged = true;
                    try { Output::send<LogLevel::Warning>(STR("[R9-NOTIFY] NotifyOnNewObject registered but the object index is not usable yet (delete listener not yet proven live, or index disabled); notifications start once it is\n")); } catch (...) {}
                }
                return;
            }
            if (s_backlog_idx.empty() && !world_ev::replay_running()) { return; } // [EARLY-4] a replay may run with no new objects
            static bool s_logged{false};
            if (!s_backlog_idx.empty() && !s_logged)
            {
                s_logged = true;
                try { Output::send(STR("[R9-NOTIFY] NotifyOnNewObject delivered from the object index (first batch: {} new objects)\n"), s_backlog_idx.size()); } catch (...) {}
            }
        }
        if (s_backlog_pos >= s_backlog_idx.size() && !world_ev::replay_running()) { return; }
        std::chrono::steady_clock::time_point budget_end{}; // ~1 ms per ~16 ms window ([R9-PACE])
        if (!s_r9_pace_match.begin(std::chrono::microseconds(1000), budget_end)) { return; }
        const R9PaceEnd r9_pace_end{s_r9_pace_match};
        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
        // [EARLY-4] the startup replay goes first (older map before newer objects); newer objects wait
        if (!world_ev::replay_step(budget_end)) { return; }
        while (s_backlog_pos < s_backlog_idx.size())
        {
            if ((s_backlog_pos & 63) == 0 && s_backlog_pos > 0 && std::chrono::steady_clock::now() > budget_end) { break; }
            const int32_t object_index = s_backlog_idx[s_backlog_pos];
            void* const expected = s_backlog_obj[s_backlog_pos];
            ++s_backlog_pos;
            Unreal::FUObjectItem* object_item = Unreal::FUObjectArray::IndexToObject(object_index);
            if (!object_item || object_item->IsUnreachable()) { continue; }
            Unreal::UObject* object = object_item->GetUObject();
            if (!object || static_cast<void*>(object) != expected) { continue; } // slot changed hands: the newer object has its own entry
            world_ev::observe(object, object_index); // [WORLD-EV]
            for (Unreal::UStruct* object_class = object->GetClassPrivate(); object_class; object_class = object_class->GetSuperStruct())
            {
                for (const auto& callback_data : LuaMod::m_static_construct_object_lua_callbacks)
                {
                    if (object_class->GetNamePrivate().Equals(callback_data.instance_class_name) &&
                        object_class->GetOuterPrivate() && object_class->GetOuterPrivate()->GetNamePrivate().Equals(callback_data.instance_class_outer_name))
                    {
                        // serial 0: FUObjectItem::SerialNumber is not in this game's layout map (launch 8: the read
                        // threw for every object); the executor's exact-pointer re-check covers reused slots.
                        LuaMod::m_pending_notify_on_new_object_callbacks.push_back(
                                LuaMod::PendingNotifyOnNewObjectCallback{callback_data.callback_id, object_index, 0});
                    }
                }
            }
        }
    }

    static auto process_pending_notify_on_new_object_callbacks() -> void
    {
        r9_feed_notify_from_index();
        world_ev::deliver();      // [WORLD-EV] paced, contained, game thread
        world_ev::report_stats(); // [WORLD-EV] one stats line per 10 s, only when something changed
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (LuaMod::m_pending_notify_on_new_object_callbacks.empty())
            {
                return;
            }
        }
        // ~2 ms per ~16 ms window for delivering notifications ([R9-PACE])
        std::chrono::steady_clock::time_point r9_exec_deadline{};
        if (!s_r9_pace_exec.begin(std::chrono::microseconds(2000), r9_exec_deadline)) { return; }
        const R9PaceEnd r9_pace_end{s_r9_pace_exec};
        struct PendingNotifyExec
        {
            uint64_t callback_id{};
            const LuaMadeSimple::Lua* lua{};
            int32_t lua_callback_function_ref{};
            int32_t lua_callback_thread_ref{};
            int32_t object_index{};
            Unreal::UObject* constructed_object{};
        };

        std::vector<PendingNotifyExec> to_execute{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (LuaMod::m_pending_notify_on_new_object_callbacks.empty())
            {
                return;
            }

            // [R9-NOTIFY] take at most 256 entries per pump (the per-frame setup stays bounded during a load)
            std::vector<LuaMod::PendingNotifyOnNewObjectCallback> pending_callbacks{};
            {
                auto& q = LuaMod::m_pending_notify_on_new_object_callbacks;
                const size_t take = q.size() < 256 ? q.size() : 256;
                pending_callbacks.assign(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(take));
                q.erase(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(take));
            }

            for (auto& pending : pending_callbacks)
            {
                Unreal::FUObjectItem* object_item = Unreal::FUObjectArray::IndexToObject(pending.object_index);
                if (!object_item)
                {
                    continue;
                }
                if (pending.object_serial != 0 && object_item->GetSerialNumber() != pending.object_serial)
                {
                    continue;
                }

                Unreal::UObject* constructed_object = object_item->GetUObject();
                if (!constructed_object)
                {
                    continue;
                }

                auto callback_it = std::find_if(
                    LuaMod::m_static_construct_object_lua_callbacks.begin(),
                    LuaMod::m_static_construct_object_lua_callbacks.end(),
                    [&](const LuaMod::LuaCancellableCallbackData& callback) { return callback.callback_id == pending.callback_id; }
                );
                if (callback_it == LuaMod::m_static_construct_object_lua_callbacks.end())
                {
                    continue;
                }

                auto& callback_data = *callback_it;
                to_execute.push_back(PendingNotifyExec{pending.callback_id,
                                                       callback_data.lua,
                                                       callback_data.lua_callback_function_ref,
                                                       callback_data.lua_callback_thread_ref,
                                                       pending.object_index,
                                                       constructed_object});
            }

            std::unordered_set<uint64_t> callbacks_to_remove{};
            std::vector<std::pair<lua_State*, int32_t>> thread_refs_to_unref{};
            // [R9-NOTIFY] per-frame budget (~2 ms, in order): a level load can queue thousands of
            // notifications; on PC they are spread across the load, so never stack them into one frame.
            const auto r9_budget_end = r9_exec_deadline;
            static uint64_t s_notify_delivered{0}, s_notify_skipped_dying{0}, s_notify_deferred{0};
            static uint32_t s_notify_errors{0};
            for (size_t r9_k = 0; r9_k < to_execute.size(); ++r9_k)
            {
                auto& exec = to_execute[r9_k];
                if (r9_k > 0 && (r9_k & 15) == 0 && std::chrono::steady_clock::now() > r9_budget_end)
                {
                    // re-queue the rest at the front, in order (serial 0: the pointer re-check still applies)
                    std::vector<LuaMod::PendingNotifyOnNewObjectCallback> rest{};
                    for (size_t j = r9_k; j < to_execute.size(); ++j)
                    {
                        rest.push_back(LuaMod::PendingNotifyOnNewObjectCallback{to_execute[j].callback_id, to_execute[j].object_index, 0});
                    }
                    s_notify_deferred += rest.size();
                    LuaMod::m_pending_notify_on_new_object_callbacks.insert(LuaMod::m_pending_notify_on_new_object_callbacks.begin(),
                                                                            rest.begin(), rest.end());
                    break;
                }
                if (callbacks_to_remove.contains(exec.callback_id))
                {
                    continue;
                }
                // Re-validate the object is still alive before calling into Lua.
                // Use the cached index to avoid dereferencing a potentially stale pointer.
                Unreal::FUObjectItem* recheck_item = Unreal::FUObjectArray::IndexToObject(exec.object_index);
                if (!recheck_item || recheck_item->GetUObject() != exec.constructed_object)
                {
                    continue;
                }
                // [R9-NOTIFY] dying-object check (reviewer): never hand a dying object to a mod
                if (recheck_item->IsUnreachable() ||
                    exec.constructed_object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_BeginDestroyed | Unreal::RF_FinishDestroyed)))
                {
                    ++s_notify_skipped_dying;
                    continue;
                }
                ++s_notify_delivered;
                const int r9_top = lua_gettop(exec.lua->get_lua_state());

                // [R9-NOTIFY] contain each callback: a Lua error in one mod's NotifyOnNewObject callback
                // (TRY re-raises it as a C++ exception here, outside any Lua frame) must not drop the
                // remaining callbacks or the rest of this pump (launch 7: 58 contained at the hook guard).
                try
                {
                TRY([&]() {
                    get_function_ref_cxx(*exec.lua, exec.lua_callback_function_ref);
                    LuaType::auto_construct_object(*exec.lua, exec.constructed_object);
                    exec.lua->call_function(1, 1);

                    if (exec.lua->is_bool(-1))
                    {
                        // get_bool pops the value from the stack, so we shouldn't call lua_pop explicitly.
                        bool cancel = exec.lua->get_bool(-1);
                        if (cancel)
                        {
                            thread_refs_to_unref.emplace_back(exec.lua->get_lua_state(), exec.lua_callback_thread_ref);
                            callbacks_to_remove.insert(exec.callback_id);
                        }
                    }
                    else
                    {
                        // No bool means we didn't call get_bool, which means the value wasn't popped from the stack, therefore we must explicitly call lua_pop.
                        lua_pop(exec.lua->get_lua_state(), 1);
                    }
                });
                }
                catch (const std::exception& e)
                {
                    lua_settop(exec.lua->get_lua_state(), r9_top);
                    if (++s_notify_errors <= 20)
                    {
                        const bool r9_need_init = exec.constructed_object->HasAnyFlags(static_cast<Unreal::EObjectFlags>(Unreal::RF_NeedInitialization | Unreal::RF_NeedLoad | Unreal::RF_NeedPostLoad));
                        try
                        {
                            Output::send<LogLevel::Error>(STR("[R9-NOTIFY] NotifyOnNewObject callback error (#{}, object needs-init={}): {}\n"),
                                                          s_notify_errors, r9_need_init ? 1 : 0, ensure_str(e.what()));
                        }
                        catch (...)
                        {
                        }
                    }
                }
                catch (...)
                {
                    lua_settop(exec.lua->get_lua_state(), r9_top);
                    if (++s_notify_errors <= 20)
                    {
                        try { Output::send<LogLevel::Error>(STR("[R9-NOTIFY] NotifyOnNewObject callback threw a non-standard exception\n")); } catch (...) {}
                    }
                }
            }
            {
                static auto s_last_report = std::chrono::steady_clock::now();
                const auto r9_now = std::chrono::steady_clock::now();
                if (r9_now - s_last_report >= std::chrono::seconds(10))
                {
                    s_last_report = r9_now;
                    try
                    {
                        Output::send(STR("[R9-NOTIFY] stats delivered={} skipped_dying={} deferred_by_budget={} errors={} pending={} paced_skips watch/match/exec={}/{}/{}\n"),
                                     s_notify_delivered, s_notify_skipped_dying, s_notify_deferred, s_notify_errors,
                                     LuaMod::m_pending_notify_on_new_object_callbacks.size(),
                                     s_r9_pace_watch.skipped, s_r9_pace_match.skipped, s_r9_pace_exec.skipped);
                    }
                    catch (...)
                    {
                    }
                }
            }

            if (!thread_refs_to_unref.empty())
            {
                for (auto& [lua_state, ref] : thread_refs_to_unref)
                {
                    luaL_unref(lua_state, LUA_REGISTRYINDEX, ref);
                }
            }

            if (!callbacks_to_remove.empty())
            {
                std::erase_if(LuaMod::m_static_construct_object_lua_callbacks, [&](const LuaMod::LuaCancellableCallbackData& callback) {
                    return callbacks_to_remove.contains(callback.callback_id);
                });
            }
        }
    }

    template <GameThreadExecutionMethod Executor>
    static auto process_delayed_actions(std::vector<LuaMod::DelayedGameThreadAction>& actions) -> void
    {
        struct ReadyDelayedExec
        {
            size_t index{};
            const LuaMadeSimple::Lua* lua{};
            int32_t lua_action_function_ref{};
        };

        std::vector<ReadyDelayedExec> ready{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const auto now = std::chrono::steady_clock::now();
            for (size_t i = 0; i < actions.size(); ++i)
            {
                auto& action = actions[i];

                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    continue;
                }

                if constexpr (Executor == GameThreadExecutionMethod::ProcessEvent)
                {
                    if (action.method != GameThreadExecutionMethod::ProcessEvent)
                    {
                        continue;
                    }
                }
                else if constexpr (Executor == GameThreadExecutionMethod::EngineTick)
                {
                    if (action.method != GameThreadExecutionMethod::EngineTick)
                    {
                        continue;
                    }
                }
                else if constexpr (Executor == kFuncPtrMarshalExecutor)
                {
                    // [R3] the marshal drains only marshal-tagged delayed actions
                    // (time-based; frame-based delays remain EngineTick-only, so the
                    // readiness block below leaves them untouched for this executor).
                    if (action.method != kFuncPtrMarshalExecutor)
                    {
                        continue;
                    }
                }

                if (action.status == LuaMod::DelayedActionStatus::Paused || action.status == LuaMod::DelayedActionStatus::Executing)
                {
                    continue;
                }

                bool ready_to_execute = false;
                if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                {
                    action.frames_remaining--;
                    ready_to_execute = action.frames_remaining <= 0;
                }
                else if (action.method != GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                {
                    Output::send<LogLevel::Warning>(STR("ProcessEvent hook received frame-based delayed action - this should not happen\n"));
                    continue;
                }
                else
                {
                    ready_to_execute = now >= action.execute_at;
                }

                if (!ready_to_execute || LuaMod::m_is_currently_executing_game_action)
                {
                    continue;
                }

                action.status = LuaMod::DelayedActionStatus::Executing;
                ready.push_back(ReadyDelayedExec{i, action.lua, action.lua_action_function_ref});
            }
        }

        for (auto& exec : ready)
        {
            if (LuaMod::m_is_currently_executing_game_action)
            {
                continue;
            }

            const int64_t run_t0 = r9perf::now_ns();
            try
            {
                const GameActionExecutingGuard executing_guard{};
                TRY([&]() {
                    get_function_ref_cxx(*exec.lua, exec.lua_action_function_ref);
                    exec.lua->call_function(0, 0);
                });
                r9perf::add_run(exec.lua->get_lua_state(), r9perf::now_ns() - run_t0);
            }
            catch (...)
            {
                // [R9-EIGT] an exception that escaped TRY (e.g. bad_alloc in its error log)
                // must not stop the remaining actions or skip the status reset below.
                try { Output::send<LogLevel::Error>(STR("[R9-EIGT] delayed game-thread action threw; continuing\n")); } catch (...) {}
            }
        }

        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const auto now = std::chrono::steady_clock::now();
            for (auto& exec : ready)
            {
                if (exec.index >= actions.size())
                {
                    continue;
                }
                auto& action = actions[exec.index];

                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    continue;
                }

                if (action.pause_after_execution)
                {
                    action.pause_after_execution = false;
                    if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                    {
                        action.frames_remaining = action.delay_frames;
                    }
                    else
                    {
                        action.time_remaining_ms = action.delay_ms;
                        action.execute_at = now + std::chrono::milliseconds(action.delay_ms);
                    }
                    action.status = LuaMod::DelayedActionStatus::Paused;
                    continue;
                }

                if (action.is_looping)
                {
                    if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                    {
                        action.frames_remaining = action.delay_frames;
                    }
                    else
                    {
                        action.execute_at = now + std::chrono::milliseconds(action.delay_ms);
                    }
                    action.status = LuaMod::DelayedActionStatus::Active;
                }
                else
                {
                    action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                }
            }

            std::erase_if(actions, [&](LuaMod::DelayedGameThreadAction& action) -> bool {
                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    luaL_unref(action.lua->get_lua_state(), LUA_REGISTRYINDEX, action.lua_action_function_ref);
                    return true;
                }
                return false;
            });
        }
    }

    auto static process_event_hook([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData,
                                   [[maybe_unused]] Unreal::UObject* Context,
                                   [[maybe_unused]] Unreal::UFunction* Function,
                                   [[maybe_unused]] void* Parms) -> void
    {
        // [R9-GUARD] the engine calls this directly: nothing may escape into engine code
        // (escaping = std::terminate / abort; launch 3 crash).
        try
        {
    #ifdef __PROSPERO__
            // ProcessEvent may run on worker threads. Do not inspect Lua action
            // state until the real Tick has published this exact caller thread.
            if (!Unreal::IsInGameThreadRaw())
            {
                return;
            }
    #endif
            const r9perf::TimedScope pe_perf{r9perf::pe_calls, r9perf::pe_ns};
            ++r9perf::pe_locks; // the dispatch scope below takes the shared lock on every call
    #ifdef __PROSPERO__
            NativeLuaActionDispatchScope action_dispatch;
            if (!action_dispatch.owns_dispatch())
                return;
    #endif
            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = true;
            }
            r9perf::pe_locks += 2; // the flag set above and the merge below

            process_simple_actions(LuaMod::m_game_thread_actions);
            process_delayed_actions<GameThreadExecutionMethod::ProcessEvent>(LuaMod::m_delayed_game_thread_actions);

            process_pending_notify_on_new_object_callbacks();

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = false;
                if (!LuaMod::m_pending_game_thread_actions.empty())
                {
                    LuaMod::m_game_thread_actions.insert(
                        LuaMod::m_game_thread_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_game_thread_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_game_thread_actions.end()));
                    LuaMod::m_pending_game_thread_actions.clear();
                }
                if (!LuaMod::m_pending_delayed_game_thread_actions.empty())
                {
                    LuaMod::m_delayed_game_thread_actions.insert(
                        LuaMod::m_delayed_game_thread_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.end()));
                    LuaMod::m_pending_delayed_game_thread_actions.clear();
                }
            }
        }
        catch (const std::exception& e)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception: {}\n"), ensure_str(e.what())); } catch (...) {}
        }
        catch (...)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception\n")); } catch (...) {}
        }
    }

    // =====================================================================================
    // [R3][worker-gate5-loadmap r3] Deferred LoadMap arm (distinct ID: l11-loadmap-defer-r3).
    // Closes the residual RUNBOOK §5.2 loss left open by l11-loadmap-defer-r2: in the R3
    // marshal world the r2 gate skips LuaMod's eager LoadMap arms and declares the Lua
    // RegisterLoadMapPreHook/PostHook callbacks "accepted but never invoked" for the
    // process lifetime. r3 keeps r2's boot-time deferral (log hygiene + R3D §5.3 item 3)
    // but QUEUES the delivery instead of retiring it: after the FuncPtr-marshal world is
    // PROVEN live (a funcptr_marshal_hook invocation that published the game-thread
    // identity, IS the published game thread, and owns the action dispatch), a ONE-SHOT
    // deferred arm registers the exact same pre/post LoadMap callbacks the eager block
    // would have. From that point every real UEngine::LoadMap event delivers the queued
    // Lua callbacks through the armed detour (Hooks AddCallback -> EnsureHooked ->
    // ActivateHook -> PLH hook), INCLUDING callbacks registered after the attempt (mod
    // scripts load asynchronously relative to carrier ticks, so queue state at attempt
    // time must not gate the attempt; the delivery lambda iterates the live vectors at
    // fire time). Ordering safety: the pending flag is set in on_program_start BEFORE
    // the marshal consumer can arm the pump, so no drain can precede the flag.
    // FAIL-CLOSED: an attempted arm that fails is NEVER silent — the composed layer's
    // own loud refusal pair fires per failed side (InstallHook's Warning, e.g. "Tried
    // to install hook but hooking is disabled for this function." / "...function is
    // unavailable.", plus AddCallback's Error "[UE4SS.LoadMap.LuaModImpl] Failed to add
    // hook, detour installation likely failed!"), and this TU adds an explicit Error
    // naming exactly what stays undelivered. As composed, bHookLoadMap has no INI
    // mapping (UnrealInitializer.hpp:144 default false; no SettingsManager key), so the
    // expected composed-world refusal is the hooking-disabled pair — reported loudly,
    // once, and never downgraded to a silent skip. Exactly one attempt per process
    // (atomic exchange); this is the r2 deferral LANDING, not a runtime re-arm of the
    // initializer's own registrations (DIFF-2 header law untouched).
    // =====================================================================================

#ifdef __PROSPERO__
    // =====================================================================================
    // [FL-MAP] (agent-IA 2026-10-10, Claude round-8 item 4): the runtime-owned,
    // game-agnostic map-load milestone logger. Replaces mod-side polling for the
    // World (zzdiag's 6s LoopAsync [MAP] cycle) with ordered, contiguous
    // '[FL-MAP] order=<n> map=<name> phase=pre|loaded' markers emitted by the
    // runtime itself, from two complementary sources:
    //
    //   src=detour — [FL-MAP] lines inside lua_load_map_pre_callback /
    //   lua_load_map_post_callback below. These are the framework's OWN
    //   RegisterLoadMapPreCallback / RegisterLoadMapPostCallback registrations
    //   (eager in W-works worlds, one-shot deferred arm in the R3 world) and fire
    //   wherever the LoadMap detour actually arms; phase=pre carries URL.Map (the
    //   map about to load), phase=loaded the constructed World's name.
    //
    //   src=marshal-watch — a bounded 1 Hz World-identity check on the proven
    //   game-thread pump (funcptr_marshal_hook). Round-4 evidence
    //   (round4-spawn-session-20261011.decoded.log) proves this is the ONLY
    //   source that can deliver in the current console world: the deferred LoadMap
    //   arm REFUSED ("[R3] LoadMap deferred arm refused (pre id 0, post id 0)" +
    //   the "[UE4SS.LoadMap.LuaModImpl] Failed to add hook" pairs) and the
    //   StaticConstructObject post registration — the engine seam behind Lua
    //   NotifyOnNewObject — is R3-deferred, so neither prescribed mechanism
    //   delivers there today. The watch resolves the current World via the
    //   FFO2-proven FindFirstOf("World") walk (q='World' exact=2
    //   first=0x18005194C0 arm=walk) and logs a NEW World instance phase=loaded.
    //   Claude's NotifyOnNewObject(World) intent is served by this watch in the
    //   R3 world and by the (already registered, W-works) StaticConstructObject
    //   seam elsewhere; a fresh FUObjectCreateListener was deliberately NOT
    //   registered (AddUObjectCreateListener is the unlocked direct-TArray-append
    //   family with no Variant-C engine-op admission — out of risk scope).
    //
    // Ordering: one order number per map-load event; a pre line and its matching
    // post line share the number. The detour post and the watch share the
    // last-world pointer so a load logged by the detour is never double-counted
    // by the watch. Pointer identity only (address reuse after GC is an accepted
    // rare miss; same-address seamless-travel reuse logs once — see agent-IA
    // limits). All sites run on the game thread (LoadMap detour / proven pump).
    // Name rendering: URL.Map (FString) via LuaFNameBridge::fstring_to_narrow_string
    // (the 'fstring-rev' proven family) for pre; World->GetName()
    // (GetNamePrivate -> pool decoder) for loaded — the same routes GetFullName
    // and [FFO2] q= prove on console. Wide-char log writer law: sanitized to one
    // line, bounded at 64 chars.
    // =====================================================================================
    namespace FlMap
    {
        std::atomic<uint32_t> s_order{0};
        std::atomic<uint32_t> s_pending{0};
        Unreal::UObject* s_last_world{nullptr};

        auto sanitize(StringType name) -> StringType
        {
            if (name.size() > 64) { name.resize(64); }
            for (auto& c : name)
            {
                if (c == '\n' || c == '\r') { c = ' '; }
                else if (c < 32 || c > 126) { c = '?'; }
            }
            return name;
        }

        auto world_name(const Unreal::UObject* world) -> StringType
        {
            if (!world) { return {}; }
            try
            {
                return sanitize(world->GetName());
            }
            catch (...)
            {
                return {};
            }
        }

        auto url_name(const Unreal::FURL& url) -> std::string
        {
            try
            {
                return LuaType::LuaFNameBridge::fstring_to_narrow_string(url.Map);
            }
            catch (...)
            {
                return {};
            }
        }

        auto note_pre_detour(const Unreal::FURL& url) -> void
        {
            const uint32_t order = ++s_order;
            s_pending.store(order, std::memory_order_relaxed);
            const std::string map_narrow = url_name(url);
            Output::send(STR("[FL-MAP] order={} map='{}' phase=pre src=detour\n"), order, ensure_str(map_narrow));
        }

        auto note_loaded_detour(Unreal::FWorldContext& world_context, const Unreal::FURL& url) -> void
        {
            Unreal::UObject* world = world_context.GetThisCurrentWorld();
            s_last_world = world;
            uint32_t order = s_pending.exchange(0, std::memory_order_relaxed);
            if (order == 0)
            {
                // Post without a matching pre (arm landed between phases):
                // allocate a fresh number rather than reusing a stale one.
                order = ++s_order;
            }
            const std::string map_narrow = url_name(url);
            Output::send(STR("[FL-MAP] order={} map='{}' phase=loaded src=detour url='{}'\n"),
                         order, world_name(world), ensure_str(map_narrow));
        }

        // Bounded 1 Hz World-identity watch; called ONLY from funcptr_marshal_hook
        // after the identity publication, the raw game-thread gate, dispatch-scope
        // ownership and the deferred LoadMap arm — i.e. on the proven pump thread.
        // The FindFirstOf("World") walk is the FFO2/console-proven verified path
        // (FindFirstOfVerified -> FName walk, ~40-150K objects); at 1 Hz that is
        // negligible against the carrier cadence, and it is the same walk the
        // game-thread self-test ([ST] lua-ffo ... PASS) already ran on console.
        auto marshal_watch() -> void
        {
            // DISABLED (launch-2 regression): the engine World scan inside
            // the marshal hook killed keybind callbacks. Event-driven only.
            return;
            static std::atomic<int64_t> s_watch_ms{0};
            const int64_t now_ms = r3_now_ms();
            int64_t last_ms = s_watch_ms.load(std::memory_order_relaxed);
            if (now_ms - last_ms < 10000) // dead code (see return above)
            {
                return;
            }
            if (!s_watch_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed))
            {
                return;
            }

            Unreal::UObject* world = nullptr;
            try
            {
                world = Unreal::UObjectGlobals::FindFirstOf(STR("World"));
            }
            catch (...)
            {
                return;
            }
            if (!world || world == s_last_world)
            {
                return;
            }
            s_last_world = world;
            const uint32_t order = ++s_order;
            Output::send(STR("[FL-MAP] order={} map='{}' phase=loaded src=marshal-watch\n"), order, world_name(world));
        }
    }
#endif

    // The LoadMap pre/post Lua-delivery bodies, verbatim from the eager block they serve
    // (only the m_load_map_*_callbacks access gained its LuaMod:: qualification for file
    // scope). Both the eager block (W-works world, pump_pending=false) and the one-shot
    // deferred arm (R3 marshal world) pass these, so delivery semantics — game-thread
    // pre/post-LoadMap timing, 5 pushed args, bool-or-nil return contract — are
    // identical whichever path arms.
    [[maybe_unused]] static auto lua_load_map_pre_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData, Unreal::UEngine* Engine, Unreal::FWorldContext& WorldContext, Unreal::FURL URL, Unreal::UPendingNetGame* PendingGame, Unreal::FString& Error) -> void
    {
        TRY([&] {
#ifdef __PROSPERO__
            // [FL-MAP] phase=pre milestone marker (see namespace FlMap above):
            // logged for EVERY real LoadMap event the armed detour delivers,
            // before any Lua callback runs, with the map about to load.
            try
            {
                FlMap::note_pre_detour(URL);
            }
            catch (...)
            {
            }
#endif
            for (const auto& callback_data : LuaMod::m_load_map_pre_callbacks)
            {
                for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                {
                    const auto& lua = *lua_ptr;

                    lua.registry().get_function_ref(registry_index.lua_index);
                    static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                    LuaType::RemoteUnrealParam::construct(lua, &Engine, s_object_property_name);
                    LuaType::RemoteUnrealParam::construct(lua, &WorldContext.GetThisCurrentWorld(), s_object_property_name);
                    LuaType::FURL::construct(lua, URL);
                    LuaType::RemoteUnrealParam::construct(lua, &PendingGame, s_object_property_name);
#ifdef __PROSPERO__
                    // [LFB-M] agent-FM: LoadMap Error FString -> Lua string
                    // via the bridge reverse family (logs 'fstring-rev').
                    LuaType::LuaFNameBridge::push_fstring_to_lua(callback_data.lua->get_lua_state(), Error);
#else
                    callback_data.lua->set_string(to_string(*Error));
#endif
                    lua.call_function(5, 1);

                    if (callback_data.lua->is_nil())
                    {
                        callback_data.lua->discard_value();
                    }
                    else if (!callback_data.lua->is_bool())
                    {
                        throw std::runtime_error{"A callback for 'LoadMap' must return bool or nil"};
                    }
                    else
                    {
                        CallbackIterationData.TrySetReturnValue(callback_data.lua->get_bool());
                    }
                }
            }
        });
    }

    [[maybe_unused]] static auto lua_load_map_post_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData, Unreal::UEngine* Engine, Unreal::FWorldContext& WorldContext, Unreal::FURL URL, Unreal::UPendingNetGame* PendingGame, Unreal::FString& Error) -> void
    {
        TRY([&] {
#ifdef __PROSPERO__
            // [FL-MAP] phase=loaded milestone marker (see namespace FlMap above):
            // logged for EVERY real LoadMap event the armed detour delivers, with
            // the constructed World's name; shares the order number of its pre
            // line and publishes the pointer so the marshal watch never
            // double-counts this load.
            try
            {
                FlMap::note_loaded_detour(WorldContext, URL);
            }
            catch (...)
            {
            }
#endif
            for (const auto& callback_data : LuaMod::m_load_map_post_callbacks)
            {
                for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                {
                    const auto& lua = *lua_ptr;

                    lua.registry().get_function_ref(registry_index.lua_index);
                    static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                    LuaType::RemoteUnrealParam::construct(lua, &Engine, s_object_property_name);
                    LuaType::RemoteUnrealParam::construct(lua, &WorldContext.GetThisCurrentWorld(), s_object_property_name);
                    LuaType::FURL::construct(lua, URL);
                    LuaType::RemoteUnrealParam::construct(lua, &PendingGame, s_object_property_name);
#ifdef __PROSPERO__
                    // [LFB-M] agent-FM: LoadMap Error FString -> Lua string
                    // via the bridge reverse family (logs 'fstring-rev').
                    LuaType::LuaFNameBridge::push_fstring_to_lua(callback_data.lua->get_lua_state(), Error);
#else
                    callback_data.lua->set_string(to_string(*Error));
#endif
                    lua.call_function(5, 1);

                    if (callback_data.lua->is_nil())
                    {
                        callback_data.lua->discard_value();
                    }
                    else if (!callback_data.lua->is_bool())
                    {
                        throw std::runtime_error{"A callback for 'LoadMap' must return bool or nil"};
                    }
                    else
                    {
                        CallbackIterationData.TrySetReturnValue(callback_data.lua->get_bool());
                    }
                }
            }
        });
    }

    // [EARLY-4] (cap #4) The PS5 one-shot deferred LoadMap DETOUR arm that used to sit here is
    // removed. On PS5 game code is execute-only and the rules forbid code detours, so that arm
    // could only ever be refused (launch 14: "[R3] LoadMap deferred arm refused (pre id 0, post id 0)"
    // after two "Failed to add hook" lines), and a half-success would have left a partial native
    // registration. LoadMap / InitGameState / BeginPlay Lua hooks on PS5 are delivered ONLY by the
    // data-only [WORLD-EV] object-index route; refused_log::loadmap_state is set to 3 at startup
    // and never changes. The delivery bodies above stay for the non-PS5 eager block.
#ifdef __PROSPERO__
    // [EARLY-4] one-shot startup timeline markers (same steady clock as r3_now_ms; the value
    // passed to the diagnostic sink is the low 32 bits of that clock, so the host verifier can
    // prove the UE4SS log clock and the sink's CLOCK_MONOTONIC t_ms are the same clock).
    namespace early4
    {
        static auto now_ms() -> int64_t
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        static std::atomic<bool> s_first_pump_noted{false};
        static auto note_first_pump(bool vmt_tick) -> void
        {
            if (s_first_pump_noted.exchange(true, std::memory_order_acq_rel)) { return; }
            const int64_t t = now_ms();
            diagnostic_event("early_first_pump", static_cast<unsigned int>(static_cast<uint64_t>(t) & 0xFFFF'FFFFu));
            try { Output::send(STR("[EARLY] stage=first-pump t_ms={} src={}\n"), t, vmt_tick ? STR("vmt-tick") : STR("marshal")); } catch (...) {}
        }
        // [cap07 step 2, launch I fix] Runtime pak mount: once per process, from WHICHEVER game-thread pump
        // runs first. Console-proven 2026-10-13 (launch I): the VMT tick pump never arms on PS5
        // ("[EU-VMT] arming refused: non-text-ent (R3 carriers remain the pump)", "[EARLY] stage=first-pump
        // src=marshal"), so a trigger only in VmtEngineTickDrain never ran (only "startup admission" was logged).
        // Both drains call this; the shared flag keeps it to one request. Nothing may escape (noexcept caller).
        static std::atomic<bool> s_pak_mount_requested{false};
        static auto request_pak_mount_once() noexcept -> void
        {
            if (s_pak_mount_requested.load(std::memory_order_relaxed) ||
                s_pak_mount_requested.exchange(true, std::memory_order_acq_rel))
            {
                return;
            }
            try
            {
                // The configured game directory (/app0 on PS5) roots the game-pak source (launch K/L).
                Unreal::RunNativePakMountOnce(UE4SSProgram::get_program().get_mods_directory(),
                                              UE4SSProgram::get_program().get_game_executable_directory());
            }
            catch (...)
            {
                try { Output::send(STR("[PAKMOUNT] stopped: exception while preparing the mount request\n")); } catch (...) {}
            }
        }
    }
#endif

    // =====================================================================================
    // [R3] The FuncPtr-marshal drain body (DIFF 1; R3D 4.1/4.3). Registered as a
    // POST-hook on the carrier: the original function has already executed unmodified;
    // this body never touches parms or RESULT_DECL and never alters control flow.
    // The PROSPERO prologue is process_event_hook's; the drain block is
    // process_event_hook's with the delayed-action executor tagged FuncPtrMarshal.
    // Thread honesty (R3D 4.3): until the initializer bridge publishes the game-thread
    // identity, the marshal DRAINS NOTHING except the publication step itself.
    // =====================================================================================
    static auto funcptr_marshal_hook([[maybe_unused]] Unreal::UnrealScriptFunctionCallableContext& Context,
                                     [[maybe_unused]] void* CustomData) -> void
    {
        // [R9-GUARD] the engine calls this directly: nothing may escape into engine code
        // (escaping = std::terminate / abort; launch 3 crash).
        try
        {
            // [l25] runtime-phase tag (Claude l25 item 6): once-only proof that a
            // carrier UFunction ever fired this hook at all (the ScriptHookThunks
            // UFunction::Func swap worked and the engine called the carrier). This
            // is the first split of any marshal-path diagnosis: absence means the
            // carrier never invoked the hook; presence localizes any stall to the
            // downstream identity/gate/drain steps below.
            static std::atomic<bool> s_marshal_hook_first_invocation{true};
            if (s_marshal_hook_first_invocation.exchange(false, std::memory_order_relaxed))
            {
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l25] marshal hook first invocation (carrier fired)\n"));
                });
                diagnostic_event("r3_marshal_first_invoke", 1u);
            }
    #ifdef __PROSPERO__
            auto native_name_ticket=NativeNameThread::acquire_runtime();
            if(!native_name_ticket.ticket)return;
            // ORDER MATTERS (R3D 4.3): under BOTH_FAIL nothing else publishes the identity,
            // so the marshal CANNOT gate on IsInGameThreadRaw() first - before publication
            // that predicate is false for every thread and the pump would never start. The
            // first accepted invocation publishes via the initializer bridge (publish_current
            // is mutex-arbitrated: exactly one thread wins; every other thread, on this and
            // later invocations, refuses); from then on the raw gate refuses worker threads.
            // The publishing thread BECOMES the game thread - the same semantics as
            // HookedEngineTick's genuine-callback publication, backed by the calibration's
            // game-thread-stability criterion C-4 (CARRIER-CANDIDATES).
            if (!m_funcptr_marshal_identity_published)
            {
                // 4.4a: the HookedEngineTick publication body, routed onto the marshal.
                // Published/AlreadyCurrent both mean this caller IS the game thread;
                // Unavailable/DifferentThread/StartupWait-cancelled refuse (false).
                if (!Unreal::R3MarshalPublishGameThreadIdentity())
                {
                    return;
                }
                m_funcptr_marshal_identity_published = true;
                if (Unreal::R3MarshalPumpPending() && !m_funcptr_marshal_pump_pending_logged)
                {
                    m_funcptr_marshal_pump_pending_logged = true;
                    Output::send(STR("[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)\n"));
                }
            }
            // The carrier may be invoked on worker threads. Do not inspect Lua action
            // state until the published game thread is this exact caller thread.
            if (!Unreal::IsInGameThreadRaw())
            {
                return;
            }
            // [claude-cloud event-carrier] liveness for r3_marshal_lazy_arm's staleness rescan.
            m_funcptr_marshal_last_fire_ms.store(r3_now_ms(), std::memory_order_relaxed);
            r9perf::frame_tick();
            const r9perf::TimedScope pump_perf{r9perf::pump_calls, r9perf::pump_ns};
            // [l25] throttled pump liveness (Claude l25 item 6, exact recommendation):
            // at most one cheap diagnostic_event("r3_marshal_pumped", ms) per second
            // once the carrier fires on the published game thread past the raw gate.
            // Proves the drain path stays reachable; no allocation, no log spam.
            {
                static std::atomic<int64_t> s_marshal_pump_event_ms{0};
                const int64_t pump_now_ms = r3_now_ms();
                int64_t last_pump_event_ms = s_marshal_pump_event_ms.load(std::memory_order_relaxed);
                if (pump_now_ms - last_pump_event_ms >= 1000 &&
                    s_marshal_pump_event_ms.compare_exchange_strong(last_pump_event_ms, pump_now_ms, std::memory_order_relaxed))
                {
                    diagnostic_event("r3_marshal_pumped", static_cast<unsigned int>(pump_now_ms & 0xFFFF'FFFF));
                }
            }
            NativeLuaActionDispatchScope action_dispatch;
            if (!action_dispatch.owns_dispatch())
                return;
            // [EARLY-4] the marshal world is now PROVEN live (identity published, this IS the
            // published game thread, dispatch owned): record the first game-thread pump once.
            // (The deferred LoadMap detour arm that ran here is removed: no detours on PS5.)
            early4::note_first_pump(false);
            early4::request_pak_mount_once();
            // [FL-MAP] REMOVED from the dispatch path (Claude round-10 item 2,
            // launch-2 spawner regression root cause): the 1 Hz World-identity
            // scan ran INSIDE funcptr_marshal_hook — the same hook that
            // dispatches keybind callbacks. The 40K-object engine scan during
            // callback dispatch corrupted/killed the Lua callback execution
            // (launch-2 evidence: F7 keybind entries with ZERO Lua output and
            // ZERO ExecuteInGameThread queues from either mod; round-4 without
            // this code: both queued and ran). Map-load logging is now
            // event-driven only (NotifyOnNewObject at framework start, see the
            // [FL-MAP-EV] registration in UnrealInitializer).
    #endif
            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = true;
            }

            process_simple_actions(LuaMod::m_game_thread_actions);
            process_delayed_actions<kFuncPtrMarshalExecutor>(LuaMod::m_delayed_game_thread_actions);

            process_pending_notify_on_new_object_callbacks();
    #ifdef __PROSPERO__
            // [CAP09] native C++ mods: game-thread tasks + per-frame callbacks
            // on the same data-only pump (never throws).
            RC::Platform::NativeModsGameThreadFrame(0);
            RC::Platform::KbmGameThreadFrame(); // [K2] keyboard/mouse engine-keys sampler (at most every 8 ms)
    #endif

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = false;
                if (!LuaMod::m_pending_game_thread_actions.empty())
                {
                    LuaMod::m_game_thread_actions.insert(
                        LuaMod::m_game_thread_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_game_thread_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_game_thread_actions.end()));
                    LuaMod::m_pending_game_thread_actions.clear();
                }
                if (!LuaMod::m_pending_delayed_game_thread_actions.empty())
                {
                    LuaMod::m_delayed_game_thread_actions.insert(
                        LuaMod::m_delayed_game_thread_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.end()));
                    LuaMod::m_pending_delayed_game_thread_actions.clear();
                }
            }
        }
        catch (const std::exception& e)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception: {}\n"), ensure_str(e.what())); } catch (...) {}
        }
        catch (...)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception\n")); } catch (...) {}
        }
    }

#if defined(__PROSPERO__)
    // =====================================================================================
    // [EU FIX4 2026-10-09, agent-EU] VmtEngineTickDrain — the copied-vtable
    // Tick pump's queue drain, declared by the arming unit (UnrealInitializer
    // VmtPump glue) and called from the genuine UEngine::Tick hook body AFTER
    // the original Tick returned and the first-call identity publication
    // succeeded. This is the R3 marshal's drain shape (funcptr_marshal_hook
    // above: raw game-thread gate -> dispatch-scope ownership -> deferred
    // LoadMap arm -> simple actions -> marshal-tagged delayed actions ->
    // pending NotifyOnNewObject callbacks -> pending requeue), with the l26
    // FIX 4 additions: TIME-BOX (a steady_clock deadline gates the later
    // stages; unrun work stays queued for the next tick — never dropped) and
    // a per-tick simple-action cap (overflow returns to the pending queue in
    // front of the requeue below, so the next genuine tick drains it). The
    // R3 carriers remain the FALLBACK pump: both drains share the queue-move
    // semantics, the dispatch-scope/thread_local guard prevents concurrent
    // double dispatch, and sequential double drains are idempotent (each
    // action is moved out of its queue exactly once).
    // =====================================================================================
    namespace Unreal
    {
        auto VmtEngineTickDrain() noexcept -> void
        {
            // [R9-GUARD] the engine calls this directly: nothing may escape into engine code
            // (escaping = std::terminate / abort; launch 3 crash).
            try
            {
                try
                {
                    // Identity was published by the hook body (UnrealInitializer
                    // VmtPump) before this call; the raw gate still refuses any
                    // foreign-thread entry (defensive: the hook cannot fire on a
                    // worker thread, but the bridge is exported).
                    if (!Unreal::IsInGameThreadRaw())
                    {
                        return;
                    }
                    NativeLuaActionDispatchScope action_dispatch;
                    if (!action_dispatch.owns_dispatch())
                    {
                        return;
                    }
                    // [EARLY-4] first game-thread pump (the deferred LoadMap detour arm
                    // that ran here is removed: no detours on PS5).
                    early4::note_first_pump(true);
                    // [cap07 review] once per process, shared with the R3 marshal drain (launch I fix).
                    early4::request_pak_mount_once();
                    {
                        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                        LuaMod::m_is_processing_actions = true;
                        // FIX4 per-tick cap: drain at most kCap simple actions per
                        // genuine tick; the overflow moves to the FRONT of the
                        // pending queue so the requeue below returns it to the
                        // active queue in order for the next tick.
                        constexpr size_t kVmtMaxActionsPerTick = 128;
                        if (LuaMod::m_game_thread_actions.size() > kVmtMaxActionsPerTick)
                        {
                            auto& active = LuaMod::m_game_thread_actions;
                            auto& pending = LuaMod::m_pending_game_thread_actions;
                            pending.insert(pending.begin(),
                                           std::make_move_iterator(active.begin() + static_cast<std::ptrdiff_t>(kVmtMaxActionsPerTick)),
                                           std::make_move_iterator(active.end()));
                            active.resize(kVmtMaxActionsPerTick);
                        }
                    }
                    process_simple_actions(LuaMod::m_game_thread_actions);
                    // FIX4 time-box: stages past the deadline are skipped this
                    // tick; their work is untouched (delayed actions keep their
                    // status, pending callbacks stay pending).
                    constexpr std::chrono::milliseconds kVmtDrainBudget{4};
                    const auto drain_deadline = std::chrono::steady_clock::now() + kVmtDrainBudget;
                    if (std::chrono::steady_clock::now() < drain_deadline)
                    {
                        process_delayed_actions<kFuncPtrMarshalExecutor>(LuaMod::m_delayed_game_thread_actions);
                    }
                    if (std::chrono::steady_clock::now() < drain_deadline)
                    {
                        process_pending_notify_on_new_object_callbacks();
                    }
                    // [CAP09] native C++ mods (source 1 = copied-vtable Tick pump).
                    RC::Platform::NativeModsGameThreadFrame(1);
                    RC::Platform::KbmGameThreadFrame(); // [K2] same sampler; its 8 ms gate keeps it once per frame
                    {
                        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                        LuaMod::m_is_processing_actions = false;
                        if (!LuaMod::m_pending_game_thread_actions.empty())
                        {
                            LuaMod::m_game_thread_actions.insert(
                                LuaMod::m_game_thread_actions.end(),
                                std::make_move_iterator(LuaMod::m_pending_game_thread_actions.begin()),
                                std::make_move_iterator(LuaMod::m_pending_game_thread_actions.end()));
                            LuaMod::m_pending_game_thread_actions.clear();
                        }
                        if (!LuaMod::m_pending_delayed_game_thread_actions.empty())
                        {
                            LuaMod::m_delayed_game_thread_actions.insert(
                                LuaMod::m_delayed_game_thread_actions.end(),
                                std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.begin()),
                                std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.end()));
                            LuaMod::m_pending_delayed_game_thread_actions.clear();
                        }
                    }
                }
                catch (...)
                {
                    // The hook body (drive_tick) already isolated this drain; a
                    // drain failure never propagates into the engine Tick path.
                }
            }
            catch (const std::exception& e)
            {
                try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception: {}\n"), ensure_str(e.what())); } catch (...) {}
            }
            catch (...)
            {
                try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception\n")); } catch (...) {}
            }
        }
    }
#endif

    auto static engine_tick_hook([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData,
                                 [[maybe_unused]] Unreal::UEngine* Context,
                                 [[maybe_unused]] float DeltaSeconds,
                                 [[maybe_unused]] bool bIdle) -> void
    {
        // [R9-GUARD] the engine calls this directly: nothing may escape into engine code
        // (escaping = std::terminate / abort; launch 3 crash).
        try
        {
    #ifdef __PROSPERO__
            // Each callback must check the identity published by the real Tick.
            // An earlier callback's rejection does not stop this callback.
            if (!Unreal::IsInGameThreadRaw())
            {
                return;
            }
            NativeLuaActionDispatchScope action_dispatch;
            if (!action_dispatch.owns_dispatch())
                return;
    #endif
            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = true;
            }

            process_simple_actions(LuaMod::m_engine_tick_actions);
            process_delayed_actions<GameThreadExecutionMethod::EngineTick>(LuaMod::m_delayed_game_thread_actions);

            process_pending_notify_on_new_object_callbacks();

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                LuaMod::m_is_processing_actions = false;
                if (!LuaMod::m_pending_engine_tick_actions.empty())
                {
                    LuaMod::m_engine_tick_actions.insert(
                        LuaMod::m_engine_tick_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_engine_tick_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_engine_tick_actions.end()));
                    LuaMod::m_pending_engine_tick_actions.clear();
                }
                if (!LuaMod::m_pending_delayed_game_thread_actions.empty())
                {
                    LuaMod::m_delayed_game_thread_actions.insert(
                        LuaMod::m_delayed_game_thread_actions.end(),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.begin()),
                        std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.end()));
                    LuaMod::m_pending_delayed_game_thread_actions.clear();
                }
            }
        }
        catch (const std::exception& e)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception: {}\n"), ensure_str(e.what())); } catch (...) {}
        }
        catch (...)
        {
            try { Output::send<LogLevel::Error>(STR("[R9-GUARD] game-thread hook contained an exception\n")); } catch (...) {}
        }
    }

    // Local convenience wrappers for Capabilities functions
    static auto is_engine_tick_hook_available() -> bool
    {
        return UE4SSRuntime::IsEngineTickAvailable();
    }

    static auto is_process_event_hook_available() -> bool
    {
        return UE4SSRuntime::IsProcessEventAvailable();
    }

    // =====================================================================================
    // [L26][P2] (agent FX, 2026-10-10): r3_marshal_carrier_precheck +
    // arm_funcptr_marshal_drain were REMOVED - dead code. Their ONLY caller
    // was the ranked-natives loop of r3_marshal_calibration_consumer, and
    // that loop could NEVER arm: both ranked names ("Actor:ReceiveTick",
    // "AnimInstance:BlueprintUpdateAnimation") are
    // BlueprintImplementableEvents, so the FUNC_Native precheck refused them
    // BY CONSTRUCTION (the code's own former comment admitted it), and on
    // console the StaticFindObject lookups cannot resolve at all (GATE-1
    // numbered duplicates; ET nul-throw-armor session evidence: "marshal
    // carrier candidate not found ... not interned"). The event-carrier
    // family (arm_event_carrier_locked, below) never called these. See
    // outputs/2026-10-08-l25-prep/parked/P2-variant-b-removal/.
    // =====================================================================================

#ifdef __PROSPERO__
    // =====================================================================================
    // [claude-cloud 2026-10-06 event-carrier] Blueprint-override carrier family.
    // Engine per-frame events (AActor::ReceiveTick, UAnimInstance::BlueprintUpdateAnimation,
    // UUserWidget::Tick) are BlueprintImplementableEvents: the engine calls them through
    // UObject::ProcessEvent -> UFunction::Invoke -> Func on the game thread, resolving the
    // MOST-DERIVED UFunction, i.e. the Blueprint class's own override object. Those
    // overrides are script functions (Func = the VM entry, no FUNC_Native), so
    // the former r3_marshal_carrier_precheck refused them (FUNC_Native gate);
    // a Func swap on the override object
    // intercepts exactly the engine-driven Invoke while the saved original Func still runs
    // the Blueprint body (post-hook, parms untouched). Calls from other Blueprint code use
    // the local-script path and bypass Func; that only means fewer drains, never a wrong
    // thread. Every armed function is game-thread-only by engine contract; the drain's
    // identity publication + IsInGameThreadRaw gate stay the backstop.
    // =====================================================================================
    static auto r3_event_carrier_precheck(Unreal::UFunction* carrier) -> bool
    {
        if (!carrier || !carrier->GetFuncPtr())
        {
            return false;
        }
        // Natives were the (removed) ranked path; this family is script overrides only.
        return !carrier->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native);
    }

    // Caller holds m_funcptr_marshal_carrier_mutex.
    static auto arm_event_carrier_locked(Unreal::UFunction* carrier) -> bool
    {
        if (!r3_event_carrier_precheck(carrier))
        {
            return false;
        }
        if (std::find(m_funcptr_marshal_event_carriers.begin(), m_funcptr_marshal_event_carriers.end(), carrier) != m_funcptr_marshal_event_carriers.end())
        {
            return false; // already armed on this function
        }
        const int32_t callback_id = carrier->RegisterPostHook(&funcptr_marshal_hook, nullptr);
        if (callback_id < 0)
        {
            Output::send<LogLevel::Error>(STR("[R3] event carrier refused: RegisterPostHook returned an invalid callback id for carrier [n: {:X}] @0x{:016X}\n"),
                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
                                         reinterpret_cast<uintptr_t>(carrier));
            return false;
        }
        m_funcptr_marshal_event_carriers.push_back(carrier);
        if (m_funcptr_marshal_event_carriers.size() <= 8)
        {
            Output::send(STR("[R3] FuncPtr-marshal event carrier armed: [n: {:X}] @0x{:016X} (callback id {})\n"),
                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
                         reinterpret_cast<uintptr_t>(carrier),
                         callback_id);
        }
        if (!is_funcptr_marshal_armed())
        {
            m_funcptr_marshal_callback_id = callback_id;
            m_funcptr_marshal_carrier_id = carrier->GetFName();
            m_funcptr_marshal_armed.store(true, std::memory_order_release);
        }
        return true;
    }

    static auto r3_marshal_arm_event_carriers() -> size_t
    {
        static constexpr const CharType* k_event_bases[]{
            STR("/Script/Engine.Actor:ReceiveTick"),
            STR("/Script/Engine.AnimInstance:BlueprintUpdateAnimation"),
            STR("/Script/UMG.UserWidget:Tick"),
        };
        std::lock_guard<std::mutex> lock{m_funcptr_marshal_carrier_mutex};
        std::vector<Unreal::UStruct*> bases{};
        for (const CharType* base_path : k_event_bases)
        {
            const StringType base_string{base_path};
            if (auto* base = Unreal::UObjectGlobals::StaticFindObject<Unreal::UFunction*>(nullptr, nullptr, base_string); base)
            {
                bases.push_back(base);
            }
        }
        if (bases.empty())
        {
            Output::send<LogLevel::Warning>(STR("[R3] event carriers: no engine per-frame event base function found yet\n"));
            return 0;
        }
        static const Unreal::FName function_class_name{STR("Function"), Unreal::FNAME_Find};
        std::vector<Unreal::UFunction*> overrides{};
        Unreal::UObjectGlobals::ForEachUObject([&](void* raw_object, [[maybe_unused]] int32_t chunk_index, [[maybe_unused]] int32_t object_index) {
            auto* object = static_cast<Unreal::UObject*>(raw_object);
            if (!object)
            {
                return LoopAction::Continue;
            }
            auto* object_class = object->GetClassPrivate();
            if (!object_class || !object_class->GetNamePrivate().Equals(function_class_name))
            {
                return LoopAction::Continue;
            }
            auto* function = static_cast<Unreal::UFunction*>(object);
            // An override's SuperStruct is the function it overrides (BP child -> BP
            // parent override -> engine base); bounded walk.
            Unreal::UStruct* super = function->GetSuperStruct();
            for (int depth = 0; super && depth < 16; ++depth, super = super->GetSuperStruct())
            {
                if (std::find(bases.begin(), bases.end(), super) != bases.end())
                {
                    overrides.push_back(function);
                    break;
                }
            }
            return LoopAction::Continue;
        });
        size_t armed_now{};
        for (auto* function : overrides)
        {
            if (arm_event_carrier_locked(function))
            {
                ++armed_now;
            }
        }
        Output::send(STR("[R3] event carrier scan: {} override(s) found, {} newly armed, {} armed total\n"),
                     overrides.size(), armed_now, m_funcptr_marshal_event_carriers.size());
        return armed_now;
    }

    // =====================================================================================
    // [R3] THE CALIBRATION CONSUMER (launch-11 marshal-integration; the driver of the
    // arming service R3-TIER-DESIGN 5.2 item 4 scopes and DIFF 1 deliberately left
    // unimplemented — worker-r3-diffs-authoring REPORT 4: "the only intended caller of
    // arm_funcptr_marshal_drain"). Lifecycle (R3D 4.4a): called ONCE at the top of
    // LuaMod::on_program_start() — after setup_unreal() (the object world exists) and
    // before start_lua_mods() — and BEFORE this function's first eager detour
    // registration, so the deferral window is used before anything that can throw under
    // BOTH_FAIL. Arming is GATED on the initializer's own deferral state (DIFF 2's
    // pump_pending via R3MarshalPumpPending): in the W-works world the initializer owns
    // the pump and this consumer performs no store at all — behavior-identical launch-10.
    // Selection is the bounded ranked stand-in for the in-game rate calibration (R3D 4.2
    // steps 2-4 stay the F-1 falsifier): the candidate set is CARRIER-CANDIDATES §3's
    // offline-closed space in rank order; resolution is DATA-SIDE ONLY via
    // StaticFindObject (the same route the Lua RegisterHook native branch uses); the
    // first candidate that passes arm_funcptr_marshal_drain's own precheck wins and
    // lower ranks are never touched. Rank 3 ("Tick", owner ambiguous offline) is
    // deliberately EXCLUDED — an unknown-owner arm is worse than a loud failure; the
    // cc1/in-runtime census resolves its owner before it may join this table.
    // No new imports: StringType, StaticFindObject, FName, Output are all already
    // included/used in this TU; FNAME_Add is never issued (lookup-only names).
    // [L26][P2] (agent FX, 2026-10-10): the ranked-natives selection described
    // above is HISTORY - the ranked loop and its arm_funcptr_marshal_drain
    // helper were removed (dead by construction; see the tombstones). This
    // consumer arms the Blueprint-override event carriers ONLY.
    // =====================================================================================
    static auto r3_marshal_calibration_consumer() -> void
    {
        if (is_funcptr_marshal_armed())
        {
            return; // idempotent: a re-entered on_program_start can never re-arm
        }
        if (!Unreal::R3MarshalPumpPending())
        {
            Output::send(STR("[R3] marshal not armed: initializer owns the game-thread pump (pump_pending=false)\n"));
            return;
        }
        // [L26][P2] (agent FX, 2026-10-10): the ranked-natives carrier loop was
        // REMOVED here - dead by construction (BlueprintImplementableEvents
        // never pass the FUNC_Native precheck; on console the long-name
        // lookups cannot resolve: GATE-1 numbered duplicates). Arm ONLY the
        // Blueprint-override event carriers below.
        m_funcptr_marshal_last_scan_ms.store(r3_now_ms(), std::memory_order_relaxed);
        if (r3_marshal_arm_event_carriers() > 0)
        {
            return;
        }
        Output::send<LogLevel::Warning>(STR("[R3] marshal arm failed: no ranked candidate passed the carrier precheck; closures will queue until a carrier arms\n"));
    }

    static auto r3_marshal_lazy_arm() -> void
    {
        // Rescan when nothing is armed yet (Blueprint classes load with levels, after
        // program start), or when armed carriers have stopped firing (their classes can
        // unload with a level). At most one GUObjectArray scan per second.
        const auto now = r3_now_ms();
        if (is_funcptr_marshal_armed())
        {
            const auto last_fire = m_funcptr_marshal_last_fire_ms.load(std::memory_order_relaxed);
            if (last_fire != 0 && now - last_fire <= 1000)
            {
                return;
            }
        }
        const auto last_scan = m_funcptr_marshal_last_scan_ms.load(std::memory_order_relaxed);
        if (last_scan != 0 && now - last_scan < 1000)
        {
            return;
        }
        m_funcptr_marshal_last_scan_ms.store(now, std::memory_order_relaxed);
        r3_marshal_arm_event_carriers();
    }
#endif

    // Helper to ensure engine tick hook is registered
    auto LuaMod::ensure_engine_tick_hooked() -> void
    {
#ifdef __PROSPERO__
        std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
#endif
        if (!m_is_engine_tick_hooked)
        {
#if defined(__PROSPERO__)
            const auto id = Unreal::Hook::RegisterEngineTickPreCallback(engine_tick_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("EngineTick callback registration failed");
#else
            Unreal::Hook::RegisterEngineTickPreCallback(engine_tick_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
#endif
            m_is_engine_tick_hooked = true;
        }
    }

    // Helper to ensure process event hook is registered
    auto LuaMod::ensure_process_event_hooked(LuaMod* mod) -> void
    {
#ifdef __PROSPERO__
        std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
        if (NativeProcessEventHookId == Unreal::Hook::ERROR_ID)
        {
            const auto id = Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("ProcessEvent callback registration failed");
            NativeProcessEventHookId = id;
        }
        mod->m_is_process_event_hooked = true;
#else
        if (!mod->m_is_process_event_hooked)
        {
#if defined(__PROSPERO__)
            const auto id = Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("ProcessEvent callback registration failed");
#else
            Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
#endif
            mod->m_is_process_event_hooked = true;
        }
#endif
    }

    auto LuaMod::setup_lua_global_functions_main_state_only() const -> void
    {
        m_lua.register_function("RegisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            std::string error_overload_not_found{R"(
No overload found for function 'RegisterHook'.
Overloads:
#1: RegisterHook(string UFunction_Name, LuaFunction Callback, LuaFunction PostCallback))"};

            if (!lua.is_string())
            {
                throw std::runtime_error(error_overload_not_found);
            }

            // [LFB] Claude item 2 (RegisterHook path names): the ONE hand
            // converter owns the Lua function-name string (was ensure_str).
            const LuaType::LuaFNameBridge::Bridge hook_name{lua.get_lua_state()};
            auto function_name_no_prefix = get_function_name_without_prefix(hook_name.string());

            if (!lua.is_function())
            {
                throw std::runtime_error(error_overload_not_found);
            }

            auto mod = get_mod_ref_cxx(lua);
            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod); // operates on LuaMod::m_lua incrementing its stack via lua_newthread

            // Duplicate the Lua function to the top of the stack for lua_xmove and luaL_ref
            lua_pushvalue(lua.get_lua_state(), 1); // operates on LuaMadeSimple::Lua::m_lua_state
            lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);

            // Take a reference to the Lua function (it also pops it of the stack)
            const auto lua_callback_registry_index = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            bool has_post_callback{};
            int lua_post_callback_registry_index = -1;
            if (lua.is_function())
            {
                lua.discard_value();
                // Duplicate the second Lua function to the top of the stack for lua_xmove and luaL_ref
                lua_pushvalue(lua.get_lua_state(), 1); // operates on LuaMadeSimple::Lua::m_lua_state
                lua_xmove(lua.get_lua_state(), hook_lua->get_lua_state(), 1);
                lua_post_callback_registry_index = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);
                has_post_callback = true;
            }

            Unreal::UFunction* unreal_function = Unreal::UObjectGlobals::StaticFindObject<Unreal::UFunction*>(nullptr, nullptr, function_name_no_prefix);
            if (!unreal_function)
            {
                refused_log::throws(lua, "RegisterHook", function_name_no_prefix, "ufunction-not-found");
                throw std::runtime_error(std::format(
                        "Tried to register a hook with Lua function 'RegisterHook' but no UFunction with the specified name was found.\nFunction Name: {}",
                        to_string(function_name_no_prefix)));
            }

            int32_t pre_id{};
            int32_t post_id{};

            auto func_ptr = unreal_function->GetFunc();
            if (func_ptr && func_ptr != Unreal::UObject::ProcessInternalInternal.get_function_address() &&
                unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
            {
                auto& custom_data = g_hooked_script_function_data.emplace_back(std::make_unique<LuaUnrealScriptFunctionData>(
                        0, 0, unreal_function, mod, *hook_lua, lua_callback_registry_index, lua_post_callback_registry_index, lua_thread_registry_index));
                pre_id = unreal_function->RegisterPreHook(&lua_unreal_script_function_hook_pre, custom_data.get());
                post_id = unreal_function->RegisterPostHook(&lua_unreal_script_function_hook_post, custom_data.get());
                custom_data->pre_callback_id = pre_id;
                custom_data->post_callback_id = post_id;
                Output::send<LogLevel::Verbose>(STR("[RegisterHook] Registered native hook ({}, {}) for {}\n"),
                                                custom_data->pre_callback_id,
                                                custom_data->post_callback_id,
                                                unreal_function->GetFullName());
            }
            else if (func_ptr && func_ptr == Unreal::UObject::ProcessInternalInternal.get_function_address() &&
                     !unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
            {
                auto function_data = find_function_hook_data(m_script_hook_callbacks, unreal_function);
                if (!function_data)
                {
                    function_data = &m_script_hook_callbacks.emplace_back(get_object_names(unreal_function), LuaCallbackData{hook_lua, nullptr, {}});
                }
                auto& callback_data = function_data->callback_data;
                // Note that non-native hooks don't have a different id for the post-callback.
                pre_id = Unreal::UnrealScriptFunctionData::MakeNewId();
                post_id = pre_id;
                callback_data.registry_indexes.emplace_back(hook_lua, LuaCallbackData::RegistryIndex{lua_callback_registry_index, pre_id});
                Output::send<LogLevel::Verbose>(STR("[RegisterHook] Registered script hook ({}, {}) for {}\n"),
                                                pre_id,
                                                post_id,
                                                unreal_function->GetFullName());
            }
            else
            {
                std::string error_message{"Was unable to register a hook with Lua function 'RegisterHook', information:\n"};
                error_message.append(fmt::format("FunctionName: {}\n", to_string(function_name_no_prefix)));
                error_message.append(fmt::format("UFunction::Func: {}\n", std::bit_cast<void*>(func_ptr)));
                error_message.append(fmt::format("ProcessInternal: {}\n", Unreal::UObject::ProcessInternalInternal.get_function_address()));
                error_message.append(
                        fmt::format("FUNC_Native: {}\n", static_cast<uint32_t>(unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))));
                refused_log::throws(lua, "RegisterHook", function_name_no_prefix,
                                    !func_ptr ? "ufunction-func-null"
                                              : (unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native) ? "native-func-is-processinternal"
                                                                                                                           : "bp-script-function-needs-processinternal-hook"));
                throw std::runtime_error(error_message);
            }

            lua.set_integer(pre_id);
            lua.set_integer(post_id);

            return 2;
        });


        // Register EGameThreadMethod enum table
        {
            lua_State* L = m_lua.get_lua_state();
            lua_newtable(L);
            lua_pushinteger(L, static_cast<int>(GameThreadExecutionMethod::EngineTick));
            lua_setfield(L, -2, "EngineTick");
            lua_pushinteger(L, static_cast<int>(GameThreadExecutionMethod::ProcessEvent));
            lua_setfield(L, -2, "ProcessEvent");
            // [R3] EGameThreadMethod.FuncPtrMarshal (DIFF 1; R3D 4.5)
            lua_pushinteger(L, static_cast<int>(kFuncPtrMarshalExecutor));
            lua_setfield(L, -2, "FuncPtrMarshal");
            lua_setglobal(L, "EGameThreadMethod");
        }

        // Register capability globals
        // These indicate whether certain hooks are available (scan succeeded)
        {
            lua_State* L = m_lua.get_lua_state();
            lua_pushboolean(L, UE4SSRuntime::IsEngineTickAvailable());
            lua_setglobal(L, "EngineTickAvailable");
            lua_pushboolean(L, UE4SSRuntime::IsProcessEventAvailable());
            lua_setglobal(L, "ProcessEventAvailable");
        }

        m_lua.register_function("ExecuteInGameThread", [](const LuaMadeSimple::Lua& lua) -> int {
        try
        {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThread'.
Overloads:
#1: ExecuteInGameThread(LuaFunction callback)
#2: ExecuteInGameThread(LuaFunction callback, EGameThreadMethod method)
    method: EGameThreadMethod.EngineTick or EGameThreadMethod.ProcessEvent)"};

            lua_State* L = lua.get_lua_state();
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
            int callback_idx = 1;

            if (lua_isfunction(L, 1) && lua_isinteger(L, 2))
            {
                // Overload #2: callback, method
                method = static_cast<GameThreadExecutionMethod>(lua_tointeger(L, 2));
            }
            else if (!lua_isfunction(L, 1))
            {
                L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "ExecuteInGameThread");
            }

            const auto mod = get_mod_ref(lua);

#if defined(__PROSPERO__)
            // [claude-cloud event-carrier] In the marshal world (pump_pending) the marshal
            // is the only pump: queue onto it even before a carrier arms, never onto the
            // EngineTick/ProcessEvent queues whose detours are refused (stranded closures).
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
#endif
            // Check hook availability before registering
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    // [R3] three-state ladder (DIFF 1; R3D 4.5):
                    // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw
                    if (is_process_event_hook_available())
                    {
                        // [R3-L11EL] method selection only; the lazy detour arm
                        // moved AFTER the enqueue (below), so a refused
                        // registration can never discard the closure.
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    else
                    {
                        L22Diagnostics::throw_lua_error(lua, "ExecuteInGameThread: EngineTick method requested but neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed", "ExecuteInGameThread");
                    }
#else
                    L22Diagnostics::throw_lua_error(lua, "ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)", "ExecuteInGameThread");
#endif
                }
                else
                {
#if defined(__PROSPERO__)
                    // [R3-L11EL] (worker-l11-enqueue-ladder, RUNBOOK 5.1 item 1):
                    // address-availability is NOT registrability (R3D 4.5 trap).
                    // In W_FAILS_R_WORKS the EngineTick address IS assigned
                    // (vtable fallback) and the lazy arm below used to run here,
                    // THROW before anything was enqueued, and bypass the armed
                    // FuncPtr marshal entirely (main.lua:695 default method).
                    // Marshal-first when armed: the marshal is the game-thread
                    // pump in this world; no second EngineTick pump may be
                    // attempted (the initializer deferred those registrations).
                    if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    // else: keep EngineTick; the lazy arm is attempted only
                    // AFTER the closure is enqueued (order guarantee below).
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    // [R3] three-state ladder (DIFF 1; R3D 4.5):
                    // ProcessEvent -> EngineTick -> FuncPtrMarshal -> throw
                    if (is_engine_tick_hook_available())
                    {
                        // [R3-L11EL] method selection only; the lazy detour arm
                        // moved AFTER the enqueue (below).
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    else
                    {
                        L22Diagnostics::throw_lua_error(lua, "ExecuteInGameThread: ProcessEvent method requested but neither ProcessEvent nor EngineTick hooks are available and the R3 FuncPtr-marshal carrier is not armed", "ExecuteInGameThread");
                    }
#else
                    L22Diagnostics::throw_lua_error(lua, "ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not available (AOB scan failed)", "ExecuteInGameThread");
#endif
                }
                else
                {
#if defined(__PROSPERO__)
                    // [R3-L11EL] mirror of the EngineTick branch: when the marshal
                    // is armed it outranks the address check (R3D 4.5), and the
                    // lazy arm runs only AFTER the closure is enqueued.
                    if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    // else: keep ProcessEvent; armed after the enqueue below.
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                }
            }
            else if (method == kFuncPtrMarshalExecutor)
            {
                // [R3] availability = carrier armed (module state), NOT an address
                // is_ready() (R3D 3 trap / 4.5). Enqueueing before arming is safe and
                // lazy (R3D 4.4a): the closure simply awaits the marshal drain.
                if (!is_funcptr_marshal_armed())
                {
                    Output::send<LogLevel::Warning>(STR("ExecuteInGameThread: FuncPtrMarshal requested but the carrier is not armed yet; the closure will await arming\n"));
                }
            }

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            // when async thread and game thread both access the same hook_lua state
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, callback_idx);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            SimpleLuaAction simpleAction{hook_lua, func_ref, lua_thread_registry_index};
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_engine_tick_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_engine_tick_actions.emplace_back(simpleAction);
                }
            }
            else
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_game_thread_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_game_thread_actions.emplace_back(simpleAction);
                }
            }

            L22Diagnostics::observe([&]() {
                Output::send(STR("[l22] ExecuteInGameThread queued method={} state={} ref={}\n"),
                             static_cast<int>(method), static_cast<void*>(hook_lua->get_lua_state()), func_ref);
            });
            // [l25] throttled queue-depth liveness (Claude l25 item 6): one depth
            // line + diagnostic event per second max; m_thread_actions_mutex is
            // already held here, so the size reads are race-free. Counts every
            // undrained ExecuteInGameThread closure across both pumps (active +
            // pending, simple + delayed). Steady non-zero depth with no matching
            // "[l22] ExecuteInGameThread drained" lines means closures are queued
            // but no pump ever drains them - the exact stall signature the phase
            // tags exist to expose.
            {
                static std::atomic<int64_t> s_eigt_depth_log_ms{0};
                const auto depth_now = std::chrono::steady_clock::now().time_since_epoch();
                const int64_t depth_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(depth_now).count();
                int64_t last_depth_log_ms = s_eigt_depth_log_ms.load(std::memory_order_relaxed);
                if (depth_now_ms - last_depth_log_ms >= 1000 &&
                    s_eigt_depth_log_ms.compare_exchange_strong(last_depth_log_ms, depth_now_ms, std::memory_order_relaxed))
                {
                    const auto queue_depth = LuaMod::m_game_thread_actions.size() + LuaMod::m_pending_game_thread_actions.size() +
                                             LuaMod::m_delayed_game_thread_actions.size() + LuaMod::m_pending_delayed_game_thread_actions.size() +
                                             LuaMod::m_engine_tick_actions.size() + LuaMod::m_pending_engine_tick_actions.size();
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] ExecuteInGameThread queued depth={}\n"), queue_depth);
                    });
                    diagnostic_event("eigt_queue_depth", static_cast<unsigned int>(queue_depth));

                    // [IS-LUA-ALLOC] Claude round-13 item 1: 1/s allocator
                    // observability, riding the existing queue-depth throttle.
                    // gc_kb = CALLING state's Lua heap (lua_gc LUA_GCCOUNT, KiB);
                    // errs/cxx = Lua errors raised / C++ exceptions converted;
                    // live/allocs/fails = the shared counting allocator across
                    // ALL Lua states. Rising fails with flat gc_kb == backing
                    // store refusal (game libc heap pressure), not Lua garbage.
                    L22Diagnostics::observe([&]() {
                        // [B2] read-only: LUA_GCCOUNT only. IS's forced LUA_GCSTEP was
                        // dropped (a diagnostic must not change GC timing).
                        const int gc_kb = lua_gc(lua.get_lua_state(), LUA_GCCOUNT);
                        const auto snap = LuaMadeSimple::query_lua_memory(lua.get_lua_state());
                        // [R9-MEM] be/be_states/libc_states/be_kb/be_fails appended (older parsers keep working).
                        Output::send(STR("[MEM] state={} gc_kb={} live_kb={} blocks={} allocs={} fails={} fail_o={} fail_n={} arena_cap_kb={} arena_used_kb={} arena_ext_kb={} errs={} cxx={} be={} be_states={} libc_states={} be_kb={} be_fails={}\n"),
                                     static_cast<void*>(lua.get_lua_state()),
                                     gc_kb,
                                     snap.live_bytes / 1024,
                                     snap.live_blocks,
                                     snap.total_alloc_calls,
                                     snap.failed_calls,
                                     snap.last_fail_osize,
                                     snap.last_fail_nsize,
                                     snap.arena_capacity_bytes / 1024,
                                     snap.arena_used_bytes / 1024,
                                     snap.arena_external_bytes / 1024,
                                     snap.raised_errors,
                                     snap.cxx_exceptions,
                                     snap.be_installed != 0 ? STR("gmalloc") : STR("libc"),
                                     snap.be_states,
                                     snap.libc_states,
                                     snap.be_live_bytes / 1024,
                                     snap.be_failed_calls);
                        diagnostic_event("lua_mem_gc_kb", static_cast<unsigned int>(gc_kb < 0 ? 0 : gc_kb));
                        diagnostic_event("lua_mem_fails", static_cast<unsigned int>(snap.failed_calls));
                    });
                }
            }

#if defined(__PROSPERO__)
            // [R3-L11EL] THE ORDER GUARANTEE (worker-l11-enqueue-ladder; RUNBOOK
            // 5.1 item 1): the lazy detour arm runs only AFTER the closure has
            // been enqueued, so a refused registration can never discard enqueued
            // work. The queue survives; drain happens via whatever pump exists.
            // Marshal-selected paths arm nothing here: the marshal is armed by the
            // calibration consumer and its pump drains the game-thread queue; a
            // second EngineTick/ProcessEvent pump is exactly what DIFF-2's
            // deferred initializer registrations must not become. The
            // recursive_mutex is already held by this thread (recursive re-entry,
            // same as the RegisterHook path); the pump queues are guarded by it.
            if (method == GameThreadExecutionMethod::EngineTick || method == GameThreadExecutionMethod::ProcessEvent)
            {
                TRY([&]() {
                    if (method == GameThreadExecutionMethod::EngineTick)
                    {
                        LuaMod::ensure_engine_tick_hooked();
                    }
                    else
                    {
                        LuaMod::ensure_process_event_hooked(mod);
                    }
                });
                // Loud stall honesty: a refused arm leaves the closure queued with
                // no drainer. The closure is NOT discarded (enqueue-first); say so
                // every time so "queued forever" is never silent.
                if (method == GameThreadExecutionMethod::EngineTick && !LuaMod::m_is_engine_tick_hooked)
                {
                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the EngineTick pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
                }
                else if (method == GameThreadExecutionMethod::ProcessEvent && !mod->m_is_process_event_hooked)
                {
                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the ProcessEvent pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
                }
            }
#endif

            return 0;
        
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("ExecuteInGameThread", error.what());
            throw;
        }
        catch (void*)
        {
            // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
            // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
            throw;
        }
        catch (...)
        {
            L22Diagnostics::error("ExecuteInGameThread", "non-standard C++ exception");
            throw;
        }
    });

        // ExecuteInGameThreadWithDelay - executes callback after a time delay
        // Uses default method from config, falls back to the other if unavailable
        m_lua.register_function("ExecuteInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThreadWithDelay'.
Overloads:
#1: ExecuteInGameThreadWithDelay(integer delayMs, LuaFunction callback) -> integer handle
#2: ExecuteInGameThreadWithDelay(integer handle, integer delayMs, LuaFunction callback) -> nil (only creates if handle doesn't exist))"};

            lua_State* L = lua.get_lua_state();

            // Determine which overload based on argument count
            int num_args = lua_gettop(L);
            bool has_handle = (num_args >= 3 && lua_isinteger(L, 1) && lua_isinteger(L, 2) && lua_isfunction(L, 3));
            bool no_handle = (num_args >= 2 && lua_isinteger(L, 1) && lua_isfunction(L, 2));

            if (!has_handle && !no_handle)
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

            // Use default method from config, fall back to the other if unavailable
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 item 1 remainder; distinct-ID revision
            // worker-r3-ladder 2026-10-05): marshal-first when the marshal is
            // the USABLE game-thread pump. Address-availability is NOT pump
            // ownership: in the walk-permitted world the initializer's vtable
            // fallback assigns TickInternal, so the availability-first order
            // below reached ensure_engine_tick_hooked() BEFORE the enqueue, its
            // refused detour registration threw "EngineTick callback
            // registration failed", and the closure was lost with the armed
            // marshal bypassed (the exact 5.1 trap). The usable marshal takes
            // the method FIRST - deterministically, EngineTick and ProcessEvent
            // defaults alike - and an armed-but-dead drain (pump_pending=false,
            // the rev-B pairing contradiction) refuses loudly instead of
            // falling back silently.
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
            else if (is_funcptr_marshal_armed())
            {
                lua.throw_error("ExecuteInGameThreadWithDelay: R3 FuncPtr-marshal carrier is armed but its drain is dead (pump_pending=false); refusing to queue onto a dead pump and refusing to arm a second EngineTick pump");
            }
#endif
            if (method == GameThreadExecutionMethod::EngineTick)
            {
#if defined(__PROSPERO__)
                if (is_engine_tick_hook_available())
                    LuaMod::ensure_engine_tick_hooked();
#else
                LuaMod::ensure_engine_tick_hooked();
#endif
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_process_event_hook_available())
                        LuaMod::ensure_process_event_hooked(mod);
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                    if (!is_process_event_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::ProcessEvent;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
#if defined(__PROSPERO__)
                if (is_process_event_hook_available())
                    LuaMod::ensure_process_event_hooked(mod);
#else
                LuaMod::ensure_process_event_hooked(mod);
#endif
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_engine_tick_hook_available())
                        LuaMod::ensure_engine_tick_hooked();
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
                    if (!is_engine_tick_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            if (has_handle)
            {
                // Overload #2: ExecuteInGameThreadWithDelay(handle, delayMs, callback)
                // Like UE's Delay - only creates if handle doesn't already exist
                auto handle = lua_tointeger(L, 1);
                auto delay_ms = lua_tointeger(L, 2);

                // Acquire mutex before ANY Lua state operations to prevent race conditions
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                // Check if handle already exists (check both main and pending queues)
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Handle exists, do nothing (like UE's Delay node)
                        return 0;
                    }
                }
                for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Handle exists in pending queue, do nothing
                        return 0;
                    }
                }

                // Handle doesn't exist, create new action
                auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

                lua_pushvalue(L, 3);
                lua_xmove(L, hook_lua->get_lua_state(), 1);
                const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

                DelayedGameThreadAction action{};
                action.lua = hook_lua;
                action.lua_action_function_ref = func_ref;
                action.lua_action_thread_ref = lua_thread_registry_index;
                action.method = method;
                action.delay_ms = delay_ms;
                action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                action.handle = handle;

                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
                }
                else
                {
                    LuaMod::m_delayed_game_thread_actions.emplace_back(action);
                }

                return 0;
            }
            else
            {
                // Overload #1: ExecuteInGameThreadWithDelay(delayMs, callback) -> handle
                auto delay_ms = lua_tointeger(L, 1);

                // Acquire mutex before ANY Lua state operations to prevent race conditions
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

                lua_pushvalue(L, 2);
                lua_xmove(L, hook_lua->get_lua_state(), 1);
                const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

                DelayedGameThreadAction action{};
                action.lua = hook_lua;
                action.lua_action_function_ref = func_ref;
                action.lua_action_thread_ref = lua_thread_registry_index;
                action.method = method;
                action.delay_ms = delay_ms;
                action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                action.handle = LuaMod::m_next_delayed_action_handle++;

                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
                }
                else
                {
                    LuaMod::m_delayed_game_thread_actions.emplace_back(action);
                }

                lua.set_integer(action.handle);
                return 1;
            }
        });

        // RetriggerableExecuteInGameThreadWithDelay - executes callback after a time delay, resets timer if called again with same handle
        // Uses default method from config, falls back to the other if unavailable
        m_lua.register_function("RetriggerableExecuteInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RetriggerableExecuteInGameThreadWithDelay'.
Overloads:
#1: RetriggerableExecuteInGameThreadWithDelay(integer handle, integer delayMs, LuaFunction callback))"};

            lua_State* L = lua.get_lua_state();
            if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || !lua_isfunction(L, 3))
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

            // Use default method from config, fall back to the other if unavailable
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 item 1 remainder; distinct-ID revision
            // worker-r3-ladder 2026-10-05): marshal-first when the marshal is
            // the USABLE game-thread pump. Address-availability is NOT pump
            // ownership: in the walk-permitted world the initializer's vtable
            // fallback assigns TickInternal, so the availability-first order
            // below reached ensure_engine_tick_hooked() BEFORE the enqueue, its
            // refused detour registration threw "EngineTick callback
            // registration failed", and the closure was lost with the armed
            // marshal bypassed (the exact 5.1 trap). The usable marshal takes
            // the method FIRST - deterministically, EngineTick and ProcessEvent
            // defaults alike - and an armed-but-dead drain (pump_pending=false,
            // the rev-B pairing contradiction) refuses loudly instead of
            // falling back silently.
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
            else if (is_funcptr_marshal_armed())
            {
                lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: R3 FuncPtr-marshal carrier is armed but its drain is dead (pump_pending=false); refusing to queue onto a dead pump and refusing to arm a second EngineTick pump");
            }
#endif
            if (method == GameThreadExecutionMethod::EngineTick)
            {
#if defined(__PROSPERO__)
                if (is_engine_tick_hook_available())
                    LuaMod::ensure_engine_tick_hooked();
#else
                LuaMod::ensure_engine_tick_hooked();
#endif
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_process_event_hook_available())
                        LuaMod::ensure_process_event_hooked(mod);
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                    if (!is_process_event_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::ProcessEvent;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
#if defined(__PROSPERO__)
                if (is_process_event_hook_available())
                    LuaMod::ensure_process_event_hooked(mod);
#else
                LuaMod::ensure_process_event_hooked(mod);
#endif
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_engine_tick_hook_available())
                        LuaMod::ensure_engine_tick_hooked();
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
                    if (!is_engine_tick_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            auto handle = lua_tointeger(L, 1);
            auto delay_ms = lua_tointeger(L, 2);

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            // Check if an action with this handle already exists (check both main and pending queues)
            for (auto& action : LuaMod::m_delayed_game_thread_actions)
            {
                if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    // Reset the timer for the existing action
                    action.delay_ms = delay_ms;
                    action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                    action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                    lua.set_integer(handle);
                    return 1;
                }
            }
            for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
            {
                if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    // Reset the timer for the existing action in pending queue
                    action.delay_ms = delay_ms;
                    action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                    action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                    lua.set_integer(handle);
                    return 1;
                }
            }

            // No existing action, create a new one
            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, 3);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.method = method;
            action.delay_ms = delay_ms;
            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
            action.is_retriggerable = true;
            action.handle = handle;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }

            return 0;
        });

        // ExecuteInGameThreadAfterFrames - executes callback after a frame delay
        // Requires EngineTick hook - cannot fall back to ProcessEvent since frames cannot be counted there
        m_lua.register_function("ExecuteInGameThreadAfterFrames", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThreadAfterFrames'.
Overloads:
#1: ExecuteInGameThreadAfterFrames(integer frames, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            // Frame-based delays require EngineTick - cannot use ProcessEvent
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 family closure; worker-r3-ladder
            // 2026-10-05): with the marshal the usable pump, no EngineTick
            // detour may be attempted here - the old path fell through to
            // ensure_engine_tick_hooked(), whose refused registration threw and
            // discarded the closure while the marshal sat armed. The marshal
            // drain deliberately skips frame-based actions
            // (process_delayed_actions: frame delays remain EngineTick-only),
            // so a frame-based request is refused LOUDLY and precisely instead
            // of being silently stalled on a pump that can never count it.
            if (funcptr_marshal_usable())
            {
                refused_log::throws(lua, "ExecuteInGameThreadAfterFrames", StringType{STR("-")}, "frame-delays-need-enginetick-pump");
                lua.throw_error("ExecuteInGameThreadAfterFrames: frame-based delays need an EngineTick pump; only the R3 FuncPtr-marshal pump exists in this world and its drain cannot count frames");
            }
#endif
            // Frame-based delays require EngineTick - cannot use ProcessEvent
            if (!is_engine_tick_hook_available())
            {
                refused_log::throws(lua, "ExecuteInGameThreadAfterFrames", StringType{STR("-")}, "enginetick-hook-unavailable");
                lua.throw_error("ExecuteInGameThreadAfterFrames: EngineTick hook is not available (AOB scan failed). Frame-based delays require EngineTick.");
            }

#if defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif
            auto frames = lua.get_integer();
            auto mod = get_mod_ref(lua);

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, 1);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.delay_frames = frames;
            action.frames_remaining = frames;
            action.handle = LuaMod::m_next_delayed_action_handle++;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }
#if !defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif

            lua.set_integer(action.handle);
            return 1;
        });

        // LoopInGameThreadWithDelay - executes callback repeatedly with a time delay
        // Uses default method from config, falls back to the other if unavailable
        m_lua.register_function("LoopInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoopInGameThreadWithDelay'.
Overloads:
#1: LoopInGameThreadWithDelay(integer delayMs, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

            // Use default method from config, fall back to the other if unavailable
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 item 1 remainder; distinct-ID revision
            // worker-r3-ladder 2026-10-05): marshal-first when the marshal is
            // the USABLE game-thread pump. Address-availability is NOT pump
            // ownership: in the walk-permitted world the initializer's vtable
            // fallback assigns TickInternal, so the availability-first order
            // below reached ensure_engine_tick_hooked() BEFORE the enqueue, its
            // refused detour registration threw "EngineTick callback
            // registration failed", and the closure was lost with the armed
            // marshal bypassed (the exact 5.1 trap). The usable marshal takes
            // the method FIRST - deterministically, EngineTick and ProcessEvent
            // defaults alike - and an armed-but-dead drain (pump_pending=false,
            // the rev-B pairing contradiction) refuses loudly instead of
            // falling back silently.
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
            else if (is_funcptr_marshal_armed())
            {
                lua.throw_error("LoopInGameThreadWithDelay: R3 FuncPtr-marshal carrier is armed but its drain is dead (pump_pending=false); refusing to queue onto a dead pump and refusing to arm a second EngineTick pump");
            }
#endif
            if (method == GameThreadExecutionMethod::EngineTick)
            {
#if defined(__PROSPERO__)
                if (is_engine_tick_hook_available())
                    LuaMod::ensure_engine_tick_hooked();
#else
                LuaMod::ensure_engine_tick_hooked();
#endif
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_process_event_hook_available())
                        LuaMod::ensure_process_event_hooked(mod);
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                    if (!is_process_event_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::ProcessEvent;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
#if defined(__PROSPERO__)
                if (is_process_event_hook_available())
                    LuaMod::ensure_process_event_hooked(mod);
#else
                LuaMod::ensure_process_event_hooked(mod);
#endif
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_engine_tick_hook_available())
                        LuaMod::ensure_engine_tick_hooked();
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
                    if (!is_engine_tick_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            auto delay_ms = lua.get_integer();

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, 1);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.method = method;
            action.delay_ms = delay_ms;
            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
            action.is_looping = true;
            action.handle = LuaMod::m_next_delayed_action_handle++;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }

            lua.set_integer(action.handle);
            return 1;
        });

        // LoopInGameThreadAfterFrames - executes callback repeatedly with a frame delay
        // Requires EngineTick hook - cannot fall back to ProcessEvent since frames cannot be counted there
        m_lua.register_function("LoopInGameThreadAfterFrames", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoopInGameThreadAfterFrames'.
Overloads:
#1: LoopInGameThreadAfterFrames(integer frames, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            // Frame-based delays require EngineTick - cannot use ProcessEvent
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 family closure; worker-r3-ladder
            // 2026-10-05): with the marshal the usable pump, no EngineTick
            // detour may be attempted here - the old path fell through to
            // ensure_engine_tick_hooked(), whose refused registration threw and
            // discarded the closure while the marshal sat armed. The marshal
            // drain deliberately skips frame-based actions
            // (process_delayed_actions: frame delays remain EngineTick-only),
            // so a frame-based request is refused LOUDLY and precisely instead
            // of being silently stalled on a pump that can never count it.
            if (funcptr_marshal_usable())
            {
                refused_log::throws(lua, "LoopInGameThreadAfterFrames", StringType{STR("-")}, "frame-delays-need-enginetick-pump");
                lua.throw_error("LoopInGameThreadAfterFrames: frame-based delays need an EngineTick pump; only the R3 FuncPtr-marshal pump exists in this world and its drain cannot count frames");
            }
#endif
            // Frame-based delays require EngineTick - cannot use ProcessEvent
            if (!is_engine_tick_hook_available())
            {
                refused_log::throws(lua, "LoopInGameThreadAfterFrames", StringType{STR("-")}, "enginetick-hook-unavailable");
                lua.throw_error("LoopInGameThreadAfterFrames: EngineTick hook is not available (AOB scan failed). Frame-based delays require EngineTick.");
            }

#if defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif
            auto frames = lua.get_integer();
            auto mod = get_mod_ref(lua);

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            // After get_integer() pops the first arg, the function is now at index 1
            lua_pushvalue(L, 1);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.delay_frames = frames;
            action.frames_remaining = frames;
            action.is_looping = true;
            action.handle = LuaMod::m_next_delayed_action_handle++;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }
#if !defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif

            lua.set_integer(action.handle);
            return 1;
        });

        // ResetDelayedActionTimer - resets the timer for any delayed action using the original delay (only if owned by calling mod)
        m_lua.register_function("ResetDelayedActionTimer", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ResetDelayedActionTimer'.
Overloads:
#1: ResetDelayedActionTimer(integer handle) -> boolean success)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            bool found = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Only allow resetting actions owned by the calling mod
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Reset the timer based on whether it's time-based or frame-based
                        if (action.delay_frames > 0)
                        {
                            action.frames_remaining = action.delay_frames;
                        }
                        else
                        {
                            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(action.delay_ms);
                        }
                        action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            if (action.delay_frames > 0)
                            {
                                action.frames_remaining = action.delay_frames;
                            }
                            else
                            {
                                action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(action.delay_ms);
                            }
                            action.status = LuaMod::DelayedActionStatus::Active;
                            found = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(found);
            return 1;
        });

        // SetDelayedActionTimer - sets a new delay for a delayed action and restarts the timer (only if owned by calling mod)
        m_lua.register_function("SetDelayedActionTimer", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'SetDelayedActionTimer'.
Overloads:
#1: SetDelayedActionTimer(integer handle, integer newDelay) -> boolean success)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isinteger(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            auto new_delay = lua.get_integer();
            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            bool found = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Only allow modifying actions owned by the calling mod
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Set new delay and reset the timer
                        if (action.delay_frames > 0)
                        {
                            action.delay_frames = new_delay;
                            action.frames_remaining = new_delay;
                        }
                        else
                        {
                            action.delay_ms = new_delay;
                            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(new_delay);
                        }
                        action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            if (action.delay_frames > 0)
                            {
                                action.delay_frames = new_delay;
                                action.frames_remaining = new_delay;
                            }
                            else
                            {
                                action.delay_ms = new_delay;
                                action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(new_delay);
                            }
                            action.status = LuaMod::DelayedActionStatus::Active;
                            found = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(found);
            return 1;
        });

        // PauseDelayedAction - pauses a delayed action timer (only if owned by calling mod)
        m_lua.register_function("PauseDelayedAction", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'PauseDelayedAction'.
Overloads:
#1: PauseDelayedAction(integer handle) -> boolean success)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            bool found = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Only allow pausing actions owned by the calling mod
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Active)
                    {
                        // Store remaining time before pausing
                        auto now = std::chrono::steady_clock::now();
                        if (action.execute_at > now)
                        {
                            action.time_remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                        }
                        else
                        {
                            action.time_remaining_ms = 0;
                        }
                        action.status = LuaMod::DelayedActionStatus::Paused;
                        found = true;
                        break;
                    }
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Executing && action.is_looping)
                    {
                        // Defer pause until callback unwinds to avoid re-entrancy issues.
                        action.pause_after_execution = true;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Active)
                        {
                            auto now = std::chrono::steady_clock::now();
                            if (action.execute_at > now)
                            {
                                action.time_remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                            }
                            else
                            {
                                action.time_remaining_ms = 0;
                            }
                            action.status = LuaMod::DelayedActionStatus::Paused;
                            found = true;
                            break;
                        }
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Executing && action.is_looping)
                        {
                            action.pause_after_execution = true;
                            found = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(found);
            return 1;
        });

        // UnpauseDelayedAction - resumes a paused delayed action timer (only if owned by calling mod)
        m_lua.register_function("UnpauseDelayedAction", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnpauseDelayedAction'.
Overloads:
#1: UnpauseDelayedAction(integer handle) -> boolean success)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            bool found = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Only allow unpausing actions owned by the calling mod
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Paused)
                    {
                        // Restore execute_at from remaining time
                        action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(action.time_remaining_ms);
                        action.status = LuaMod::DelayedActionStatus::Active;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status == LuaMod::DelayedActionStatus::Paused)
                        {
                            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(action.time_remaining_ms);
                            action.status = LuaMod::DelayedActionStatus::Active;
                            found = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(found);
            return 1;
        });

        // CancelDelayedAction - cancels a delayed action (only if owned by calling mod)
        m_lua.register_function("CancelDelayedAction", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'CancelDelayedAction'.
Overloads:
#1: CancelDelayedAction(integer handle) -> boolean success)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            bool found = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Only allow cancelling actions owned by the calling mod
                    if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                            found = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(found);
            return 1;
        });

        // IsValidDelayedActionHandle - checks if a handle refers to an existing, non-cancelled action
        m_lua.register_function("IsValidDelayedActionHandle", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsValidDelayedActionHandle'.
Overloads:
#1: IsValidDelayedActionHandle(integer handle) -> boolean valid)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            bool valid = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        valid = true;
                        break;
                    }
                }
                if (!valid)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            valid = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(valid);
            return 1;
        });

        // IsDelayedActionActive - checks if a delayed action is active (not paused or cancelled)
        m_lua.register_function("IsDelayedActionActive", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsDelayedActionActive'.
Overloads:
#1: IsDelayedActionActive(integer handle) -> boolean active)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            bool active = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status == LuaMod::DelayedActionStatus::Active)
                    {
                        active = true;
                        break;
                    }
                }
                if (!active)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status == LuaMod::DelayedActionStatus::Active)
                        {
                            active = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(active);
            return 1;
        });

        // IsDelayedActionPaused - checks if a delayed action is paused
        m_lua.register_function("IsDelayedActionPaused", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'IsDelayedActionPaused'.
Overloads:
#1: IsDelayedActionPaused(integer handle) -> boolean paused)"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            bool paused = false;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status == LuaMod::DelayedActionStatus::Paused)
                    {
                        paused = true;
                        break;
                    }
                }
                if (!paused)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status == LuaMod::DelayedActionStatus::Paused)
                        {
                            paused = true;
                            break;
                        }
                    }
                }
            }

            lua.set_bool(paused);
            return 1;
        });

        // GetDelayedActionTimeRemaining - returns remaining time in milliseconds (or frames for frame-based)
        m_lua.register_function("GetDelayedActionTimeRemaining", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetDelayedActionTimeRemaining'.
Overloads:
#1: GetDelayedActionTimeRemaining(integer handle) -> integer remainingMs (or -1 if not found))"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            int64_t remaining = -1;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        if (action.delay_frames > 0)
                        {
                            // Frame-based: return frames remaining
                            remaining = action.frames_remaining;
                        }
                        else if (action.status == LuaMod::DelayedActionStatus::Paused)
                        {
                            // Paused: return stored remaining time
                            remaining = action.time_remaining_ms;
                        }
                        else
                        {
                            // Active: calculate remaining time
                            auto now = std::chrono::steady_clock::now();
                            if (action.execute_at > now)
                            {
                                remaining = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                            }
                            else
                            {
                                remaining = 0;
                            }
                        }
                        break;
                    }
                }
                if (remaining == -1)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            if (action.delay_frames > 0)
                            {
                                remaining = action.frames_remaining;
                            }
                            else if (action.status == LuaMod::DelayedActionStatus::Paused)
                            {
                                remaining = action.time_remaining_ms;
                            }
                            else
                            {
                                auto now = std::chrono::steady_clock::now();
                                if (action.execute_at > now)
                                {
                                    remaining = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                                }
                                else
                                {
                                    remaining = 0;
                                }
                            }
                            break;
                        }
                    }
                }
            }

            lua.set_integer(remaining);
            return 1;
        });

        // GetDelayedActionTimeElapsed - returns elapsed time in milliseconds (or frames for frame-based)
        m_lua.register_function("GetDelayedActionTimeElapsed", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetDelayedActionTimeElapsed'.
Overloads:
#1: GetDelayedActionTimeElapsed(integer handle) -> integer elapsedMs (or -1 if not found))"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            int64_t elapsed = -1;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        if (action.delay_frames > 0)
                        {
                            // Frame-based: return frames elapsed
                            elapsed = action.delay_frames - action.frames_remaining;
                        }
                        else if (action.status == LuaMod::DelayedActionStatus::Paused)
                        {
                            // Paused: calculate from stored remaining time
                            elapsed = action.delay_ms - action.time_remaining_ms;
                        }
                        else
                        {
                            // Active: calculate elapsed time
                            auto now = std::chrono::steady_clock::now();
                            if (action.execute_at > now)
                            {
                                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                                elapsed = action.delay_ms - remaining;
                            }
                            else
                            {
                                elapsed = action.delay_ms;
                            }
                        }
                        break;
                    }
                }
                if (elapsed == -1)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            if (action.delay_frames > 0)
                            {
                                elapsed = action.delay_frames - action.frames_remaining;
                            }
                            else if (action.status == LuaMod::DelayedActionStatus::Paused)
                            {
                                elapsed = action.delay_ms - action.time_remaining_ms;
                            }
                            else
                            {
                                auto now = std::chrono::steady_clock::now();
                                if (action.execute_at > now)
                                {
                                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(action.execute_at - now).count();
                                    elapsed = action.delay_ms - remaining;
                                }
                                else
                                {
                                    elapsed = action.delay_ms;
                                }
                            }
                            break;
                        }
                    }
                }
            }

            lua.set_integer(elapsed);
            return 1;
        });

        // GetDelayedActionRate - returns the configured delay rate (not remaining time)
        m_lua.register_function("GetDelayedActionRate", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'GetDelayedActionRate'.
Overloads:
#1: GetDelayedActionRate(integer handle) -> integer rateMs (or -1 if not found))"};

            if (!lua.is_integer())
            {
                lua.throw_error(error_overload_not_found);
            }

            auto handle = lua.get_integer();
            int64_t rate = -1;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        if (action.delay_frames > 0)
                        {
                            // Frame-based: return frames
                            rate = action.delay_frames;
                        }
                        else
                        {
                            // Time-based: return ms
                            rate = action.delay_ms;
                        }
                        break;
                    }
                }
                if (rate == -1)
                {
                    for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                    {
                        if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                        {
                            if (action.delay_frames > 0)
                            {
                                rate = action.delay_frames;
                            }
                            else
                            {
                                rate = action.delay_ms;
                            }
                            break;
                        }
                    }
                }
            }

            lua.set_integer(rate);
            return 1;
        });

        // ClearAllDelayedActions - cancels all delayed actions for the current mod
        m_lua.register_function("ClearAllDelayedActions", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ClearAllDelayedActions'.
Overloads:
#1: ClearAllDelayedActions() -> integer count)"};

            const auto mod = get_mod_ref(lua);
            const LuaMadeSimple::Lua* mod_hook_lua = mod->m_hook_lua;
            int64_t count = 0;

            {
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                for (auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    // Check if this action belongs to the current mod by comparing hook lua states
                    if (action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Mark for removal
                        action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                        count++;
                    }
                }
                for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                {
                    if (action.lua == mod_hook_lua && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                        count++;
                    }
                }
            }

            lua.set_integer(count);
            return 1;
        });

        // MakeHandle - returns a unique action handle
        m_lua.register_function("MakeActionHandle", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'MakeActionHandle'.
Overloads:
#1: MakeActionHandle() -> integer Handle)"};

            lua.set_integer(m_next_delayed_action_handle++);

            return 1;
        });

        m_lua.register_function("RestartCurrentMod", [](const LuaMadeSimple::Lua& lua) -> int {
            auto mod = get_mod_ref(lua);
            if (!mod)
            {
                lua.throw_error("RestartCurrentMod: Could not get mod reference");
            }

            // Use mod ID for safe cross-thread reference
            ModId mod_id = mod->get_id();
            UE4SSProgram::get_program().queue_reinstall_mod(mod_id);

            return 0;
        });

        m_lua.register_function("UninstallCurrentMod", [](const LuaMadeSimple::Lua& lua) -> int {
            auto mod = get_mod_ref(lua);
            if (!mod)
            {
                lua.throw_error("UninstallCurrentMod: Could not get mod reference");
            }

            // Use mod ID for safe cross-thread reference
            ModId mod_id = mod->get_id();
            UE4SSProgram::get_program().queue_uninstall_mod(mod_id);

            return 0;
        });

        // P1: string mod_name - Name of the mod to restart
        m_lua.register_function("RestartMod", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RestartMod'.
Overloads:
#1: RestartMod(string mod_name))"};

            if (!lua.is_string())
            {
                lua.throw_error(error_overload_not_found);
            }
                        
            UE4SSProgram::get_program().queue_reinstall_mod_by_name(lua.get_string());

            return 0;
        });

        // P1: string mod_name - Name of the mod to uninstall
        m_lua.register_function("UninstallMod", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UninstallMod'.
Overloads:
#1: UninstallMod(string mod_name))"};

            if (!lua.is_string())
            {
                lua.throw_error(error_overload_not_found);
            }
            
            UE4SSProgram::get_program().queue_uninstall_mod_by_name(lua.get_string());

            return 0;
        });
    }

    auto static is_unreal_version_out_of_bounds_from_64bit(int64_t major_version, int64_t minor_version) -> bool
    {
        if (major_version < std::numeric_limits<uint32_t>::min() || major_version > std::numeric_limits<uint32_t>::max() ||
            minor_version < std::numeric_limits<uint32_t>::min() || minor_version > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }
        else
        {
            return true;
        }
    };

    using UnrealVersionCheckFunctionPtr = bool (*)(int32_t, int32_t);
    auto static unreal_version_check(const LuaMadeSimple::Lua& lua, UnrealVersionCheckFunctionPtr check_function, const std::string& error_overload_not_found) -> void
    {
        // Removing the table from the stack as we don't need it
        // This is required in order to align the parameters (or manually provide the stack index for the params)
        if (lua.is_table())
        {
            lua.discard_value();
        }

        // Checking the first and second param, without retrieving either
        // Makes for less code
        if (!lua.is_integer() || !lua.is_integer(2))
        {
            lua.throw_error(error_overload_not_found);
        }

        int64_t major_version = lua.get_integer();
        int64_t minor_version = lua.get_integer();

        if (!is_unreal_version_out_of_bounds_from_64bit(major_version, minor_version))
        {
            lua.throw_error("[UnrealVersion::unreal_version_check] Major/minor version numbers must be within the range of uint32");
        }

        lua.set_bool(check_function(static_cast<int32_t>(major_version), static_cast<int32_t>(minor_version)));
    }

    auto static setup_lua_classes_internal(const LuaMadeSimple::Lua& lua) -> void
    {
        // UE4SS Class -> START
        auto mod_class = lua.prepare_new_table();
        mod_class.set_has_userdata(false);

        mod_class.add_pair("GetVersion", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(UE4SS_LIB_VERSION_MAJOR);
            lua.set_integer(UE4SS_LIB_VERSION_MINOR);
            lua.set_integer(UE4SS_LIB_VERSION_HOTFIX);
            return 3;
        });
        mod_class.make_global("UE4SS");
        // UE4SS Class -> END

        // UnrealVersion Class -> START
        auto unreal_version_class = lua.prepare_new_table();
        unreal_version_class.set_has_userdata(false);

        unreal_version_class.add_pair("GetMajor", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(Unreal::Version::Major);
            return 1;
        });

        unreal_version_class.add_pair("GetMinor", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(Unreal::Version::Minor);
            return 1;
        });

        unreal_version_class.add_pair("IsEqual", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnrealVersion:IsEqual'.
Overloads:
#1: IsEqual(number MajorVersion, number MinorVersion))"};

            unreal_version_check(lua, &Unreal::Version::IsEqual, error_overload_not_found);

            return 1;
        });

        unreal_version_class.add_pair("IsAtLeast", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnrealVersion:IsAtLeast'.
Overloads:
#1: IsAtLeast(number MajorVersion, number MinorVersion))"};

            unreal_version_check(lua, &Unreal::Version::IsAtLeast, error_overload_not_found);

            return 1;
        });

        unreal_version_class.add_pair("IsAtMost", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnrealVersion:IsAtMost'.
Overloads:
#1: IsAtMost(number MajorVersion, number MinorVersion))"};

            unreal_version_check(lua, &Unreal::Version::IsAtMost, error_overload_not_found);

            return 1;
        });

        unreal_version_class.add_pair("IsBelow", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnrealVersion:IsBelow'.
Overloads:
#1: IsBelow(number MajorVersion, number MinorVersion))"};

            unreal_version_check(lua, &Unreal::Version::IsBelow, error_overload_not_found);

            return 1;
        });

        unreal_version_class.add_pair("IsAbove", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'UnrealVersion:IsAbove'.
Overloads:
#1: IsAbove(number MajorVersion, number MinorVersion))"};

            unreal_version_check(lua, &Unreal::Version::IsAbove, error_overload_not_found);

            return 1;
        });
        unreal_version_class.make_global("UnrealVersion");
        // UnrealVersion Class -> END

        // FName Class -> START
        // Pre-load the global FName table
        // Without this, the metatable won't be created until an FName is constructed by another part of UE4SS
        LuaType::FName::construct(lua, Unreal::FName(static_cast<int64_t>(0)));
        lua_setglobal(lua.get_lua_state(), "FName");
        LuaType::FName::construct(lua, Unreal::NAME_None);
        lua_setglobal(lua.get_lua_state(), "NAME_None");
        // FName Class -> END

        // FText Class -> START
        // Pre-load the global FText table
        // Without this, the metatable won't be created until an FText is constructed by another part of UE4SS
        LuaType::FText::construct(lua, Unreal::FText());
        lua_setglobal(lua.get_lua_state(), "FText");
        // FText Class -> END

        // FString Class -> START
        // Pre-load the global FString constructor
        lua.register_function("FString",
                              [](const LuaMadeSimple::Lua& lua) -> int {
                                  if (lua.get_stack_size() < 1 || !lua.is_string())
                                  {
                                      lua.throw_error("FString constructor requires a string argument");
                                  }
                                  std::string_view str = lua.get_string();
#ifdef __PROSPERO__
                                  // [LFB-M] agent-FM (forward-family site in
                                  // this TU): Lua string -> FString via the
                                  // ONE bridge converter (hand UTF-8 ->
                                  // UTF-16 -> FString::AppendChars; logs
                                  // 'fstring-fwd'). Was ensure_str +
                                  // FString(TCHAR*) — the Strlen/NUL-scan
                                  // ctor seam. Refused input throws the
                                  // binding's normal loud error.
                                  {
                                      const LuaType::LuaFNameBridge::Bridge bridged{str.data(), str.size()};
                                      if (!bridged.ok())
                                      {
                                          lua.throw_error("FString constructor: Lua string refused: embedded NUL/invalid/incomplete/non-scalar UTF-8 or overlong (no wide-libc fallback)");
                                      }
                                      auto fstring = bridged.to_fstring();
                                      LuaType::FString::construct(lua, &fstring);
                                      return 1;
                                  }
#else
                                  auto fstring = Unreal::FString(ensure_str(std::string(str)).c_str());
                                  LuaType::FString::construct(lua, &fstring);
                                  return 1;
#endif
                              });
        // FString Class -> END

        // FUtf8String Class -> START
        // Pre-load the global FUtf8String constructor
        lua.register_function("FUtf8String",
                              [](const LuaMadeSimple::Lua& lua) -> int {
                                  if (lua.get_stack_size() < 1 || !lua.is_string())
                                  {
                                      lua.throw_error("FUtf8String constructor requires a string argument");
                                  }
                                  std::string_view str = lua.get_string();
                                  auto utf8string = Unreal::FUtf8String(reinterpret_cast<const Unreal::UTF8CHAR*>(str.data()));
                                  LuaType::FUtf8String::construct(lua, &utf8string);
                                  return 1;
                              });
        // FUtf8String Class -> END

        // FAnsiString Class -> START
        // Pre-load the global FAnsiString constructor
        lua.register_function("FAnsiString",
                              [](const LuaMadeSimple::Lua& lua) -> int {
                                  if (lua.get_stack_size() < 1 || !lua.is_string())
                                  {
                                      lua.throw_error("FAnsiString constructor requires a string argument");
                                  }
                                  std::string_view str = lua.get_string();
                                  auto ansistring = Unreal::FAnsiString(str.data());
                                  LuaType::FAnsiString::construct(lua, &ansistring);
                                  return 1;
                              });
        // FAnsiString Class -> END

        // FPackageName -> START
        auto package_name = lua.prepare_new_table();
        package_name.set_has_userdata(false);

        package_name.add_pair("IsShortPackageName", [](const LuaMadeSimple::Lua& lua) -> int {
            static std::string error_overload_not_found{R"(
No overload found for function 'FPackageName:IsShortPackageName'.
Overloads:
#1: IsShortPackageName(string PossiblyLongName))"};

            if (!lua.is_string())
            {
                lua.throw_error(error_overload_not_found);
            }

#ifdef __PROSPERO__
            // [LFB-M] agent-FM: ONE hand converter for the package-name
            // Lua string (was ensure_str); the engine call keeps its
            // StringType shape via the bridge's .string() fill.
            const auto possibly_long_name_lua = lua.get_string();
            File::StringType PossiblyLongName = LuaType::LuaFNameBridge::Bridge{possibly_long_name_lua.data(), possibly_long_name_lua.size()}.string();
#else
            File::StringType PossiblyLongName = ensure_str(lua.get_string());
#endif
            lua.set_bool(Unreal::FPackageName::IsShortPackageName(PossiblyLongName));

            return 1;
        });

        package_name.add_pair("IsValidLongPackageName", [](const LuaMadeSimple::Lua& lua) -> int {
            static std::string error_overload_not_found{R"(
No overload found for function 'FPackageName:IsValidLongPackageName'.
Overloads:
#1: IsValidLongPackageName(string InLongPackageName))"};

            if (!lua.is_string())
            {
                lua.throw_error(error_overload_not_found);
            }

#ifdef __PROSPERO__
            // [LFB-M] agent-FM: same ONE-converter route as
            // IsShortPackageName above (was ensure_str).
            const auto in_long_package_name_lua = lua.get_string();
            File::StringType InLongPackageName = LuaType::LuaFNameBridge::Bridge{in_long_package_name_lua.data(), in_long_package_name_lua.size()}.string();
#else
            File::StringType InLongPackageName = ensure_str(lua.get_string());
#endif
            lua.set_bool(Unreal::FPackageName::IsValidLongPackageName(InLongPackageName));

            return 1;
        });

        package_name.make_global("FPackageName");
        // FPackageName -> END
    }

    auto LuaMod::setup_lua_classes(const LuaMadeSimple::Lua& lua) const -> void
    {
        setup_lua_classes_internal(lua);
    }

    auto LuaMod::prepare_mod(const LuaMadeSimple::Lua& lua) -> void
    {
        try
        {
            lua.open_all_libs();
            setup_lua_require_paths(lua);
#if defined(__PROSPERO__)
            // [SHIM] io/require logical->physical path resolver (agent-GA,
            // Claude round-5 item 4). Registration only — implementation:
            // candidate/src/LuaPathShim.cpp (+ include/LuaPathShim.hpp).
            // Runs AFTER setup_lua_require_paths on purpose: native mode
            // blanks package.path there, and the shim republishes logical
            // entries. Declared at block scope so this stays the ONE
            // insertion in this file (no #include edit higher up).
            {
                extern auto LuaPathShim_install(lua_State*, const char* mod_name, const char* modbridge_root) -> void;
                // Rule 10: derive the modbridge root from the RUNTIME-resolved
                // mods directory (<root>/ue4ss[/Mods]) — never a title-id.
                // HOTFIX 2026-10-11 (root, launch-2 session): weakly_canonical
                // THROWS on the PS5 game-process filesystem (observed console:
                // "Exception during mod preparation: filesystem error: in
                // weakly_canonical" for every mod, killing ALL mod startup).
                // The shim is a convenience, NEVER a mod-killer: any failure
                // in derivation -> install with no root (the shim disables
                // itself loudly; mods run with raw io like round-4).
                const char* shim_root_c = nullptr;
                std::string shim_root_str;
                try
                {
                    // ROOT FIX (Claude round-10 item 1): pure LEXICAL ops only.
                    // weakly_canonical/canonical/absolute/current_path all
                    // touch the OS filesystem (and THREW on the PS5 game
                    // process — launch-2 session evidence). The mods dir is
                    // already absolute; lexically_normal cleans "./" etc.
                    // without any syscall.
                    std::filesystem::path shim_root = std::filesystem::path(m_program.get_mods_directory()).lexically_normal();
                    if (shim_root.filename() == "Mods")
                    {
                        shim_root = shim_root.parent_path(); // <root>/ue4ss
                    }
                    shim_root = shim_root.parent_path(); // <root> (the modbridge game root)
                    shim_root_str = shim_root.string();
                    shim_root_c = shim_root_str.c_str();
                }
                catch (...)
                {
                    Output::send(STR("[SHIM] path-resolver disabled reason=root-derivation-failed (lexical path ops must not throw; if you see this, inspect get_mods_directory)\n"));
                }
                LuaPathShim_install(lua.get_lua_state(), to_utf8_string(m_mod_name).c_str(), shim_root_c);
            }
#endif
            setup_lua_global_functions(lua);
            setup_lua_classes(lua);

            // Setup a global reference for this mod
            // It can be accessed later when you otherwise don't have access to the 'Mod' instance
            LuaType::LuaModRef::construct(lua, this);
            lua_setglobal(lua.get_lua_state(), "ModRef");

            // Setup all the input related globals (keys & modifier keys)
            register_input_globals(lua);

            register_all_property_types(lua);
            register_object_flags(lua);
            register_efindname(lua);

            lua.set_nil();
            lua_setglobal(lua.get_lua_state(), "__OriginalReturnValue");
        }
        catch (std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("Exception during mod preparation: {}\n"), ensure_str(e.what()));
            throw; // Re-throw to allow proper error handling upstream
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("Unknown exception during mod preparation\n"));
            throw; // Re-throw to allow proper error handling upstream
        }
    }

    auto LuaMod::fire_on_lua_start_for_cpp_mods() -> void
    {
        if (!is_started())
        {
            return;
        }

        for (const auto& mod : UE4SSProgram::get_program().m_mods)
        {
            if (auto cpp_mod = dynamic_cast<CppMod*>(mod.get()); cpp_mod && mod->is_started())
            {
                if (mod->get_name() == get_name())
                {
                    cpp_mod->fire_on_lua_start(m_lua, *m_main_lua, *m_async_lua, m_hook_lua);
                }
                cpp_mod->fire_on_lua_start(get_name(), m_lua, *m_main_lua, *m_async_lua, m_hook_lua);
            }
        }
    }

    auto LuaMod::fire_on_lua_stop_for_cpp_mods() -> void
    {
        if (!is_started())
        {
            return;
        }

        for (const auto& mod : UE4SSProgram::get_program().m_mods)
        {
            if (auto cpp_mod = dynamic_cast<CppMod*>(mod.get()); cpp_mod && mod->is_started())
            {
                if (mod->get_name() == get_name())
                {
                    cpp_mod->fire_on_lua_stop(m_lua, *m_main_lua, *m_async_lua, m_hook_lua);
                }
                cpp_mod->fire_on_lua_stop(get_name(), m_lua, *m_main_lua, *m_async_lua, m_hook_lua);
            }
        }
    }

    auto LuaMod::load_and_execute_script(const std::filesystem::path& script_path) -> bool
    {
        try
        {
            std::optional<Platform::NativeFileBuffer> native_buffer;
            std::vector<char> disk_buffer;
            std::string_view bytes;
            if (m_native_source)
            {
                native_buffer = m_native_source->read(modbridge::normalize_path_for_lua(script_path));
                if (!native_buffer) return false;
                bytes = native_buffer->bytes;
            }
            else
            {
                if (!std::filesystem::exists(script_path)) return false;
                std::ifstream file(script_path, std::ios::binary);
                if (!file.is_open())
                    throw std::runtime_error(fmt::format("Failed to open script file: {}", to_utf8_string(script_path)));
                file.seekg(0, std::ios::end);
                std::streamsize size = file.tellg();
                if (size < 0) throw std::runtime_error("Failed to measure script file");
                file.seekg(0, std::ios::beg);
                disk_buffer.resize(static_cast<size_t>(size));
                if (!file.read(disk_buffer.data(), size))
                    throw std::runtime_error(fmt::format("Failed to read script file: {}", to_utf8_string(script_path)));
                bytes = std::string_view(disk_buffer.data(), disk_buffer.size());
            }

            // Generate a UTF-8 chunk name for better error messages
            std::string chunk_name = "@" + to_utf8_string(script_path);

            lua_State* L = main_lua()->get_lua_state();

            // Push error handler first so we capture the stack before it unwinds
            int err_handler_idx = LuaMadeSimple::push_pcall_error_handler(L);

            // Load the buffer
            if (int status = m_native_source ? native_lua_chunk(L, bytes, chunk_name.c_str()) :
                                              luaL_loadbuffer(L, bytes.data(), bytes.size(), chunk_name.c_str()); status != LUA_OK)
            {
                std::string error_msg = lua_tostring(L, -1);
                Output::send<LogLevel::Error>(STR("Error loading script: {}\n"), ensure_str(error_msg));
                lua_pop(L, 1);
                lua_remove(L, err_handler_idx); // Clean up error handler
                return false;
            }

            // Execute the chunk with our error handler
            if (int status = lua_pcall(L, 0, 0, err_handler_idx); status != LUA_OK)
            {
                // Error handler already captured the stack and notified debugger
                std::string error_msg = lua_tostring(L, -1);
                Output::send<LogLevel::Error>(STR("Error executing script: {}\n"), ensure_str(error_msg));
                lua_pop(L, 1);
                lua_remove(L, err_handler_idx); // Clean up error handler
                return false;
            }

            lua_remove(L, err_handler_idx); // Clean up error handler
            return true;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("Exception in load_and_execute_script: {}\n"), ensure_str(e.what()));
            return false;
        }
    }

    auto LuaMod::start_mod() -> void
    {
        try
        {
            m_main_thread_id = std::this_thread::get_id();

#if defined(__PROSPERO__)
            // [R9-MEM-REBASE] this mod's Lua state was created in the constructor (setup_mods),
            // before the engine allocator was bound; while it is still empty, move it onto
            // the installed backing store (GMalloc). Fail-closed: anything else keeps libc.
            {
                const int rebase = LuaMadeSimple::rebase_pristine_state_on_backing(const_cast<LuaMadeSimple::Lua&>(lua()));
                try
                {
                    if (rebase == 1 || rebase == 0)
                    {
                        Output::send(STR("[MEM] mod={} lua-state-backend=gmalloc rebase={}\n"), m_mod_name, rebase);
                    }
                    else
                    {
                        // reviewer: a mod left on libc is a FAILURE, not a pass (rebase=-1 no backing,
                        // -2 state not pristine, -3 allocation failed).
                        Output::send<LogLevel::Error>(STR("[MEM] FAIL mod={} lua-state-backend=libc rebase={}\n"), m_mod_name, rebase);
                    }
                }
                catch (...)
                {
                }
            }
#endif
            prepare_mod(lua());
            make_main_state(this, lua());
            setup_lua_global_functions_main_state_only();
            make_async_state(this, lua());
            try
            {
                Output::send(STR("[PERF-MAP] mod={} main_state={} async_state={}\n"), m_mod_name,
                             main_lua() ? static_cast<const void*>(main_lua()->get_lua_state()) : nullptr,
                             async_lua() ? static_cast<const void*>(async_lua()->get_lua_state()) : nullptr);
            }
            catch (...)
            {
            }
            start_async_thread();

            m_is_started = true;
            fire_on_lua_start_for_cpp_mods();

            // Set up the custom module loader for handling UTF-8 paths
            setup_custom_module_loader(main_lua());

            // Use the scripts path that was already determined in the constructor
            std::filesystem::path main_script_path = m_scripts_path / STR("main.lua");

            if (m_native_source || std::filesystem::exists(main_script_path))
            {
                if (!load_and_execute_script(main_script_path))
                {
                    Output::send<LogLevel::Error>(STR("Failed to execute main script: {}\n"), ensure_str(main_script_path));
                }
                else
                {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] Lua mod started: {} (main script returned)\n"), m_mod_name);
                    });
#if defined(__PROSPERO__)
                    L23GameThreadProbe::Queue(this,main_lua()->get_lua_state());
#endif
                }
            }
            else
            {
                // This case implies m_scripts_path itself is valid, but main.lua is missing
                Output::send<LogLevel::Error>(
                        STR("Main script 'main.lua' not found in scripts directory: {} -- Ensure your script file uses the correct casing.\n"),
                        ensure_str(m_scripts_path));
            }
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("Exception during mod startup: {}\n"), ensure_str(e.what()));
        }
    }

    template <typename T>
    concept IsPair = requires(T t) {
        { t.second };
    };
    template <typename T>
    concept IsDataIndirect = requires(T t) {
        { t.callback_data };
    };

    static auto erase_from_container(LuaMod* mod, auto& container) -> void
    {
        for (auto it = container.begin(); it != container.end();)
        {
            const auto& data = [&] {
                if constexpr (IsPair<decltype(*it)>)
                {
                    return it->second;
                }
                else if constexpr (IsDataIndirect<decltype(*it)>)
                {
                    return it->callback_data;
                }
                else
                {
                    return *it;
                }
            }();
            if (get_mod_ref(*data.lua) == mod)
            {
                it = container.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    auto LuaMod::fire_on_lua_stop_for_self() -> void
    {
        if (!is_started())
        {
            return;
        }

        if (m_on_unload_lua_function_ref != LUA_NOREF && m_on_unload_lua_function_ref != LUA_REFNIL)
        {
            TRY([&]() {
                lua().registry().get_function_ref(m_on_unload_lua_function_ref);
                lua().call_function(0, 0);
            });
        }
    }

    auto LuaMod::uninstall() -> void
    {
#ifdef __PROSPERO__
        {
            // Wait for an in-flight action dispatch before any teardown side
            // effect. Do not retain this mutex while joining the async worker,
            // which may itself need it to queue an action.
            std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
            if (NativeLuaActionDispatching)
                throw std::runtime_error("Cannot synchronously uninstall a mod during Lua action dispatch; queue uninstall instead");
        }
#endif
        Output::send(STR("Stopping mod '{}' for uninstall\n"), m_mod_name);

        // Stop the async thread BEFORE acquiring the mutex to avoid deadlock
        // (async thread's callbacks may need the mutex when calling ExecuteInGameThread)
        if (m_async_thread.joinable())
        {
            m_async_thread.request_stop();
            m_async_thread.join();
        }

        // Now acquire mutex to safely modify shared data structures
        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

#if defined(__PROSPERO__)
        L23GameThreadProbe::Cancel(this);
#endif
        fire_on_lua_stop_for_cpp_mods();
        fire_on_lua_stop_for_self();

        erase_from_container(this, m_static_construct_object_lua_callbacks);
        m_pending_notify_on_new_object_callbacks.clear();
        erase_from_container(this, m_process_console_exec_pre_callbacks);
        erase_from_container(this, m_process_console_exec_post_callbacks);
        erase_from_container(this, m_global_command_lua_callbacks);
        erase_from_container(this, m_custom_command_lua_pre_callbacks);
        erase_from_container(this, m_custom_event_callbacks);
        erase_from_container(this, m_load_map_pre_callbacks);
        erase_from_container(this, m_load_map_post_callbacks);
        erase_from_container(this, m_init_game_state_pre_callbacks);
        erase_from_container(this, m_init_game_state_post_callbacks);
        erase_from_container(this, m_begin_play_pre_callbacks);
        erase_from_container(this, m_begin_play_post_callbacks);
        erase_from_container(this, m_call_function_by_name_with_arguments_pre_callbacks);
        erase_from_container(this, m_call_function_by_name_with_arguments_post_callbacks);
        erase_from_container(this, m_local_player_exec_pre_callbacks);
        erase_from_container(this, m_local_player_exec_post_callbacks);
        erase_from_container(this, m_script_hook_callbacks);

        UE4SSProgram::get_program().get_all_input_events([&](auto& key_set) {
            std::erase_if(key_set.key_data,
                          [&](auto& item) -> bool {
                              auto& [_, key_data] = item;
                              std::erase_if(key_data,
                                            [&](Input::KeyData& key_data) -> bool {
                                                // custom_data == 1: Bind came from Lua, and custom_data2 is a pointer to LuaMod.
                                                // custom_data == 2: Bind came from C++, and custom_data2 is a pointer to KeyDownEventData. Must free it.
                                                if (key_data.custom_data == 1)
                                                {
                                                    return key_data.custom_data2 == this;
                                                }
                                                else
                                                {
                                                    return false;
                                                }
                                            });

                              return key_data.empty();
                          });
        });


        // Mark all hooks for this mod as scheduled_for_removal FIRST
        // This prevents hooks from firing Lua code with a potentially invalid Lua state
        // The pre-hook checks this flag and returns early if true
        for (auto& item : g_hooked_script_function_data)
        {
            if (item->mod == this)
            {
                item->scheduled_for_removal = true;
            }
        }

        // Unhook all UFunctions for this mod BEFORE closing Lua state
        // This must happen BEFORE lua_close because:
        // 1. The remove_if_scheduled lambda in lua_unreal_script_function_hook_post calls luaL_unref
        //    on the Lua state, which would cause use-after-free if called after lua_close
        // 2. The hook callbacks access Lua state data that becomes invalid after lua_close
        //
        // By unregistering hooks first:
        // - If not mid-execution: callbacks are removed immediately from UFunction's callback maps
        // - If mid-execution: UnregistrationRequested is set, and FirePostCallbacks will skip the callback
        //   without calling it (it checks UnregistrationRequested before accessing CustomData)
        //
        // Note: We do NOT erase from g_hooked_script_function_data here yet, because the data
        // must remain valid in case a post-hook is still pending (the erase happens later).
        for (auto& item : g_hooked_script_function_data)
        {
            if (item->mod == this)
            {
                Output::send(STR("\tUnregistering hook by id '{}#{}' for mod {}\n"), item->unreal_function->GetName(), item->pre_callback_id, item->mod->get_name());
                Output::send(STR("\tUnregistering hook by id '{}#{}' for mod {}\n"), item->unreal_function->GetName(), item->post_callback_id, item->mod->get_name());
                item->unreal_function->UnregisterHook(item->pre_callback_id);
                item->unreal_function->UnregisterHook(item->post_callback_id);
            }
        }

        // Remove any pending game thread actions for this mod BEFORE closing Lua state
        // Otherwise process_event_hook may try to execute actions with an invalid Lua state
        // Note: action.lua points to m_hook_lua (a thread), so compare against that
        // Must be done BEFORE m_hook_lua is set to nullptr
        // Must hold mutex to synchronize with hook functions that process these vectors
        {
            std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
            std::erase_if(m_game_thread_actions, [&](const SimpleLuaAction& action) {
                return action.lua == m_hook_lua;
            });
            std::erase_if(m_pending_game_thread_actions, [&](const SimpleLuaAction& action) {
                return action.lua == m_hook_lua;
            });

            // Remove any pending engine tick actions for this mod
            std::erase_if(m_engine_tick_actions, [&](const SimpleLuaAction& action) {
                return action.lua == m_hook_lua;
            });
            std::erase_if(m_pending_engine_tick_actions, [&](const SimpleLuaAction& action) {
                return action.lua == m_hook_lua;
            });

            // Remove any delayed game thread actions for this mod
            std::erase_if(m_delayed_game_thread_actions, [&](const DelayedGameThreadAction& action) {
                return action.lua == m_hook_lua;
            });
            std::erase_if(m_pending_delayed_game_thread_actions, [&](const DelayedGameThreadAction& action) {
                return action.lua == m_hook_lua;
            });
        }

        if (m_hook_lua != nullptr)
        {
            m_hook_lua = nullptr; // lua_newthread results are handled by lua GC
        }

        if (m_async_lua && m_async_lua->get_lua_state())
        {
            lua_resetthread(m_async_lua->get_lua_state());
        }

        if (m_main_lua && m_main_lua->get_lua_state())
        {
            lua_resetthread(m_main_lua->get_lua_state());
        }

        lua_close(lua().get_lua_state());

        // Now erase the hook data from g_hooked_script_function_data
        // This is safe because:
        // 1. We already unregistered the hooks (callbacks won't fire)
        // 2. If any post-hooks were pending, FirePostCallbacks skips them due to UnregistrationRequested
        // 3. The Lua state is closed, so we don't need to call luaL_unref
        std::erase_if(g_hooked_script_function_data, [&](const std::unique_ptr<LuaUnrealScriptFunctionData>& item) {
            return item->mod == this;
        });

        clear_delayed_actions();
    }

    auto LuaMod::lua() const -> const LuaMadeSimple::Lua&
    {
        return m_lua;
    }

    auto LuaMod::main_lua() const -> const LuaMadeSimple::Lua*
    {
        return m_main_lua;
    }

    auto LuaMod::async_lua() const -> const LuaMadeSimple::Lua*
    {
        return m_async_lua;
    }

    auto LuaMod::get_lua_state() const -> lua_State*
    {
        return lua().get_lua_state();
    }

    auto static start_console_lua_executor() -> void
    {
        LuaStatics::console_executor = &LuaMadeSimple::new_state();
        LuaStatics::console_executor->open_all_libs();
        setup_lua_global_functions_internal(*LuaStatics::console_executor, LuaMod::IsTrueMod::No);
        setup_lua_classes_internal(*LuaStatics::console_executor);
        register_input_globals(*LuaStatics::console_executor);
        register_all_property_types(*LuaStatics::console_executor);
        register_object_flags(*LuaStatics::console_executor);
        LuaStatics::console_executor_enabled = true;
    };

    auto static stop_console_lua_executor() -> void
    {
        lua_close(LuaStatics::console_executor->get_lua_state());

        LuaStatics::console_executor = nullptr;
        LuaStatics::console_executor_enabled = false;
    }

    static auto script_hook([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::UObject* Context, Unreal::FFrame& Stack, [[maybe_unused]] void* RESULT_DECL) -> void
    {
        std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

        auto execute_hook = [&](std::vector<LuaMod::FunctionHookData>& callback_container, bool precise_name_match) {
            if (callback_container.empty())
            {
                return;
            }
            auto* node = Stack.Node();
            if (!node)
            {
                return;
            }
            auto data = precise_name_match ? LuaMod::find_function_hook_data(callback_container, node)
                                           : LuaMod::find_function_hook_data(callback_container, node->GetNamePrivate());
            if (data)
            {
                const auto& callback_data = data->callback_data;
                for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                {
                    const auto& lua = *lua_ptr;

                    // -1 is a special value that signifies that the hook has been unregistered.
                    if (registry_index.lua_index == -1)
                    {
                        continue;
                    }

                    // Guard each callback separately so a throwing hook can't skip the remaining callbacks or escape into the hook layer.
                    const auto stack_top_before_callback = lua_gettop(lua.get_lua_state());
                    try
                    {
                        get_function_ref_cxx(lua, registry_index.lua_index);

                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);

                        auto return_value_offset = node->GetReturnValueOffset();
                        auto has_return_value = return_value_offset != 0xFFFF;
                        auto num_unreal_params = node->GetNumParms();
                        if (has_return_value && num_unreal_params > 0)
                        {
                            --num_unreal_params;
                        }

                        if (has_return_value || num_unreal_params > 0)
                        {
                            for (Unreal::FProperty* param : Unreal::TFieldRange<Unreal::FProperty>(node, Unreal::EFieldIterationFlags::IncludeDeprecated))
                            {
                                if (!param->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                                {
                                    continue;
                                }
                                if (has_return_value && param->GetOffset_Internal() == return_value_offset)
                                {
                                    continue;
                                }

                                auto param_type = param->GetClass().GetFName();
                                auto param_type_comparison_index = param_type.GetComparisonIndex();
                                if (auto it = LuaType::StaticState::m_property_value_pushers.find(param_type_comparison_index);
                                    it != LuaType::StaticState::m_property_value_pushers.end())
                                {
                                    void* data{};
                                    if (param->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_OutParm))
                                    {
                                        data = Unreal::FindOutParamValueAddress(Stack, param);
                                    }
                                    else
                                    {
                                        data = param->ContainerPtrToValuePtr<void>(Stack.Locals());
                                    }
                                    const LuaType::PusherParams pusher_param{
                                            .operation = LuaType::Operation::GetParam,
                                            .lua = lua,
                                            .base = nullptr,
                                            .data = data,
                                            .property = param,
                                    };
                                    auto& type_handler = it->second;
                                    type_handler(pusher_param);
                                }
                                else
                                {
                                    throw std::runtime_error(fmt::format(
                                            "[script_hook] Tried accessing unreal property without a registered handler. Property type '{}' not supported.",
                                            to_string(param_type.ToString())));
                                }
                            };
                        }

                        lua.call_function(num_unreal_params + 1, 1);

                        bool return_value_handled{};
                        if (has_return_value && RESULT_DECL && lua.get_stack_size() > 0 && !lua.is_nil())
                        {
                            auto return_property = node->GetReturnProperty();
                            if (return_property)
                            {
                                auto return_property_type = return_property->GetClass().GetFName();
                                auto return_property_type_comparison_index = return_property_type.GetComparisonIndex();

                                if (auto it = LuaType::StaticState::m_property_value_pushers.find(return_property_type_comparison_index);
                                    it != LuaType::StaticState::m_property_value_pushers.end())
                                {
                                    const LuaType::PusherParams pusher_params{.operation = LuaType::Operation::Set,
                                                                              .lua = lua,
                                                                              .base = static_cast<Unreal::UObject*>(RESULT_DECL),
                                                                              .data = RESULT_DECL,
                                                                              .property = return_property};
                                    auto& type_handler = it->second;
                                    type_handler(pusher_params);
                                    return_value_handled = true;
                                }
                                else
                                {
                                    auto return_property_type_name = return_property_type.ToString();
                                    auto return_property_name = return_property->GetName();

                                    Output::send(STR("Tried altering return value of a custom BP function without a registered handler for return type Return "
                                                     "property '{}' of type '{}' not supported."),
                                                 return_property_name,
                                                 return_property_type_name);
                                }
                            }
                        }

                        if (!return_value_handled)
                        {
                            lua.discard_value();
                        }
                    }
                    catch (std::exception& e)
                    {
                        lua_settop(lua.get_lua_state(), stack_top_before_callback);
                        Output::send<LogLevel::Error>(STR("[script_hook] A callback threw an exception: {}\n"), ensure_str(e.what()));
                    }
                    catch (...)
                    {
                        lua_settop(lua.get_lua_state(), stack_top_before_callback);
                        Output::send<LogLevel::Error>(STR("[script_hook] A callback threw a non-standard exception\n"));
                    }
                }
            }
        };

        TRY([&] {
            execute_hook(LuaMod::m_custom_event_callbacks, false);
            execute_hook(LuaMod::m_script_hook_callbacks, true);
        });
    }

    namespace LuaMadeSimple
    {
        // [B7] defined in LuaMadeSimple.cpp; declared here, not in the header, so no consumer is recompiled for it.
        auto lua_cpp_probe(int* out) noexcept -> void;
    }

    auto LuaMod::on_program_start() -> void
    {
        // [B7] one startup line that proves how Lua errors travel in this binary. Contained: it runs before
        // the delete-listener registration, and a failed log write must never skip mod start (merge review).
        try
        {
            int b7_probe[9]{};
            LuaMadeSimple::lua_cpp_probe(b7_probe);
            if (b7_probe[0] == 1)
            {
                Output::send(STR("[LUA-CPP] mode=cxx-throw err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={}\n"), b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7]);
            }
            else if (b7_probe[0] == 0)
            {
                Output::send(STR("[LUA-CPP] mode=longjmp err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={}\n"), b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7]);
            }
            else
            {
                Output::send(STR("[LUA-CPP] mode=UNKNOWN ran={} err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={} void_status={}\n"), b7_probe[5], b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7], b7_probe[8]);
            }
        }
        catch (const std::exception&) // a log-write failure only; a Lua throw (never expected here) is not swallowed
        {
        }
        // [L27][ET nul-throw-armor] FIX 3 (listener registration armor, Phase
        // 5/6 catch-and-log pattern): the delete-listener registration is the
        // first statement of the mod-start phase — a throw here previously
        // propagated to UE4SSProgram::on_program_start's single TRY block and
        // silently skipped start_lua_mods (mods-started=0). Logged, never
        // fatal to mod start.
        // [L26][P2] (agent FX, 2026-10-10): the variant-B skip arm of this
        // registration (agent-AC G1) was REMOVED - the Lua-mod delete-listener
        // registration is unconditional. See
        // outputs/2026-10-08-l25-prep/parked/P2-variant-b-removal/.
        try
        {
            Unreal::UObjectArray::AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener);
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[L25] delete-listener registration exception: {}\n"), ensure_str(std::string{e.what()}));
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[L25] delete-listener registration exception: unknown\n"));
        }
#ifdef __PROSPERO__
        // [R3] launch-11 marshal consumer (see r3_marshal_calibration_consumer): must run
        // BEFORE the RegisterLoadMapPreCallback below — the first eager detour registration
        // in this function, which is exactly the class of registration that throws under
        // BOTH_FAIL. Ordering obligation of R3D 4.4a's deferral window.
        // [L27][ET nul-throw-armor] FIX 3 (carrier arming armor, Phase 5/6
        // catch-and-log pattern): this consumer's StaticFindObject long-path
        // lookups ("/Script/Engine.Actor:ReceiveTick" family) were the exact
        // throw site that killed mod start on console (the EC refusal threw,
        // UE4SSProgram's single TRY swallowed it, start_lua_mods never ran,
        // mods-started=0 while the event loop lived). The arming is now
        // armored AND its result is a marker; mods start regardless of the
        // arming outcome.
        try
        {
            r3_marshal_calibration_consumer();
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[L25] carrier arming exception: {}\n"), ensure_str(std::string{e.what()}));
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[L25] carrier arming exception: unknown\n"));
        }
        if (is_funcptr_marshal_armed())
        {
            Output::send(STR("[L25] carrier arming result: armed\n"));
        }
        else
        {
            Output::send(STR("[L25] carrier arming result: no carrier\n"));
            Output::send(STR("[R3] no carrier armed; mods starting anyway (keybinds live, EIGT deferred)\n"));
        }
#endif
        const Unreal::Hook::FCallbackOptions common_opts {false, false, STR("UE4SS"), STR("LuaModImpl")};
#ifdef __PROSPERO__
        // =================================================================================
        // [R3][worker-l11-loadmap-guard r2] RUNBOOK §5.2 closure (launch-11, 2026-10-05).
        // MEASURED routing of the first eager arm (LoadMap): RegisterLoadMapPreCallback
        // (Hooks.cpp:211) -> DetourInstance AddCallback -> EnsureHooked -> InstallHook
        // (is_ready flag check only, no exec access) -> ActivateHook ->
        // PLH::x64Detour::hook(). This path routes through NEITHER the asmhelper
        // WalkRxGuard site (ASMHelper::get_first_instruction_at_address is hit only by
        // the initializer's EARLIER resolve walks that assign the LoadMap address) NOR
        // a raw unguarded copy: in the composed launch-10/11 units every PLH touch of
        // live game text is mediated by the owned-memory registry (x64Detour 5bb9cea4
        // owned path; ZydisDisassembler facfea22 accessor reads; OwnedMemory read-lease
        // d824528d), whose copy/protect soft-refuse non-owned regions with ZERO
        // dereference and ZERO syscalls. Live game text is never registered in that
        // registry (scan sources are file-backed owned images; the AllocatorBridge
        // records only allocator-owned mappings), so in EVERY world this composition
        // reaches, each arm below soft-refuses at its FIRST step: the 100-byte prologue
        // read returns nothing (ZydisDisassembler.cpp:65) and hook() returns false at
        // the empty-insts check (x64Detour.cpp:355-358) BEFORE any protection or write
        // attempt; the isGood gates (:487-492) are never reached. Per arm: one PLH SEV
        // log + one Output Error from AddCallback (DetourInstance.hpp:203-206) + an
        // ERROR_ID return that this TU ignores. Crash-free, throw-free, and futile.
        // The igf6 naked-read risk named by RUNBOOK §5.2/DRAIN-TRACE C1 does NOT exist
        // on this path as composed (their XD pin targets a different-lineage port,
        // 0619d2b4; the composed unit is 5bb9cea4).
        // Why defer anyway (honest): (1) R3D §5.3 item 3 — in the R3 world the
        // initializer deferred the identical-class registrations (pump_pending=true),
        // so these arms are futile by the same law; (2) log hygiene — ONE loud Warning
        // naming exactly what stays unregistered instead of ~19 per-arm failure pairs
        // on every R3 boot; (3) process-lifetime honesty — there is no runtime re-arm
        // path (DIFF 2 header law), so the Lua-side loss is declared explicitly rather
        // than left emergent from the owned layer's refusals. Deferral predicate = the
        // initializer's own pump decision (R3MarshalPumpPending, declared in this TU at
        // line 93 by the marshal consumer — zero new imports). W-works world
        // (pump_pending=false): the block below runs unchanged. Lua-side loss (as of
        // [R3][worker-gate5-loadmap r3], which supersedes r2's "accepted but never
        // invoked" for the LoadMap pair ONLY): RegisterLoadMapPreHook/PostHook callbacks
        // are still accepted (they append to m_load_map_pre/post_callbacks and mod
        // scripts keep loading) and a one-shot deferred arm (try_deferred_loadmap_arm at
        // funcptr_marshal_hook) re-attempts this exact registration once the marshal
        // world is proven live — on success TownfallItemSpawner main.lua:731-744's
        // per-map state reset delivers again; on refusal the loss is reported LOUDLY
        // (fail-closed), never silently. The sibling registrations below keep the full
        // r2 law (accepted, never invoked, no re-arm).
        // Reachability: at walkguard fold-0 the initializer's walks refuse loudly
        // BEFORE on_program_start (failed(1,<msg>) ack chain), so this gate is reached
        // only in worlds where the walks were admitted (fold-1) or seeded.
        // =================================================================================
        // [EARLY-4] (cap #4) PS5: this data-only branch is UNCONDITIONAL (the eager detour block
        // below is compiled out with #else), whatever the initializer's pump decision was; a
        // world where the pump is not pending (another game / engine version) logs it here and
        // still never reaches a code-detour registration.
        {
            if (!Unreal::R3MarshalPumpPending())
            {
                Output::send<LogLevel::Warning>(STR("[EARLY] pump-not-pending world: the eager detour block stays compiled out on PS5; the game-thread pump is the carrier marshal\n"));
            }
            // Publish the final LoadMap state BEFORE the skipped flag: WORLD-EV owns LoadMap
            // delivery for the whole process (3 = no native delivery, never changes on PS5).
            refused_log::loadmap_state.store(3, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            Output::send(STR("[EARLY] stage=loadmap-native state=off reason=ps5-no-code-detours owner=world-ev t_ms={}\n"), early4::now_ms());
            // One startup line for every family below that will accept registrations and never deliver them.
            // [WORLD-EV] InitGameState / BeginPlay left this list: the data-only object-index route delivers them
            refused_log::emit("detour-block", StringType{STR("EndPlay,StaticConstructObject(NotifyOnNewObject),ULocalPlayerExec,CallFunctionByNameWithArguments,ProcessConsoleExec(+console handlers)")},
                              StringType{STR("*")}, "detour-block-skipped-r3-marshal", "never-delivered");
            Output::send<LogLevel::Warning>(STR("[R3] LoadMap detour registrations deferred: in this world the initializer deferred its own detour registrations (R3 marshal owns the game-thread pump), so these eager arms are futile by the same law (owned-memory layer soft-refuses every live game-text access; each arm would log PLH SEV + an Error line and register nothing); InitGameState/BeginPlay/EndPlay/StaticConstructObject/ULocalPlayerExec/CallFunctionByNameWithArguments/ProcessConsoleExec/custom-event script hooks stay unregistered for this process lifetime (no re-arm); Lua-side LoadMap/InitGameState/BeginPlay callbacks are instead delivered by the data-only [WORLD-EV] object-index route (after the event; BeginPlay partial; the map that is already loaded when mods start is replayed once, see [EARLY] stage=startup-replay); no LoadMap detour is ever attempted on PS5\n"));
        }
#else
        {
        // [R3][worker-gate5-loadmap r3] delivery bodies moved to file-scope
        // lua_load_map_pre_callback / lua_load_map_post_callback (see funcptr_marshal_hook
        // region above) so the one-shot deferred arm registers the SAME callbacks;
        // W-works behavior (arm here, deliver on real LoadMap events) is unchanged.
        Unreal::Hook::RegisterLoadMapPreCallback(&lua_load_map_pre_callback, common_opts);

        Unreal::Hook::RegisterLoadMapPostCallback(&lua_load_map_post_callback, common_opts);

        Unreal::Hook::RegisterInitGameStatePreCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AGameModeBase* Context) {
            TRY([&] {
                for (const auto& callback_data : m_init_game_state_pre_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        lua.call_function(1, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterInitGameStatePostCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AGameModeBase* Context) {
            TRY([&] {
                for (const auto& callback_data : m_init_game_state_post_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        lua.call_function(1, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterBeginPlayPreCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AActor* Context) {
            TRY([&] {
                for (const auto& callback_data : m_begin_play_pre_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        lua.call_function(1, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterBeginPlayPostCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AActor* Context) {
            TRY([&] {
                for (const auto& callback_data : m_begin_play_post_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        lua.call_function(1, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterEndPlayPreCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AActor* Context, Unreal::EEndPlayReason EndPlayReason) {
            TRY([&] {
                for (const auto& callback_data : m_end_play_pre_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        static auto s_int_property_name = Unreal::FName(STR("IntProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &EndPlayReason, s_int_property_name);
                        lua.call_function(2, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterEndPlayPostCallback([]([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] Unreal::AActor* Context, Unreal::EEndPlayReason EndPlayReason) {
            TRY([&] {
                for (const auto& callback_data : m_end_play_post_callbacks)
                {
                    for (const auto& [lua_ptr, registry_index] : callback_data.registry_indexes)
                    {
                        const auto& lua = *lua_ptr;

                        lua.registry().get_function_ref(registry_index.lua_index);
                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &Context, s_object_property_name);
                        static auto s_int_property_name = Unreal::FName(STR("IntProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(lua, &EndPlayReason, s_int_property_name);
                        lua.call_function(2, 0);
                    }
                }
            });
        }, common_opts);

        Unreal::Hook::RegisterStaticConstructObjectPostCallback([](const Unreal::FStaticConstructObjectParameters&, Unreal::UObject* constructed_object) {
            return TRY([&] {
                // IMPORTANT: StaticConstructObject can be called from outside of the game thread (loading threads, etc.)
                // Lua is NOT thread-safe. If we're on the game thread, we can call Lua directly.
                // If we're on another thread, we must queue the callback for later processing.
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                Unreal::UStruct* object_class = constructed_object->GetClassPrivate();
                while (object_class)
                {
                    if (Unreal::IsInGameThreadRaw())
                    {
                        // On game thread call Lua directly, but avoid iterator invalidation:
                        // collect IDs first, then resolve each callback by ID when executing.
                        std::vector<uint64_t> callbacks_to_execute{};
                        callbacks_to_execute.reserve(m_static_construct_object_lua_callbacks.size());
                        for (const auto& callback_data : m_static_construct_object_lua_callbacks)
                        {
                            if (object_class->GetNamePrivate().Equals(callback_data.instance_class_name) && object_class->GetOuterPrivate()->GetNamePrivate().Equals(callback_data.instance_class_outer_name))
                            {
                                callbacks_to_execute.push_back(callback_data.callback_id);
                            }
                        }

                        std::unordered_set<uint64_t> callbacks_to_remove{};
                        for (uint64_t callback_id : callbacks_to_execute)
                        {
                            auto callback_it = std::find_if(
                                m_static_construct_object_lua_callbacks.begin(),
                                m_static_construct_object_lua_callbacks.end(),
                                [&](const LuaMod::LuaCancellableCallbackData& callback) { return callback.callback_id == callback_id; }
                            );
                            if (callback_it == m_static_construct_object_lua_callbacks.end())
                            {
                                continue;
                            }

                            auto& callback_data = *callback_it;
                            get_function_ref_cxx(*callback_data.lua, callback_data.lua_callback_function_ref);
                            LuaType::auto_construct_object(*callback_data.lua, constructed_object);
                            callback_data.lua->call_function(1, 1);

                            if (callback_data.lua->is_bool(-1))
                            {
                                // get_bool pops the value from the stack, so we shouldn't call lua_pop explicitly.
                                bool cancel = callback_data.lua->get_bool(-1);
                                if (cancel)
                                {
                                    // Release the thread_ref to GC.
                                    luaL_unref(callback_data.lua->get_lua_state(), LUA_REGISTRYINDEX, callback_data.lua_callback_thread_ref);
                                    callbacks_to_remove.insert(callback_id);
                                }
                            }
                            else
                            {
                                // No bool means we didn't call get_bool, which means the value wasn't popped from the stack, therefore we must explicitly call lua_pop.
                                lua_pop(callback_data.lua->get_lua_state(), 1);
                            }
                        }

                        if (!callbacks_to_remove.empty())
                        {
                            std::erase_if(m_static_construct_object_lua_callbacks, [&](const LuaMod::LuaCancellableCallbackData& callback) {
                                return callbacks_to_remove.contains(callback.callback_id);
                            });
                        }
                    }
                    else
                    {
                        // NOT on game thread - queue callbacks for later processing
                        for (const auto& callback_data : m_static_construct_object_lua_callbacks)
                        {
                            if (object_class->GetNamePrivate().Equals(callback_data.instance_class_name) && object_class->GetOuterPrivate()->GetNamePrivate().Equals(callback_data.instance_class_outer_name))
                            {
                                int32_t object_index = constructed_object->GetInternalIndex();
                                Unreal::FUObjectItem* object_item = Unreal::FUObjectArray::IndexToObject(object_index);
                                if (!object_item)
                                {
                                    continue;
                                }

                                // Queue this callback to be executed on the game thread
                                // Include validation fields to detect if callback was removed/modified
                                const auto serial = object_item->GetSerialNumber();
                                m_pending_notify_on_new_object_callbacks.push_back(
                                    LuaMod::PendingNotifyOnNewObjectCallback{
                                        callback_data.callback_id,
                                        object_index,
                                        serial
                                    }
                                );
                            }
                        }
                    }

                    object_class = object_class->GetSuperStruct();
                }

                return constructed_object;
            });
        });

        Unreal::Hook::RegisterULocalPlayerExecPreCallback([](Unreal::ULocalPlayer* context, Unreal::UWorld* in_world, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar)
                                                                  -> Unreal::Hook::ULocalPlayerExecCallbackReturnValue {
            return TRY([&] {
                for (const auto& callback_data : m_local_player_exec_pre_callbacks)
                {
                    Unreal::Hook::ULocalPlayerExecCallbackReturnValue return_value{};

                    for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                    {
                        callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
                        LuaType::RemoteUnrealParam::construct(*callback_data.lua, &in_world, s_object_property_name);
#ifdef __PROSPERO__
                        // [LFB-M] agent-FM: ULocalPlayerExec command echo
                        // (engine TCHAR*) -> Lua string via the bridge
                        // reverse loop (logs 'fname-narrow').
                        lfb_set_string_from_wide(*callback_data.lua, cmd);
#else
                        callback_data.lua->set_string(to_string(cmd));
#endif
                        LuaType::FOutputDevice::construct(*callback_data.lua, &ar);

                        callback_data.lua->call_function(4, 2);

                        if (callback_data.lua->is_nil())
                        {
                            return_value.UseOriginalReturnValue = true;
                            callback_data.lua->discard_value();
                        }
                        else if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"The first return value for 'RegisterULocalPlayerExecPreHook' must return bool or nil"};
                        }
                        else
                        {
                            return_value.UseOriginalReturnValue = false;
                            return_value.NewReturnValue = callback_data.lua->get_bool();
                        }

                        if (callback_data.lua->is_nil())
                        {
                            return_value.ExecuteOriginalFunction = true;
                            callback_data.lua->discard_value();
                        }
                        else if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"The second return value for callback 'RegisterULocalPlayerExecPreHook' must return bool or nil"};
                        }
                        else
                        {
                            return_value.ExecuteOriginalFunction = callback_data.lua->get_bool();
                        }
                    }

                    return return_value;
                }

                return Unreal::Hook::ULocalPlayerExecCallbackReturnValue{};
            });
        });

        Unreal::Hook::RegisterULocalPlayerExecPostCallback([](Unreal::ULocalPlayer* context, Unreal::UWorld* in_world, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar)
                                                                   -> Unreal::Hook::ULocalPlayerExecCallbackReturnValue {
            return TRY([&] {
                for (const auto& callback_data : m_local_player_exec_post_callbacks)
                {
                    Unreal::Hook::ULocalPlayerExecCallbackReturnValue return_value{};

                    for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                    {
                        callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                        static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                        LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
                        LuaType::RemoteUnrealParam::construct(*callback_data.lua, &in_world, s_object_property_name);
#ifdef __PROSPERO__
                        // [LFB-M] agent-FM: ULocalPlayerExec command echo
                        // (engine TCHAR*) -> Lua string via the bridge
                        // reverse loop (logs 'fname-narrow').
                        lfb_set_string_from_wide(*callback_data.lua, cmd);
#else
                        callback_data.lua->set_string(to_string(cmd));
#endif
                        LuaType::FOutputDevice::construct(*callback_data.lua, &ar);

                        callback_data.lua->call_function(4, 2);

                        if (callback_data.lua->is_nil())
                        {
                            return_value.UseOriginalReturnValue = true;
                            callback_data.lua->discard_value();
                        }
                        else if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"A callback for 'RegisterULocalPlayerExecPreHook' must return bool or nil"};
                        }
                        else
                        {
                            return_value.UseOriginalReturnValue = false;
                            return_value.NewReturnValue = callback_data.lua->get_bool();
                        }

                        if (callback_data.lua->is_nil())
                        {
                            return_value.ExecuteOriginalFunction = true;
                            callback_data.lua->discard_value();
                        }
                        else if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"The second return value for callback 'RegisterULocalPlayerExecPreHook' must return bool or nil"};
                        }
                        else
                        {
                            return_value.ExecuteOriginalFunction = callback_data.lua->get_bool();
                        }
                    }

                    return return_value;
                }

                return Unreal::Hook::ULocalPlayerExecCallbackReturnValue{};
            });
        });

        Unreal::Hook::RegisterCallFunctionByNameWithArgumentsPreCallback(
                [](Unreal::UObject* context, const Unreal::TCHAR* str, Unreal::FOutputDevice& ar, Unreal::UObject* executor, bool b_force_call_with_non_exec)
                        -> std::pair<bool, bool> {
                    return TRY([&] {
                        std::pair<bool, bool> return_value{};
                        for (const auto& callback_data : m_call_function_by_name_with_arguments_pre_callbacks)
                        {

                            for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                            {
                                callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                                static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
#ifdef __PROSPERO__
                                // [LFB-M] agent-FM: CallFunctionByNameWithName
                                // command echo (engine TCHAR*) -> Lua string
                                // via the bridge reverse loop (logs
                                // 'fname-narrow').
                                lfb_set_string_from_wide(*callback_data.lua, str);
#else
                                callback_data.lua->set_string(to_string(str));
#endif
                                LuaType::FOutputDevice::construct(*callback_data.lua, &ar);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &executor, s_object_property_name);
                                callback_data.lua->set_bool(b_force_call_with_non_exec);

                                callback_data.lua->call_function(5, 1);

                                if (callback_data.lua->is_nil())
                                {
                                    return_value.first = false;
                                    callback_data.lua->discard_value();
                                }
                                else if (!callback_data.lua->is_bool())
                                {
                                    throw std::runtime_error{"A callback for 'RegisterCallFunctionByNameWithArgumentsPreHook' must return bool or nil"};
                                }
                                else
                                {
                                    return_value.first = true;
                                    return_value.second = callback_data.lua->get_bool();
                                }
                            }
                        }

                        return return_value;
                    });
                });

        Unreal::Hook::RegisterCallFunctionByNameWithArgumentsPostCallback(
                [](Unreal::UObject* context, const Unreal::TCHAR* str, Unreal::FOutputDevice& ar, Unreal::UObject* executor, bool b_force_call_with_non_exec)
                        -> std::pair<bool, bool> {
                    return TRY([&] {
                        std::pair<bool, bool> return_value{};
                        for (const auto& callback_data : m_call_function_by_name_with_arguments_post_callbacks)
                        {

                            for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                            {
                                callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                                static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
#ifdef __PROSPERO__
                                // [LFB-M] agent-FM: CallFunctionByNameWithName
                                // command echo (engine TCHAR*) -> Lua string
                                // via the bridge reverse loop (logs
                                // 'fname-narrow').
                                lfb_set_string_from_wide(*callback_data.lua, str);
#else
                                callback_data.lua->set_string(to_string(str));
#endif
                                LuaType::FOutputDevice::construct(*callback_data.lua, &ar);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &executor, s_object_property_name);
                                callback_data.lua->set_bool(b_force_call_with_non_exec);

                                callback_data.lua->call_function(5, 1);

                                if (callback_data.lua->is_nil())
                                {
                                    return_value.first = false;
                                    callback_data.lua->discard_value();
                                }
                                else if (!callback_data.lua->is_bool())
                                {
                                    throw std::runtime_error{"A callback for 'RegisterCallFunctionByNameWithArgumentsPreHook' must return bool or nil"};
                                }
                                else
                                {
                                    return_value.first = true;
                                    return_value.second = callback_data.lua->get_bool();
                                }
                            }
                        }

                        return return_value;
                    });
                });

        // Lua from the in-game console.
        Unreal::Hook::RegisterProcessConsoleExecCallback([](Unreal::UObject* context, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar, Unreal::UObject* executor) -> bool {
            auto logln = [&ar](const File::StringType& log_message) {
                Output::send(fmt::format(STR("{}\n"), log_message));
                ar.Log(FromCharTypePtr<Unreal::TCHAR>(log_message.c_str()));
            };

            if (!LuaStatics::console_executor_enabled && String::iequal(File::StringViewType{ToCharTypePtr(cmd)}, File::StringViewType{STR("luastart")}))
            {
                start_console_lua_executor();
                logln(STR("Console Lua executor started"));
                return true;
            }
            else if (LuaStatics::console_executor_enabled && String::iequal(File::StringViewType{ToCharTypePtr(cmd)}, File::StringViewType{STR("luastop")}))
            {
                stop_console_lua_executor();
                logln(STR("Console Lua executor stopped"));
                return true;
            }
            else if (LuaStatics::console_executor_enabled && String::iequal(File::StringViewType{ToCharTypePtr(cmd)}, File::StringViewType{STR("luarestart")}))
            {
                stop_console_lua_executor();
                start_console_lua_executor();
                logln(STR("Console Lua executor restarted"));
                return true;
            }
            else if (String::iequal(File::StringViewType{ToCharTypePtr(cmd)}, File::StringViewType{STR("clear")}))
            {
                // TODO: Replace with proper implementation when we have UGameViewportClient and UConsole.
                //       This should be fairly cross-game & cross-engine-version compatible even without the proper implementation.
                //       This is because I don't think they've changed the layout here and we have a reflected property right before the unreflected one that we're looking for.
                Unreal::UObject** console = static_cast<Unreal::UObject**>(context->GetValuePtrByPropertyName(FromCharTypePtr<Unreal::TCHAR>(STR("ViewportConsole"))));
                auto* default_texture_white = std::bit_cast<Unreal::TArray<Unreal::FString>*>(
                        static_cast<uint8_t*>((*console)->GetValuePtrByPropertyNameInChain(FromCharTypePtr<Unreal::TCHAR>(STR("DefaultTexture_White")))) + 0x8);
                auto* scrollback = std::bit_cast<int32_t*>(std::bit_cast<uint8_t*>(default_texture_white) + 0x10);
                default_texture_white->SetNum(0);
                default_texture_white->SetMax(0);
                *scrollback = 0;
                return true;
            }
            else if (LuaStatics::console_executor_enabled)
            {
                if (!LuaStatics::console_executor)
                {
                    logln(STR("Console Lua executor is enabled but the Lua instance is nullptr. Please try run RC_LUA_START again."));
                    return true;
                }

                LuaLibrary::set_outputdevice_ref(*LuaStatics::console_executor, &ar);

                // logln(fmt::format(STR("Executing '{}' as Lua"), cmd));

                try
                {
                    lua_State* L = LuaStatics::console_executor->get_lua_state();

                    // Push error handler to capture stack before it unwinds
                    int err_handler_idx = LuaMadeSimple::push_pcall_error_handler(L);

#ifdef __PROSPERO__
                    // [LFB-M] agent-FM: console-executor engine command
                    // (TCHAR*) -> narrow Lua chunk via the bridge reverse
                    // loop (logs 'fname-narrow'). Was to_string (Helpers
                    // codec).
                    if (int status = luaL_loadstring(L, lfb_narrow_from_wide(cmd).c_str()); status != LUA_OK)
#else
                    if (int status = luaL_loadstring(L, to_string(cmd).c_str()); status != LUA_OK)
#endif
                    {
                        lua_remove(L, err_handler_idx);
                        LuaStatics::console_executor->throw_error(
                                fmt::format("luaL_loadstring returned {}", LuaStatics::console_executor->resolve_status_message(status, true)));
                    }

                    if (int status = lua_pcall(L, 0, LUA_MULTRET, err_handler_idx); status != LUA_OK)
                    {
                        lua_pop(L, 1); // Pop error message
                        lua_remove(L, err_handler_idx);
                        LuaStatics::console_executor->throw_error(
                                fmt::format("lua_pcall returned {}", LuaStatics::console_executor->resolve_status_message(status, true)));
                    }

                    lua_remove(L, err_handler_idx);
                }
                catch (std::runtime_error& e)
                {
                    logln(ensure_str(e.what()));
                }

                // We always return true when the console Lua executor is enabled in order to suppress other handlers
                return true;
            }
            else
            {
                return false;
            }
        });

        // RegisterProcessConsoleExecPreHook
        Unreal::Hook::RegisterProcessConsoleExecGlobalPreCallback(
                [](Unreal::UObject* context, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar, Unreal::UObject* executor) -> std::pair<bool, bool> {
                    return TRY([&] {
                        auto command = File::StringType{ToCharTypePtr(cmd)};
                        auto command_parts = explode_by_occurrence_with_quotes(command, STR(' '));

                        std::pair<bool, bool> return_value{};
                        for (const auto& callback_data : m_process_console_exec_pre_callbacks)
                        {

                            for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                            {
                                callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                                static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
        #ifdef __PROSPERO__
                        // [LFB-M] agent-FM: console command echo (wide
                        // StringType) -> Lua string via the bridge reverse
                        // loop (logs 'fname-narrow').
                        lfb_set_string_from_stringtype(*callback_data.lua, command);
#else
                        callback_data.lua->set_string(to_string(command));
#endif
                                auto params_table = callback_data.lua->prepare_new_table();
                                for (size_t i = 0; i < command_parts.size(); ++i)
                                {
                                    const auto& command_part = command_parts[i];
#ifdef __PROSPERO__
                                    // [LFB-M] agent-FM: command part (wide
                                    // StringType) -> Lua table value via
                                    // the bridge reverse loop (logs
                                    // 'fname-narrow').
                                    params_table.add_pair(i + 1, lfb_narrow_from_stringtype(command_part).c_str());
#else
                                    params_table.add_pair(i + 1, to_string(command_part).c_str());
#endif
                                }
                                LuaType::FOutputDevice::construct(*callback_data.lua, &ar);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &executor, s_object_property_name);

                                callback_data.lua->call_function(5, 1);

                                if (callback_data.lua->is_nil())
                                {
                                    return_value.first = false;
                                    callback_data.lua->discard_value();
                                }
                                else if (callback_data.lua->is_bool())
                                {
                                    return_value.first = true;
                                    return_value.second = callback_data.lua->get_bool();
                                }
                                else
                                {
                                    throw std::runtime_error{"A callback for 'RegisterProcessConsoleExecHook' must return bool or nil"};
                                }
                            }
                        }

                        return return_value;
                    });
                });

        // RegisterProcessConsoleExecPostHook
        Unreal::Hook::RegisterProcessConsoleExecGlobalPostCallback(
                [](Unreal::UObject* context, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar, Unreal::UObject* executor) -> std::pair<bool, bool> {
                    return TRY([&] {
                        auto command = File::StringType{ToCharTypePtr(cmd)};
                        auto command_parts = explode_by_occurrence_with_quotes(command, STR(' '));

                        std::pair<bool, bool> return_value{};
                        for (const auto& callback_data : m_process_console_exec_post_callbacks)
                        {
                            for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                            {
                                callback_data.lua->registry().get_function_ref(registry_index.lua_index);

                                static auto s_object_property_name = Unreal::FName(STR("ObjectProperty"), Unreal::FNAME_Find);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &context, s_object_property_name);
        #ifdef __PROSPERO__
                        // [LFB-M] agent-FM: console command echo (wide
                        // StringType) -> Lua string via the bridge reverse
                        // loop (logs 'fname-narrow').
                        lfb_set_string_from_stringtype(*callback_data.lua, command);
#else
                        callback_data.lua->set_string(to_string(command));
#endif
                                auto params_table = callback_data.lua->prepare_new_table();
                                for (size_t i = 0; i < command_parts.size(); ++i)
                                {
                                    const auto& command_part = command_parts[i];
#ifdef __PROSPERO__
                                    // [LFB-M] agent-FM: command part (wide
                                    // StringType) -> Lua table value via
                                    // the bridge reverse loop (logs
                                    // 'fname-narrow').
                                    params_table.add_pair(i + 1, lfb_narrow_from_stringtype(command_part).c_str());
#else
                                    params_table.add_pair(i + 1, to_string(command_part).c_str());
#endif
                                }
                                LuaType::FOutputDevice::construct(*callback_data.lua, &ar);
                                LuaType::RemoteUnrealParam::construct(*callback_data.lua, &executor, s_object_property_name);

                                callback_data.lua->call_function(5, 1);

                                if (callback_data.lua->is_nil())
                                {
                                    return_value.first = false;
                                    callback_data.lua->discard_value();
                                }
                                else if (callback_data.lua->is_bool())
                                {
                                    return_value.first = true;
                                    return_value.second = callback_data.lua->get_bool();
                                }
                                else
                                {
                                    throw std::runtime_error{"A callback for 'RegisterProcessConsoleExecHook' must return bool or nil"};
                                }
                            }
                        }

                        return return_value;
                    });
                });

        // RegisterConsoleCommandHandler
        Unreal::Hook::RegisterProcessConsoleExecCallback([](Unreal::UObject* context, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar, Unreal::UObject* executor) -> bool {
            (void)executor;

            if (!Unreal::Cast<Unreal::UGameViewportClient>(context))
            {
                return false;
            }

            return TRY([&] {
                auto command = File::StringType{ToCharTypePtr(cmd)};
                auto command_parts = explode_by_occurrence_with_quotes(command, STR(' '));
                File::StringType command_name = command;
                if (command_parts.size() > 1)
                {
                    command_name = command_parts[0];
                }

                if (auto it = m_custom_command_lua_pre_callbacks.find(command_name); it != m_custom_command_lua_pre_callbacks.end())
                {
                    const auto& callback_data = it->second;

                    bool return_value{};

                    for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                    {
                        callback_data.lua->registry().get_function_ref(registry_index.lua_index);
#ifdef __PROSPERO__
                        // [LFB-M] agent-FM: console command echo (wide
                        // StringType) -> Lua string via the bridge reverse
                        // loop (logs 'fname-narrow').
                        lfb_set_string_from_stringtype(*callback_data.lua, command);
#else
                        callback_data.lua->set_string(to_string(command));
#endif

                        auto params_table = callback_data.lua->prepare_new_table();
                        for (size_t i = 1; i < command_parts.size(); ++i)
                        {
                            const auto& command_part = command_parts[i];
#ifdef __PROSPERO__
                            // [LFB-M] agent-FM: command part (wide
                            // StringType) -> Lua table value via the
                            // bridge reverse loop (logs 'fname-narrow').
                            params_table.add_pair(i, lfb_narrow_from_stringtype(command_part).c_str());
#else
                            params_table.add_pair(i, to_string(command_part).c_str());
#endif
                        }

                        LuaType::FOutputDevice::construct(*callback_data.lua, &ar);

                        callback_data.lua->call_function(3, 1);

                        if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"A custom console command handle must return true or false"};
                        }

                        return_value = callback_data.lua->get_bool();
                    }

                    return return_value;
                }

                return false;
            });
        });

        // RegisterConsoleCommandGlobalHandler
        Unreal::Hook::RegisterProcessConsoleExecCallback([](Unreal::UObject* context, const Unreal::TCHAR* cmd, Unreal::FOutputDevice& ar, Unreal::UObject* executor) -> bool {
            (void)context;
            (void)executor;

            return TRY([&] {
                auto command = File::StringType{ToCharTypePtr(cmd)};
                auto command_parts = explode_by_occurrence_with_quotes(command, STR(' '));
                File::StringType command_name = command;
                if (command_parts.size() > 1)
                {
                    command_name = command_parts[0];
                }

                if (auto it = m_global_command_lua_callbacks.find(command_name); it != m_global_command_lua_callbacks.end())
                {
                    const auto& callback_data = it->second;

                    bool return_value{};

                    for (const auto& [lua, registry_index] : callback_data.registry_indexes)
                    {
                        callback_data.lua->registry().get_function_ref(registry_index.lua_index);
#ifdef __PROSPERO__
                        // [LFB-M] agent-FM: console command echo (wide
                        // StringType) -> Lua string via the bridge reverse
                        // loop (logs 'fname-narrow').
                        lfb_set_string_from_stringtype(*callback_data.lua, command);
#else
                        callback_data.lua->set_string(to_string(command));
#endif

                        auto params_table = callback_data.lua->prepare_new_table();
                        for (size_t i = 1; i < command_parts.size(); ++i)
                        {
                            const auto& command_part = command_parts[i];
#ifdef __PROSPERO__
                            // [LFB-M] agent-FM: command part (wide
                            // StringType) -> Lua table value via the
                            // bridge reverse loop (logs 'fname-narrow').
                            params_table.add_pair(i, lfb_narrow_from_stringtype(command_part).c_str());
#else
                            params_table.add_pair(i, to_string(command_part).c_str());
#endif
                        }

                        LuaType::FOutputDevice::construct(*callback_data.lua, &ar);

                        callback_data.lua->call_function(3, 1);

                        if (!callback_data.lua->is_bool())
                        {
                            throw std::runtime_error{"A custom console command handle must return true or false"};
                        }

                        return_value = callback_data.lua->get_bool();
                    }

                    return return_value;
                }

                return false;
            });
        });

        if (Unreal::UObject::ProcessLocalScriptFunctionInternal.is_ready() && Unreal::Version::IsAtLeast(4, 22))
        {
            Output::send(STR("Enabling custom events\n"));
            Unreal::Hook::RegisterProcessLocalScriptFunctionPostCallback(script_hook, {false, false, STR("UE4SS"), STR("LuaModImplScriptHook")});
        }
        else if (Unreal::UObject::ProcessInternalInternal.is_ready() && Unreal::Version::IsBelow(4, 22))
        {
            Output::send(STR("Enabling custom events\n"));
            Unreal::Hook::RegisterProcessInternalPostCallback(script_hook, {false, false, STR("UE4SS"), STR("LuaModImplScriptHook")});
        }
        } // [R3][worker-l11-loadmap-guard r2] end of the eager detour-arm block (see gate above)
#endif // [EARLY-4] eager detour block: non-PS5 only
    }

    auto LuaMod::update_async() -> void
    {
        int64_t async_perf_ns{0};
        int64_t async_perf_window_start_ns{0};
        for (m_processing_events = true; m_processing_events && !m_async_thread.get_stop_token().stop_requested();)
        {
            if (!Unreal::UnrealInitializer::IsInitialized())
            {
                m_processing_events = false;
                break;
            }

            if (m_pause_events_processing)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            // [R9-GUARD] an error in a mod's LoopAsync/ExecuteAsync must never end this thread
            // (an escaping exception terminates the game): contain it and keep looping, as PC does.
            const int64_t async_t0 = r9perf::now_ns();
            try
            {
                process_delayed_actions();
            }
            catch (...)
            {
                try { Output::send<LogLevel::Error>(STR("[R9-GUARD] async action loop contained an exception; continuing\n")); } catch (...) {}
            }
            {
                // [R9-PERF] per-mod async (LoopAsync/ExecuteAsync/delays) time, one line per second
                const int64_t async_t1 = r9perf::now_ns();
                async_perf_ns += async_t1 - async_t0;
                if (async_perf_window_start_ns == 0) { async_perf_window_start_ns = async_t1; }
                if (async_t1 - async_perf_window_start_ns >= 1'000'000'000)
                {
                    if (async_perf_ns >= 5'000'000) // only mods that took >= 5 ms this second
                    {
                        try { Output::send(STR("[PERF-ASYNC] mod={} ms={:.1f}\n"), m_mod_name, async_perf_ns / 1e6); } catch (...) {}
                    }
                    async_perf_ns = 0;
                    async_perf_window_start_ns = async_t1;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        m_processing_events = false;
    }

    auto LuaMod::process_delayed_actions() -> void
    {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

        {
            const ActionsLockGuard actions_guard{this};

            m_delayed_actions.insert(m_delayed_actions.end(), std::make_move_iterator(m_pending_actions.begin()), std::make_move_iterator(m_pending_actions.end()));
            m_pending_actions.clear();

        }

        m_delayed_actions.erase(std::remove_if(m_delayed_actions.begin(),
                                               m_delayed_actions.end(),
                                               [&](AsyncAction& action) -> bool {
                                                   auto passed =
                                                           now -
                                                           std::chrono::duration_cast<std::chrono::milliseconds>(action.created_at.time_since_epoch()).count();
                                                   auto duration_since_creation = (action.type == LuaMod::ActionType::Immediate || passed >= action.delay);
                                                   if (duration_since_creation)
                                                   {
                                                       if (!Unreal::UnrealInitializer::IsInitialized())
                                                       {
                                                           return true;
                                                       }
                                                       bool result = true;
                                                       try
                                                       {
                                                           async_lua()->registry().get_function_ref(action.lua_action_function_ref);
                                                           if (action.type == LuaMod::ActionType::Loop)
                                                           {
                                                               async_lua()->call_function(0, 1);
                                                               result = async_lua()->is_bool() && async_lua()->get_bool();
                                                               action.created_at = std::chrono::steady_clock::now();
                                                           }
                                                           else
                                                           {
                                                               async_lua()->call_function(0, 0);
                                                           }
                                                       }
                                                       catch (std::exception& e)
                                                       {
                                                           try
                                                           {
                                                               Output::send(STR("[{}] {}\n"),
                                                                            ensure_str(action.type == LuaMod::ActionType::Loop ? "LoopAsync" : "DelayedAction"),
                                                                            ensure_str(e.what()));
                                                           }
                                                           catch (...)
                                                           {
                                                           }
                                                       }
                                                       catch (...)
                                                       {
                                                           try
                                                           {
                                                               Output::send(STR("[{}] Unknown exception\n"),
                                                                            ensure_str(action.type == LuaMod::ActionType::Loop ? "LoopAsync" : "DelayedAction"));
                                                           }
                                                           catch (...)
                                                           {
                                                           }
                                                       }

                                                       return result;
                                                   }
                                                   else
                                                   {
                                                       return false;
                                                   }
                                               }),
                                m_delayed_actions.end());
    }

    auto LuaMod::clear_delayed_actions() -> void
    {
        {
            const ActionsLockGuard actions_guard{this};
            m_pending_actions.clear();
            m_delayed_actions.clear();
        }
    }

    auto LuaMod::set_on_unload_callback(int32_t ref) -> void
    {
        m_on_unload_lua_function_ref = ref;
    }
} // namespace RC

#if PLATFORM_WINDOWS
#include <Unreal/Core/Windows/HideWindowsPlatformTypes.hpp>
#endif

#undef UE4SS_LUAMOD_PRINTF

// [EU FIX4 scope repair, root] C-linkage entry for the VMT pump drain:
// the implementation lives in the L23GameThreadProbe namespace structure
// above (its enclosing class owns the queue); this extern "C" trampoline
// gives the UnrealInitializer glue a stable, unmangled symbol.
#if defined(__PROSPERO__)
extern "C" void vmt_engine_tick_drain() noexcept
{
    RC::Unreal::VmtEngineTickDrain();
}
#else
extern "C" void vmt_engine_tick_drain() noexcept {}
#endif
