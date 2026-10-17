# REPORT — worker-rea-trial (2026-10-05, RESTART — mission complete)

## What was done

1. **Setup completed and proven** (details + exact commands in REA-TRIAL.md):
   - Prior attempt: built macOS-arm64 Ghidra native decompiler (`make ghidra_opt` with
     arm64 overrides; documented `make decomp_opt` route fails on missing `bfd.h`),
     installed at `Ghidra/Features/Decompiler/os/mac_arm_64/decompile`; two backed-up
     one-line dist patches (darwin/arm64 gate; startup timeout 330s -> 90 min).
   - This restart added: (a) diagnosis of the prior 53-min `execution_failure` as
     **Java heap OOM at Ghidra's 2G headless default**, fixed with zero patches via
     `GHIDRA_HEADLESS_MAXMEM=8G` (passes through REA's launch env); (b) a third backed-up
     dist patch adding an opt-in **persistent-project `-process` mode** to
     `GhidraLauncher.js` (env `REA_GHIDRA_PROCESS_*`; skips per-command 194MB re-import;
     stock behavior when unset; bridge sha256 verification still passes).
   - `rea doctor --json` final: all ghidra checks healthy (incl. native decompiler,
     java 21, headless 12.1.4); hopper missing + skill/registration integrations
     unconfigured (acceptable, documented). Overall healthy:false solely from those.

2. **Investigation delivered through REA's CLI** on the full 194MB eboot
   (sha256 `3b6cfad4...22fc`), all four queries in 6-14 s each:
   - Q1 `rea function 0x23d590`: full dossier + Ghidra pseudocode of the FName ctor —
     line-by-line match with the independent capstone ground truth (NULL branch,
     wide-char OR-reduce, `(0x7f < acc) << 0x20` flag, `bytes >> 1` char count, intern
     call 0x23d600, 8-byte handle store). Corrected the trial's own "13-bit bextr"
     annotation (control 0x2001 = start 1, length 32).
   - Q3 `rea xrefs 0x9365ec0`: **49 references, site list identical to the independent
     byte scan (49/49, zero differences)** — pool label confirmed.
   - Q2 `rea xrefs 0x23d590`: 2 instruction-verified call sites in analyzed code
     (0x22d96a, 0x22dd99) vs the 14,401 byte-level upper bound (image-wide verification
     blocked, see below).
   - Q4 `rea decompile 0x23a5d0`: pool-init/allocation function decompiled — every
     instruction of the ground-truth trace confirmed (memset pool+8/0x10008,
     Malloc(0x20000,2) -> Blocks[0] @pool+0x10, 256 x 0x40 descriptors, bucket loop
     malloc(0x400,4)/mask 0xff) plus **236 default-name registrations** (corrects the
     local "~20" scan-window estimate) and an init self-check + stack-cookie tail.
   - Failure path honesty: the first post-restart xref queries returned EMPTY because the
     reused DB had no disassembly/references in the accessor functions; documented, then
     fixed by bounded cluster enrichment (0x230000-0x249fff) before re-querying.

3. **Full auto-analysis attempt — externally killed, honestly recorded**: tuned-options
   pass + 8G-heap full analysis on a copy of the other worker's verified project ran
   ~113 min (real progress verified via jstack; past the prior OOM point), then the
   background task was killed by something outside this worker. Per no-blind-replay rules
   it was not restarted; the DB reverted to its saved pre-kill state. Consequence:
   instruction-verified caller counts remain limited to analyzed code (FINDINGS 5.3).

## Workflow verdict: REA + Ghidra provider for this project's binary work

**Viable and, with the persistent-project mode demonstrated here, the preferred route for
repeat agent-driven RE queries on this binary.** The prior attempt's "not yet ergonomic"
verdict was blocked on exactly the gap this restart closed:

- Persistent analysis snapshots (import once, query many) were the single missing
  capability. Demonstrated: 7 query commands, 6.0-13.7 s wall each, versus 45+ min per
  command in import mode (a ~300x improvement), on the same 16GB Mac.
- Evidence hygiene is genuinely good: every command returns a schema'd Evidence JSON
  bound to the target sha256, with operation parameters, analysis-profile digest, and
  limitations — the empty-xref result was reported honestly rather than faked, which is
  exactly the behavior wanted from a verification tool.
- The remaining known costs/limits (all observed, quantified):
  1. Image-wide instruction verification on a 194MB stripped binary needs one successful
     full auto-analysis (~2h+, must not be interrupted; the external kill lost the
     in-memory pass). REA can't do this today in import mode (deadline + OOM) — with
     process mode the analysis is a one-time preprocessing step outside REA, after which
     REA queries are cheap.
  2. Setup on macOS still needs the three documented dist patches (darwin gate, timeout,
     process-mode env hook) + built natives + `GHIDRA_HEADLESS_MAXMEM`.
  3. No leaked-JVM cleanup on abort (prior attempt observed twice).
- Recommendation if adopted: upstream the launcher env hook (`REA_GHIDRA_PROCESS_*`),
  honor `GHIDRA_HEADLESS_MAXMEM` in docs/doctor hints, make the startup deadline
  configurable, and add orphan cleanup on abort.

## Cross-check (REA channel vs local byte-level vs worker-re-fname-pool)

- REA decompiles (ctor, pool-init) match the trial's capstone traces line-by-line; the
  two corrections (bextr control width; 236 default names) came FROM the REA decompile
  and improve the record. FINDINGS.md updated with provenance notes.
- REA pool xrefs: 49/49 identical to the byte scan. Peer worker's independent trace:
  full agreement on pool address, init sequence, allocator chain, layout offsets
  (table in FINDINGS.md unchanged; no disagreements before or after).
- One inherited open item remains (not resolvable statically): the launch-9 runtime
  observation placing Blocks[0] in module BSS vs the static OS-heap chain
  (worker-re-fname-pool §7.1); needs one fresh runtime read.

## Honest limits

- Image-wide instruction-verified caller count NOT secured (full analysis externally
  killed at ~113 min; 14,401 remains a byte-level upper bound; 2 sites verified).
- Cluster references were computed by a bounded enrichment script using Ghidra's operand
  semantics (documented in FINDINGS 5.2 provenance) — not by whole-image analysis.
- Exact Binned2-vs-Binned3 allocator leaf label remains unproven (no symbols).
- No runtime/dynamic verification performed; static only.

## Artifacts

REA-TRIAL.md (setup + restart log), FINDINGS.md (evidence + corrections + cross-check),
CLAIM.json, this file, `run-final-queries.sh` + `run-sessions.sh`, `tools/` (Ghidra
scripts: InspectProg, SetAnalysisOptions, AnalyzeCluster, DisassembleCluster,
BuildClusterRefs2, CheckPoolRefs), evidence: `function-23d590.json`, `xrefs-pool.json`,
`xrefs-ctor.json`, `decompile-pool-init.json` (+ `.time` latency records),
`function-23d590.timeout-45min.json`, `xrefs-pool.OOM-execution-failure.json`,
`pool-lea-refs-scan.json`, `ctor-callers-14401.txt`, `analysis-pass2.log`.
