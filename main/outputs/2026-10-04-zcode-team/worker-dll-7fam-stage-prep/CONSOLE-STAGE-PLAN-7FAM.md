# CONSOLE-STAGE-PLAN-7FAM — all-families + static-TLS owned-dll candidate (e72f5318)

Prepared by worker-dll-7fam-stage-prep, 2026-10-04. NO console contact was
made by this worker; every step below is controller work for when the user
authorizes console time. This plan makes the 7-family console stage a
zero-decision, single-pass operation by freezing the exact pattern that
passed the 6-family payload (`ALLFAM_DLL_TEST`, build 133c5e9e — all 4
records on target) and the four family payloads before it.

## Candidate identity (verify before touching the console)

| item | value | source (verified this session) |
|---|---|---|
| build_id | `e72f5318233845369a68556cd61e59ca` | worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/receipt.json `result_profile.build_id` |
| artifact | `owned-dll-candidate.elf` | same dir (on disk, verified) |
| sha256 | `83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae` | same receipt `artifact.sha256`; **re-hashed locally this session — EXACT match** |
| bytes | `334656` | same receipt `artifact.bytes`; re-verified on disk |
| nature | SDK AMD64 ET_DYN payload, genuine crt1 `_start` — NOT a PRX ("Genuine SDK payload crt1 _start, not a PRX module entry"); stage and dispatch as payload ELF under pldmgr only | same receipt `artifact.startup`; `file(1)` confirms "ELF 64-bit LSB pie executable, x86-64, FreeBSD" |
| report path compiled in | `/data/modbridge-owned-dll-candidate.jsonl` | same receipt `result_profile.path`; `target_path_opened_by_build: false` (the c49108fe /tmp-trap lesson does not apply — verify, do not assume) |
| status | `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; `console_contact: false`, `target_execution: false`, `prx_created: false`, `downloaded_dll_executed: false` | same receipt — candidate never executed anywhere, still true up to dispatch |
| import surface | 55 undefined imports, **set byte-identical to the console-passed 6-family ELF** (`nm -u` diff clean, delta ZERO); win64cc definitions +1 exactly (`@tls_import_thunk`), consumer binds +4 (`__tls_index`, `__tls_start`, `__tls_end`, `__tls_import`) | `nm -u` diff run this session; receipt `static_tls_profile` |

Pre-flight local checks (no console, repeat at staging time):
1. Re-hash: `shasum -a 256` and byte count of the artifact must equal the
   table above EXACTLY. Payload ELFs staged via `manage:upload` are stored
   verbatim, so the readback law applies: target readback sha MUST equal
   `83ee2ae9…` and 334656 bytes (6d295f56 matched byte-identical;
   1a6b37b7 recorded `ftp_readback_sha256 == artifact_sha256`, attempts 1;
   ALLFAM_DLL_TEST passed the same gate).
2. Confirm `result_profile.path` is `/data/...` (not /tmp) in the receipt.
3. Confirm `console_contact: false`, `target_execution: false`,
   `prx_created: false` in the receipt.

## Console prerequisites (from the five payload precedents)

- Manager reachable: `GET http://ps5-host-not-configured.invalid:8084/list_payloads` -> 200.
  If this fails, stop; do not probe anything else.
- Payload-only state, game closed: every prior payload dispatch ran with
  `game_launched: false` via pldmgr `/loadpayload`. No Townfall launch, no
  ploader arm, no PRX-slot interaction. See "Sequencing" below.
- Standing game-file HOLD honored: this stage touches only
  `/data/pldmgr/payloads/ALLFAM7_DLL_TEST/` and appends to
  `/data/modbridge-owned-dll-candidate.jsonl` — no game files, no autoload
  changes, no `/data/plugins/` writes, no prior payload alias touched.

## Step 1 — Upload (once)

Fresh, distinct display filename so no prior alias is touched and history
stays attributable (MUTEX_DLL_TEST precedent — fresh alias for a changed
implementation; never overwrite TEST_THIS_DLL / NEXT_DLL_THREADS / INITONCE /
MUTEX / ALLFAM / MODBRIDGE_INST_E2E):

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
WS=/WORKSPACE/.codex/worktrees/translation-core/what
ELF=$WS/outputs/2026-10-04-zcode-team/worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/owned-dll-candidate.elf

curl -sS -X POST "$HTTP/manage:upload?filename=ALLFAM7_DLL_TEST.elf" \
     --data-binary @"$ELF"
```

Expected: response `OK`; a new catalog entry appears in `/list_payloads` as
`/data/pldmgr/payloads/ALLFAM7_DLL_TEST/ALLFAM7_DLL_TEST.elf`.

## Step 2 — Verification before dispatch (mandatory, all precedents)

1. `curl -sS "$HTTP/manage:check"` -> `ok` / `folder_exists` / `file_exists`
   for the new entry.
2. Anonymous FTP bounded `RETR` readback of
   `/data/pldmgr/payloads/ALLFAM7_DLL_TEST/ALLFAM7_DLL_TEST.elf`:
   - bytes must be `334656`
   - sha256 must be `83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae`
3. If the readback hash mismatches: STOP. Do not dispatch a bytes-divergent
   payload; record and leave staged (f72 precedent: "If the dispatch route
   misbehaves, stop and leave staged").

## Step 3 — Dispatch (once)

```sh
curl -sS "$HTTP/loadpayload:/data/pldmgr/payloads/ALLFAM7_DLL_TEST/ALLFAM7_DLL_TEST.elf"
```

- Exactly one dispatch. Never re-send on ambiguity; reconcile first
  (AGENTS.md: reconcile ambiguous attempts, no duplicate dispatch; every
  prior receipt recorded `count: 1` / `attempts: 1`).
- Basis (for the receipt): standing family-console authorization; changed
  implementation — first target run of the 7-family candidate (static TLS
  is the new family; the other six family surfaces are byte-identical in
  import set to the console-passed 133c5e9e build).

## Step 4 — Wait guidance

Same self-terminating profile as ALLFAM_DLL_TEST: 4 report phases, exits
within seconds (`repeat_wait: 0` in every prior run; worker timeout proof
values 258/259 are µs/ms-scale WAIT_TIMEOUT proof).

- Dispatch -> wait ~15 s -> FTP readback of the results file.
- If fewer than 4 new records for build_id `e72f5318…`: wait another 60 s
  and re-read ONCE. If still incomplete: stop, record what exists, do NOT
  re-dispatch.

## Step 5 — Readback

Anonymous FTP `RETR /data/modbridge-owned-dll-candidate.jsonl` (append-only;
NEVER truncate or rewrite — every prior run only extended it).

- Expected: exactly 4 new appended records for build_id
  `e72f5318233845369a68556cd61e59ca`, sequences 0..3, phases [1,2,3,5],
  contiguous, appended after the **36 records already on the file**
  (db1c40e6 ×4, f72faea5 ×4, 6d295f56 ×4, 1a6b37b7 ×4, 6f5bf85a ×4,
  664f83a5 ×8 [fileio rebuild: one stage-1 partial + full pass],
  53808597 ×4 [crt-integrate], 133c5e9e ×4 [6-family ALLFAM_DLL_TEST]).
  Count was re-verified from the controller result copy this session; if the
  on-target file already differs (another worker's authorized run landed in
  between), assert only "appended after ALL existing records, none modified".
- Save the full file copy + a receipt JSON under
  `outputs/2026-10-04-zcode-controller/console/` following the existing
  naming convention (`allfam7-console-receipt.json`,
  `allfam7-console-result-2026-10-04.jsonl` with its own sha256), then run
  EXPECTED-LEDGER-7FAM.md assertions (same dir as this plan) on the final
  record.
- Update CURRENT-BUILD.md alias notes if used; leave all prior payload
  aliases untouched (history_preserved pattern).

## Sequencing — order across the pending console tests

Recommendation: **all payload-only stages first in one game-closed window —
(1) this 7-family DLL stage, (2) the E2E installer stage sequence — then the
UE4SS composition-5 launch session LAST, with the qdrv1 add-on riding in
that same single game session.** Only one game-launch window total.

The load-bearing distinction is payload-only vs game-launch:

- **Payload-only tests (7-family DLL, E2E installer)** run through pldmgr
  `/loadpayload` with `game_launched: false`: manager 8084 up, game closed,
  nothing else. Self-terminating in seconds (DLL) or cmd-file-paced (E2E).
  They need no ploader arm and no PRX-slot interaction, and both plans
  explicitly require the same idle state — "Never interleave the two
  dispatch mechanisms" (E2E plan §1) / "Do not dispatch while Townfall is
  running" (6-family plan).
- **Game-launch tests (UE4SS composition-5, qdrv1 ridealong)** need ploader
  armed + PRX slot + Townfall running. This is the only state that costs
  the user real effort (close game -> stage PRX(s) -> re-arm ploader ->
  launch -> wait -> recover), and it is shared: qdrv1
  (worker-record-sink-addon STAGE-PLAN-QDRV1) is designed to ride ALONGSIDE
  composition-5 as the `=2` entry in the same ploader.ini — it adds zero
  extra launches. So compose both into ONE launch session, never two.

Why this order inside the payload-only window:

1. **7-family DLL first**: it is the shortest stage (one upload, one check,
   one dispatch, one readback). Running it first keeps failure attribution
   clean in an otherwise idle state — if anything diverges, the ledger's
   triage table applies with no other test's state in play. Its only write
   outside its own alias is an append to the shared DLL-candidate JSONL;
   the E2E stage does not read that file, so no interference either way.
2. **E2E installer second**: its stage 3 commits a managed mod set under
   `/data/modbridge/**`; its own stage-6 uninstall + stage-7 reset returns
   the tree to the pre-E2E state, so the window ends clean. Running E2E
   first would put a committed mod set between upload and dispatch of the
   DLL payload — harmless in principle (the payload reads no mods) but an
   avoidable variable. Running E2E after the DLL readback also means the
   shared JSONL is final-before-E2E, simplifying the append assertion.
3. **UE4SS + qdrv1 session last**: single user-effort window; both PRX
   modules staged in one ploader.ini edit before one launch. If either
   payload stage above failed and needs a rebuild, the rebuild happens
   host-side while the console is still free — no game session is wasted
   waiting on host work.

If the console is already inside a UE4SS/game session when console time
opens: defer BOTH payload stages until that session ends and the console is
back to payload-only. If the user's available window only fits one stage:
take the 7-family DLL stage first (smallest, fastest, cleanest
attribution), E2E next, UE4SS+qdrv1 only when a full launch window exists.

## Risk-review summary (full table in EXPECTED-LEDGER-7FAM.md)

The 55-import surface is set-identical (`nm -u` diff clean this session) to
the ELF that already PASSED on this console as ALLFAM_DLL_TEST — every
provider library involved is already loaded and proven on this path. The
never-dispatched surface is narrow and specific: the image's own TLS
directory (build-patched data directory 9 -> `.rdataT`), the `.tls` /
`.rdataC` sections, and the 4 bound TLS symbols resolved inside the image
(`__tls_index`/`__tls_start`/`__tls_end` data + `@tls_import_thunk`
win64cc definition). A clean 6-family run followed by a TLS-specific
failure is attributable to exactly that surface; the ledger maps each
fixture code -120..-127 to its meaning.
