# TEST-RECEIPT — worker-batch-runner-8fam-patch (dll8 + fallback wiring), 2026-10-04

Scope: host/fixture testing ONLY. Zero console contact, zero network, zero target execution.
Raw outputs captured in this directory: `selftest-out.txt`, `fake-drive-out.txt`,
`plan-dll8-dryrun.txt`, `plan-all-dryrun.txt`, `plan-all-dll7-dryrun.txt`.

## 1. Pinned 8-family identity re-verified before any edit (BUILD-ID LAW)

Artifact `worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf`:

| check | pinned (CONSOLE-STAGE-PLAN-8FAM) | measured this session | result |
|---|---|---|---|
| sha256 | `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb` | identical (shasum -a 256) | EXACT MATCH |
| bytes | 336552 | 336552 | EXACT MATCH |
| receipt build_id / path | c3aba79f… / `/data/modbridge-owned-dll-candidate.jsonl` | identical | MATCH |
| receipt flags | console_contact / target_execution / prx_created / downloaded_dll_executed all false | all false | MATCH |
| receipt status | OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED | identical | MATCH |

## 2. Selftest (console_batch.py selftest) — 162 ok, 0 failed, rc=0

New coverage added this patch (on top of the previously green 122):

- `selftest A` plan-mode: `dll8` flow — all steps PLANNED, every step gated, non-handoff steps
  carry commands; anchors: ALLFAM8_DLL_TEST upload URL, ALLFAM8 loadpayload URL, manage:check,
  results jsonl path, `792811911e77`, `336552`, `EXPECTED-DELTA-8FAM`, `CONSOLE-STAGE-PLAN-8FAM`,
  `consumer_imports 90`, `memory.last.length 114688`, `memory.last.length 163840`,
  `-130..-137`, `allfam8-console-receipt.json`.
- `selftest A2` session order: default `all` runs dll8 (never dll7) with dll8 FIRST and names
  dll7 as the fallback attribution path in its ORDER note; with the dll7 selection, `all` runs
  dll7 (never dll8) and labels itself the fallback attribution session.
- `selftest B2` 8FAM verdict fixtures (verdict_8fam_records vs EXPECTED-DELTA-8FAM):
  - PASS: bare 4-record fixture; full 40-line transcript notes "positions 37-40 (dll8-first)".
  - FAIL: consumer_imports 82 (stale ALLFAM7 artifact shape) and 88 (partial SEH64 bind chain);
    helper_imports drift (must stay 10); memory.last.length stale 7fam 147456 at seq2; final
    memory.last oracle lost (163840 -> 114688 at seq3); seh64 key missing at record1;
    census-zero carrying seh64.
  - Triage: -133 names `__C_specific_handler`; -135 names unwind-info registration order;
    -136 names `RtlAddFunctionTable`; 7fam ranges still map (-122); foreign code -140 rejected.
  - Fallback-order positional case: 36 prior + 4 sevenfam (37-40) + 4 eightfam (41-44) -> PASS
    with an explicit "41-44 (expected: dll7 fallback ran first)" note; zero c3aba79f records
    names the stage 55/56 dual-owner suspects.
- All previously green sections (7FAM verdict, E2E blocks/ledger, MODE gate, cmd_gone,
  ploader.ini edit, CP0-CP7 classifiers) unchanged and still green — the dll7 behavior
  preservation check.

## 3. Fake-console engine drive (tests/fake_console_drive.py) — 31 ok, 0 failed checks, rc=0

In-process simulated console; curl + user gates intercepted; no network. New dll8 coverage:

- dll8 drive: all 10 steps PASS; upload used the fresh `ALLFAM8_DLL_TEST.elf` alias; the FTP
  readback served the real c3aba79f artifact bytes and passed the byte-identical gate
  (readback sha == 79281191… law); EXACTLY ONE dispatch (running total 1 dll7 + 15 e2e + 1 dll8);
  `allfam8-ledger-verdict.json` PASS vs EXPECTED-DELTA-8FAM with `appended_after_all_existing`
  true; `allfam8-console-receipt.json` written.
- dll8 stale-7fam shape (canned transcript with consumer_imports 82): verdict step dll8.7 FAILS,
  console-receipt step dll8.8 SKIPPED_HALT, dispatch still exactly once (by design, no re-send).
- All pre-existing scenarios (dll7 full drive, e2e 15-dispatch 35-line ledger with the rescue
  step dispatching nothing, stage-qdrv1 ini edit, game-evidence CP wiring, dll7 corrupt-transcript
  halt) remain green.

## 4. CLI wiring smoke checks (dry-run only)

- `python3 console_batch.py dll8 --dry-run` prints the exact 8FAM plan commands: upload URL,
  `loadpayload:/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf`, readback gate
  `sha256 == 792811911e…`, verdict gates with the 90 / 114688 / 163840 / -130..-137 oracles.
- `python3 console_batch.py all --dry-run`: 10 dll8/* steps, 0 dll7/* steps, dll8 first;
  ORDER note names dll7 the fallback attribution path.
- `python3 console_batch.py all --dll 7 --dry-run`: 10 dll7/* steps, 0 dll8/* steps; ORDER note
  labels the session the fallback attribution run and dll8 the preferred single run.
- `plan --only dll8` previews the dll8 flow only; help lists dll8 before dll7 (PREFERRED vs
  FALLBACK); `py_compile` clean.

## 5. Honesty

Everything above is logic testing against fixtures and the real on-disk artifact bytes.
No console was contacted, no payload uploaded or dispatched, no target process started, and no
claim is made about on-target behavior or timings — those remain [TARGET] until the
user-authorized console session runs the tool. The 4-record PASS shapes are canned fixtures
shaped from CONSOLE-STAGE-PLAN-8FAM / EXPECTED-DELTA-8FAM, not target evidence.
