# worker-dll-console-stage-prep — REPORT

Worker: worker-dll-console-stage-prep (ZCode team, 2026-10-04). ~25 tool calls
of 60. No console contact, no network, no target execution, no nested
subagents; writes only under this worker directory.

Status: **CONSOLE_STAGE_PLAN_COMPLETE** — the controller can run the
all-families candidate's console stage as a single pass with zero decisions,
using CONSOLE-STAGE-PLAN.md + EXPECTED-RESULTS-LEDGER.md.

## 1. Working pattern distilled from the four family console receipts

Sources: `outputs/2026-10-04-zcode-controller/console/{6d295f56,1a6b37b7,mutex}-console-receipt.json`,
their result JSONL copies, and CONSOLE-LOG.md items 1-9. The pattern that
passed every family payload, unchanged:

1. `GET :8084/list_payloads` -> 200 (reachability only).
2. Upload ONCE: `POST /manage:upload?filename=<NAME>.elf` with artifact bytes
   -> `OK`; catalog entry `/data/pldmgr/payloads/<NAME>/<NAME>.elf`.
3. Verify: `/manage:check` ok/folder_exists/file_exists + anonymous FTP bounded
   RETR readback; bytes + sha256 must equal the artifact receipt exactly.
4. Dispatch ONCE: `GET /loadpayload:/data/pldmgr/payloads/<NAME>/<NAME>.elf`.
   Every receipt records count/attempts = 1.
5. Readback: FTP RETR `/data/modbridge-owned-dll-candidate.jsonl` — append-only,
   history preserved, never truncated; expect exactly 4 new records
   (phases [1,2,3,5]) for the new build_id; save copy + receipt JSON locally.
6. All four runs ran with `game_launched: false` — payload-only via pldmgr,
   no Townfall, no ploader, no PRX-slot interaction.

Known prior failure burned into the plan: c49108fe was composed without a
`/data` report path and logged to target /tmp (FTP-refused) — CONSOLE-LOG item
8. The 133c5e9e receipt `result_profile.path` is `/data/modbridge-owned-dll-candidate.jsonl`;
the plan verifies this locally BEFORE upload.

Frozen plan values: payload name `ALLFAM_DLL_TEST.elf` (fresh alias, MUTEX
precedent — no prior alias touched), artifact sha256
`96296c1c92850623781c0ecf8126f2307c21a1380cc95bb99d277a8d7a07e81c`, 300456 B.

## 2. RISK REVIEW — 55 SDK imports vs previously console-passed payloads

Console-passed PAYLOAD (ELF) candidates and their undefined-import counts:

| payload (console) | build_id | ELF undefined | console result |
|---|---|---|---|
| NEXT_DLL_THREADS.elf | 6d295f56 | < 42 (pre-mutex tree) | PASSED |
| INITONCE_DLL_TEST.elf | 1a6b37b7 | 42 | PASSED |
| MUTEX_DLL_TEST.elf | 6f5bf85a | **43** | PASSED |

The mutex payload's exact import list survives in
`worker-mutex-sdk-retry/elf-undefined.stdout` (43 symbols) and its receipt
(`import_providers`: 35 libkernel_web.so + 8 libSceLibcInternal.so = abort,
calloc, free, malloc, memcmp, memcpy, memmove, memset).

Specific imports named in the task:
- **libSceLibcInternal malloc/free: console-proven** — both in the 43-symbol
  passed mutex payload.
- **libkernel_web clock_gettime and sceKernelMprotect: console-proven** — both
  in the same list (clock_gettime since the first clock family run;
  sceKernelMprotect visible in elf-undefined.stdout line 38).

Exact delta, console-passed 43 -> candidate 55 (computed by set difference):

+12, all introduced by the fileio family:
- via **libSceLibcInternal.so** (provider already console-proven for
  malloc/free): `closedir, opendir, readdir, strcmp, strdup, strlen, strrchr`
- via **libkernel_web.so** (provider already console-proven for
  open/close/fstat/write/munmap): `lstat, mkdir, pread, pwrite, unlink`

Flagged as never-dispatched: exactly those 12 symbols. No import comes from a
provider library that has not already loaded and served a passed payload on
this exact route. Supporting stability evidence: the identical 55-symbol set
was already compiled and hash-recorded twice (worker-fileio-apply cd9b5aec
279504 B; worker-crt-integrate 53808597 281040 B), and the candidate links the
same provider stubs (`provider-libSceLibcInternal.so`, `provider-libkernel_web.so`,
`provider-libSceNet.so` commands in the receipt). Residual risk is therefore
symbol-resolution-level (12 first-time resolutions), not provider-level; the
ledger's failure-signature table makes a phase-1-only record immediately
attributable to them.

## 3. Static TLS as 7th family — assessment and recommendation

Delivery read: `worker-static-tls/REPORT.md` + `INTEGRATION.md` + receipts.
Host state: complete and clean (host/ASan+UBSan/TSan/red-stub/full
native+bridge receipts all green; `target_executed=false`, `console_contact=false`).

Integration requirements if added to the combined build:
- **ELF imports delta: ZERO** — the owner imports only internal
  `mb_win_threads_*` symbols; the static-TLS owner "requires no SDK symbols"
  (worker-static-tls REPORT). The 8 binder names (`__tls_index`, `_tls_start`,
  `__tls_end`, `__tls_import`, ...) are PE-side census names, not ELF imports.
- **win64cc IR count: +1** — exactly one bridge definition,
  `@tls_import_thunk(i32, i64)` (full receipt, verified).
- **Fixture hooks**: new `tests/worker-statictls.h` + def following the
  crtstartup recipe; new result key; census entries UE4SS.dll (TLS dir present,
  728 raw bytes, 2 callbacks) and dwmapi.dll (no TLS directory — family unused
  there).
- **The real cost is loader behavior, not imports** (INTEGRATION.md):
  point 4 — `mb_pe_module` currently REFUSES TLS-bearing images; a controller
  decision is required (extend admission when a static-TLS session is bound, or
  pre-open directory extraction). Point 3/5 — session/register/attach must be
  wired into the entry adapter lifecycle and thread attach/detach around every
  worker spawn/join. Point 6 — win64cc callbacks need an entry-bridge
  trampoline before any target-execution claim. Point 7 — one dynamic index per
  registered module from the shared 1088-slot pool.

**Recommendation: WAIT.** Do not integrate static TLS before the 6-family
candidate 133c5e9e passes on console. Rationale:
1. The candidate's console pass should stay single-variable; its only
   never-dispatched surface is the 12 fileio imports, and that attribution is
   worth protecting.
2. Static TLS requires a pe_module admission decision + entry-adapter lifecycle
   changes — modifications to the most sensitive component, exactly the kind of
   change that must ride its own host validation cycle and its own new
   build_id, not be folded in pre-dispatch.
3. Its zero-new-ELF-import property makes it SAFE to add after the console
   pass; there is no capacity or import pressure (78 consumer imports vs the
   documented 96 sync cap headroom) that would justify coupling it earlier.
Sequence: 6-family console pass -> static-TLS integration (7-family) with its
own host validation + receipt -> its own console stage reusing this same plan
template.

## 4. Sequencing vs the UE4SS composition-5 critical path

**Run the DLL console stage FIRST, in payload-only state (game closed), before
the composition-5 launch session.** Evidence and mechanics in
CONSOLE-STAGE-PLAN.md "Sequencing"; key facts:
- All four family precedents dispatched via pldmgr 8084 with
  `game_launched: false` — minimal launch state = manager responsive only.
- composition-5 is a different mechanism (ploader + PRX slot + running game,
  CONSOLE-LOG steps 10-12); mixing routes adds uncontrolled variables to both,
  and the staged sdg2 fself + relaunch procedure is untouched by this plan.
- If the composition-5 session is already underway: defer this stage until the
  console is back to payload-only. Never dispatch while Townfall runs.

## 5. Deliverables

- `CLAIM.json` (this directory)
- `CONSOLE-STAGE-PLAN.md` — exact upload/dispatch/wait/readback steps + sequencing
- `EXPECTED-RESULTS-LEDGER.md` — record-by-record oracle with exact expected key
  set (derived from the candidate receipt `host_result_records.normal.final` +
  result-key assertions), prior-console deltas to not misread (worker 4/4,
  consumer_imports 78, stdout_error 32), and a failure-signature triage table
- `REPORT.md` (this file)

## Honest limits

No console contact was made and no console state was changed by this worker;
the plan is untested until the controller executes it. Expected values are
derived from the candidate's host oracle plus four console precedents — target
behavior can still diverge (the ledger covers the plausible shapes, not all
possible ones). No quality scores, no >8 claims, no completion claim for the
modbridge runtime: this is console-stage preparation evidence only.
