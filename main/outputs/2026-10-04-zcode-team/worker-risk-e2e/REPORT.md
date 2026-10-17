# REPORT.md — worker-risk-e2e (team 2026-10-04)

Both deliverables complete. Host-only; no network; no console; no nested
subagents; reads were existing files elsewhere (read-only); all writes under
`outputs/2026-10-04-zcode-team/worker-risk-e2e/` (CLAIM.json first, this
REPORT last). Tool calls: ~35 of <=80.

## 1. Deliverables

| File | Content |
|---|---|
| `RISK-REGISTER.md` | 24 open risks: the 9 task-named risks (R-RW-01 Rosetta wedge, R-DRV-01 driver retry, R-E2E-FR3 F-R3, R-PAD-01 pad, R-ARGV-01 argv crash, R-V3-01 v3-after-VMMAP, R-FTP-01 FTP readback artifact, R-PLD-01 ploader return invisibility, R-D1-01 D1 propagation) each with {id, area, risk, evidence, likelihood, impact, mitigation, blocking_what}, plus 15 supplementary rows compiled from every worker REPORT's gaps/limits sections (all 59 delivered REPORTs swept) and the checkpoint unresolved lists (mutex-fix-review, v3-startup-diag matrix, e2e-console-staging GO-NOGO, e2e-rehearsal F-R1..F-R3). Ends with a priority reading order for refill dispatch. |
| `D1-PROPAGATION.md` | Per-chain-root map of the D1 fix: current shas (recomputed this session, all match delivered pins), D1 fix status per root, exact anchor-occurrence verification, and exact patch application steps including a verified merge order that resolves the uf/r2 branch divergence. |

## 2. Key verified findings (new evidence produced this session)

1. **D1 fix status per chain root (sha-recomputed, anchors counted)**:
   worker-e2e-final (41 tuples, host `c1a594c3…`, console `3860cd56…`),
   worker-zip-to-mfb (45, `947affc8…`), worker-zip-bridge-console-link
   (45, staged console payload `cfa96ab5…`) — all **pre-D1**; only
   worker-uninstall-fault-expansion's 49-tuple chain has the fix
   (binary `46d87345…`, core `f9110fb4…`).
2. **Branch divergence + anchor conflict found**: the F-R2 fix (f-r2-fix,
   47 tuples) and the D1 fix (uf, 49 tuples) live on diverged branches.
   Appending uf onto the r2 core FAILS verbatim: `uf-order-commit-b`'s
   anchor has 0 occurrences there (r2-install-status-ok inserts a line
   after the shared `say("installed …")`). Verified resolution: apply the
   4 uf tuples BEFORE the 2 r2 tuples — I simulated the merge (uf-b NEW
   text onto the 45-chain core; r2 status anchor still occurs exactly
   once) — giving a canonical 51-tuple chain needing zero text edits.
   Recommended base for the console relink: `compose_console_bridge.py`
   (`ab9de3dd…`) + uf block (+ r2 block), then re-pin the staging manifest.
3. **argv console crash has NO workspace receipt**: worker-argv-probe
   built and gated ARGV_ECHO.elf (`53075bd7…`) but never dispatched; the
   latest controller console receipt is the 17:05Z INITONCE_DLL_TEST pass.
   The crash is recorded as controller-reported with root cause unknown
   (R-ARGV-01); G1 mitigation (config-file-driven entry) already designed.
4. **v3-after-VMMAP**: `ingame-v3-full-results.jsonl` ends at v1_vmmap_raw
   chunk 7/8, followed by the igd1 minimal diagnostic's entry record
   (outcome pending in the workspace) — the STAGE-PLAN-DIAG decision
   matrix is the registered next step (R-V3-01).
5. **Driver retry verdict (the "fixed? verify" ask)**: NOT fixed. The -68
   fixture root cause and MFR-2 leak fixes exist only as proposal diffs in
   worker-mutex-fix-2 (vendored-harness repro 3/3 green); real files
   untouched; `sdk_driver.c`'s unbounded retry loop unchanged; real
   build.py rebuild remains the controller's acceptance gate (R-DRV-01).

## 3. Honest limits

- Static compilation of evidence only: I re-ran no suites, rebuilt no
  binaries, executed no probes; likelihood/impact fields are engineering
  judgments, not measurements.
- The uf-order anchor compatibility under the r2 layer is verified by
  string counting + a one-tuple merge simulation, not by a full 51-tuple
  build; the merged core/binary shas must be pinned at build time (stated
  in D1-PROPAGATION §4/§5).
- Checkpoint unresolved lists were taken from the worker-level checkpoint
  documents that exist in the workspace; no controller session state beyond
  `outputs/2026-10-04-zcode-controller/` and `outputs/ZCODE-HANDOFF-2026-10-04/`
  was available, and the argv crash event itself is not receipted anywhere
  I could read.
- Three new worker dirs appeared mid-task (worker-anchor-validation,
  worker-expected-bytes, worker-predicate-compile) with no REPORTs yet;
  their outcomes are not in the register. worker-launch-automator and
  worker-fname-fc1-prep also have no REPORTs (CLAIM only / in flight).
- ploader return invisibility is sourced from worker-v3-startup-diag's
  retained-source reading; I did not re-read the ploader source myself.

## 4. Bounds

Writes confined to this directory; CLAIM.json written before any other
file; every existing file elsewhere untouched. Shas cited were recomputed
locally (shasum -a 256) or taken from receipts and marked as inherited.
