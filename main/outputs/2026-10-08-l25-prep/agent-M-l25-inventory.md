# Agent M — l25 runtime prep tree inventory / gap report

Snapshot taken 2026-10-08 14:05–14:08 CDT. Read-only: no file in
`2026-10-08-l25-runtime/` was touched. All sha256 values below were recomputed
from disk and matched their CHANGES.json entries unless stated otherwise.

TREE: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/`

## 1. Inventory by subdirectory

### Root
- `build_runtime.py` — 13,349 B, mtime 13:53:30. Parent twin-build driver. Reads
  `profile/`, `typescan-r4/`, `thread/` CHANGES.json, asserts original/candidate
  hashes, builds a merged header VFS overlay from the l24-build-r2 composition,
  recompiles affected units in 2 passes, full-links 391 inputs, asserts import
  symbol delta == 0 and twin identity, and writes
  `outputs/2026-10-08-l25-build-r2/`. **That output dir does not exist yet —
  the script has not been run** (`OUT.mkdir(exist_ok=False)` would fail on a
  rerun over an existing dir).

### typescan-r4/ — Claude items 1–4 + 7
Completed worker's finalization wave ended ~13:54 (REPORT.md 13:53:39,
`original/` provenance copies 13:54:09). Continued writes after that (see §4).
- `include/NativePropertyPointerGuard.hpp` — 4,033 B, 13:49:19. Declares
  `ReadPointerChecked/ReadClassChecked/ReadNextChecked` (lines 86–88),
  `BeginScanCollection/EndScanCollection` (46–47), `class ScanCollectionScope`
  (55–60, non-copyable, dtor calls End).
- `include/NativePropertyPointerGuard.inl` — 21,159 B, 13:50:15. Implements the
  Checked variants with `record_refusal`; `known_property_class_index_match`;
  `fname_index_within_verified_pool` (live CurrentBlock/CurrentByteOffset reads,
  fail-closed); `KnownPropertyClassBootstrap` at line 194 — (a) exact known-name
  index compare first, (b) suffix only after the pool bound; single-shot
  `PublishVerifiedPoolAddress` (CAS, first-wins); `classify_class_refusal`
  (SlotRead/ClassNull/ClassMisaligned/ClassIdentity).
- `candidate/src/TypeChecker.cpp` — 68,931 B, **14:00:52 (active write)**.
  Scan loop: `parse_property_pointer_refusal` lambda (line 50);
  `RefusalLog store_all_object_types_scan_refusals` (128); per-object `try`
  ending 754 with `catch (const std::runtime_error&)` at 756–767 (records
  `[PropertyPointer]` refusals with parsed reason/field/class, rethrows
  everything else); `ReadClassChecked`/`ReadNextChecked` calls at 249, 290–293;
  `ScanCollectionScope` at 779; refusal summary print 947–961. sha256
  `bb91f45d…` matches the 14:01:58 CHANGES.json entry (hash was recomputed after
  the last edit — write appears to be a tracked finalization, not torn).
- `candidate/src/UnrealInitializer.cpp` — 137,417 B, 13:52:18.
  `NativePropertyGuard::PublishVerifiedPoolAddress(pool_runtime)` at line 1105,
  immediately after FName pool admission, before any ToString suffix path.
- `candidate/src/CustomProperty.cpp`, `src/Property/{FMap,FSet,FArray}Property.cpp`
  and the six `generated_include/MemberVariableLayout_*` wrappers —
  byte-identical to their l24 originals (original_sha256 == candidate_sha256);
  carried, not re-authored.
- `original/` — 12 provenance copies of the l24 originals (13:54:09).
- `REPORT.md` — 13:53:39. l25 header paragraph states: "Nothing here has been
  host-tested, target-compiled, packaged or run on console in the l25 edition
  yet."
- `CHANGES.json` — 23,914 B, **14:01:58 (active write)**. 12 rows + 4 added
  headers with hashes (all verified in sync with disk), scope covering items
  1–4+7, explicit NOT-YET-DONE list (no host re-runs, no target compile, no twin
  build, no packaging, no console proof).
- `target_compile.py` — 1,105 B, **13:55:40 (active write)**. New script:
  recompiles each l25 candidate .cpp with the l24-build-r2 composition argv +
  l25 include dirs, writes `results/TARGET-COMPILE.json`, asserts all exit 0.
  **Not yet run** — only `results/inherited-l23-r2/TARGET-COMPILE.json`
  (l23-era) exists. Note it resolves paths from `Path.cwd()`, so it must be
  launched from the worktree root.
- `tests/` + `results/inherited-l23-r2/` — all mtime 13:48:37: l24/l23-era host
  suites, binaries and objects. STALE for l25 (scope text: `actual_scan.cpp`
  still extracts the l24 scan body).

### profile/ — Claude item 5
Last write 13:43:50 — ~22 min before snapshot; this worker's run looks finished
(no writes in the 10-min window).
- `profiles/townfall-build-facts.json` — 80,238 B (l23 original 75,099).
  `required_members[69..71]` = FMapProperty::KeyProp (112,8), ValueProp (120,8),
  FSetProperty::ElementProp (112,8); `evidence/l25_containers/promoted_totals`:
  FMapProperty::UEP_TotalSize=160, FSetProperty::UEP_TotalSize=144.
- `units/CompiledNativeLayoutDeclarations.hpp` — 2,918 B. INI section:
  `[FMapProperty] KeyProp=0x70 ValueProp=0x78 UEP_TotalSize=0xa0`;
  `[FSetProperty] ElementProp=0x70 UEP_TotalSize=0x90`; plus
  `FFieldClass UEP_TotalSize=0x40`. Embedded digest
  `4667891671fda00328bc61e1029c86058b1f558df4c4f4ae0922aa18ddd13317` matches
  CHANGES.json `profile_digest` and RESULTS.json.
- `units/JoinedLayoutPreflight.hpp` — rows 75–77: the three container members,
  `Disposition::Required`, offsets 112/120/112, group `startup_core_map_set_members`.
- `units/RuntimeBuildProfile.hpp` — 17,930 B (l23 original 15,052).
- `generate_overlay.py` (13:43:45), `original/` l23 copies, `tests/`
  (run_tests.py 11,565 B 13:41:15, preflight.cpp, digest_gate.cpp, binaries).
- `tests/RESULTS.json` — 9,734 B, 13:43:50: **PASS_HOST_ONLY_MAC**. 10 preflight
  scenarios PASS (66 keys / 53 required / 11 conditional / 2 deferred); digest
  gate compiles+passes against the l25 units and correctly FAILS (exit 1)
  against the l23 units; 15 join decision controls including refusal of
  conflicting FMapProperty::KeyProp facts and unagreed class totals; l24 sibling
  corroboration `container_ini_sections_identical: true`. Limits field: official
  PS5 compiler measurement still pending.
- `CHANGES.json` — 3,953 B, 13:43:48; all 4 candidate hashes verified in sync.

### tests/ — Claude item 6
Last write 13:43:52 — also looks finished; no writes in the window.
- `run_tests.py` (2,575 B, 13:43:18) — compiles `test_scan_refusals.cpp`
  **against the l24 typescan-r3 include dir** ("unmodified, read-only
  reference"; path hardcoded to `2026-10-08-l24-runtime/typescan-r3/include`).
- `test_scan_refusals.cpp` — 15,358 B, 13:43:25. Cases: (a) unknown-yet
  *Property passes bootstrap without CastFlags; (b) refusals carry
  reason/field/class detail; (c) refusal ends only its own chain; (d) RAII
  scope resets scan mode during unwind.
- `RESULTS.json` — 4,363 B, 13:43:52: PASS_HOST_ONLY_MAC in plain and
  asan-ubsan. `tested_headers` hashes are **l24** (guard .inl
  `69075e73…` vs the l25 `c7217cbd…`). Notes field states the l24 candidate
  TypeChecker pairs Begin/End without RAII and to "adopt the scope in the next
  runtime revision" — i.e. the suite predates the l25 guard/TypeChecker edits
  (13:49–13:50, 14:00).
- Binaries: `test_scan_refusals-plain` (338,968 B), `-asan-ubsan` (946,592 B) + dSYM.

### thread/
- `CHANGES.json` — 858 B, 13:53:34. Rewritten l25-accurate scope ("thread part
  unchanged from l24… if l25 later changes thread files, add rows…"), `files: []`,
  include dirs now point at l23 thread include + l25 typescan-r4/include. The
  stale l24 text is gone.

## 2. Item → file mapping and status

| # | Item | Status | Evidence |
|---|------|--------|----------|
| 1 | Refusal detail wired into real scan path (Checked variants) | DONE (code; host re-run pending) | TypeChecker.cpp:249,290–293 use `ReadClassChecked/ReadNextChecked`; guard .inl implements them with `record_refusal`; trailer parsed at TypeChecker.cpp:50 |
| 2 | Per-object catch during scan | DONE (code; host re-run pending) | TypeChecker.cpp try 258–754, catch 756–767; non-`[PropertyPointer]` errors rethrow |
| 3 | RAII Begin/EndScanCollection | DONE (code; host re-run pending) | `ScanCollectionScope` NativePropertyPointerGuard.hpp:55–60; used TypeChecker.cpp:779 |
| 4 | Bootstrap check order (exact name first, then suffix) | DONE (code) | `KnownPropertyClassBootstrap` guard .inl:194–226 ((a) index match, (b) pool-bounded suffix); `PublishVerifiedPoolAddress` called UnrealInitializer.cpp:1105 |
| 5 | Container offsets + UEP_TotalSize | DONE (overlay; host PASS) | See §3 |
| 6 | Host tests for items 1–5 | PARTIAL — PASS but against l24 headers for items 1–4; item 5 tested against real l25 units | tests/RESULTS.json PASS_HOST_ONLY_MAC (l24 header hashes); profile/tests/RESULTS.json PASS (l25 units) |
| 7 | Fix stale CHANGES.json text | DONE | thread/CHANGES.json rewritten, mtime 13:53:34 |

## 3. Item 5 — do the offsets actually resolve?

Yes, end-to-end on host, in the current tree state:
- Generation: `profile/profiles/townfall-build-facts.json` → `generate_overlay.py`
  → `profile/units/CompiledNativeLayoutDeclarations.hpp` INI carries
  `FMapProperty KeyProp=0x70 (112) / ValueProp=0x78 (120) / UEP_TotalSize=0xa0
  (160)` and `FSetProperty ElementProp=0x70 (112) / UEP_TotalSize=0x90 (144)`;
  digest `4667891671…` consistent across facts, CHANGES.json, declarations and
  RESULTS.json.
- Preflight: `JoinedLayoutPreflight.hpp:75–77` requires all three members
  (Required, 112/120/112); 10 host scenarios PASS, including map_key_missing /
  map_key_mismatch / map_value_mismatch / set_element_missing / container_absent.
- Digest binding: the digest gate compiles green with the l25 units and red
  (exit 1, static_assert) with l23 units — the runtime is actually pinned to
  these values.
- Consumer side: `typescan-r4/{original,candidate}/generated_include/
  MemberVariableLayout_{Header,Src}Wrapper_{FMap,FSet}Property.hpp` (byte-identical
  to l24) expose `GetKeyProp/GetValueProp/GetElementProp` resolving via
  `MemberOffsets.find` + `NativePropertyGuard::ReadPointer`, and
  `static int32_t& UEP_TotalSize()` exists for both classes — the positive totals
  in both new INI sections also cover the latent NativeLayoutBinding::Prepare
  UEP_TotalSize bug per the profile CHANGES scope.
- Not yet proven on target: no l25 target compile or console run has consumed
  these values; promotion provenance is generic-platform compiler + static image
  agreement only (official PS5 compiler measurement pending).

## 4. Files modified in the last 10 minutes (cutoff 13:55:43, snapshot 14:05:43)

- `typescan-r4/target_compile.py` — 13:55:40
- `typescan-r4/candidate/src/TypeChecker.cpp` — 14:00:52
- `typescan-r4/CHANGES.json` — 14:01:58

All three are one worker's target: l25 items 1/2 finalization in TypeChecker +
hash recompute in CHANGES.json (hashes verified in sync) + preparation of the
l25 target compile (`target_compile.py`, not yet executed). No reads beyond
identification were made of these. Note: profile/ and tests/ workers last wrote
13:43:48–52 (~22 min ago) — their RESULT files show completed passing runs; if
they were expected still in-progress, they appear to have finished.

## 5. Gap list before an l25 build attempt

1. Item-6 suite vs actual l25 headers: tests/run_tests.py hardcodes the l24
   typescan-r3 include dir and its RESULTS.json records l24 header hashes; the
   l25 guard headers differ. Re-run (and extend for item 4's pool-bound and
   item 5's offsets) against `typescan-r4/include` before treating item 6 as
   certifying the shipped code.
2. typescan-r4 full host suite re-run: `tests/actual_scan.cpp` still extracts
   the l24 scan body; binaries under `typescan-r4/tests` are l24-era
   (13:48:37); no l25 HOST-TESTS.json exists (only inherited-l23-r2).
3. Target compile: `target_compile.py` present but unexecuted; no l25
   `results/TARGET-COMPILE.json`. (The active worker appears to be on this;
   script must be run from the worktree root because it uses `Path.cwd()`.)
4. Twin build: `build_runtime.py` unexecuted; `outputs/2026-10-08-l25-build-r2`
   absent. Prerequisites (3 CHANGES.json hash-sync) currently satisfied.
5. Packaging + final PRX import-identity check (script's IMPORT-DELTA is raw
   symbols only), then the PS5 upload discipline (fake-sign, decrypt-OFF
   readback) — not started.
6. Console proof: no console contact anywhere; `native_execution: false`
   throughout; official PS5 compiler measurement for the 5 promoted facts
   pending.
