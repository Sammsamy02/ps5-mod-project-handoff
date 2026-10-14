# CHAIN — post-seeds chain, receipt-pinned composed units (worker-post-seeds-chain, 2026-10-05)

Local-only source trace. All line numbers are in the exact units the launch-11 combined
composition pinned (receipt `run-l11-2/receipt.json` `selected_runtime_units`, built
2026-10-05T18:37, gate GO; bridge from `EDITIONS.json` `bridge_cli`):

| unit | file (workspace-root relative) | sha256 (first 8) |
|---|---|---|
| initializer | `outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp` | `22f06286` |
| luamod | `outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` | `3c39a86b` |
| program | `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` | `7b5d4636` |
| bridge | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` | `faec2336` |
| seed header | `outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp` | `b3d81b0f` |
| seed table | `outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedTable.h` | (4/4, see below) |
| mod source | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/Platform/NativeModSource.cpp` (+ header) | — |
| backend/ack | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp`, `runtime_owner.cpp` | — |
| string fix | `outputs/2026-10-04-zcode-team/worker-iniparser-repro/staged/Helpers/String.hpp` | `7c983db4` |

Which fself pid 672 actually ran is a controller-side fact; where it matters, this chain
names the discriminating log lines.

## 0. Boot frame (before seeds)

1. `module_start` -> bridge publishes readable spans + envelope at module_start
   (bridge `bridge_spanpub_publish`, 714-741; accessors 748-774) -> activation consumed ->
   backend ctor (ue4ss_backend.cpp:20-58): program ctor (setup_paths -> settings
   deserialize -> validate_native_services, UE4SSProgram.cpp:219-278) -> **start_native_mods**
   (backend:53; UE4SSProgram.cpp:585-628) -> **setup_mods -> "Setting up mods..."**
   (UE4SSProgram.cpp:616 -> 1807-1823, log at 1811) -> `mods_startup_verified_` (backend:56)
   -> `ack->activated()` (backend:107).
   - LuaMod ctor runs here per `lua_mods()` row: `m_native_source->lua_mods()` lookup
     (LuaMod.cpp:934-947). Lookup MISS -> **"Mod is absent from native Lua source"** (941),
     `set_installable(false)` (942). MISS = the normalized path did not byte-match the
     source row (the codecvt-era failure). The Lua state is still created (932).
2. `RuntimeOwner::start` -> `backend.initialize` (backend:59-71) -> `program_->init()`
   (UE4SSProgram.cpp:636) -> `setup_unreal()` (655).

Source-order note: **"Setting up mods..." precedes every [Seeds] line** on this source,
because seeds run only inside `init() -> setup_unreal -> Initialize -> ScanGame`. If the
pid-672 log really lists seeds lines before "Setting up mods...", re-check raw timestamps.

## 1. Seeds (ScanGame -> consume_required_slot_seeds)

Arming: table 4/4 (`SeedTable.h` kGenerated L10, kRequiredSetComplete L35) -> armed log
`[Seeds] Required-slot seed table armed (slots 0/2/3/4); ...` (UnrealInitializer.cpp:1153);
ps_scan arm and both Lua override scans skipped (1094-1106); consume called at 1178.

`consume_required_slot_seeds` (833) — **this exact order**:

| phase | slots | bind | log | lines |
|---|---|---|---|---|
| 1 | base check | — | — | 837-842 |
| 2 | slot 0 GUA | `SetupGUObjectArrayAddress` 857 | `[Seeds] slot 0 g_uobject_array bound` 858 | 845-859 |
| 2b | slot 3 GMalloc | `GMalloc = bit_cast<...>` 908 | `[Seeds] slot 3 gmalloc bound ... V1 instance + V2 vptr match` 909 | 865-910 |
| 3 | FName pool 0x9765EC0 | — (validation only) | `[Seeds] FName pool validated at 0x9765EC0 ...` 1038 | 916-1039 |
| 4 | slot 2 ctor 0x63d590 | `assign_address` 1053 | `[Seeds] slot 2 FName ctor bound ...` 1063 | 1041-1064 |
| 5 | slot 4 SCO 0x890aa0 | `SetupStaticConstructObjectInternalAddress` 1079 | `[Seeds] slot 4 static_construct_object_internal bound` 1080 | 1066-1081 |

Pool address: `kBaseObserved` 0x400000 + label 0x9365ec0 = **0x9765EC0** (SeedTable.h:11-12).

Phase 3 detail (all data-side): classify DataReadable 920; snapshot published spans
942-963 (`bridge_readable_span_count`/`bridge_readable_span_at`); capture envelope
970-978 (`bridge_capture_envelope`); **shape+interned-id retry loop 981-1003** —
`check_pool_shape` 991 (v4: none_id>>16 <= CurrentBlock 0-based, Blocks[0] nonzero
8-aligned, membership in one published readable span; 'snapshot truncated' class from
SeedValidation.hpp:347), budget refusal 996-1000, 50 ms retry 1002; Blocks[0] entry-0
'None' content anchor 1013-1019; 'None'/'ByteProperty' entry ASCII match 1021-1037.

**Q1 answer (bind despite refusal):** the pool validates strictly AFTER slots 0/3 bind;
`seed_refused` throws (no rollback), so the two bind logs legitimately exist even when the
pool phase refuses. Boot-path corollary: staged settings carry no `DoEarlyScan` key and the
default is false (SettingsManager.hpp:43), so ScanGame runs once — from `Initialize`
(UnrealInitializer.cpp:1644-1647), inside the PROSPERO try whose only catch disarms the
layout gate and rethrows (2241-2248). The `std::runtime_error` swallow lives in the
DoEarlyScan early-scan call site (UE4SSProgram.cpp:1329-1332) and is dead here. A pool
refusal therefore propagates and aborts init — it cannot coexist with a healthy startup.

## 2. The stop ("Stopping mod ... for uninstall")

`LuaMod::uninstall()` logs `Stopping mod '{}' for uninstall` as its first act
(LuaMod.cpp:7039-7051). On PROSPERO the only callers in this shape:

```
shutdown_native_program (UE4SSProgram.cpp:520)
  └─ uninstall_mods (546)  [s_program == this]
       └─ for each Lua mod: try { mod->uninstall() } (2241-2259)   <- unconditional, even not-installable
            └─ LuaMod::uninstall -> "Stopping mod 'TownfallItemSpawner' for uninstall" (7051)
```

`shutdown_native_program` runs from: `~UE4SSProgram` (511), ctor catch paths (494/502),
`init()` post-join error (716-718), and `init()` catch (721-751, shutdown at 740).

**Seeds-refusal -> mod stop path (the only coupling):**
`ScanGame` consume throw (1178 -> 833) propagates through `Initialize`/`setup_unreal` ->
`init()` catch (727-743): `copy_error_into_message("[Seeds] REFUTED: ...")` 737 ->
`shutdown_native_program()` 740 -> uninstall chain above. The same throw then surfaces:
backend rethrow (ue4ss_backend.cpp:67-70) -> RuntimeOwner start-thread catch
(runtime_owner.cpp:111-118) -> `ack->failed(1, "[Seeds] REFUTED: ...")` -> the
runtime-status **failed line carries the refusal text**. There is no direct
seeds->mod-loader signal; "Mod is absent" (941) only marks a mod not-installable and never
uninstalls anything.

Owner stop during any initializer wait lands in the same chain via
`CheckStartupWait -> ThrowIfStoppedAt` (UnrealInitializer.cpp:76-85; wired to
`m_shutdown_requested` at UE4SSProgram.cpp:1021-1025): the pool retry loop (983) and the
object-construction wait (1688) both exit this way.

Timing discriminator: the truncated-shape refusal retried in the loop would burn the full
`SecondsToScanBeforeGivingUp` budget (**120 s**; staged ini `UE4SS-settings.ini:11`;
SettingsManager.cpp:146-153 coerces 30->120). An **11 s** gap is consistent only with an
immediate terminal refusal (span-table unavailable 943-962, 'None' anchor 1013-1019, entry
mismatch 1021-1031, class/presence refusals), another init throw, or an owner stop.

## 3. Post-seeds chain (assuming all five seeds pass)

| stage | what | evidence lines (UnrealInitializer.cpp unless noted) |
|---|---|---|
| S1 | seeds consumed, scan marked complete | 833-1082; `bScanFullyCompleted = true` 1516 |
| S2 | ctor detour verifier skipped (seed-bound); engine version; object wait | 1666-1670; `Using engine version: {}.#{}` 1678; `Waiting for object construction...` 1685 (NumElements>=10000, 1689-1700); `Locating KismetStringLibrary...` 1704; Conv_NameToString 1722-1739; CDO 1740-1753 |
| S3 | required objects | AddRequiredObject 1756-1803; wait/timeout 1808-1870 (`Constructed [n/m]: ...` 1834) |
| S4 | engine singletons | Object/Struct/GameEngine/GameMode/Actor/GameViewportClient 1883-1930 (named throws 1886/1893/1900/1912/1920) |
| S5 | hook resolution | `Finding GameEngine::LoadMap` 1934; `Finding GameEngine::Tick` 1943 — seeds leave TickInternal not-ready, so the **vtable fallback binds Tick** (1981-2010, log 2008); R3 marshal **compiles out** both EngineTick detour registrations: `[R3] refused: EngineTick pre-callback registrations ...` 2039, `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` 2040, `pump_pending = true` 2041; InitGameState 2069 / BeginPlay 2078 / EndPlay 2087 / AActor::Tick 2096 / ViewportTick 2105 / ProcessEvent 2114 / ProcessConsoleExec 2124 / UStruct::Link 2132 |
| S6 | close-out | `Initializing global FName variables` 2140; required-objects fatal check 2143-2153; `Initializing type system` 2155; ProcessInternal derivation 2174-2235 (Zydis code-read, settings-gated); disarm-on-throw 2241-2248; `PostInitialize` 2250 (GMalloc check 442-445, `Post-initialization: GMalloc: ... -> ...` 447, searcher pools 460-495) |
| S7 | program ready + ack | init(): fire_unreal_init_for_cpp_mods 684, setup_unreal_properties 685, share_lua_functions 688; `m_is_program_started = true` 691; on_engine_initialized (693-697) -> backend chain -> **ack engine_ready** (ue4ss_backend.cpp:33-45; mods_started only when ctor already verified mods, 44); event-loop jthread 709 |
| S8 | mods | update -> on_program_start (1586) -> TRY 1510 -> install_lua_mods 1557 (gate 1898-1902) -> LuaMod::on_program_start 1558 -> start_lua_mods 1560 -> start_mods<LuaMod> PROSPERO branch 2019-2027 (startup_mod_names loop; `find_mod_by_name(..., IsInstalled::Yes)` 2024; `start_mod()` 2025) |
| S9 | game-thread pump arm | NOT an EngineTick detour; the R3 FuncPtr-marshal carrier drain owns it: bridges declared LuaMod.cpp:106-108, defined initializer 2316-2373 (`R3MarshalPublishGameThreadIdentity` 2316, `R3MarshalQueryHarnessDispatch` 2357, `R3MarshalPumpPending` 2370); carrier post-hook publishes identity 4571 and consumes `pump_pending` 4576-4580; arming is the calibration owner's `arm_funcptr_marshal_drain` (4717+). No carrier armed -> no identity publication, no query-harness dispatch. |

## 4. Lua file read path (Q4)

```
start_mod (LuaMod.cpp:6943)
  main_script_path = m_scripts_path / "main.lua" (6962)     m_scripts_path from ctor 945
  load_and_execute_script (6966 -> 6874)
    read(normalize_path_for_lua(script_path)) (6883)        local fix 42-54: path.string() + '\'->'/'
    miss -> return false -> "Failed to execute main script: ..." (6968)
    hit -> chunk "@"+path (6904) -> native_lua_chunk (6912) -> lua_pcall (6923)
```

`NativeModSource::read` (NativeModSource.cpp:48-55): the argument is an **exact UTF-8
logical key below `ue4ss/`** — the same strings the loadlist staging stored
(`NativeModSource.hpp:23-25`: "never filesystem paths"). `validate_path` (10-45) throws on
anything non-canonical (control chars, `\`, `:`, bad UTF-8, `.`, `..`, trailing `/`).
Lookup is a sorted binary search; absent -> nullopt.

Expected key for this mod: `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` — the
`lua_mods()` derivation (77-105) uses exactly this namespace (`ue4ss/Mods/<Mod>/Scripts`
+ `main.lua` presence), and the ctor's `scripts_path` (945) is the same family. So
loadlist path and "filesystem-looking" path coincide byte-for-byte; after the fix the
normalizer reproduces the key for pure-ASCII paths and **read() returns the bytes**.
Failure modes are distinct and loud: key miss -> 6968; mangled/invalid key ->
validate_path throw -> `Exception in load_and_execute_script` (6938).

Residual honesty: `path.string()` on this target converts wide->narrow via the libc++
locale codecvt — a different facet from the `codecvt_utf8/utf16 do_in/do_out` helpers
proven broken (worker-construction-error ANALYSIS §3). The fix is compile-verified in situ
on the real TUs, but no console read-back yet proves runtime byte-exactness.

## 5. Lua state creation and first output (Q5)

- The Lua state is created in the **ctor**: `m_lua(LuaMadeSimple::new_state())`
  (LuaMod.cpp:932; `luaL_newstate` at LuaMadeSimple.cpp:1088-1090, lms port/first) — even
  for a not-installable mod.
- Script execution happens only in `start_mod` (6943-6983), which requires ALL of:
  1. ctor lookup matched (`lua_mods()` contains the normalized path, 936-946) — else
     `set_installable(false)` and never installed;
  2. enabled: `startup_mod_names()` contains the name — mods.txt row `Name:1`
     (NativeModSource.cpp:113-133) or `<Mod>/enabled.txt` (134-135);
  3. installed: `install_lua_mods` (UE4SSProgram.cpp:1557 -> 1915; not-installable skip
     warning 1900) then `find_mod_by_name(..., IsInstalled::Yes)` (2024);
  4. `start_mod`: prepare_mod 6949 (6798) -> make_main_state 6950 (1090) ->
     setup_lua_global_functions_main_state_only 6951 -> make_async_state 6952 (1064) ->
     start_async_thread 6953 -> `m_is_started = true` 6955 -> custom module loader 6959;
  5. `load_and_execute_script(main_script_path)` (6966): read hit + `native_lua_chunk`
     OK (6912) + `lua_pcall` OK (6923).
- The first line `[Lua] [TownfallItemSpawner] Loaded v1.0...` is the script's own
  top-level `log(...)` (main.lua:746 pin, worker-executedrain-trace CLAIM hop 1) running
  during that `lua_pcall`. Failure surfaces: 6968 / 6916 / 6927 / 6981.
