# REPORT — worker-user-launch-guide

Worker: worker-user-launch-guide, 2026-10-04. Workspace root for all relative
paths: `/WORKSPACE/.codex/worktrees/translation-core/what`. No console
contact, no network, no nested subagents.

## Deliverable

- `USER-LAUNCH-GUIDE.md` — one-page, non-technical launch guide for the console
  owner, covering all seven requested sections: (1) what appears at launch
  (ploader popup, ~1-minute timing, what "OK" does and does not mean),
  (2) the game looking completely normal with no visible runtime changes,
  (3) what to do (play normally, OPTIONS/F7 opens the Item Spawner menu once a
  save is loaded), (4) success (the byte-exact
  `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
  line in the log the controller collects, plus the menu opening on OPTIONS),
  (5) failure (loader FAIL, game not starting, crash, frozen console, or missing
  log line), (6) what to do on failure (close the game, record what was seen,
  tell the controller, no retries; console restart is user-controlled and
  reported back first), (7) timing (~1 minute injection window, then normal
  gameplay, nothing time-sensitive after that).
- `CLAIM.json` — written first, per order rules.

## Sources

All content is translated from the four assigned documents, nothing else:

- worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md — launch sequence §5
  (game fully closed, ploader armed, user launches, injection reported ~1 minute
  in, one launch per staged build), evidence channels §6, directory layout §2.
- worker-acceptance-evidence/EVIDENCE-SPEC.md — launch ritual §0.2 (user launch,
  never automated, no blind relaunch), rung 4 load line, rung 5 pad mapping
  (OPTIONS = F7, CROSS = confirm), negative-evidence discipline.
- worker-itemspawner-bundle/FIRST-RUN-CHECKLIST.md — byte-exact phase-1 load
  line, phase-2 no-save behavior ("Item database not ready. Load a save first."
  is designed, not a fault; menu may not appear without a save), failure
  discrimination (no records = check the ploader popup, do not blind-retry).
- worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md — no repeated relaunches,
  records-not-popups are truth (softened for the user to "controller checks the
  logs"), crash recovery (stop contact, user controls power/restart, report back
  before anything else).

## Honesty notes

- The guide asserts nothing has run on the console; it only tells the owner what
  WOULD happen and look like. The success log line is quoted byte-exactly from
  the pinned mod source path (IS:746) as recorded by worker-itemspawner-bundle.
- Deliberately omitted as developer-only: PRX/ini/FTP/hashes, ploader mode
  numbers, rung/ladder terminology, runtime-status states, log file paths. The
  one retained term is "ploader" (the owner already knows the Payload Manager
  surface) plus the F7 = OPTIONS equivalence, which the notes use.
- The "OK ≠ everything works" caveat is included because CONSOLE-SAFETY-PROCEDURE
  R-PLD-01 forbids treating a loader OK as proof the module ran; phrased
  non-technically as "the controller checks the hidden log files afterwards".
- The no-save F7 case is presented as expected-not-fault per the checklist's
  phase-2 design (the discriminating line goes to the log, screen stays silent).

## Bounds

- Tool calls used: 9 of 20 (1 locate, 4 source reads, 1 format reference,
  3 writes).
- Writes confined to `outputs/2026-10-04-zcode-team/worker-user-launch-guide/`:
  CLAIM.json, USER-LAUNCH-GUIDE.md, REPORT.md. No other files touched.
