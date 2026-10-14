# DIFF — worker-evidence-gap-fix-2 (2026-10-05, LOCAL ONLY)

File: `outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh`
sha256 before: `76559126074c28dad936905a8d9c2c3c3d7fffedc47907cc7f0f356616ae858a` (snapshot: `evidence.sh.pre-patch`)
sha256 after:  `7b03ca9503a2cd229d391e3f116d27e308b9de39124ee438711319554412d7ee`
Machine diff: `evidence.sh.diff` (347 lines) in this directory.

Fixes the nine REAL-DATA gaps from `worker-verdict-first-real/LAUNCH10-RESULTS.md` §5.
The three real runs (`worker-verdict-first-real/fixtures/run{1,2,3}`, documented sdiag
placement rename) are the regression corpus and were rerun after the patch.

## G1 — verdict-name fidelity (HIGH)

Observed bug: the pad/loadlist refusals fell through every `map_detail` pattern, so the
real runs produced STAGEHIT "failure line without a pinned detail string" and verdicts
`FAILED_code3` / `FAILED_code-8`, while the oracle classes them E2/B7 (EXPECTED-REFUSAL;
E2 ⇒ verdict ≥ MINIMAL-ACCEPT).

Patch (three layers):

1. `map_detail` arms added (classify_stream):
   - `mfa_activate_failed` → stage `S2_ACTIVATION`, treason = "B7 EXPECTED-REFUSAL — MFA
     activation refused (Phase B; …); NOTHING downstream ran"
   - `runtime_start_failed code=3` → stage `S9_START_REFUSED` (new display token), treason =
     "E2 EXPECTED-REFUSAL — START-stage refusal (post-conversion…)". The misleading
     `S9_START_ACK` staging of refusal lines is gone (it remains only for genuinely
     unpinned non-mfa/non-code3 failure lines).
2. Verdict mapping after the existing pinned-detail `case` (which keeps absolute
   precedence): if no pinned arm matched —
   - `LASTF_STAGE = mfa_activate_failed` → `VERDICT: FAILED_B7_MFA`
   - `CODE = 3 && LASTF_STAGE = runtime_start_failed` → `VERDICT: FAILED_E2_START`
   - else bare `FAILED_code<N>` fallback (unchanged).
3. `MINIMAL_ACCEPT_E2_REFUSAL` state: upgrades `FAILED_E2_START` only when ALL hold:
   - current session's sdiag conversion quartet: `extent_events=4` AND `anchor_count>0`
     AND `intervals_found>0` AND `cfg_events>0` (oracle §2 clause 1, session-scoped per G2)
   - `n_d6 = 0` — no D6 geometry-refusal line anywhere in the window (clause 2)
   - `n_crash_tail = 0` (clause 3 NO-CRASH)
   - `fresh=yes` AND `last_state=failed`
   The E4 `activated` soft point is handled honestly: ORACLE-CLASS + NEXT print the
   terminal-write O_TRUNC caveat and the evidence-equivalence basis
   (LAUNCH10-RESULTS §2), plus "Status detail matches the refusal line verbatim." when
   it does. No status capture at all (run1) ⇒ no upgrade; verdict stays
   `FAILED_E2_START` with "quartet=yes, fresh=unknown … rung claim reserved" in NEXT.

New output lines: `ORACLE-CLASS:` under VERDICT (one-pager) and `refusal_class` /
`oracle_class` in the JSON tail.

## G2 — sdiag per-pid/session split (HIGH)

Observed bug: run3's SDIAGSUM aggregated 1591 events across pids 317/341/355;
`pid_first=317` was stale; `start_failed=2` mixed two sessions; the STIME sequence
crossed session boundaries (+239,970 ms jumps) under a "single-writer" label;
last-write-wins value tags were only correct by luck.

Patch (`classify_sdiag` rewritten, per-pid state):
- `SDIAGSUM` = file-total line: `events=… sessions=… pid_first=… pid_last=…
  baseline_cut=… start_ok=… start_failed=… <value tags = LAST session's> (file totals;
  value tags = last session pid=…; per-session lines follow)`
- one `PIDSUM` line per session (tab-separated, fixed field order):
  `pid, events, t_first, t_last, start_ok, start_failed, mfa_activate_failed (int32
  rendering: 4294967288(int32 -8)), runtime_start_failed, extent_events (0-4),
  cfg_events, lease_spans, lease_reg, anchor_count, intervals_found, vmentry,
  vm_walk_stop, exepath_source, cfg_gamedir, spanpub_count, spanpub_total, spanpub_cap,
  env_low, env_high (value_hex), stat_ownmod, stat_ownmod_ec, module_start_return`
- one `PIDCHAIN` line per session (first-seen stage chain, session-scoped) and one
  `PIDJSON` line (JSON record consumed by the tail)
- STIME deltas: computed only when `prev_pid == pid`; a session switch prints
  `SDIAGSESS` (displayed as `-- session pid=N (delta base resets) --`) and the next
  event reads `(first t_ms)`. Never cross-session.
- optional append-only cut: `--baseline-sdiag-bytes N` or a
  `modbridge-supplier-diag-baseline.txt` ref file in `--baseline-dir`
  (launch-10 pinned NO sdiag baseline ⇒ default OFF; honest `baseline_cut=` label).
- one-pager: PIDSUM lines under the SDIAGSUM line; per-session chains under the
  file-level chain when `sessions>1`, current session marked; the "single-writer"
  framing replaced by "per-SESSION deltas — deltas NEVER cross sessions/pids".

Real-run result: run3 → sessions=3; pid 341 shows `mfa_activate_failed=-8`,
no bridge-side values, chain `S1 -> S2 -> S11` (B7's "nothing downstream ran" is now
visible per session); pid 355 (current) carries the quartet 7/1/6 + vmentry=10028.

## G3 — JSON tail carries sdiag evidence (HIGH)

Added keys (all old keys untouched): `refusal_class`, `oracle_class`,
`sdiag {present, events, sessions, pid_first, pid_last, current_pid, chain,
start_failed_total, baseline_cut, quartet, current{…18 value fields incl. the predicted
pair}}`, `sessions[ …PIDJSON records… ]`, `vm_capacity {pinned_kBridgeVmmapEntries:
16384, current_vmentry, current_vm_walk_stop, exceeds_pinned}`.
A JSON-only consumer of run3 now sees `quartet=yes`, `anchor_count=7`,
`intervals_found=1`, `lease_spans=6` instead of "no staged markers".

## G7 — vm_total vs capacity (note only)

Current-session `bridge_vmentry_entries > 16384` ⇒ diag-note with walk_stop, the C1/S3
prediction, and the three candidate explanations; "note field, no verdict impact".
Fires on run1 (22761 > 16384, stop=0); silent on run3 (10028). JSON `vm_capacity`
mirrors it (`exceeds_pinned` boolean).

## G4 — S11_STOP display quirk (note)

`n_fail>0 && s_modret=0 && CURCHAIN contains S11_STOP` ⇒ note: module_start_return=0
AFTER the refusal = entry returned 0, game continues UNMODDED; not a stop/unload event.
Fires on run1/run3 (+ rehearsal set D, same honest condition); absent on run2/A/B/C/E.

## G5 — fixture-name coupling (alias)

`--sdiag FILE` flag (from-dir mode): explicit supplier-diag path when the fixture is
not named `modbridge-supplier-diag.jsonl`. Verified with run1's sdiag under its original
documented name `run1-sdiag.jsonl`.

## G6 — cumulative UE4SS.log windows (scoped note)

`failure lines by session (r4 pid)` grouping + window note: cumulative across the boot's
launches; PATH_TEST/ACTIVATION_TEST stamps carry no pid (positional attribution only);
verdict precedence = LAST failure line (append-only ordering discipline). Full
per-session windowing of the log text is NOT attempted (r4 prefixes exist only on
failure lines) — reported as the scoped cheap part only.

## G8 — oracle C7 nuance (note only)

Current session `exepath_source=0` WITH `stat_ownmod=1` ⇒ diag-note: the r10 fork
refuses only when ALL THREE stats fail; oracle C7 wording looser than live behavior;
wording update at next oracle revision. Fires on run1/run3.

## G9 — spanpub env shape + saturation (note only)

`spanpub_cap=1` ⇒ diag-note: env pair is the 64-bit low/high split (hex forms printed);
count/total with saturation at kMaxPublishedSpans=16 (both real boots). run1:
`0x409d0000`/16, 16/119; run3: `0x00410000`/17, 16/127.

## Deliberately NOT done (reported per instructions)

- E1 (unpinned code=2) verdict naming: not an observed class in G1's scope; pinned
  code=2 details already map. Left for the oracle owner.
- Per-session UE4SS.log windowing beyond the pid grouping note (G6) — not cheap; the
  failure-line r4-pid grouping + positional caveat is the scoped fix.
- STIME display cap stays 24 (prior GAP-5 decision kept).
- STAGE-TABLE.md not edited (outside owned scope); new display token `S9_START_REFUSED`
  and the ORACLE-CLASS line are documented here and in the script header.
