# Agent AT — code-6 pre-read guard fix (AK item-4 verification finding 2)

Date: 2026-10-08. Target:
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp`
(start sha `4f4afddfd24e213faa2e80bc2ac3d41e23e634446d5f27f23c97ea5b7ac78b09`
verified on disk before editing — unchanged from AK's pin; no drift, no stop).

Files touched: ONLY `candidate/src/UObjectArray.cpp` + `typescan-r4/CHANGES.json`
(pinned scope). AK's raw artifacts under `/tmp/l25-verify-item4/` untouched;
all AT artifacts under `/tmp/l25-at-code6/`.

## Chosen option: MOVE (not the bolt-on pre-check)

The full growth-refusal block — code 5 (GMalloc-bound) and code 6
(ImplausibleDataForGrowth / `l25_data_heap_plausible`) — was hoisted ABOVE
`DeleteListeners.Contains(Listener)`. New callee order:

  sane gate (3) → **5 → 6 (hoisted)** → Contains (4) → RISK / fresh-alloc logs → Append

Justification (also the in-code comment at candidate lines 627-629):
duplicate detection inherently dereferences Data — there is no way to answer
"already present?" without reading the array — so Contains can NEVER safely
precede the screen; the task's fallback (a separate cheap pre-check keeping
Contains first) would leave two overlapping code-6 sites and a still-hazardous
duplicate path, which is strictly dirtier. Moving the term makes refusal 6 a
true pre-read guard: it now executes before ANY dereference of Data.

Code 5 was hoisted together with 6 (immediately ahead of it) to preserve the
AK-verified ordering "refusal 5 wins over 6 when both conditions hold" (AK
section 3, check C6). Code 5 touches only the GMalloc chain — never Data — and
that chain is already dereferenced earlier by `l25_snapshot_gmalloc()` before
the pre-Append log, so hoisting 5 introduces no new dereference.

## What is preserved

- Codes 1-5 semantics: every code keeps its trigger condition, message text,
  and diag tag; sane gate (3) still first; fresh-alloc path (Num==Max==0)
  identical (block is skipped: `pre_max > 0` false); RISK log and fresh marker
  still print after Contains and before Append, so RISK ("proceeding") is never
  printed on a path that then refuses as duplicate; the pre-Append `header pre:`
  line still prints before Append with all 8 fields (untouched, above the gate);
  post-success lines and all marker strings byte-identical.
- Disclosed precedence corner (intended consequence, not a semantic change): a
  duplicate listener at Num==Max>0 with GMalloc unbound now refuses (5) instead
  of (4), and with implausible Data now refuses (6) instead of faulting inside
  Contains. No case that previously refused 5/6 can now refuse something else.

## Verification

1. Region integrity: re-extracted all three AK regions from the edited file.
   Region 1 (helpers, 423-562) sha `23a6fe99...` and region 3 (Remove,
   720-767) sha `e2cef4ea...` are byte-identical to AK's extractions — the
   edit touched only the Add region (now lines 579-718, +7 lines).
2. Compile (logs `/tmp/l25-at-code6/logs/`):
   - host `/usr/bin/clang++` `-fsyntax-only` on the real tree file via the
     `/tmp/l25-combined-smoke` stub recipe: exit 0, 0 errors
     (`at-host-syntax-unreal-uobjectarray.log`).
   - host full `-c` codegen: exit 0, object written
     (`at-host-obj-unreal-uobjectarray.log`, `at-unreal-uobjectarray.o`).
   - real `prospero-clang++` `-fsyntax-only` (receipt argv + l25 -I inserts):
     exit 0, 0 errors, only the 2 known inherited Atomic.hpp -Wswitch warnings
     (`at-prospero-syntax-unreal-uobjectarray.log`).
3. Harness = AK's item-4 harness (`ak_item4_harness.cpp`) copied to
   `/tmp/l25-at-code6/at_harness.cpp`, re-pointed at the edited regions, with
   ONE stub addition: a Contains call counter (`g_contains_calls`, incremented
   on every invocation, guard or not). All AK fixtures rerun verbatim.
   - Plain build: **103/103 PASS** (AK's 92 unchanged + 11 new), exit 0.
   - Prospero-diag build (`-D__PROSPERO__`): **115/115 PASS** (103 + 12 F-tag
     checks), exit 0.
   - Harness proof of the fix (section C):
     - C1/C2 (Data = 1<<47 / kernel-half, Num==Max=2): REFUSED (6), Append
       counter unchanged AND **Contains counter unchanged** — Contains never
       entered on the code-6 path.
     - C9/C10 (AT, stronger): same two fixtures with `g_contains_guard` OFF —
       i.e. the raw dereferencing Z12 Contains stub. On this host those
       addresses are unmapped, so reaching Contains would segfault the harness.
       Result: no segfault, REFUSED (6), Contains and Append counters both
       unchanged — behavioral end-to-end proof the screen fires first.
     - C3 (plausible live buffer, Num==Max=2): still proceeds — Contains called
       exactly once, RISK line printed, exactly one Append, Num 2→3.
     - C6: 5 still wins over 6 when both hold (order preserved); Contains not
       called on that path either.
     - C11 (AT): duplicate at capacity with plausible Data + bound GMalloc
       still refuses (4); Contains called exactly once (duplicate detected),
       Append not called.
     - AK's A/B/D/E sections (growth flag, guarded reads, codes 1-5 battery,
       log fields, post-lines) all pass unchanged.
4. l25 item-6 host suite re-run (`tests/run_tests.py`): PASS plain + asan-ubsan,
   exit 0 (`logs/item6-suite.log`).

## CHANGES.json

- `candidate_sha256` for `candidate/src/UObjectArray.cpp` recomputed in BOTH
  the `changes[]` and `files[]` rows: `4f4afddf...` →
  `694639580d4b10fdda7a03a4b273d7d913e543983ee09b0a1d3ca92d9ddd80a0`.
- Dated `review_fixes` entry added (2026-10-08, agent AT, citing AK finding 2)
  with the ordering, rationale, disclosed precedence corner, and verification.
- Post-edit sweep: JSON parses; `files[]` still array-equal to `changes[]`;
  **58/58 hash pin checks match disk** (54 row pins + 4 added-header hashes);
  no duplicated path+sha pairs; AD's historical description (abbreviated
  `4f4afddf...`) left untouched. Log: `/tmp/l25-at-code6/logs/changes-sweep.log`.

## New TU sha256

`694639580d4b10fdda7a03a4b273d7d913e543983ee09b0a1d3ca92d9ddd80a0`

Not done here (out of scope): target link, packaging, console contact.
