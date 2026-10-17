# LAUNCH9-PREDICTION — the launch-9 sequence re-predicted with the FINAL composition-6 piece set

worker-launch9-prediction · 2026-10-04 · read-only synthesis; no console contact, no nested
subagents, nothing executed. Supersedes `worker-engine-ready-walkthrough/ENGINE-READY-PATH.md`
(which predates Resolution B, D2, the r11 lease, entry r4, the staged-initializer merge and
settings v3; its P1–P6 loader-thread facts and §6 event-loop ordering are reused where the
comp6 manifest does not touch those files — cited as OWNER/BACKEND/PROG per its §0 legend).

All paths relative to `/WORKSPACE/.codex/worktrees/translation-core/what` (`W:`).
Line numbers were read back from the named files in this synthesis (30–50 call budget).

## 0. The final piece set (as assembled)

| Piece | File(s) | What changed |
|---|---|---|
| Bridge r12 | `W:outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp` | configured roots (`kConfiguredGameDirectory="/app0"` `:149`, attach `:636-639`), adapter opt-in set BEFORE conversion `:608-611`, r11 page-quantized lease `:480-493`, B&&D&&E&&(C\|\|C2) gate `:375-389` |
| Adapter + headers + context + program | `W:outputs/2026-10-04-zcode-controller/comp6-integration/` (9 files, hashed in `change-manifest.json`) | Resolution B §2.1–§2.6 + D2 diffs A/B/C (`change-manifest.json` changed_units/changed_headers_staged) |
| r11 page-quantized lease | bridge `:480-491` | launch-8 "unregistered bytes" root cause (final partial page) fixed by outward page clamp |
| Entry r4 | `W:.../worker-module-start-wiring/runtime_module_entry_r4.c` | `[t=<ms>ms pid=<pid>]` freshness prefixes `:190-202`, best-effort failed status `:224-247` |
| Staged-initializer merge (IN FLIGHT, owner worker-initializer-integration) | `W:.../worker-initializer-integration/staging/UnrealInitializer.cpp` (2006 lines; cited as MERGED) | seed consumption slots {0,2,4} `:719-832`, arming gate `:862-877`, ps_scan arm + both DoScan arms gated `:913-922/:1146-1151/:1234-1245`, FName detour verifier skipped `:1385-1397`, tick vtable fallback `:1708-1745`, A-PI honoring gate `:1876` |
| Seed table (final) | `W:.../worker-seed-table-derivation/SEED-TABLE.json` | slots 0/2/4 seeded (`:21-86`), slot 3 GMalloc UNSEEDED `:55-70`, slot 6 by settings `:87-94`, tick convention correction `:95-100` |
| Settings v3 (1448 B, sha `d8faa4ec…` — recomputed on disk) | `W:.../worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini` | `[EngineVersionOverride] MajorVersion=5 MinorVersion=6` `:24-26`; `[Hooks] HookProcessInternal=0 HookProcessLocalScriptFunction=0` `:34-35` |
| Ridealongs | observer `1afa0997…`, qdrv1 `52c32216…` (ridealongs REPORT §1), fc1 validator (seed-table slot-2 validator, SEED-TABLE.json `:53`), IGW-1 exec-write probe (exec-writability REPORT §3: rides the comp6 launch as an appended `= 2` ploader.ini line) | non-interference proven (ridealongs REPORT §3); IGW emits {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL} in the same session |

Merge status honesty: the initializer replacement and settings delta are IN FLIGHT
(`comp6-integration/change-manifest.json` fold_ins: status lines); every MERGED citation
below is against the merge owner's staging copy — the final shipped copy may differ, and
the one place that changes the predicted STOP LOCATION (not the stop family) is called out
as Shape A vs Shape B in §2.

## 1. Answer to the central question, up front

**Does any step still unconditionally require an exec-page READ or WRITE? YES — two, both
inside the init thread, both downstream of everything the piece set fixed:**

1. **READ:** every `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL`/`GET_ADDRESS_OF_UNREAL_VIRTUAL`
   vtable walk ends in `ASM::resolve_function_address_from_potential_jmp`
   (`W:.superpowers/.../port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/
   UnrealVirtualBaseVC.hpp:115,133`) → `get_first_instruction_at_address(function_ptr)`
   (`W:.superpowers/.../port/first/src/ASMHelper/ASMHelper.cpp:57-69`) — a data READ of the
   first instruction bytes AT the resolved virtual function's text address. There is no
   `__PROSPERO__` conditional in that file (grep: zero hits). First walk in init order:
   LoadMap (MERGED `:1659-1667`, `bHookLoadMap` default true, scaffold
   `comp6-integration/UE4SSProgram.cpp:1242`), then Tick (`:1671`), then ProcessEvent
   (`:1817`). This is FLAG-2 (SIGNATURES-INVENTORY `:220-229`): under the trusted
   prot-0x4 record the read faults — the igf6 class.
2. **WRITE:** the EngineTick detour install (MERGED `:1749-1764` `RegisterEngineTickPreCallback`
   → `TDetourInstance::InstallHook` → `PLH::x64Detour::hook`: disassembles up to 100 bytes at
   the target, `MemoryProtector(…, RWX)`, `writeEncoding` — x64Detour.cpp `:322-397`, cited
   SIGNATURES-INVENTORY `:206-215`). FLAG-1 class; never once exercised in-game.

**Where the dependency NOW sits (what the piece set already removed):** D2 diffs A/B/C make
`facts.mapped.text_base=0` the honest absence spelling and drop exec-only spans from the
leased/derived ranges — the scan-class exec reads (ps_scan over text, AOB, G0–G4) are gone
because the ps_scan arm itself is skipped when seeds are armed (MERGED `:913-922`); settings
v3 `:34-35` + scaffold `:1240-1241` make the ProcessInternal/PLSF Zydis block (FLAG-3,
MERGED `:1876`) skip entirely; the seed-bound FName path skips the FName-constructor detour
verifier (MERGED `:1385-1397`, `bSeedBoundFNameCtor` `:616/:819`) — no detour is ever armed
for FName. The FC1 validator and IGW probe ride in the same session to answer the R/W
question empirically (exec-writability REPORT §3–4 decision tree).

**And independent of exec pages entirely:** the piece set leaves REQUIRED slot 3 (GMalloc)
unseeded (`SEED-TABLE.json:55-70`, "under-determined"; consumer skipped, `MERGED :1216-1219`
config gate + `SEED-TABLE.json:102` required_mask_effect), so `PostInitialize` throws
`"UnrealInitializer::PostInitialize: GMalloc is uninitialized."` (MERGED `:396-398`) even in
a world where exec pages are fully readable and writable. Engine-ready is therefore not the
expected outcome of launch 9 under this piece set.

## 2. Ordered hop table (loader thread E*, init thread I*, event loop L*)

Failures surface three ways: loud `MRS_ERR_*` + `[t=… pid=…] modbridge module_start failed:
<stage> code=<n>` + best-effort `failed` status line (entry r4, `:190-202/:224-247`); loud
init throw → `failed(1,<what>)` ack via BACKEND `:59-71` → OWNER `:111-118`; or crash-shaped
module death (igf6 class) with NO failed line and the observer capturing the UE4SS.log tail.

| # | Thread | Step | Pass condition (piece that satisfies it) | Exact honest stop |
|---|---|---|---|---|
| E1 | loader | entry r4 `module_start` → `mfa_activate` (activation preload, loadlist size law) | proven 8×; merged package re-verified host-side incl. 1448-B settings row (settings-bundle-merge REPORT §4–6; INDEX Track 8) | `modbridge module_start failed: mfa_activate_failed code=…` (entry r4 `:296`) |
| E2 | loader | bridge identity gate: fresh VMMAP (r4 16384-entry buffer `:160-163`, r5 walk `:236-259`), window verify B&&D&&E&&(C\|\|C2) (`:340-389`, diagnostics `:360-375`) | in-game proven (igf7 4/4, base 0x400000; rung 1→2 PROMOTED, INDEX `:36`) | identity refusal → `MRS_ERR_CONVERSION` (r12 `:694-697`) |
| E3 | loader | lease registration, r11 page-quantized (`bridge_register_readable_ranges` `:462-493`; clamp outward start-down/end-up `:480-491`; R-only, exec text NOT registered `:449-450`) | FIRST REAL EXERCISE — launch-8 root cause was the unclamped final partial page; D2 Diff A keeps adapter ranges == registrations (D2-DESIGN §5 cross-check) | `Owned read range includes unregistered bytes` / `bridge_lease_reg=0xffffffff` (`:466-468`) — only on regression |
| E4 | loader | conversion (adapter): opt-ins `allow_vm_scan_base_as_module_image_base` + `allow_configured_roots_for_missing_executable_path` (`:608-611`); empty observed path converts UNFABRICATED; D2 Diff A filters `(protection&1)==0` from readable_ranges; `text_base` stays 0 | launch-8 passed conversion before the lease threw; the opt-in removes the last refusal (`GameExecutablePathMissing`, RESOLUTION-DESIGN §2.5); diagnostics `bridge_exepath_source=0` `:435`, `bridge_cfg_gamedir/workdir=1` `:638-639` | conversion error → `MRS_ERR_CONVERSION` with unique what() |
| E5 | loader | Stage 2 context: `MakeNativeProgramContext` → `ValidateNativeProgramContext` (scaffold `Platform-NativeProgramContext.hpp:92-135`): module_file_path lexical+`is_regular_file` (proven launch-8); exe EMPTY ⇒ configured game dir REQUIRED, lexical-only, never stat'd as a file (`:117-118/:132-134`); `/app0` `:149`, mod_root `:637` | Resolution B contract; gate v2 B1/B3 cover the new symbols (INDEX Track 2) | `MRS_ERR_CONTEXT` + unique what() (r12 `:683-692`) |
| E6 | loader | Stage 3 `StartActivatedRuntimeWithAck` (r12 `:649-661`): engine bindings, RuntimeAckConfig, prepare (P1–P3), UE4SSBackend ctor (P4): banner with configured-dir line, no stat of unobserved path (scaffold `:427-433`); `setup_paths` from `configured_game_directory` (scaffold `:774-786`); settings v3 passes `validate_native_services` (v2-prefix verified, settings-v3 REPORT); `start_native_mods` one-shot (P6) → `activated` ack (ue4ss_backend.cpp `:107`) | all P-gates unchanged and previously exercised; layout pair staged (ParsePair-verified, INDEX `:36`) | prepare-time `failed(1,"Native UE4SS construction rejected: …")` (BACKEND `:49-50`) |
| I1 | init | OWNER `start()` → spawn init thread (OWNER `:23-42`); `program_->init()` one-shot guards (PROG `:600-618`) | unchanged by comp6 | thread-spawn throw → `Failed` |
| I2 | init | `setup_unreal`: layout staging from mod_source keys; 5-min StartupWait; **version override consumed** (scaffold `:1011-1028`, settings v3 → Version 5.6 + no-op `version_finder` override); hook config copied (`:1240-1252`: HookLoadMap=true default, PI/PLSF=0 from v3); optional `DoEarlyScan` (`:1259-1275`) hits the same ScanGame early (std::runtime_error swallowed there, resurfaces at the main Initialize call) | settings v3 supplies the override; profile pins 5.6 | uint32-range throw only on malformed ini |
| I3 | init | **ScanGame arming + seed consumption** (MERGED `:836-953`): arming gate `:862-877` — **Shape A** (staged rule, generator requires all 4, `gen_seed_table.py:93/:126` + final table slot 3 absent): loud throw `[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc] …` (`:876`) — TERMINAL, nothing binds. **Shape B** (merge narrows required to {0,2,4}, matching the described piece set): `consume_required_slot_seeds` (`:719-832`) — base check `:724-728`; GUA probe+bind 0x97ba110 `:731-739` (live-validated class); FName pool validation at runtime 0x9765ec0 `:765-804` (data-side u32/u64 reads `:656-685`, retry within `SecondsToScanBeforeGivingUp` `:769-784`, 'None'/'ByteProperty' ASCII match `:786-804`); ctor bind base+0x23d590=0x63d590 + FNAME_Find ByteProperty behavioral probe `:809-821`; SCO bind 0x890aa0 `:827-831`; ps_scan arm + both DoScan arms skipped (`:913-922`, `:1146-1151`, `:1234-1245`) | Shape B pass = first behavioral test of the derived constants; every read is DATA-side inside readable spans | Shape A: the `:876` throw (loud). Shape B: `[Seeds] REFUTED: …` (`:635-641`) — pool never initialized within budget (`:780-781`), probe id divergence (`:813-818`), image moved (`:727`) — terminal per launch, never replayed |
| I4 | init | axes + layout publication: `SetResolvedNativeLayoutAxes` (`:1159-1182`), `InitializeVersionedContainer` → `PublishStagedNativeLayoutMaps` (5.6) → S2 gate → `ArmLayoutGate` (`:1184-1203`); disarm-on-throw wrapper `:1204-1206/:1260-1266` | profile-validated pair (promoted rung; C14 "works-data-side", SIGNATURES-INVENTORY) | publish refusal = loud throw, gate closed first |
| I5 | init | FName detour verifier SKIPPED (`:1385-1397`, `bSeedBoundFNameCtor`); object wait ≥10000 (`:1413-1427` — loaded save holds 71,959); Kismet locations (`:1431-1480`, seed-bound FName); required objects + waits (`:1483-1597`) | all data-side (C2/C11 "works-data-side") | required-objects timeout → warnings `:1568-1579`, later empty-message throw `:1848-1854` |
| I6 | init | **FIRST vtable walk — LoadMap** (`bHookLoadMap` default true, scaffold `:1242`; MERGED `:1659-1667`): instance lookups data-side (`:1610-1657`) then `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, LoadMap, …)` → jmp-probe **exec-page READ** (UnrealVirtualBaseVC.hpp `:115`; ASMHelper.cpp `:57-69`, no PROSPERO branch) | nothing in the piece set removes this read | **predicted stop under trusted prot-0x4**: igf6-class module death, crash-shaped — UE4SS.log ends right after the GameEngine CDO / "Finding GameEngine::LoadMap" lines; runtime-status.txt last line `activated`; NO failed line |
| I7 | init | **Tick resolve + detour install** (`:1668-1767`): vtable route (scan address null with seeds, fallback `:1726-1730`); `RegisterEngineTickPreCallback` ×2 `:1749-1764` → PLH x64Detour: 100-byte disassembly READ + RWX protect + jmp WRITE (x64Detour.cpp `:322-397`) | piece: tick vtable route (data-side bind); the WRITE itself is covered by NOTHING — IGW W_WORKS is the only thing that makes it pass | if reads survived but writes refuse: LOUD `Native game-thread initializer callback registration failed` (`:1750-1753`/`:1761-1764`) → `failed` ack |
| I8 | init | ProcessEvent walk (`:1814-1822`) + ProcessConsoleExec/UStruct::Link walks (`:1823-1840`): same jmp-probe READ class | nothing removes these | same igf6 class if I6 somehow passed |
| I9 | init | TypeChecker + required-objects check (`:1842-1855`) | data-side | `TypeChecker: Was unable to find…` (`:1860`) / empty-message throw (`:1854`) |
| I10 | init | **ProcessInternal block SKIPPED** — gate `:1876` false: settings v3 `:34-35` → scaffold `:1240-1241` → MERGED honoring comment `:1863-1875`; the FLAG-3 Zydis exec read (`:1895-1936`) never runs | piece: settings v3 (both keys REQUIRED 0, SETTINGS-V3-DECISION §1) | none — this hop is now a no-op |
| I11 | init | **PostInitialize GMalloc gate** (`:1952` → `:396-398`): slot 3 unseeded → `config.gmalloc=false` (`:885/:1216-1219`) → GMalloc null → throw `UnrealInitializer::PostInitialize: GMalloc is uninitialized.` | covered by NOTHING in this piece set — only a future derived slot-3 constant flips it | LOUD, `failed(1,<msg>)`; **terminal even if exec R/W fully work** |
| I12 | init | (if GMalloc bound) pools, listeners, `IsInitialized()=true` (`:394-464`), callback GC `:476`, **StaticConstructObject detour** (`:480`) — another exec WRITE at the identity-UNVERIFIED 0x890aa0 | IGW W_WORKS + correct SCO constant | wrong-constant detour = write to a wrong text address (new hazard beyond writability) |
| I13 | init | ack chain: `on_engine_initialized` (PROG `:654-666`) → `engine-ready` + `mods-started` back-to-back (BACKEND `:33-46` → OWNER `:171-173`) | all upstream hops pass | shutdown race → clean stop (PROG `:662-666`) |
| L1 | loop | event loop starts AFTER the acks (PROG `:670-683`); `on_program_start` → `start_lua_mods` → `LuaMod::start_mod` → main.lua top level (6 RegisterKeyBind + pcall RegisterLoadMapPreHook + Loaded log; zero engine calls) | engine-ready walkthrough §6 correction stands (mods-started precedes main.lua) | main.lua load failure is non-fatal, log-only ("Failed to execute main script") |
| L2 | loop | pad pump (`m_input_handler.process_event`) delivers OPTIONS→F7 (PS5Pad.ini staged) | pad provider proven; input path independent of hooks | see §3 |

## 3. The F7 path after mods-started (task 2), with the current piece set

| # | Hop | Prediction | Basis |
|---|---|---|---|
| F7-1 | OPTIONS chord → pad → `InputEvent{F7}` | WORKS (hardware path proven; PS5Pad.ini in activation) | ACTIVATION-PATH §C1; settings-bundle-merge §1 (PS5Pad row in loadlist) |
| F7-2 | callback → `ExecuteInGameThread(toggleMenu)` → EngineTick closure drain | WORKS **only if** I7's detour landed (the closure drain rides the C4 hook; engine-tick-fallback REPORT cross-fallback to ProcessEvent mode exists but needs the ProcessEvent binding, which needs the I8 walk too) | MERGED `:1749-1764`; SETTINGS-V3-DECISION §2; SIGNATURES E7 |
| F7-3 | `showMenu` → `scanItems` → `FindFirstOf("ItemDatabaseSubsystem")` (IS:64) | WORKS data-side IF seeds bound: FName ctor seed-bound (string ctor functional), GUObjectArray seed-bound 0x97ba110 (live-validated). Whether the subsystem INSTANCE exists is content-dependent (loaded save). pcall-guarded: failure logs `Scan failed: …` and continues — silent-nil is designed out by the behavioral probe | ACTIVATION-PATH §C3; SIGNATURES D2/E1; MERGED `:809-821` |
| F7-4 | `buildUI` — PlayerController FindFirstOf (IS:14-24), `StaticFindObject(WidgetBlueprintLibrary)` (IS:499), `Create` (IS:510) w/ `StaticConstructObject` fallback (IS:516), UMG tree, `AddToViewport(9999)` (IS:631) | REFLECTION-dependent: layout maps (S2-gated) + seeded SCO 0x890aa0 (identity-unverified — wrong constant ⇒ silent nullptr per UObjectGlobals.cpp:70-91, or a bad call); UMG classes resident in a running game | ACTIVATION-PATH §C4/T3 |
| F7-5 | spawn chain — `LineTraceSingle` out-param marshalling (first predicted break, B5), class resolve (IS:283-291), `BeginDeferredActorSpawnFromClass`+`FinishSpawningActor` (IS:295-303), property writes | LEAST certain: B5 marshalling + Kismet statics + property writes via layout maps; `RegisterHook`-class detours (E4/E5) remain dead-needs-remedy but main.lua uses none of them | ACTIVATION-PATH §C6/T4; SIGNATURES E2/E3/E8 |

Honest note: under the §2 prediction, launch 9 never reaches L1/L2 — the F7 table is the
contingency map for the launch that gets past I11.

## 4. Ranked remaining failure candidates (exact signatures)

1. **GMalloc gate (structural, terminal).** `UnrealInitializer::PostInitialize: GMalloc is
   uninitialized.` — MERGED `:396-398`; loud `failed(1,<msg>)`. Fires under Shape B even
   with exec pages fully R/W. The one REQUIRED slot the piece set does not resolve
   (SEED-TABLE.json `:55-70`: 0/14 idiom patterns, string anchors absent, DeleteFile-route
   function disassembled — no GMalloc shape; SIGNATURES C13).
2. **Seed-arming gate (Shape A — the code exactly as staged).**
   `[Seeds] Required-slot seed table incomplete: present [g_uobject_array
   fname_ctor_wchar static_construct_object_internal] missing [gmalloc] …` — MERGED `:876`;
   fires at ScanGame entry if the shipped SeedTable.h keeps the current generator's
   all-4 completeness rule (`gen_seed_table.py:93,126`) with the final {0,2,4} table. Same
   root cause as #1, earlier stop, nothing binds (no pool validation, no FName probe).
3. **FLAG-2 exec-page READ at the first vtable walk.** LoadMap (MERGED `:1662`), else Tick
   (`:1671`), else ProcessEvent (`:1817`): `get_first_instruction_at_address` reads text
   (ASMHelper.cpp `:57-69`). Signature: crash-shaped igf6 class — module dies mid-init,
   game survives; UE4SS.log's last line is the GameEngine CDO/"Finding GameEngine::LoadMap"
   vicinity; runtime-status.txt ends at `activated`; observer/qdrv1 records show the tail.
4. **FLAG-1 exec-page WRITE at the tick detour.** `Native game-thread initializer callback
   registration failed` (MERGED `:1750-1753`; the query-dispatch twin `:1761-1764`); loud,
   preceded by the detour's own 100-byte disassembly read (itself candidate #3-class). If
   mprotect-RWX succeeds but the jmp write corrupts, crash-shaped later.
5. **Seed refutation (loud, terminal-per-launch).** `[Seeds] REFUTED: …` variants — pool
   never initialized within the wait budget (`:780-781`), FName probe id ≠ pool id
   (`:813-818`, reset+throw `:815`), base moved (`:727`), GUA probe fail (`:733-736`).
   First in-game test of derived-static-strong constants (slots 2/4 never behaviorally
   verified in-game; SEED-TABLE.json `:51/:84`).
6. **Lease/D2 regression at Stage 1-2 (should not fire; signatures kept for triage).**
   `Owned read range includes unregistered bytes` (lease/D2 mismatch), `Mapped text base
   must be registered readable executable bytes` (must NOT appear post-D2 — its appearance
   means a stale header slipped past the ODR gate; D2-DESIGN §5 table).
7. **Layout publish / S2 gate refusal** after `PublishStagedNativeLayoutMaps`
   (MERGED `:1184-1203`) — loud, gate disarmed first (`:1262-1266`).
8. **Required-objects empty-message throw** — logs "Fatal error! The following objects were
   never constructed:" then throws EMPTY runtime_error (MERGED `:1848-1854`) — blank status
   detail; distinguishable only via the UE4SS.log lines.

## 5. The ONE most likely outcome (honest, no promises)

Launch 9 most plausibly gets FURTHER THAN ANY PREVIOUS LAUNCH and still stops **before
engine-ready**. Specifically: E1–E6 pass (first real exercise of the r11 page-quantized
lease, D2 guards and configured roots — the dry build passed the offline gate 8/8
(worker-comp6-drybuild DRYBUILD-RECEIPT.md) and launch-8 already proved conversion+context
once), the merged initializer consumes the seeds and — if Shape B ships — the FName
ByteProperty behavioral probe gives the project its first in-game validation of the
0x63d590 constant. The session then terminates at the first unconditional exec-page
dependency in init order — the vtable-walk jmp-probe READ (candidate #3, crash-shaped) if
the trusted prot-0x4 record holds for our module's data reads, else the tick-detour WRITE
(candidate #4, loud) — and EVEN IF the IGW probe's same-session records show exec pages are
readable and writable, the walk still ends at candidate #1/#2 (GMalloc unseeded). Which of
these fires first depends on the one in-flight decision this synthesis cannot resolve: the
controller's final seed-arming rule (all-4 ⇒ #2 first; {0,2,4} ⇒ #3/#4/#1 in that order).
The launch's guaranteed products are independent of the stop: first console evidence for
r11+D2+configured roots, the seed constants' first behavioral test, and the
IGW/fc1/observer/qdrv1 records that decide the exec-span approach and the slot-3
derivation for launch 10. Engine-ready, mods-started and the spawner menu remain
unproven-by-prediction; nothing here claims completion or quality of any kind.
