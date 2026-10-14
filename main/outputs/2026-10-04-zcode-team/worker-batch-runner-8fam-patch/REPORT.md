# REPORT — worker-batch-runner-8fam-patch, 2026-10-04

Task: add `dll8` (CONSOLE-STAGE-PLAN-8FAM, ALLFAM8_DLL_TEST) to
worker-console-batch-runner's `console_batch.py` with the same gate discipline as
`dll7`; make the session order dll8-first with dll7 as the documented fallback
attribution path (`--dll 7|8`, exactly-one default); update USER-SESSION-GUIDE.md;
extend selftest + fake-console fixtures; all green; record ownership transfer +
diffs. Status: **complete, all tests green** (162 selftest ok / 0 failed; fake-console
drive 31 ok / 0 failed checks). Host/fixture testing only — NO console contact.

## Ownership transfer

`worker-console-batch-runner/` (tool author worker-console-batch-runner, CLAIM
status=complete) was handed to this worker and edited IN PLACE:

- edited: `console_batch.py`, `USER-SESSION-GUIDE.md`, `tests/fake_console_drive.py`
- preserved untouched: previous owner's `CLAIM.json`, `receipts/` (BUILD-RECEIPT.json,
  HOST-TEST-RECEIPT.md), `REPORT.md`, `runs/`
- pre-patch snapshots + unified diffs live in THIS worker's `diffs/` (see below)

## What changed in console_batch.py

1. **DLL8 pinned identity** — build `c3aba79f968a492b849e5ece2f009e8e`, alias
   `ALLFAM8_DLL_TEST.elf`, payload path `/data/pldmgr/payloads/ALLFAM8_DLL_TEST/…`,
   sha256 `792811911e773c9b…`, 336552 bytes, 36 existing records (dll8-first
   appends at 37-40; 41-44 after a dll7 fallback run). Identity re-verified on
   disk before any edit: shasum + stat EXACT match vs the artifact and its
   receipt (status OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED, all
   execution/contact flags false, /data report path).
2. **8FAM ledger** — `FAMILY_KEYS_8` (nine keys incl. `seh64`),
   `SEH64_FIXTURE_CODES` -130..-137 with the plan's per-code meanings,
   `FAMILY_RANGES_8`, `triage_8fam`, and `verdict_8fam_records` implementing the
   EXPECTED-DELTA-8FAM oracle: 4 records seq 0..3 / phases [1,2,3,5], census-zero
   clean, nine family keys `{"enabled":1}` from record1 (seh64 flagged first-time
   on target), `consumer_imports` 90 + `helper_imports` 10, `results` [86,93],
   worker 4/4 begin [5,1,258,259] + result quadruple, entry 7/7 / [0,0],
   contract_errors 0, **memory.last.length 114688 (record2) -> 163840 (record3
   FINAL; image grew 334656 -> 336552)**, cleanup {pending:0, retired:1},
   first_failure all zeros with -130..-137 triage naming, stdout_error-32
   console-only note, 82-shape stale-artifact note, and the hard append-only
   invariant (positional 37-40/41-44 expectation yields to it, as the plan
   requires).
3. **`build_dll8` + shared gate chain** — build_dll7 was refactored into a
   parameterized `_dll_payload_flow` (identity/receipt pre-flight with BUILD-ID
   LAW stop, G0 reachability, upload ONCE as fresh alias, manage:check, FTP
   readback == pinned sha EXACTLY + ELF magic with STOP on mismatch, dispatch
   EXACTLY ONCE, 15 s wait, results readback + verdict, console receipt);
   `build_dll7` and `build_dll8` are thin wrappers over it, so the gate
   discipline is literally the same code path. dll7 verdict gates and behavior
   are preserved verbatim.
4. **Session order / `--dll 7|8`** — `DEFAULT_DLL = "dll8"`; `build_all` composes
   `dll8 -> e2e -> stage-qdrv1 -> game-evidence` with an ORDER note naming dll7
   the fallback attribution path; `--dll 7` (on any flow subcommand and on
   `plan`) swaps dll7 in and labels the session the fallback attribution run.
   EXACTLY ONE DLL stage per session either way — never both by default. `dll8`
   and `dll7` also remain individually runnable subcommands; `plan --only`
   accepts both.
5. **selftest + fake-console drive** — dll8 plan anchors, session-order checks
   (default dll8-first, --dll 7 never dll8), the 8FAM PASS/FAIL fixture battery
   (incl. memory oracle mismatches, stale-7fam 82-shape, fallback-order 41-44
   case, -130..-137 triage needles), canned 8FAM transcript + ALLFAM8
   upload/readback/dispatch wiring in the fake console, a full dll8 drive
   (one dispatch, verdict PASS, receipt written) and a stale-shape drive
   (verdict FAIL -> receipt step SKIPPED_HALT, no re-send).

## What changed in USER-SESSION-GUIDE.md

Rewritten for the dll8-first session: `all` = dll8 -> e2e -> stage-qdrv1 ->
game-evidence; the exactly-one-DLL-stage rule; when (and only when) to run the
dll7 fallback attribution path (-130..-137 codes / stage 55/56 dual-owner
phase-2 stop / consumer_imports 88-89, remaining budget); the full 8FAM oracle
in Phase 1; receipt/result-copy naming (`allfam8-console-receipt.json`,
`allfam8-console-result-2026-10-04.jsonl`, positions 37-40 vs 41-44); updated
host-test counts (162 selftest checks + 31-check drive).

## Deliverables (this dir) and owned-tool edits

| file | what |
|---|---|
| `CLAIM.json` | ownership transfer + scope + verified pinned identity (written FIRST) |
| `diffs/console_batch.py.orig` / `.patch` | pre-patch snapshot (119783 B) + unified diff (719 lines) |
| `diffs/USER-SESSION-GUIDE.md.orig` / `.patch` | pre-patch snapshot + unified diff (95 lines) |
| `receipts/TEST-RECEIPT.md` | full test receipt (identity re-verification, selftest, drive, CLI smoke) |
| `receipts/selftest-out.txt` | raw selftest output (162 ok, 0 failed, rc=0) |
| `receipts/fake-drive-out.txt` | raw fake-console drive output (31 ok, 0 failed checks, rc=0) |
| `receipts/plan-dll8-dryrun.txt` | dll8 plan-mode preview (no execution) |
| `receipts/plan-all-dryrun.txt` / `plan-all-dll7-dryrun.txt` | default vs --dll 7 session order previews |
| owned tool: `worker-console-batch-runner/console_batch.py` | patched (sha256 8c3031ebf34c3475…; py_compile clean) |
| owned tool: `worker-console-batch-runner/USER-SESSION-GUIDE.md` | rewritten (sha256 7980aef66fe6cf62…) |
| owned tool: `worker-console-batch-runner/tests/fake_console_drive.py` | extended (sha256 999b974279883975…) |

## Honesty / limits

- Zero console contact in this worker: everything was tested in dry-run,
  selftest, and in-process fake-console mode against fixtures shaped from the
  frozen 8FAM plan documents plus the real on-disk artifact bytes for the
  readback law. No upload, no dispatch, no target execution, no claim about
  on-target behavior; timings and record shapes remain [TARGET] until the
  user-authorized session runs.
- The 4-record 8FAM PASS fixtures encode the EXPECTED-DELTA-8FAM oracle (incl.
  the memory.last 114688/163840 values the plan flags as corrected against
  EXPECTED-DELTA-8FAM's omission); they are expectations, not measurements.
- dll7 is intentionally untouched in behavior (fallback attribution path); its
  plan, alias, and ledger remain frozen per CONSOLE-STAGE-PLAN-8FAM Sequencing.
- No re-dispatch logic was added anywhere; the exactly-once dispatch law,
  stop-on-first-failure default, and reconcile-on-ambiguity rules are unchanged.
