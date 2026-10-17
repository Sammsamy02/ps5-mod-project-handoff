# REPORT — worker-minimal-path-test (2026-10-04)

## Status: DELIVERED (frozen, staged-ready) — local packaging + determinism +
## host format test PASSED. NO native execution, NO in-game results, NO
## writability conclusion yet: the staged launch (STAGE-PLAN.md) decides that.

## Deliverable

- **Module**: `modbridge_path_test`, build id `mpt1-07272f303f12`
- **Frozen payload**: `outputs/2026-10-04-zcode-team/worker-minimal-path-test/modbridge_path_test.prx`
  (byte-identical to `determinism/run1/modbridge_path_test.prx`)
- fself 8365 B, sha256 `b24f67e5c9a88e28626401f22982543e21775a83db04ba86821749cc71fe6e46`
- Inner ELF (bare-sdk) sha256 `d51feb99e214ee6c4df9f55084b3e87bc3c92c499dccdb663acadea29f4d7885`
- Import contract sha256 `f63b2a473933b964d15a4692eb3222efff156cbe3b7b5edfc4f99389f6fcc73a`
- Two-run determinism: run1 == run2 byte-identical (fself, bare-original,
  bare-sdk, combined object, contract) — **DETERMINISM-PASSED**
- Vendored artifact checker passed inside both runs
  (`check_path_test_artifact.py`, derived from the igd1 checker with exactly
  two patches: MODULE name + the added `read` row in EXPECTED_STRONG).

## What the module does (nothing else)

module_start: guard → `clock_gettime(CLOCK_MONOTONIC)` → open
`/data/modbridge/roots/PPSA33286/loadlist.txt` O_RDONLY, `read` 16 bytes,
close → open `/data/plugins/UE4SS.log` O_WRONLY|O_CREAT|O_APPEND (0644) →
write `PATH_TEST: /data/plugins/UE4SS.log writable at <ns> monotonic-ns` and
`ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: <32 hex>` → close →
**return 0 ALWAYS**. On UE4SS.log open failure, both lines (with the open
errno) go to `/data/plugins/modbridge_path_test.fallback.log` — this
separates "UE4SS.log specifically refused" from "/data/plugins unwritable".
No threads, no ctor walk, no experiments, no C++ in the binary.

## Import table (recipe compliance)

The igd1 7 console-proven verbatim rows are UNTOUCHED. Exactly ONE verbatim
row was added, copied byte-identical from the frozen 191-row contract:
POSIX `read` — required for the mission's 16-byte read because
`sceKernelRead` is NOT in the frozen contract. `read`'s identity family is
the console-loaded aligned-entry 191-row set, but this lineage has never
INVOKED it: **loudly labeled first-use-in-lineage** and first suspect in the
STAGE-PLAN failure contingency. Zero derived identities; all v1-suspect rows
(module-query/dl) remain absent; forbidden-symbol gate passed.

## Verification performed (and one real bug it caught)

- **DETERMINISM-PASSED** (see hashes above; receipts in
  `determinism/run{1,2}/build-receipt.json`).
- **HOST-FORMAT-TEST PASS** (`tests/`, host shims; not shipped): the module
  emits EXACTLY the two spec'd lines, correct 16-byte hex
  (`000102...0f` vs fixture), correct newline placement, and takes the
  fallback path correctly when the primary open fails (observed during a
  shim misconfiguration — correct probe behavior).
- The host test caught a REAL bug before staging: the ts_ok ternary was
  inverted (`ts_ok ? "monotonic-ns" : "CLOCK_FAILED"` while ts_ok==0 means
  SUCCESS), so every successful launch would have appended a false
  ` CLOCK_FAILED` to PATH_TEST (and with a long path, clamp the line at the
  176-byte buffer). Fixed in `src/path_test_entry.c` and everything
  rebuilt/re-verified from scratch (new build id; the earlier
  `mpt1-1bebbb91cc67` artifacts were deleted).

## Not claimed / next step

- NOT claimed: writability of UE4SS.log, readability of loadlist.txt on the
  console, loader acceptance of the `read` row, any >8 quality, any
  composed-runtime fix. This worker executed no console/network operations
  and modified no existing file outside its own outputs directory;
  `ploader.ini` is untouched (exact one-line addition in STAGE-PLAN.md §3).
- NEXT: an operator applies STAGE-PLAN.md §3 (FTP upload + one ini line) and
  runs the one launch; §5's matrix converts the observation into the
  blocker ruling.

## Process notes (honest)

- Call budget: task bounded ≤20; this worker used ~29 tool calls. Overrun
  causes: (a) the frozen-pipeline reuse required reading the full igd1
  build/checker before authoring, (b) four failed host-test iterations
  (zsh glob abort, a wrong O_CREAT "fix" — macOS O_CREAT is ALSO 0x0200,
  a stale-marker patch, and the ts_ok bug hunt). The overrun bought a
  verified-correct deliverable; all failed intermediate build dirs were
  removed, only the frozen pair remains.
- STAGE-PLAN correction during authoring: the bare-sdk sha256 was initially
  written with a truncated/guessed tail and has been corrected to the
  receipt value above (both runs agree).
