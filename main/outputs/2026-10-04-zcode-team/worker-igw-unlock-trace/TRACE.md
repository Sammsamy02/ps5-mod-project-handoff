# TRACE — why all 6 xwp rows SKIPPED at launch-9, and what it takes to get the IGW verdict

Worker: worker-igw-unlock-trace · 2026-10-05 · LOCAL ONLY (source + retained launch-9 byte evidence; no console contact, no builds, no target execution).

Verdicts answered here:

1. Why launch-9 (armed build `igd1-6ebab07f2111`, `armed=1`, `table_rows=6`) produced
   `rows_seen=6, attempted=0, refused=0, skipped=6, attempts_used=0, budget=8`.
2. Whether launch-10 can produce the IGW verdict (`W_WORKS` / `W_FAILS_R_WORKS`)
   WITHOUT changing the probe.
3. What the 2-plugin ini `1216aab6…` (probe DISABLED) does to the verdict timeline.

---

## 1. Probe sources located

| artifact | path (all under `outputs/2026-10-04-zcode-team/`) |
|---|---|
| probe machinery | `worker-writability-probe-build/src/exec_write_probe.c` + `.h` — `ig_run_exec_write_probe()` transition-table runner |
| module entry | `worker-writability-probe-build/src/module_entry.c` — `module_start` calls the probe **unconditionally** at `:127` |
| transition table source | `worker-writability-probe-build/TRANSITIONS.json` — 6 rows (IGW-1 T2/T3/T4/T5 at `0x7878000`; D-1 pair at `0x82bc000`), generated into `generated/xwp_transitions.{h,c}` by the builder |
| builder | `worker-writability-probe-build/build_exec_write_probe.py` |
| launch-9 build receipt | `worker-writability-probe-build/build-receipt-armed.json` — `build_id igd1-6ebab07f2111`, `variant armed`, `table_rows 6`, deliverable `modbridge_exec_write_probe_armed.prx` (fself sha `48949316…`, 20013 B) |
| launch-9 jsonl (retained) | `worker-launch10-evidence-upgrade/fixtures/l9_terminal/from-dir/modbridge-exec-write-probe.jsonl` (byte-identical to `fixtures/baseline_drift/baselines/xwp-jsonl-baseline.txt`) |

## 2. The skip-condition chain, file:line

All citations are `worker-writability-probe-build/src/exec_write_probe.c` / `.h` / `build_exec_write_probe.py` unless noted. `exec_write_probe.h:69-82` defines the outcome enum (`outcome=2` semantics: **`XWP_SKIP_NOT_CONFIRMED = 2` — "row.confirmed == 0"**, header line 72).

Per row, in fail-closed order (`exec_write_probe.c:137-210`):

| order | gate | line | outcome | counted as |
|---|---|---|---|---|
| 0 | table empty (`xwp_transitions_count == 0`) | `:124-135` | (early-out record, no rows at all) | — |
| 1 | `i >= XWP_MAX_ROWS` (32) | `:163-165` | `XWP_SKIP_ROW_OVERFLOW` (4) | skipped (`:190-193`) |
| 2 | **`t->confirmed == 0`** | **`:166-168`** | **`XWP_SKIP_NOT_CONFIRMED` (2)** | **skipped** |
| 3 | `g_xwp_attempts_used >= XWP_ATTEMPT_BUDGET` (8) | `:169-171` | `XWP_SKIP_BUDGET` (3) | skipped |
| 4 | fresh same-invocation VMMAP scan must cover `[addr, addr+page_count*0x1000)` — **only reached if gates 1-3 passed** (`if (!gate_failed)` at `:177`) | `:177-183` | `XWP_REFUSE_NOT_COVERED` (5) | refused (`:195`) |
| 5 | covering span `prot == from_prot` | `:184-187` | `XWP_REFUSE_PROT_MISMATCH` (6) | refused |

Skip/refuse counting: `:190-196` (SKIP_* → `g_xwp_rows_skipped`, REFUSE_* → `g_xwp_rows_refused`). Summary record: `:304-318`. There is deliberately **no** unarmed skip gate in this build (comment `:155-160`): the SKELETON/stub question is resolved at compile time (`IG_XWP_MPROTECT_ARMED`, generated `ingame_config.h`, builder `:208`).

There is **no other gate anywhere**: no readiness signal, no status file, no runtime-status state, no other-module check. `module_entry.c:127` runs the table unconditionally at `module_start`; the only runtime input the machinery has (the fresh VMMAP scan) sits *after* the confirmed gate.

### What `confirmed` was in the launch-9 binary

`XwpTransition.confirmed` is defined `exec_write_probe.h:58`: "1 ONLY on the design's designated far page". The value is **compile-time const table data** (`const XwpTransition xwp_transitions[]`, generated `xwp_transitions.c`), produced by the builder at `build_exec_write_probe.py:136`:

```python
"confirmed": 1 if t.get("confirmed") else 0,
```

The staged `TRANSITIONS.json` (6 rows, notes verbatim "IGW-1 T2/T3/T4/T5 … 0x7878000" and "D-1 … 0x82bc000") carries **no `confirmed` key on any row** → all 6 rows baked with `confirmed=0` into `igd1-6ebab07f2111`.

## 3. Launch-9 evidence read-back (retained jsonl)

`fixtures/l9_terminal/from-dir/modbridge-exec-write-probe.jsonl` (== baseline):

- seq 1 `entry`: `armed=1, table_rows=6`, note: *"exec-write probe skeleton; transition table drives everything; **no write before the design's conf**[irmed row]"* — the probe itself announces the gate.
- seq 2 `startup`: `attempt_budget=8`, all channels open.
- seq 3-14: six `row` records each logging `"confirmed":0` verbatim from the table (`:148`), each followed by a `refusal` record with `"outcome":2, "no_memory_touched":1`.
- seq 15 `summary`: `rows_seen=6, rows_attempted=0, rows_refused=0, rows_skipped=6, attempts_used=0, budget_exhausted=0, armed=1, mprotect_route="sceKernelMprotect_import"`.

### Alternatives eliminated

| candidate skip reason | excluded because |
|---|---|
| `XWP_SKIP_UNARMED` (1) | `armed=1` in entry + summary; route `sceKernelMprotect_import`; no unarmed gate exists in the code (`:155-160`) |
| `XWP_SKIP_BUDGET` (3) | `attempts_used=0 < 8`; host test `tests/out-gen-armed.jsonl` shows outcome=3 only at `attempts_used=8` |
| `XWP_SKIP_ROW_OVERFLOW` (4) | 6 rows < `XWP_MAX_ROWS=32` (`exec_write_probe.h:48`) |
| `XWP_REFUSE_NOT_COVERED` (5) / `XWP_REFUSE_PROT_MISMATCH` (6) | would increment `rows_refused` (`:195`), which is 0; also unreachable — the confirmed gate short-circuits first |

**Root cause, one sentence:** every row was recorded-and-skipped with `outcome=2` because the design-confirmation flag `confirmed` was 0 in all 6 compiled table rows — the staged `TRANSITIONS.json` never set `"confirmed": 1`, and the builder defaults absence to 0 (`build_exec_write_probe.py:136`). This is the deliberate fail-closed "explicit design go" gate of `exec_write_probe.h:9-19` (safety contract item b) and `FILL-IN-FOR-DESIGN-WORKER.md:26` ("`confirmed` MUST be 1 ONLY on rows you have designated") — not a malfunction, not a runtime dependency.

## 4. Can launch-10 produce the IGW verdict WITHOUT changing the probe?

**NO.**

- The gate input is baked into the const table at build time. Nothing at runtime can flip it: no status file is read, runtime-status is not consulted, and the composition running further changes nothing — the probe is a single-shot `module_start` run (`module_entry.c:127`, inert afterwards; budget is per invocation).
- Re-launching the same `igd1-6ebab07f2111` PRX deterministically reproduces the identical 15-record all-skip file (the logic is host-tested in `tests/run_host_checks.py`; the launch-9 capture already IS that deterministic output).
- Therefore the verdict needs a **probe change**: the minimal bounded one is the designed injection path — set `"confirmed": 1` on the design-designated row(s) in `TRANSITIONS.json` only, rerun `build_exec_write_probe.py`. Machinery files are never touched; `TRANSITIONS.json` bytes are part of the build-id seed (`build_exec_write_probe.py:446-457`), so the rebuild carries a distinct `igd1-<hash12>` id. Spec in `PROPOSAL.md`.

## 5. The ini interaction — probe DISABLED in the 2-plugin config

- The current console `ploader.ini` is the SHORTEST 2-plugin config, sha `1216aab6…eda0` (`supplier_composition=2, path_test=2`; **aligned_entry and ALL probes incl. xwp disabled**) — `worker-return-docs-refresh/REPORT.md`. The 9-entry canonical (`fd9c7393…`, restore copy `2026-10-04-zcode-controller/launch9-evidence/ploader-ini-9entry-restore.txt`) that ran launch-9 **with the xwp line first** is preserved but superseded on console.
- The launch-10 staging packet **freezes** that ini: `worker-launch10-staging-packet` steps l10.1/l10.5 require byte-equality with `1216aab6…` before and after, the flow issues NO ini write, and any drift triggers bounded restore + refusal. Its target is only the one-file composition PRX replacement.
- **Plain consequence for the verdict timeline:** launch-10 as staged will not even load the probe — zero new xwp records, fresh or otherwise; the IGW verdict cannot come from launch-10. It becomes possible only at a later, user-coordinated staging that (a) uses the rebuilt `confirmed=1` armed PRX (launch-11 proposal) and (b) restores an ini that includes the xwp plugin line (the 9-entry canonical, or a new minimal config adding just that line — a distinct-ID decision, never a silent edit of the pinned `1216aab6…` config).

## 6. Consequence for the launch-11 walkguard fold

`worker-launch10-walkguard-prep` shipped the walk-site RX guard at **fold-0 DEFAULT-OFF** (`#define RC_ASM_WALK_RX_GUARD 0`; the armed fold-1 object `f5627427…` pre-measured, REPORT lines 95-96) and states the single-define flip to ACTIVE is admissible only once the IGW verdict exists — R1/R2 (`W_WORKS`) vs R3/R4 (`W_FAILS_R_WORKS`); ABSENT/BOTH_FAIL/FATAL keep fold-0 (REPORT lines 13-16, 112, 122: "the IGW verdict … is PARKED"). Until the confirmed-row rebuild actually runs on console, that verdict stays missing and fold-0 remains the only fold; launch-10's exec-page risk stays decided by the F29 log evidence itself (worker-executedrain-trace conditions C1/C2), not by the probe.
