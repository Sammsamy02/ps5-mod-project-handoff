# REPORT — worker-combined-editions (2026-10-05)

LOCAL PRE-BUILD ONLY. No composition run, no packaging, no console contact. All writes
confined to `outputs/2026-10-04-zcode-controller/launch11-combined/` plus this dir;
launch10-build read-only (copy-from). CLAIM.json written first; this REPORT last.

## 1. Verdict

**DELIVERED.** The launch11-combined driver + fixed edition + one-pass runner are built,
py_compile-clean, sha-pinned in `EDITIONS.json`, and diff-audited against their l10
sources. The away-loop's compose is **pure execution once two recorded admissions
resolve**: `admissions.pad_defer` (PENDING — the pad worker's FOLD.md decides the exact
unit; placeholders in all three places) and `admissions.initializer_fold` (PENDING-ACK —
one conscious lineage decision, see §3). The runner structurally refuses `--full` until
both resolve (plan A3 discipline, executed and proven during the pre-build).

## 2. Deliverables (all under launch11-combined/)

| file | sha256 (16) | what |
|---|---|---|
| `build_runtime_composition6_fixed_l11.py` | `6e943678a927db37` | l10 fixed edition + ONE comment-block insertion (hunk 44a45,84; zero executable change). Carries the frozen base, the l10 `linker_provided` extension, allow-list `{'program','luamod','cppmod','settings','asmhelper-asmhelper'}`, and the pad/fold1/luamod admission notes. |
| `build_runtime_composition6_l11.py` | `c94110d2b8ecc3d8` | l10 driver + exactly five tagged deltas (L11-E1..E5, §4). Everything else byte-carried: PKG_FIXED pin, bridge CLI, 14-symbol chain, 31-header staging defaults, ODR/receipt/edited-landed/entry gates, artifacts hook, stage-aware lineage wrapper, diagnostic sink, D1 reuse fix. |
| `launch11_one_pass.py` | `c18f52ed7ac0e037` | l10 runner adapted: candidates `prx-candidate-l11-{1,2}`, run dirs `run-l11-{1,2}`, `admission_gate()` (A3), `luamod_comparator_gate()` + `consumer_und_gate()` (plan D) wired into post_build, l11 receipt names, compare-dir `prx-candidate-l11-1`. SeedValidation `f09ec9b3` / live_module_facts `67170b21` stage-header pins BYTE-CARRIED (same shas; same units). ODR/finalize/gate-v2/accessor/determinism stages byte-carried. |
| `EDITIONS.json` | — | base shas (+ l10 drift note), edition shas, 14 verified units, comparators, admissions, 5 placeholders, 6 exact loop edits. |
| `diff_{fixed,driver,runner}_l10-to-l11.diff` | — | full diffs; every removal audited (driver removals = exactly the 5 delta anchors; runner removals = exactly the listed adaptations). |
| `freshen-deps.txt` | — | copied from launch10-build (1322 entries; verified below). |

Smoke tests executed (read-only): all three modules import; `verify_units()` verifies
**14/14 units** against EDITIONS.json; `admission_gate()` refuses PENDING with exit 1;
driver `COMPOSE6_FIXED_SHA` re-measures to the on-disk fixed edition; `compose_argv`
redirects assert to the merged-luamod path (sha `dc687602` re-measured), the armed
initializer, the fold-0 asmhelper, and the byte-carried bootstrap pair.

## 3. Key findings (loop must read before --full)

1. **Initializer lineage conflict (structural, decision gated).** The merged luamod
   odr-uses `RC::Unreal::R3MarshalPumpPending` / `R3MarshalPublishGameThreadIdentity`
   under `__PROSPERO__` (mangled UNDs measured in worker-l11-merge receipts); the plan-D
   pairing law therefore REQUIRES the armed initializer — the l10 driver's byte-carried
   launch-10 final-units initializer (`4a2b9400`, v4-folded, NOT armed) can never pass
   the D gate. The plan's A0 armed staging (`8b0c7e99`, `R3_MARSHAL_MODE=1` fallback) is
   however **PRE-launch10-final-c**: vs the shipped launch-10 unit it lacks the GMalloc
   V1/V2 validators, the v4 pool predicate and the `read_bytes_at` entry-0 anchor. **No
   v4+armed fold exists on disk (verified 2026-10-05).** Resolution: the driver defaults
   to the plan's pinned A0 staging (keeps compose pure-execution), the caveat is recorded
   at the redirect (L11-E2) and in EDITIONS.json, and the runner refuses `--full` until
   the loop sets `admissions.initializer_fold` = `ACK-PLAN-PINNED` (plan path, caveat
   consciously accepted) / `ADMITTED-V4ARMED-FOLD` (new worker-delivered staging; re-point
   L11-E2) / recorded `WAIVED`. Precedent for the default: worker-launch10-r3-armed
   composed this exact staging on the launch-10 pair (367/368 units identical, bridge
   object byte-identical, gate v2 GO 15/15). This delta is LISTED (mission requires
   diff-and-list of every delta); it was not silently byte-carried because a byte-carried
   initializer would compose a candidate that fails the mission-mandated D gate by
   construction.
2. **Fold-1 comparator correction adopted** (coordinator notice + FOLD1-READY.md): the
   editions reference ONLY `6604ce2d423c5d0dee4de6a089bd8141126e0fb02e7538ae08a4076d9b5
   af8d5` (basename-preserving one-token flip of the SAME staged `ASMHelper.guarded.cpp`)
   as admissible; `f5627427` is recorded REJECTED (renamed-probe basename violation).
   Fold-1 is a placeholder comment only (L11-E5); the default stays fold-0 (`81dd832b`,
   staging `694af016`, confirmed reproducing). Not needed for the combined candidate.
3. **launch10 bookkeeping drift (found, recorded, NOT fixed there):**
   `launch10-build/EDITION.json` pins `l10_driver_sha256 399d62c2…` but the on-disk
   proven driver is `f4103572…` (the validator-v4 fold updated the driver after that
   EDITION.json was written). Base chosen = the on-disk file; its `COMPOSE6_FIXED_SHA`
   pin `b78fb031` matches the on-disk l10 fixed edition byte-for-byte.
4. **Merged-luamod dependency closure verified:** the merged staging's `.d` walk is 1306
   files, ALL covered by the copied `freshen-deps.txt` (1322) except the merged source
   itself (regen adds it via `--file`). A future pad/backend TU that adds includes needs
   its files appended (loop edit; the fixed edition refuses unpinned fresh deps loudly).
5. **Driver write-scope note (byte-carried, run-time):** like the l10 driver, MY stays
   `worker-comp6-driver`, so compose-time logs/overlay/chain-symbols write there (keeps
   the stage-aware wrapper's hardcoded overlay path valid). No pre-build write occurred
   outside my scope.

## 4. Driver delta inventory (complete; diff file is authoritative)

- **L11-E1** `COMPOSE6_FIXED`/`COMPOSE6_FIXED_SHA` → `6e943678…` (this dir's fixed edition).
- **L11-E2** initializer `--replace-runtime-unit` → `worker-luamod-rebase/staging/
  initializer-l10-r3/UnrealInitializer.cpp` (`8b0c7e99…`), with the recorded caveat
  comment (§3.1) and the rr-coverage of its SeedValidation/SeedTable handled by the
  runner's regen merge.
- **L11-E3** luamod `--replace-runtime-unit` → `worker-l11-merge/staging/luamod-l11-merged/
  LuaMod.cpp`, with source pin `dc687602…` and expected unit object `813b249d…` as
  --require-symbol/pin comments (plan comparator discipline; `8593884e`/`3eeba02f`
  documented as rollback-only).
- **L11-E4** "LAUNCH11 PAD PLACEHOLDER" comment block after the `ue4ss_backend` bootstrap
  line (BACKEND/BOOTSTRAP lineage per plan §0.5/A3; both fill shapes documented;
  companion placeholders in the runner's regen and EDITIONS.json admissions).
- **L11-E5** FOLD-1 asmhelper placeholder comment (default OFF; `6604ce2d` admissible;
  `f5627427` rejected).
- Header comment block rewritten (l10 header + l11 lineage/deltas). **Nothing else.**

## 5. Comparator enforcement wired into the runner (post_build)

- `luamod_comparator_gate`: run receipt `retained_units_replaced == ["luamod","program"]`
  (plan D); `selected_runtime_units.luamod.source_sha256 == dc687602…`;
  `object_sha256 == 813b249d…` AND the object re-measured from disk equals it. Drift =
  STOP, evidence receipt `receipts/luamod-comparator.json`.
- `consumer_und_gate` (plan-D pairing law): on the composed runtime object,
  `_ZN2RC6Unreal20R3MarshalPumpPendingEv`, `_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv`,
  `_ZN2RC6Unreal9UFunction10GetFuncPtrEv` must NOT be undefined (armed initializer +
  unreal-ufunction unit), and NO `*R3Marshal*` symbol may remain undefined; evidence in
  `receipts/consumer-und-resolution.json`.
- Byte-carried gates continue unchanged: ODR emitter check, finalize, gate v2 (D1-only
  tolerance on candidate 1), the three accessor verification classes, composed-object
  determinism across the two passes.

## 6. Non-claims

No composition run; no packaging; no console/target execution. `813b249d` is a HOST
measurement by worker-l11-merge — not yet reproduced by a launch-11 composition; the
runner enforces and would STOP on drift, never widen. No launch-11 composed-object
comparator exists yet (`633a8c82` dry-run comparator invalid on this lineage per plan D).
No in-game acceptance, no >8 quality, no completion claim. The pad unit is NOT admitted;
fold-1 is NOT activated. launch10-build is untouched.
