# REPORT.md — worker-e2e-runconfig-b, 2026-10-04 (written LAST)

Worker: worker-e2e-runconfig-b. Task: pin down what RUN-CONFIG B is exactly,
then execute every host-executable step of it against the PROMOTED tree,
leaving the console-bound remainder explicitly [PENDING user console session].
No console contact, no target execution, no nested subagents. CLAIM.json first,
RUNCONFIG-B-RESULTS.md carries the per-step verdicts + receipts; this report is
the summary.

## 1. RUN-CONFIG B (the answer to task 1)

RUN-CONFIG B is the **MODE B / config-file-entry E2E dispatch sequence**:
`worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` §1–§6, which fuses
`worker-e2e-console-dry/CONSOLE-E2E-EXECUTION.md` (execution plan) +
`worker-e2e-console-dry/E2E-CMD-SCHEMA.md` (config-entry design, §8
preconditions). It is distinguished from RUN-CONFIG A (same stage order via
argv grammar against the D1-fixed build `e3489297…`) by a hard payload
identity gate: MODE B runs ONLY when the staged readback equals the re-pinned
config-entry revision (`CONFIG_ENTRY_SHA`/`CONFIG_ENTRY_BYTES`); it can never
pass against `e3489297…`, which has no cmd-file reader. Sequence: 13 sha-verified
cmd JSONs STORed to `/data/modbridge/intake/e2e-cmd.json`, bare dispatch, the
installer consumes the file (atomic `.consumed` rename BEFORE the verb) and
synthesizes argv; per-dispatch proof = `cmd_gone` + `evid` readback; PASS =
Step C evidence set with the 35-line corrected ledger, SIMULATED label on
stages 4-5, stage-6 CORRECTION expectation, F-R2 notes.

## 2. Execution form (task 2 precondition)

`worker-e2e-composer-splice` has NOT delivered (directory absent at run
start). Verified directly that `composition/vendored/modinst_core.c` is still
pristine `45c04d08…` (74749 B). Execution form: **PROMOTED-RECEIPT +
VENDORED-PRE-SPLICE**. The build-r1 payload was built from the patch-apply
form of the same target bytes (`43ea66c1…`), so no payload byte depends on the
pending splice; a post-splice re-run of my steps should reproduce them.

## 3. What I executed and found (all fresh outputs, logs/01–08)

| step | verdict | key receipt |
|---|---|---|
| §2 pre-flight: `shasum -c SHASUMS.txt` over the 13 staged cmd files | **PASS** 13/13, exit 0 | logs/01 |
| Promoted receipt re-verification | **PASS** — `receipts/patches.json` = `63c28979…80f9`, cmp-identical to `patches.json.new`; 54 tuples; prefix ends `r2-install-status-ok`; tail `g1-cmd-entry-{parse,allowlist,consume}`; endpoint `43ea66c1…`; `.51.bak` present | logs/02 |
| Schema grammar validation of the staged bytes (§2/§3/§5/§6 + §4 exact-byte compare) | **PASS** 13/13, exit 0 | logs/03 |
| §3 identity gate, host form | **PASS → MODE=B resolvable** — build-r1 `a21771f8…`/681888 B/ELF magic == re-pin values; r2 twin differs in exactly 16 build-id bytes (offsets 605681–605696); D1 fallback `e3489297…`/681528 still present | logs/04 |
| Independent G1 replay on the promoted tree (read-only) | **PASS** — `45c04d08…→330d47f2…→91a122a8…→43ea66c1…`, all anchors occurrences==1, byte-identical to the promotion endpoint, receipt tail literals == G1_BLOCK | logs/05 |
| Precondition ledger (schema §8) | **MET** — imports 116/missing=[] (census gate PASS); suites re-run green on the build-r1 chain: bridge 75/0, F-R2 87/0, fault 186/0, config-entry suite OVERALL PASS (46 OK, 13x exit-6 refusal matrix, transcript `31a4764a…`); build receipt `console-build-config-entry.json` all gates PASS; re-pin values delivered | logs/06–07 |

## 4. Findings (task 3 remainder + deltas)

1. **[PENDING user console session]** — the inherently console-bound part:
   Step P upload + FTP readback gate, all 15 `cmd_put → dispatch → cmd_gone →
   evid` cycles, Step C on-console evidence, and (only on abort) the §6 reset.
   Exact commands: `worker-e2e-console-stage-prep/CONSOLE-STAGE-PLAN.md` §2–§5
   (the substitution-complete block: 8084 conventions, `CONFIG_ENTRY_SHA=
   a21771f8…`, `CONFIG_ENTRY_BYTES=681888`, `$PL` → build-r1) executed by
   `worker-console-batch-runner/console_batch.py`. That runner's fake-drive
   rehearsal (2026-10-04 22:04) shows **15/15 dispatches PASS, ledger 35/35,
   0 failed ends** — rehearsal only, NOT console evidence. The console E2E
   stage remains pending the user, consistent with promotion-apply's note.
2. **Re-pin substitution state**: `STAGED-E2E-COMMANDS.md` §1 lines 49-50 still
   hold `REPLACE_WITH_…` placeholders. Values are delivered (PAYLOAD-DELIVERY
   §2) and independently recomputed here; the operative substituted paste-block
   already exists in the stage-prep plan. I did not edit the staged doc —
   that literal substitution is the controller's.
3. **Port erratum** (already adjudicated upstream, verified here): manager is
   **8084** (proven via console-context version endpoint); PAYLOAD-DELIVERY
   §3's "9223" is a documented erratum; batch runner acks it and stops rather
   than probing alternates.
4. Minor doc nit, no gate impact: STAGED header claims SHASUMS.txt has no
   trailing newline; the file ends `0x0a`. The 13 dispatch JSONs are
   newline-free compact bytes (the load-bearing claim).

## 5. Honest scope

Everything above is host-side verification/replay against local bytes and
existing receipts. No step is claimed PASS without its own captured output.
No console behavior is claimed anywhere; MODE B on the console remains
[TARGET]. No writes outside this worker dir (`CLAIM.json`, `REPORT.md`,
`RUNCONFIG-B-RESULTS.md`, `logs/`).

## 6. Artifacts

| file | what |
|---|---|
| `CLAIM.json` | claim (written FIRST; updated with final result + log list) |
| `RUNCONFIG-B-RESULTS.md` | definition + per-step verdicts + receipts + pending-console command sequence |
| `logs/01…08-*.log` | raw transcripts: cmd-file shas, promoted receipt, schema validation, identity gate, G1 replay, precondition ledger, suites/batch-runner, port delta |
| `REPORT.md` | this summary (written LAST) |
