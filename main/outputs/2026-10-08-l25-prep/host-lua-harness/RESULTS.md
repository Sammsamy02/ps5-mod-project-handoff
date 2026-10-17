# RESULTS — host Lua harness for the 12-mod ladder (+ calibration subject)

Agent FU, 2026-10-10. Mac-local Lua 5.5.1 only (`lua5.5`); no builds, no
console contact, no mod edits. Companion to `mod-ladder/MANIFEST.md` (FO) and
`mod-ladder/STATIC-SCAN.md` (FT, landed mid-task and consumed below).

## How to reproduce

```
cd outputs/2026-10-08-l25-prep/host-lua-harness
./run-all.sh                 # 13 subjects x {io-off, io-shim-on} -> runs/*.log
lua5.5 runner.lua --main <main.lua> --name X --shared <Mods/shared dir> [--with-io]
```

## Method

- **Stub** (`stub-ue4ss.lua`): every port-registered global from the composed
  console runtime `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`
  (grep of `register_function`/`make_global` this pass) is a RECORDING stub.
  UObject-ish values are one unified chainable stub ("cell"): any method call
  is recorded and returns a fresh stub; any property read returns a stub
  (X/Y/Z/W/Roll/Pitch/Yaw read 0.0; array indices 1..4 return stubs, then nil);
  property writes are recorded and persisted for read-back. Fidelity-relevant
  methods are special-cased (IsValid/GetName/GetFullName/ToString/GetClass/IsA/
  IsClass/HasAnyInternalFlags→false/GetAddress/GetFName/type/get/set/Log/Get/
  GetSuperStruct→terminates/GetSuperClass→terminates/LineTraceSingle→fills the
  OUT HitResult table). `CreateInvalidObject` returns an IsValid=false object.
  FName/FText constructors, NAME_None with FName `__eq`, Key/ModifierKey/
  EObjectFlags/EInternalObjectFlags/EFindName/PropertyTypes auto-enums,
  UnrealVersion (UE 5.6 per STATE-L26-WAVE.md:257), UE4SS, ModRef,
  FPackageName (LuaMod.cpp:7827), FVector/FRotator/FTransform pass-throughs.
- **Runner** (`runner.lua`), one OS process per subject per mode (full
  isolation): compile → run startup chunk → fire every keybind callback once
  (incl. RegisterKeyBindAsync) → fire every RegisterHook callback once →
  fire every console-command handler once with sample args → fire LoadMap
  pre/post + BeginPlayPost hooks once → fire NotifyOnNewObject callbacks once.
  Custom events are never fired (no BP caller can exist on console).
  `Execute*` stubs invoke immediately (depth cap 24); `Loop*` stubs record and
  never loop (spec rule). Sandbox `require` resolves UE4SS-style search paths
  (mod Scripts dir, mod root, `Mods/shared/<name>[/]<name>.lua]`, shared
  subdirs); modules load with the same sandbox env. Watchdog:
  20 s wall / instruction-count hook — it caught a real infinite loop during
  calibration (see dump_object note).
- **Modes**: `io-off` (io/os absent — the conservative port hypothesis) and
  `io-shim-on` (recorded io/os stubs). IMPORTANT reconciliation: FT's
  STATIC-SCAN §0.1.3 shows the port actually calls plain `luaL_openlibs`
  (patched Sony liolib; linit opens io, os, debug) — so **io-shim-on is the
  mode that matches the composed source**; io-off brackets the failure case if
  the console P0 probe (`type(io)`) ever disagrees. Both are reported.
- **Calibration law upheld**: the KNOWN-GOOD console copy
  `evidence/townfall-main.lua.console-copy` is LOADS-CLEAN in BOTH modes with
  all 6 keybinds + its LoadMapPreHook firing clean (844 recorded calls). Two
  stub defects were found and fixed BY this law (not by editing mods):
  1. missing `FPackageName` (port registers it; first seen via dump_object:137),
  2. non-terminating `GetSuperStruct` chain — dump_object's
     `while Object:IsValid() do Object = Object:GetSuperStruct() end` (:156/
     :181/:210) spun the watchdog until the stub was taught to terminate.
- **Lua version note**: harness code is restricted to the 5.2–5.5 common
  subset (no `setfenv`, no 5.5-only `global` declarations) so it parses under
  the console's 5.4 family. All 13 subjects compile under 5.5.1; no 5.4/5.5
  syntax divergence was observed in any mod.

## Summary table

| # | Mod | io-off | io-shim-on | predicted console behavior |
|---|-----|--------|------------|-----------------------------|
| 0 | TownfallItemSpawner console copy (CALIBRATION) | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN (known-good; stub validity proof) |
| 1 | LineTraceMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; F3 keybind fires clean at Lua level (LineTraceSingle struct-OUT still P5's question) |
| 2 | CheatManagerEnablerMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; ClientRestart hook fired clean (CheatManager-already-valid branch) |
| 3 | ConsoleEnablerMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; load-time EIT(CreateConsole) + hook + UnregisterHook all clean |
| 4 | ConsoleCommandsMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; all 3 handlers ("set"/"summon"/"dump_object") register AND fire clean at Lua level |
| 5 | SplitScreenMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; CTRL+Y/U/I keybinds + all 3 EIT closures fire clean (PAD-GAP: CTRL chords unmappped) |
| 6 | TownfallMiniMap v3.1 | **FAILS-AT main.lua:251** | LOADS-CLEAN | LOADS-CLEAN on the composed source (io open; failed settings open degrades non-fatally) — NEEDS-IO only under the absent hypothesis |
| 7 | TownfallModMenu v0.66 | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; 4 keybinds, 9 hooks, 5 map hooks fire clean; ExecuteAsync 2-arg fallbacks never taken (latent, FT gap #19) |
| 8 | Keybinds | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; all 7 tool callbacks fire (all seven globals REGISTERED in typescan-r4 — FO's "five absent" is stale, confirms FT) |
| 9 | BPML_GenericFunctions | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN; 2 custom events registered, never fire (no BP caller on console) |
| 10 | BPModLoaderMod | FAILS-AT :117 | FAILS-AT :117 | FAILS-AT main.lua:117 with its own guard message if IterateGameDirectories yields nil (PS5-faithful assumption); if it returns a Dirs tree instead, next blockers are io.open/io.lines + CreateLogicModsDirectory. SKIP stance unchanged |
| 11 | ActorDumperMod | LOADS-CLEAN | LOADS-CLEAN | LOADS-CLEAN at Lua level; offset-0x98 crash law unchanged (SKIP stance; never run) |
| 12 | jsbLuaProfilerMod | FAILS-AT jsbProfi.lua:115 | LOADS-CLEAN | LOADS-CLEAN as a no-op (os/io open per FT); under absent-io hypothesis FAILS inside require at main.lua:1. Stock-disabled anyway |
| + | bonus: stamina mod (FT §E.1) | LOADS-CLEAN | — | LOADS-CLEAN; its PrintString plain-string FString param remains a console bridge question |

Per-mod raw logs: `runs/<name>.io-{off,on}.log` (verdict, first_error,
missing globals, io/os uses, require resolutions, fire-phase counts, captured
prints, async errors).

## Per-mod detail (default io-off mode unless noted)

- **0 TownfallItemSpawner console copy** — load OK; 0 missing globals; 0 io/os;
  UEHelpers resolves (pcall'd); 6/6 keybinds fired 0 errors (menu build path
  runs end-to-end against the stub: scan → construct UMG tree → SetText →
  AddToViewport); LoadMapPreHook fired clean. LOADS-CLEAN both modes.
- **1 LineTraceMod** — load OK; require UEHelpers OK; Init() prints both
  Kismet full names; F3 fired 0 errors (trace runs, HitResult OUT filled by
  stub, `HitObjectHandle.ReferenceObject:Get()` chain works). Missing: none.
- **2 CheatManagerEnablerMod** — load OK; hook fired: self:get() →
  CheatManager valid → "already exist, skipping restoration". Missing: none.
- **3 ConsoleEnablerMod** — load OK; load-time ExecuteInGameThread(CreateConsole)
  ran inline clean (Default__InputSettings find, ConsoleKeys walk, FName ==
  NAME_None compares all fine); hook fired → UnregisterHook(path, Pre, Post)
  recorded. Missing: none.
- **4 ConsoleCommandsMod** — load OK; requires resolve all 3 sidecars +
  UEHelpers; 3/3 handlers fired 0 errors. `set` handler runs to completion
  against chain stubs (EObjectFlags bitor on integers OK). Missing: none.
  Note: harness proves LUA-level health only — the Reflection/ImportText
  family is BOUND per FT §0.1.1 and gated by P2 on console.
- **5 SplitScreenMod** — load OK; Init property Sets persist (prints
  "UseSplitScreen: true"); CTRL+Y/U/I fired 0 errors; CreatePlayer/
  DestroyPlayer/TeleportPlayers EIT closures all clean. Missing: none.
- **6 TownfallMiniMap** — io-off: RUNTIME-ERROR main.lua:251
  `attempt to index a nil value (global 'io')` — `loadSettings()` runs at load
  (:4266) and io.open is NOT pcall-wrapped. io-shim-on: LOADS-CLEAN
  (settings open stub-succeeds; LoopInGameThreadWithDelay(:4034/:4268)
  recorded-not-invoked by design — registration-time behavior of the real
  binding is P7's question). Reconciled prediction: LOADS-CLEAN on console
  (io open per FT); if the settings open fails there it returns nil and the
  read path degrades non-fatally (f=nil skip), write path stays
  assert-guarded until a user save (documented S5 owner).
- **7 TownfallModMenu** — load OK both modes; pcall(require UEHelpers) OK;
  4 keybinds + 9 hooks + 5 map hooks fired 0 errors; EITWD immediate-ran
  clean at every site; NotifyOnNewObject registrations recorded. Missing:
  none. (ExecuteAsync 2-arg fallback branches are never reached while EITWD
  registers — matches FT gap #19 as latent-dead.)
- **8 Keybinds** — load OK; all 7 RegisterKeyBindAsync callbacks fired 0
  errors: DumpAllObjects, GenerateSDK, GenerateUHTCompatibleHeaders,
  DumpStaticMeshes, DumpAllActors, DumpUSMAP, DumpJMAP are ALL present in the
  stub because they are ALL registered in typescan-r4 LuaMod.cpp:2420-2479.
  Independent confirmation of FT's correction to FO. Console functional risk
  moves to dump OUTPUT PATHS (FT gap #20), not bindings.
- **9 BPML_GenericFunctions** — load OK; RegisterCustomEvent x2 recorded;
  never fired (correct). Missing: none.
- **10 BPModLoaderMod** — RUNTIME-ERROR main.lua:117
  `[BPModLoader] UE4SS does not support loading mods for this game.`
  (LoadModConfigs → IterateGameDirectories → nil stub, the PS5 "directory
  structure unknown" warning path of LuaMod.cpp:3280). io mode changes
  nothing (io.open/io.lines at :29/:44 are only reachable after a Dirs tree).
  CreateLogicModsDirectory (:122) is registered in the port (:3604 per FT).
  Matches FT's predicted first failure exactly; SKIP verdict unchanged.
- **11 ActorDumperMod** — load OK; RegisterCustomProperty table recorded;
  CTRL+NUM_THREE fired 0 errors (Actors:ForEach recorded-not-iterated).
  Missing: none. Offset law (hardcoded 0x98) is a NATIVE crash risk the
  harness cannot see — never run (SKIP).
- **12 jsbLuaProfilerMod** — io-off: RUNTIME-ERROR inside require at
  main.lua:1 → jsbProfi.lua:115 `attempt to index a nil value (global 'os')`
  (os.clock/os.date in the session-header write). io-shim-on: LOADS-CLEAN.
  Reconciled with FT §12: os is open on console → loads as a no-op; the
  harness's io-off failure brackets the absent-io world. SKIP stands.
- **bonus stamina (FT §E.1)** — load OK, 0 missing, UEHelpers resolves;
  LoopAsync recorded-not-invoked. Lua-level LOADS-CLEAN; its PrintString
  plain-string FString param stays BRIDGED-UNTESTED for console.

## Aggregate findings for the ladder

1. **Execution evidence now backs the census**: 10 of 12 ladder mods are
   host-VERIFIED LOADS-CLEAN including keybind/hook/console-handler fire
   paths (FO's manifest had no execution data; FT's scan is static). The two
   exceptions fail only inside io/os-dependent code that the composed source
   opens anyway.
2. **Missing globals union (both modes, all subjects)**: `io` (MiniMap:251,
   load-time, unguarded) and `os` (jsbProfi:115, load-time). Both dissolve
   under FT's luaL_openlibs evidence. NO OTHER global the mods read is absent
   from the composed runtime — FO's c2 list (EObjectFlags, IsClass,
   Reflection/GetProperty, ImportText, ContainerPtrToValuePtr, the five dump
   tools, IterateGameDirectories, RegisterBeginPlayPostHook) is fully
   reconciled as BOUND in the current tree.
3. **New console-hang risk found only by execution** (gap-queue feed):
   dump_object.lua's `while Object:IsValid() do Object = Object:GetSuperStruct()
   end` loops (:156/:181/:210) have NO bound other than IsValid. A
   non-terminating port GetSuperStruct/GetSuperClass (LuaUStruct.cpp:57 —
   BOUND-UNTESTED per FT) would spin the console Lua thread exactly like the
   harness did before the stub fix. Recommendation: the P2 `dump_object`
   dispatch test should carry a wall-clock guard / bounded-iteration
   expectation on the super-struct walk.
4. **io decision is bounded and cheap**: only MiniMap (load-time read,
   degradable) and jsbProfi (no-op write, stock-disabled) touch io/os at
   load; dump_object's io.open (:309) and BPModLoader's io use are handler/
   skip-gated. P0 `type(io)` + `type(os)` rows settle it; no port edit needed.
5. **Keybind fire coverage**: 21 keybind callbacks fired across subjects
   (spawner 6, LineTrace 1, SplitScreen 3, ModMenu 4, Keybinds 7, ActorDumper
   1 — zero errors). Remaining keybind risk on console is purely PAD-GAP
   (PS5Pad.ini chord rows — FT gap #24), not Lua.

## Host harness results

Gap-queue feed for `mod-ladder/STATIC-SCAN.md` §F (appended here only; FT's
file untouched — FU/FT reconciliation):

| # | item | evidence (this harness) | suggested action |
|---|------|--------------------------|------------------|
| H1 | Super-struct/class walk termination (dump_object:156/:181/:210 `while IsValid` + `GetSuperStruct`/`GetSuperClass`) | harness watchdog caught an infinite spin when the stub chain didn't terminate; loop has no bound other than IsValid | add a wall-clock guard to the P2 dump_object dispatch; verify port GetSuperStruct (LuaUStruct.cpp:57) returns an invalid/nil terminal |
| H2 | io at MiniMap load (main.lua:251, unguarded `io.open` in loadSettings, runs at load via :4266) | io-off FAILS-AT :251; io-shim-on LOADS-CLEAN | P0 `type(io)`; expected green per FT §0.1.3 (libs open); failed open degrades non-fatally on read |
| H3 | os at jsbProfi load (jsbProfi.lua:115 session header; reached via require at main.lua:1) | io-off FAILS-AT jsbProfi:115; io-shim-on LOADS-CLEAN | none beyond P0 `type(os)`; mod is stock-disabled/SKIP |
| H3b | IterateGameDirectories return shape (BPModLoader:115→:117) | FAILS-AT :117 with the mod's own guard under the nil assumption | none (SKIP mod); FT's BOUND-UNTESTED classification stands |
| H4 | FPackageName live use (dump_object:137 `FPackageName.IsShortPackageName`) | first calibration stub gap; port registers it (LuaMod.cpp:7827) | none (informational — first live consumer in the pool) |
| H5 | Keybinds tool globals all callable at Lua level | 7/7 fired clean (typescan-r4 LuaMod.cpp:2420-2479) | closes FO's c2 residue; console functional risk = dump output paths only (FT gap #20) |
| H6 | RegisterConsoleCommandGlobalHandler | zero callers across the 12 mods (only the per-mod Handler form is used) | no action |

Harness limitations (read before trusting green): chainable stubs make every
reflected call "succeed" — native marshal questions (P3/P5 struct-OUT shapes,
array growth writes, property Sets, hook param unwrap, LIGWD registration
behavior, FString userdata semantics) are INVISIBLE to this harness and stay
with their probes; custom events are never fired by design; Loop* tick bodies
never execute host-side.
