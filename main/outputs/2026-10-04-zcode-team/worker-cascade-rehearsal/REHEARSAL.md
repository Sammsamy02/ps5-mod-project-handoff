# REHEARSAL — pin-cascade dry run for the successor candidate (next-candidate merged luamod + copyin trio)

worker-cascade-rehearsal · 2026-10-05 · PS5 UE4SS runtime port

**DRY-RUN ONLY.** No shared file was edited. Every value below was pre-computed by
reading the live bytes of the named files on 2026-10-05; nothing here is an edit,
a composition, a console step, or a game claim. Companion machine-readable file:
`cascade_values.json` (same directory; 58 pre-computed fields, execution-ordered,
superset of the §4 summary).

Successor candidate = base twelve-edit baseline (live EDITIONS state, post 13b)
+ merged luamod `facf993a…` (worker-next-candidate) + copyin trio
(worker-copyin-unit, **shas PENDING — PLACEHOLDER tokens below**) + the pin
cascade instantiated from today's controller precedent
(`seven_edit_fold_2026_10_05` edits 1–13b, edit 12 sanctioned allow-list
extension, and `MANIFEST-REPOINT.seven-edit-fold.json`).

---

## 0. Gates — ALL must hold before a single pin is touched

| Gate | Test | Status at rehearsal |
|---|---|---|
| G0.1 | Loading regression resolved by **controller declaration** (`outputs/2026-10-04-zcode-controller/incident-loading-regression`) | NOT declared — blocked |
| G0.2 | worker-copyin-unit REPORT + receipts landed; staging hashes **frozen** | NOT landed — staging churned three times under inspection (§1.1) |
| G0.3 | Copyin trio **rebased onto the live 13b baselines** | NOT done — ADMISSION-NOTE diffs target pre-13b bytes (§1.2) |
| G0.4 | Fresh luamod comparator prebuild measured (§5) — never a hoped-for sha | NOT run |

## 1. Point-in-time evidence (read-only observations, 2026-10-05)

### 1.1 Copyin staging churn (why every copyin value is a PLACEHOLDER)
| File | next-candidate.json snapshot (20:38–20:47) | ADMISSION-NOTE.md | live at rehearsal |
|---|---|---|---|
| staging/SeedValidation.hpp | `444f2722…` | `444f2722…` | `c301b09e…` |
| staging/UnrealInitializer.cpp | `d0dcf411…` | `d0dcf411…` | `22bc2651…` |
| staging/runtime_start_bridge_r14_diag_spanpub.cpp | `25232995…` | `4f2a1408…` | `7b97b95f…` |

Files changed UNDER inspection between snapshots ⇒ the worker's own final report
supersedes; **no successor pin may be written until G0.2 freezes the bytes.**

### 1.2 Baseline skew of the staged copyin diffs (mandatory rebase)
The copyin diffs are derived on **pre-13b** baselines that no longer exist as live:
- bridge diff `f9747267… → 4f2a1408…`: the live bridge is `abeaa06a…` (edit 13b
  *removed* `bridge_kernel_range_verdict` + the mincore path; the copyin hunk is
  "+208 after bridge_kernel_range_verdict" — its anchor is gone).
- initializer diff `e7f349af… → d0dcf411…`: the live units initializer is
  `1660b749…` (13b).
- SeedValidation diff `8a9a1c83… → …`: baseline still live (`8a9a1c83…`
  re-hashed at all three copies) — this one is current.
⇒ Re-derive all three diffs on the live baselines before admission
(next-candidate `copyin_crosscheck.risks[0]` predicted exactly this).

### 1.3 Live pin values verified on disk this rehearsal
| Artifact | Live sha256 (verified) |
|---|---|
| EDITIONS.json | `e9bac35c4bfd5704f1b2612511ffc7db96525b5864b468de2ba3b8c4ed802327` |
| launch11-combined/units/UnrealInitializer.cpp | `1660b749c86e4944fb1ea93b622b8d84b93ac745697ec08c9fb8bb39453e148a` |
| launch11-combined/units/SeedValidation.hpp | `8a9a1c83b676f223db9cc733437f2663eece1fbd09f79215563c0dfc9af923c2` |
| launch10-build/final-units/SeedValidation.hpp | `8a9a1c83…` (same) |
| worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp | `8a9a1c83…` (same) |
| launch11-combined/units/SeedTable.h | `d183e0c03e4ec7568e2e1416787599f8c9f363f5a940cfc47a2a31ff04ca3cda` |
| worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp | `c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0` |
| driver build_runtime_composition6_l11.py | `c244f2f1c3f6c0cc84d152ee9a5b4d798139a871f72a11320b1c6408eabec328` |
| fixed edition build_runtime_composition6_fixed_l11.py | `f1302b3f02ef3adec8e8ed321495bd7ceeba19551512675a099af089f23861ad` |
| runner launch11_one_pass.py | `1e7b4831e2944853a56fa5202a9f773b2d2164b71664c3edfb1b3dba521f5650` |
| bridge (launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp) | `abeaa06a49879922cccb1c8ec56a91097532124f13b92384a583e27c7f06704c` (per EDITIONS bridge_cli) |
| String.hpp (codecvt_fix staged) | `c76be502fbe6f9c2c6589f141c1caded84af482a1f03a428005574850ca47788` (UNCHANGED) |
| wpd1 program staging | `cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af` (UNCHANGED) |

## 2. The driver allow-list check the task ordered (flag result)

Current driver tuple, `build_runtime_composition6_l11.py:692`
(stage-aware reconciliation allow-list):
```
('bridge_capture_envelope', 'bridge_readable_span_at',
 'bridge_readable_span_count', 'diagnostic_event',
 'bridge_kernel_range_verdict')
```
Current driver pre-proof bridge-definition set, `:733`:
```
_bridge_acc = {'bridge_capture_envelope', 'bridge_readable_span_at',
               'bridge_readable_span_count', 'bridge_kernel_range_verdict'}
```
Current fixed-edition `linker_provided`, `build_runtime_composition6_fixed_l11.py:283`:
```
{'_DYNAMIC','bridge_capture_envelope','bridge_readable_span_at',
 'bridge_readable_span_count','diagnostic_event','bridge_kernel_range_verdict'}
```

**FLAG — extension REQUIRED, precedent = today's edit `12_gate-accessor-allowlist`
(bridge_kernel_range_verdict, "same class as the span accessors") and the l10
`diagnostic_event` addition:** the copyin bridge defines
`bridge_kernel_write_copyin_probe`, and the copyin initializer additionally
declares/calls the accessor `bridge_write_copyin_sink_bytes` (ADMISSION-NOTE §3/§4
hunk 1 + anchor hunk 4). Neither symbol is in any of the three sets above. When
the trio lands, the composed runtime object's raw undefined-delta will add both
⇒ `expected_bootstrap_resolutions` (fixed_l11.py:287 derives it from
`linker_provided`) will name them, and the driver reconciliation tuple will
refuse unless extended. Exact symbol set must be re-verified on the REBASED
initializer (rebase may rename/drop; never extend on prediction).

Rule (precedent discipline): **ADD** the new symbols; do NOT remove
`bridge_kernel_range_verdict` residue as a drive-by — allow-list residue is
harmless, an unrequested semantic change is not.

## 3. Cascade gaps found (the precedent missed these)

1. **Runner-sha doc drift (gap).** `EDITIONS.editions.runner.sha256` records
   `c81f89c7…`; the live runner is `1e7b4831…` — the SeedValidation stage-header
   pin refresh (REQUIRED_BEFORE_ANY_PASS) edited the runner without refreshing
   the editions doc field. Same drift for `editions.driver.sha256` (`3ee720e2…`
   vs live `c244f2f1…`) and `editions.fixed_edition.sha256` (`67b005ef…` vs live
   `f1302b3f…`). Not gate-enforced (verify_units checks units[] + bridge_cli
   only) but the successor cascade must sweep all three or the drift compounds.
2. **Stale header-sha doc fields (gap).** `stage_header_pins_carried_from_l10.
   SeedValidation.hpp.sha256 = f09ec9b3…` (pre-v5; live pin is `8a9a1c83…`) and
   `compose.initializer_armed.files['SeedValidation.hpp'] = db07ac60…` (edit-8
   value; live rebase-staging copy is `8a9a1c83…`). Both documentation-only
   (the operative pins are the runner `--stage-header` line and the receipt
   re-measure) — refresh, and label them as swept-stale so nobody "fixes" them
   to the wrong generation again.
3. **Second copyin symbol (gap).** The task context names only
   `bridge_kernel_write_copyin_probe`; the staged initializer hunks also
   reference `bridge_write_copyin_sink_bytes`. A single-symbol extension would
   fail the undefined-delta gate at pass time. Both-or-verified-set (§2).
4. **Replace-receipt merge reason strings are stale labels** (runner
   regen_receipts cites `b8d851e9`/`59effe6e` for the armed staging). Bytes are
   re-measured so coverage is correct; refresh the strings only if the
   controller touches that block anyway.
5. **Regression-guard pin already tripping.** `worker-regression-guard/
   regression-input-manifest.json` pins `editions.sha256 = aacbe5dd…`; live
   EDITIONS is `e9bac35c…`. Fail-closed trip is EXPECTED per the EDITIONS
   regression_guard_note; the successor re-pin can only be measured AFTER the
   successor EDITIONS lands (PLACEHOLDER `PH_EDITIONS_POST_SHA`). Suite inputs
   themselves are current (wpd1 `cb6cb14a…`, String.hpp `c76be502…` — verified).
6. **`runtime_start_bridge.h` pin check (open).** Driver DEFAULT_STAGE_HEADERS
   pins `runtime_start_bridge.h = 4bb02413…`. The staged copyin bridge diff is
   .cpp-only (1 hunk, SYS_open via the existing generic trampoline, header-only
   constants) ⇒ pin should survive; verify on the rebased bridge that no header
   (event-code table, prototype) changed. If it did, that header becomes a NEW
   pin site the precedent does not have a step for.

## 4. Pre-computed pin cascade — execution order

Full machine-readable form in `cascade_values.json`. Phases in the order the
controller must execute them. `PH_*` = placeholder pending a pending input.

### Phase 1 — file placements (verify hash before replacing owned files)
- F1 `launch11-combined/units/SeedValidation.hpp` := copyin v5.1 bytes
  (`8a9a1c83…` → `PH_COPYIN_SEEDVAL_SHA`)
- F2 `launch10-build/final-units/SeedValidation.hpp` := same bytes (three-copy
  mirror, procedure §4.3)
- F3 `worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp` :=
  same bytes — or recorded per-copy divergence decision (procedure §4.3(c);
  ADMISSION-NOTE item 2)
- F4 `launch11-combined/units/UnrealInitializer.cpp` := rebased copyin
  initializer bytes (`1660b749…` → `PH_COPYIN_INITIALIZER_SHA`); driver argv
  line 397 path stays
- F5 bridge: RE-POINT `EDITIONS.bridge_cli.file` to the frozen copyin staging
  path (alternative: copy into `launch10-build/final-units/` in place, 13b
  precedent — pick ONE and record it)

### Phase 2 — executable pin edits (driver + fixed edition + runner)
- P1 fixed_l11.py:283 `linker_provided` += `bridge_kernel_write_copyin_probe`,
  `bridge_write_copyin_sink_bytes` (verified set) — FLAGGED, edit-12 precedent
- P2 driver:692 reconciliation tuple += same two symbols — FLAGGED
- P3 driver:733 `_bridge_acc` += same two symbols — FLAGGED
- P4 driver:414 luamod `--replace-runtime-unit` path →
  `outputs/2026-10-04-zcode-team/worker-next-candidate/staging/luamod-next-candidate-merged/LuaMod.cpp`
- P5 driver:403–404 pin comments: source `facf993a…`, expected object
  `PH_PREBUILD_LUAMOD_OBJ` (fill AFTER the §5 measure — never prediction)
- P6 driver:94 `COMPOSE6_FIXED_SHA` → `PH_FIXED_POST_SHA` (recompute after P1;
  edit-12 precedent note)
- P7 runner:164 `--stage-header SeedValidation.hpp=…=8a9a1c83…` →
  `PH_COPYIN_SEEDVAL_SHA` (controller-owned executable pin; build_overlay
  refuses drift)

### Phase 3 — luamod comparator prebuild (BEFORE any EDITIONS comparator write)
See §5 commands. Evidence set: new source+object sha, UND added ∅ / removed ∅ vs
the `8343578b…` object, the three consumer symbols still referenced, sanity
reproduction of `8343578b…` byte-exact from `c92c92ff…` first.

### Phase 4 — EDITIONS.json regen (fields 14–42 in cascade_values.json)
units[] luamod row (path+sha+what), units[] initializer/SeedValidation×2 rows,
bridge_cli (path+sha+provenance), compose.luamod_merged (path+sha+lineage+
freshen_closure re-verify), compose.initializer_armed sweep, comparators
(luamod_object ← measured, luamod_source ← `facf993a…`, lineage note append),
admissions.initializer_fold.files ← `PH_COPYIN_INITIALIZER_SHA` + evidence,
stage_header_pins doc sweep, seven_edit_fold NEW entry `13c_next-candidate-
integration` (sha256_after = measurement, never prediction), prebuild_object_
expectations['luamod.o'] ← measured, REQUIRED_BEFORE_ANY_PASS refresh,
editions.runner/driver/fixed doc shas ← recomputed.
**Unchanged (verify-only):** `admissions.pad_defer` (wpd1 `cb6cb14a…`,
native-pad-input `ac8325f7…`), `codecvt_fix` (`c76be502…`),
`comparators.consumer_und_symbols` (3 mangled names), `fold1_asmhelper`,
`admissions.pad_coexist`.

### Phase 5 — change-manifest re-point (`comp6-integration/change-manifest.json`)
- `changed_units[luamod]`: source → merged staging path; receipt →
  `receipts/luamod-comparator.prebuild-next-candidate.json`; import_delta empty
  both directions (declared, prebuild-proven)
- NEW `changed_units[initializer]` + `changed_units[SeedValidation]` entries
  (copyin trio = ONE atomic entry set, provenance = the three rebased diffs)
- `packaging_units['lifecycle/runtime_start_bridge.cpp']`: source → copyin
  bridge staging (`PH_COPYIN_BRIDGE_PATH`); lineage append
- `import_delta.note`: append the copyin bootstrap-resolutions expectation
  (driver `--extend-undefined-baseline` wired at :364; declared sets NOT widened
  silently — the sanctioned §2 extension is the only widening)
- `changed_units[program]`: **UNCHANGED** (`sha256_after` wpd1 `cb6cb14a…`;
  `PENDING-FROM-ONE-PASS` fields refill at pass time via finalize_manifest.py)

### Phase 6 — receipts + regression guard
- Receipts: **NO manual edits.** `regen_receipts` at `--full` re-measures and
  renames priors `.prev`. Requirements already satisfied by F1–F5 + P4: merged
  LuaMod in the `--file` list via the repointed `compose.luamod_merged.file`;
  copyin files at runner-covered paths (`_pf` list lines 82–94 covers
  `units/UnrealInitializer.cpp` + `units/SeedValidation.hpp`; bridge via
  `bridge_cli`). `freshen-deps.txt`: extend ONLY if the merged/rebased `.d`
  walk adds files (never bypass).
- Pre-pass comparator receipt: copy the prebuild declaration to
  `receipts/luamod-comparator.prebuild-next-candidate.json` BEFORE the pass
  (the runner overwrites the live `luamod-comparator.json` at pass 1).
- Regression guard: after the successor EDITIONS lands, re-pin
  `regression-input-manifest.json → editions.sha256 = PH_EDITIONS_POST_SHA`
  (reported regen, not silent). Expected fail-closed trip until then.

### Phase 7 — pre-flight and the pass (procedure §5 order)
```
cd /WORKSPACE/.codex/worktrees/translation-core/what
# 1. HOLD-PASS verify (units + verify-only; refuses on any drift):
python3 outputs/2026-10-04-zcode-controller/launch11-combined/launch11_one_pass.py verify
# 2. comparator pre-agreement: offline label == new EDITIONS pin byte-for-byte;
#    receipts/luamod-comparator.prebuild-next-candidate.json present;
#    admission_gate preconditions (pad_defer ADMITTED, initializer_fold resolved)
# 3. only then the full pass (admission_gate -> regen_receipts -> verify-only
#    -> pass1 -> luamod comparator gate vs NEW pin + consumer-UND gate + ODR +
#    finalize-manifest + verify_accessors + gate-v2 -> pass2 determinism twin):
python3 outputs/2026-10-04-zcode-controller/launch11-combined/launch11_one_pass.py --full
```
`--full` acceptance: gate GO ×2 (D1-only failure on candidate-1 accepted),
`determinism(o1) == determinism(o2)`, luamod unit object == `PH_PREBUILD_LUAMOD_OBJ`
(measured), 3/3 consumer UNDs resolve, 0 `R3Marshal*` UND, accessor-verification
PASS ×2, finalize fills every `PENDING-FROM-ONE-PASS` (any declared sha must
EQUAL measurement or the tool refuses).

### Phase 8 — POPUP / runbook expectations (documentation, controller lane)
- Success provenance line changes: listing-gap success message becomes
  `"kernel-write-copyin-read (listing-gap owned-read probe)"` instead of claiming
  span membership (initializer hunk 5) — runbooks/POST-POPUP discrimination
  tables must expect it.
- New diagnostic tags: `pool_copyin_probe_fresh` (1/0/2/3) +
  `pool_copyin_probe_rc` / `pool_copyin_probe_errno` / `pool_copyin_probe_sink`.
- POPUP path itself unchanged: `[Lua] Loaded` banner (String.hpp print pipeline,
  `c76be502…` unchanged), OPTIONS-press discrimination per
  `worker-post-popup-evidence/POST-POPUP.md` unaffected.
- First on-target run of this candidate IS the design note's in-game smoke
  (one planted game-side write-copyin + one known-unmapped); standalone payload
  results do not preview in-game source-class semantics. Console steps remain
  the controller's per the RUNBOOKs — never this rehearsal's claim.

## 5. Comparator prebuild command shape (S7a — verbatim shape)

```
# sanity first: same tool must reproduce 8343578b… byte-exact from c92c92ff…
python3 outputs/2026-10-04-zcode-team/worker-luamod-lineage/offline_object_hash.py \
  --run-dir .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-2 \
  --unit luamod \
  --source outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp \
  --label sanity-current-source

# then the merged source (ONLY source/-o/-MF redirected from the pinned 172-token argv):
python3 outputs/2026-10-04-zcode-team/worker-luamod-lineage/offline_object_hash.py \
  --run-dir .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-2 \
  --unit luamod \
  --source outputs/2026-10-04-zcode-team/worker-next-candidate/staging/luamod-next-candidate-merged/LuaMod.cpp \
  --label luamod-next-candidate-merged
```
NEVER valid: reusing `8343578b…` (c92c92ff object) or `761b610a…` (r3-only
6f97f8a1 object) or rollback comparators `8593884e…` / `3eeba02f…`; pinning a
hoped-for object sha anywhere; editing the runner to skip the gate; widening
`retained_units_replaced`.

## 6. Honesty ledger

- Dry-run only: no file outside this worker dir was written; no composition, no
  console, no packaging, no game claims, no quality scores.
- Every "current" value above was read from live bytes on 2026-10-05; every
  copyin successor value is a PLACEHOLDER until the copyin report lands, hashes
  freeze, and the trio is rebased on the 13b baselines.
- The merged LuaMod has NO PROSPERO compile gate yet — the §5 prebuild is its
  first target-toolchain evidence; this rehearsal predicts nothing about the
  measured value beyond the next-candidate UND expectation (added ∅ / removed ∅).
- Gate-v2/determinism/accessor outcomes are acceptance-time facts, never
  rehearsal predictions.
