# agent-FO — 12-mod generality ladder infrastructure (mod-ladder/)

2026-10-08. Offline prep only — console in REST MODE, zero contact, no tree
edits. All new files under `outputs/2026-10-08-l25-prep/mod-ladder/`.

## Deliverables

| File | Content |
|---|---|
| mod-ladder/MANIFEST.md | All 12 staged mods with full sha256 of every main.lua (both staging trees), per-mod API-risk classification (a proven-tonight / b bridged-unexercised / c unknown-risk with probe-first vs binding-gap vs crash-law sub-grades / d harmless) with exact line cites, per-mod ladder verdict, recommended RUN-1..7 order |
| mod-ladder/probe-template.lua | zzladder v1 probe, 4,080 B (< 4,096 B budget), luac -p clean. zzdiag v10 chassis: pcall everything, 6 s T auto-report, +18 s one-shot game-thread run, +45 s WD, F7 (= pad L3+R3) rerun, NO FindFunction/bpfn/class-children, never AddToViewport, no spawn/destroy, P6 hooks+unhooks without waiting. CONFIG line at top selects MOD + probe steps (P0 env; P3+P4 joint TextBlock SetText/GetText read-back + FName FNAME_Find roundtrip + text property Set; P5 Kismet vector math + LineTraceSingle with HitResult struct OUT; P6 RegisterHook/UnregisterHook; P7 ExecuteInGameThreadWithDelay + LoopInGameThreadWithDelay). It is a SEPARATE probe mod riding beside each target — target mods are NEVER edited. |
| mod-ladder/VERDICT-SCHEMA.md | Five-stage per-mod ladder (S1 loads-without-uninstall → S2 keybinds-fire/alternate-trigger → S3 engine-lookups-resolve → S4 capability-probe → S5 full-mod-function) with PASS/FAIL/SKIP semantics, universal + per-mod exact log markers, zzladder marker parse contract, harvest.py per-launch treatment, one-launch-per-mod PS5 discipline (fresh boot, klog tap before launch, clean exits, no rapid launches, rollback = loadlist row removal) |

## Key findings

1. **All 12 candidates exist locally** (the "12 staged" docs are accurate):
   10 stock UE4SS mods in TREE-B LF + TREE-A CRLF copies, plus TownfallMiniMap
   v3.1 (196,743 B) and TownfallModMenu v0.66 (128,009 B) which exist ONLY as
   TREE-A CRLF copies — that is the one staging gap (FINDING recorded in
   MANIFEST: they must be staged + byte-pinned at upload time).
2. **Crash-law surface is clean**: grep over all 12 trees finds ZERO
   FindFunction/bpfn/GetDefaultClass/class-children use — no mod takes the
   auto-SKIP. The verdict mix: 7 RUN (LineTrace, CheatManager, ConsoleEnabler,
   ConsoleCommands-partial, SplitScreen, MiniMap, ModMenu), 2 LOAD-ONLY
   (Keybinds — five C++ dumper globals absent; BPML — no BP caller can exist),
   3 SKIP (BPModLoader pak/Dirs gap; ActorDumper hardcoded 0x98 offset law;
   jsbProfiler debug-hook profiler).
3. **Binding facts verified against the composed runtime** (typescan-r4
   LuaMod.cpp): FindObject :3956, FindObjects :4128,
   RegisterConsoleCommandHandler :3865, LoadAsset :3906 ARE bound (agent-EF's
   list was incomplete); Reflection/ImportText/ContainerPtrToValuePtr are NOT
   (ConsoleCommandsMod `set` deep path = documented binding-gap). Proven set
   expanded on paper: K2_GetActorLocation (FVector return) and
   FinishSpawningActor (FTransform param) upgrade vector math to (a).
4. Order rationale: one NEW marshaling class per boot — RUN-1 LineTrace adds
   struct-OUT HitResult + weak `.Get()`; RUN-2 adds RegisterHook + 2-arg SCO +
   object property Set; RUN-3 adds array-element struct-field Set + FName
   roundtrip; RUN-4 adds console-command handlers; RUN-5 adds player mgmt +
   CTRL chords (PS5Pad.ini rows required first); RUN-6 MiniMap adds the 50 ms
   LIGWD tick + io.open settings + NotifyOnNewObject; RUN-7 ModMenu is
   RegisterHook-saturated and MUST run spawner-free (nav co-fire).
