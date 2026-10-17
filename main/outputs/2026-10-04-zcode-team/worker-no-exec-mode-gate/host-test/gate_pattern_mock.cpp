// Host-side syntax/warning check for the U-SWAP-5 gate hunks.
// Mirrors the exact statement structure inserted into UnrealInitializer.cpp
// (arming guard, SCO wrap, EngineTick wrap, per-hook wraps, PI else-if wrap,
// FName verifier guard, summary line) against stub UE4SS APIs.
// Not engine code; compiled -Wall -Wextra -Werror under gate 0, 1, and undef.
#include <stdexcept>
#include <string>
#include <cstdio>

#ifndef UE4SS_NO_EXEC_MODE
#define UE4SS_NO_EXEC_MODE 0
#endif

using CharType = wchar_t;
#define STR(Text) L##Text

enum class LogLevel { Default, Verbose, Warning, Error };

namespace RC::Output
{
    template <typename... Args>
    auto send(const CharType*, Args&&...) -> void {}

    template <LogLevel, typename... Args>
    auto send(const CharType*, Args&&...) -> void {}
} // namespace RC::Output

struct HookStub
{
    using Id = unsigned;
    static constexpr Id ERROR_ID{0xffffffffu};
};
static HookStub::Id RegisterHook(const CharType*) { return 1; }
[[maybe_unused]] static HookStub::Id (*resolve_target)(const CharType*){};

namespace RC::Unreal::UnrealInitializer
{
    [[maybe_unused]] static auto QueryHarnessEngineTickDispatch() -> void
    {
        Output::send(STR("query harness tick dispatch\n"));
    }

    static bool bSeedsArmed{true};
    static bool bHookLoadMap{true};
    static bool bHookEngineTick{true};
    static bool bHookUObjectProcessEvent{true};
    static bool bHookProcessInternal{true};
    static bool bHookProcessLocalScriptFunction{true};
    static bool bSeedBoundFNameCtor{true};

    // ==== verbatim hunk NE-1 (namespaced to NoExecGate) ====
    namespace NoExecGate
    {
        static constexpr bool kNoExecMode = UE4SS_NO_EXEC_MODE != 0;

        [[maybe_unused]] static unsigned refused_site_count{0};

        [[maybe_unused]] static auto refuse_exec_site(const CharType* site, bool write_class) -> void
        {
            ++refused_site_count;
            Output::send<LogLevel::Warning>(
                STR("[NoExec] refused: {} (exec-page {} - {} class); not armed this launch\n"),
                site,
                write_class ? STR("write") : STR("read"),
                write_class ? STR("FLAG-1") : STR("FLAG-2/FLAG-3"));
        }
    }

    // ==== hunk NE-5 pattern: guard at function head ====
    static auto VerifyFNameConstructor() -> void
    {
        if constexpr (NoExecGate::kNoExecMode)
        {
            throw std::runtime_error{"[NoExec] REFUSED: FName constructor detour verification requires a code detour (FLAG-1); the seed-bound behavioral ByteProperty probe is the only legal ctor source in no-exec mode."};
        }
        if (!bSeedBoundFNameCtor)
        {
            RegisterHook(STR("FNameVerifier"));
        }
    }

    // ==== hunk NE-3/NE-6/NE-7/NE-8 patterns: wrap inside a config-gated block ====
    static auto init_region() -> void
    {
        // NE-3 pattern (SCO): if constexpr refusal / else original
        if constexpr (NoExecGate::kNoExecMode)
        {
            NoExecGate::refuse_exec_site(STR("StaticConstructObject searcher hook (detour at the identity-unverified slot-4 constant)"), true);
            Output::send(STR("[NoExec] consequence: post-init constructions join searcher pools via GUObjectArray listeners only\n"));
        }
        else
        {
            Output::send(STR("Hooking StaticConstructObject for searcher pools\n"));
            RegisterHook(STR("SCO"));
        }

        // NE-6 pattern (LoadMap): refusal + consequence inside if constexpr
        if (bHookLoadMap)
        {
            if constexpr (NoExecGate::kNoExecMode)
            {
                NoExecGate::refuse_exec_site(STR("GameEngine::LoadMap virtual resolve (vtable jmp-probe read)"), false);
                Output::send(STR("[NoExec] consequence: RegisterLoadMapPreHook cannot arm this launch (Lua callers pcall-guard it)\n"));
            }
            else
            {
                Output::send(STR("Finding GameEngine::LoadMap\n"));
                resolve_target = &RegisterHook;
            }
        }

        // NE-7 pattern (EngineTick): refusal naming consequences, else original
        // with a local that is used only inside the discarded branch.
        if (bHookEngineTick)
        {
            if constexpr (NoExecGate::kNoExecMode)
            {
                NoExecGate::refuse_exec_site(STR("GameEngine::Tick hook (vtable jmp-probe read + x64Detour install, initializer and QueryHarness callbacks)"), true);
                Output::send(STR("[NoExec] consequence: game-thread identity never publishes (IsInGameThread/GetGameThreadId unavailable), ExecuteInGameThread closures never drain, QueryHarness tick dispatch never fires\n"));
            }
            else
            {
                const HookStub::Id initializer_callback = RegisterHook(STR("GameThreadInitializer"));
                if (initializer_callback == HookStub::ERROR_ID)
                {
                    throw std::runtime_error("Native game-thread initializer callback registration failed");
                }
                QueryHarnessEngineTickDispatch();
                const HookStub::Id query_dispatch_callback = RegisterHook(STR("QueryHarnessDispatch"));
                if (query_dispatch_callback == HookStub::ERROR_ID)
                {
                    throw std::runtime_error("Query harness tick dispatch callback registration failed");
                }
            }
        }

        // NE-8f pattern (ProcessEvent): refusal + consequence
        if (bHookUObjectProcessEvent)
        {
            if constexpr (NoExecGate::kNoExecMode)
            {
                NoExecGate::refuse_exec_site(STR("UObject::ProcessEvent virtual resolve (vtable jmp-probe read)"), false);
                Output::send(STR("[NoExec] consequence: the ExecuteInGameThread ProcessEvent-mode cross-fallback cannot arm\n"));
            }
            else
            {
                Output::send(STR("Finding ProcessEvent\n"));
                resolve_target = &RegisterHook;
            }
        }

        // NE-9 pattern (PI derivation): if constexpr + else if
        if constexpr (NoExecGate::kNoExecMode)
        {
            if (bHookProcessInternal || bHookProcessLocalScriptFunction)
            {
                NoExecGate::refuse_exec_site(STR("ProcessInternal/ProcessLocalScriptFunction derivation (GetFuncPtr jmp-probe + Zydis code read)"), false);
            }
        }
        else if (bHookProcessInternal || bHookProcessLocalScriptFunction)
        {
            Output::send(STR("Finding ProcessInternal\n"));
            RegisterHook(STR("PI"));
        }

        VerifyFNameConstructor();

        // NE-10 pattern: summary after PostInitialize
        if constexpr (NoExecGate::kNoExecMode)
        {
            Output::send(STR("[NoExec] summary: mode=on exec_sites_refused={} engine_ready_data_side=1\n"), NoExecGate::refused_site_count);
        }
    }

    // ==== hunk NE-4 pattern: arming guard with its own throw ====
    static auto scan_game() -> void
    {
        if constexpr (NoExecGate::kNoExecMode)
        {
            if (!bSeedsArmed)
            {
                throw std::runtime_error{"[NoExec] REFUSED: no-exec mode forbids the legacy exec-page scan path (ps_scan / Lua AOB); a complete REQUIRED-slot seed table is the only data-side address source. Derive and ship the seed table (SEED-TABLE-FORMAT.md) or build without UE4SS_NO_EXEC_MODE. Startup blocked; nothing bound, nothing exec-side touched."};
            }
            Output::send(STR("[NoExec] mode ON (UE4SS_NO_EXEC_MODE=1): exec-page reads/writes compiled out; the data-side seed path owns every binding\n"));
        }
        if (bSeedsArmed)
        {
            Output::send(STR("[Seeds] Required-slot seed table armed\n"));
        }
    }
} // namespace RC::Unreal::UnrealInitializer

int main()
{
    try
    {
        RC::Unreal::UnrealInitializer::scan_game();
        RC::Unreal::UnrealInitializer::init_region();
    }
    catch (const std::exception& e)
    {
        std::printf("REFUSED: %s\n", e.what());
        return 2;
    }
    return 0;
}
