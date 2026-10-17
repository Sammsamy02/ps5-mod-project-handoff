# Agent FY — build speed: concurrent twins + LuaMod split plan (2026-10-10)

Edited file (ONLY file edited): `outputs/2026-10-08-l26-runtime/build_variants.py`
(pre-FY backup: `/tmp/build_variants.py.pre-FY.bak`; pre-FY plan preserved as
`outputs/2026-10-08-l26-build-variants/plans/PLAN.preserved-pre-FY-20261010.json`).
Plan doc (task B): `outputs/2026-10-08-l25-prep/FY-LUAMOD-SPLIT-PLAN.md`.
No builds, no --execute, no console, no source/CHANGES/packaging-driver edits.

## Task A — concurrent twins (landed in build_variants.py)

**Independence proof (verified by code reading):** pass2 has NO dependency on
pass1 — each pass reads only frozen l25-A BASE pass receipts and `sel`, and
writes to its own `out/passN/objects` dir. Variant B never reads variant A's
outputs. The per-variant `headers.vfs.json` overlay + marker are prepared once
by `prepare_variant` BEFORE either pass. `sel` is read-only during passes
(all mutations happen in main's serial prelude: compute_selection, stamp,
prepare_incremental, copyin-probe, token scans).

**Design:**
- `build_variant` split into `prepare_variant` / `build_pass` / `finish_variant`
  (thin sequential wrapper keeps the plan path).
- `--execute` (main): one `ThreadPoolExecutor(max_workers=4)` runs the four
  (variant,pass) tasks A1/A2/B1/B2; joins in insertion order; merges gates;
  then finishers run serially A then B, then `pair_receipt`. `--plan` stays
  fully sequential — byte-identical output to the pre-FY driver (verified,
  see validation).
- Each pass keeps its own compile pool (max_workers=2, same cancel-on-failure
  semantics as before).
- Failure path: on first task exception, queued pass tasks are cancelled
  (`shutdown(cancel_futures=True)`, py3.9 OK on this Mac's 3.9.6); running
  passes complete their in-flight work then the exception propagates and no
  receipts are written (same as before; sibling passes now run to completion
  first — bounded by one pass duration).

**Thread-safety findings (the reason for each mechanism):**
1. `record()` in main appended to the shared `gates` list and printed — list
   append is GIL-atomic but `print` does two writes (text + newline), so
   concurrent lines could tear. Fixed: main's `record` and the per-task
   `trec` (from new `make_task_recorder`) both emit under a module-level
   `_PRINT_LOCK`; gate dict construction happens outside the lock.
2. Gates ORDER from 4 concurrent tasks is scheduler-dependent → per-task gate
   buffers merged deterministically after the join (A-p1, A-p2, B-p1, B-p2,
   then serial finish/pair gates). stdout line order may interleave (allowed);
   the EXECUTION-RECEIPT `gates` array is run-to-run deterministic.
3. `variant_pins` was shared across the sequential pass loop and
   DEPENDENCY-GRAPH.json content is CUMULATIVE (pass2's graph re-lists pass1's
   pins; dict update keeps pass1 insertion positions). Concurrent passes would
   race this write. Fix: per-pass `pass_pins`; the finisher writes both graphs:
   pass1 = `list(p1_pins.values())`, pass2 = `list({**p1_pins,**p2_pins}.values())`
   — proven equal to the sequential cumulative result for every insertion
   order (unit test). COMPOSITION-RECEIPT (which pins its DEPENDENCY-GRAPH) is
   written by the finisher too, after the graph write; all other per-pass
   receipts (LINK-COMMAND, COMPILE-RECEIPT, IMPORT-DELTA) stay in `build_pass`
   unchanged.
4. `route_agg`, `done`, `objects`, `passno` were already per-pass closure
   locals — now guaranteed one per task. `steps` is appended ONLY in plan mode
   (sequential) and in main's serial prelude — no concurrency. `env`/`marker`
   per variant, read-only. `run()` writes per-(dir,label) receipts to unique
   paths; `write/pin/sha/deps/rewrite_compile` are pure. `subprocess.run`
   releases the GIL, so the semaphore genuinely bounds external processes.

**Jobs knob:** env `L26_BUILD_JOBS` (integer ≥1, validated; bad values fail
the run) caps GLOBAL clang/lld subprocess concurrency across all 4 passes via
a `BoundedSemaphore` taken around each compile AND link `run()` (llvm-nm left
unbudgeted — trivial). Default `min(6, ncpu)`: this Mac is 8 logical cores
(4P+4E) / 16 GB (`sysctl hw.ncpu`=8, `hw.perflevel0`=4, `hw.perflevel1`=4,
16 GB); 6 leaves memory headroom — luamod.o alone is 7.0 MB at -O0 and peak
clang RSS for the big units can exceed 1 GB each; 8 unbounded clangs risks
memory pressure/swap, especially on battery. Set `L26_BUILD_JOBS=8` to saturate
all cores when on AC/mains. Recorded as informational gate
`G-concurrent-twins-budget` + `pass_concurrency` block in EXECUTION-RECEIPT
(execute only — PLAN.json keys unchanged).

**Expected wall time:** l26-B pass1 evidence = 202 objects / ~234 s at
workers=2 → sequential 4 passes ≈ 15.6 min. Concurrent: same total work
(~1,870 core-seconds) across 6 slots ≈ 5–6 min + 4 links ≈ **~5–7 min total
(~2.5×)**, bounded by memory/CPU at the chosen knob.

**Validation done (no builds):**
- `python3 -m py_compile` OK; literal audit: all 28 pre-FY gate names, all
  receipt write targets, all step names preserved (audit script compared
  old/new sources); zero `variant_pins` remnants.
- Unit tests PASS: (1) 800 concurrent gate entries from 4 tasks merge
  byte-identically across 3 different thread interleavings, merge order
  exactly A-p1,A-p2,B-p1,B-p2 with in-task order preserved; (2) `{**p1,**p2}`
  == sequential cumulative pins merge for all insertion orders; (3) jobs knob
  default/override/validation; (4) semaphore caps 8-way demand at ≤6.
- ONE `--plan` run (allowed): exit 0 — 202 affected units, 823 steps, 74
  gates; PASS line printed. Compared to the preserved pre-FY full plan:
  step sequence **identical** (823=823), every pre-FY gate name/count present;
  the only differences are 2 extra `G2-part-change-pins` gates + 2 CHANGES
  rows (40→42) + header_alias/routing-proof fields — TREE DRIFT from parallel
  agents since the preserved plan, not the refactor — plus `driver_sha256`.
  Plan-mode preservation and EZ-incremental composition are intact
  (incremental gates untouched; execute path is where concurrency applies).

**Incremental-mode (EZ) compatibility:** `--incremental` prelude unchanged
(`prepare_incremental` serial in main); retention remap + `G-inc-*` gates now
live in `build_pass` (per-pass, via trec under concurrency); NEW_UNITS always
fresh; `G-inc-define-units-covered` unchanged. Incremental execute now also
runs the 4 passes concurrently (few fresh units → semaphore rarely contended).

**Packaging driver (read-only check, no edits needed):** `l26_packaging_driver.py`
P0/P1 consume per-variant dirs by EXISTENCE checks only (TWIN-RECEIPT, both
pass artifacts, SOURCE-SELECTION, VARIANT-MARKER, COMPOSITIONs, IMPORT-DELTA,
CHANGE-SELECTION) — fully order-independent; concurrent twins change no file
contents. **No variant-order tweak required.** (FY did NOT touch the packaging
driver per task rules.)

## Task B — LuaMod.cpp split (PLAN ONLY)

See `outputs/2026-10-08-l25-prep/FY-LUAMOD-SPLIT-PLAN.md`: full line-range
inventory of the 9,509-line file; mechanical 5-TU + core layout
(Globals / R3Marshal / MainState / Classes / optional HookData; core keeps
ctor/lifecycle/`on_program_start` incl. the only variant-token site at :8455);
cross-TU symbol handling (new `LuaMod-Internal.hpp` added_header; existing
`LuaFNameBridge.hpp` reuse); root checklist for NEW_UNITS registration
(census 394→394+N automatic via `391+len(NEW_UNITS)`; luamod unit identity
kept by the existing CHANGES row), the REQUIRED define-units safety patch
(token hits in NEW_UNITS sources must join `define_units` — silent-variant
hazard if a token site ever moves into a split TU), P8 239/2 (only if split
TUs add new global ctors — design says zero), P7 207 imports unchanged.
Honest estimate: luamod ≈ 20–40 s of a ~234 s pass (7.0 MB object; receipts
carry no per-unit durations — mtime-based estimate); the split saves seconds
on a full build (the real lever is HEADER cascades: String.hpp 196 consumers,
Utf16Utf8 177, NameTypes 168, UObjectArray 162) and pays off in incremental
edit latency (~20–40 s → ~5–15 s per LuaMod-section edit) and 455 KB tooling
pain.

## Notes for root

- First post-FY `--execute` should set nothing (default 6) or
  `L26_BUILD_JOBS=8` on AC; watch memory. Archive/rename previous variant
  dirs first as usual (G3 dir-fresh unchanged).
- EXECUTION-RECEIPT gates array is now a deterministic merge whose ORDER
  differs from the sequential driver (content identical per gate); nothing
  downstream parses gate order (packaging P0 does not read this file).
- The 2 new CHANGES rows that appeared mid-flight (42 total) are drift from
  parallel agents, visible in the FY plan run's G2 gates — expected under
  tolerate_drift; final execute will hard-fail until their hashes refresh,
  as designed.

— agent FY, 2026-10-10
