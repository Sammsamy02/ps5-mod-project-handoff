# CONSOLE-STAGE-PLAN-8FAM — all-families + static-TLS + SEH64 owned-dll candidate (c3aba79f)

Prepared by worker-8fam-console-prep, 2026-10-04. NO console contact was made
by this worker; every step below is controller work for when the user
authorizes console time. This plan makes the 8-family console stage a
zero-decision, single-pass operation by freezing the exact pattern that passed
the 6-family payload (`ALLFAM_DLL_TEST`, build 133c5e9e — all 4 records on
target) and the four family payloads before it. It supersedes
CONSOLE-STAGE-PLAN-7FAM.md as the PREFERRED first run (see "Sequencing" for
the fallback rule and why).

## Candidate identity (verify before touching the console)

| item | value | source (re-verified this session) |
|---|---|---|
| build_id | `c3aba79f968a492b849e5ece2f009e8e` | worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/receipt.json `result_profile.build_id` |
| artifact | `owned-dll-candidate.elf` | same dir (on disk, verified) |
| sha256 | `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb` | same receipt `artifact.sha256`; **re-hashed locally this session — EXACT match** |
| bytes | `336552` | same receipt `artifact.bytes`; re-verified on disk |
| nature | SDK AMD64 ET_DYN payload, genuine crt1 `_start` — NOT a PRX ("Genuine SDK payload crt1 _start, not a PRX module entry"); stage and dispatch as payload ELF under pldmgr only | same receipt `artifact.startup`; `file(1)` confirms "ELF 64-bit LSB pie executable, x86-64, FreeBSD", build-id 0224ee94363709e5114bb133231aa4a7 |
| report path compiled in | `/data/modbridge-owned-dll-candidate.jsonl` | same receipt `result_profile.path`; `target_path_opened_by_build: false` — verify, do not assume |
| status | `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; `console_contact: false`, `target_execution: false`, `prx_created: false`, `downloaded_dll_executed: false` | same receipt — candidate never executed anywhere, still true up to dispatch |
| import surface | 55 undefined imports — **set byte-identical to BOTH the console-passed 6-family ELF (133c5e9e) and the 7-family ELF (e72f5318)** (`nm -u` sorted-set diff clean both ways this session; 55/55/55). SEH64 adds ZERO SDK imports: its 8 KERNEL32/VCRUNTIME names live only in the consumer PE fixture surface, bound through the two def libs | `nm -u` diffs run this session (VERIFICATION.md §2); receipt `import_providers` provider mapping equal to the 7-family receipt |
| host validation | `OWNED_CANDIDATE_HOST_CONTROLS_PASSED` twice, deterministic; results [86,93], worker 4/4, entry 7/7/[0,0], contract_errors 0, cleanup retired; consumer_imports measured exactly 90 from PE metadata; full -130..-137 exercise passed inside both runs | worker-dll-8fam-combined REPORT.md + both out-host-validation receipts |

Pre-flight local checks (no console, repeat at staging time):
1. Re-hash: `shasum -a 256` and byte count of the artifact must equal the
   table above EXACTLY. Payload ELFs staged via `manage:upload` are stored
   verbatim, so the readback law applies: **target readback sha MUST equal
   `79281191…` and 336552 bytes** (every prior payload run passed this gate:
   6d295f56 byte-identical, 1a6b37b7 `ftp_readback_sha256 == artifact_sha256`,
   ALLFAM_DLL_TEST the same).
2. Confirm `result_profile.path` is `/data/...` (not /tmp) in the receipt.
3. Confirm `console_contact: false`, `target_execution: false`,
   `prx_created: false` in the receipt.

## Console prerequisites (from the five payload precedents)

- Manager reachable: `GET http://ps5-host-not-configured.invalid:8084/list_payloads` -> 200.
  If this fails, stop; do not probe anything else.
- Payload-only state, game closed: dispatch via pldmgr `/loadpayload` with
  `game_launched: false`, as in every prior payload run. No Townfall launch,
  no ploader arm, no PRX-slot interaction. See "Sequencing" below.
- Standing game-file HOLD honored: this stage touches only
  `/data/pldmgr/payloads/ALLFAM8_DLL_TEST/` and appends to
  `/data/modbridge-owned-dll-candidate.jsonl` — no game files, no autoload
  changes, no `/data/plugins/` writes, no prior payload alias touched.

## Step 1 — Upload (once)

Fresh, distinct display filename so no prior alias is touched and history
stays attributable (MUTEX_DLL_TEST precedent — fresh alias for a changed
implementation; never overwrite TEST_THIS_DLL / NEXT_DLL_THREADS / INITONCE /
MUTEX / ALLFAM / MODBRIDGE_INST_E2E / ALLFAM7_DLL_TEST):

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
WS=/WORKSPACE/.codex/worktrees/translation-core/what
ELF=$WS/outputs/2026-10-04-zcode-team/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf

curl -sS -X POST "$HTTP/manage:upload?filename=ALLFAM8_DLL_TEST.elf" \
     --data-binary @"$ELF"
```

Expected: response `OK`; a new catalog entry appears in `/list_payloads` as
`/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf`.

## Step 2 — Verification before dispatch (mandatory, all precedents)

1. `curl -sS "$HTTP/manage:check"` -> `ok` / `folder_exists` / `file_exists`
   for the new entry.
2. Anonymous FTP bounded `RETR` readback of
   `/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf`:
   - bytes must be `336552`
   - sha256 must be `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb`
3. If the readback hash mismatches: STOP. Do not dispatch a bytes-divergent
   payload; record and leave staged (f72 precedent: "If the dispatch route
   misbehaves, stop and leave staged").

## Step 3 — Dispatch (once)

```sh
curl -sS "$HTTP/loadpayload:/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf"
```

- Exactly one dispatch. Never re-send on ambiguity; reconcile first
  (AGENTS.md: reconcile ambiguous attempts, no duplicate dispatch; every
  prior receipt recorded `count: 1` / `attempts: 1`).
- Basis (for the receipt): standing family-console authorization; changed
  implementation — first target run of the 8-family candidate (SEH64 is the
  new family; the SDK import surface it rides on is set-identical to the
  console-passed 133c5e9e build, and the static-TLS surface was host-proven
  in the 7-family tree).

## Step 4 — Wait guidance

Same self-terminating profile as ALLFAM_DLL_TEST: 4 report phases, exits
within seconds (`repeat_wait: 0` in every prior run; worker timeout proof
values 258/259 are µs/ms-scale WAIT_TIMEOUT proof).

- Dispatch -> wait ~15 s -> FTP readback of the results file.
- If fewer than 4 new records for build_id `c3aba79f…`: wait another 60 s
  and re-read ONCE. If still incomplete: stop, record what exists, do NOT
  re-dispatch.

## Step 5 — Readback

Anonymous FTP `RETR /data/modbridge-owned-dll-candidate.jsonl` (append-only;
NEVER truncate or rewrite — every prior run only extended it).

- Expected: exactly 4 new appended records for build_id
  `c3aba79f968a492b849e5ece2f009e8e`, sequences 0..3, phases [1,2,3,5],
  contiguous. Position depends on whether this stage runs before or after
  the 7-family stage:
  - **Recommended order (this plan first): the file currently holds 36
    records** (db1c40e6 ×4, f72faea5 ×4, 6d295f56 ×4, 1a6b37b7 ×4,
    6f5bf85a ×4, 664f83a5 ×8 [fileio rebuild], 53808597 ×4 [crt-integrate],
    133c5e9e ×4 [6-family ALLFAM_DLL_TEST]; count re-verified by the 7fam
    stage-prep from the controller result copy — this corrected the stale
    6-family-plan "20 records" figure). ALLFAM8_DLL_TEST then appends at
    **file positions 37–40**.
  - If the 7-family stage (ALLFAM7_DLL_TEST) runs first instead, it takes
    37–40 and **this stage appends at 41–44**.
  - Hard invariant either way: assert only "appended after ALL existing
    records, none modified"; if the on-target file already differs (another
    authorized run landed in between), the positional expectation yields to
    the invariant.
- Save the full file copy + a receipt JSON under
  `outputs/2026-10-04-zcode-controller/console/` following the existing
  naming convention (`allfam8-console-receipt.json`,
  `allfam8-console-result-2026-10-04.jsonl` with its own sha256), then run
  the ledger assertions in "Expected deltas" below on the final record.
- Update CURRENT-BUILD.md alias notes if used; leave all prior payload
  aliases untouched (history_preserved pattern).

## Expected deltas vs BOTH prior runs

Pass oracle per record (host-oracle values; the 6-family run proved these
fields match host on target):

| field | 6-family PASSED (133c5e9e) | 7-family plan (e72f5318) | **8-family expected (c3aba79f)** |
|---|---|---|---|
| records | 4: seq 0..3, phases [1,2,3,5] | same | same |
| enabled keys (`{"enabled":1}`) | 10 | 11 (`+ static_tls` from seq1) | **12: `+ seh64` from seq1** — 9 counted family keys (sync, init_once, mutex, fileio, crt_startup, module_enum, vm, static_tls, seh64) plus entry/clock/worker enablement fields |
| `counts.consumer_imports` | 78 | 82 (+4 TLS symbols) | **90 = 82 + 8 SEH64 names**: 7 × KERNEL32 (`RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`, `RtlAddFunctionTable`, `RtlRemoveFunctionTable`, `SetUnhandledExceptionFilter`, `RaiseException`) + 1 × VCRUNTIME140 (`__C_specific_handler`) — source-verified in `tests/worker-seh64-kernel.def` / `worker-seh64-vcruntime.def` |
| `counts.helper_imports` | 10 | 10 | 10 (SEH64 names are consumer-only; disjoint census) |
| `memory.last.length` (seq2/phase3) | 98304 | 98304 | **114688** |
| `memory.last.length` (seq3/phase5, final) | 98304 | 147456 | **163840** — host-measured in BOTH 8fam host runs (`memory.last` shape unchanged: operation 3, native_result 0); NOTE: EXPECTED-DELTA-8FAM.md omits this field and the "unchanged from 7fam" assumption is FALSE — the mapped image grew 334656 -> 336552 bytes |
| `results` | [86,93] | [86,93] | **[86,93]** (unchanged) |
| `worker` | 4/4, begin [5,1,258,259], result 0x81234567 | same | same (unchanged) |
| `entry` | attach [7,7], detach [0,0], contract_errors 0 | same | same (unchanged) |
| `cleanup` | {pending:0, retired:1} | same | same (unchanged); retire order now SEH64 first, then static TLS, then families 6..1 |
| SDK ELF undefined imports | 55 | 55 | **55 — identical set, delta ZERO** (verified vs both priors) |
| win64cc IR definitions | (6-family dict) | +1 `mb_win_static_tls_abi` | **+7 exactly: `mb_win_seh64_abi: 7`** added to build.py's expected dict (union asserted at build; host `seh64_profile` recorded `win64cc_entries: 7`, `exercised_imports: 8`) |
| new fixture codes | — | -120..-127 (static TLS) | **-130..-137 (SEH64)** — see triage below |
| console-only `stdout_error 32` / `stdout_records 1` | present | present | present — delivery-channel status, NOT a failure (target has no stdout) |

PASS = final record (seq3, phase 5) matches every 8-family value above,
`first_failure` all zeros, `seh64: {enabled:1}` present from seq1 onward,
and 4 records appended with no existing byte modified.

## Failure triage

Range ownership (inherited + new): sync/initonce −1..−53, mutex −60..−78,
fileio −80..−89, crt_startup −90..−97, module_enum −100..−104, vm
−110..−117 (6-family ledger), static_tls −120..−127 (7-family ledger),
**seh64 −130..−137 (new; `tests/worker-seh64.h`, source-verified)**.

| code | assertion that fired (source-verified) | meaning / first suspect |
|---|---|---|
| -130 | context probe: `mb_seh_probe() != 0x11`, or RIP/RSP insane (`!rip \|\| rsp <= rip`) | the CONTEXT capture over a real frame failed — `RtlCaptureContext` route or frame sanity in the fixture |
| -131 | in-image lookup: no function entry, or `entry < base`, or `end_address <= begin_address` | `RtlLookupFunctionEntry` did not return an in-image entry — unwind-data directory (.pdata/.xdata) not visible to the lookup at runtime |
| -132 | virtual unwind: control PC / establisher frame / frame RSP zero, or RIP outside the caller's begin/end range, or RSP not below probe RSP | `RtlVirtualUnwind` produced an insane caller frame — unwinding math or the copied CONTEXT |
| -133 | same-frame raise returned instead of transferring, or `mb_seh_handled != 1` (handle must happen exactly once) | `__C_specific_handler` dispatch did not route the raise to the handler |
| -134 | integer guard words not intact after restore (`guard[i] != 0x5E64+i`) | guarded-state restore corrupted registers/nonvolatiles |
| -135 | cross-frame: `__finally` not run, cross handler not reached, `mb_seh_raiser()` returned `0x5E65`, or unwound frames resumed | unwinding across the callee frame failed — the classic break is unwind-info registration order |
| -136 | dynamic registration over a bridge (non-PE) address: lookup misses after `RtlAddFunctionTable`, or base inexact | dynamic function-table registration path (the PS5-backend trampoline route's host-side analogue) |
| -137 | removal: table not gone after `RtlRemoveFunctionTable`, double-removal not refused, or unhandled-filter state round-trip wrong | deregistration/filter-state cleanup |

Generic signatures (all inherited from the 6/7-family ledgers, with two
8-family adjustments):

- 0 new records for `c3aba79f…`: payload never ran — `/list_payloads`,
  dispatch URL matched `ALLFAM8_DLL_TEST`; do NOT blind re-dispatch.
- Exactly 1 record (phase 1 only): early load crash. NOT the import set —
  the undefined-import set is byte-identical to the console-passed 133c5e9e
  ELF. First suspects: the two build-patched data directories already proven
  additive on this path (dir 9 TLS from 7fam; dir 3 EXCEPTION admitted only
  under the `MB_PE_REQUIRES_EXCEPTION` + `MB_PE_SEH64` build profile, per the
  merged pe_module admission) and the `.ehflags`-emitted unwind tables.
- 2 records, stops at phase 2 (stage 2, run_attempted 0): owner setup
  hang/crash. 8-family note: stages 55/56 are now the open/close window of
  TWO owners (static TLS then SEH64; each keeps its own status enum as the
  detail code). `module_cause` / detail names which.
- `consumer_imports` != 90: bind-chain regression. 78 = allfam; 82 = +TLS
  binds; 88..90 → count which of the 8 SEH64 binds landed (KERNEL32 73 /
  VCRUNTIME140 1 is the host-split oracle). `helper_imports` must stay 10.
- `seh64` key missing or `{enabled:0}` from seq1: stale artifact under the
  new alias (an ALLFAM7 payload would show 82/no seh64; an ALLFAM payload
  would show 78/neither) — re-check the readback hash before anything else.
- `results` != [86,93], worker/entry quadruple drift, cleanup
  {pending:1,retired:0}, `delivery.first_file_error` != 0,
  `memory.first_failure` nonzero: same meaning and first suspects as the
  7-family ledger.
- Manager `OK` but console unresponsive afterwards: stop, record, no
  re-dispatch; recovery is a user-level console action.

## Sequencing — single-run recommendation at the user's return window

**Recommendation: run THIS 8-family stage as the single DLL payload of the
window, with the 7-family stage kept staged as the fallback attribution run
— do not default to running both.** The batch-runner session order becomes
**dll8 -> e2e -> stage-qdrv1 -> game-evidence** (payload-only first; the
runner's current `all` order lists dll7 — apply the same slot to dll8; that
file belongs to worker-console-batch-runner's scope and was not edited by
this worker).

Why the 8-family run is the preferred single test:

1. **Superset, one window**: the 8-family build contains every surface the
   7-family run would prove (the six allfam families + static TLS ride the
   same host-proven code paths; the SDK import surface is set-identical to
   the console-passed 133c5e9e ELF). A PASS on ALLFAM8_DLL_TEST proves
   strictly more than a PASS on ALLFAM7_DLL_TEST, in the same one
   upload/check/dispatch/readback budget.
2. **Clean attribution is preserved anyway**: the never-dispatched surface
   is narrow and named — the SEH64 fixture codes -130..-137, the
   dir-3/EXCEPTION admission amendment, and the +8 consumer binds. A 6-family
   PASSED baseline followed by a failure mapped into -130..-137 attributes
   to the 8th family without needing the 7-family run first.
3. **Fallback rule**: if the 8-family run fails on a SEH64-specific surface
   (any -130..-137 code, a phase-2 stop in the stage 55/56 dual-owner
   window, or consumer_imports landing at 88/89), and the window still has
   budget, run ALLFAM7_DLL_TEST (its own plan, alias, and ledger are frozen
   and unchanged) to separate "7-family candidate fine, SEH64 graft broke
   it" from a shared-surface regression. That fallback run then appends at
   41–44 (this one having taken 37–40).
4. **Payload-only first, unchanged**: as in the 7-family plan, all
   payload-only stages (DLL + E2E installer) run in one game-closed window;
   the UE4SS composition-5 + qdrv1 ridealong is the single game-launch
   session LAST. Never interleave the two dispatch mechanisms.

If the console is already inside a game session when the window opens:
defer all payload stages until it ends. If the window fits only one stage:
take this 8-family DLL stage (smallest, fastest, cleanest attribution, now
also the superset), E2E next, UE4SS+qdrv1 only with a full launch window.

## Risk-review summary

The 55-import surface is set-identical to the ELF that already PASSED on
this console as ALLFAM_DLL_TEST; every provider library on the SDK side is
already loaded and proven on this path. The never-dispatched surface is
narrow and specific: the image's exception directory (admitted only under
the `MB_PE_REQUIRES_EXCEPTION` requirement gated by the `MB_PE_SEH64`
profile in the merged pe_module admission), the `.ehflags` unwind tables in
the consumer fixture, the two SEH64 def-lib bind surfaces, and the 8 bound
KERNEL32/VCRUNTIME names. A clean 6-family run followed by a
SEH64-specific failure is attributable to exactly that surface; the triage
table maps each fixture code -130..-137 to its meaning. Honest scope: a
PASS proves the candidate as built on the target route of the exercised
fixtures — it does NOT establish Windows-loader parity, H2 guard-frame RSP
parity, signal->exception delivery, resume-execution, vectored handlers, or
C++ EH (SEH64-IMPLEMENTATION §4 divergences inherited verbatim).
