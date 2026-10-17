# REPORT — worker-launchday-update (rev 2026-10-05b)

Worker: worker-launchday-update · 2026-10-05 · local only · one controller file + this worker dir; nothing else touched.

## What was done

Updated `outputs/2026-10-04-zcode-controller/LAUNCH-DAY-10.md` to rev 2026-10-05b, integrating the
post-popup evidence layer that landed after the runbook was written. Five edits, nothing else changed:

1. **Changelog header** (new blockquote under the title): names rev b, its sources
   (`worker-post-popup-evidence/POST-POPUP.md` + `FAILURE-TAXONOMY.md`, `worker-mod-load-readiness/READINESS.md`,
   `worker-launch10-acceptance-criteria/ORACLE.md`), states that ORACLE verdict definitions are UNCHANGED
   (the post-popup layer only EXTENDS BEST's post-load evidence set), and carries the gate §1 pin note.
2. **§1 stage-header pin**: the old bullet pinned only `53b1188f` (anchor cap). Verified on this host that the
   on-disk staged `launch10-build/staging/live_module_facts.hpp` hashes to
   `67170b21fc3b00947d848fa94337c94c9b324155e27010259a192bfd32bedad5` — exactly EDITION.json edit #4 and the
   `launch10_one_pass.py:110 --stage-header` argument (kMaxAnchorMappings 8→16 + kMaxVmMappings 1024→16384).
   The bullet now requires BOTH `53b1188f` (anchor cap) AND `67170b21` (capture capacity) in the receipts /
   `--stage-header` args, and notes the pins live in `launch10-build/EDITION.json`.
3. **§2 USER INSTRUCTION**: one-liner extended (OPTIONS opens the F7 menu after loading a save; D-pad navigates;
   CROSS/X confirms spawn; report what appears) with the standing rule kept: no OPTIONS/spawn attempt until the
   `[Lua] [TownfallItemSpawner] Loaded` line is seen in UE4SS.log — mods-started evidence first
   (READINESS: the mods-started ack fires at engine-ready and does NOT prove main.lua ran; the `[Lua] Loaded`
   line is the only mod-executed proof). Full copy-paste walkthrough referenced at POST-POPUP.md §1.
4. **§4 VERDICT — new sub-table 4b "Post-load evidence"**:
   - First OPTIONS press after map load = free drain probe: ≥1 NEW `[TownfallItemSpawner]` line MUST appear;
     zero lines = keybind-unreachable or drain-stalled, discriminated by the init-era `[R3] refused:` /
     `[R3] EngineTick detour unavailable…` lines (POST-POPUP.md §3).
   - Navigation is mute by design — D-pad/page silence is not a stall; OPTIONS press #2+ is also mute by design;
     only the first press after a map load is the probe.
   - `Spawned <name> x<Q> ID=<id>` closes the spawn stage; visible spawn additionally needs the user's visual
     report/captured frame — per the oracle's G4 discipline, visible spawn is NOT a launch-10 verdict input.
   - No verdict definition (BEST/GOOD/MINIMAL-ACCEPT/FAIL/S-30S/NO-CRASH) was redefined or reordered.
5. **§5 IF-THEN — one appended row**: `Failed at <stage>. See UE4SS.log.` → the 16-payload spawn-failure taxonomy
   across the 5 stages (player / ground trace / load pickup class / spawn actor / configure pickup), read against
   `worker-post-popup-evidence/FAILURE-TAXONOMY.md` §C, with the ground-trace highest-risk note
   (`LineTraceSingle` out-param marshalling of `hit.ImpactPoint`/`.ImpactNormal` — retry elsewhere / flat ground
   before concluding an API gap; the mod never crashes the game, pcall-caught).

## Verification performed on this host

- Read all three source documents in full plus `FAILURE-TAXONOMY.md` before editing; every log line quoted in the
  runbook is verbatim from those sources (load line, drain-probe lines, `[R3]` discrimination lines, the
  call-time `ExecuteInGameThread` throw, `Spawned ... ID=` shape).
- Re-hashed `launch10-build/staging/live_module_facts.hpp` (67170b21fc3b… confirmed) and cross-checked
  `EDITION.json` edit #4 and `launch10_one_pass.py:110` before touching the §1 pin bullet.

## Limits / honesty notes

- `EDITION.json` on disk reads edition `launch10-final-a (fold: … POSITIVE FIXTURE PENDING before staging)`;
  the header quotes that on-disk form rather than inventing a `launch10-final` edition value. No build-exists
  claim is made anywhere.
- Payload count follows the task framing: 16 distinct error payloads across 5 spawn stages (taxonomy §C lists 17
  rows; the KismetSystemLibrary CDO payload appears under both C1 and C2).
- All rev-a content outside the five edits is preserved byte-for-byte (restore paths §6, gate §1 boxes other than
  the pin bullet, staging flow §1, §3 evidence commands, §4 verdict text, §5 existing rows).
- The post-popup rungs are G4-class evidence law, not outcome promises; verdicts remain controller declarations
  from real console evidence only.

## Deliverables

- `outputs/2026-10-04-zcode-controller/LAUNCH-DAY-10.md` (rev 2026-10-05b)
- `outputs/2026-10-04-zcode-team/worker-launchday-update/CLAIM.json`
- `outputs/2026-10-04-zcode-team/worker-launchday-update/REPORT.md` (this file)
