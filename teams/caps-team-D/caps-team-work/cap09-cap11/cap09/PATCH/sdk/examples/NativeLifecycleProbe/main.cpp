// NativeLifecycleProbe -- the #9 launch mod. Generic: engine class/function
// names only (any UE4/UE5 game), no game-specific name, offset or value.
//
// What it proves on the console (each line is a PASS marker in LAUNCH-CARD.md):
//   [NPROBE] constructed                  -> loader found mbm_mod_entry_v1, start_mod ran
//   [NPROBE] on_program_start             -> lifecycle delivered
//   [NPROBE] on_unreal_init object=...    -> reflected lookup works from a native mod
//   [NPROBE] admission family=...         -> 17 lines: the hook backend admission table, live
//   [NPROBE] hook ClientRestart ...       -> data-only per-UFunction hook fires (map load)
//   [NPROBE] game-thread tick n=1         -> data-only game-thread pump reaches the mod
//   [NPROBE] key F9 speed=2.00 ...        -> keybind + game-thread task + reflected property
//                                            write (player moves at double speed: VISIBLE)
//   [NPROBE] on_update n=1                -> event-loop callback
//   [NPROBE] destroyed                    -> clean uninstall at shutdown (if the game exits cleanly)
#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <atomic>
#include <cstdio>

using namespace RC;
using namespace RC::Unreal;

class NativeLifecycleProbe : public CppUserModBase
{
    std::atomic<uint64_t> m_updates{0};
    uint64_t m_ticks{0};
    int m_hook_hits{0};
    bool m_fast{false};

    static auto family_name(uint32_t f) -> const char*
    {
        static const char* names[] = {"none", "ufunction", "game-thread-tick", "engine-tick", "viewport-tick", "process-event",
                                      "process-internal", "process-local-script", "static-construct", "object-delete", "load-map",
                                      "begin-play", "init-game-state", "call-function-by-name", "process-console-exec",
                                      "local-player-exec", "gnatives-slot", "code-address"};
        return f < sizeof(names) / sizeof(names[0]) ? names[f] : "?";
    }
    static auto route_name(uint32_t r) -> const char*
    {
        switch (r)
        {
        case MBM_ROUTE_DATA_ONLY: return "DATA_ONLY";
        case MBM_ROUTE_DATA_ONLY_PARTIAL: return "DATA_ONLY_PARTIAL";
        case MBM_ROUTE_PENDING_VALUE: return "PENDING_VALUE";
        default: return "REFUSED";
        }
    }

    auto player_pawn() -> UObject*
    {
        auto* pc = UObjectGlobals::FindFirstOf(STR("PlayerController"));
        if (!pc) return nullptr;
        if (auto pawn = pc->Get<UObject*>(STR("Pawn")); pawn && *pawn) return *pawn;
        if (auto pawn = pc->Get<UObject*>(STR("AcknowledgedPawn")); pawn && *pawn) return *pawn;
        return nullptr;
    }

  public:
    NativeLifecycleProbe()
    {
        ModName = STR("NativeLifecycleProbe");
        ModVersion = STR("1.0");
        ModDescription = STR("Generic native C++ mod probe for the PS5 Mod Bridge (#9)");
        ModAuthors = STR("PS5 Mod Bridge");
        Output::send(STR("[NPROBE] constructed\n"));
    }
    ~NativeLifecycleProbe() override { Output::send(STR("[NPROBE] destroyed hook_hits={} ticks={}\n"), m_hook_hits, m_ticks); }

    auto on_program_start() -> void override { Output::send(STR("[NPROBE] on_program_start\n")); }

    auto on_unreal_init() -> void override
    {
        auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR("/Script/CoreUObject.Object"));
        Output::send(STR("[NPROBE] on_unreal_init object={}\n"), object ? object->GetFullName() : StringType(STR("<not found>")));

        for (uint32_t f = 1; f < MBM_HOOK_FAMILY_COUNT; ++f)
        {
            const auto a = Sdk::admission(f);
            Output::send("[NPROBE] admission family={} route={} live={} via={}\n", family_name(f), route_name(a.route), a.live,
                         a.route_name ? a.route_name : "?");
        }

        auto ids = UObjectGlobals::RegisterHook(
                STR("/Script/Engine.PlayerController:ClientRestart"), {},
                [this](UnrealScriptFunctionCallableContext& ctx, void*) {
                    if (++m_hook_hits <= 3)
                        Output::send(STR("[NPROBE] hook ClientRestart post n={} context={}\n"), m_hook_hits,
                                     ctx.Context ? ctx.Context->GetName() : StringType(STR("<null>")));
                },
                nullptr);
        Output::send("[NPROBE] RegisterHook ClientRestart id={}\n", ids.first);

        const bool tick_ok = register_game_thread_tick([this](float) {
            ++m_ticks;
            if (m_ticks == 1 || m_ticks % 1800 == 0) Output::send(STR("[NPROBE] game-thread tick n={}\n"), m_ticks);
        });
        Output::send("[NPROBE] game-thread tick registered={}\n", tick_ok);

        register_keydown_event(Input::Key::F9, [this] {
            const bool queued = Sdk::execute_in_game_thread([this] {
                auto* pawn = player_pawn();
                if (!pawn)
                {
                    Output::send<LogLevel::Warning>(STR("[NPROBE] key F9: no player pawn\n"));
                    return;
                }
                m_fast = !m_fast;
                const float speed = m_fast ? 2.0f : 1.0f;
                const bool ok = pawn->Set<float>(STR("CustomTimeDilation"), speed);
                const auto now = pawn->Get<float>(STR("CustomTimeDilation"));
                Output::send(STR("[NPROBE] key F9 speed={:.2f} write={} readback={:.2f} pawn={}\n"), speed, ok, now ? *now : -1.0f,
                             pawn->GetFullName());
            });
            if (!queued) Output::send<LogLevel::Warning>(STR("[NPROBE] key F9: game-thread task refused\n"));
        });
    }

    auto on_update() -> void override
    {
        if (++m_updates == 1) Output::send(STR("[NPROBE] on_update n=1\n"));
    }

    auto on_cpp_mods_loaded() -> void override { Output::send(STR("[NPROBE] on_cpp_mods_loaded\n")); }
};

extern "C"
{
    UE4SS_EXPORT RC::CppUserModBase* start_mod() { return new NativeLifecycleProbe(); }
    UE4SS_EXPORT void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
