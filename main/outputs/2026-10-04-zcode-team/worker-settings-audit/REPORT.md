# REPORT — worker-settings-audit

2026-10-04 · LOCAL ONLY · read-and-verify task, no edits made outside this directory.

## Question

Is settings v3 (`UE4SS-settings.ini`, sha256 `d8faa4ec…`) correct for the LAUNCH-10 build — i.e. no key drift between what v3 sets and what the launch-10 composition actually parses — given that a wrong/missing key could silently re-enable the unconditional exec-page reads v3 was crafted to prevent (`[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0`), or leave a needed feature off?

## Verdict

**SETTINGS-SAFE-FOR-LAUNCH-10.** No v3.1 needed.

## What was done

1. **v3 read in full** (13 keys / 5 sections; full listing in DRIFT-TABLE §A). Hash verified: the local copy is at `worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini` (the task path omitted the `ue4ss/` level); sha256 `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038` matches the pinned `d8faa4ec…`. 1448 bytes, ASCII, no BOM, no NUL (both would break the parser — BOM at StartOfFile throws; NUL is refused at SettingsManager.cpp:64-65).
2. **Parser enumerated.** The composed `settings` unit is `native-coherent-runtime-1/source/port/ue4ss/src/SettingsManager.cpp` (byte-identical to the `ue4ss-unreal-native-build-1/port` copy — diffed; the `ue4ss-native-settings-defaults-1/include` shadow on its include line is inert because the CR1 include dir precedes it). All 42 registered keys + the Overrides list were extracted with defaults from `SettingsManager.hpp` (its header-cited line anchors 116-117/279-280 match the composed copies exactly).
3. **Ini parser behavior proven from source.** Composed ini units are `port/first/src/IniParser/{Ini,TokenParser,Value}.cpp` against the `deps/first/IniParser/include` headers:
   - missing `[Overrides]` section → `get_list` returns `List{nullptr}` (Ini.cpp:91-103) and `for_each` null-guards (Section.hpp:44-47/67-70) → omitted section is safe, no throw;
   - bools accept `0/1/false/true` case-insensitively (TokenParser.cpp:84-105) — all six v3 `0`/`false` values valid; ints and the `EngineTick`/`Scan`/`ExternalThread` method strings parse;
   - the v3 header's "comment-only section wedges the parser" claim is mechanically true (a key-less section leaves state `StoreSectionKey`; the next `[` throws, TokenParser.cpp:242-248) — v3 contains no key-less section;
   - parse failure cannot silently fall back: `deserialize` throws abort activation loudly (staged program:232-236 → Prospero stores the message and shuts down).
4. **Consumers mapped across the exact launch-10 units** — `program` = `worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp` (`75abf2ab…`), `unreal-unrealinitializer` = `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` (current `b943aed5…`), composed LuaMod = `ue4ss-native-notify-cancel-1/src/LuaMod.cpp` (the unit map from the notify-cancel-1 base receipt, 359 units, pins which tree every unit comes from — e.g. no GUI.cpp, no upstream SDKGenerator.cpp). Every v3 key has a live consumer; every consumed-but-unset key's default is safe (full tables in DRIFT-TABLE §A/§B).
5. **The two `[Hooks]` zeros verified end-to-end:** SM:279-280 → SMH:116-117 default true → staged PROG:1240-1241 → INIT:1971 `if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)`. Both=0 skips the ExecuteUbergraph FuncPtr jmp-resolve read + the 164-byte Zydis derivation (INIT:1971-2035). The OR-gate + the PLSF sub-derivation never re-checking its flag (INIT:1990 checks only the non-ini `ScanOverrides` lambda) confirm BOTH keys must be 0 — v3 does that, and no other ini-reachable disable route exists (grep: only `version_finder` is set programmatically, PROG:1030).
6. **Launch-10 deltas checked for new settings-dependent behavior:** read-path guard (5 seed-class guards + gmalloc validators), geometry fix, diagnostic sink, bridge diag, and the staged program's Resolution-B delta — **zero settings reads in all of them**; no new key is required. The r3-armed initializer (launch-11 candidate) gates on a build define (`UE4SS_R3_MARSHAL_MODE`), deliberately not a settings key.
7. **Value-correctness re-check:** `EngineVersionOverride 5/6` is correct for UE 5.6.1 (layouts key off major.minor; forcing also disables the version-finder scan, PROG:1009-1031, and the initializer comment assigns engine version to this settings route, INIT:591-593). `bUseUObjectArrayCache=false` skips the searcher pools (INIT:414) — the intended conservative mode. The five native-service zeros are exactly what `validate_native_services` (PROG:727-755) + `create_simple_console` (PROG:890) demand; `HookEngineTick`/`HookUObjectProcessEvent` correctly stay default-true (INIT:1756/1909) so the EngineTick closure route and vtable-resolved ProcessEvent survive, exactly as v3's header claims.

## Key findings

- **Zero drift.** No v3 key is stale/unread (the only parsed-but-unconsumed keys — `bEnableSeachByMemoryAddress`, `SDKGenerator.OutputPath`, GUI font/monospace keys — are ones v3 does NOT set, because their consumers live only in non-composed upstream units).
- **Five v3 zeros are REQUIRED for launch-10 to start at all** (`EnableDumping`, `ConsoleEnabled`, `GuiConsoleEnabled`, `GuiConsoleVisible` + `RenderMode=ExternalThread`): defaults would hard-refuse activation in `validate_native_services`.
- **Two v3 zeros are REQUIRED to prevent the A-PI exec-text reads** (`HookProcessInternal`, `HookProcessLocalScriptFunction`): with either left at default true, the OR-gate at INIT:1971 runs the init-time exec-page read chain.
- **Two defaults are load-bearing-true** and v3 correctly leaves them unset: `HookEngineTick` (the EngineTick closure drain + game-thread identity publication + query-harness dispatch) and `HookUObjectProcessEvent`.
- `Debug.GraphicsAPI`, `ToggleGuiKey`, `CrashDump.FullMemoryDump`, and the GUI thread consumers are all `#ifndef __PROSPERO__` — no PS5 consumer, so their defaults are moot.

## What could NOT be verified host-side (honest bounds)

1. **Initializer sha drift vs witness:** the launch-10 witness pinned `1fc82e6b…` for the readpath-guard initializer; the current staging is `b943aed5…`. I diffed the current file against the gmalloc staging base (`499a814f…`): the delta is exactly the seed-guard/diagnostic hunks, **0 settings/config/hook lines**, and the A-PI gate + guards are present at their cited lines in the current bytes. Still, the launch-10 one-pass must re-pin whatever bytes it composes.
2. **The launch-10 one-pass build had not run** at witness time (existing comp6 run-1 predates the launch-10 staging). This audit proves the staged inputs — what composition will consume — not a measured PRX.
3. No PS5-side parse was executed; parse tolerance is proven from source plus the v3 package's own host oracles (`host-test/`, T1-T6 PASS), not re-run here. Only the local package copy was hashed — not any payload already staged into a ploader image.
4. The game's true engine version (5.6.1) is taken from project context; the game binary was not inspected.
5. `UseCache=true` (sig-scan disk cache) Prospero behavior is unchanged from launch-9 and was not re-verified.
6. Input-handler availability refusals (PROG:744-752) are settings-independent (`InputSource` is not ini-parseable) — neither caused nor fixable by any ini.

## Deliverables

- `CLAIM.json` — SA-1..SA-6 with evidence, non-claims, verdict rationale, input list.
- `DRIFT-TABLE.md` — §A every v3 key (parse line + consumer + verdict), §B every parsed-but-unset key (default + consumer + safety), §C value-correctness re-check, §D delta-vs-settings table, §E honesty items, §F verdict.
- `REPORT.md` — this file.

No files outside `outputs/2026-10-04-zcode-team/worker-settings-audit/` were created or modified.
