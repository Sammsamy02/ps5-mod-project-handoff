# Handoff for a new Claude chat: PS5 Mod Bridge (state as of 2026-10-12 ~21:00 UTC)

You are taking over from a Claude Code session. Read this file first. Then read `CLAUDE.md` in the repo root
(binding rules) and the memory notes (`~/.claude/projects/-Users-developer-Documents-Codex-2026-09-29-what/memory/MEMORY.md`).
- **Repo / worktree:** `/WORKSPACE/.codex/worktrees/translation-core/what/` (branch `codex/experiment-native-transports`)
- **Main working folder:** `outputs/2026-10-08-l25-prep/` (all paths below are relative to the repo unless they start with `/`)


## 000. UPDATE 2026-10-13 ~02:00 UTC (newest)

- TOWNFALL STEP 1 DONE. Launch 13 (R10b again) and launch 14 (R11 `bfd95db6`) were both clean. User: "EVERYTHING IS PERFECT".
  Report: `evidence/launch-l26-R11-launch14-20261013/REPORT-l26-R11-launch14-20261013.md`. R11 is installed and is the
  rollback.
- Known, later: custom textures look too bright / wrong in every launch. That is pak work (capabilities #7/#8/#12).
- Open perf item: watched empty slots still cost ~20-60 ms/s after a level change. The proper fix is an engine
  create-listener (two-source address, generic).
- NEXT: capabilities 1-12, one launch each. ChatGPT prep is in outputs/2026-10-12-caps-prep-chatgpt/.

## 00. UPDATE 2026-10-13 ~01:15 UTC (newest; read first)

- Launch 12 (R10b `dc2d570c`): CLEAN for mods, no stutter, self-test 161/168. B7 proven (`[LUA-CPP] mode=cxx-throw`).
  Report: `evidence/launch-l26-R10b-launch12-20261012/REPORT-l26-R10b-launch12-20261012.md`. It is the rollback.
- Two issues found there, fixed in **R11** (`bfd95db6`), which is built, frozen, verified and reviewed (PASS):
  `outputs/2026-10-08-l26-frozen/R11-watch-world-uenum-bfd95db6/` (REVIEWER-SUMMARY-R11.md).
  1. Freed-slot re-check made every lookup ~2 ms after a level change: [R11-WATCH].
  2. The hooks route refused at 60 s: [R11-WORLD], it now waits.
  3. UEnum::Names is back (81 rows): [R11-UENUM]. All 5 getters are private; the override names GetNames415's type.
- NEXT: launch 13 = R11 (`prep_modded_launch.py --variant A --prx-path <ABSOLUTE path to the R11 prx>`; a relative
  path fails). If clean, Townfall step 1 is done (2 clean launches), then capabilities 1-12.
- Review follow-ups for the build after R11: `round11-staging/review/REVIEW-R11.md` (FIX-SOON #4: a louder pending
  signal).

## 0. UPDATE 2026-10-12 ~23:20 UTC (read this first; it supersedes section 3's "next steps")

- **Round 10 is merged and built.**
  - R10 (`06913711`) failed launch 11 at startup: `Native accessor width unavailable: UEnum::Names`; report in
    `evidence/launch-l26-R10-launch11-20261012/`.
  - **R10b `dc2d570c…` is frozen and verified and is the NEXT LAUNCH:** `outputs/2026-10-08-l26-frozen/R10b-fields-fix-dc2d570c/`
    (reviewer summary inside).
  - It has not been installed yet. R10 is still on the console.
  - Install with `prep_modded_launch.py --prx-path ../2026-10-08-l26-frozen/R10b-fields-fix-dc2d570c/modbridge_supplier_composition.prx`
    only when the user says "ready" (2-minute start window).
- **First log check on launch:**
  - `[LUA-CPP] mode=cxx-throw err_status=2 err_dtor=1 yield_status=1 yield_dtor=1 void_catch=1 swallowed=0`. Anything else = roll back to B6n.
  - `[FIELDS] staged layout facts: members=79`.
- **New pre-compile gate** `harvest/layout_binder_gate.py`, wired into `run_labelled_build.sh`: it runs the runtime
  binder's startup checks on the Mac.
- **Build script resume:** `RESUME_AT_PACKAGE=1` reruns steps 5-9 when compile passed and packaging failed.
- **Builds need** `L26_LUA_AS_CPP=1` from now on (B7).
- **Packaging driver** (`outputs/2026-10-08-l26-runtime/l26_packaging_driver.py`) accepts exactly `-setjmp -longjmp`
  when `L26_LUA_AS_CPP=1`.
- **Open items:**
  - (a) `UEnum::Names` needs an active-width override in `outputs/2026-10-08-l26-package-A/lineage/NativeLayoutActiveWidths.hpp`
    (genuine 5.6 getter), then re-add `[UEnum] Names=0x40 UEP_TotalSize=0x68`. The enum API stays fail-closed until then.
  - (b) A real frame-time measure.
  - (c) The ChatGPT/Codex capability prep (`outputs/2026-10-12-caps-prep-chatgpt/`).

## 1. Who the user is and how to work with them

- The user is not a programmer. Give short, plain answers. They relay reports to their own reviewer (a claude.ai chat).
- They have given a **blanket OK for console actions at launch time**: "u already have my ok for everything just
  when its launchtime give summary of stuff u did so i can send it to my own reviewer while i playtest". "u launch
  it when its done." You still follow the CLAUDE.md order of operations.
- **Always ask "is the game closed?" before reading console logs.** Reads during gameplay come back incomplete.
- **Speed mode:**
  - Work directly.
  - Runtime code that ships to the PS5 gets ONE independent reviewer agent; small scripts get a self-check.
  - Never make a build wait on background work.
  - The user explicitly asked to use parallel agents and workflows to go faster ("LITERALLY USE ALL UR AGENTS").
    Workflows are opted in, but keep the Mac safe: it has 16 GB, and only one PRX build runs at a time.
- **Build policy (latest):**
  - No preemptive or intermediate builds. Make all changes for the next launch, run the gates, build ONCE, launch.
  - The last clean launched build is the rollback.
  - A split/bisect build only if a launch fails and the logs don't show why.
- **No launch for a single small change:** fold it into the next planned launch.
- **Reports:** one markdown report per launch (template: section 7 of CLAUDE.md), plus a short paste-ready summary
  for the user's reviewer at each launch. Never put the console IP in reports for others.

## 2. Goals and order (user decisions, 2026-10-12)

**Goal:** unmodified PC mods run on a jailbroken PS5 exactly as downloaded. Support is per engine (~95% of its
games), never per game or mod. Test game: **Silent Hill Townfall** (UE 5.6.1, title PPSA33286).

**Order:**
1. **Finish Townfall.** Done = 2 clean launches: all mods working (including the minimap), no stutter, the
   self-test runs once per launch.
   - Next = ONE combined launch carrying, in this order:
     - the stutter fix (B6q pacing, already in the candidate);
     - **B7** (compile Lua as C++ so Lua errors unwind properly);
     - the **map-load / InitGameState / BeginPlay / EndPlay** hooks through data-only routes;
     - the **15 grandfathered engine fields**: verified from two sources and added to the layout data, with the
       field gate tightened to zero grandfathered reads (Stellar Blade is UE4, so hard-wired offsets would be wrong);
     - the **IsA / StaticFindObject by-name index** (a speed target: misses currently walk all 150k objects).
2. **Capabilities 1-12, one launch each, each proven on Townfall.**
   - #7, #8 and #12 = paks (the Leon outfit mod); #9 = UE4SS C++ mods; #11 = UE4SS GUI windows.
   - Kernel #1/#3 keep the 3-passes-across-restarts rule. #10 (C#) is excluded and moves to the Unity phase.
   - ChatGPT/Codex is doing the Mac-side prep now (section 6).
3. **Stellar Blade (UE4)** as a fully blind test: Lua mods, outfit paks, CNS. Zero Stellar Blade-specific code or
   hand-written values. **No Stellar Blade prep before step 3.**
4. Then other engines, then the mod manager and special-case kit.

## 3. Where things stand right now

### Launch history (2026-10-12; evidence in `outputs/2026-10-08-l25-prep/evidence/launch-l26-<build>-launch<N>-20261012/`, each with REPORT-*.md)

| Launch | Build (PRX sha prefix) | Result |
|---|---|---|
| 6 | B6g object index (`0d14ef38`) | Clean (Townfall clean launch #1) |
| 7 | B6j NotifyOnNewObject (`144a89b2`) | Mods worked; elevator stutter |
| 8 | B6m (`079c30b7`) | **Failed:** read UClass::ClassFlags and FUObjectItem::SerialNumber, which are not in the layout data, so mods broke. This led to the automatic **field gate**. |
| 9 | B6n proven fields (`348b4e0d`) | All mods worked, minimap perfect; stutter in the new area after the elevator: freed-slot re-check cost ~4 ms/frame |
| 10 | B6p watch budget (`852d72ce`) | All mods worked. Heavy stutter: the game-thread pump runs on game **event carriers** (~800-2900 calls/s, NOT once per frame), so the per-call 0.5 ms budget multiplied to ~650 ms/s after a 30k-object unload. Fix = B6q time-window pacing (in the candidate, reviewed SHIP, not built). |

- **Rollback build:** B6n `348b4e0d…` (`outputs/2026-10-08-l26-frozen/B6n-proven-fields-348b4e0d/`). The last fully
  clean launch was B6g.
- **On the console now:** B6p (`852d72ce`).
- B6o (`22b511bf`) was built but never launched; it is archived.

### The candidate (runtime source): `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`

It contains everything through **B6q** (time-window pacing in `src/LuaMod.cpp`, R9Pacer). Change ledger:
`outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json` (rows `b5…b6p_row_sync`; add `b6q`+ when you build).

### In-flight parallel work (round 10)

- A workflow ran 4 tracks from one frozen base copy, `outputs/2026-10-08-l25-prep/round10-staging/base-tree/`
  (shas: `round10-staging/BASE-SHA256.txt`; base = the candidate with B6q).
- Each track has its own folder with `tree/` (patched copy), `apply_<track>.py` (exact-once patch for the candidate;
  takes a root argument), `delta.diff`, `DESIGN.md`, and its syntax logs:
  - `round10-staging/b7/`: Lua as C++, plus `build-tool/` (staged build_variants change, NOT applied). Implemented and reviewed.
  - `round10-staging/hooks/`: map-load/BeginPlay/... First review BLOCK, fixed, re-review was running.
  - `round10-staging/fields/`: 15 fields plus the new strict `field_gate.py` (staged copy). Still implementing at handoff time.
  - `round10-staging/isa/`: by-name object index. Implemented and reviewed.
- Workflow run id `wf_8e2876c1-9db`. Its journal (each agent's final report and the reviewer verdicts):
  `/WORKSPACE/.claude/projects/-Users-developer--codex-worktrees-translation-core-what/916ce04c-e506-427f-9525-afe2b8b9084b/subagents/workflows/wf_8e2876c1-9db/journal.jsonl`.
  It cannot be resumed from a new chat. If a track is unfinished, read its folder and finish or redo that track.

### Next steps

1. Confirm all 4 tracks are SHIP: read each folder's review result from the journal, or re-review with one agent.
2. Apply to the candidate **in this order**: fields → isa → b7 → hooks. Each apply script must hit every anchor
   exactly once; fix any merge conflicts by hand. Apply the staged b7 build-tool change and the strict field gate
   (`round10-staging/fields/field_gate.py` → `harvest/field_gate.py`) and the compiled layout declaration edit from
   the fields track.
3. Checks:
   - `python3 outputs/2026-10-08-l25-prep/harvest/syntax_check_tree.py` (PS5 syntax check, default 6 units; add
     `--unit name=src/…` for ScriptHookThunks etc.)
   - `python3 outputs/2026-10-08-l25-prep/harvest/field_gate.py`
   - One independent reviewer agent on the **merged** diff.
4. Register log markers: add a set to `harvest/add_build_markers.py` and run it. `verify_build` checks the markers
   are in the PRX.
5. Build once: `cd outputs/2026-10-08-l25-prep && mkdir -p evidence/builds-20261012/<LABEL> && (L26_BUILD_JOBS=4 nohup bash harvest/run_labelled_build.sh <LABEL> B6p-watch-budget > evidence/builds-20261012/<LABEL>/run.log 2>&1 &)`
   - Labels are single-use.
   - Never edit the candidate during a build.
   - A killed build can leave `staging-copyin-probe` missing. Restore it from `outputs/2026-10-08-l26-build-variants/archive-r6/<latest>/`.
6. Freeze and verify:
   - Copy `2026-10-08-l26-package-A/pass-2/modbridge_supplier_composition.prx` + `.symbols.json` + `FINAL-RECEIPT-VARIANT-A.json` + the snapshot into `outputs/2026-10-08-l26-frozen/<LABEL>-<sha8>/`.
   - Run `bash harvest/verify_build.sh <frozen prx> --manifest harvest/markers-declared.json 2026-10-08-l26-package-A`; it must PASS.
7. **Install + arm** (user's blanket OK at launch time; tell them right after):
   - First kill the previous klog tap and eboot monitor (pids under "- pid:" in the newest `harvest/LAUNCH-READY-A-*.md`).
   - Then: `cd outputs/2026-10-08-l25-prep && python3 harvest/prep_modded_launch.py --variant A --prx-path ../2026-10-08-l26-frozen/<dir>/modbridge_supplier_composition.prx > evidence/deploy-launchN-20261012/prep-<ts>.log 2>&1`
   - It backs up, uploads, does the decrypt-OFF readback (full sha + magic 4f153d1d), runs the loadlist row-check
     and arms the ploader. Expect `PASS 32 FAIL 0` in the newest `harvest/LAUNCH-READY-A-*.md`.
   - **The user must start the game within ~2 minutes** of arming. Tell them the time; re-arm if it's missed.
8. **Route for the user:** all mods on (including the minimap), ride the elevator, a few minutes in the new area,
   change speed a few times, self-test once, close the game, say "closed".
9. **Harvest after "closed":**
   - `python3 harvest/harvest.py --label l26-<LABEL>-launchN-20261012`
   - Copy `harvest/harvest-<label>-*` and `HARVEST-<label>.md`, plus the newest klog and eboot-monitor files from
     `evidence/runtime/l25-variant-A/`, into `evidence/launch-<label>/`.
   - Decode: `python3 harvest/decode_session_log.py <ue4ss.log> <dir>/decoded/ue4ss.decoded.log`. Logs are
     NUL-interleaved, so never plain-grep the raw files.
   - Check:
     - 0 `doesn't exist in this engine version`, 0 `[R9-GUARD]`, 0 `LUA_ERRRUN`
     - `[ST2] SUMMARY` (161/168 is the current level)
     - `[R9-IDX] … direct-slots=1`
     - per-second `[PERF]` lines. Remember `frames=` counts pump calls, not real frames. The keys are
       `pump_ms`, `idx_ms`, `idx_watch`, `idx_cut`, `run_max_ms`.
   - The klog line `CRASH KILL_END … pid[0xffffffff]` is the normal app-close event, not a crash.
10. Write `REPORT-<label>.md`. If clean, it is Townfall clean launch #2; then capabilities 1-12.

## 4. Key tools and gates (`outputs/2026-10-08-l25-prep/harvest/`)

- `run_labelled_build.sh`: the 9-step build. Pre-compile gates: fs_throw_gate, no_detour_gate, **field_gate**.
  Then build, package, PRX identity, verify_build, a post-compile fs gate. Also an import gate (G7: no new
  imported symbols; prefer `__builtin_readcyclecounter`, `__atomic_*`).
- `field_gate.py`: lists every engine field the runtime reads, including through helper functions and direct
  `X::MemberOffsets.find(STR("M"))` reads. It fails on any read not in the proven layout data, prints counts, and
  fails on zero. Proven data:
  - `outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp`
  - `evidence/morning-readonly-20261012T114008Z/spool/rows/ue4ss/MemberVariableLayout.ini`
  - Proof runs: `evidence/field-gate-20261012/`
- `syntax_check_tree.py --root <tree>`: PS5 -fsyntax-only on any tree.
- `add_build_markers.py <SET>`: markers verify_build looks for (sets B2…B6p).
- `prep_modded_launch.py`, `prelaunch_rowcheck.py`, `harvest.py`, `decode_session_log.py`, `launch_report.py`.
- Console mod set and pins: `outputs/2026-10-08-l25-prep/launch3-20261012/rowcheck-pins.json`. Mods:
  CheatManagerEnablerMod, ConsoleEnablerMod, SpeedHackMod (pad combos in PS5Pad.ini), TownfallItemSpawner,
  TownfallMiniMap, ueselftest, zzdiag v12. The loadlist has 30 rows.

## 5. Runtime facts you need

- **PS5 limits:**
  - Game code is execute-only, so no detours; hooks are data-only (vtable / function-pointer swaps).
  - The engine tick hook is unavailable. The game-thread pump runs on 3-4 "R3 FuncPtr-marshal event carriers" whose
    call rate depends on gameplay, so **every per-call budget must be time-paced** (R9Pacer, ~16 ms windows).
- **Engine hash tables are unavailable on PS5**, so lookups walk GUObjectArray. Our object index (`[R9-IDX]` in
  `src/UObjectGlobals.cpp`) replaces the walk for FindFirstOf/FindAllOf. It is fed by the engine delete listener.
  New objects append or reuse freed slots; freed slots are watched (FreeWatch, InWatch de-dup, direct slot reads).
- **NotifyOnNewObject** is delivered from the index (budgeted, contained per callback).
- **Lua state:** compiled as C today, so errors longjmp and skip destructors (the reason for B7). Lua memory is on
  GMalloc.
- **Values that ship need two sources.** Missing values fail closed. Mod files are never edited (diagnostics go in zzdiag).
- **Remaining PC-timing gaps:** IsA ~0.5 ms per miss (the isa track), property/method calls ~13 µs, index query ~3.5 ms.
- **Known self-test FAILs** (the same 5 every launch): uobj.CallFunction, uobj.ProcessConsoleExec, RetriggerableEITWD,
  ExecuteInGameThreadAfterFrames, LoopInGameThreadAfterFrames-one-shot. 26 `[REFUSED]` hooks (map-load etc.; the hooks track).
- **Refused-hooks analysis:** `outputs/2026-10-08-l25-prep/round8-prep/refused-hooks/REFUSED-HOOKS.md`.
  **B7 plan:** `round8-prep/b7-lua-cpp/B7-PLAN.md`.

## 6. What ChatGPT/Codex is doing in parallel

- **Handoff given:** `outputs/2026-10-08-l25-prep/HANDOFF-CAPS-1-12-for-ChatGPT.md`.
- **Task:** Mac-side prep for capabilities 1-12, starting with #7 (paks, no game file changed), then #8, #12, #9, #11.
- **It works only in** `outputs/2026-10-12-caps-prep-chatgpt/`. It never touches the console, never builds, never edits
  the candidate, harvest/, round10-staging/ or the frozen builds, and hands back staged patches plus `HANDBACK.md` for
  Claude to merge.
- **Capability status table:** `outputs/2026-10-08-l25-prep/CLAUDE-CAPABILITY-STATUS.md` (2026-10-10; summary in the
  ChatGPT handoff, section 3).

## 7. Things not to forget

- Speed changes in SpeedHack can trigger big object unloads. Test the route with speed changes plus the elevator.
- `[PERF] frame_max_ms` is the gap between pump calls, not the real frame time. A real frame-time measure is still an
  open item.
- Field-gate grandfathered list: 15 fields (`evidence/field-gate-20261012/B6n-fields.txt`). The fields track should
  end it.
- The create-listener (per-class ClassConstructor swap) idea is **parked**: design only, ask before building.
- The reviewer suggested skipping a separate no-mods launch unless timing is ambiguous.
