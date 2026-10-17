# agent-EN -- fast-cycle launcher (Claude directives 2/3/4)

- generated (UTC): 2026-10-09T16:02Z
- deliverable: `harvest/fast_cycle.sh` (25,244 bytes, executable,
  `bash -n` clean; the ONLY file written by this agent besides this report)
- mission: collapse the per-launch command storm (~20 min cycle -> ~5 min
  target) into ONE command per cycle phase, REUSING the verified tools in
  this directory verbatim (prep_modded_launch.py / harvest.py /
  bind_launch.sh command shapes). No upload/verify/analysis logic was
  reimplemented anywhere in fast_cycle.sh.

## Directive mapping

| directive | mode | what runs |
|---|---|---|
| 2 (one command per cycle) | `prep`, `watch`, `verdict`, `next` | see usage below |
| 3 (verdict first) | `verdict` | quick single UE4SS.log pull + `harvest.py --local-ue4ss --skip-klog` (its own verified analysis) prints the compact `[harvest] verdict:` line + `key: db=<..> menu=<..> spawn=<..>`; the FULL `harvest.py --label auto-<ts>` run (all pulls + klog pickup + `HARVEST-auto-<ts>.md` write) is backgrounded via nohup |
| 4 (kick next build early) | `next` | CHANGES files[] row check (bind_launch preflight gate-2 mirror: profile/typescan-r4/thread, candidate+original sha256 vs disk); clean -> nohup archive+build+package chain (exact bind_launch steps 1-3 shapes + PRX magic 4f153d1d/5414f5ee + not-byte-identical-to-5ea2c845 checks), final line `NEXT PRX READY: <path>`; stale -> prints exactly the stale rows and does NOTHING else. Zero console contact. |

## Usage one-liners

    fast_cycle.sh prep --prx <packaged.prx> [--variant A|B] [--console-ip IP] [--skip-klog]
    fast_cycle.sh watch [--console-ip IP] [--source FILE] [--interval 5]
    fast_cycle.sh verdict [--console-ip IP] [--source FILE]
    fast_cycle.sh next [--dry-check]

Cycle shape: `next` (build kicked in background right after a run's verdict
is known) -> when its log shows `NEXT PRX READY:` -> `prep --prx <path>`
(console gates, upload+decrypt-off readback, settings, ploader armed LAST
via prep_modded_launch.py step order, klog tap) -> banner `=== LAUNCH NOW ===`
-> `watch` in a second terminal during the run -> `verdict` right after
clean exit -> repeat.

## Design decisions (all reuse, no reimplementation)

- prep: invokes prep_modded_launch.py verbatim (variant A default, matching
  bind_launch step 4). All safety laws live INSIDE it and are inherited
  unchanged: eboot-must-not-be-running (step 1), ploader-lock residency so
  the loadpayload dispatch is never sent twice (step 1 + one-shot step 6),
  decrypt-OFF (`-Q 'SITE SELF'`) readback with full sha256 match and the
  one-verified-restore attempt (step 3), MFA size-exact settings law
  (step 4). ploader is armed LAST among console writes (prep step order:
  uploads at 3/4/5 -> arm at 6 -> local-only taps at 7). fast_cycle adds
  only: banner gating (rc 0 AND a `LAUNCH-READY-*` receipt must exist,
  bind_launch step-4 rule), failing-gate extraction from prep's own
  `[prep] ABORT:` line, and a per-step remediation one-liner table.
- watch: bind_launch `--watch` pattern, hardened. Differences, both proven
  necessary by the fixtures:
  1. NEW matches only: bind_launch's `awk '!seen[$0]++'` restarts awk every
     poll, so it reprints the same lines every 5 s; fast_cycle tracks a
     byte offset (each byte range printed exactly once) and rebaselines
     safely if the log shrinks/rotates.
  2. NUL normalization: the console's wide-char Lua writer emits NUL-
     interleaved lines (harvest.py zz rule; verified in /tmp/bind1.log where
     RAW grep finds ZERO marker lines); fast_cycle greps both the raw fetch
     and a `tr -d '\000'` copy, so wide-char `Menu created`/`Spawned` lines
     are caught.
  Marker set per the task directive: `[FN] tostring-bind`, `[FFO]`,
  `db=[vnie]`, `Menu created`, `Spawned `, `seed_refused`, `Scan failed`.
  `*** RUNG HIT ***` fires the moment a NEW `Menu created` or `Spawned `
  line appears. `*** runtime live ***` fires on each NEW UE4SS session
  banner (harvest.py UE4SS_BANNER_RE shape) = injection worked.
- verdict: two-phase, both phases are harvest.py's own code. Quick phase =
  one read-only FTP fetch of /data/plugins/UE4SS.log + harvest.py
  `--local-ue4ss --skip-klog` (analysis-only regression mode, no writes);
  the verdict line + db/menu/spawn are extracted from its stdout (grep -a
  is required: the report carries NUL bytes from raw log lines). Full phase
  = the canonical `harvest.py --label auto-<ts>` live run backgrounded
  (nohup log under /tmp/fast-cycle/; report lands as
  `harvest/HARVEST-auto-<ts>.md`).
- next: double-kick guard (`/tmp/fast-cycle/next.pid`, live-pid check;
  stale pid passes -- verified) so two chains can never interleave the
  archive moves. The generated runner mirrors bind_launch steps 1-3
  exactly (archive -> `build_variants.py --execute` cwd=what/ -> receipts
  -> `l26_packaging_driver.py --execute` -> FINAL-RECEIPT + PRX magic +
  not-5ea2c845 checks) and ends with `NEXT PRX READY:`.

## Offline test results (all on this Mac, zero console contact)

| test | input | result |
|---|---|---|
| syntax / usage | `bash -n`; no-arg run | clean; usage exit 2 path exercised |
| watch static | /tmp/bind1.log (fixture; RAW log has 0 grep-able markers -- all marker lines are NUL-interleaved or absent) | baseline `108216 bytes, 1 banner(s) (not replayed)`; ZERO marker spam over 7 s; clean SIGINT stop |
| watch growing | copy of bind1.log + appended banner, zz `T1 db=v pc=v`, `[FFO]`, NUL-interleaved `Menu created`, `Spawned Antibiotics x1 ID=...`, `seed_refused`, noise lines | `*** runtime live *** (banner #2)`; each marker printed exactly once, in order, timestamped; NUL-interleaved `Menu created` caught; `*** RUNG HIT ***` on BOTH `Menu created` and `Spawned `; noise line filtered; no repeats across polls |
| watch shrink | truncate mid-run, then append | `(log shrank: ... rebaselined, existing content not replayed)`; post-shrink new marker still caught; no false replay |
| watch unreachable | --console-ip 127.0.0.1 | `baseline: source not fetchable yet; will keep trying`; no crash |
| verdict negative | /tmp/l25-B-live1-ue4ss.log (copy of harvest-l25-B-live1-ue4ss.log) | `verdict: diag-zz=ABSENT database-live=n/a auto-probe-cycles=0 acceptance=NOT-UP`; `key: db=n/a menu=NO spawn=NO`; full analysis backgrounded, completed, wrote nothing (local mode) |
| verdict positive | synthesized log = live1 + `Z0 v8`, `T1 db=v pc=v`, `Menu created.`, `Spawned ...` | `diag-zz=PRESENT:DB-LIVE database-live=YES auto-probe-cycles=1`; `key: db=YES menu=YES spawn=YES` (acceptance honestly NOT-UP: rung 1 `spawnable pickups found` absent) |
| verdict quick-pull failure | unreachable IP (code path, not left running) | clean `VERDICT QUICK-PHASE FAILED` + remediation, no full harvest launched |
| prep failure | --console-ip 127.0.0.1 + dummy file | prep's own STEP 1 abort surfaced verbatim; `FAILING GATE: STEP 1 ...`; receipt path printed; STEP 1 remediation one-liner; exit 1; NO banner |
| prep bad --prx | nonexistent path | local gate + remediation, exit 1, no subprocess |
| next --dry-check | real tree | `rows clean: 34 files[] rows (candidate+original) match disk` + the exact 5-step would-run plan; exit 0; nothing launched |
| next stale rows | fabricated tree (MISSING original + STALE candidate) against the SAME embedded python, extracted verbatim from the script | `STALE 2` + both `STALE-ROW` lines (the exact strings mode_next prints before exiting 4); real-tree control `OK 34` |
| next runner generation | sandboxed copy (nohup line neutralized) | runner generated, `bash -n` clean, inspected: paths expanded at generation, runtime vars escaped, bind_launch step 1-3 + magic/baseline checks intact |
| next double-kick guard | pidfile with a LIVE pid | `a build chain is ALREADY running (pid N) -- refusing a double kick`, exit 4, no runner created; stale-pid (dead) correctly passes |

## INCIDENT (full disclosure) -- accidental live `next` launch during guard testing, killed and fully restored

- 15:59:00Z: my first double-kick-guard test wrote pid 999999 into the
  pidfile. That pid does not exist, so `kill -0` failed and the guard
  (correctly, by design -- stale pidfiles must not wedge `next`) passed,
  and the REAL archive+build chain launched. This was a test-setup error:
  the task authorized `next` testing in dry-check mode only.
- Stopped within seconds: runner bash processes pkill'd; the orphaned
  `build_variants.py --execute` child (pid 4088) killed explicitly. The
  driver was still in its G2 gate/.d-regeneration phase -- it had NOT yet
  created any new l26-A/l26-B output.
- Restored byte-for-byte: `l26-A`, `l26-B`, `staging-copyin-probe`,
  `2026-10-08-l26-package-A`, `2026-10-08-l26-package-B` moved back from
  `archive-20261009T155900Z` (archive dir removed empty); the pre-existing
  `archive-20261009T151128Z` (not mine) untouched.
- Post-restore verification: 68 row sha checks (34 rows x candidate+original)
  all clean; TWIN-RECEIPT A/B + FINAL-RECEIPT-VARIANT-A.json present;
  package-A PRX sha256 `10b11e437fbe9808d3db1a77019489d791edabf76fc38ef5a3d3a4d6adbdad8a`
  unchanged; no leftover build processes. The two LAUNCH-PREP-FAILED
  receipts my prep-failure tests created (bogus-IP offline attempts, not
  real console attempts) were removed so the receipt provenance in
  harvest/ stays real; the genuine pre-existing
  LAUNCH-PREP-FAILED-A-20261009T040918Z.md remains.
- Leftover evidence of the incident intentionally kept in /tmp scratch:
  /tmp/fast-cycle/next-20261009T155900Z.log (the killed run's gate log).

## Not testable offline (honest gaps)

- prep SUCCESS path (banner) requires a reachable console with all gates
  passing; only its failure paths and receipt-name gating were exercised.
  The success path is prep_modded_launch.py's already console-proven flow
  (LAUNCH-READY-A receipts exist from 2026-10-09 runs) plus printing.
- verdict live mode (real FTP quick-pull + full harvest backgrounded) ran
  only in --source mode; the live commands are harvest.py's own.
- The `next` chain was never allowed to complete (by design, per task);
  its commands are bind_launch's proven step 1-3 shapes, and the artifact
  checks were verified by inspection against bind_launch step 3.

## Cycle-time accounting (directive 2 target)

- prep: one command (~2-4 min, dominated by the verified PRX
  upload+readback that CANNOT be safely skipped).
- watch: runs concurrently with the user sequence (zero added wall time).
- verdict: verdict line + 3 key fields in ~5-10 s (one FTP fetch + local
  analysis); full report writes itself in the background.
- next: fired right after a run's outcome is known; build+package
  (~10-15 min) overlaps the human/verdict phase, so the NEXT prep can
  start immediately after the current cycle's verdict + clean exit +
  reboot discipline. Sequential human-facing time per cycle drops to
  roughly: prep wait + launch + menu/save/pad sequence + verdict -- the
  ~5 min target, with no safety gate removed.
