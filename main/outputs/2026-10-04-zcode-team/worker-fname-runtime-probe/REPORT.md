# REPORT — worker-fname-runtime-probe (2026-10-04)

Mission: build the FC-1 runtime verification probe add-on validating the
FName ctor candidate at runtime 0x63d590 (vaddr 0x23d590) with the
mprotect-first technique, for the NEXT probe iteration (igf6 or a combined
igf5 build). DELIVERED as a preparation module; NOT natively executed; no
in-game claims.

## What was built

1. **`src/fc1_probe.cpp` / `.h`** — `ig_run_fc1_probe(IgSink*)`: standalone
   add-on callable from the next probe's module_start chain (after the
   mapscan/Tick/SCO probe). Emits 12 records (seq base 10):
   `fc1_a_candidate`, 9 x `fc1_b_site`, `fc1_d_anchor_strings`,
   `fc1_summary`. Injectable variant `ig_run_fc1_probe_env` for tests.
2. **FC1-A** — mprotect-first 64-byte read at 0x63d590 (16 KiB page, add
   READ keep EXEC), first-32-byte hard gate + full 64-byte compare, diff
   masks, first-divergence offset, restore + anomaly semantics identical
   to igf5 `run_anchor`.
3. **FC1-B** — the 9 recorded call-site windows read mprotect-first and
   decoded by a bounded branch-free x86-64 walker (`src/fc1_windows.c`):
   rule 1 `48 8D 35 disp32` lea-rsi target == anchor string runtime,
   rule 2 exactly one qualifying LEA RDI store, rule 3 exactly one
   `BA 01 00 00 00`, rule 4 no branch/RSI write, rule 5 `E8 rel32` ->
   0x63d590. Unknown encodings fail closed (`unsupported_encoding_...`),
   never guessed through.
4. **FC1-C** — fail-closed summary: candidate prologue holding while ANY
   site disagrees sets `fc1_c_fail_closed=1`; `fc1_validated_indicator`
   requires A(32+64) AND 9/9 sites.
5. **FC1-D** — gated UTF-16LE anchor strings (data region): only when A+B
   pass AND the trusted current map shows a readable covering span; never
   mprotect'd; never gates the code verdict.
6. **Fixtures re-verified from the frozen capture** this session
   (sha256 `3b6cfad4...`): target 64 bytes + all 9 windows re-extracted
   (file offset = vaddr+0x4000), every site re-validated byte[site]==E8 and
   site+5+rel32==0x23d590, cross-checked three ways (checked-in fixtures,
   `worker-fname-fc1-materialize/calls_by_anchor.materialized.json`,
   `worker-expected-bytes/bytes_fname.bin`).
7. **`build_fc1_module.py`** — parses SYS_mprotect=74 / PROT_READ=0x1 /
   PROT_EXEC=0x4 / PAGE_SHIFT=14 from the PINNED SDK headers at build
   time, generates `generated-target/fc1_config.h` (real constants) plus
   host config + captured-byte fixtures, compiles all module TUs with the
   pinned prospero clang under `-Wall -Wextra -Werror` (CLEAN), builds and
   runs the host tests, emits `build-receipt.json`.
8. **Host tests, 8/8 scenarios PASSED** (`tests/run_host_checks.py`):
   all-pass (validated, 10 mprotect-request/restore pairs logged with
   prot 5 -> restore 0x4), target-byte drift (hard gate, divergence
   localized to byte 10, D skipped), mov-edx,1 corruption (site fails,
   FC1-C fail-closed), mprotect refusal (NO reads anywhere, no bytes in
   any record), restore anomaly (read kept, anomaly recorded — igf5
   semantics), map untrusted (LABELED checkpoint fallback 0x4, FC1-D
   refused, verdict stands), call rel32 drift (site T2 + decode flag it),
   unsupported encoding (fail closed). The host build runs the REAL probe
   code end-to-end against synthetic arenas mapped MAP_FIXED at
   0x110000000/0x110010000 containing the captured window bytes with
   disp32/rel32 recomputed for the synthetic layout (everything else
   byte-identical); the only untested-on-host piece is the raw `syscall`
   instruction itself, which cannot run on this arm64 host and is verified
   by the pinned-toolchain compile + the igf5 lineage convention.

## Honesty notes

- This delivery contains NO runtime/console evidence; `fc1_validated_*`
  exists only as a record indicator for the next in-game probe.
- The vendored `record_sink.c` etc. are byte-identical copies of the igf5
  lineage (sha256 verified, receipt).
- Mid-task corrections made and covered by tests: synthetic arenas must be
  REALLY mapped at the synthetic runtime addresses (mmap MAP_FIXED; macOS
  arm64 forbids < 4 GiB — bases moved to 0x110000000); fake mprotect
  accepts any page range inside the arena; `t2_rel32_target_ok` is a true
  boolean in records; record fields live under the lineage envelope's
  `fields` object.
- Write scope respected: everything under
  `outputs/2026-10-04-zcode-team/worker-fname-runtime-probe/`; scratch
  debug files removed.

## Acceptance evidence in-repo

- `build-receipt.json` (build_id `igf6-fc1prep-204a6d60a4ed`): target
  compile clean, host tests passed, SDK constants, extraction cross-checks.
- `tests/out/scenario_*.jsonl`: the probe's own records for all 8
  scenarios, every line strict JSON (checked by the Python verifier).
- `INTEGRATION.md`: exact igf6 / combined-igf5 wiring (vendor list,
  config emission, build script entries, call site, seq renumbering,
  residual risks).
- `CLAIM.json`: scope, provenance, bounds.
