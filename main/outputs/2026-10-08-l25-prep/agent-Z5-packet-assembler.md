# agent-Z5 — l25 packet assembler tooling (2026-10-08)

Scope honored: wrote ONLY under
`outputs/2026-10-08-l25-prep/packet/` (plus this report, explicitly
requested one level up). Mac-local, stdlib-only, no network, no console
contact, no source/engine files touched. Raw evidence files were read only.

## Deliverables (packet/)

1. **`assemble_packet.py`** (executable, python3 stdlib only) —
   manifest-driven assembler:
   - default (assemble): preflights required entries, stages every file into
     `review-l25-<UTCSTAMP>/` at its manifest `dest`, copies the manifest in
     as `MANIFEST.json`, writes `INDEX.md` (size + sha256 + one-line purpose
     per file + omitted-optional list), creates
     `review-l25-<UTCSTAMP>.zip` **create-only** (O_EXCL `open(...,'xb')` +
     explicit pre-check; exit 3 if the path exists), then re-reads every zip
     member, recomputes size+sha256 vs staging and writes
     `VERIFY-<UTCSTAMP>.md` (overall PASS/FAIL + zip sha256; exit 4 on
     mismatch).
   - `--check`: read-only readiness table (READY/PENDING per entry, owner
     column, READY*/PENDING counts, per-owner pending breakdown); exit 1
     while any `required:true` entry is missing — `pending` markers do not
     waive requirements. Duplicate manifest paths and duplicate zip dests are
     schema errors (exit 2). Exit 5 guards a pre-existing staging dir.
   - Entry schema supports `path` (relative-to-manifest-dir or absolute),
     `glob` (+`dest_dir`, `min_matches`), `dest`, `required`, `pending`
     (owner label), `purpose`.

2. **`packet-manifest.json`** (TEMPLATE, JSON validated) — 6 sections,
   48 entries:
   - claude-review: `../claude-review-20261008-l25.md`,
     `../CLAUDE-REPORT-20261008.md` (both exist).
   - analysis-corpus: 14 agent-*.md reports present today (B,F,G,H,I,J,K,N,
     P,S,V,X,Y,Z3); agent-M and agent-T live in their dedicated sections so
     each file appears exactly once (16 agent reports total).
   - launch-ops: LAUNCH-PLAN.md, experiment-matrix.md, 3 settings arms +
     3 .sha256 sidecars.
   - l25-tree-state: agent-M inventory + absolute l25-runtime paths: the 3
     CHANGES.json (typescan-r4/profile/thread), tests/PRE_RUN_HASHES.txt,
     tests/RESULTS.json, profile/tests/RESULTS.json, typescan-r4/REPORT.md —
     all verified present.
   - evidence: agent-T klog-capture (exists); agent-Z1 baseline report
     (**pending: agent Z1**); vanilla klog glob under
     `outputs/2026-10-08-l25-vanilla/` + `VANILLA-VERDICT.md` (**pending:
     vanilla test (root)**); `/tmp/l24-ab2-ue4ss.log` (exists; copied
     durably into the packet as `evidence/`); optional raw l23 klog baseline
     (3.1 MB, required:false).
   - build-artifacts: 11 rows (**pending: agent R**) — variant A/B PRXs,
     per-variant STATIC-CHECKS.json + EXACT-IMPORT-IDENTITIES.json,
     TWIN-RECEIPT.json, TARGET-COMPILE.json, BUILD-REPORT.md,
     PREFLIGHT-TABLE.md, FINAL-RECEIPT.json — placeholder paths under the
     future `../build/` dir, layout mirrored from the l24 package zip.

3. **`PACKET-PROCEDURE.md`** — when to run `--check` (after each agent
   lands; add/land manifest rows first), the hard assemble gate (only after
   agent-R build artifacts + vanilla verdict exist and `--check` exits 0),
   the create-only one-name-per-review-round zip rule (failed/superseded
   rounds keep their zip untouched; new round = new stamp), and the exact
   two root commands.

## Path assumptions (also in manifest meta.path_assumptions)

- `../build/` does not exist yet; `build_runtime.py` natively writes
  `outputs/2026-10-08-l25-build-r2/`. Agent R or root must copy final
  artifacts into `../build/` (l24-package layout) or repoint the manifest
  rows. PRX filename assumed `modbridge_supplier_composition.prx` (l24
  precedent).
- Z1 report assumed to land as `../agent-Z1-klog-baselines.md` (in flight
  per CLAUDE-REPORT §8f; no naming precedent exists yet).
- Vanilla harvest: canonical archive dir
  `outputs/2026-10-08-l25-vanilla/` per agent-T §5.6; verdict file name
  `VANILLA-VERDICT.md` assumed; klog row is a glob (timestamped filename).
  Live tap `/tmp/l25-vanilla/klog-20261008-192654.log` is volatile /tmp —
  only the archived copy is packet evidence.
- `/tmp/l24-ab2-ue4ss.log` (107,889 B, exists) is volatile /tmp; the
  assembler copies it into the packet as `evidence/l24-ab2-ue4ss.log`.

## --check run on the template (2026-10-08, exit 1 — expected)

35 READY files on disk; 14 PENDING entries, all required (owners:
agent R x11, agent Z1 x1, vanilla test (root) x2); 0 optional missing.
Full table archived in this session's terminal output; the READY set covers
both claude-review receipts, all 14 corpus reports, all 8 launch-ops files,
all 8 l25-tree-state files, agent-T, the A/B v2 log, and the optional l23
klog. RESULT: NOT READY — do not assemble (until agent R's artifacts and the
vanilla verdict land).

## Self-test (in a throwaway `.selftest/` dir, deleted afterwards)

- Assemble happy path with pending rows relaxed: staged 38 files (incl. 3
  absolute-glob matches), INDEX.md + MANIFEST.json generated, zip created,
  VERIFY overall PASS (every member hash matched staging). Cleanup done;
  no test artifacts remain in packet/.
- Refusal path: assemble with required entries missing → readiness table +
  exit 1 (no writes).
- Create-only guards (deterministic, stubbed clock): pre-existing zip →
  exit 3; pre-existing staging dir → exit 5.

## Root's two commands at packet time

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/packet
python3 assemble_packet.py --check     # readiness gate; exit 0 required
python3 assemble_packet.py             # assemble INDEX+zip+VERIFY (create-only)
```
