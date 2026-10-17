# REPORT — worker-record-sink-addon (qdrv1, 2026-10-04)

Task: design and build the minimal late-load PRX add-on that drives the
query-harness record path from OUTSIDE composition-5, so
`/data/modbridge-ingame-facts.jsonl` can carry the rung-1/2
QueryResultRecord evidence (ticks_accepted advancing, game_thread_id_key)
once composition-5 r6 reaches engine-ready. No console contact, no nested
subagents; writes only in this directory; CLAIM.json first, this report last.

## 1. Answer to the route question

The igf-lineage route SUFFICES; the add-on is a self-contained driver PRX
(`modbridge_qh_driver`). Facts pinned in DESIGN.md 1: the composed PRX stages
`native-query-harness` but nothing in it ever calls `OnEngineTickDispatch` or
`FormatRecordJsonLine` (host-test callers only; WIRING.md:120-124 defers the
driver), while the igf probe family proves a separate module in the same
process can read the live GUA (layout A validated live at runtime 0x97ba110:
NumElements=71959, first_gc=35477/last_non_gc=35476, NumChunks=2) and write
this exact JSONL through its own sceKernel trio. The harness's injectable
`EngineBindings` seam is the designed entry for the add-on's own four probes.
The one thing a separate PRX cannot do is execute on the engine's OS game
thread — LOUDLY documented (CLAIM/DESIGN/STAGE-PLAN): this add-on evidences
the RECORD PATH + real GUA count observations, not game-thread execution.

## 2. What was built

- `modbridge_qh_driver.prx` — 26157 bytes,
  sha256 `52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4`,
  build_id `qdrv1-c20c70ec4112`. Entry = lineage module_start/module_stop
  (force_align_arg_pointer, entry-guard, entry-marker-first law); one bounded
  detached driver thread (30 polls x 4 s, observer-lineage pattern) runs:
  fresh VMMAP scan → GUA 96-byte header-window admission (span-gated direct
  dereference, 7 recorded rcodes) → wiring schedule → one
  `OnEngineTickDispatch` → per-poll ig-envelope counter line; the terminal
  `FormatRecordJsonLine` output is written once as a STANDALONE line (the
  pinned checker shape), plus lineage summary line; then the thread closes
  the channel.
- Sources under `src/`: 10 files byte-identical to the igf5 lineage,
  `NativeQueryHarness.hpp` byte-identical to worker-query-harness, three
  labeled-change files pinned (harness cpp freestanding variant — diff
  deliverable `receipts/nqh-addon-variant.diff`; import_references.S +4
  verbatim rows; module_entry.c thread spawn), three new files (`qh_libc.c`
  memcpy/memset shims, `qh_driver.h/.cpp` driver).
- Imports: EXACTLY 11 verbatim rows of the frozen 191-row contract — the igf
  7 PLUS the four FLAGGED additions `nanosleep`, `pthread_create`,
  `pthread_detach`, `pthread_self` (driver-thread minimum; pthread rows are
  the observer-lineage precedent; nothing derived, no dl*/mprotect/
  module-query; recipe asserts 0 raw `syscall` instructions and enforces the
  igf6 forbidden-symbol list).

## 3. Verification (local only; no native execution claimed)

- **Build**: frozen igf6 pipeline reused (vendored dotnet SDK + Packager.dll
  in place + finalize_sdk 0x11000000 + pinned make_fself). One labeled
  post-packaging transform, receipted and byte-stable: the used-in-place
  Packager.dll hardcodes moduleName/exportLibraryName
  `modbridge_ingame_facts`; a length-preserving string patch to
  `modbridge_qh_driver` runs BEFORE finalize/make_fself so all hashes and the
  fself wrapper cover the patched bytes (1 occurrence patched per run; final
  bare carries 3 `modbridge_qh_driver` strings, 0 old strings).
- **Determinism**: run1/run2 byte-equal on final PRX, inner ELF, combined
  object, and import contract (build-receipt.json `determinism` block, all
  `equal: true`).
- **Artifact checks**: `check_qdrv_artifact.py` (adapted from
  worker-startup-checkpoint/diag/check_diag_artifact.py) PASSED in both runs:
  fself container law, per-phdr segment equality, init/fini slot law (1/1),
  SDK stamp, explicit DT_INIT/DT_FINI, loader-unowned arrays, and the dynsym
  import gate as EXACT equality — with NIDs derived INDEPENDENTLY via the
  pinned SceNid algorithm (no circular oracle). artifact-checks.json in both
  determinism runs.
- **Host tests**: `tests/run_host_tests.sh` (receipt
  `receipts/host-test-receipt.txt`) compiles the REAL vendored sources and
  drives the full paced loop synchronously: main mode (30 polls, 29 sleeps,
  0 wiring refusals, 1 qh record, ticks_accepted 0→1→2, both guard refusals
  counted) + four refusal modes (bad_counts→rcode 6, bad_gc→7,
  ambiguous→3, uncovered→1: never a tick accepted, never a qh record). The
  emitted JSONL is REAL vendored-harness+sink output; replayed through
  `worker-rung-evidence-reader/rung_check.py --from-dir`:
  **RUNG1 PASS (all 5 conditions + driver line), RUNG2 PASS (terminal
  completed record), overall PASS** on Python 3.9 stdlib.

## 4. Deliverables (this directory)

CLAIM.json (first) · DESIGN.md · STAGE-PLAN-QDRV1.md ·
`modbridge_qh_driver.prx` · `build_qdrv_probe.py` · `check_qdrv_artifact.py`
· `src/` (16 files, per-file provenance in build-receipt.json
`vendored_identity`) · `tests/` (driver_host_test.cpp, run_host_tests.sh,
fixtures) · `determinism/run1,run2` (+ per-run build receipts and
artifact-checks.json) · `receipts/` (host-test-receipt.txt,
nqh-addon-variant.diff) · REPORT.md (this file, last).

## 5. Honest limits

- No console contact was made; nothing here claims native execution, in-game
  records, or rung PASS on any console file. STAGE-PLAN-QDRV1.md gives the
  controller the ploader `=2` entry, the expected line shapes, and the rung
  timing window (the ticks_accepted ramp is an ~8 s window opening
  ~30-60 s after module_start; re-run rung_check every ~20-30 s during it).
- The add-on dispatches from its own driver thread; `game_thread_id_key` is
  the driver fold, not a UE4SS game-thread identity. Pair with the
  composition's WIRING.md persistent-callback route for in-game-thread
  claims.
- The GUA count read is a header-window admission (layout-A offsets pinned by
  live-validated facts); the corrected-predicate chunk chase (cfgL) is not
  performed in-PRX — raw header fields ride every counter line for offline
  replay.
- If the console loader rejects any of the four flagged verbatim rows (the v1
  correlate was DERIVED rows, so unlikely), the expected failure shape is the
  v2-documented silent load-reject with no C2 lines from this module;
  `rung_check --discover` remains the fallback, and a no-thread variant is
  NOT built (it cannot pace the rung window).
- A genuine NumElements decrease between the two accepted polls honestly
  publishes `monotonic="decreasing"` → rung-2 FAIL; do not re-rate; new
  window needs a new launch.
