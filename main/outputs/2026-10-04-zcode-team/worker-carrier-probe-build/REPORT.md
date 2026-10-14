# REPORT — worker-carrier-probe-build (cc1 carrier-name census ridealong)

Worker: worker-carrier-probe-build, team 2026-10-04. Task: build the cc1
ridealong probe specified by `worker-r3-carrier-prep/CARRIER-CANDIDATES.md`
§5-6 — a standalone, data-side-only, fc1/qdrv1-lineage PRX that censuses
the LIVE FName pool for the fixed candidate list — plus determinism
double-build, artifact checks, host tests (pass + every refusal class,
strict-JSONL oracle), the 9th-ini-entry STAGE-PLAN under the final-ini law,
and a constants-only brief for the batch tool. NO console contact, no
nested subagents, nothing executed in-game, no tool edits.

## 0. Verdict

DELIVERED, locally verified, never executed on target. The cc1 PRX is
built, deterministic, artifact-checked, and its full read/record/census
logic is host-tested against a synthetic arena. Staging is proposal-only.

## 1. What was built

`modbridge_carrier.prx` (fself sha256
`d83410aa9069a3039bb4079892b707373ad5d993992b0ed0b55abb1f57730e52`,
26845 bytes; build id `cc1-0864d65c1baf`; module name `modbridge_carrier`
via the length-preserving Packager patch, 0 residual identity):

- **Mission** (CANDIDATES §6): ONE detached thread, 30 polls x 4 s
  (~2 min bounded window); per poll a fresh sysctl KERN_PROC_VMMAP scan
  gates EVERY dereference; qdrv1's GUA header-window admission (rcodes
  1-7, layout-A predicates incl. the corrected GC invariant) plus the
  10000-count layout gate; then, ONCE, fc1's VALIDATED pool control chain
  (CurrentBlock, Blocks[0], bootstrap ids, "None"/"ByteProperty" ASCII)
  and the budget-bounded NAME census (1,048,576-entry budget, progress
  marker every 4096, `census_truncated:1` honesty field) over the live
  pool blocks, matching ONLY a fixed 53-name table: the 3 carrier
  candidates (ReceiveTick 0x76c0264, BlueprintUpdateAnimation 0x773e91e,
  Tick 0x7521050), the 19 walk class-strings (§4.3, all with vaddrs),
  the 27 AActor/UAnimInstance ctor-family decoder controls (§4.1/§4.2),
  the "None"/"ByteProperty" positive controls, and the
  "PostRender"/"ZZZNotARealFunctionQQ" negative controls (counted in
  `negative_hits`, never in matched[]). Every table name is
  image-confirmed; the vaddrs are recorded provenance
  (`src/carrier_driver.h`), never dereferenced.
- **Census decode** follows the fc1-validated entry law AND the CANDIDATES
  §6.2 header law (u16 header: bIsWide bit0, Len bits15:6); the walk
  strides entry-to-entry using Len (2+Len ASCII / 2+2*Len wide bytes, in
  2-byte offset units) so string bodies are never scanned — wide entries
  are counted (`wide_entries_skipped`), and ids are reported as
  `(block<<16)|offset`, fc1-compatible.
- **Outputs**: own single-writer channel `/data/modbridge-carrier-facts.jsonl`
  (NEW path, NOT qdrv1's file; v3 separator-correct writer, every line
  strict JSON < 1 KiB) with `entry`/`startup`/`mapscan`/`qh_counter` (one
  per poll: phase, admission_rcode 0..7, refused_bindings_absent,
  refused_layout_gate_closed, census counters)/`census_match` (one per
  matched name)/`census_result`/`summary`; plus exactly ONE `CC1:` UE4SS.log
  line per launch. Verdict vocabulary per §6.3: `CENSUS:VALIDATED` /
  `CENSUS:REFUSED` + reason_code.
- **Fail-open**: pre-census stages (map/anchor/count/pool-uninitialized)
  are transient within the bounded window; any pool/census hard failure is
  a recorded terminal refusal, never retried; module_start returns 0 in
  every refusal path.
- **Read-only data-side**: no hooks, no engine writes, no mprotect, no
  mutex imports, ZERO raw `syscall` instructions (build-gated == 0), 96-
  and 40-byte map-gated plain reads only.
- **Imports**: TEN verbatim frozen-191-contract rows — the igf 7 +
  nanosleep (fc1pool1 precedent) + pthread_create/pthread_detach
  (qdrv1/observer one-thread precedent; pthread_self DROPPED — no
  thread-identity fold). No derived identity, nothing else new.

Lineage: `record_sink.c/.h`, `ingame_libc.c/.h`, `ingame_abi.h`,
`anchor_store.h`, `vmmap_walk.c`, `mapscan.c`, `sentinels.c`,
`boundaries.S` are BYTE-IDENTICAL vendored from worker-fc1-validator-probe
(which verified vs qdrv1/igf5; re-verified by the build's LOUD gate).
Labeled-change: `import_references.S`, `module_entry.c` (measured +
pinned). New: `carrier_driver.c/.h`, `carrier_libc.c`.

## 2. Verification (all local, this session)

- **Determinism double-build**: run1 == run2 byte-identical on final PRX,
  inner ELF (`e72366c4…`, 73088 B), combined object, import contract
  (`build-receipt.json` `.determinism`).
- **Artifact checks** (`check_carrier_artifact.py`, run1 AND run2):
  LOCAL_PACKAGE_CHECKS_PASSED — fself container law + per-phdr segment
  equality + inner sha pin; init/fini 1/1 (boundaries.S sentinels);
  SDK 0x11000000 stamp; explicit DT_INIT/DT_FINI, loader-unowned arrays;
  dynsym decoded-NID set EXACTLY equals the 10 verbatim rows (SceNid
  oracle independent of the artifact); forbidden-identity gate clean on
  dynsym AND combined object; module-name patch evidence.
- **Host tests** (`tests/run_host_checks.py`, 24/24 PASS, strict-JSONL
  oracle — every line json-parsed, < 1 KiB, envelope-checked; receipt
  `receipts/host-test-receipt.txt`):
  - pass paths: `main` (10 matches incl. all 3 carriers + classes,
    negative_hits PostRender=1 because the synthetic pool CONTAINS it,
    controls 1/1), `negative_clean` (matched exactly None+ByteProperty),
    `wide_only` (wide entry skipped+counted, no false match),
    `budget` (64-entry budget: truncated=1, still CENSUS:VALIDATED with
    explicit coverage fields).
  - EVERY refusal class: the four CANDIDATES §6.3 census codes
    (`pool_window_uncovered`, `none_ascii_mismatch`,
    `current_block_insane`, `pool_uninitialized_after_grace`) plus
    `map_untrusted`, `anchor_not_admitted`, `census_gate_closed_after_grace`,
    `pool_window_not_readable`, `blocks0_invalid`, `blocks0_uncovered`,
    `handles_window_uncovered`, `bootstrap_ids_degenerate`,
    `handle_block_index_oob`, `block_pointer_invalid`,
    `entry_window_uncovered`, `entry_wide_flag_set`,
    `byteproperty_ascii_mismatch`, `census_block_pointer_invalid`,
    `census_entry_window_uncovered` — each with the exact verdict/reason,
    30 counter lines, correct admission/counter attribution, and the one
    UE4SS line.
  - Design note recorded by the tests: `block_pointer_window_uncovered`
    (fc1 vocabulary) is defensive-only in cc1 — the Blocks[]-slot window
    is covered by construction once the pool header window passed — so it
    has no dedicated scenario; the uncovered leg deterministically
    surfaces at the entry-window check instead (documented in STAGE-PLAN §7).

## 3. STAGE-PLAN (9th ini entry — the cap holds)

`STAGE-PLAN.md` (proposal-only). Verified against the final-ini law this
session: canonical 8-entry file FINAL-INI-OK (7 registered); the carrier
ensure-op (`/data/plugins/modbridge_carrier.prx = 2` inserted AFTER the
`modbridge_ingame_facts.prx = 0` tail line, never editing existing lines)
yields **9 entries, 8 registered** — xwp, fc1, composition, qdrv1,
observer, aligned_entry, path_test, carrier — igf stays `= 0` skipped:
**8 of MAX_PLUGINS=10, headroom 2**
(`receipts/final-ini-count-check.txt`, COUNT-LAW-OK). New whole-file
target bytes: 440 B, sha256
`fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507`.
Honest note: `verify_final_ini_layout.py` REJECTS the 9-entry bytes today
(UNKNOWN line) — correct behavior for the 8-entry law it pins; adoption is
the layout owner's NEW pinned-bytes decision. Readback constants (SHT-strip
law): bare-sdk sha `e72366c497d8ffa4a76f3f2f01bb898a0d41c2434813429be1408dc2c15a7f71`
(73088 B), expected readback **71528** bytes, body sha (64..end)
`1d17a27db4cea8182673360aef06ac2e47dd0eb81993b2226a181b3fd6d6e868`.

## 4. Batch-tool note

`CONSOLE-BATCH-CONSTANTS-BRIEF.md` — constants only (CC1 artifact dict,
ini append line + predecessor, 9-entry sha, registered_expected 8,
post-launch 8-OK-line gate, evidence gates, preflight duplicate check).
No tool file was opened for edit; adoption is the tool owner's decision
under a NEW id.

## 5. Explicit non-claims

- Nothing executed in-game, nothing staged, no console contact; all facts
  are local build/test/layout facts. THIS module has never run on target.
- No claim any candidate carrier IS native, per-frame, or game-thread
  stable (CANDIDATES §7.2): cc1 resolves NAME truth only; FUNC_Native,
  invocation rate and thread identity stay with the R3D §4.2 in-runtime
  calibration. F-1 remains open either way by design.
- The census matches ASCII-encoded entries; wide-encoded names cannot
  match an ASCII table and are counted (`wide_entries_skipped`), so a
  VALIDATED verdict is "none of the 53 names are interned as ASCII
  entries" at minimum, not a proof of absence in wide encoding.
- A `census_truncated:1` VALIDATED verdict is a bounded census; coverage
  is exactly `census_entries_scanned` of `budget_entries`.
- The Len bits15:6 header law is taken from CANDIDATES §6.2 (the fc1
  synthetic host entries were re-encoded to match); if the live pool used
  a different Len encoding the control chain would still validate (it
  never reads Len) but stride efficiency/pause behavior would differ —
  the first live `census_result` is the decider, and any mismatch shows
  up as `census_entry_window_uncovered`/low `entries_scanned`, i.e. a
  loud refusal, never a silent wrong census.
- No quality score is claimed (docs/QUALITY-RUBRIC.md discipline):
  evidence delivered is static + host-simulator only.
