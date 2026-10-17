# DRIFT-TABLE — settings v3 vs the LAUNCH-10 composition

Worker: worker-settings-audit · 2026-10-04 · read-and-verify, no edits, local only.

Subject ini: `TEAM:worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini`
sha256 `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038`, 1448 bytes, ASCII, no BOM.
(The task-stated path `package-v3/UE4SS-settings.ini` omitted the `ue4ss/` level; the hash-pinned file is one level deeper.)

Audited launch-10 units (composition-6 + launch-10 deltas):
- `settings` parser = `CR1/source/port/ue4ss/src/SettingsManager.cpp` (byte-identical to the `ue4ss-unreal-native-build-1/port` copy — diffed).
- `program` = `TEAM:worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp` (sha `75abf2ab…`).
- `unreal-unrealinitializer` = `TEAM:worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` (current `b943aed5…`; witness pinned `1fc82e6b…` — see §E).
- ini units = `port/first/src/IniParser/{Ini,TokenParser,Value}.cpp` (unit map from the notify-cancel-1 base receipt, 359 selected runtime units) compiled against `UONB/deps/first/IniParser/include` headers.
- Composed LuaMod = `ue4ss-native-notify-cancel-1/src/LuaMod.cpp` (NOT the CR1 port copy — checked separately).
- No GUI.cpp and no upstream `SDKGenerator/SDKGenerator.cpp` unit is composed (so those upstream-only consumers do not exist in launch-10).

Citation shorthand: SM = port `SettingsManager.cpp`; SMH = `SettingsManager.hpp`; PROG = staged program; INIT = launch-10 initializer.

Legend for verdicts in §C: **REQUIRED** = wrong/missing value breaks launch-10 (refusal or the exec-read hazard); ok-default = default applies and is the intended/vetted behavior; inert = parsed+consumed only in dead/non-Prospero paths.

---

## A. Every key v3 sets (13 keys, 5 sections) — parse + consumer proof

| v3 key (section) | parsed at | consumer in launch-10 (file:line) | verdict |
|---|---|---|---|
| `[General] bUseUObjectArrayCache=false` | SM:146 | PROG:980 → `UnrealConfig.bUseUObjectArrayCache` → INIT:414 (skips searcher-pool construction; linear ForEachUObject fallback) | **ok — intended conservative value for unverified 5.6.1 hash-table layout** |
| `[General] SecondsToScanBeforeGivingUp=120` | SM:136 (30→120 rewrite SM:137-145) | PROG:979 → INIT:1009 `consume_required_slot_seeds(seconds(...))`, INIT:1033, INIT:1093 (seed grace + PS/Lua scan timeout) | **ok — equals code default (SMH:39); doubles as the seed-consumption grace window** |
| `[General] DefaultExecuteInGameThreadMethod=EngineTick` | SM:151-159 | PROG:1494 → `LuaMod::m_default_game_thread_method` → composed LuaMod:4709/4805/4964/5151 | **ok — equals default (SMH:45); requires HookEngineTick=true (default, unset by v3) — satisfied** |
| `[General] DefaultFNameToStringMethod=Scan` | SM:161-169 | PROG:1256 → `UnrealConfig.FNameToStringMethod`; port Signatures.cpp:251 | **ok — equals default (SMH:46); avoids Conv_NameToString dependency** |
| `[Debug] ConsoleEnabled=0` | SM:223 | PROG:732 (validate_native_services throws if true) + PROG:890 (create_simple_console throws if true; called unconditionally at PROG:278) | **REQUIRED 0** |
| `[Debug] GuiConsoleEnabled=0` | SM:224 | PROG:734 (validate throws if true) | **REQUIRED 0** |
| `[Debug] GuiConsoleVisible=0` | SM:225 | PROG:734 (same compound condition); UE4SSProgram.hpp:36 macro is GUI-only (non-Prospero) | **REQUIRED 0** |
| `[Debug] RenderMode=ExternalThread` | SM:239-251 | PROG:735 (validate throws unless ==ExternalThread) | **REQUIRED (or default-equal; explicit is correct)** |
| `[CrashDump] EnableDumping=0` | SM:268 | PROG:730 (validate throws if true). The crash-dumper `enable()` consumer (PROG:239-242) is `#ifndef __PROSPERO__` | **REQUIRED 0** (default is true = hard activation refusal) |
| `[EngineVersionOverride] MajorVersion=5` | SM:172 | PROG:1011-1031 (forces `Unreal::Version::Major`, no-ops version-finder scan) | **ok — correct for UE 5.6.1** |
| `[EngineVersionOverride] MinorVersion=6` | SM:173 | PROG:1012-1031 (same block; needs BOTH ≠ -1) | **ok — correct for UE 5.6.1** |
| `[Hooks] HookProcessInternal=0` | SM:279 | PROG:1240 → `UnrealConfig.bHookProcessInternal` → INIT:1971 (OR-gate) | **REQUIRED 0** (skips init-time ExecuteUbergraph FuncPtr jmp-resolve read + Zydis block INIT:1971-2035) |
| `[Hooks] HookProcessLocalScriptFunction=0` | SM:280 | PROG:1241 → `UnrealConfig.bHookProcessLocalScriptFunction` → INIT:1971 (same OR-gate; sub-derivation INIT:1990 never re-checks the flag) | **REQUIRED 0 — BOTH keys must be 0 because the gate is an OR** |

Zero drift in §A: no stale keys, no invalid spellings, no wrong values.

## B. Every key the launch-10 build parses but v3 does NOT set (default applies)

Registered parsers: SM:84,110,117,119,132-136,146-149,151,161,172,173,176,192,203,204,207-209,212-217,220,223-227,229,239,253,268,269,272,273,276,279-303. Defaults: SMH.

| key (section) | default | consumer (launch-10) | default safe? |
|---|---|---|---|
| `[Overrides] ModsFolderPath` / `ControllingModsTxt` / `+/-ModsFolderPaths` | empty / empty / none | SM:112-113 REFUSES non-empty under native activation; PROG:852-857 uses logical `ue4ss/Mods` and never consults Overrides when a mod source exists | yes — omission is the ONLY valid state; §A of v3 header documented correctly |
| `[General] EnableHotReloadSystem` | false | PROG:737 (validate refuses true), 1458-1466 | yes — false required, default is false |
| `[General] HotReloadKey` | R | PROG:1460 (only under hot reload) | yes — inert |
| `[General] EnableAutoReloadingLuaMods` | false | PROG:737, 1536, 1693 | yes — false required, default false |
| `[General] UseCache` | true | PROG:978 (sig-scan disk cache config) | yes — launch-9-vetted value; unchanged by v3 (honest note: PS5 cache-write path not re-verified host-side) |
| `[General] InvalidateCacheIfDLLDiffers` | true | PROG:977 | yes — same |
| `[General] EnableDebugKeyBindings` | false | PROG:749/752 (validate), 1510 | yes — false required, default false |
| `[General] bForceGUObjectArrayForIteration` | false | PROG:981, 1287 | yes — keeps hash-table/linear decision to runtime |
| `[General] DoEarlyScan` | false | PROG:1259 | yes — seed path arms DoScan instead; Shape-A designed default |
| `[General] bEnableSeachByMemoryAddress` → `General.SearchByAddress` | false | **NO consumer in the composed launch-10 units** (only upstream GUI LiveView.cpp, not composed) | yes — inert key; v3 rightly omits |
| `[EngineVersionOverride] DebugBuild` | Detect | PROG:1257 → INIT:1136/1197/1252 (Detect = use ps_scan `build_configuration` from seeds/scan) | yes — Detect is REQUIRED for launch-10; forcing would fight the seed facts |
| `[EngineVersionOverride] Stats` | Detect | PROG:1258 → INIT:1172/1262 | yes — same |
| `[ObjectDumper] LoadAllAssetsBeforeDumpingObjects` / `UseModuleOffsets` | false/false | PROG:1448/3155; ObjectToString.cpp:29 — dump flows only | yes |
| `[CXXHeaderGenerator] DumpOffsetsAndSizes` / `KeepMemoryLayout` / `LoadAllAssetsBeforeGeneratingCXXHeaders` | false/false/false | SDK Generator.cpp:261/817/886/925; PROG:1448 — SDK flows only | yes |
| `[UHTHeaderGenerator]` 6 keys | false (IgnoreEngineAndCoreUObject=true) | UEHeaderGenerator.cpp (composed) — SDK flows only | yes |
| `[SDKGenerator] OutputPath` | "" | **NO consumer in composed units** (upstream SDKGenerator.cpp not composed) | yes — inert key; v3 rightly omits |
| `[Debug] GuiConsoleFontScaling` / `GuiConsoleMonospaceTextEditors` | 1.0 / false | **no composed consumer** (upstream GUI.cpp only) | yes |
| `[Debug] GraphicsAPI` | GLFW3_OpenGL3 | PROG:246 is `#ifndef __PROSPERO__` — no Prospero consumer | yes |
| `[Debug] ToggleGuiKey` | O | PROG:1408 inside `#ifndef __PROSPERO__` GUI block | yes |
| `[CrashDump] FullMemoryDump` | false | PROG:244 `#ifndef __PROSPERO__` | yes |
| `[Threads] SigScannerNumThreads` / `SigScannerMultithreadingModuleSizeThreshold` | 8 / 16777216 | PROG:990-1006 → INIT:1441-1442 (SinglePassScanner) | yes — launch-9-vetted |
| `[Memory] MaxMemoryUsageDuringAssetLoading` | 85 | PROG:650 | yes |
| `[Hooks] HookLoadMap` | true | INIT:1747 (vtable read, guarded) | yes — designed launch-10 behavior |
| `[Hooks] HookInitGameState` | true | INIT:1864 | yes |
| `[Hooks] HookCallFunctionByNameWithArguments` | true | PROG:1244 → config only; **no arm site found in composed units** (Hooks.cpp:618 definition; Metadata.hpp:250 address-taken) | yes — inert |
| `[Hooks] HookBeginPlay` | true | INIT:1873 | yes |
| `[Hooks] HookEndPlay` | true | INIT:1882 | yes |
| `[Hooks] HookLocalPlayerExec` | true | PROG:1247 → config only; **no arm site found** (Metadata.hpp:258 address-taken) | yes — inert |
| `[Hooks] HookAActorTick` | true | INIT:1891 | yes |
| `[Hooks] HookEngineTick` | true | INIT:1756-1863 — **MUST stay true**: arms the EngineTick route that v3's EngineTick method + closures drain through, incl. GameThreadInitializer + QueryHarnessDispatch callback registration (INIT:1844-1859) | yes — v3 correctly does NOT set it; setting 0 would kill the tick route |
| `[Hooks] EngineTickResolveMethod` | Scan | INIT:1796-1840 — with seeds armed, scan_address is null → live-vtable fallback is the only tick route (INIT:1798-1825 comment) | yes — Scan default ≡ VTable on the armed path |
| `[Hooks] HookGameViewportClientTick` | true | INIT:1900 | yes |
| `[Hooks] HookUObjectProcessEvent` | true | INIT:1909 — ProcessEvent resolved from the vtable (data read), as v3's header comment states | yes — REQUIRED-true; v3 correctly does not touch it |
| `[Hooks] HookProcessConsoleExec` | true | INIT:1918 | yes |
| `[Hooks] HookUStructLink` | true | INIT:1927 | yes |
| `[Hooks] FExecVTableOffsetInLocalPlayer` | 0x28 | INIT:2034 (Verbose log only on this port) | yes |
| `General.InputSource` (member, SMH:42) | "Default" | PROG:744-752/1480-1488 — **NOT ini-parseable** (no REGISTER in deserialize; API-only) | yes — no drift possible via ini; noted for completeness |

**§B verdict: zero unsafe defaults.** Every default that launch-10 consumes is either the launch-9-vetted value, a Detect mode the seed path requires, or false-by-design. The two defaults that would be actively harmful (`CrashDump.EnableDumping=true`, `Debug.*=true`) are exactly the keys v3 zeroes (§A). The defaults that must remain true (`HookEngineTick`, `HookUObjectProcessEvent`) are untouched by v3.

## C. Keys both read and set — value-correctness re-check (none wrong)

- `bUseUObjectArrayCache=false`: conservative; skips INIT:414 pool construction → no 5.6.1 hash-table-layout dependency. Intended. (Cost: linear object search — honest perf note, not a correctness issue.)
- `SecondsToScanBeforeGivingUp=120`: == code default; feeds the seed grace window (INIT:1009). Redundant but harmless.
- `DefaultExecuteInGameThreadMethod=EngineTick` + unset `HookEngineTick`: consistent pair — closures drain per-frame via the tick hook (LuaMod 4709+). Correct.
- `DefaultFNameToStringMethod=Scan`: == default; consistent with the seeded FName pool slot.
- `MajorVersion=5/MinorVersion=6`: correct for 5.6.1 (see CLAIM SA-4). Forcing ALSO disables the version-finder scan (PROG:1030) — intended, and the initializer expects the settings route to own engine version (INIT:591-593).
- `ConsoleEnabled=0`, `GuiConsoleEnabled=0`, `GuiConsoleVisible=0`, `RenderMode=ExternalThread`, `EnableDumping=0`: precisely the five conditions `validate_native_services` (PROG:730-736) plus `create_simple_console` (PROG:890) demand.
- `HookProcessInternal=0` + `HookProcessLocalScriptFunction=0`: precisely the OR-gate at INIT:1971; the only ini route to the A-PI skip (no ScanOverrides ini setters exist).

## D. Launch-10 delta vs settings (task step 4)

| delta unit | settings reads | new key needed? |
|---|---|---|
| read-path guard (initializer `classify_seed_address` guards 757/777/828/880/906, gmalloc validators 804/811) | 0 (grep) | no — guards classify seed addresses, not settings |
| geometry fix (`worker-facts-geometry-fix` lmf geometryfix; witness-verified single intersect-extent hunk) | 0 | no |
| diagnostic sink (`worker-diagnostic-sink` diagnostic.c) | 0 | no |
| bridge diag (`runtime_start_bridge_r13.geometrydiag.cpp`, 3 hunks after the conversion decision) | 0 | no |
| staged program (Resolution-B delta, 22 lines vs port) | no new reads; hooks mapping unchanged (1240-1259); validate_native_services unchanged (727-755) | no |
| r3-armed initializer (launch-11 candidate, superset of the launch-10 file) | adds `UE4SS_R3_MARSHAL_MODE` build-define gate — explicitly NOT a settings key; cites "SETTINGS-V3 trap-2 rationale" | no — arming it needs a -D define, not an ini key |

## E. Honesty items (what this audit could NOT verify host-side)

1. **Initializer staging sha drift:** the launch-10 witness pinned `1fc82e6b…`; the current staging hashes `b943aed5…`. Diff of the current file against the gmalloc staging base (`499a814f…`) shows the delta is exactly the 5 seed guards + diagnostic-text enrichment — **0 settings/config/hook lines** (grep count 0), and the A-PI gate (1971) + guards are present in the current bytes. Whoever runs the launch-10 one-pass must re-pin the actual composed bytes.
2. The launch-10 one-pass build had **not run** at witness time; the existing comp6 run-1 predates the launch-10 staging. This audit therefore proves the STAGED inputs, not a measured PRX.
3. No PS5-side parse was executed; parse tolerance is proven from source + the v3 package's own host oracles (`host-test/settings_parse_test_v3`, `v3-activation-output.txt` T1-T6 all PASS), not re-run here.
4. The packaged copy inside any final ploader payload was not available to hash — only the local `package-v3` copy.
5. The game's true engine version (5.6.1) is taken from project context; the game binary was not inspected.
6. Prospero behavior of the `UseCache=true` sig-scan disk cache is unchanged from launch-9 and was not re-verified.
7. `PROG:744-752`: if a native input source is ever supplied and unavailable, activation refuses regardless of ini (`InputSource` is not ini-parseable) — settings can neither cause nor fix that class of refusal.

## F. Verdict

**SETTINGS-SAFE-FOR-LAUNCH-10.** No key drift in either direction; the two `[Hooks]` zeros and the five native-service zeros are exactly the load-bearing values the launch-10 units demand; every unset key's default is correct or Detect/required-true. **No v3.1 proposed** — any change would be churn.
