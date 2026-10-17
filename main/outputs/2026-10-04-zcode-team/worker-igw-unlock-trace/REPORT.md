# REPORT — worker-igw-unlock-trace

Worker: worker-igw-unlock-trace · 2026-10-05 · LOCAL ONLY. Deliverables in order written: CLAIM.json → TRACE.md → PROPOSAL.md → REPORT.md (this file, last).

## Mission

Explain why the armed exec-write probe (xwp, build `igd1-6ebab07f2111`) SKIPPED all 6 transition rows at launch-9 (rows_seen 6, attempted 0, refused 0, skipped 6, attempts_used 0, budget 8), decide whether launch-10 can produce the IGW `W_WORKS`/`W_FAILS_R_WORKS` verdict without a probe change, spec a minimal bounded change if not, and state the effect of the 2-plugin ini (`1216aab6…`) that disables the probe.

## Answers

1. **Root cause (file:line):** the design-confirmation gate `t->confirmed == 0 → XWP_SKIP_NOT_CONFIRMED (outcome=2)` at `exec_write_probe.c:166-168` (enum `.h:72`, skip-counting `:190-193`). All 6 rows compiled with `confirmed=0` because `build_exec_write_probe.py:136` defaults `"confirmed"` to 0 when absent and the staged `TRANSITIONS.json` (IGW-1 T2/T3/T4/T5 at `0x7878000`, D-1 pair at `0x82bc000`) sets no `confirmed` key. The launch-9 jsonl corroborates: every `row` record logs `"confirmed":0`, every `refusal` logs `"outcome":2`. This is the deliberate fail-closed "explicit design go" gate (`exec_write_probe.h:9-19`, `FILL-IN-FOR-DESIGN-WORKER.md:26`) — not a readiness signal, status file, or other-module check; `module_entry.c:127` invokes the probe unconditionally and the VMMAP gates sit after it (and would count as `refused`, which is 0).
2. **Launch-10 without probe change: NO.** The gate input is compile-time const table data; no runtime state can flip it. The same PRX deterministically reproduces the all-skip file (host-tested logic).
3. **Needed change: YES — minimal and bounded:** add `"confirmed": 1` to row 0 (IGW-1 T2 X->RX, the verdict row) in `TRANSITIONS.json` only, rerun `build_exec_write_probe.py` + artifact gates + host checks (the designed injection path; machinery untouched; new `igd1-<hash12>` id since table bytes seed the build id). Full spec in PROPOSAL.md.
4. **Ini interaction:** the current 2-plugin ini `1216aab6…eda0` (supplier_composition=2, path_test=2; aligned_entry and ALL probes incl. xwp disabled) does not load the probe at all, and the launch-10 staging flow freezes that sha (l10.1/l10.5 byte-equality; no ini write; drift → bounded restore + refusal). So launch-10 yields zero new xwp records — the verdict timeline moves to the next user-coordinated staging that pairs the rebuilt `confirmed=1` armed PRX with an ini that includes the xwp line (9-entry canonical `fd9c7393…` restore or a new distinct minimal config). Until then the launch-11 walkguard ACTIVE fold stays blocked at fold-0 DEFAULT-OFF (worker-launch10-walkguard-prep: the one-define flip needs the IGW verdict, which is PARKED), and launch-10's exec-page risk remains decided by its own F29 log evidence (worker-executedrain-trace conditions C1/C2).

## Evidence pins

- `worker-writability-probe-build/src/exec_write_probe.c` (:124-135 table-empty early-out; :163-171 skip gates; :177-188 refuse gates; :190-196 skip/refuse counting; :304-318 summary)
- `worker-writability-probe-build/src/exec_write_probe.h` (:45 budget 8; :48 MAX_ROWS 32; :58 confirmed semantics; :69-82 outcome enum; :9-19 safety contract)
- `worker-writability-probe-build/src/module_entry.c` (:127 unconditional invocation)
- `worker-writability-probe-build/build_exec_write_probe.py` (:136 confirmed default; :143-175 table emission; :446-457 build-id seed)
- `worker-writability-probe-build/TRANSITIONS.json` (6 rows, no confirmed key)
- `worker-writability-probe-build/build-receipt-armed.json` (igd1-6ebab07f2111, armed, 6 rows)
- `worker-launch10-evidence-upgrade/fixtures/l9_terminal/from-dir/modbridge-exec-write-probe.jsonl` (== `fixtures/baseline_drift/baselines/xwp-jsonl-baseline.txt`) — launch-9 capture
- `worker-return-docs-refresh/REPORT.md` (2-plugin ini state, probe disabled, ini history)
- `worker-launch10-staging-packet/CLAIM.json` + `DIFF.md` (ini freeze at `1216aab6…`, no-ini-write law)
- `worker-launch10-walkguard-prep/REPORT.md` (fold-0 default; ACTIVE fold gated on the IGW verdict)
- `worker-executedrain-trace/DRAIN-TRACE.md` (§3: verdict still MISSING; C1/C2 decide launch-10)

## Honesty limits

- Local trace of retained bytes + sources; no console contact, no rebuilds executed, nothing staged, no game launch. The verdict itself remains UNPRODUCED until the confirmed-row rebuild actually runs on a probe-loading ini.
- No claim that launch-10 is unsafe or blocked by the probe: its exec-page outcome is orthogonal (decided by the F29/C1/C2 evidence at that launch).
