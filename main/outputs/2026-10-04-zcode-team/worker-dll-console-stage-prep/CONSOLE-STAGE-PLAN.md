# CONSOLE-STAGE-PLAN — all-families owned-dll candidate (133c5e9e)

Prepared by worker-dll-console-stage-prep, 2026-10-04. No console contact was
made by this worker; every step below is controller work. This plan makes the
controller's console stage of the all-families candidate a zero-decision,
single-pass operation by freezing the exact pattern that passed four previous
family payloads on this console.

## Candidate identity (verify before touching the console)

| item | value | source |
|---|---|---|
| build_id | `133c5e9e59f24c2fb57e7e3422c665a1` | out-sdk-composition receipt `result_profile.build_id` |
| artifact | `owned-dll-candidate.elf` | worker-combined-dll-allfam/out-sdk-composition/133c5e9e59f24c2fb57e7e3422c665a1/ |
| sha256 | `96296c1c92850623781c0ecf8126f2307c21a1380cc95bb99d277a8d7a07e81c` | same receipt `artifact.sha256` |
| bytes | `300456` | same receipt `artifact.bytes` |
| nature | SDK AMD64 ET_DYN payload, genuine crt1 `_start` — NOT a PRX; stage and dispatch as payload ELF under pldmgr only | same receipt `artifact.startup` |
| report path compiled in | `/data/modbridge-owned-dll-candidate.jsonl` | same receipt `result_profile.path`; `target_path_opened_by_build: false` |
| status | `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` (never executed anywhere) | same receipt `status` |

Pre-flight local checks (no console):
1. Re-hash the artifact locally: sha256 and byte count must match the table above.
2. Confirm the receipt `result_profile.path` is `/data/...` — this is the c49108fe
   lesson (CONSOLE-LOG step 8): a build composed without a /data report path logs
   to target /tmp, which the FTP server refuses; result is then unreadable and the
   build is wasted. This build already has the correct path; verify, do not assume.
3. Confirm `console_contact: false`, `target_execution: false`, `prx_created: false`
   in the receipt (candidate never executed; still true up to dispatch).

## Console prerequisites (from the four family precedents)

- Manager reachable: `GET http://ps5-host-not-configured.invalid:8084/list_payloads` -> 200 (same
  first step as CONSOLE-LOG items 1 and 7). If this fails, stop; do not probe
  anything else.
- Payload-only state, game closed: all four family payloads were dispatched with
  `game_launched: false` (explicitly recorded in the 6d295f56 and 1a6b37b7
  receipts; the mutex run used the same pldmgr route). No Townfall launch, no
  ploader involvement, no PRX slot interaction. See "Sequencing" below.
- Standing game-file HOLD honored: this stage touches only
  `/data/pldmgr/payloads/<NAME>/` and appends to
  `/data/modbridge-owned-dll-catalog` history — no game files, no autoload
  changes, no `/data/plugins/` writes.

## Step 1 — Upload (once)

New, distinct display filename so no prior alias is touched and history stays
attributable (MUTEX_DLL_TEST precedent — fresh alias for a changed
implementation; never overwrite TEST_THIS_DLL/NEXT_DLL_THREADS/INITONCE/MUTEX):

```
POST http://ps5-host-not-configured.invalid:8084/manage:upload?filename=ALLFAM_DLL_TEST.elf
     --data-binary @<build dir>/owned-dll-candidate.elf
```

Expected: response `OK`; a new catalog entry appears in `/list_payloads` as
`/data/pldmgr/payloads/ALLFAM_DLL_TEST/ALLFAM_DLL_TEST.elf`.

## Step 2 — Verification before dispatch (mandatory, both precedents)

1. `GET /manage:check` -> `ok` / `folder_exists` / `file_exists` for the new entry.
2. Anonymous FTP bounded `RETR` readback of
   `/data/pldmgr/payloads/ALLFAM_DLL_TEST/ALLFAM_DLL_TEST.elf`:
   - bytes must be `300456`
   - sha256 must be `96296c1c92850623781c0ecf8126f2307c21a1380cc95bb99d277a8d7a07e81c`
   (6d295f56 did exactly this and matched byte-identical; 1a6b37b7 recorded
   `ftp_readback_sha256` equal to the artifact hash, `attempts: 1`).
3. If the readback hash mismatches: STOP. Do not dispatch a bytes-divergent
   payload; record and leave staged (f72 precedent: "If the dispatch route
   misbehaves, stop and leave staged").

## Step 3 — Dispatch (once)

```
GET http://ps5-host-not-configured.invalid:8084/loadpayload:/data/pldmgr/payloads/ALLFAM_DLL_TEST/ALLFAM_DLL_TEST.elf
```

- Exactly one dispatch. Never re-send on ambiguity; reconcile first (AGENTS.md:
  reconcile ambiguous attempts, no duplicate dispatch; every prior receipt
  recorded `count: 1` / `attempts: 1`).
- Basis (for the receipt): standing family-console authorization; changed
  implementation (first target run of the combined all-families candidate —
  four new result families never run on target: fileio, crt_startup,
  module_enum, vm). Same basis wording as MUTEX_DLL_TEST ("new diagnostic,
  first target run of the family").

## Step 4 — Wait guidance

The payload is self-terminating: 4 report phases and exits within seconds
(worker timeout proof values 258/259 are the µs/ms-scale WAIT_TIMEOUT proof;
`repeat_wait: 0` in every prior run). Prior controllers read results
immediately after the dispatch `OK` and found all records present.

- Dispatch -> wait ~15 s -> FTP readback of the results file.
- If fewer than 4 records for build_id `133c5e9e...`: wait another 60 s and
  re-read ONCE. If still incomplete: stop, record what exists, do NOT re-dispatch.

## Step 5 — Readback

Anonymous FTP `RETR /data/modbridge-owned-dll-candidate.jsonl` (append-only,
history preserved — NEVER truncate or rewrite it; every prior run only
extended it).

- Expected: exactly 4 new appended records for build_id
  `133c5e9e59f24c2fb57e7e3422c665a1`, sequences 0..3, phases [1,2,3,5],
  contiguous, appended after the 20 records already collected
  (db1c40e6 ×4, f72faea5 ×4, 6d295f56 ×4, 1a6b37b7 ×4, 6f5bf85a ×4).
- Save the full file copy + a receipt JSON under
  `outputs/2026-10-04-zcode-controller/console/` following the existing naming
  convention (`allfam-console-receipt.json`,
  `allfam-console-result-2026-10-04.jsonl` with its own sha256), then run
  EXPECTED-RESULTS-LEDGER.md assertions on the final record.
- Update CURRENT-BUILD.md alias notes if used; leave all four prior payload
  aliases untouched (history_preserved pattern from the 1a6b37b7 receipt).

## Sequencing (vs the UE4SS composition-5 critical path)

Recommendation: run THIS console stage BEFORE the composition-5 launch session,
while the console is in payload-only state (game closed, manager 8084 up).

Evidence:
- All four family payload runs executed via pldmgr `/loadpayload` with
  `game_launched: false` — the payload route needs no game and no ploader arm.
  This is the minimal launch state: manager responsive, nothing else.
- The composition-5 path is a different mechanism entirely (ploader + PRX slot +
  game running; CONSOLE-LOG steps 10-12). Mixing an untested payload dispatch
  into a live ploader/game session would add an uncontrolled variable to both.
- Running first also keeps failure attribution clean: if the candidate fails to
  attach on target, the 12 new fileio imports are the prime suspects
  (see ledger failure signatures), and that conclusion is only attributable in
  an otherwise idle state.
- The sdg2 startup-checkpoint fself staged over the composition PRX slot
  (CONSOLE-LOG step 12) is untouched by this plan; the pending "user closes
  Townfall, re-arms ploader, relaunches" procedure can proceed afterwards
  unchanged.

If the console is already inside the composition-5 session when this stage is
scheduled: defer the DLL stage until that session ends and the console returns
to the payload-only state. Do not dispatch while Townfall is running.

## Risk-review summary (full table in REPORT.md)

The 55-import surface is byte-for-byte the same import set already compiled and
hash-recorded twice before (fileio-apply cd9b5aec, crt-integrate 53808597). The
console-passed mutex payload 6f5bf85a already imported libSceLibcInternal
malloc/free and libkernel_web clock_gettime/sceKernelMprotect. The only
never-dispatched imports are the 12 fileio-driven symbols; both of their
provider libraries are already loaded and console-proven on this path.
