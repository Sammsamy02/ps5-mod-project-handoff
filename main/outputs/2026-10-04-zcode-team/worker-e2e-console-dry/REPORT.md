# REPORT.md — worker-e2e-console-dry (2026-10-04)

Task: prepare the exact console E2E execution for the moment the installer
chain is ready — stages 0 / 1-owner / 2a / 2b / 3 / 4-5 / 6 with the
D1-fixed merged-chain payload — including the staging re-pin, the G1
config-file-driven entry design, the full controller sequence with verbatim
expected transcripts, and the PASS definition. CLAIM.json first; writes
confined to this worker dir; no console, no network, no dispatch; inputs
read-only. Budget ≤60 calls (used ~21).

## Deliverables

- `STAGING-MANIFEST-D1REL.md` — supersedes staging row #1: payload
  `worker-d1-chain-relink/composition/build/console/modbridge-installer.payload.elf`,
  sha `e3489297…436`, 681528 B, 51-tuple chain (D1 before F-R2), 116 imports
  missing=[], F-R3 size-law acceptance (phdr prefix 606256 B + magic).
  Corpus rows #2-#4 + optional archive UNCHANGED — every sha recomputed by
  this worker this session and byte-identical to the pins.
- `E2E-CMD-SCHEMA.md` — the G1 fallback design: fixed path
  `/data/modbridge/intake/e2e-cmd.json`, top-level schema
  `{schema, verb, state_root{state,roots}, mods[], options{}}`, exact
  options→argv flag mapping (the existing main() parser stays the single
  validation point), per-stage exact JSON for all 13 dispatches, bounded
  flat-JSON grammar (≤4096 B, depth ≤2, no \u escapes), consume-by-rename
  contract (`e2e-cmd.json` → `.consumed` BEFORE dispatch; stale re-dispatch
  = loud usage error, never a silent repeat), refusal matrix, verb
  allow-list (make-bundle and all fault-injection flags excluded), and the
  evidence list the implementing revision must produce (new tuple IDs, 116
  imports re-proof, suites re-run + new config-entry cases, fresh re-pin).
- `CONSOLE-E2E-EXECUTION.md` — controller sequence: MODE A (argv GO) vs
  MODE B (bare dispatch + cmd file), helpers (`cmd_put` with stale-file
  pre-check + readback sha gate, `cmd_gone` consumption proof, `dispatch`),
  per-stage exact curl, verbatim expected e2e-status blocks, expected
  records.txt deltas (store format read from the merged-chain
  `test/run/zb-pak/state/records.txt`), per-stage rollback, recover path,
  evidence collection, full-tree reset, and a 33-line all-green ledger.
- `SUCCESS-CRITERIA.md` — PASS = identity gates + dispatch discipline +
  verbatim stage blocks + truthful records + uninstall-restores-state +
  honesty labels, in ONE operator sequence; deviations reported distinctly.

## Key findings (things the old STEP-BY-STEP got wrong for THIS payload)

1. **Stage 3 expected lines changed by F-R2.** `r2-install-status-ok` emits a
   terminal `{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"<mods>"}`
   after every committed mods-install (verified in the merged fr2-87 run
   states: 30 occurrences). The old STEP-BY-STEP "begin+end only" block for
   stage 3 is outdated. Conversely, **removals emit NO status line** (the
   tuple guards on `mods_out[0]`; confirmed — zero empty-detail status lines
   in any merged transcript). Both corrected verbatim in the execution doc.
2. **Controlled refusals now close with a matched failed end**
   (`r2-exit-shim`): e.g. a refused install is begin + `{"event":"end","exit":2}`,
   not a dangling begin. The run's ledger expects 0 unmatched begins (no
   `_exit(86)` probes are scheduled on console).
3. **`recover` maps to stage `"recovery"`** (not stage-0-baseline) in
   `stage_for_cmd` — the recover rollback block has its own verbatim lines.
4. **G1 fallback needs a payload that does not exist yet.** The
   config-file entry is a design only; the current `e3489297…` build
   bare-dispatches to `usage()`/EX_BAD (main requires cmd+--roots). MODE B
   therefore BLOCKS on the implementing revision + fresh re-pin; the docs
   make that dependency explicit so nobody stages MODE B against the wrong
   ELF. MODE A remains available if the argv probe proves forwarding.
5. movspeed.zip sha re-verified twice (`3850a2b5…d6ad`); a transcription
   slip in the first manifest draft was caught and fixed before delivery —
   the shipped value equals the original staging pin byte-for-byte.

## Honesty

All expected lines are merged-chain host transcript facts; console results
remain [TARGET] until the controller executes. Stage 4-5 running=1 stays
SIMULATED-only; stage 1 native download stays deferred; no quality scores
implied. No file outside this worker dir was written.
