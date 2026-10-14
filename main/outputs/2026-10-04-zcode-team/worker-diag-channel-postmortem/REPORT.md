# REPORT — worker-diag-channel-postmortem (FINISH pass, 2026-10-04)

Final state of `outputs/2026-10-04-zcode-team/worker-diag-channel-postmortem/`.
Host-only work; no console contact, no nested subagents, no writes outside this
directory.

## What was intact from attempt 1 (kept, not redone)

- `CLAIM.json` — complete channel trace + -22 analysis claims (revised this
  pass; see below).
- `host-test/` design assets — `module_entry_patched.c/.diff`, `apply_patch.py`
  (5-edit fail-open patch spec for `lifecycle/module_entry.c`), shims, test
  scenarios.
- `receipts/host-test-output.txt` — the attempt-1 FAILED receipt, preserved
  unchanged as raw provenance.

## What had died mid-work, and the repair

Attempt 1's host test reported HOST-TEST-FAIL on every failover-line check.
Root cause found this pass: `test_main.c:set_paths` read the log paths from
`argv[1]`/`argv[2]`, but `argv[1]` is the SCENARIO NAME — the failover writes
landed in scenario-named relative files (`t1_channel_open_failed`, …) that
accumulate across runs, so "exactly one line" could never pass. The code under
test was correct the whole time (the stray files contained the exact expected
lines, e.g. `modbridge-entry-failover/v1 channel_open_failed rc=0xfffffffe`).
Repair: one indexed fix in `set_paths` (documented in a comment), stray
pollution files deleted, rerun → **HOST-TEST-PASS, 6 scenarios, 26/26 checks**
(`receipts/host-test-pass-patched-design.txt`).

## Task pivot honored: audit instead of design

The wired entry now exists
(`../worker-module-start-wiring/runtime_module_entry.c`, sha256
`1c041f8b…`), so the host test was extended to audit THAT implementation:

- `host-test/audit/` — byte-exact staged copy (hash-checked every run) compiled
  against fakes (kernel/diagnostic/probe-lifecycle/mfa/bridge).
- **AUDIT-TEST-PASS, 7 scenarios (a1-a7), 33/33 checks**
  (`receipts/audit-test-pass-delivered-entry.txt`).

### Audit finding (the headline)

**The delivered entry's fail-open markers cover ONLY post-lifecycle failures.
The -20/-21/-22 early returns are still silent — including the -22 diagnostic
channel-open failure.** `write_ue4ss_log_line` (:166-192) and
`write_status_failed` (:198-219) are reached only via `runtime_failure`
call sites :266 (mfa_activate_failed), :289 (runtime_start_failed), :346
(runtime_stop_failed). The -22 silence is by OMISSION, not incapacity:
write_ue4ss_log_line's own guard only needs `diagnostic_ready()`, which is true
in the channel-open-failed sub-case (discriminating host scenario a1).

### The exact 3-line patch (proposal; nothing outside this dir changed)

Replacing `runtime_module_entry.c:303-304` with the 3 lines in
`CHANNEL-POSTMORTEM.md` section 4 closes the gap with unchanged return
semantics — host-proven by `host-test/audit/patched_proof_runtime_module_entry.c`
+ `run_patched_proof.sh`: **PATCHED-PROOF-PASS, 8/8**
(`receipts/patched-proof-pass.txt`; observed marker
`modbridge module_start failed: diag_channel_open_failed code=-22 detail=…`).
Analogous one-call additions for -20/-21 are noted but not proof-run.

## Deliverables

| file | state |
|---|---|
| `CLAIM.json` | revised to final state (audit + patch-proof + repaired-design claims; attempt-1 claims preserved) |
| `CHANNEL-POSTMORTEM.md` | new — channel write path with file:line cites, why composition-4's -22 was silent, delivered-entry audit table, 3-line patch, receipts |
| `receipts/` | attempt-1 FAIL (raw) + 3 new PASS receipts (design 26/26, audit 33/33, proof 8/8) |
| `REPORT.md` | this file |

## Not claimed

No console execution or in-game behavior claims; no claim the patch was applied
to the delivered entry (it was not — proposal with host proof only); module_stop's
silent -20 and the constructor-failure channel-only path are documented as
out-of-scope observations; the -22-silence vs unwired-caller question for
composition-4's root cause remains unfalsified on console, unchanged from
attempt 1.
