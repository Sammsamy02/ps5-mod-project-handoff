# GO/NO-GO — per-launch checklists (wake-time gates, WAKE-RUNBOOK §10 law)

Agent GQ (ZCode), 2026-10-10. Every box must be PASS (or a documented
N/A) before the user is handed the controller. Sources: WAKE-RUNBOOK
§1/§10 (power-loss wake sequence), GB CONSOLE-TEST-PLAN, GC LADDER-V2
§4.0, LAUNCH-READY receipt format. `grep -a` always (mixed encodings).

## 0. Common wake gates (both launches)

- [ ] Console booted through the normal jailbreak entry (user-driven);
      daemon ladder up via autoload.txt (kstuff-lite, nanoDNS, ftpsrv,
      elfldr, ps5upload, OnionHEN, klogsrv, ploader arms per cycle).
- [ ] Daemon census via `http://$CONSOLE:8084/processes_list`: kstuff /
      pldmgr / ftpsrv / klogsrv all present.
- [ ] klogsrv dead? Restart ONCE via
      `loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fklogsrv-ps5.elf` (never
      double-dispatch); re-census >= 1.
- [ ] NO game running: `processes_list` eboot count == 0.
- [ ] ploader-resident wait (Z2 law): if a `payload.elf` pid is present,
      WAIT <= 120 s for its exit (no double loadpayload, no launch).
- [ ] klog tap started into /tmp/l25-variant-A/ with a NEW file per
      launch, BEFORE the launch (klogsrv has no history replay).
- [ ] No prior-session crash un-harvested: if the last session crashed,
      it was harvested + the console was REBOOTED before now.

## 1. LAUNCH 1 — round-4 spawn + zzdiag-v11 + ueselftest

PRX gates:
- [ ] Round-4 PRX BUILD RECEIPT exists (AC-power build; contents LOCKED:
      round-3 tree + FR parity revert + FW routed sites + literal
      markers; MODBRIDGE_RELEASE_DIAG_OFF stays OFF; FX patches stay
      OUT) — `<round4-prx>` resolved to a real path. NO round-3
      56a9b1a0 (fallback only if round-4 fails a gate).
- [ ] `verify_build.sh <round4-prx> --manifest $PREP/harvest/markers-declared.json`
      = PASS (all declared markers composed in the PRX).
- [ ] Deployed via `prep_modded_launch.py --variant A --prx-path
      <round4-prx>`; DECRYPT-OFF READBACK DONE: magic 4f153d1d + full
      sha256 + size match (curl -Q "SITE SELF"). Never upload an
      unsigned/modified executable (bricks — PS5 rules).
- [ ] FY equivalence: same-snapshot rebuild with the pre-FY serial
      driver during the launch window -> PRX shas match (else stop
      concurrent builds).

Staging gates (this manifest, game closed):
- [ ] zzdiag-v11 deployed via `lua_fast_deploy.sh` — `SCRIPT VERIFIED`
      and `LOADLIST OK`; live zzdiag row now reads the measured v11 size (4065 as of this manifest).
- [ ] Spawner original bytes intact on console (FN freeze; readback hash
      == f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658,
      21415 B).
- [ ] ueselftest 13 files uploaded; EVERY file readback `cmp` VERIFIED.
- [ ] `launch1-PS5Pad.ini` uploaded; readback `cmp` VERIFIED (1736 B,
      957d3e25...).
- [ ] Live loadlist pulled + reconciled; upload `launch1-loadlist.txt`
      LAST; readback cmp VERIFIED; `grep -c ueselftest` == 13; 22 file
      rows; pad row == 1736; `grep mfa_activate_failed` == none (pre-check
      of shape only — the real gate is the launch log).
- [ ] NO other loadlist row touched (sidecar law kept).

Handoff:
- [ ] User sequence issued (MANIFEST-LAUNCH-1 §7): launch -> same save
      -> in-world -> SPAWN FIRST (L3+R3) -> ~10 s -> L1+R1 ONCE ->
      optional re-press if SKIP no-valid-pc -> CLEAN EXIT.
- [ ] GO only when all above PASS. Any FAIL: stop, name the owner
      (admission/pad/PRX), no launch, no rapid-fire retry.

Post-launch (read-only, BEFORE any relaunch):
- [ ] `harvest.py --label l26-round4-spawn-st2-r1`; spawn acceptance
      markers present (`Spawned pickup`/inventory ID vs `Failed at`);
      `[BUILD]` stamp = round-4 (snapshot != e8c4bb62); `[LFB-M] fname-rev
      ... ok=1`; `[PRB-1..6]` probe set; `[ST2] SUMMARY ... of 167 PASS`
      with io-roundtrip `PASS rw roundtrip`; klog clean (no copyin
      bursts, no nonsleeping, no fatal backtrace); crash -> note code +
      WHEN -> REBOOT.

## 2. LAUNCH 2 — ladder v2 four type-mods (FX-final PRX)

PRX gates:
- [ ] FX FINAL build (P2; P1 fault-ledger then P2 variant-B removal,
      both fully gated: snapshot/build/package/manifest) exists —
      `<fx-final-prx>` resolved. If only P1 exists: STOP (the FINAL
      build is what round-6 tests here). If FX builds are absent and
      round-4 remains resident from launch 1, an explicit reviewed
      decision is required to run ladder v2 on round-4 (documented in
      the receipt) — do not silently swap builds.
- [ ] `verify_build.sh --manifest` PASS + `prep_modded_launch.py` deploy
      + decrypt-off readback (magic + sha + size) DONE.

Staging gates (game closed; MANIFEST-LAUNCH-2):
- [ ] Launch-1 state still live (zzdiag 4065 + ueselftest rows + pad
      1736) — or the launch-1 rollback was executed and RE-STAGED per
      §4 of the manifest; either way pull the LIVE loadlist and
      reconcile before the manager runs (sidecar law).
- [ ] The four mods staged byte-exact (hashes = MANIFEST-LAUNCH-2 §1);
      every uploaded file readback cmp VERIFIED (ledger).
- [ ] `launch2-PS5Pad.ini` uploaded (1736 B, a6313eec...), readback cmp
      VERIFIED; `launch2-loadlist.txt` uploaded LAST, readback cmp
      VERIFIED; 30 file rows; pad row 1736.
- [ ] Manager gates recorded: preflight 6/6 LOADS-CLEAN; keymap
      --check 0 violations; conflicts = only the reviewed F7 co-fire
      (`--allow-cofire`); NO `mfa_activate_failed`-shaped rows.
- [ ] MiniMap CRLF bytes preserved end-to-end (196743 B, d8f8d64f...).

Handoff:
- [ ] User sequence issued (MANIFEST-LAUNCH-2 §7): launch -> same save
      -> in-world -> 10-15 s -> L1+R1 (self-test) -> L3+R3 spawner
      control -> 60-90 s play + minimap captures t0/t0+30 -> optional
      MiniMap setting change -> one UI at a time -> CLEAN EXIT.
- [ ] GO only when all above PASS.

Post-launch (read-only, BEFORE any relaunch):
- [ ] `harvest.py --label ladder2-combo-r1`; per-type markers per
      LADDER-V2 §2 (CheatManager in the ClientRestart window;
      ConsoleEnabler ConsoleKey read-back lines incl. Tilde/F10;
      MiniMap started + NO `:4270` too-old line + HUD captures; Stamina
      init line + zero async errors; spawner control green; self-test
      summary still 167-class). klog clean; crash -> harvest + REBOOT.
- [ ] Restarts r2/r3 (3-passes law): NOTHING restaged between passes
      (byte-identical); ploader re-arm per Z2; verdicts compared across
      the three harvests; a build change resets the cycle.

## 3. Standing hard stops (both launches)

- NEVER a rapid-fire second launch (kstuff pause/auth breakage); one
  launch per clean boot; crash => harvest + REBOOT before anything.
- NEVER upload any downloaded/modified executable unsigned; fake-sign
  via make_fself.py if an executable must change (not needed by these
  Lua-only stages).
- NEVER re-run a failed configuration identically; name the owner and
  change one thing.
- NEVER touch: autoload.txt, ploader.ini beyond prep's own restore,
  UE4SS-settings.ini except via prep, UEHelpers, the spawner bytes, or
  any other loadlist row outside this manifest's deltas.
- Text-only uploads here need no SITE SELF toggle (decrypt hazard is
  executable-only); the PRX readback ALWAYS does.
