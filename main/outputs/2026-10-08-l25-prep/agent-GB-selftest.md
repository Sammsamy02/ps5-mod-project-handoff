# agent-GB-selftest.md — UE4SS parity self-test (ueselftest v1)

Agent GB, 2026-10-10. Claude round-5 item 6a: REPLACE the 12-mod ladder's
first phase with a game-agnostic UE4SS SELF-TEST. Mac on battery the whole
session: Lua scripting + docs only — no builds, no console contact, no mods
edited. All deliverables under `outputs/2026-10-08-l25-prep/ueselftest/`:

| artifact | what |
|---|---|
| `gen_selftest.py` | generator: curated 167-call list (embedded, every row cites FS's API-PARITY-TABLE.md) -> emits the mod + docs |
| `main.lua` + `part2..part12.lua` | the generated self-test mod (chassis + 167 numbered calls, 12 files) |
| `EXPECTED-MARKERS.txt` | full marker contract (boot/arm/per-call/summary/resume + operator checks) |
| `CALL-SHEET.md` | per-call interpretation sheet (all 167 rows + 27 static skips, citations, semantic-assert notes) |
| `CONSOLE-TEST-PLAN.md` | deploy (13 size-exact loadlist rows + pad row), round-4 ride-along, 3-passes-across-restarts law, opportunistic-only crash-resume drill |
| `validate.lua` | host validation driver (3 scenarios, assertions) |
| `SAMPLE-HOST-LOG.txt` | full 211-line reference host run (167/167) |
| `PS5Pad-round4-proposed.ini` | regenerated pad profile for the round-4 set: adds `L1+R1 = F12` (1736 B, keymap --check 0 violations / 0 warnings) |
| stub extension | append-only `[GB]` section in `host-lua-harness/stub-ue4ss.lua` (no existing line altered) |

## Call count and classification

- **167 numbered calls** (one log line each, pcall'd):
  - Phase A pure-Lua surface 57 (tables, UE4SS/UnrealVersion/FPackageName
    members, FName/FText/FString/FUtf8/FAnsi ctors + full member sets +
    metamethods incl. the three port-only `__tostring`s and the P3
    `FName(name, EFindName.FNAME_Find)` ctor)
  - Phase B thread/time queries 10 (incl. semantic assert `IsInGameThread()`
    == true — the run itself rides ExecuteInGameThread)
  - Phase C engine reads 44 (acquisition via engine base classes ONLY:
    FindFirstOf/FindAllOf/StaticFindObject on PlayerController/GameEngine/
    Default__InputSettings; UObject member surface; IsA both overloads;
    reflected GetName/K2_GetActorLocation/CallFunction/ProcessConsoleExec
    with a no-op unknown command; Reflection/GetProperty; UStruct
    GetSuperStruct/ForEachProperty; UEnum ForEachName; TArray read family +
    the FR-revert NameProperty-userdata shape; native loadfile)
  - Phase D hooks/registrations 26 (RegisterHook+UnregisterHook pair on
    ClientRestart, NotifyOnNewObject with port 2-arg signature, custom-event
    register+unregister, LoadMap pre/post, all 12 detour-family hooks with
    soft-refusal-tolerant PASS, console-command handlers, ForEachUObject,
    IterateGameDirectories, IsKeyBindRegistered (asserts true post-arm),
    RegisterKeyBind arm report, RegisterKeyBindAsync)
  - Phase E one-shot scheduling 9 (all loop forms use `return false`;
    EITWD/REITWD/AfterFrames capture handles)
  - Phase F delayed-action family 14 (ends with mod-scoped
    ClearAllDelayedActions as final cleanup)
  - Phase G write-restore + hygiene 7 (SetPropertyValue and __newindex
    write-SAME-value + read-back verify + restore-rewrite on
    bShowMouseCursor — the RUN-2/5 gap family; ModRef shared-variable
    roundtrip; io roundtrip in the mod's own folder; hygiene audit;
    static-skip accounting)
- **EX=163, PR=4** (SAFE-PRESENCE only: __OriginalReturnValue, MainThread,
  AsyncThread, ue4ss_loaded_modules — surfaces with no safe full exercise).
- **27 static skips** (unnumbered log lines with reasons, full sheet in
  CALL-SHEET): SKIP-WRITE 12 (LoadAsset + all 8 Dump/Generate globals +
  CreateLogicModsDirectory + TArray mutation + ImportText),
  SKIP-CONSTRUCT 1 (StaticConstructObject — no safe teardown from Lua;
  already PROVEN by spawner:50/516), SKIP-CRASH-LAW 1
  (RegisterCustomProperty — offset law), SKIP-DESTRUCTIVE 4 (Restart/
  Uninstall mod family), SKIP-COVERED 1 (UWorld.SpawnActor — the spawner
  owns it), SKIP-NO-INSTANCE 7 (TMap/TSet, UScriptStruct, FWeakObjectPtr,
  FOutputDevice, FURL/FSoftObjectPath/TSoftObjectPtr/UInterface/UDataTable,
  UFunction, ThreadId/LuaCustomProperty — each mapped to its ladder probe
  P2/P5/P6 owner), SKIP-NO-CALLER 1 (RemoteUnrealParam get/set/type —
  reachable only inside a fired hook).

## Runtime laws implemented (all verified on host)

- ONE log line per call: `[ST2] <idx>/<167> <name> PASS|FAIL|SKIP <tail-40>`
  via `print` (port flushes per line); ends `[ST2] SUMMARY <N> of 167 PASS`
  + `[ST2] done`.
- Crash resume: progress marker (index ABOUT TO RUN) written to
  `ueselftest/progress.txt` BEFORE each call; on resume the marker names the
  crasher: `[ST2] <idx>/<167> <name> CRASH (resumed)` and the run continues
  after it. Completed runs write the `-1` marker so the next press is fresh.
  io unavailability degrades gracefully (logged, resume disabled) — the run
  still completes.
- Side-effect hygiene: every property write restores + verifies the
  original; hooks unregister; loops one-shot; nothing constructed; no
  spawns; no save data; the only files touched are progress.txt/st2io.txt
  inside the mod's own folder (st2io removed when os.remove exists).
- Game-agnostic law (A1/A2): only engine base classes; the single
  deployment constant is the modbridge ROOT path (clearly marked, per-title).
- Crash law: no FindFunction/bpfn/class-children anywhere.

## File sizes (console admission law: every file < 4096 B)

`main.lua` 3961 B (chassis + resume + summary + arm + first call) — the
chassis is dense, so N=1 inline entry; the remaining 166 calls load from
`part2..part12.lua` (3646-3869 B, part11 2251 B, part12 3180 B) at run time
via `require("partN")` with `dofile` fallbacks (relative name, then the
canonical `ue4ss/Mods/ueselftest/Scripts/partN.lua` key). NOTE: the task
suggested an "absolute physical path" dofile fallback — that is REJECTED by
the port's native `native_lua_file` (source.read refuses absolute paths,
LuaMod.cpp:1003-1010); the canonical-key fallback is the correct form, and
`require` works TODAY without GA's shim through the composed runtime's
native module searcher (`custom_module_searcher` -> `native_lua_module`,
scripts_path-relative, LuaMod.cpp:1073-1112) as long as each part has a
loadlist row.

## Button-combo arm

`RegisterKeyBind(Key.F12)` — pad chord **L1+R1** per FV's pairs10 ladder
(sha256("FV1|F12") slot 4 = L1+R1, confirmed by running keymap-gen.py: the
generated round-4 profile assigns exactly `L1+R1 = F12`, 0 violations /
0 warnings). NOT L3+R3 (universal mod-menu chord). The arm runs the test on
the game thread via ExecuteInGameThread; the round-4 spawn launch rides the
same session: user does the spawn test FIRST, then presses L1+R1 (saving a
launch — CONSOLE-TEST-PLAN section 3). Deploying the chord requires the
regenerated PS5Pad.ini (1736 B vs live 3718 B) + MFA loadlist config-row
re-pin — exact commands in the plan.

## Host validation results (Mac Lua 5.5.1)

- `luac -p` clean: all 12 mod files + validate.lua + extended stub.
- `validate.lua` — **ALL SCENARIOS PASS**:
  - S1 (stub io=on, GA path shim, staged `ue4ss/Mods/ueselftest` tree):
    `[ST2] SUMMARY 167 of 167 PASS`, zero FAIL/SKIP lines, `done`, io
    roundtrip host shape, both write-restore entries PASS with "restored",
    stub-level assert that every recorded property write ends in the same
    value (originals restored), RegisterHook/UnregisterHook PASS, hygiene
    audit clean, 27 static-skip lines, zero missing globals, and a SECOND
    arm press reproduces 167/167 with no part re-append (re-entry guard).
  - S2 (crash-resume drill, in-memory io, progress seeded "5"):
    `[ST2] 5/167 EObjectFlags.RF_Public CRASH (resumed)`, entries 1-4 not
    re-run, resume at 6, `SUMMARY 162 of 167 PASS` (= M - 4 prior-session -
    1 crashed), progress file ends at the -1 completed marker.
  - S3 (io=off): exactly the 2 documented FAILs (os.clock, os.date) + 1
    SKIP (io-roundtrip no-io-path), `SUMMARY 164 of 167 PASS`.
- Official `runner.lua`: io-on **LOADS-CLEAN, 0 missing globals**, all 11
  parts required, keybinds/hooks/console-handlers fired with 0 errors;
  io-off LOADS with gap-reads io+os only (the deliberate P0 probes).
- Stub-extension neutrality: full `run-all.sh` calibration re-run — **27/27
  verdicts byte-identical** to the pre-extension snapshot (all 13 subjects
  x 2 io modes, incl. the TownfallItemSpawner console-copy calibration
  law). The extension only ADDS what FS's parity table documents as bound
  (40 present() globals incl. the delayed-action family, detour-hook
  families, LoadExport, UnregisterCustomEvent, CreateLogicModsDirectory,
  thread-id/is-in-thread, Restart/UninstallMod, FUtf8String/FAnsiString,
  capability globals) plus additive wrappers (FName/FText/FString member
  sets, TArray/FPackageName members, bShowMouseCursor boolean property,
  prop-write recording, registry-aware IsKeyBindRegistered, game-thread
  flag, handle returns, ModRef shared store, NotifyOnNewObject port
  signature superset).

## Coordination notes

- GA's path shim (`host-lua-harness/path-shim.lua` + stub io routing)
  LANDED mid-task; the self-test is shim-compatible by design: on host the
  io-roundtrip PASSes with detail `w-only open-r-miss` (virtual-store
  semantics), while on console it must read `rw roundtrip` — an explicit
  operator check in EXPECTED-MARKERS (the one place host and console
  interpretation differ).
- Interlock with round-4: the WAKE-RUNBOOK §10 step 4 already anticipates
  "if the UE4SS self-test (agent GB) made it into the build window, press
  its combo" — the deploy is Lua-only (no PRX change), so it fits the same
  launch window whenever ops stage it.

## Not done / out of scope (by law)

No console contact, no builds, no edits to any real mod or zzdiag, no
changes to GA's path-shim.lua, no engine/runtime source changes. The three
console passes, the pad-row re-pin and the opportunistic crash-resume
validation remain for the next AC-power console window per
CONSOLE-TEST-PLAN.md.
