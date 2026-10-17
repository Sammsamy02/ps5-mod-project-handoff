# MERGE — P-1 / gate M-0 derivation record

Worker: worker-l11-merge · 2026-10-05 · LOCAL ONLY. Mission: merge the two
parallel luamod deltas into ONE staged source and prove it (LAUNCH11-PLAN §0
P-1, §2 M-0, §6 U-1). No composition, no packaging, no console contact.

## 1. Inputs (all sha256 verified before use)

| role | artifact | sha256 (first 16) | verified |
|---|---|---|---|
| BASE | `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp` | `04c9d91664dd82c7` | matches MI RUNBOOK/plan pin |
| DELTA A | `worker-l11-enqueue-ladder/diffs/l11-enqueue-ladder.diff` | `3b19068a194ec82d` | matches plan pin |
| DELTA B | `worker-l11-loadmap-guard/diffs/l11-loadmap-defer-r2.diff` | `d2ad2818db4b7338` | matches plan pin |
| A's own staging (reference) | `worker-l11-enqueue-ladder/staging/luamod-l11-el-ladder/LuaMod.cpp` | `f50976ae1e2626b0` | matches plan pin |
| B's own staging (reference) | `worker-l11-loadmap-guard/staging/luamod-l11-loadmap-defer-r2/LuaMod.cpp` | `9a7a37d6c8377c02` | matches plan pin |

## 2. Hunk-region disjointness (from the diff headers themselves)

- Delta A hunks (old-file): 5004–5010, 5021–5027, 5033–5039, 5050–5056, 5100–5105
  — all inside the `ExecuteInGameThread` binding (~4973–5106), as the plan stated.
- Delta B hunks (old-file): 7321–7326, 8100–8105 — the `on_program_start`
  eager-arm block and its close, as the plan stated.
- Maximum A extent 5105 < minimum B extent 7321: gap ≈ 2216 lines. DISJOINT.
  Both deltas are `__PROSPERO__`-guarded; no hunk shares context lines with the other.

## 3. Application (GNU/BSD `patch`, file operand overrides diff headers)

Fresh byte-verified copies of BASE in `work/merge-AB/` and `work/merge-BA/`:

- **A then B** (`work/merge-AB/`): `patch LuaMod.cpp <A>` rc=0, then
  `patch LuaMod.cpp <B>` rc=0. No offsets, no fuzz, no `.rej`.
- **B then A** (`work/merge-BA/`): `patch LuaMod.cpp <B>` rc=0, then
  `patch LuaMod.cpp <A>` rc=0. No offsets, no fuzz, no `.rej`.

### ORDER-INDEPENDENCE PROOF

`work/merge-AB/LuaMod.cpp` and `work/merge-BA/LuaMod.cpp` are **byte-identical**
(`cmp` clean):

```
dc68760253ec141f2eec4696050b8a2dd9f01b7f2894829d5f543516fb44f7f4  both orders
```

No conflict resolution was needed or performed — the proof required zero
manual intervention.

### 4-way staging consistency (bonus cross-proof)

- EL staging (`f50976ae…`) → merged: changed-line set == delta B exactly (51 lines; `work/el-vs-merged.diff`).
- LG r2 staging (`9a7a37d6…`) → merged: changed-line set == delta A exactly (73 lines; `work/lg-vs-merged.diff`).

I.e. merged = A-staging + B = B-staging + A = base + A + B, all four consistent.

## 4. Round-trip (plan §2 M-0 requirement)

`work/roundtrip/LuaMod.cpp` = merged, then `patch -R <B>` rc=0, then
`patch -R <A>` rc=0 → result is **byte-identical to BASE**
(sha256 `04c9d91664dd82c7…`, `cmp` clean). The staged result round-trips exactly.

## 5. Staged result

`staging/luamod-l11-merged/LuaMod.cpp`
sha256 `dc68760253ec141f2eec4696050b8a2dd9f01b7f2894829d5f543516fb44f7f4`
(8327 lines = base 8207 + 69 from A + 51 from B). See DIFF.md for the hunk
inventory; full diff at `receipts/merged-vs-base.diff`.

## 6. Frozen-argv compile gate (plan §2 M-0 comparator chain)

argv: `worker-luamod-l11-dryrun/scratch/staging/luamod-argv.json` (172 tokens,
verbatim), drift policy per dry-run VERDICT/MI compile-gate.json: ONLY the
source (token 165), `-o` (168), `-MF` (171) tokens redirected; count stays 172.
Compiler: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
with `LLVM_CONFIG=outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`.
My argv receipts: `work/argv-control.json`, `work/argv-merged.json`
(drifted-token lists printed at write time: [165, 168, 171] only).

| step | source | object | result |
|---|---|---|---|
| control | BASE `04c9d916…` | `work/control-luamod.o` | **`854a476bdd4f998581f206af9faaa10b64eba8814fc590350c9565dce47f9565`** — BYTE-MATCHES the MI consumer object (frozen-argv path-independence re-proven) |
| merged | staged `dc687602…` | `work/merged-luamod.o` | **`813b249d0ba3c16a641db53c3c4dc04b52442cccc553b88369d0ca50eb96f4a9`** — THE M-0 COMPARATOR "NEW" (7,039,032 bytes; control 7,034,504; +4,528) |

- control exit 0, merged exit 0; 0 errors both; stdout empty both.
- Warning-kind parity: 23 warnings / 13 kinds on BOTH sides;
  `diff receipts/gate-control.warn-kinds.txt receipts/gate-merged.warn-kinds.txt`
  is EMPTY (exact kind-for-kind, count-for-count parity). All pre-existing
  baseline diagnostics; none introduced by A or B.

Incremental chain status vs plan §2: `04c9d916 → 854a476b (reproduced)` ✓;
`+EL → 8593884e` and `+LG → 3eeba02f` were proven by EL/LG and are consistent
with my staging cross-diffs but were NOT re-compiled by this worker (their
objects are theirs; the merge gate needs control + merged only).

## 7. Symbol / UND proof (plan §2: "no new UND symbols beyond the three already declared")

`llvm-nm -u` sorted sets (`receipts/undefined-control.txt`,
`receipts/undefined-merged.txt`):

- control UND: 568 symbols; merged UND: 568 symbols.
- added merged−control: **∅** (empty). removed control−merged: **∅** (empty).
- The three declared bridge UNDs present in BOTH sets (lineage, not new):
  `_ZN2RC6Unreal20R3MarshalPumpPendingEv`,
  `_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv`,
  `_ZN2RC6Unreal9UFunction10GetFuncPtrEv`.

This satisfies BOTH input claims simultaneously:
- Delta B's claim (LG CLAIM.json: r2 UND identical to control) holds through the
  merge — B adds zero UNDs.
- Delta A's new calls (`is_funcptr_marshal_armed()`,
  `ensure_engine_tick_hooked()`, `ensure_process_event_hooked(mod)`,
  `m_is_engine_tick_hooked`, `m_is_process_event_hooked`, `Output::send`) bind
  exclusively to symbols ALREADY in the control UND/defined set — proven by the
  empty added set, no separate declaration inventory required.
- Defined-symbol shape preserved: all five consumer symbols present in merged
  (`arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
  `r3_marshal_carrier_precheck`, `r3_marshal_calibration_consumer`,
  `k_r3_marshal_carrier_candidates`).
- Marker strings byte-present in the merged object only (UTF-16LE, 1 hit each;
  0 in control): delta A's stall log ("closure enqueued but the EngineTick
  pump…") and delta B's deferral log ("LoadMap detour registrations deferred…")
  — both deltas' runtime-visible content demonstrably compiled IN.

## 8. Host suite re-runs against the MERGED unit

### 8a. worker-l11-enqueue-ladder suite (expected 23/23)

Suite copied byte-for-byte into `work/el-host-test/` (scripts/shims/harness
unchanged; only the hard-coded HT output-root re-pointed at my dir). Regions
re-extracted from BASE + MERGED by the suite's own content-anchored
`extract_regions.py`; ALL 12 region files (2 worlds × 6 regions) are
byte-identical to the suite's original regions (merged "fixed"/binding.inc ==
EL's fixed binding.inc — delta B touches no extracted region). Fresh build,
fresh run:

- control world: exit 0 — **14/14 checks passed, 0 failed** (bug still reproduced).
- merged world: exit 0 — **23/23 checks passed, 0 failed** (armed=enqueued+drained;
  W_FAILS=enqueued+loud-stall, closure never discarded; working world intact;
  address-unavailable ladder fallback intact).
- Both stdout receipts are byte-identical to the EL suite's recorded run.
- PASS receipt: `receipts/el-host-merged-PASS-receipt.txt`.

### 8b. worker-l11-loadmap-guard 5-phase suite (expected 5/5)

Suite copied byte-for-byte into `work/lg-worker/host-test/`
(`test_detour_refusal_r2.cpp` sha `b761074987aff6e5…` verified; ONLY the W=
output-root re-pointed). This suite is LuaMod-independent by construction (it
exercises the real composed polyhook owned-memory detour layer; its connection
to the merged unit is the compile-gated delta-B gate + the §3 cross-diff).
Fresh rebuild of all 11 sha-gated units, fresh run:

- **5/5 phases PASS**, exit 0, ASan/UBSan log 0 bytes (0 errors).
- stdout byte-identical to LG's recorded `run-r2.stdout`.
- PASS receipt: `receipts/lg-host-merged-PASS-receipt.txt`.

## 9. What this proves / does not prove

PROVEN (host): the merged staging exists, is order-independently derivable,
round-trips, compiles clean under the frozen argv with the control byte-matching
the lineage, has an unchanged UND set, and passes both delivered host suites.

NOT PROVEN here (honest): no composition run (controller owns B5); the composed
object comparator for the launch-11 candidate is NOT `633a8c82…` and NOT any
value in this receipt — the controller measures it; no packaging, no gate v2,
no determinism double-pass; no console/in-game behavior of any kind (the tier
oracle of LAUNCH11-PLAN §4 remains the only acceptance path).
