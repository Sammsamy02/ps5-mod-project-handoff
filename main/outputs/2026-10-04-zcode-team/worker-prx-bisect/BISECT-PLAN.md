# BISECT-PLAN — post-fold pre-entry death (worker-prx-bisect, 2026-10-05)

Preparation only. No composition, no packaging, no console contact from this
worker. The controller executes rungs on demand.

## 0. Failure signature and the pre-entry execution surface

Failing candidate: compose `64d8c6e7` (coherent-tick-file-witness-runtime.o,
117,979,024 B), bare-sdk PRX `8ede717f`, bare-original `a978849c`, final
`d6922807`, bridge object `309676bf`, combined `4a218ac3` — live
`launch11-combined/prx-candidate-l11-{1,2}` (post-fold determinism pair, both
passes byte-identical). Death: ploader injection, ZERO sdiag events, no
UE4SS.log line, no runtime-status line.

GO (RAN, 18:37): compose `39c83a3c`, bridge object `195ed762`, combined
`fd2d6eef`, final PRX `f1320b75`, bare-original `ade6151a`, bare-sdk
`69c70d7b` — asides `prx-candidate-l11-1-aside-1791239949/1791242170`,
`prx-candidate-l11-2-aside-1791240003/1791242170` (pair byte-identical; the
live run dirs and live receipts were overwritten by the post-fold passes —
GO comparators MUST be taken from these asides, not from `receipts/`).

What executes BEFORE `module_start_entered` (from
`worker-comp6-driver/staging/lineage-overlay-2/lifecycle/runtime_module_entry.c`):

1. ploader-side processing of the PRX image itself: mapping, relocation,
   import binding (content-dependent; a failure here kills silently before
   any of our code runs — this is the only "zero evidence" window);
2. `module_start` prologue: `entry_guard` atomic test-and-set (.bss),
   `lifecycle.state` checks (.data), `diagnostic_ready()` +
   `diagnostic_open_channel()` (diagnostic.c `f5e96c41`, byte-carried,
   unchanged by the fold);
3. `diagnostic_event("module_start_entered", 0)` — the first event.

NOTHING else runs pre-entry. GO-run sdiag (`launch10-evidence/l11run3-sdiag.jsonl`,
pid 317) proves it: seq 1 = `module_start_entered`, seq 4 = `constructor_count`
= 235, and every `constructor_begin/return` comes AFTER the first event. The
loader does NOT run init_array; the entry shim runs all 235 constructors
later, each exception-guarded. Consequence: **a static-initializer crash
cannot produce this signature** — a ctor crash would land after
`module_start_entered` (+ `constructor_begin`) was already on the wire.

Therefore the killer must change the LOAD-TIME surface (image bytes,
relocations, imports, section/COMDAT layout) or the entry prologue (unchanged
by the fold). Risk ordering below follows load-time surface magnitude.

## 1. Revert matrix (the twelve edits of `seven_edit_fold_2026_10_05`)

Legend: BYTE-EXACT = old bytes verified on disk (sha checked).
RECONSTRUCT = forward/reverse-apply of an archived diff, byte-verified.
SEMANTIC = no old bytes on disk; revert by documented excision; verify by
offline object sha against the GO artifact.

| # | edit | file (canonical path) | before -> after | old bytes | where |
|---|------|----------------------|-----------------|-----------|-------|
| 1 | SeedValidation v5 policy | `launch10-build/final-units/SeedValidation.hpp` | b3d81b0f -> db07ac60 | BYTE-EXACT | `worker-luamod-lineage/provenance-originals/SeedValidation.hpp.orig` (= b3d81b0f) |
| 2 | same, mirror copy | `worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp` + `launch11-combined/units/SeedValidation.hpp` | b3d81b0f -> db07ac60 | BYTE-EXACT | same .orig |
| 3 | bridge range-verdict fn | `launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` | faec2336 -> ec53442b | SEMANTIC (diff archive EMPTY, 0 B) | excision: `#include <sys/syscall.h>` (l.114), `#include <errno.h>` (l.117), `bridge_kernel_range_verdict` block (~l.798-835); proof = offline object == GO `195ed762` |
| 4 | initializer gap-resolution | `launch11-combined/units/UnrealInitializer.cpp` | 22f06286 -> e7f349af | BYTE-EXACT | `worker-init-v4r3/staging/UnrealInitializer.cpp` (= 22f06286) |
| 5 | program widen (ensure_str -> widen_ascii) | `worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` | 7b5d4636 -> 68b8bd18 | BYTE-EXACT | `worker-luamod-lineage/offline-proof/sanity-program-baseline/UE4SSProgram.cpp` (= 7b5d4636) |
| 6 | LuaMod 8 qualifications | `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` | 3c39a86b -> c92c92ff | BYTE-EXACT | `worker-luamod-lineage/provenance-originals/LuaMod.cpp.orig` (= 3c39a86b); diff also archived non-empty |
| 7 | bridge errno-convention fix | same bridge TU | ec53442b -> 59db9dcd | SEMANTIC (no bytes; inside the edit-3 block) | excised together with 3 |
| 8 | SeedValidation hardening | `launch10-build/final-units/SeedValidation.hpp` (+ mirrors) | db07ac60 -> 8a9a1c83 | RECONSTRUCT | b3d81b0f + `diffs/SeedValidation.hpp.b3d81b0f-to-db07ac60.diff` forward-apply reproduces db07ac60 (VERIFIED this session: forward-apply -> `db07ac602cd9e57a...` exact) |
| 9 | program P0/P1 reinstall widen | `worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` | 68b8bd18 -> cb6cb14a | BYTE-EXACT | `worker-luamod-lineage/offline-proof/program-widen-ascii-68b8bd18/UE4SSProgram.cpp` (= 68b8bd18) |
| 10 | String.hpp to_generic_string | overlay `lineage-overlay-2/Helpers/String.hpp` (source `worker-iniparser-repro/staged/Helpers/String.hpp`, runner `--stage-header` pin) | 7c983db4 -> c76be502 | BYTE-EXACT | port original `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp` (= 7c983db4); edit is a pure `#elif defined(__PROSPERO__)` branch insertion (609a610,631) |
| 11 | bridge errno-macro fix | same bridge TU | 59db9dcd -> f9747267 | SEMANTIC (inside the edit-3 block) | excised together with 3 |
| 12 | gate allow-lists | `build_runtime_composition6_fixed_l11.py` + `build_runtime_composition6_l11.py` | (68182543… / unrecorded) -> f1302b3f / c244f2f1 | SEMANTIC | remove `bridge_kernel_range_verdict` from: fixed `linker_provided`, driver l.692 tolerated set + l.733 `_bridge_acc`; restore driver `COMPOSE6_FIXED_SHA` self-pin to the reverted fixed script's recomputed sha. Also restore the composition checkpoint's `undefined-delta.json` `expected_bootstrap_resolutions` to the GO set (drop `bridge_kernel_range_verdict`, keep the four GO resolutions) |

Diff-archive caveat: `worker-luamod-lineage/diffs/bridge_r14.faec2336-to-ec53442b.diff`,
`UE4SSProgram.wpd1.7b5d4636-to-68b8bd18.diff`, `UnrealInitializer.22f06286-to-e7f349af.diff`
are all 0 BYTES (archiver produced nothing for the non-git-tracked files).
They are NOT usable; the BYTE-EXACT recoveries above come from the
`provenance-originals/` and `offline-proof/` copies instead (all sha-verified
this session).

Also on disk and relevant:
- `worker-luamod-lineage/provenance-originals/UE4SSProgram.cpp.orig` = 75abf2ab
  (the A0 pre-wpd1 program; NOT needed for the twelve, listed to avoid confusion).
- 16 byte-exact copies of SeedValidation b3d81b0f exist in
  `worker-comp6-driver/staging/lineage-overlay-2-aside-1791242276..1791247296`.

### Coupling law (which reverts can be combined)

- Edits {1,2,8} (header), {4} (initializer), {3,7,11} (bridge), {12} (gates +
  undefined-delta) form the **v5/bridge cluster**. The armed initializer
  includes `SeedValidation.hpp` and calls `rc_seed::resolve_listing_gap`
  (v5-only) and `extern "C" bridge_kernel_range_verdict` (bridge-only, l.699
  + l.1027-1030). Legal revert states:
  - revert 4 alone -> OK (v5 header + bridge fn stay, merely unused);
  - revert 3/7/11 without 4 -> link failure (caught offline, fail-closed);
  - revert 1+2 (+8) without 4 -> compile failure (caught offline);
  - full-cluster revert = 1+2 -> b3d81b0f, 4 -> 22f06286, 3+7+11 -> excision,
    12 -> de-allow-list + undefined-delta restore.
- {5,9} pair: single ladder only — undo 9 (-> 68b8bd18) or undo 9+5 (-> 7b5d4636).
- {6} independent (comparator 8343578b -> fe4c8113, the GO-pass luamod object).
- {10} independent.

## 2. Ladder (minimize launches)

Risk ordering (load-time surface, high -> low):

1. **Bridge trio {3,7,11} + gate 12** — the ONLY edits that add a new dynamic
   import (`syscall` UND; extended baseline) and rewrite the fresh-compiled
   packaging TU (the bridge object is rebuilt at pass time, 48336 B GO).
   Import binding happens exactly in the zero-evidence window.
2. **String.hpp {10}** — broadest codegen delta: an inline function-template
   branch instantiated under `__PROSPERO__` across every TU that uses the
   print-pipeline conversion; changes COMDAT/instantiation sets image-wide.
3. **SeedValidation {1,2,8}** — struct 24 -> 32 bytes; every consumer TU
   recompiles; contained to initializer + fixtures.
4. **Initializer {4}** — one TU; adds an extern reference + a branch.
5. **Program {5,9}** — one TU; widen helper + lambda; runs post-entry anyway.
6. **LuaMod {6}** — one TU; smallest symbol-table delta (one dead defined
   symbol dropped).

Ladder (expected 2-4 launches):

- **Rung -1 (0 launches, offline forensics — run FIRST):**
  a. Hash the live failing PRXs and confirm the identity table in §0.
  b. `llvm-nm`/`llvm-objdump` diff: GO bare-sdk `69c70d7b` (aside 1791240003)
     vs failing `8ede717f` (live): dynamic symbol/import tables, relocation
     counts by type, section sizes. Any import-table surprise outranks the
     whole ladder.
  c. Confirm entry-prologue inputs unchanged: overlay `runtime_module_entry.c`
     and `diagnostic.c` byte-identity vs GO-era (diagnostic.c pin f5e96c41 in
     EDITIONS units[]; rebuild `runtime_module_entry.o` offline and compare
     against the GO aside's `build/runtime_module_entry.o`).
  d. Rung-0 rehearsal (see rung 0) WITHOUT launching: if the full revert
     reproduces compose `39c83a3c` + bare-sdk `69c70d7b` byte-exactly offline,
     the PRX-content hypothesis is confirmed and rung 0 needs NO launch.
- **Rung 0 — full revert (all twelve).** Expect GO-identical bytes; expect
  OK. Skip the launch if rung -1d already reproduced GO bytes; launch only if
  a byte-exact reproduction is impossible (then it doubles as an
  environment re-baseline under the current loader state).
- **Rung 1 — GO + {10} only** (String.hpp back in at c76be502, everything
  else at GO bytes). Dead => String.hpp is the killer. OK => exclude it.
- **Rung 2 — GO + {3,7,11,12} only** (bridge trio + coherent gates). Dead =>
  bridge/import is the killer. OK => exclude.
- **Rung 3 — GO + {10, 3,7,11,12}** (the two suspects together) — only if
  rungs 1 and 2 are both OK (interaction effect).
- **Rung 4 — GO + everything EXCEPT the rung-1/2/3-eliminated suspects**
  (i.e., {1,2,8,4,5,9,6} as one group; post-entry-logic edits, lowest
  pre-entry risk). Dead => split further (single highest-suspicion member
  first: {1,2,8,4} cluster, then {5,9}, then {6}; one launch each, at most 3).
- Decision rule per launch: first sdiag event MUST be `module_start_entered`.
  Absent => the variant's reverted-IN set contains the killer (or the revert
  was incoherent — re-check offline gates before concluding).

Each rung keeps the OTHER ten edits at GO bytes — every rung is
"GO + subset", never "candidate − subset", so a dead rung convicts and an OK
rung acquits unambiguously.

## 3. Per-rung pin/receipt refresh cascade (checklist — controller executes)

Full skeleton in `bisect_rung.sh` (sibling file). Summary:

1. VAULT: hash-backup every file about to be swapped into
   `worker-prx-bisect/vault/<rung>-pre/` (never edit in place without a
   vault copy; restore path is always open).
2. SWAP bytes into the CANONICAL paths (paths never move, so every pin below
   stays structurally valid): bridge TU, `units/UnrealInitializer.cpp`,
   `units/SeedValidation.hpp`, `final-units/SeedValidation.hpp`,
   `worker-luamod-rebase/.../SeedValidation.hpp`, wpd1 `UE4SSProgram.cpp`,
   `luamod-l11-merged/LuaMod.cpp`, overlay `Helpers/String.hpp` — each cp is
   sha-verified against the matrix target before proceeding.
3. EDITIONS.json: refresh `units[]` shas for every swapped file,
   `bridge_cli.sha256`, `compose.luamod_merged.sha256`,
   `compose.initializer_armed.files`, `admissions.initializer_fold.files`,
   `comparators.luamod_object/luamod_source` (fe4c8113 / 3c39a86b when edit 6
   is reverted), and bump the `edition` marker string with the rung id
   (`worker-prx-bisect rung-N`). The runner's unit-drift gate reads exactly
   these pins.
4. `launch11_one_pass.py`: update the two `--stage-header` shas
   (String.hpp, SeedValidation.hpp). Known drift to fix once at bisect start:
   on-disk runner is `1e7b4831`, EDITIONS still records `c81f89c7` (the
   REQUIRED_BEFORE_ANY_PASS stage-header pin edit post-dated the EDITIONS
   refresh) — record it, then re-pin EDITIONS to the on-disk file.
5. `build_runtime_composition6_l11.py`: refresh `COMPOSE6_FIXED_SHA` to the
   recomputed sha of the (possibly reverted) fixed script; when the bridge is
   reverted also de-allow-list `bridge_kernel_range_verdict` (l.692 + l.733).
6. `build_runtime_composition6_fixed_l11.py`: same `linker_provided` removal
   when the bridge is reverted.
7. `undefined-delta.json` at the composition checkpoint: GO set
   (`bridge_capture_envelope`, `bridge_readable_span_at`,
   `bridge_readable_span_count`, `diagnostic_event`) when the bridge is
   reverted; candidate set (+ `bridge_kernel_range_verdict`) otherwise.
8. OFFLINE PRE-CHECKS (all must pass before `--full`):
   `py_compile` all three scripts; runner unit-sha verification; bridge
   one-TU offline compile under the pinned argv must equal `195ed762` when
   fully excised (byte-exact GO proof; sanity labels first per the
   worker-luamod-lineage methodology); luamod offline object measurement
   (fe4c8113 when reverted, 8343578b otherwise); fixed-edition fresh-deps
   refusal check.
9. Two-pass determinism (`prx-candidate-l11-{1,2}`; never delete run dirs —
   the aside flow accumulates). Record compose/combined/bridge/PRX shas per
   pass; the pair must agree byte-for-byte.
10. Launch + sdiag triage per the decision rule; append the rung verdict line
    to `worker-prx-bisect/RUNG-LOG.md` (rung, variant sha set, first sdiag
    event, verdict).

## 4. String.hpp static-init assessment (explicit answer)

**NOT >50% likely as the killer — static-init is ruled out by direct
evidence; residual risk is load-time layout only.**

- GO sdiag (pid 317): `module_start_entered` is event seq 1; `constructor_count`
  = 235 and all constructor events follow it. The loader runs NO init_array
  before `module_start`; a static-ctor crash cannot precede the first event.
- `to_generic_string` has ZERO namespace-scope callers in the port (grep over
  `native-coherent-runtime-1/source/port`): call sites are
  `LuaLibrary.cpp:58/72/86` (print paths), `UE4SSProgram.cpp:1466`,
  `UEHeaderGenerator.cpp:1359-1401` — all function-local, executed long after
  entry. `ensure_str` callers (21 files) are likewise function-local; the
  namespace-scope grep for static objects initialized via string conversion
  returned nothing.
- The edit itself (609a610,631) is a pure inline-body insertion: no new
  globals, no static data. The only statics in the header (`uestringpool`,
  `uestringpool_lock` in `ensure_str_const`) are function-local, lazily
  guarded, and predate the edit.
- Residual mechanism: image-wide codegen/COMDAT delta in TUs instantiating
  the changed `__PROSPERO__` branch — this can only kill pre-entry through
  the loader (layout/relocation), which is why {10} still ranks second,
  behind the bridge trio's new-import + fresh-TU surface.

## 5. Bounds

- This worker: plan + variants only. No composition, packaging, console, or
  staging contact. All paths above are relative to
  `/WORKSPACE/.codex/worktrees/translation-core/what/`.
- Nothing under any canonical path has been modified by this worker; all
  matrix entries were verified read-only (the two `/tmp` patch trials touched
  copies only).
