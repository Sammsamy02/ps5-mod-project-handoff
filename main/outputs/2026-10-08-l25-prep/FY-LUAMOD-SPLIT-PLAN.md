# FY — LuaMod.cpp mechanical split PLAN (2026-10-10, plan-only; no split this round)

File: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`
— 455 KB / 9,509 lines / 7.0 MB object at -O0 (`luamod.o`, by far the largest
unit of the 202-fresh-unit full build; next is `lua-luauobject.o` at 1.9 MB).

## 1. Inventory (current line ranges, all inside `namespace RC` unless noted)

| lines | content | notes |
|---|---|---|
| 1–51 | includes (SDK/vendored/UE4SS, `<Mod/LuaMod.hpp>` from the FROZEN vendored include tree) | class decl is NOT in the candidate tree — see constraints |
| 52–168 | `modbridge::` helpers: `normalize_path_for_lua` (52–60), LFB-M wide→narrow bridge (72–168, agent FM) | non-static, external linkage |
| 169–292 | `RC::L23GameThreadProbe` self-test namespace (callback/request, ExecuteInGameThread probe) | self-contained |
| 293–1121 | pre-ctor statics: R3 marshal declarations (294–301, e.g. `auto R3MarshalPublishGameThreadIdentity() -> bool;`), LuaStatics/hook-thread plumbing | mixed decls + defs |
| 1122–1165 | `LuaMod::LuaMod` ctor | class member |
| 1166–1210 | `LuaMod::global_uninstall` | class member |
| 1211–1349 | `ensure_hook_thread_exists` + async-thread bootstrap | free static |
| 1350–1579 | `LuaMod::setup_custom_module_loader` | class member |
| 1580–1645 | `LuaMod::setup_lua_require_paths` | class member |
| 1646–1748 | `LuaMod::get_object_names` + `find/remove_function_hook_data` (7 overloads) | class members |
| 1749–4435 | **`setup_lua_global_functions_internal`** — ONE 2,687-line function: registrations at 1751 `print`, 1759 `StaticFindObject`, 1870 `FindFirstOf`, 1921 `FindAllOf`, 2002–2323 keybinds (`IsKeyBindRegistered`/`RegisterKeyBindAsync`/`RegisterKeyBind`), 2324 `UnregisterHook`, 2420–2494 dumps (`DumpAllObjects`/`GenerateSDK`/`DumpUSMAP`/`DumpJMAP`/…), 2495 `StaticConstructObject`, 2630 `RegisterCustomProperty`, 2897 `ForEachUObject`/`NotifyOnNewObject`, 2986–3269 custom-event + LoadMap/InitGameState/BeginPlay/EndPlay pre/post hook registrations, 3270 `IterateGameDirectories`, 3604 `CreateLogicModsDirectory`, 3675–3726 `ExecuteAsync`/`ExecuteWithDelay`/`LoopAsync`, 3759–3884 ProcessConsoleExec/CallFunctionByName/ULocalPlayerExec hook registrations, 3885–3969 `RegisterConsoleCommandGlobalHandler`/`RegisterConsoleCommandHandler` (console commands), 3970 `LoadAsset`, 4020 `FindObject`, 4192 `FindObjects`, 4355–4434 thread-id queries | free static; single function — cannot be split across TUs without decomposition (see §3) |
| 4437–4444 | `LuaMod::setup_lua_global_functions` wrapper | calls the internal fn |
| 4446–5656 | `#ifdef __PROSPERO__` block: delayed-action state, R3 FuncPtr-marshal state/drain/carriers/calibration consumer (4488–5656 banners), deferred LoadMap arm (4929), `VmtEngineTickDrain` (5193), `RC::Unreal` extensions (5211) | mostly anonymous-namespace statics; some referenced from `on_program_start` |
| 5657–5702 | `LuaMod::ensure_engine_tick_hooked` / `ensure_process_event_hooked` | class members |
| 5703–7604 | **`setup_lua_global_functions_main_state_only`**: `RegisterHook` (5705), `ExecuteInGameThread` (5836), delayed-action API (6093–7491: WithDelay/AfterFrames/Loop*/Get/Set/Pause/Unpause/Cancel/IsValid…/ClearAll/MakeActionHandle), mod management (7502–7548 Restart/Uninstall) | free static; single function |
| 7606–7830 | `setup_lua_classes_internal` (Lua type classes: FText/FString/…) | free static |
| 7831–9509 | `LuaMod` lifecycle: `setup_lua_classes` (7831), `prepare_mod` (7836), fire_on_lua_start/stop (7872/7892), `load_and_execute_script` (7912), `start_mod` (7981), `uninstall` (8086), accessors (8254–8280), **`on_program_start` (8447–9393; carries the ONLY variant token site at line 8455 `#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)`)**, `update_async` (9394), `process_delayed_actions` (9417), `set_on_unload_callback` (9486) | class members; token site MUST stay in the core TU (§4) |

## 2. Proposed TU layout (mechanical, minimal restructuring)

Five split TUs + the core. Byte-move whole functions/blocks; do NOT rewrite bodies
(twin determinism and G10 delta confinement depend on unchanged code).

| new file | moved content | ~lines |
|---|---|---|
| `LuaMod.cpp` (core, keeps unit identity) | 1–1748, 4437–4444, 5657–5702, 7831–9509 | ~3,250 |
| `LuaMod-Globals.cpp` | `setup_lua_global_functions_internal` (1749–4435) | ~2,690 |
| `LuaMod-R3Marshal.cpp` | the `__PROSPERO__` R3/delayed-state block (4446–5656) | ~1,210 |
| `LuaMod-MainState.cpp` | `setup_lua_global_functions_main_state_only` (5703–7604) | ~900 |
| `LuaMod-Classes.cpp` | `setup_lua_classes_internal` (7606–7830) | ~225 |
| (optional) `LuaMod-HookData.cpp` | `find/remove_function_hook_data` + `ensure_hook_thread_exists` (1211–1349 + 1656–1748) | ~200 |

Each TU opens with the same include block (1–51) — self-contained for include
purposes; unused includes in a TU are harmless and keep the argv include set
identical to the `luamod` template (required by the NEW_UNITS mechanism).

Shared symbols across TUs (declarations needed):
- `setup_lua_global_functions_internal`, `setup_lua_global_functions_main_state_only`,
  `setup_lua_classes_internal` — called from the core wrappers (4437/5703/7833/8279).
  Today they are file-`static`; they must become external-linkage (namespace
  `modbridge` or a new `l26split` namespace) with declarations in a new internal
  header, e.g. `typescan-r4/candidate/include/LuaMod-Internal.hpp` (a NEW file —
  must be registered as an `added_header`, see §4).
- `modbridge::normalize_path_for_lua` and the LFB-M wide helpers: definition
  stays in the core (or moves to the existing `LuaFNameBridge.cpp` NEW_UNIT,
  whose header `LuaFNameBridge.hpp` is already an added_header — preferred,
  zero new headers for these), declaration visible to consumers.
- R3-block statics referenced from `on_program_start` (`R3MarshalPumpPending`,
  `try_deferred_loadmap_arm`, …): forward declarations already exist at 294–301
  for exactly this purpose; move those decls into `LuaMod-Internal.hpp`.
- Anonymous-namespace statics: any static referenced from ANOTHER TU must be
  promoted to named-namespace + header decl; everything else stays
  anonymous-namespace in its TU (ODR-safe). Function-local statics
  (`s_object_property_name` …) move with their functions — always safe.

## 3. Why not a finer per-family split (Find*/Console/Keybinds/Thread TUs)

The registration families live INSIDE the two 2,000+ line single functions; a
per-family TU split (LuaMod-Find.cpp / LuaMod-Console.cpp / LuaMod-Keybinds.cpp /
LuaMod-Thread.cpp) first requires decomposing those functions into per-family
`static void register_globals_<family>(const Lua&)` helpers called in the
original registration order. That is a second, higher-risk source edit. The
§2 layout achieves the tooling/edit-latency goals mechanically; the per-family
decomposition can be a later phase ON TOP of it (each family helper then moves
to its own TU unchanged).

## 4. Driver integration — exact steps (ROOT checklist; driver edits are root's)

The split CANNOT ride the existing `luamod` CHANGES row (a row maps one
original → one candidate; the split files have no l25-A BASE original). It uses
the NEW_UNITS mechanism (build_variants.py):

1. **(code agents, coordinated by root)** land the §2 split under
   `typescan-r4/candidate/src/` + `LuaMod-Internal.hpp` under
   `typescan-r4/candidate/include/`. Update the `luamod` row's
   `candidate_sha256` in `typescan-r4/CHANGES.json` (core shrank) and add
   `LuaMod-Internal.hpp` to that part's `added_headers` (compute_selection
   auto-derives `-I` from added-header parents).
2. **(root, build_variants.py NEW_UNITS table)** register the split TUs:
   ```python
   'luamod-globals':{'source':HERE/'typescan-r4/candidate/src/LuaMod-Globals.cpp','template':'luamod','note':'FY split TU: setup_lua_global_functions_internal'},
   'luamod-r3marshal':{...template 'luamod'...},'luamod-mainstate':{...},'luamod-classes':{...}[, 'luamod-hookdata':{...}]
   ```
   Mechanics that then apply AUTOMATICALLY (verified against the driver code):
   - `make_new_units` clones the `luamod` argv, swaps the source, asserts the
     template argv mentions its own source once — true (single `-c` file).
   - `compute_selection` seeds each new unit's dep whitelist from the `luamod`
     template dep set (SDK/vendored includes covered); new include paths under
     HERE (e.g. `LuaMod-Internal.hpp`) are accepted by the attributable
     whitelist (`not p.resolve().is_relative_to(HERE)` exemption).
   - New units are ALWAYS fresh in both full and `--incremental` modes
     (`affected += NEW_UNITS`), never retained.
   - Link: objects appended before the linker `-o`; the BASE[i]↔units[i] zip
     pairing is preserved. G6 census expected = `391+len(NEW_UNITS)`
     auto-adjusts (3 → 3+N, i.e. 394 → 394+N with 4 split TUs); the same
     expression feeds COMPOSITION/TWIN `full_link_inputs` and the
     `--incremental` G6 note. **The `luamod` unit keeps its identity** — the
     existing row still maps the l25 candidate → the shrunken core candidate.
3. **(root, REQUIRED safety patch — the one real hazard)** variant defines do
   NOT reach NEW_UNITS today: `token_targets` maps token-file hits to BASE
   units only, so a split TU containing a variant-token site would compile
   UNGUARDED in BOTH variants (and if the only token site moved out of the
   core, G9 would catch it — but a token site in a split TU while one remains
   in core is a SILENT divergence G10 cannot see). Patch in `build_pass`
   (or `prepare_variant`): for each `NEW_UNITS` entry, read the unit's source
   bytes and add its name to `define_units` if the variant token (B) or
   CTOKEN (A) is present:
   ```python
   for name,spec in NEW_UNITS.items():
       data=Path(spec['source']).read_bytes()
       if variant['token'].encode() in data: define_units.add(name)
   ```
   With the §2 layout this is moot TODAY (the only B-token site, line 8455,
   is in `on_program_start`, which stays in the core TU; CTOKEN does not
   appear in LuaMod.cpp) — but land the patch WITH the split so any later
   move of token-guarded code cannot silently de-variant a TU.
4. **(root) P8 constructor pin**: `l26_packaging_driver.py` P8 requires
   `.init_array == 239` and `.fini_array == 2`. The §2 split moves existing
   statics (init slots MOVE between objects, count unchanged) and adds NO new
   namespace-scope dynamic-init objects — mandate zero new globals so P8 stays
   239/2. If a split TU genuinely must add a ctor, re-baseline P8 239 → 239+N
   with attribution (same discipline as the 2026-10-10 238→239 re-baseline).
5. **(root) other packaging gates**: P0/P1 only existence-check per-variant
   files (order-independent — no variant-order tweak needed for FY's
   concurrent twins); P7 stays 207 exact imports (split TUs reference only
   already-linked static symbols — no new dynamic imports); P9/P10/P11
   unaffected. G7 (`llvm-nm -u` equality with BASE) enforces "no new
   undefined symbols" at build time — the split must satisfy it for free.
6. First validation ladder: `--plan` (census 394+N in the G6 note, 4 new
   compile-unit steps per pass), then `--execute` after archiving the previous
   pair, then `l26_packaging_driver.py --plan/--prepare/--execute` (P8 239/2
   unless step 4 says otherwise).

## 5. Honest win estimate

- luamod compile time is NOT recorded per-unit in receipts
  (`.command.json` stores argv+exit only). Evidence from the l26-B pass1
  object mtimes: 202 objects over ~234 s at `max_workers=2`; `luamod` is
  submitted 31st (starts within seconds of pool start) and lands at t≈+36 s
  with a 7.0 MB object → **~20–40 s, the single slowest unit of a ~234 s pass**.
- Full build: splitting saves ~seconds, not minutes — total CPU is unchanged,
  the wall time is dominated by the ~200-unit cascade, and FY's concurrent
  twins + L26_BUILD_JOBS already parallelize it (~15.6 min sequential →
  ~5–7 min expected). **The big build-time lever remains HEADER cascades**
  (EZ's numbers: String.hpp 196 / Utf16Utf8.hpp 177 / NameTypes.hpp 168 /
  UObjectArray.hpp 162 consumers recompile on header rows), not this file.
- Where the split genuinely pays: (a) edit latency — a .cpp-row edit to one
  LuaMod section rebuilds a ~200–2,700-line TU instead of 9,509 (~20–40 s →
  ~5–15 s per incremental iteration); (b) the 455 KB single-file tooling pain
  (grep/diff/review/context windows); (c) modest parallelism granularity gain
  at high L26_BUILD_JOBS.

— agent FY, 2026-10-10. No sources, CHANGES, or drivers were edited for this
plan; the only file FY edited is build_variants.py (concurrent twins).
