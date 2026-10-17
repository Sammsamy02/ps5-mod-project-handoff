// Host stubs for the cap #4 world_ev simulation. Mac-only test scaffolding: the code under test
// (world_ev, r9_feed_notify_from_index, R9Pacer) is extracted VERBATIM from the patched LuaMod.cpp
// and compiled against these minimal stand-ins for the engine / Lua / index types.
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define STR(x) x

struct lua_State
{
    int top{0};
};
inline auto lua_gettop(lua_State* L) -> int { return L->top; }
inline auto lua_settop(lua_State* L, int n) -> void { L->top = n; }

namespace RC
{
    using StringType = std::string;
    inline auto ensure_str(const std::string& s) -> std::string { return s; }
    inline auto ensure_str(const char* s) -> std::string { return s ? s : ""; }

    enum class LogLevel { Normal, Warning, Error };

    namespace Output
    {
        inline std::vector<std::string> g_lines{};
        template <LogLevel L = LogLevel::Normal, typename... A>
        auto send(std::string_view fmt, A&&... a) -> void
        {
            std::string s = std::vformat(fmt, std::make_format_args(a...));
            if (!s.empty() && s.back() == '\n') { s.pop_back(); }
            g_lines.push_back((L == LogLevel::Warning ? "W " : (L == LogLevel::Error ? "E " : "  ")) + s);
        }
    }

    // ---------------------------------------------------------------- fake engine objects
    namespace Unreal
    {
        enum EObjectFlags : int32_t
        {
            RF_NoFlags = 0,
            RF_ClassDefaultObject = 0x10,
            RF_ArchetypeObject = 0x20,
            RF_BeginDestroyed = 0x8000,
            RF_FinishDestroyed = 0x10000,
            RF_WasLoaded = 0x80000,
        };
        enum { FNAME_Find = 0 };
        struct FName
        {
            std::string s;
            FName() = default;
            FName(const char* c, int = 0) : s(c) {}
            explicit FName(std::string c) : s(std::move(c)) {}
            auto Equals(const FName& o) const -> bool { return s == o.s; }
        };
        struct UClass;
        struct UStruct;
        struct UObject
        {
            UClass* cls{nullptr};
            UObject* outer{nullptr};
            int32_t flags{0};
            std::string name{};
            virtual ~UObject() = default;
            auto GetClassPrivate() const -> UClass* { return cls; }
            auto GetOuterPrivate() const -> UObject* { return outer; }
            auto HasAnyFlags(EObjectFlags f) const -> bool { return (flags & static_cast<int32_t>(f)) != 0; }
            auto GetName() const -> StringType { return name; }
            auto GetNamePrivate() const -> FName { return FName{name}; }
        };
        struct UStruct : UObject
        {
            UStruct* super{nullptr};
            auto GetSuperStruct() const -> UStruct* { return super; }
        };
        struct UClass : UStruct
        {
        };
        struct FUObjectItem
        {
            UObject* obj{nullptr};
            bool unreachable{false};
            auto GetUObject() const -> UObject* { return obj; }
            auto IsUnreachable() const -> bool { return unreachable; }
        };
        inline std::vector<FUObjectItem> g_items(4096);
        struct FUObjectArray
        {
            static auto IndexToObject(int32_t i) -> FUObjectItem*
            {
                if (i < 0 || static_cast<size_t>(i) >= g_items.size()) { return nullptr; }
                return &g_items[static_cast<size_t>(i)];
            }
        };

        namespace UObjectGlobals
        {
            inline std::map<std::string, UClass*> g_paths{};
            template <typename T>
            auto StaticFindObject(void*, void*, const StringType& path) -> T
            {
                auto it = g_paths.find(path);
                return it == g_paths.end() ? nullptr : it->second;
            }
            inline bool g_index_usable{false};      // false until the "delete listener" is proven live
            inline std::vector<int32_t> g_new_idx{}; // objects the next collect reports as new
            inline int g_arm_calls{0}, g_disarm_calls{0}, g_collect_calls{0};
            inline auto r9_index_disabled() -> bool { return false; }
            inline auto r9_index_notify_arm() -> void { ++g_arm_calls; }
            inline auto r9_index_notify_disarm() -> void { ++g_disarm_calls; }
            inline auto r9_index_collect_new(std::vector<int32_t>& Out, std::vector<void*>& OutObj, bool (*)()) -> bool
            {
                ++g_collect_calls;
                Out.clear();
                OutObj.clear();
                if (!g_index_usable) { g_new_idx.clear(); return false; } // dropped, like DeletesSeen == 0
                for (int32_t i : g_new_idx)
                {
                    Out.push_back(i);
                    OutObj.push_back(g_items[static_cast<size_t>(i)].obj);
                }
                g_new_idx.clear();
                return true;
            }
            // The snapshot as the index holds it: every slot "synced" (== already reported or baseline).
            inline std::vector<uint8_t> g_synced(4096, 0);
            inline int g_snapshot_calls{0};
            inline auto r9_index_snapshot_live(std::vector<int32_t>& OutIdx, std::vector<void*>& OutObj, bool (*Want)(UClass*)) -> bool
            {
                ++g_snapshot_calls;
                OutIdx.clear();
                OutObj.clear();
                if (!g_index_usable) { return false; }
                for (size_t i = 0; i < g_items.size(); ++i)
                {
                    if (!g_synced[i] || !g_items[i].obj || g_items[i].unreachable) { continue; }
                    if (!Want(g_items[i].obj->GetClassPrivate())) { continue; }
                    OutIdx.push_back(static_cast<int32_t>(i));
                    OutObj.push_back(g_items[i].obj);
                }
                return true;
            }
        }
    }

    // ---------------------------------------------------------------- fake Lua
    struct CallRec
    {
        int32_t ref;
        int nargs;
        std::vector<void*> objs;
    };
    inline std::vector<CallRec> g_calls{};
    inline int32_t g_current_ref{-1};
    inline std::vector<void*> g_pushed{};
    namespace LuaMadeSimple
    {
        struct Lua
        {
            mutable lua_State L{};
            auto get_lua_state() const -> lua_State* { return &L; }
            auto call_function(int nargs, int) const -> void
            {
                g_calls.push_back(CallRec{g_current_ref, nargs, g_pushed});
                g_pushed.clear();
            }
            auto set_nil() const -> void { g_pushed.push_back(nullptr); }
            auto set_string(std::string_view) const -> void {}
        };
    }
    inline auto get_function_ref_cxx(const LuaMadeSimple::Lua&, int32_t ref) -> void
    {
        g_current_ref = ref;
        g_pushed.clear();
    }
    namespace LuaType
    {
        struct RemoteUnrealParam
        {
            template <typename P>
            static auto construct(const LuaMadeSimple::Lua&, P* pp, const Unreal::FName&) -> void
            {
                g_pushed.push_back(static_cast<void*>(*pp));
            }
        };
    }

    struct LuaMod
    {
        struct LuaCallbackData
        {
            struct RegistryIndex
            {
                int32_t lua_index;
            };
            const LuaMadeSimple::Lua* lua{nullptr};
            void* instance_of_class{nullptr};
            std::vector<std::pair<const LuaMadeSimple::Lua*, RegistryIndex>> registry_indexes{};
        };
        struct StaticConstructCb
        {
            Unreal::FName instance_class_name;
            Unreal::FName instance_class_outer_name;
            int callback_id;
        };
        struct PendingNotifyOnNewObjectCallback
        {
            int callback_id;
            int32_t index;
            int serial;
        };
        static inline std::recursive_mutex m_thread_actions_mutex{};
        static inline std::vector<LuaCallbackData> m_load_map_pre_callbacks{}, m_load_map_post_callbacks{},
                m_init_game_state_pre_callbacks{}, m_init_game_state_post_callbacks{}, m_begin_play_pre_callbacks{},
                m_begin_play_post_callbacks{};
        static inline std::vector<StaticConstructCb> m_static_construct_object_lua_callbacks{};
        static inline std::vector<PendingNotifyOnNewObjectCallback> m_pending_notify_on_new_object_callbacks{};
    };

    namespace refused_log
    {
        inline std::atomic<bool> eager_detour_block_skipped{false};
        inline std::atomic<int> loadmap_state{0};
        inline std::atomic<int> world_route_state{0};
        inline int g_emits{0};
        inline auto emit(const char*, const StringType&, const StringType&, const char*, const char*) -> void { ++g_emits; }
        inline auto mod_name_of(const LuaMadeSimple::Lua&) -> StringType { return "mod"; }
    }

    inline int64_t g_now_ms{1000000};
    inline auto r3_now_ms() -> int64_t { return g_now_ms; }
}
