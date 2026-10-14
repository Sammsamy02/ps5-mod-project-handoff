# DIVERGENCE-EXPLAIN — the 4 declared-vs-consumed divergences of the launch-11 one-pass

Worker: worker-divergence-explain · 2026-10-05 · LOCAL ONLY, read-only analysis.
Scope: explain or eliminate every content divergence recorded by the launch-11
combined one-pass finalization block. No source edits, no builds, no console
contact. All hashes sha256; "prefix" = first 16 hex.

Primary evidence:
- `outputs/2026-10-04-zcode-controller/comp6-integration/change-manifest.finalized.json`
  (finalization.divergences, `changed_units_fill`, `packaging_units_fill`)
- `outputs/2026-10-04-zcode-controller/comp6-integration/change-manifest.finalized.json.receipt.json`
  (finalizer tool `worker-manifest-finalizer/finalize_manifest.py` 48654a7b…, ok=true, refusals [])
- Build logs `/tmp/l11-full-fix2.log` and `/tmp/l11-cap-bump3.log` (identical content),
  mirrored under `outputs/2026-10-04-zcode-controller/launch11-combined/` receipts:
  both passes logged `[!] 4 declared-vs-consumed divergence(s) recorded …`;
  candidate-1 D1-only, candidate-2 `verdict: GO (offline gate)`, determinism
  proven `a0dacb3afd3a60be` across run-l11-1 / run-l11-2.
- `outputs/2026-10-04-zcode-controller/launch11-combined/EDITIONS.json`
  (pin set + deliberate-edit notes; post-run `six_edit_fold_2026_10_05` block)
- `outputs/2026-10-04-zcode-team/worker-luamod-lineage/COMPARATOR-REGEN-PROCEDURE.md`
  (sanctioned pin-refresh + declared-divergence comparator procedure)

---

## 1. The 4 divergences, verbatim sources, and verdicts

| # | unit | DECLARED (manifest) | CONSUMED (run-l11-2, finalizer-measured) | Content verdict | Divergence verdict |
|---|------|---------------------|------------------------------------------|-----------------|--------------------|
| 1 | `changed_units[program]` | source `outputs/2026-10-04-zcode-controller/comp6-integration/UE4SSProgram.cpp` — on disk `75abf2ab77791f4e…` | `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` = `7b5d4636919b2707…`; compiled program.o `0d6ed951b43f4361…` (attested in run receipt) | CONTENT DIVERGENCE (`content_identical_to_declared_source: false`) | **EXPLAINED** — deliberate, receipted pad-defer + IniParser-diagnostic fold (see §2.1) |
| 2 | `changed_units[lmf-loader-adapter]` | `outputs/2026-10-04-zcode-controller/comp6-integration/lmf_to_native_loader_facts.cpp` = `b63f482a37d9337f…` (re-verified on disk) | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` = `b63f482a37d9337f…` (re-verified on disk) | byte-identical (finalizer: `content_identical_to_declared_source: true`) | **EXPLAINED — path-only** (spike-tree staging mirror of the authoritative copy; no content risk) |
| 3 | `changed_units[native-loadercontext]` | `outputs/2026-10-04-zcode-controller/comp6-integration/NativeLoaderContext.cpp` = `bc982594c68ce61f…` (re-verified on disk) | `.superpowers/…/src/NativeLoaderContext.cpp` = `bc982594c68ce61f…` (re-verified on disk) | byte-identical (finalizer: `true`) | **EXPLAINED — path-only** (same staging-mirror mechanism as #2) |
| 4 | `packaging_units[lifecycle/runtime_start_bridge.cpp]` | source `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp` — that file on disk = `feed525440233603…` (mtime Oct 4 21:13, never modified) | staged overlay `worker-comp6-driver/staging/lineage-overlay-2/lifecycle/runtime_start_bridge.cpp`, from `launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` = `faec2336fd8f6eb7…` at run time (EDITIONS `bridge_cli` pin; run log `OK runtime_start_bridge_r14_diag_spanpub.cpp faec2336fd8f`) | declared sha == consumed sha (`faec2336…`); the divergence is the declared source NAME vs the staged source NAME. Note the landmine: the named r12 file's content (`feed5254…`) ≠ the declared sha — the pre-final manifest carried `feed5254-pending-final` (r12 prefix placeholder) and the loop-side re-point fixed the sha but not the source path | **EXPLAINED — stale declared-source label** over an explicitly receipted r12→r13→r14→r14_diag_spanpub lineage (see §2.2) |

No divergence in the recorded set is content-divergent without a deliberate-edit
record: #1 has the full pad-defer/diagnostic chain; #2/#3 are content-identical;
#4's declared sha and consumed sha are the same bytes and the lineage is
receipt-attested. **Zero UNEXPLAINED divergences in the recorded four.**

---

## 2. Cause traces

### 2.1 Divergence #1 — program (7b5d4636 consumed vs 75abf2ab declared)

The run consumed the pad-defer worker's staging copy, which is the comp6-program
copy PLUS the admitted pad-defer fold PLUS targeted IniParser diagnostics.
Provenance chain (every step has a durable record):

1. `worker-pad-defer/FOLD.md` §0 (proposal): two-unit fold, staged
   `program-wpd1/UE4SSProgram.cpp` initially `a16c8972e22b9b30…`; program is in
   `RETAINED_REPLACEABLE`, so admission is the existing
   `--replace-runtime-unit program=<staged>` + replace-receipt mechanism.
2. EDITIONS `diagnostic_build_note`: "wpd1 program sha updated for targeted
   IniParser diagnostics (raw exception bytes + settings path/mod_source
   state)" → `7b5d4636919b2707…`.
3. EDITIONS `admissions.pad_defer.files{…/program-wpd1/UE4SSProgram.cpp: 7b5d4636…}`
   (admission gate re-verifies; log: `admission gate OK: pad_defer=ADMITTED`).
4. `launch11-combined/receipts/l11-replace-receipt.json`: the path is recorded
   as `explicit --file admission` with sha `7b5d4636…` at regen time.
5. run-l11-2 receipt: program.o `0d6ed951b43f4361…`;
   `worker-luamod-lineage/offline-proof/sanity-program-baseline/measurement.json`
   recompiled `7b5d4636…` under the run's pinned 172-token argv and reproduced
   `0d6ed951…` byte-exact (`matches_receipt_pin: true`) — the consumed content
   is proven, not assumed.
6. `COMPARATOR-REGEN-PROCEDURE.md` §4.1 states the wiring fact outright: the
   l11 runner passes `--program-source …/program-wpd1/UE4SSProgram.cpp`
   (7b5d4636…), NOT the `comp6-program` copy (75abf2ab…) that EDITIONS units[]
   pins; "The finalized manifest already records this as a CONTENT DIVERGENCE
   for `changed_units[program]`."

Verdict: EXPLAINED (deliberate edit with provenance). Elimination for the next
pass = the loop-side change-manifest re-point (PLAN §1-E) naming the wpd1
staging as the declared source (or annotating both paths), as the procedure's
§5 step 5 already prescribes.

### 2.2 Divergence #4 — runtime_start_bridge (declared r12 vs staged r14_diag_spanpub)

Lineage, each hop receipted:

- `worker-module-start-wiring/runtime_start_bridge_r12.cpp` (`feed5254…`,
  launch-10's wired bridge CLI).
- r13 → r14 lease-coverage hardening: `worker-lease-coverage-fix/`
  (`r13_to_r14.diff`, `fixtures/PASS.json`, worker r14 = `03a9acb1d1b4df67…`);
  the staged file's own header comment names "r14 (2026-10-05,
  worker-lease-coverage-fix): r13 + the lease-coverage hardening" with the L1–L4
  rationale (registration moved INTO scan_image_base, true-protection
  registration, coverage gate, anchor-read gating).
- r14 → `runtime_start_bridge_r14_diag_spanpub.cpp` (`faec2336…`,
  launch10-build/final-units): the launch-10 controller fold adding the
  diagnostic sink calls + spanpub publication naming (`bridge_spanpub_publish`);
  attested as the pinned bridge of the GO run by
  `worker-post-seeds-chain/{CHAIN.md,CLAIM.json,REPORT.md}`
  ("bridge … `faec2336fd8f…`") and by EDITIONS `bridge_cli` provenance
  ("byte-carried from launch10-build/EDITION.json bridge_cli (r14+diag+spanpub;
  passed with all 3 accessors defined)").
- l11 staging: `worker-comp6-driver/staging/overlay-manifest.json` units entry
  `lifecycle/runtime_start_bridge.cpp` ← r14_diag_spanpub `faec2336…`; the
  staged copy on disk still hashes `faec2336…` (mtime 17:37, pre-run).

Bookkeeping failure, not content failure: the pre-final manifest
(`comp6-integration/change-manifest.json`, 10:37) declared the bridge as
`"feed5254-pending-final"`; before the pass the sha field was re-pointed to the
true content `faec2336…` but the `source` path still said r12. The finalizer
correctly recorded the name divergence. The run compiled exactly the pinned
`faec2336…` bytes. Verdict: EXPLAINED (stale declared-source label; deliberate
lineage). Elimination = re-point the declared source to the
`launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` path.

### 2.3 The candidate causes named in the task brief — where each actually sits

- **SeedValidation.hpp kMaxPublishedSpans 16→64**: deliberate, recorded as
  EDITIONS `spanpub_bump` ("the top-16-by-size publication cap was the actual
  blocker; the capture capacity was fine"; discovery chain in
  `worker-capture-22k-fixture/` CAPTURE-CAPACITY + PASS receipts). At run time
  the pins were `b3d81b0f57188d0c…` (units[4]/units[9] + the hardcoded
  `--stage-header` pin), verified OK in the run log — the bump predates the
  L11 pin, so it caused NO run-time divergence. It is NOT one of the 4.
- **kMaxVmMappings / kBridgeVmmapEntries 16384→32768**: deliberate, recorded as
  EDITIONS `capacity_bump` (pool heap 0x22C0000000 coverage), with
  `worker-bss-budget/CLAIM.json` verdict AFFORDABLE (+19.60 MiB BSS, 815x under
  MAXDSIZ). The l11 run still used `live_module_facts.hpp` `917f13a7381a85ca…`
  (anchor-cap 16 + vm 16384, "SAME sha as launch-10", verified OK in-log). NOT
  one of the 4; the bump is staged for the next cycle.
- **LuaMod staging drift (MERGE.md dc687602 vs disk 3c39a86b)**: real, and
  deliberate — the staging was updated post-merge by the codecvt path fix
  (EDITIONS `codecvt_fix` + `luamod_path_fix` + units[2]
  "COMBINED luamod staging …" = `3c39a86b…` + `compose.luamod_merged.sha256`).
  The stale informational field `comparators.luamod_source` (dc687602) was
  explicitly documented as stale in COMPARATOR-REGEN-PROCEDURE §0 ("Informational
  EDITIONS fields NOT gate-enforced (documented for honesty)") and has now been
  refreshed. Reconciled in full in §4. It is NOT one of the 4 (the luamod unit
  is not a manifest changed_unit), but it is exactly the class the brief worries
  about — and it is fully explained.
- **Comparator drift fe4c8113 ≠ b52a3f83**: recorded comparator lineage, not a
  silent drift. 813b249d (M-0, source dc687602, worker-l11-merge MERGE.md §6) →
  b52a3f83 ("codecvt affects all units including it", transport-ledger
  `zcode_codecvt_fix_staged` entry, Helpers/String.hpp 7c983db4) → fe4c8113
  (source 3c39a86b with the local `modbridge::normalize_path_for_lua` override;
  both GO passes measured fe4c8113 identically — run log lines for run-l11-1 and
  run-l11-2; `receipts/luamod-comparator.json`). Path-independence re-proven
  offline: `sanity-current-source/measurement.json` reproduced fe4c8113
  byte-exact from 3c39a86b (`matches_receipt_pin: true`).

### 2.4 Post-run edit wave ("six-edit fold", 19:11–19:26) — recorded, none silent

AFTER the GO run, six deliberate edits landed (controller-owned qualification
fold), all hash-recorded in EDITIONS `six_edit_fold_2026_10_05`
("EDITIONS PINS REFRESHED by worker-luamod-lineage (deliberate, none silent)"):

1. `launch10-build/final-units/SeedValidation.hpp` b3d81b0f→`db07ac602cd9e57a…`
   (v5 publish policy: image-extent overlap excluded; PoolShapeCheck::FailureClass;
   `resolve_listing_gap`; struct 24→32 B) — diff written:
   `worker-luamod-lineage/diffs/SeedValidation.hpp.b3d81b0f-to-db07ac60.diff`.
2. rebase-staging SeedValidation mirror → db07ac60 (byte-identical).
3. `runtime_start_bridge_r14_diag_spanpub.cpp` faec2336→`ec53442b577ef768…`
   (`bridge_kernel_range_verdict`, SYS_mincore via generic trampoline, 16 MiB
   cap, `pool_range_verdict_fresh` tag).
4. `launch11-combined/units/UnrealInitializer.cpp` 22f06286→`e7f349aff7594ba2…`
   (one-shot resolve_listing_gap consultation).
5. `worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp`
   7b5d4636→`68b8bd187531574d…` (setup_mods PROSPERO widen_ascii byte-loop;
   offline object `e05ffa9c3e287ca9…` prebuild-measured).
6. `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp`
   3c39a86b→`c92c92ff66686bf6…` (modbridge:: qualification on ALL 8
   normalize_path_for_lua call sites; offline object
   `8343578bfa735843…` prebuild-measured, UND 540==540 added∅/removed∅,
   defined delta exactly `-RC::normalize_path_for_lua`, consumers still
   referenced — DECLARED DIVERGENCE per procedure §2).

Supporting refresh observed on disk: EDITIONS.json rewritten 19:23 (db07ac60
pins in; the run-verify stage-header pin in `launch11_one_pass.py` refreshed
19:26 to db07ac60; `launch11-combined/units/SeedValidation.hpp` mirror completed
19:26, 36084 bytes = db07ac60). Pre-edit world preserved:
`worker-luamod-lineage/provenance-originals/` (SHA256SUMS.txt +
EDITIONS.pre-six-edit-fold.json).

Two evidence gaps in the fold record (flagged, not silent):
- `diffs/bridge_r14.faec2336-to-ec53442b.diff`,
  `diffs/UE4SSProgram.wpd1.7b5d4636-to-68b8bd18.diff`,
  `diffs/UnrealInitializer.22f06286-to-e7f349af.diff` are still 0 bytes
  (19:21) — the diff half of procedure §3 is unfinished for 3 of the 6 edits
  (I verified independently that the files DO differ; e.g. the bridge delta is
  the +59-line mincore block). The edit hashes themselves ARE recorded in
  EDITIONS, so nothing is unverifiable, but the per-edit hunk receipts should be
  completed.
- `worker-regression-guard/regression-input-manifest.json` still pins wpd1
  `7b5d4636…`; the fail-closed trip is EXPECTED until regenerated (EDITIONS
  `regression_guard_note`; `worker-preflight-runner/results/lineage-gate-probe.json`
  already shows `fail_closed_triggered: true` — the gate chain works).

Peripheral observation (checked, not post-run drift):
`launch11-combined/units/SeedTable.h` = `d183e0c03e4ec756…` (mtime 14:33, BEFORE
the 18:37 run; init-v4r3 lineage) is a different copy from the gate-pinned
`worker-luamod-rebase/staging/initializer-l10-r3/SeedTable.h` = `59effe6e4cd34eb0…`
(which is unchanged and is what the run log verified). Not part of the fold; no
post-run change.

---

## 3. Unexplained-divergence sweep (task item 4)

Method: re-hashed every unit the run's unit-sha verification gate checked, plus
the manifest declared/consumed copies, and cross-checked each delta against a
deliberate-edit record.

- Unchanged since the run (match run-log pins exactly): fixed_l11.py 67b005ef…,
  driver 3ee720e2…, live_module_facts.geometryfix.cpp 440a5786…, diagnostic.c
  f5e96c41…, live_module_facts.hpp 917f13a7…, comp6-program/UE4SSProgram.cpp
  75abf2ab…, package_prx.py e6fbe6c5…, ASMHelper.guarded.cpp 694af016…,
  initializer-l10-r3/{SeedValidation.hpp → db07ac60 mirrored per fold,
  SeedTable.h 59effe6e}.
- Changed post-run: exactly the six_edit_fold set (LuaMod c92c92ff,
  program-wpd1 68b8bd18, UnrealInitializer e7f349af, SeedValidation db07ac60
  ×3, r14 bridge ec53442b) + the runner pin refresh — every one hash-recorded
  in EDITIONS with a reason and prebuild object expectation.

**Finding: no unit exists whose content differs across the run boundary without
a deliberate-edit record. No UNEXPLAINED divergence.** The nearest things to
findings are the three 0-byte diff placeholders and the stale
regression-guard manifest — both already self-declared in EDITIONS, both
fail-closed (the pin gates refuse to pass until refreshed), neither capable of
admitting silent content.

---

## 4. Menu-spawn / LuaMod lineage reconciliation (task item 5)

Chain (no gap; every stage's bytes are preserved on disk):

```
merge receipt        dc68760253ec141f…   worker-l11-merge/MERGE.md (order-independent A->B==B->A,
                                         round-trip to base 04c9d916; M-0 object 813b249d;
                                         work/merge-AB + work/merge-BA still hold these bytes)
   |  codecvt path fix (recorded: EDITIONS codecvt_fix + luamod_path_fix +
   |  comparators.luamod_object_note; Helpers/String.hpp 7c983db4 header fix)
   v
run-time staging     3c39a86b84428e06…   EDITIONS units[2] + compose.luamod_merged (gate-enforced);
                                         run log "OK LuaMod.cpp 3c39a86b8442"; GO passes measured
                                         object fe4c8113 (both runs identical; offline-reproduced)
   |  controller qualification edit 2026-10-05 ~19:1x (recorded: six_edit_fold edit 6;
   |  provenance original captured pre-edit; diff 3c39a86b-to-c92c92ff = exactly the
   |  8 call-site qualification hunks, nothing else)
   v
current staging      c92c92ff66686bf6…   worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp;
                                         offline object 8343578b declared-divergence prebuild
                                         (UND 540==540 added-empty/removed-empty; defined delta
                                         exactly -RC::normalize_path_for_lua; consumers retained);
                                         stale informational comparators.luamod_source refreshed
                                         dc687602 -> c92c92ff with an explicit lineage note
```

The menu-spawn observation "staging 3c39a86b vs MERGE.md dc687602" is therefore
EXPLAINED: MERGE.md's artifact is intact in `work/`, and the staging was
deliberately advanced past it by the codecvt fix before the run, with the drift
documented at the time (units[2] note + comparator note) rather than discovered.
Today's c92c92ff continues the same recorded-chain discipline. **No gap where an
unrecorded change entered**: consecutive-stage diffs exist (merged-vs-base.diff;
3c39a86b→c92c92ff.diff), every intermediate hash is byte-archived, and the
object comparators at each stage were measured, never predicted.

---

## 5. Can the next one-pass start from a divergence-free manifest?

**Yes — conditionally.** Content-wise the tree is now fully pinned: every
compose input has a refreshed EDITIONS sha (19:23), the runner's stage-header
pin is refreshed (19:26), the units/ SeedValidation mirror is in place (19:26),
and prebuild object expectations exist for the two recompiled units (luamod.o
8343578b…, program.o e05ffa9c…) with sanity reproductions of the OLD pins done
first (fe4c8113 and 0d6ed951 byte-exact). The gate chain
(verify_units → admission_gate → build_overlay stage-header pins → luamod
comparator gate → consumer-UND → determinism twin) fails closed on anything
unrecorded, and the preflight-runner probe demonstrates the trip works.

Outstanding bookkeeping before `--full` (all local, none content work):
1. Complete the three 0-byte edit diffs (bridge, program-wpd1, initializer) per
   procedure §3 — worker-luamod-lineage appears mid-task.
2. Regenerate `worker-regression-guard/regression-input-manifest.json` against
   the refreshed editions (expected fail-closed trip until then).
3. Loop-side change-manifest re-point (PLAN §1-E): program entry → wpd1 staging
   path (both paths annotated), bridge entry → r14_diag_spanpub path, luamod
   entry added with provenance diffs and empty import_delta — otherwise the
   finalizer will record divergences again (they would be the declared,
   explained kind, but the REAL one-pass deserves a zero-divergence manifest).
4. Preserve the prebuild comparator declarations as
   `luamod-comparator.prebuild-*.json` before the pass overwrites
   `receipts/luamod-comparator.json` (procedure §2.2 step 6).

With those four items done, the next pass's finalization block should record
zero divergences — or only divergences that are themselves declared in advance
with measured object expectations.

— worker-divergence-explain, read-only; no source, build, or console state was modified.
