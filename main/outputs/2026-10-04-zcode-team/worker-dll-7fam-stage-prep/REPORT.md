# worker-dll-7fam-stage-prep — REPORT.md (2026-10-04)

Worker: worker-dll-7fam-stage-prep. No console contact, no target execution,
no network, no nested subagents; writes only under this worker directory.
Budget: ~30 tool calls of 50 — finished under budget.

Status: **7-FAMILY CONSOLE STAGE FULLY PREPARED** — artifact verified EXACT,
zero-decision plan + expected ledger + sequencing recommendation delivered.
The console run itself remains PENDING user authorization (console runs are
user-gated; nothing was uploaded, dispatched, or read back by this worker).

## Task 1 — Sources read and artifact verified

Read in full: `worker-static-tls-7fam/` (REPORT.md, CONSOLE-DELTA.md,
CLAIM.json, out-sdk-composition receipt + host JSONLs, host-validation dirs)
and the 6-family precedent pair
(`worker-dll-console-stage-prep/CONSOLE-STAGE-PLAN.md` +
`EXPECTED-RESULTS-LEDGER.md`), plus the pending-test sources for sequencing
(E2E console-stage plan, qdrv1 STAGE-PLAN-QDRV1, composition-5 rebuild
report) and the controller console copies.

Verification of the 7-family artifact
(`worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/owned-dll-candidate.elf`),
all run this session, all EXACT:

1. Local re-hash `shasum -a 256` = `83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae`
   == receipt `artifact.sha256`; bytes 334656 == receipt `artifact.bytes`.
   Readback law satisfied for a payload ELF (stored verbatim per the
   6d295f56/1a6b37b7 precedents), so the target FTP readback must equal this
   hash exactly.
2. `nm -u` undefined-import sets of the 7-family ELF and the
   console-PASSED 6-family ELF (`worker-combined-dll-allfam/…/133c5e9e…/owned-dll-candidate.elf`)
   are IDENTICAL: 55 == 55, diff clean, delta ZERO — every import provider on
   this ELF is already target-proven by the 6-family pass.
3. `file(1)`: ELF 64-bit LSB pie executable x86-64 (FreeBSD), genuine payload
   — not a PRX; `receipt.status` = `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_
   EXECUTED`, `console_contact/target_execution/prx_created/downloaded_dll_
   executed` all false.
4. Ledger oracle extracted from `host_result_records.normal` (all 4 records
   read from `host-normal.jsonl`): final record consumer_imports **82**,
   helper **10**, results **[86,93]**, worker **4/4** begin [5,1,258,259]
   with the unchanged result quadruple, all seven family keys
   `{"enabled":1}` incl. `static_tls`, cleanup {pending:0, retired:1},
   `memory.last.length` **147456** (6-family: 98304 — the one additional
   intra-record value change, consistent with the image growing 300456 ->
   334656 bytes). Cleanup-failure mode keeps first failure stage 25 /
   MB_WIN_WORKERS_NATIVE_ERROR, worker 2/2 retained-admission proof.
5. Console file state re-counted: `/data/modbridge-owned-dll-candidate.jsonl`
   holds **36 records** (through 133c5e9e seq3 phase5). The 6-family plan's
   "20 records" figure is stale — the 7-family 4 records must append at file
   positions 37-40. The plan/ledger state this explicitly.
6. Fixture codes -120..-127 verified in source
   (`vendor/windows-candidate/tests/worker-statictls.h` lines 128-159), so
   the triage table maps real assertions, not the delta doc's paraphrase.

## Task 2 — Deliverables

- `CONSOLE-STAGE-PLAN-7FAM.md` — frozen identity table (build
  `e72f5318233845369a68556cd61e59ca`, fresh alias **`ALLFAM7_DLL_TEST.elf`**,
  no prior alias touched), the exact upload (`manage:upload`) /
  check (`manage:check`) / FTP readback sha+bytes gate / single dispatch
  (`loadpayload:`) / ~15 s wait / JSONL readback commands with manager 8084
  + FTP 2121 endpoints, pre-flight local re-verification steps, stop rules
  (hash mismatch -> leave staged; ambiguity -> reconcile, never re-dispatch),
  and the payload-only vs game-launch sequencing section.
- `EXPECTED-LEDGER-7FAM.md` — the exact record deltas vs the 6-family
  console run: `static_tls` `{"enabled":1}` from seq1 (correctly ABSENT at
  seq0), consumer_imports 78 -> **82** (the 4 TLS symbols:
  `__tls_index`, `__tls_start`, `__tls_end`, `__tls_import`), helper 10
  unchanged, worker calls unchanged 4/4, results unchanged [86,93],
  `memory.last.length` 98304 -> **147456**, **phase shape otherwise
  unchanged** (phases [1,2,3,5], stages 0/2/9/11, dll_calls 3/3, entry
  attach [7,7]); console-only stdout 32/1 delivery deltas restated;
  failure triage mapping every plausible record shape, including the full
  -120..-127 code table (assertion + meaning + first suspect per code) and
  the honest-observation note that a NON-NULL block on the never-attached
  internal thread is fabrication-suspicious, not a pass.
- `CLAIM.json` — scope, read-only inputs, verification evidence, non-goals
  (written first).

## Task 3 — Sequencing recommendation

**All payload-only stages first in one game-closed window — (1) 7-family DLL
payload, (2) E2E installer stage sequence — then ONE UE4SS launch session
last, with the qdrv1 add-on riding alongside composition-5 in that same
session.** Cited basis:

- Payload-only tests (this DLL stage; E2E `MODBRIDGE_INST_E2E`) run via
  pldmgr `/loadpayload` with `game_launched: false` — five-proven pattern;
  both plans demand the same idle state and forbid interleaving the
  mechanisms. They cost the user almost nothing.
- Game-launch tests (composition-5 + qdrv1) need ploader arm + PRX slot +
  Townfall running — the only real user-effort window; qdrv1 is designed as
  the `=2` ploader.ini entry riding the SAME launch, so it adds zero extra
  launches. One launch window total.
- DLL before E2E inside the window: shortest stage first, cleanest failure
  attribution in an idle state, and the shared result JSONL is final before
  E2E's mod-set commits; E2E's own uninstall+reset ends the window clean.
- Fallbacks stated: if a game session is already running, defer both payload
  stages; if only one stage fits the window, take the 7-family DLL first.

## Honest limits

- Preparation only: zero console packets were sent; "zero-decision" applies
  to the controller run WHEN authorized, and every command still passes
  through the user's console-time authorization.
- The ledger's oracle is host-validated + SDK-compile-only evidence; a
  target PASS/FAIL is the console checkpoint's to record. No quality score
  is implied; no Windows-loader parity claim; gameplay remains a prototype.
- If the on-target JSONL grows between now and the run (another authorized
  worker's stage lands first), the append assertion degrades gracefully to
  "appended after ALL existing records, none modified" — stated in both
  deliverables.
