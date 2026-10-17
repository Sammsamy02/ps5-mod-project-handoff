# DRIFT-AUDIT — launch-10 composition layout drift (worker-layout-drift-audit, 2026-10-05)

Object-level adjudication of the worker-luamod-rebase REPORT §3 finding, scoped to the
staged launch-10 composition (`launch10-build/build_runtime_composition6_l10.py`,
sha 399d62c2..., over the frozen `worker-comp6-driver/build_runtime_composition6_fixed.py`,
sha 8dd92a6f...). LOCAL ONLY; no input modified; evidence = receipts, .d files, source
greps, llvm-nm/llvm-objdump reads of existing pinned and run-1 objects.

## 0. Boundary and edit shape

- Edited header: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp`
  — edited sha `0a068bb9...`, mtime **2026-10-04 21:41:25**; pre-Resolution-B baseline
  `66499615...` (= the driver's `ODR_HEADER_UNEDITED` pin). The
  comp6-integration copy `Platform-NativeProgramContext.hpp` is byte-identical
  (`0a068bb9...`) — the ODR gate at `build_runtime_composition6_l10.py:229-251`
  enforces this for the packaging TUs.
- The edit inserts exactly two `std::filesystem::path` fields
  (`configured_game_directory`, `configured_working_directory`) after `mod_source`
  and before `mod_startup_policy` (hunk `@@ -67,6 +67,15 @@`); the second hunk
  rewrites only the inline `ValidateNativeProgramContext` body (no layout change).
  sizeof(std::filesystem::path) = 0x18 on this libc++ target → **+0x30 constant
  shift of every byte offset after the insertion point**.
- `UE4SSProgram.hpp:127` (sha 9529fe76...) embeds `Platform::NativeProgramContext
  m_native_context;` by value (PROSPERO branch) → every UE4SSProgram member after
  the embed shifts +0x30. Measured immediates confirm: `m_event_loop_thread_id`
  0x3c0→0x3f0, `m_mods` 0x3c8→0x3f8, `m_mods_are_being_touched` 0x473→0x4a3,
  `m_processing_events` 0x471→0x4a1. Members before the embed are unchanged
  (`m_input_handler`, `m_event_loop`, `m_render_thread`, `m_is_program_started`,
  statics `settings_manager` / `cpp_mods_done_loading` / `s_program`).
- `NativeProgramContext` fields before the insert (`module_file_path`,
  `game_executable_path`, `module_source`, `scanner_source`, `mod_source`) are
  unchanged; `mod_startup_policy`, `on_engine_initialized`, `input_source` shift +0x30.

## 1. TRUE fresh/retained split (adjudication of the task brief)

The brief's classification does not match the build mechanics. Per the frozen fixed
script, at compose time it recompiles FRESH:

| group | units | mechanism |
|---|---|---|
| extra units | lmf-live-module-facts (geometryfix), lmf-sce-ops, lmf-loader-adapter (edited source), lmf-base-scan, native-query-harness, module-runtime-ack-supplier | `build_runtime_composition6_fixed.py:185-201`, loader-context argv template |
| bootstrap replacements | runtime_owner, ue4ss_backend | `:203-228` |
| affected six + elfsource | unreal-unrealinitializer, native-ownedmappedscansource, native-psscanbinding, native-ownedmappedcandidateselection, native-loadercontext, native-programfactory, native-ownedserializedelfsource | `:118,125,144-162` |
| retained allow-list | program (staged UE4SSProgram.cpp, SEC32-3 own pinned argv) | `:52,164-180` |
| packaging BOOTSTRAP_UNITS | entry (runtime_module_entry_r4.c), bridge (geometrydiag r13), diagnostic (sink staging), probe/*, lifecycle/*, import_check, constructor_boundary, sentinels | `package_prx.py:128-140,645-665` (prospero-clang at packaging time, overlay headers, ODR-gated) |

**RETAINED = the base-receipt pinned objects** (`ue4ss-native-notify-cancel-1/build-1`,
BASE_SHA bcd23a2a...; all compiled 2026-10-03 17:23–22:04, i.e. pre-boundary):
~241 selected_runtime_units + bootstrap `activated_mod_source` + the 15
retained_fixed_dependencies (Rust/libc providers, ABI-independent).

Consequently the brief's six "RETAINED" units (lmf-sce-ops, lmf-base-scan,
native-query-harness, module-runtime-ack-supplier, runtime_owner, ue4ss_backend)
are all FRESH at launch-10 compose time — none is exposed. (In the Oct 4 23:29
run-1 they were compiled post-boundary too; mtimes in the run dir.)

## 2. Per-unit verdicts

Screen: grep every retained unit's pinned `.d` for `UE4SSProgram.hpp` /
`NativeProgramContext.hpp` (necessary condition for layout closure; `-MD` lists the
full transitive include set). Result: 13 of ~241 retained units carry the headers.
All other retained units: **SAFE by construction** (cannot name the shifted layout).

### 2.1 Header-closing RETAINED units (the only candidates)

| unit | link pos (of 368) | .d headers | own-code member access (source grep) | defines stale inline/template copies? | verdict |
|---|---|---|---|---|---|
| cppmod (CppMod.cpp) | 29 | UE4SSProgram + NPC | none (ctor passes `UE4SSProgram&` down; `fire_dll_load_for_cpp_mods` is out-of-line, fresh callee) | YES — 192 defs incl. the 4 event-query inlines + find_mod_by_name clones | **AT-RISK (definer)** — first definer before program; its stale copies win weak resolution |
| luamod (LuaMod.cpp, notify-cancel) | 30 | UE4SSProgram + NPC | **DIRECT**: `get_program().m_mods` in fire_on_lua_start/stop_for_cpp_mods (:6348/:6368); called from start_mod (:6463) and uninstall (:6571) | YES — 194 defs; UNIQUE T-definer of the two fire_* functions | **AT-RISK (execution path + definer)** — proven 6-byte drift; garbage-vector iteration if a Lua mod starts/uninstalls |
| settings (SettingsManager.cpp) | 33 | UE4SSProgram + NPC | `native_mod_source()` inline (reads `m_native_context.mod_source` — BEFORE insert, unshifted); add/remove_mods_directory out-of-line | YES — 193 defs | **AT-RISK (definer only)** |
| program-signatures (Signatures.cpp) | 47 | UE4SSProgram + NPC | `native_mod_source()` inline (unshifted); statics | YES (193) | SAFE own-code / inert definer (post-program) |
| program-object-to-string | 48 | UE4SSProgram + NPC | `settings_manager` static only | YES (192) | SAFE (statics unaffected; post-program definer) |
| generator-sdkgenerator-generator | 201 | UE4SSProgram + NPC | `settings_manager` static only | YES | SAFE (statics; post-program) |
| generator-sdkgenerator-ueheadergenerator | 203 | UE4SSProgram + NPC | statics + `get_game_executable_path()` (out-of-line, fresh callee) | YES | SAFE (post-program) |
| generator-usmapgenerator-generator | 204 | UE4SSProgram + NPC | `get_working_directory()` out-of-line | YES | SAFE (post-program) |
| lua-library (LuaLibrary.cpp) | 205 | UE4SSProgram + NPC | `find_lua_mod_by_name` static out-of-line | YES | SAFE own-code / inert definer (post-program) |
| csv_data_commands | 266 | UE4SSProgram + NPC | `get_working_directory()` out-of-line | YES | SAFE (post-program) |
| native-jmap | 344 | UE4SSProgram + NPC | `get_game_executable_path()`/`get_working_directory()` out-of-line | YES | SAFE (post-program) |
| native-ownedmappedresolvercollector | 349 | NPC only | none (0 hits; includes header for types only) | NO (nm: 0 UE4SSProgram/NPC symbols) | SAFE |
| cppmod-base (CppUserModBase.cpp) | 357 | UE4SSProgram + NPC | `get_program().m_input_handler.get_events_safe(...)` — `m_input_handler` is BEFORE the embed (unshifted); gui/keydown out-of-line | YES | SAFE own-code / inert definer (post-program) |
| activated_mod_source (bootstrap) | bootstrap | none | none | NO (nm: 0 symbols) | SAFE |

"Definer" risk = the object carries stale (−0x30) inline/template copies that can
win weak-symbol dedup; "execution path" risk = the object's own code reads shifted
members at stale offsets. Only luamod has the latter.

### 2.2 Fresh units (all post-boundary at launch-10 compose/packaging)

program (staged), unreal-unrealinitializer (readpath guard), lmf-live-module-facts,
lmf-loader-adapter, lmf-sce-ops, lmf-base-scan, native-query-harness,
module-runtime-ack-supplier, runtime_owner, ue4ss_backend (closes over BOTH headers —
fresh layout), native-ownedserializedelfsource, affected six, entry/bridge/diagnostic
and all packaging units. **SAFE** — fresh compile = current offsets; the ODR gate pins
the port vs staged NPC header byte-identical for packaging TUs.

## 3. Composition-wide mechanism (worse than per-TU skew)

`ld -r` deduplicates the weak inline definitions to ONE survivor per symbol, and the
survivor is decided by link order. In run-1 (`4dd4f309...`, the object launch-9 ran):

| surviving symbol | kind | first definer (link pos) | immediate in run-1 | fresh value |
|---|---|---|---|---|
| `_ZN2RC12UE4SSProgram18can_process_eventsEv` | W | cppmod (29) | **0x471** | 0x4a1 |
| `_ZNK2RC12UE4SSProgram24get_event_loop_thread_idEv` | W | cppmod (29) | **0x3c0** | 0x3f0 |
| `_ZN2RC12UE4SSProgram20is_event_loop_threadEv` | W | cppmod (29) | **0x3c0** | 0x3f0 |
| `_ZN2RC12UE4SSProgram22are_mods_being_touchedEv` | W | cppmod (29) | **0x473** | 0x4a3 |
| `_ZN2RC6LuaMod30fire_on_lua_start_for_cpp_modsEv` | T | luamod (30, unique) | **0x3c8** | 0x3f8 |
| `_ZN2RC6LuaMod29fire_on_lua_stop_for_cpp_modsEv` | T | luamod (30, unique) | **0x3c8** | 0x3f8 |

program.o (fresh, position 40) also defines the four inline functions — its copies
LOSE. `llvm-nm` on run-1 shows exactly one definition of each. Therefore every caller
inside the module — including fresh program.o's `queue_event` (:2855, :2861) and the
ten `is_event_loop_thread()` guard sites (:2267–:2516) — binds to STALE bodies. This
upgrades the luamod worker's "retained vs fresh TU pair" class: the exposure is not
limited to pairs; the single surviving copy is stale for the whole composition.

Failure modes if executed (static offset arithmetic, NOT observed):
- `can_process_events()` reads a byte 0x30 early (inside the NPC tail / pointer
  region) — plausibly always-false → `queue_event` becomes a silent no-op.
- `is_event_loop_thread()` compares the current thread id against pointer bytes as
  `thread::id` → wrong branch on every guard site.
- `fire_on_lua_start/stop_for_cpp_mods` iterate `m_mods` read at −0x30 (device-pointer
  region in the new layout) as `vector<unique_ptr<Mod>>` → dynamic_cast on garbage →
  near-certain crash, IF a Lua mod actually starts or is uninstalled.

## 4. Cross-boundary interface traces (task 3)

1. **Retained → fresh out-of-line calls** (luamod: 18 undefs — queue_reinstall_mod,
   get_working_directory, get_game_executable_directory, generate_lua_types,
   register_keydown_event, ...; cppmod: find_mod_by_name_internal,
   fire_dll_load_for_cpp_mods): all resolve to fresh program.o bodies; offsets are
   resolved inside the fresh callee. **SAFE direction.**
2. **Fresh → retained calls** (program.o → LuaMod::start_mod/update_async/uninstall,
   → CppMod callbacks): parameters carry no shifted members (LuaMod/CppMod/Mod layouts
   are untouched by the edit — they hold `UE4SSProgram&`/`shared_ptr<const
   NativeModSource>`, not the context). The exposure is inside the callee's stale
   inline copies / direct member reads — already counted in §2.1.
3. **NativeProgramContext by-value flows — all fresh-fresh**: bridge geometrydiag →
   `StartActivatedRuntimeWithAck(NativeProgramContext context, ...)`
   (runtime_ack_supplier.hpp:56, fresh extra unit); lmf-loader-adapter (fresh) →
   `MakeNativeProgramContext(NativeLoaderFacts)` (NativeLoaderContext.cpp, fresh
   affected unit) → NativeProgramFactory (fresh) → `UE4SSProgram(NativeProgramContext
   context, ...)` ctor (fresh program.o); ue4ss_backend (fresh replacement) closes
   over both headers at the new layout. No retained object defines or references any
   NativeProgramContext symbol (resolvercollector nm census 0/0). **SAFE.**
4. **Validator**: `ValidateNativeProgramContext` is inline in the header; the
   composed object holds one lambda-copy contributed by a fresh unit. In-game proof:
   launch-9's configured-roots PASS requires reading the NEW
   `configured_game_directory` field — an old-layout copy would have thrown
   "observed game executable path or explicit configured game directory". **SAFE
   (measured via launch-9).**
5. **Ack supplier hooks / query harness API / lmf ops structs**: query-harness and
   lmf-sce-ops/lmf-base-scan .d files contain neither header; the ack supplier's NPC
   by-value is fresh-fresh (item 3). No retained unit sits on any of these paths.

## 5. Launch-9 evidence — what it does and does not prove (task 5)

- Launch-9 ran the staged fself `76ee6af3` (30,773,943 B) =
  prx-candidate-composition6-1, whose build-receipt pins
  `composition_object_sha256 = 4dd4f309...` = run-1 — the exact object whose six
  survivors I disassembled. (composition6-2 pins the older 45dd9901 composition-5-era
  object and was NOT the launch-9 payload.)
- Proven: the mixed old/new-layout composition BOOTS in the real game — 8-plugin
  injection, path validation, configured-roots (Resolution B) PASS, VMMAP walk,
  lease registration, loud fail-closed refusals (composition code=2 lease READ;
  aligned-entry code=1 geometry), zero crashes, green self-contained entry harness.
- NOT proven: anything about the drift. The refusals happen upstream of the
  initializer, so the UE4SSProgram object is never constructed; every one of the six
  stale-offset functions takes the program object as `this` — none ever executed.
  Launch-9 is therefore fully consistent with both "harmless" and "crashes on first
  Lua mod start". It is positive evidence ONLY that the fresh-fresh context chain
  (§4.3/4.4) works end-to-end.

## 6. Remediation (task 4)

Minimal measured fix for the six composition-wide stale symbols: freshen the three
retained definers that precede program.o in link order —

1. **cppmod** (position 29),
2. **luamod** (30) — already staged in the L10LM edition
   (`RETAINED_REPLACEABLE={'program','luamod'}`, one-line driver edit precedent,
   driver `9de2c4c8...`, 1306/1306 fresh-.d coverage pre-measured),
3. **settings** (33).

With all three fresh, the four inline event functions resolve to fresh bodies
regardless of which definer wins, and the two `fire_*` symbols get their only
definer fresh. **Important correction to the L10LM claim:** the staged
`{'program','luamod'}` admission alone does NOT repair the four inline functions —
cppmod.o (29) still precedes program.o (40) with stale copies; it repairs only the
two `fire_*` symbols and luamod's own direct `m_mods` reads.

Admission reality: cppmod and settings are in NEITHER admission set (affected six
nor retained allow-list) — each needs the luamod precedent applied: a one-line
`RETAINED_REPLACEABLE` extension in a new driver edition + replace-receipt
regeneration covering the replacement source and its full fresh-.d closure
(fail-closed gate at `build_runtime_composition6_fixed.py:173-178`). Retained
definers AFTER program (program-object-to-string 48, generator-* 201–205,
lua-library 205, csv_data_commands 266, native-jmap 344, cppmod-base 357) are inert
once program wins — no admission needed for them.

Alternative not recommended: reordering program.o before cppmod would also flip the
winners but changes the pinned link order (input-hashes substitution order) — a
larger, unattested change than three replace-unit admissions.

All remediation is proposal-only; the controller owns the driver edition and the
compose. Nothing in this audit modifies any staged file.
