# REPORT — worker-luamod-rebase (2026-10-04)

Mode: LOCAL ONLY. Staged rebase + integration path; the controller builds. No
web, no console contact, no composition rebuild, no nested subagents, nothing
executed in-game. All writes confined to
`outputs/2026-10-04-zcode-team/worker-luamod-rebase/`; every input (spike trees,
frozen scripts, the port tree, other workers' outputs) read-only and
byte-verified before use. Deliverable order honored: CLAIM.json first, staged
sources/diffs/drivers/receipts, REBASE.md, INTEGRATION.md, this REPORT last.
Tool calls: ~50.

## 1. What was delivered

1. **The rebased DIFF-1 source** — `staging/luamod-l10rebase/LuaMod.cpp`
   (`4540593ac9e66cdc`), with the anchored 3-way derivation record (REBASE.md):
   overlay lineage `0f0697a3...` + DIFF-1's luamod hunks (byte-identical hunk
   bodies, `cdec0435...`) + the L10LM executor-tag formulation (`70bcb1ab...`).
   Order-independence PROVEN: `rebase(DIFF-1, overlay) == overlay(DIFF-1(base))`
   — both application orders yield `bc082c724776ea97`, and the port-side apply
   reproduces the authoring worker's anchored copy `ce61f02a...` byte-exactly.
   A single-step controller diff (`a06101ec...`) round-trips on a fresh copy.
2. **The composition admission path** (BLOCKER 1 closed per the WGP E0/E1
   new-edition pattern): `build_runtime_composition6_fixed_l10lm.py`
   (`9de2c4c8...` = frozen `8dd92a6f...` + EXACTLY the one-line
   `RETAINED_REPLACEABLE={'program','luamod'}` edit, diff-verified) +
   `build_runtime_composition6_l10lm.py` (`9b332fc5...` = r3-armed driver
   `51646132...` + the six tagged L10LM edit sites, py_compile-clean) +
   `receipts/composition6-replace-receipt.json` (`27c1b3b0...`, 100 files,
   regenerated fail-closed from the r3-armed receipt `67793c0c...` with exactly
   3 staging re-points + 1 luamod source addition). The luamod retained-path
   fresh-dependency gate is fully pre-covered: **1306/1306** pinned `luamod.d`
   deps inside the existing pin layers, 0 rr additions needed beyond the
   replacement source (measured with the regen tool's own coverage logic).
3. **The shared-header resolution** (BLOCKER 3 dissolved): variant B needs NO
   SettingsManager.hpp change — the would-be appended enumerator's value (2,
   appended last per DIFF-1 hunk 1) is provided by a file-local constexpr, and
   **variant A (with the append) and variant B compile to the BYTE-IDENTICAL
   object `5595feb7...`**. Variant A files staged for the record only, with the
   4-unit recompile-set cost documented; NOT admitted by the delivered driver.
4. **Host verification** (L10LM-COMPILE-GATE-2, exit 0, under the exact pinned
   172-token luamod argv + LLVM_CONFIG, the gmalloc-integration recovery
   pattern): control, variant B, variant A + the four comparisons in CLAIM
   L10LM-5.

## 2. The three blockers from the r3-armed CLAIM — status

| blocker | resolution |
|---|---|
| 1. frozen-script eligibility | SOLVED (staged): the L10LM E0 fixed-script edition admits `luamod` via the retained allow-list; the affected-set assertion is untouched; the SEC32-3 argv-drift gate enforces source/-o/-MF-only redirection under the real 172-token argv; receipt coverage via the regenerated 100-file receipt. |
| 2. DIFF-1 base skew | SOLVED (staged, anchored): the rebase onto `0f0697a3...` with new ids (`L10LM-*`), cross-checked byte-exactly in both application orders; no hand-patch anywhere. |
| 3. shared-header recompile set | AVOIDED (proven): the enum append is not landed; equivalence proven at object level (byte-identical variant A/B objects). |

## 3. Measured facts the controller should know (new, this session)

- **Drift witness (§ CLAIM L10LM-6):** the untouched overlay source, compiled
  today under the pinned argv, differs from the receipt-pinned
  `2371839b...` object in EXACTLY 6 bytes — member-offset immediates, each
  +0x30, inside RC::UE4SSProgram header-member functions
  (`can_process_events`, `get_event_loop_thread_id`, `is_event_loop_thread`)
  and `LuaMod::fire_on_lua_start_for_cpp_mods`
  (`receipts/control-vs-pinned.disasm-diff`). Attribution: the rr-attested
  Resolution-B `Platform/NativeProgramContext.hpp` edit (mtime 2026-10-04
  21:41) post-dates the pinned build (2026-10-03 22:04) and is the ONLY
  luamod.d-closed header modified in between; `UE4SSProgram.hpp:127` embeds
  `Platform::NativeProgramContext` by value. **Consequence class:** every
  retained base-1 unit compiled before that edit whose TU closes over that
  header holds the old UE4SSProgram layout while the freshly composed units
  hold the new one. This worker measured the object-level exposure; whether
  any cross-TU consumer pair actually exchanges drifted members is a
  controller-level analysis — NOT claimed as a launch blocker, NOT fixable by
  this worker, and NOT introduced by this staging. The delivered admission
  REPAIRS the luamod instance (fresh compile = current offsets).
- **DIFF-1 link-level inertness (§ CLAIM L10LM-7):** in the fresh luamod object
  the drain cluster (`funcptr_marshal_hook`, `arm_funcptr_marshal_drain`,
  `r3_marshal_carrier_precheck`) and the three `R3Marshal*` bridge references
  are dead-code-ELIMINATED — they are file-static with no TU-internal caller
  (grep-verified: only definitions, comments, and internal calls). What IS
  live: `m_funcptr_marshal_armed`, `is_funcptr_marshal_armed`, the
  `EGameThreadMethod.FuncPtrMarshal` Lua key, the delayed-action executor
  filter, and all five ladder rungs (+1,808 object bytes vs control, zero new
  UND symbols). Consequence: even with DIFF-1 composed, nothing arms the
  marshal or drains the deferred pump until a caller exists — this sharpens
  the r3-armed "no drain consumer" honesty note into a measured symbol-census
  fact. It is inherent to DIFF-1's authored linkage (the variant A object is
  byte-identical; the as-authored port copy cannot even compile without the
  header append — 4 no-member errors, `receipts/gate-portA.stderr`), so it is
  a design decision for the controller/authoring worker; this worker did NOT
  silently change DIFF-1's linkage.
- **UND stability:** fresh luamod UND set == pinned luamod UND set ==
  control UND set (565 == 565, identical name sets). The candidate's import
  contract should therefore hold with the same empty-delta declaration as the
  r3-armed manifest; I1/I1+ will measure it for real.

## 4. Honest limits

- **No composition was run.** No composed L10LM object, no candidate PRX, no
  packaging statics, no gate-v2 verdict, no determinism double-pass exist. The
  driver/edition/receipt are staged and statically verified (diffs, py_compile,
  receipt self-check); the first real compose is the controller's and could hit
  fail-fast cases documented in INTEGRATION.md §3.
- The compile gate is a **host object-level gate only**: it proves the staged
  source compiles under the exact composition argv and characterizes the
  deltas. It proves nothing about in-game behavior, and the control is a drift
  witness rather than a bit-reproduction (the pinned object is not
  reproducible over the current tree — measured and attributed above).
- **No completion/quality/launch claims of any kind.** F-1 stays open; the
  calibration service does not exist; the pump drain is link-inert as authored.
- The 6-byte layout-drift analysis is object-level evidence, not a proof of any
  runtime fault; equally, its absence of analysis for OTHER retained units is
  not an all-clear — that class is flagged for the controller.
- LAUNCH10-MATRIX, the frozen scripts, the spike trees, and all other workers'
  outputs remain untouched; nothing here re-opens L11-5 site ownership.

## 5. File inventory (this dir)

| file | what |
|---|---|
| `CLAIM.json` | written first; 7 claims + non-claims + pins + measured verdicts |
| `REBASE.md` | the anchored 3-way derivation record (versions, deltas, collisions, cross-check, formulation, equivalence) |
| `INTEGRATION.md` | the exact controller runbook (sha gates, optional host gate, one-pass invocation, fail-fast map, expected unit delta, manifest/gate steps) |
| `staging/luamod-l10rebase/LuaMod.cpp` | THE staged rebased DIFF-1 (variant B, `4540593a...`) |
| `staging/luamod-l10rebase-variantA-full/` | fallback variant A (LuaMod.cpp `bc082c72...` + patched SettingsManager.hpp `5b7c50f4...`), NOT admitted by the driver |
| `staging/initializer-l10-r3/` | byte-verified re-stage of the r3-armed initializer staging (5 files; 3 are receipt-covered) |
| `diffs/r3-luamod-funcptr-marshal.luamod-only.diff` | DIFF-1's LuaMod section byte-identical (`cdec0435...`) |
| `diffs/r3-luamod-funcptr-marshal.overlay-rebase.combined.diff` | single-step V3→variantB diff (`a06101ec...`), round-trip verified |
| `diffs/l10lm-executor-tag-formulation.diff` | the anchored formulation edit (`70bcb1ab...`) |
| `build_runtime_composition6_fixed_l10lm.py` | fixed-script edition (`9de2c4c8...`, frozen + 1 line) |
| `build_runtime_composition6_l10lm.py` | one-pass driver (`9b332fc5...`, l10r3 + L10LM E0/E1/E2/E3/E4/E5) |
| `regen_receipt_l10lm.py` | fail-closed receipt regenerator |
| `receipts/composition6-replace-receipt.json` | the regenerated 100-file receipt (`27c1b3b0...`) |
| `compile_gate.py` | L10LM-COMPILE-GATE-2 (control/variantB/variantA + 4 comparisons) |
| `receipts/compile-gate.json` | the gate verdict (exit 0) incl. drift attribution, formulation equivalence, inertness census |
| `receipts/gate-*.o/.d/.stderr/.disasm` | gate artifacts (control `a9716f55...`, variantB/A `5595feb7...`, portA failure stderr, disassembly diff) |
| `receipts/sha-manifest.json` | every input/deliverable/object sha |
| `receipts/patch-A.log`, `patch-B.log` | clean apply logs (A reproduces `ce61f02a...`) |
| `work/` | dry-run trees, cross-check copy, combined-diff round-trip tree (all reproducible from pinned inputs) |
