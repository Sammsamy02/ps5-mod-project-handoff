# worker-predicate-gc-fix — REPORT (GUA GC-order predicate correction)

CLAIM.json was written first; this REPORT is the last artifact. Everything
written lives under `outputs/2026-10-04-zcode-team/worker-predicate-gc-fix/`.
Original worker-5 files untouched (read-only; changes are vendored copies +
diffs). 19 tool calls used (bound: 20). No network, no nested subagents.

## 1. The bug

`worker-5-layout-validator/src/townfall_layout_probes.cpp` predicate chain P3
(layout A, FUObjectArray outer) asserted:

```cpp
if (!(first_gc >= 0 && first_gc <= last_non_gc && last_non_gc <= max_elements))
    → Refuted GOA_R_GC_ORDER
```

The real UE invariant is the opposite ordering: permanent objects occupy
indices [0, ObjLastNonGCIndex]; GC-visible objects start at ObjFirstGCIndex ==
ObjLastNonGCIndex + 1 in the settled state (a gap, first > last+1, is legal
transiently while GC mutates the pool). Under the old rule EVERY settled
FUObjectArray state (first == last+1) is auto-refuted, so layout A could never
validate against a real array. The live capture proves it: at 0x97ba110,
ObjFirstGCIndex=35477 (0x8A95), ObjLastNonGCIndex=35476 (0x8A94) = first-1 —
the normal state — yet the old predicate refuted it.

## 2. Invariant verification (honest citation status)

- Offsets are port-confirmed, identical across all retained 4_07..5_06+ setters:
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/
  unreal/generated_include/FunctionBodies/N_MemberVariableLayout_DefaultSetter_
  FUObjectArray.cpp` line 3: `MemberOffsets.emplace(STR("ObjFirstGCIndex"), 0x0)`
  (ObjLastNonGCIndex = 0x4 on the adjacent line). Binary self-reports engine
  5.6; the first 0x30 bytes are identical across the 4.25/5.06 templates
  (worker-expected-bytes/REPORT.md lines 41-45).
- Maintenance code (GarbageCollection.cpp / UObjectArray index allocation) is
  NOT retained anywhere in this tree — verified by exhaustive grep for
  "ObjFirstGCIndex/ObjLastNonGCIndex/FirstGC/LastNonGC" across the whole
  workspace: only generated layout setters (offsets only) and worker outputs.
  The `ue4ss-unreal-source-access-1/upstream` reconstruction has no GC-index
  logic either. I therefore cannot cite an exact engine line for the
  relational invariant from retained sources, and I do not pretend to.
- The invariant is established instead by:
  (a) upstream UE FUObjectArray semantics (permanent objects [0, Last] are
  excluded from GC; GC-visible objects begin at First; settled state
  First == Last+1), and
  (b) decisive internal consistency of the captured bytes: first = last+1
  exactly, last (35476) <= NumElements (71959) as worker-expected-bytes'
  EXPECTED-BYTES.json line 99 independently requires
  ("u32@0x04 <= u32@0x24 once GC pool is open"), Objects/Max/Num/Chunks all
  sane under layout A (0x13800220a0, 2162688, 71959, 33/2). The old check
  contradicts all of this simultaneously; the corrected check agrees with all
  of it.

## 3. The fix (unified diff)

`townfall_layout_probes_gc_order.diff` — P3 becomes:

```cpp
if (!(first_gc >= 0 && first_gc > last_non_gc &&
      last_non_gc >= -1 && last_non_gc <= max_elements)) {
```

(first > last accepts the settled equality and a transient GC gap;
`last >= -1` admits the no-permanent-objects state first=0/last=-1;
`last <= max_elements` bound kept. RCode GOA_R_GC_ORDER retained, so the
refute-class code stays stable; only its meaning is now correct.)

`test_fixtures_live_replay.diff` — the synthetic fixtures encoded the wrong
invariant (first=0x100 < last=0x200) and were corrected to first=0x201,
last=0x200 across defaults, T3/T5/T8/T9/T10/T11/T13 call sites, T1b, and the
T14 dual-layout image (P0 0x0000002000000010 → 0x0000000F00000010, keeping the
low dword 8-aligned and first=last+1). T6 (last > max) is unchanged and still
refutes via GOA_R_GC_ORDER. New T20 replays the exact live bytes.

## 4. Replay — distinguishable results (replay_results.txt)

Harness `replay_live_gua.cpp`, compiled twice against
`vendored-original/` (buggy P3) and `vendored-fixed/` (corrected P3).
cfgH = exactly the captured span [0x970c000,0x9a34000) RW with the exact 96
bytes at 0x97ba110 (image_base 0x400000, goa_vaddr 0x93ba110).
cfgL = cfgH + reconstructed chunk scaffolding at the observed Objects pointer
0x13800220a0 (chunk table + two zeroed chunk bodies of the needed extents
0x180000 / 0x1C41E bytes). Scaffolding is clearly labeled reconstruction, NOT
live capture — cfgL chunk contents were not retained anywhere.

| Label | Config | Combined | layout_a | layout_b |
|---|---|---|---|---|
| [A] ORIGINAL (historical reproduction) | cfgL | Refuted | **Refuted (GOA_R_GC_ORDER)** | Refuted (GOA_R_PTR_NONCANONICAL) |
| [B] FIXED (pending evaluation) | cfgL | **Validated (OK)** | **Validated (OK)** | Refuted (GOA_R_PTR_NONCANONICAL) |
| [A]/[B] ORIGINAL and FIXED | cfgH | Refuted | Refuted (GOA_R_PTR_OUTSIDE_READABLE) | Refuted (GOA_R_PTR_NONCANONICAL) |

- [A] cfgL reproduces the historical verdict exactly: Refuted via
  GOA_R_GC_ORDER on layout A. The original verdict is preserved, unchanged.
- [B] cfgL: hypothesis A now **Validated** with the live values —
  first=35477, last=35476, num=71959, max=2162688, chunks 2/33, layout
  FUObjectArray_Outer; layout B stays refuted (no ambiguity).
- cfgH row is identical before/after and is honestly reported: with only the
  header span retained, both predicates refute at P1 (Objects pointer bytes
  live outside the retained span), so cfgH cannot reach P3 at all. The
  GC-order discrimination requires cfgL; the cfgL replay's header bytes are
  the real capture, its chunk scaffolding is reconstruction.

## 5. Host test suite (corrected check)

`vendored-fixed/tests/` builds warning-free
(`c++ -std=c++17 -Wall -Wextra -Werror`) and runs **ALL TESTS PASSED: 0
failures — 24 checks** (the original 23-check suite, fixtures corrected to the
real invariant, plus T20). Full log: `test_results_fixed_suite.txt`. Note: the
task brief said "13/13"; the actual worker-5 suite is 23 checks (24 with T20) —
24/24 reported, nothing skipped. T20 asserts the live bytes yield
Validated/layout-A with the exact observed scalars.

## 6. Deliverables (this directory)

- `townfall_layout_probes_gc_order.diff` — the surgical predicate fix.
- `test_fixtures_live_replay.diff` — fixture corrections + T20.
- `vendored-original/`, `vendored-fixed/` — pristine and fixed copies
  (src + tests), plus built `tests/test_layout_probes_fixed`.
- `replay_live_gua.cpp`, `replay_original_predicate`, `replay_fixed_predicate`,
  `replay_results.txt` — distinguishable [A]/[B] replay evidence.
- `test_results_fixed_suite.txt`, `CLAIM.json`, this `REPORT.md`.

## 7. Honest limits

Host-side offline replay only; no live-process run was performed. cfgL chunk
scaffolding is reconstruction (labeled as such); the cfgL Validated claim
elevates the layout hypothesis against the captured header bytes, it does not
by itself prove worker-1 identity/lifetime facts. The relational invariant
could not be cited from retained engine maintenance code (absent from the
tree) — established from UE semantics + captured-data self-consistency, as
detailed in section 2.
