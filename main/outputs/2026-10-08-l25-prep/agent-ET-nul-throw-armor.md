# agent-ET — NUL-terminator + never-throw + on_program_start armor (Claude fixes 1-3 + 6, mods-started=0 blocker)

2026-10-09. Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (candidate edited
in place, CHANGES rows rekeyed, consumption law respected — no original/ touched).

## Root cause CONFIRMED in code (Claude's analysis verified end-to-end)

- `NameTypes.hpp:367-370` — the `FName(StringViewType)` ctor forwards
  `str_name.data()` with **no length** into `construct_with_string(const CharType*)`
  → the bound engine ctor scans to NUL. **Every string_view slice into an FName
  is over-read.**
- `UObjectGlobals.cpp` EC long-name arm: `SplitLongNameIntoPathParts("/Script/Engine.Actor:ReceiveTick")`
  → part slice `"/Script/Engine"` → ctor read the WHOLE `"/Script/Engine.Actor:ReceiveTick"`
  → `FNAME_Find` miss → **threw** `"[StaticFindObject] long-name part '/Script/Engine' not in FName pool"`
  — byte-identical to the console refusal in the readfix session log.
- Kill chain to mods-started=0: `update()` → `on_program_start()` (UE4SSProgram.cpp:1612-old)
  → single `TRY` block (1529-old) → `install_lua_mods()` ran → `LuaMod::on_program_start()`
  → `r3_marshal_calibration_consumer()` → `r3_marshal_arm_event_carriers()` →
  `StaticFindObject("/Script/Engine.Actor:ReceiveTick")` (LuaMod.cpp:5176-5190, owned
  StringType — the slice bug is on the UObjectGlobals side) → EC throw → `TRY`
  (ExceptionHandling.hpp — catches `std::exception`, logs, **swallows**) →
  **`start_lua_mods()` never ran** → event loop alive, mods-started=0. The two
  `[R3] marshal carrier candidate not found: Actor:ReceiveTick / AnimInstance:...`
  console lines are consistent: those two candidates are SHORT names (no '/'),
  so they cleanly missed first; the throw fired on the first `/Script/...` LONG
  path in `r3_marshal_arm_event_carriers`.

## FIX 1 — NUL-terminated copies at every slice→FName seam (8 ctor sites + 1 seam helper)

New TU-local helper `UObjectGlobals.cpp:195` `NulTerminatedNameCopy(StringViewType) -> StringType`
(owned, explicitly sized, empty-view guarded). Fixed ctor sites (file:line in the
final tree):

| # | file:line | site |
|---|-----------|------|
| 1 | UObjectGlobals.cpp:283 | `StaticFindObject_InternalNoToStringFromStrings` per-part `FNAME_Find` (vector<StringViewType> seam; in-tree callers pass literals, seam now safe for any slice) |
| 2 | UObjectGlobals.cpp:468-469 | **THE core fix** — EC long-name arm per-part `FName{PartCopy, FNAME_Find}` |
| 3 | UObjectGlobals.cpp:529 | ShortName after last dot (`FNAME_Add` slice) |
| 4 | UObjectGlobals.cpp:534 | ShortName after last colon |
| 5 | UObjectGlobals.cpp:542 | ShortName after last colon (dots+colons mix) |
| 6 | UObjectGlobals.cpp:547 | ShortName after last dot (dots+colons mix) |
| 7 | UObjectGlobals.cpp:705-706 | Prospero `MakeFindXOfClassName` — one copy now feeds BOTH the Find and the Add construction (seals the whole FindFirstOf/FindAllOf string-overload surface) |
| 8 | LuaMod.cpp:1579 | `LuaMod::remove_function_hook_data(std::vector<FunctionHookData>&, StringViewType)` — the raw Lua→C++ view seam (in-tree callers pass owned StringTypes; seam sealed) |

**Audit of every other FName ctor call site in the tree** (all safe, no change
needed): literals (STR("...")) throughout TypeChecker/UE4SSProgram/LuaMod/NameTypes
(NUL-terminated by definition); `const CharType*`/`const TCHAR*` sites
(UObject.cpp GetPropertyByName/GetFunctionByName family, UObjectGlobals 349/424/1150/1180);
`ensure_str(lua.get_string())` sites in LuaMod (3782/3833/3876/4014/4052/2652/2828-2831)
— ensure_str returns an OWNED StringType temporary that outlives the ctor call
(this is the "get_string → FName seam" — already routed through owned strings,
which is exactly why the mangling lived in the UObjectGlobals splitter, not Lua);
`explode_by_occurrence` (LuaMod 2776-2785) returns `vector<basic_string<CharT>>`
(owned); `GetPackageNameFromLongName` returns StringType by value (NUL-terminated);
`UObjectGlobals.cpp:560` `FName(NameView, FNAME_Add)` — full view over the
NUL-terminated `InName` buffer (documented in-code why safe). Systemic note (NOT
applied, by scope discipline): the NameTypes.hpp StringViewType ctor itself
remains the latent hazard for future callers; the call-site law now covers every
in-tree slice.

## FIX 2 — nullptr semantics, never throw (upstream FindObject)

`UObjectGlobals.cpp:466-540`: part-miss → loud `Output::send<LogLevel::Error>`
`"[StaticFindObject] long-name part '<part>' not in FName pool (FNAME_Find miss); trying decoded full-name walk"`
(**no throw**), then the Claude-addendum fallback `StaticFindObject_DecodedFullNameWalk`
(new, `UObjectGlobals.cpp:302-331`): bounded first-match GUObjectArray walk
comparing pool-decoded full names (`GetFullName` = the console-verified in-port
pool decoder; per-object try/catch so one bad object ends only that comparison;
match logged). Clean not-found returns nullptr through the existing fall-through
scan. The two remaining throws in FindObject are unchanged preconditions
(null `InName`), not not-found paths.

## FIX 3 — on_program_start armor + markers (Phase 5/6 pattern)

- `UE4SSProgram.cpp` on_program_start: the single TRY now covers only the
  pre-mod body; the mod-start chain is per-step armored —
  install_lua_mods (1596-1615), LuaMod::on_program_start (1618-1632),
  fire_program_start_for_cpp_mods + start_lua_mods (1635-1648) — each with
  `catch(const std::exception&)`+`catch(...)` catch-and-log. Markers:
  `[L25] install_lua_mods result: {}` (1615), `[L25] start gate: {} started mod(s){}` (1654,
  counts `mod->is_started()` after start), exception logs per step. **Mods now
  start even if any single step throws.**
- `LuaMod.cpp` on_program_start: delete-listener registration armored
  (8188-8205), `r3_marshal_calibration_consumer()` armored (8226-8239), then
  `[L25] carrier arming result: armed|no carrier` (8242/8246) and, when no
  carrier: `[R3] no carrier armed; mods starting anyway (keybinds live, EIGT deferred)` (8247)
  — verbatim Claude text.
- `[L25] on_program_start entered`: the pre-existing marker
  `[l25] on_program_start entered` (UE4SSProgram.cpp:1473, agent-K/P phase tags)
  already fills this slot; kept as-is (case-sensitive grep tooling depends on it)
  rather than adding a near-duplicate line.
- `update()` ticks: exactly the first three loop iterations log
  `[L25] update tick 1|2|3` (UE4SSProgram.cpp:1755-1770 counter), never again.

## FIX 6 — diff vs the l25-era originals (mod-start path)

- `UE4SSProgram.cpp` vs `l25-runtime/.../UE4SSProgram.cpp`: 81 changed lines =
  ET armor/markers only, plus ONE pre-existing l26-era line (1561,
  `to_string(settings_manager.General.InputSource)` in the input-source error
  log — the [EI]-family stringfix, present before this task). update() and
  on_program_start otherwise byte-identical to l25.
- `LuaMod.cpp` vs l25: 56 changed lines = ET edits only (seam fix + armor +
  markers). The on_program_start body is otherwise l25-identical.
- `UObjectGlobals.cpp`: l25 did not select this TU (not present in the l25
  candidate); its CHANGES original is the l22-diagnostics baseline. Diff vs
  that original = EC layer-3 (splitter + long-name arm + GetPackageNameFromLongName
  de-fang) + the CP/DA/EB/ER Prospero FindXOf block + ET edits, nothing else;
  the `FindObject(const TCHAR*)` body diff is exactly EC + ET (verified
  function-scoped).
- **Verdict: no OTHER regression in the mod-start path.**

## VERIFY — host fixtures + compile matrix

`tests/run_et_verify.py` → `results/et-nul-verify/HOST-TESTS.json` — **16/16 entries pass**:

- NEW `tests/actual_et_nul_terminator.cpp` (Claude's exact spec; decoder region
  extracted VERBATIM from candidate/src/NameTypes.cpp with marker+sha pins —
  `results/et-nul-verify/er-region-manifest.json`; the fixture's
  NulTerminatedNameCopy is asserted textually equal to the production helper):
  builds `"/Script/Engine"` as a real non-NUL-terminated slice of
  `"/Script/Engine.Actor"`; RED proves the raw-slice call shape NUL-scans
  through ".Actor" and lands on the whole-path pool entry / FNAME_Find miss
  (the console throw condition), plus the same for `"Actor"` of
  `"Actor:ReceiveTick"` (the R3 candidate shape); GREEN proves the u16string-copy
  construction resolves the package entry and **decodes to EXACTLY
  "/Script/Engine"** (no tail, no truncation); round trips CASE-INSENSITIVE
  ('/script/engine' constructs the SAME entry, stored spelling compares
  case-insensitively equal); empty-view guard. 21 checks, plain + ASan/UBSan.
- UPDATED `tests/actual_staticfindobject_path_fix.cpp` (EC fixture, nullptr
  semantics): mock engine ctor now does a REAL NUL-scan; RED 3 reproduces the
  pre-fix over-read + the old throw; GREEN asserts missing part = loud log (no
  throw) + fallback, the fallback RESOLVES an object whose part is not interned
  (`/Script/Engine.LateLoadedClass`), true miss = clean nullptr, all previous
  EC contracts still hold. Plain + ASan/UBSan.
- ER decoder regression suites happy/refuse/race/truncation × plain/ASan — 8/8
  pass against the current (unmodified) NameTypes.cpp region.

Compile matrix (all on the FINAL edited sources; receipts
`results/ET-TARGET-COMPILE.json`):

| TU | host (EC/L26 recipe: clang++ -D__PROSPERO__ -fshort-wchar, vfs+stubs) | real prospero-clang++ (EU/EC receipt argvs) |
|----|------------------------------------------------------------------------|----------------------------------------------|
| UObjectGlobals.cpp | exit 0, 3 warnings (pre-existing header-level, same as EC control) | exit 0, 0 errors, 5 warnings (all port-tree headers) |
| UE4SSProgram.cpp | — | exit 0, 0 errors, 7 warnings (none in this TU) |
| LuaMod.cpp | — | exit 0, 0 errors, 25 warnings — identical count to the pre-edit l25 control compile (all pre-existing deprecated-conversion/u8path/header warnings) |

## Provenance / hashes (CHANGES.json rows rekeyed on disk)

- candidate/src/UObjectGlobals.cpp `47f5e3c5ab48d2f3b6b08d8530a4fe5fbb3ff67aba6fd379f7870ab9cbf8cc7e`
- candidate/src/UE4SSProgram.cpp `314de5c1320d8a50d9dbb7cb7df51ff21c5b13fa116145ddeeea3b9423264122`
- candidate/src/LuaMod.cpp `d9d96e45997093b4a653d884e544773cf524312b02492b96bc373f9565374354`
- CHANGES.json: scope row `scope_et_nul_throw_armor` added; the three files'
  candidate_sha256 refreshed. New artifacts: tests/actual_et_nul_terminator.cpp,
  tests/run_et_verify.py, results/et-nul-verify/ (manifest + HOST-TESTS.json),
  results/ET-TARGET-COMPILE.json.

NOT DONE (never claim otherwise): no console contact, no twin build, no
packaging, no runtime proof that mods-started flips to >0 on the PS5 — the
root cause is fixed and proven at the host-fixture + real-toolchain compile
level; the next console run decides.
