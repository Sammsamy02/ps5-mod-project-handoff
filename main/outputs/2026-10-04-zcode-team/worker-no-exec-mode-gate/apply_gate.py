#!/usr/bin/env python3
"""Apply the U-SWAP-5 no-exec-mode gate to the staging baseline.

Every insertion is an exact-anchored replacement; a missing or ambiguous
anchor aborts with a nonzero exit so a drifted staging copy can never be
silently gated. Produces UnrealInitializer.gated.cpp (the NEXT revision's
candidate, default UE4SS_NO_EXEC_MODE=0 = frozen-unit behavior).
"""
import sys

SRC = "UnrealInitializer.staging-baseline.cpp"
DST = "UnrealInitializer.gated.cpp"

text = open(SRC, encoding="utf-8").read()

def replace_once(old, new, tag):
    global text
    n = text.count(old)
    if n != 1:
        sys.exit(f"ANCHOR FAILURE [{tag}]: {n} matches (need exactly 1)")
    text = text.replace(old, new, 1)
    print(f"applied [{tag}]")

# ---------------------------------------------------------------- NE-0: define
replace_once(
    "namespace RC::Unreal::UnrealInitializer\n{",
    """// =====================================================================================
// [NoExec] U-SWAP-5 compile-time no-exec-mode switch (worker-no-exec-mode-gate,
// 2026-10-04; LAUNCH10-MATRIX U-SWAP-5/R5/R9). Preferred sourcing: the build
// define (-DUE4SS_NO_EXEC_MODE=1) recorded in the run manifest; the in-file
// fallback default 0 keeps THIS file's behavior identical to the frozen merged
// unit until a controller arms it. Rationale for compile-time over a settings
// key: SETTINGS-V3-DECISION section 1 trap 2 (missing/invalid key silently
// keeps the true default = a typo would silently re-enable exec) and the
// FATAL-row console-fragility rule; the v3 ini keys stay complementary.
#ifndef UE4SS_NO_EXEC_MODE
#define UE4SS_NO_EXEC_MODE 0
#endif

namespace RC::Unreal::UnrealInitializer
{""",
    "NE-0 define-before-namespace")

# ------------------------------------------------- NE-1: gate namespace + helper
replace_once(
    "    std::atomic_bool StaticStorage::bUObjectArrayCacheRetired{false};",
    """    std::atomic_bool StaticStorage::bUObjectArrayCacheRetired{false};

    // =====================================================================================
    // [NoExec] gate core. kNoExecMode=false (the staged default) constant-folds
    // every gate site below away: the compiled unit is behavior-identical to the
    // frozen merged initializer. kNoExecMode=true compiles OUT every exec-page
    // READ/WRITE site in this unit (LAUNCH9-PREDICTION I6/I7/I8/I12 classes) and
    // refuses each one LOUDLY; only the data-side seed path binds addresses.
    // There is no runtime re-arm path: no ini key, Lua call, or probe file can
    // make a no-exec build touch an exec page.
    //
    // Refusal policy (never silently half-armed):
    //  - exec features whose absence would strand a caller get a named
    //    "[NoExec] refused:" line that also names the consequence
    //    (game-thread identity, ExecuteInGameThread drain, QueryHarness tick
    //    dispatch, ProcessEvent cross-fallback, RegisterLoadMapPreHook);
    //  - address sources that are ONLY exec-class (legacy ps_scan / Lua AOB;
    //    the FName ctor detour verifier) THROW: with no seeds there is no
    //    data-side fallback, so startup refuses instead of degrading to nothing;
    //  - one post-PostInitialize summary line makes the refused-site count
    //    machine-checkable by the launch log parser (matrix box-2/3 consumers).
    // =====================================================================================
    namespace NoExecGate
    {
        static constexpr bool kNoExecMode = UE4SS_NO_EXEC_MODE != 0;

        // All refusals run on the serialized startup thread (ScanGame ->
        // Initialize -> PostInitialize are single-threaded init, I1-I13);
        // no atomics needed.
        [[maybe_unused]] static unsigned refused_site_count{0};

        // One named line per refused exec site. write_class=true for detour
        // installs (FLAG-1: disasm read + protect + jmp write, x64Detour.cpp
        // :322-397), false for vtable jmp-probe resolves (FLAG-2,
        // UnrealVirtualBaseVC.hpp:115,133 -> ASMHelper.cpp:57-69, no PROSPERO
        // branch) and Zydis derivations (FLAG-3).
        [[maybe_unused]] static auto refuse_exec_site(const CharType* site, bool write_class) -> void
        {
            ++refused_site_count;
            Output::send<LogLevel::Warning>(
                STR("[NoExec] refused: {} (exec-page {} - {} class); not armed this launch\\n"),
                site,
                write_class ? STR("write") : STR("read"),
                write_class ? STR("FLAG-1") : STR("FLAG-2/FLAG-3"));
        }
    }""",
    "NE-1 gate-namespace")

# --------------------------------------- NE-2: maybe_unused on query dispatch
replace_once(
    "    static auto QueryHarnessEngineTickDispatch(",
    "    [[maybe_unused]] static auto QueryHarnessEngineTickDispatch(",
    "NE-2 attr-query-dispatch")

# --------------------------------------------------- NE-3: PostInitialize SCO
replace_once(
    """        Output::send(STR("Hooking StaticConstructObject for searcher pools\\n"));
        Hook::RegisterStaticConstructObjectPostCallback([](auto& data, auto& params) {""",
    """        if constexpr (NoExecGate::kNoExecMode)
        {
            // [NoExec] I12: this registration arms a PLH detour (disasm read +
            // protect + jmp WRITE) at the slot-4 seed constant — code whose
            // identity is UNVERIFIED by design (data-side validation for slot 4
            // does not exist), the one site where even a writability pass would
            // not make it safe (LAUNCH9-PREDICTION I12 "wrong-constant detour"
            // hazard). Compiled out; the slot-4 address stays bound (data-side,
            // inert). Post-init constructions reach the searcher pools only via
            // the GUObjectArray create/delete listeners added above.
            NoExecGate::refuse_exec_site(STR("StaticConstructObject searcher hook (detour at the identity-unverified slot-4 constant)"), true);
            Output::send(STR("[NoExec] consequence: post-init constructions join searcher pools via GUObjectArray listeners only\\n"));
        }
        else
        {
        Output::send(STR("Hooking StaticConstructObject for searcher pools\\n"));
        Hook::RegisterStaticConstructObjectPostCallback([](auto& data, auto& params) {""",
    "NE-3 sco-open")

replace_once(
    """        }, {false, true, STR("UE4SS"), STR("ObjectSearcherPoolHook")});

        if (Version::IsAtLeast(4, 17))""",
    """        }, {false, true, STR("UE4SS"), STR("ObjectSearcherPoolHook")});
        }

        if (Version::IsAtLeast(4, 17))""",
    "NE-3 sco-close")

# ------------------------------------------------- NE-4: ScanGame arming guard
replace_once(
    """        if (bSeedsArmed)
        {
            // Seeded slots leave the required mask; their consumer blocks below
            // skip (SeedArming::consume binds them explicitly, with validation).""",
    """        // [NoExec] Gate precondition. The partial-table throw above kept its
        // unchanged [Seeds] signature (matrix box-2 D1 lookup, row R8); reaching
        // here with !bSeedsArmed means the table is ABSENT, i.e. the legacy path
        // would run ps_scan over text — an exec-page read class this build
        // forbids. With no data-side fallback available the gate THROWS (never a
        // silent degrade-to-nothing).
        if constexpr (NoExecGate::kNoExecMode)
        {
            if (!bSeedsArmed)
            {
                throw std::runtime_error{"[NoExec] REFUSED: no-exec mode forbids the legacy exec-page scan path (ps_scan / Lua AOB); a complete REQUIRED-slot seed table is the only data-side address source. Derive and ship the seed table (SEED-TABLE-FORMAT.md) or build without UE4SS_NO_EXEC_MODE. Startup blocked; nothing bound, nothing exec-side touched."};
            }
            Output::send(STR("[NoExec] mode ON (UE4SS_NO_EXEC_MODE=1): exec-page reads/writes compiled out; the data-side seed path owns every binding\\n"));
        }
        if (bSeedsArmed)
        {
            // Seeded slots leave the required mask; their consumer blocks below
            // skip (SeedArming::consume binds them explicitly, with validation).""",
    "NE-4 arming-guard")

# ------------------------------------------- NE-5: VerifyFNameConstructor guard
replace_once(
    """    auto VerifyFNameConstructor(void* AddressOverride) -> void
    {
        if (StaticStorage::GlobalConfig.bIsForcedPreScan)""",
    """    auto VerifyFNameConstructor(void* AddressOverride) -> void
    {
        if constexpr (NoExecGate::kNoExecMode)
        {
            // [NoExec] The detour-based verifier is code READ+WRITE (FLAG-1,
            // FNAME-VERDICT section 2). Unreachable when seeds are armed (the
            // call site in Initialize() checks bSeedBoundFNameCtor) and the
            // seedless path already refused at the arming guard; kept as a named
            // invariant so a future call-site change cannot silently arm a
            // detour in a no-exec build.
            throw std::runtime_error{"[NoExec] REFUSED: FName constructor detour verification requires a code detour (FLAG-1); the seed-bound behavioral ByteProperty probe is the only legal ctor source in no-exec mode."};
        }
        if (StaticStorage::GlobalConfig.bIsForcedPreScan)""",
    "NE-5 fname-verifier-guard")

# ---------------------------------------------------------- NE-6: LoadMap walk
replace_once(
    """        if (UnrealConfig.bHookLoadMap)
        {
            Output::send(STR("Finding GameEngine::LoadMap\\n"));
            if (auto func_address = OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, LoadMap, GameEngine); func_address)
            {
                Output::send(STR("GameEngine::LoadMap address {}\\n"), func_address);
                UEngine::LoadMapInternal.assign_address(func_address);
            }
        }""",
    """        if (UnrealConfig.bHookLoadMap)
        {
            if constexpr (NoExecGate::kNoExecMode)
            {
                // [NoExec] I6: the OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL walk
                // ends in a jmp-probe exec-page READ (UnrealVirtualBaseVC.hpp
                // :115,:133 -> ASMHelper.cpp:57-69, no PROSPERO branch) — the
                // igf6 crash class. The resolve itself is the read; skipping the
                // whole block leaves LoadMapInternal unbound and nothing touches
                // the text page.
                NoExecGate::refuse_exec_site(STR("GameEngine::LoadMap virtual resolve (vtable jmp-probe read)"), false);
                Output::send(STR("[NoExec] consequence: RegisterLoadMapPreHook cannot arm this launch (Lua callers pcall-guard it)\\n"));
            }
            else
            {
            Output::send(STR("Finding GameEngine::LoadMap\\n"));
            if (auto func_address = OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, LoadMap, GameEngine); func_address)
            {
                Output::send(STR("GameEngine::LoadMap address {}\\n"), func_address);
                UEngine::LoadMapInternal.assign_address(func_address);
            }
            }
        }""",
    "NE-6 loadmap")

# --------------------------------------------------------- NE-7: EngineTick block
replace_once(
    """        if (UnrealConfig.bHookEngineTick)
        {
            Output::send(STR("Finding GameEngine::Tick\\n"));""",
    """        if (UnrealConfig.bHookEngineTick)
        {
            if constexpr (NoExecGate::kNoExecMode)
            {
                // [NoExec] I7: the Tick vtable walk is a FLAG-2 exec-page READ
                // and BOTH RegisterEngineTickPreCallback installs are FLAG-1
                // writes (PLH x64Detour: 100-byte disasm read + RWX protect +
                // jmp write). The whole block is compiled out: no walk, no
                // TickInternal assign, no detour, no vtable display walk.
                NoExecGate::refuse_exec_site(STR("GameEngine::Tick hook (vtable jmp-probe read + x64Detour install, initializer and QueryHarness callbacks)"), true);
                Output::send(STR("[NoExec] consequence: game-thread identity never publishes (IsInGameThread/GetGameThreadId unavailable), ExecuteInGameThread closures never drain, QueryHarness tick dispatch never fires\\n"));
            }
            else
            {
            Output::send(STR("Finding GameEngine::Tick\\n"));""",
    "NE-7 tick-open")

replace_once(
    """#else
            Hook::RegisterEngineTickPreCallback(HookedEngineTick, {true, false, STR("UE4SS"), STR("GameThreadInitializer")});
#endif
        }""",
    """#else
            Hook::RegisterEngineTickPreCallback(HookedEngineTick, {true, false, STR("UE4SS"), STR("GameThreadInitializer")});
#endif
            }
        }""",
    "NE-7 tick-close")

# ------------------------------- NE-8: the eight remaining virtual-resolve blocks
def gate_hook_block(tag, config_field, log_line, site_name, consequence=None):
    old = f"""        if (UnrealConfig.{config_field})
        {{
            Output::send({log_line});"""
    cons = ""
    if consequence:
        cons = f"\n                Output::send(STR(\"[NoExec] consequence: {consequence}\\n\"));"
    new = f"""        if (UnrealConfig.{config_field})
        {{
            if constexpr (NoExecGate::kNoExecMode)
            {{
                // [NoExec] I8-class: vtable jmp-probe exec-page READ.
                NoExecGate::refuse_exec_site(STR("{site_name}"), false);{cons}
            }}
            else
            {{
            Output::send({log_line});"""
    replace_once(old, new, f"{tag}-open")

def close_hook_block(tag, config_field, tail):
    old = f"""            if (auto func_address = {tail})
            {{
                Output::send({{0}});
            }}
        }}"""
    # generic close handled separately below
# The eight blocks all end with the same shape; close each by matching its
# unique assign line + closing braces.

def close_block(tag, assign_line):
    old = f"""                {assign_line}
            }}
        }}"""
    new = f"""                {assign_line}
            }}
            }}
        }}"""
    replace_once(old, new, f"{tag}-close")

gate_hook_block("NE-8a", "bHookInitGameState",
                'STR("Finding GameModeBase::InitGameState\\n")',
                "AGameModeBase::InitGameState virtual resolve (vtable jmp-probe read)")
close_block("NE-8a", "AGameModeBase::InitGameStateInternal.assign_address(func_address);")

gate_hook_block("NE-8b", "bHookBeginPlay",
                'STR("Finding AActor::BeginPlay\\n")',
                "AActor::BeginPlay virtual resolve (vtable jmp-probe read)")
close_block("NE-8b", "AActor::BeginPlayInternal.assign_address(func_address);")

gate_hook_block("NE-8c", "bHookEndPlay",
                'STR("Finding AActor::EndPlay\\n")',
                "AActor::EndPlay virtual resolve (vtable jmp-probe read)")
close_block("NE-8c", "AActor::EndPlayInternal.assign_address(func_address);")

gate_hook_block("NE-8d", "bHookAActorTick",
                'STR("Finding AActor::Tick\\n")',
                "AActor::Tick virtual resolve (vtable jmp-probe read)")
close_block("NE-8d", "AActor::TickInternal.assign_address(func_address);")

gate_hook_block("NE-8e", "bHookGameViewportClientTick",
                'STR("Finding GameViewportClient::Tick\\n")',
                "UGameViewportClient::Tick virtual resolve (vtable jmp-probe read)")
close_block("NE-8e", "UGameViewportClient::TickInternal.assign_address(func_address);")

gate_hook_block("NE-8f", "bHookUObjectProcessEvent",
                'STR("Finding ProcessEvent\\n")',
                "UObject::ProcessEvent virtual resolve (vtable jmp-probe read)",
                consequence="the ExecuteInGameThread ProcessEvent-mode cross-fallback cannot arm")
close_block("NE-8f", "UObject::ProcessEventInternal.assign_address(func_address);")

gate_hook_block("NE-8g", "bHookProcessConsoleExec",
                'STR("Finding ProcessConsoleExec\\n")',
                "UObject::ProcessConsoleExec virtual resolve (vtable jmp-probe read)")
close_block("NE-8g", "UObject::ProcessConsoleExecInternal.assign_address(func_address);")

gate_hook_block("NE-8h", "bHookUStructLink",
                'STR("Finding UStruct::Link\\n")',
                "UStruct::Link virtual resolve (vtable jmp-probe read)")
close_block("NE-8h", "UStruct::LinkInternal.assign_address(func_address);")

# ------------------------------------------- NE-9: ProcessInternal derivation
replace_once(
    """        if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)
        {
            Output::send(STR("Finding ProcessInternal\\n"));""",
    """        if constexpr (NoExecGate::kNoExecMode)
        {
            if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)
            {
                // [NoExec] FLAG-3: GetFuncPtr + jmp-probe + the 164-byte Zydis
                // decode are code reads. Settings v3 already ships both keys = 0
                // (SETTINGS-V3-DECISION); this gate makes the skip unconditional
                // regardless of the ini and names the refusal if it still asked.
                NoExecGate::refuse_exec_site(STR("ProcessInternal/ProcessLocalScriptFunction derivation (GetFuncPtr jmp-probe + Zydis code read)"), false);
            }
        }
        else if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)
        {
            Output::send(STR("Finding ProcessInternal\\n"));""",
    "NE-9 pi-derivation")

# --------------------------------------------------------- NE-10: summary line
replace_once(
    """        PostInitialize(UnrealConfig);
    }
}""",
    """        PostInitialize(UnrealConfig);
        if constexpr (NoExecGate::kNoExecMode)
        {
            // [NoExec] Machine-checkable end-of-startup marker for the launch
            // log parser (matrix section 4 boxes 2-3). Only reached by a healthy
            // degraded run; any guard throw above surfaces as the usual loud
            // failed(1,<msg>) ack instead.
            Output::send(STR("[NoExec] summary: mode=on exec_sites_refused={} engine_ready_data_side=1\\n"), NoExecGate::refused_site_count);
        }
    }
}""",
    "NE-10 summary")

open(DST, "w", encoding="utf-8").write(text)
print(f"wrote {DST}")
