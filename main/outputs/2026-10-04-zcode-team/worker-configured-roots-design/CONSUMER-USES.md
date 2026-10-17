# CONSUMER-USES.md — every consumer of `game_executable_path` in the coherent-runtime port

Worker: worker-configured-roots-design · 2026-10-04 · proposals only.

## Method (what was actually grepped)

The authoritative unit set is the composition receipt of the current composed
runtime object (composition-4 run-3, the object composition-5 packages;
`ue4ss-native-supplier-composition-5/run-1` does not exist yet on disk):

- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/receipt.json`
  → 368 `selected_runtime_units` + `bootstrap_units` + `retained_fixed_dependencies`
  = 371 sources (dumped to `unit-sources.txt` in this dir; all 371 exist on disk).
- Every one of those files was grepped for `game_executable_path`,
  `game_exe_path`, `executable_path`. A second grep covered
  `get_game_executable_path`, `m_game_executable_directory`,
  `m_legacy_root_directory`, `m_game_path_and_exe_name` across the port tree
  (`native-coherent-runtime-1/source/port`), the native-unit include/src tree
  (`ue4ss-native-tick-file-owner-1/{include,src}`), the packaging wiring
  (`worker-module-start-wiring`), and the other spike src dirs that contribute
  units (`ue4ss-native-jmap-port-1`, `ue4ss-native-notify-cancel-1`).
- Copy discipline: the adapter source at
  `ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` is
  byte-identical to `worker-supplier-ingame-update/src/lmf_to_native_loader_facts.cpp`
  (diff clean) and strictly newer than `worker-s1-adapter/src/…` (which lacks the
  vm-scan opt-in). The tick-file-owner copy is the composed one (composition
  argv `--extra-unit lmf-loader-adapter=…`).

## Verdict on "directories only"

**CONFIRMED for the PROSPERO+mod_source startup path — with two exact
exceptions that the design must handle:**

1. `UE4SSProgram.cpp:427` — `std::filesystem::file_size(m_game_path_and_exe_name)`
   executes unconditionally in the constructor banner. With an empty/unreal
   path this THROWS `std::filesystem_error` → caught at 445 → startup fails.
   So the string is not "directories only"; one use requires an existing
   regular file.
2. The filename (not the directory) is consumed by two dump/generator
   call-sites (`UEHeaderGenerator.cpp:4172`, `JMapGenerator.cpp:2015`) via
   `get_game_executable_path()`. Neither runs during startup; both degrade to
   empty-name metadata under Resolution B.

All other PROSPERO+mod_source uses are `parent_path()` directory derivations.
Exactly as the task context stated: `UE4SSProgram.cpp:755-784` uses
`game_exe_path` only for `m_legacy_root_directory`,
`m_game_executable_directory`, `m_game_path_and_exe_name`.

## Complete use list (file:line, canonical files)

### A. Contract side (mandatory-field enforcement — the blocker)

| # | Site | Use | PROSPERO+mod_source weight |
|---|------|-----|----------------------------|
| A1 | `lmf_to_native_loader_facts.cpp:139-152` (canonical `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/`) | `snapshot.executable_path` empty → refuse (`GameExecutablePathMissing`, "observed executable path is empty; refusing to fabricate game_executable_path"); non-lexical → `GameExecutablePathInvalid`. Then `facts.game_executable_path = executable_path` (`:224`) | **BLOCKER**: on this target the sysctl route is ENOENT and the exec-record kve_path is zeros, so the field is always empty and conversion always refuses (`launch-7`: outward code `GameExecutablePathMissing`) |
| A2 | `live_module_facts.cpp:181,219-223` (`…/tick-file-owner-1/src/`) | Supplier probe: `ops_.executable_path(...)` → `facts_.executable_path` + `capabilities.executable_path` Ok/Failed | Upstream of A1; Failed in-game (honest capability record, no refusal by itself) |
| A3 | `live_module_os_ops_sce.cpp:110-119` | `lmf_sce_executable_path`: `sysctl(CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1)` | Returns −1 in-game (ENOENT — launch evidence); the "authoritative" source is dead on this kernel |
| A4 | `NativeLoaderContext.cpp:15` (`…/tick-file-owner-1/src/`) | `context.game_executable_path = std::move(facts.game_executable_path)` then `ValidateNativeProgramContext(context)` (`:18`) | Pass-through into the context |
| A5 | `Platform/NativeProgramContext.hpp:85-92,97-100` (port: `native-coherent-runtime-1/source/port/ue4ss/include/…`; overlay copy byte-identical — verified by diff) | `ValidateNativeProgramContext`: lexical gate on BOTH paths (`:85-92`), `is_regular_file` on BOTH (`:97-99`, throws "Native module and executable paths must name existing regular files" — the launch-6 in-game proof), `file_size(game_executable_path)` (`:100`) | **BLOCKER**. Note the function is `inline` → each TU embeds its own copy; the retained `program` object carries the OLD gate until that unit is recompiled |
| A6 | `UE4SSProgram.cpp:215` (port `…/port/ue4ss/src/`) | Constructor re-validates: `Platform::ValidateNativeProgramContext(m_native_context)` | Second enforcement point; same inline-copy caveat |
| A7 | bridge ladder `worker-module-start-wiring/runtime_start_bridge_r9.cpp:385-432` (r10/r11 refine) | `bridge_executable_path` op feeding A2: rung 1 kernel kve_path (r7), rung 2 sysctl, rung 3 configured `/app0/eboot.bin` iff `is_regular_file`, and — **r9 only** — rung source=3 substituting `/data/plugins/modbridge_supplier_composition.prx`. r10/r11 REMOVE the PRX substitution (user directive) and add the stat-both diagnostics (`r10:407-429`) then refuse honestly (source=0) | r9's source=3 ran in launch-8 and PASSED the path gate — proving that once a stat-able path is supplied, conversion + context build proceed. That substitution is exactly what the binding user directive forbids; r10/r11 already removed it |

### B. Consumer side (what the string is actually used FOR)

| # | Site | Use | PROSPERO+mod_source weight |
|---|------|-----|----------------------------|
| B1 | `UE4SSProgram.cpp:761` | `const std::filesystem::path& game_exe_path = m_native_context.game_executable_path;` | Source of all three derivations |
| B2 | `UE4SSProgram.cpp:767-768` | `game_directory_path = game_exe_path.parent_path();` → `m_legacy_root_directory` | Directory only. On mod_source, `m_legacy_root_directory` is set but the mod_source branch (`:776-784`) returns before any legacy-settings use; residual consumers: banner/getter/Lua fallbacks |
| B3 | `UE4SSProgram.cpp:771` | `m_game_executable_directory = game_directory_path;` | Directory only. Consumed by banner `:426` and getter `:2579-2582` → LuaMod Lua funcs (B8) |
| B4 | `UE4SSProgram.cpp:773` | `m_game_path_and_exe_name = game_exe_path;` | Full path retained |
| B5 | `UE4SSProgram.cpp:427` | `Output::send(… std::filesystem::file_size(m_game_path_and_exe_name))` in the startup banner | **Startup-functional**: throws on empty/nonexistent → startup failure. The single non-directory, non-derivation use on the PROSPERO path |
| B6 | `UE4SSProgram.cpp:2573-2576` (`get_game_executable_path`, PROSPERO-only getter) | Returns `m_game_path_and_exe_name` | Name-bearing consumers below |
| B7a | `UE4SSProgram.cpp:426` | Banner prints `m_game_executable_directory` | Display only |
| B7b | `SDKGenerator/UEHeaderGenerator.cpp:4172-4181` (`determine_primary_game_module_name`, PROSPERO branch) | `filename().replace_extension()` minus `-Win64-Shipping` → SDK module name | Name-only; SDK generation is not on the startup path (invoked via GUI/Lua `generate_sdk`). Under B: empty name |
| B7c | `ue4ss-native-jmap-port-1/src/JMapGenerator.cpp:2015` (composition unit `native-jmap`) | `Native::source_filename(get_game_executable_path())` → JMap metadata "source" | Name-only, cosmetic metadata |
| B8 | `Mod/LuaMod.cpp:2939` (`IterateGameDirectories`), `:3134` (LogicMods path reconstruction), `:3274` (`CreateLogicModsDirectory`) | `get_game_executable_directory().parent_path().parent_path()/…` expecting the Windows `<Root>/Game/Binaries/Win64` layout | Directory derivations; every site is guarded by `std::filesystem::exists` checks that warn/nil/throw gracefully on PS5 (layout will not match regardless). No startup weight |
| B9 | `UE4SSProgram.cpp:788` | `AddDllDirectory(game_exe_path.c_str())` | **Unreachable on PROSPERO**: inside `#ifdef _WIN32` AND after the mod_source early return |

### C. Non-consumers (verified, for the record)

- `NativeModSource` / `PrepareActivatedUE4SSRuntime` / `RuntimeOwner` /
  `StartActivatedRuntimeWithAck`
  (`ue4ss_backend.hpp`, `runtime_owner.hpp`,
  `worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.hpp`):
  no reference to `game_executable_path` anywhere. Mods/config are selected
  from retained logical `ue4ss/` keys; the ack status root comes from
  `mod_root`, not from the executable path.
- Entry (`worker-module-start-wiring/runtime_module_entry_r3.c:65-68`): owns
  the constants `MODBRIDGE_TITLE_ID "PPSA33286"`,
  `MODBRIDGE_MOD_ROOT "/data/modbridge/roots/PPSA33286"`,
  `MODBRIDGE_MODULE_PATH "/data/plugins/modbridge_supplier_composition.prx"`,
  `MODBRIDGE_UE4SS_LOG_PATH "/data/plugins/UE4SS.log"`; passes
  `(module_path, mod_root, title_id)` to
  `modbridge_runtime_start_with_activation` (`:281-282`) — i.e. the configured
  roots already reach the bridge as parameters; no entry change is needed for
  Resolution B.
- No port file outside the above touches `game_executable_path` /
  `game_exe_path` / `get_game_executable_path` (greps quoted above; 371-file
  unit list + port include tree).

## Functional-weight summary for the fork

On PROSPERO+mod_source the string has exactly three honest jobs:
1. **Directory derivations** (B2/B3) — replaceable by an explicit configured
   game directory (Resolution B).
2. **One startup banner `file_size`** (B5) — needs a guard when unobserved.
3. **Filename for two metadata/generator sites** (B7b/B7c) — degrades to empty;
   not startup-critical; optional follow-up hardening.

Game-image identity is NOT carried by this string anywhere: identity comes from
the retained VmScan anchor facts (`NativeLoaderFacts.modules.images[MainExe]`,
`lmf_to_native_loader_facts.cpp:238-247`), and the runtime-module path is
`module_file_path` (our PRX) — both fully distinct, which is exactly the
separation Resolution B must preserve.
