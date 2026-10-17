# REPORT.md — worker-8fam-console-prep

Worker: worker-8fam-console-prep (ZCode team, 2026-10-04). ~15 tool calls of
40 used. NO console contact, no target/PS5 execution, no nested subagents,
no network to the console; writes only under
`outputs/2026-10-04-zcode-team/worker-8fam-console-prep/`. Read-only inputs:
worker-dll-8fam-combined (candidate + delta doc + REPORT), worker-static-tls-7fam
(7-family ELF + receipt + plan/ledger), worker-combined-dll-allfam
(console-passed 6-family ELF), worker-console-batch-runner (order note —
not edited, out of scope).

Status: **EIGHT_FAMILY_CONSOLE_STAGE_PREPARED** — artifact verified against
receipt and BOTH prior ELFs; zero-decision stage plan frozen; no dispatch
authorized or attempted (console runs PENDING user).

## Deliverables (in write order)

1. `CLAIM.json` — bounds, verified claims, non-goals; written first.
2. `VERIFICATION.md` — raw evidence: re-hash output, the three 55-symbol
   `nm -u` sets and pairwise diffs, host-oracle record comparison (7fam vs
   8fam host-normal.jsonl), the 8 SEH64 consumer symbols from the def files.
3. `CONSOLE-STAGE-PLAN-8FAM.md` — the frozen stage plan (identity table,
   pre-flight, prerequisites, upload/check/readback/dispatch/readback steps,
   expected deltas vs BOTH prior runs, failure triage incl. -130..-137,
   sequencing recommendation).
4. `REPORT.md` — this file, last.

## Task 1 — Artifact verification (all exact, this session)

- Re-hash: sha256 `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb`,
  336552 bytes — EXACT match to the receipt
  (`worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/receipt.json`:
  `artifact.sha256`/`artifact.bytes`), `file(1)` = ELF 64-bit LSB pie
  executable x86-64 FreeBSD, build-id 0224ee94363709e5114bb133231aa4a7.
- Receipt sanity: build_id `c3aba79f968a492b849e5ece2f009e8e`, report path
  `/data/modbridge-owned-dll-candidate.jsonl` with
  `target_path_opened_by_build: false`, startup "Genuine SDK payload crt1
  _start, not a PRX module entry", status
  `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`, `console_contact`/
  `target_execution`/`prx_created`/`downloaded_dll_executed` all false.
- Undefined-import set diff (`nm -u`, sorted, unique):
  - 6-family console-PASSED ELF (133c5e9e, 300456 B): 55 symbols
  - 7-family ELF (e72f5318, 334656 B): 55 symbols
  - 8-family ELF (c3aba79f, 336552 B): 55 symbols
  - diff 6fam vs 8fam: **IDENTICAL**; diff 7fam vs 8fam: **IDENTICAL** —
    the expected 55-set with delta ZERO confirmed against BOTH priors, so
    import resolution on the SDK side is pre-proven by the ALLFAM_DLL_TEST
    console pass.

## Task 2 — CONSOLE-STAGE-PLAN-8FAM.md

Frozen alias **`ALLFAM8_DLL_TEST.elf`** (no prior alias touched), payload
law explicit: target readback sha MUST equal the upload sha
`79281191…` EXACTLY and 336552 bytes, else STOP and leave staged. Single
dispatch, never re-send on ambiguity. JSONL is append-only.

Key contents and the assumptions checked behind them:

- **JSONL positions**: the 7fam stage-prep re-count established the file
  holds **36 records** (correcting the stale 6-family-plan "20 records"
  figure). The plan states: with the recommended order (8fam first) this
  stage appends at **37–40**; if 7fam runs first, 8fam appends at **41–44**.
  Hard invariant overrides position: "appended after ALL existing records,
  none modified".
- **Expected deltas vs BOTH priors** (pass-oracle table): `seh64`
  `{enabled:1}` from seq1 (the 9th counted family key; full enabled set is
  12 keys — enumerated from the host record itself); consumer_imports
  78 (6fam) -> 82 (7fam) -> **90**, with the 8 named symbols
  source-verified in `tests/worker-seh64-kernel.def` (7 × KERNEL32) and
  `worker-seh64-vcruntime.def` (`__C_specific_handler`); win64cc IR +7
  (`mb_win_seh64_abi: 7` union entry); helper_imports 10; results [86,93],
  worker 4/4, entry [7,7]/[0,0], cleanup retired — all unchanged.
- **memory.last correction (load-bearing)**: the task assumption
  "memory.last unchanged from 7fam" is **FALSE** — EXPECTED-DELTA-8FAM.md
  does not mention the field, but the host oracle measured
  `memory.last.length` **114688** (seq2/phase3) and **163840**
  (seq3/phase5) vs 7fam's 98304/147456 (6fam final 98304). The plan carries
  the host-measured values (the 6-family console run proved this field
  matches host on target) and flags the delta-doc omission explicitly, so a
  correct console run is not misread as a failure.
- **Failure triage**: -130..-137 mapped to the source-verified assertions in
  `tests/worker-seh64.h` (probe/lookup/virtual-unwind; same-frame raise
  transfer + handle-once; guarded restore; cross-frame `__finally`/no-resume;
  dynamic register over a bridge address; removal/double-removal/filter
  round-trip), plus the inherited 6/7-family ranges, the stage-55/56
  dual-owner (static TLS then SEH64) phase-2 note, an import-count
  discriminator (88/89 → which SEH64 binds landed; KERNEL32 73 /
  VCRUNTIME140 1 host split), a stale-artifact discriminator (82/no-seh64 →
  an ALLFAM7 payload under the new alias; 78 → ALLFAM), and the console-only
  `stdout_error 32` do-not-misread note.

## Task 3 — Sequencing recommendation

**Prefer the 8-family run as the single DLL payload of the user's return
window; keep the 7-family stage frozen as fallback attribution; do not
default to running both.** Rationale in the plan: superset proof in one
upload/check/dispatch/readback budget; attribution stays clean because the
never-dispatched SEH64 surface is narrow (dir-3 EXCEPTION admission, .ehflags
unwind tables, 8 consumer binds, -130..-137); explicit fallback rule — if
the 8-family run trips a SEH64-specific surface (-130..-137, stage-55/56
phase-2 stop, imports 88/89) and window budget remains, run ALLFAM7_DLL_TEST
afterwards (then at positions 41–44) to separate "SEH64 graft broke it" from
a shared-surface regression. Payload-only first is kept: the batch-runner
session order becomes **dll8 -> e2e -> stage-qdrv1 -> game-evidence**
(the runner's `all` order currently names dll7; that file is
worker-console-batch-runner's scope and was NOT edited — the substitution
is recorded here and in the plan). UE4SS composition-5 + qdrv1 stays the
single game-launch session LAST, never interleaved with payload dispatch.

## Honest limits

- Nothing console-proven by this worker; no dispatch attempted; the
  candidate remains compile-only + host-validated
  (`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`).
- Expected deltas are host-oracle-based; the 6-family console pass proved
  the record-shape fields match host on target, but `seh64` values,
  consumer_imports 90, and memory.last 114688/163840 are until-dispatch
  expectations, not console evidence.
- EXPECTED-DELTA-8FAM.md's silence on memory.last was found in verification;
  this report and the plan carry the corrected host-measured values. The
  delta doc itself was not edited (out of write scope).
- batch-runner `console_batch.py` still lists dll7 in its `all` order; the
  dll8 substitution is recommended here but must be applied by that worker's
  owner if the runner script itself is to drive the window.
