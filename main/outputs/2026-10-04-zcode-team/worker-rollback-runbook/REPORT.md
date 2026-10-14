# REPORT — worker-rollback-runbook, 2026-10-04

Deliverables (in required order): `CLAIM.json`, `ROLLBACK-RUNBOOK.md`, this
`REPORT.md`. Documentation only: no console contact, no network commands
against ps5-host-not-configured.invalid, no nested subagents; writes confined to this directory.

## What was read

- `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` — all 15 steps.
- Checkpoint `zcode_sdg2_staged` from
  `worker-composition-5-rebuild/transport-ledger.composition5-mirror.json` plus
  the surrounding composition-5/outage/crash entries. The named checkpoint
  `zcode_composition_5_gate_go_staged` does not exist in the local mirror
  (grep across the worktree finds it only in my CLAIM reference); its content
  is attested by CONSOLE-LOG step 15 (gate ALL 8 PASS → staged, readback
  verified) and `worker-composition5-gate/gate-receipt-composition5-r2.json`
  (verdict GO). The runbook says so explicitly instead of pretending to quote
  the missing entry.
- `outputs/ps5-mod-bridge/EXPERIMENTS.md` — present; policy: promotion-boundary
  review, cleanup/rollback paths required, never control the user's FTP
  service, no compile-success-as-runtime-claim.
- Pinned ploader source
  `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`
  in full (852 lines), including `parse_mode`, `read_ini`, `normalize_path`,
  `inject_prx`, `get_mode_params`, `run_plugins`, lock handling.
- Backup/staging precedents: `worker-final-staging-rehearsal/REHEARSAL-RESULT.md`
  (endpoints HTTP :8084 / FTP :2121, pre-image→bak→STOR→readback procedure,
  launch ritual), `worker-prx-packaging/STAGING-CHECKLIST.md` (ini shape),
  `worker-preflight/REPORT.md` (live ini bytes never recorded host-side).

## Key decisions and their evidence

1. **Disable mechanism (severity b): ploader.ini value edit 2→0, rename only
   as fallback.** From pinned main.c: `read_ini` skips any entry with
   `mode <= 0` (L672) — a `=0` entry is never registered, so ploader never
   PT_ATTACHes the (possibly unstable) game for it, no FAIL spam, no 120 s
   injector timeout. Mode 0 is SKIP, not inject-early (mode 1 is early,
   `get_mode_params` L471–476). A renamed/missing file takes the opposite
   path: the entry registers, ploader attaches + jacks ucreds, `access()` then
   fails → FAIL notification → polls to `INJECTOR_TIMEOUT_S` = 120 s. The ini
   edit is a single reversible byte and leaves the .prx untouched. Exact
   variant byte layouts and the mandatory pre-image RETR are in the runbook
   §4.3–4.4 (the live ini bytes are NOT host-provable — worker-preflight
   discrepancy is honored, not smoothed over).
2. **SHT-strip readback law** verified from artifacts, not just prose: sdg2
   upload fself 22087 B `0e5b36bc…` vs fresh readback 81368 B `182f5d3b…`
   (local files byte-compared: `/tmp/sdg2-final-backup.prx` ==
   `console/pathtest-sdg2-run/sdg2-readback.prx`). Documented artifact classes
   (e_shnum=0 @60, last-24B zeroed, 1560 B tail); plain files (ini, logs) read
   back byte-exact. Restore verification in §6 keys on the expected READBACK
   sha, never the upload sha — that is the false-alarm guard the task asked
   for.
3. **Severity ladder** (a) failure-but-game-fine → do nothing, collect once,
   next revision offline; (b) unstable after composition-5 → user closes game,
   ini disable per (1); (c) console crash/power loss → recovery ritual built
   from the three real 2026-10-04 precedents (`zcode_console_unreachable`,
   `zcode_console_crash_2`, `zcode_outage_recovery`: /data persisted twice,
   no dispatch into a half-booted console, no replay of the suspect payload);
   (d) full restore of composition-4 (original fself `de372302…` +
   readback-backup `d4fc0acf…` as expected-readback oracle) or sdg2
   (`0e5b36bc…` upload / `182f5d3b…` expected readback), with exact curl
   commands.
4. **Pre-flight** §7: exactly the 3 required checks (backup + shas recorded
   pre-overwrite; readback-law expectation written before upload; ini
   unchanged before/after), framed by the standing one-dispatch-per-
   verification discipline.

## Discrepancies found (recorded in the runbook, not resolved by invention)

- CONSOLE-LOG step 15 logs the staged composition-5 fself as 30786663 B; every
  local composition-5 candidate is 30786775 B (sha `04869ab3…` matches to full
  length for the candidates); no 30786663-byte local file exists. Runbook
  treats console RETR as ground truth and flags the 112 B delta for incident-
  time reconciliation.
- `zcode_composition_5_gate_go_staged` not present in the local ledger mirror
  (see above).
- Live ploader.ini full byte content unproven host-side (preflight found
  history staged ingame_facts as `=2`, never `=0`; task context says `=0`
  now). Runbook marks the 4-line layout as expected shape, RETR as authority.

## Limitations

No command in the runbook has been executed against the console by this
worker; the curl/FTP forms replicate the controller's previously used,
rehearsal-verified patterns. The composition-4 fallback restore (uploading the
bare stripped readback) is explicitly marked as loader-unproven, with the
fail-safe ploader FAIL path as the bounded worst case.
