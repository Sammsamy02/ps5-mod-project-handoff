# TEST-PLAN — Stellar Blade first-launch blind test (console sequence)

Agent GJ, 2026-10-08. The CONSOLE-side plan for when prep ends and the user
authorizes a session. Aligned with GC LADDER-V2 §3/§4 (the standard check:
profile → self-test → mod-per-type) and addendum A5 (the same flow becomes
automatic at first launch — agent GI's FIRST-LAUNCH-AUTOMATION.md had NOT
landed when this was written; this plan is the manual execution of that
pipeline and its schema must be reconciled with GI's when it lands).

LAWS: one launch per clean boot, never rapid-fire (kstuff pause rules);
clean exit to dashboard every session; crash ⇒ harvest immediately, note
code+WHEN, REBOOT before anything; klog tap running BEFORE launch; mods
never edited; zero Blade special cases in any fix.

## 0. Preconditions (all before launch 1)

1. User's own Blade dump acquired per decrypt-off law (PREREQ-CHECKLIST §1)
   and `run-blade-profile.py` verdict = ACCEPTED with the generated profile
   (`runs/<sha12>/profile.json`). If REFUSED: STOP — the refusal IS the
   result of this stage (a seed-version refusal means the table must be
   upgraded by measurement first; a 0-marker refusal would mean not-UE).
2. Generic runtime rebuilt (mains power, not battery prep) against the
   generated profile only — same generic source shape as the Townfall build,
   re-pointed at the new profile units; manifest-PASS + decrypt-off readback
   (magic 4f153d1d + sha256) per WAKE-RUNBOOK §10.3. Rule-10 audit gate:
   grep the composition + profile for the title id / game name — must be
   absent from runtime logic (paths/config may carry it).
3. Mods staged per BLIND-TEST-KIT §2 (CheatManagerEnabler, ConsoleEnabler,
   Stamina, ConsoleCommands, shared/UEHelpers + enabled.txt rows), `wc -c`
   size-exact loadlist rows (MFA law), readback `cmp` every file. ZERO
   PS5Pad.ini changes (all picks keybind-free).
4. Console healthy: no game running (eboot count 0), daemon census, klog tap
   (TCP 3232, NEW file per launch) started, kstuff quarantine respected
   (never autoload kstuffSharp/InstallerApp).
5. GB's ueselftest present in the build (combo press available in-session).
6. Baseline: Blade launched UNMODDED once before any modded launch (clean
   exit) — a game-health control separating game issues from port issues.

## 1. First-launch sequence (launch 1 of the 3-pass law)

1. ploader payload armed per the Z2 timing law (≤120 s wait if resident).
2. LAUNCH Blade. Watch klog from t=0 (first-launch profile+self-test are
   the generic runtime's own startup work: image bind → live base resolve →
   slot arming → ueselftest availability).
3. Load a save / reach the game's main gameplay state (ClientRestart window:
   CheatManagerEnabler + ConsoleEnabler markers expected here).
4. Press the SELF-TEST COMBO once. ueselftest verdict rows = the engine-level
   gate (LADDER-V2 §3.2 bar: >=95% rows PASS AND 0 FAIL in the four critical
   families — hook, property/array-write, io, delay/loop).
5. Play 60–90 s (Stamina's LoopAsync survival window; observe no error
   chunks).
6. Optional only if the console surface opened: enter `dump_object
   <sidecar-path>` (mod-level io evidence; otherwise record the N/A gap).
7. Screen capture if any user-visible surface appeared.
8. CLEAN EXIT via in-game quit to dashboard. Never a power event.

## 2. Harvest (read-only, BEFORE any relaunch)

Fetch UE4SS.log + klog for the label; grep the marker set:
`Lua mod started` (4 mods admitted: CheatManagerEnabler, ConsoleEnabler,
Stamina, ConsoleCommands + UEHelpers require), `[CheatManagerEnabler]`,
`[ConsoleEnabler]` / `ConsoleKey[`, `[Stamina]`, ueselftest verdict rows,
error chunks. klog gates: no copyin bursts, no nonsleeping-thread floods, no
fatal backtrace. Verdict per type per BLIND-TEST-KIT §2 table.

## 3. The 3-pass law (acceptance)

- Launch-1 verdicts are PROVISIONAL-PASS only.
- ACCEPTED requires the same pass markers in 3 total user-launched, clean-exit
  sessions (launch 1 + up to 2 restarts), byte-identical staging (no loadlist
  churn, no re-pin), klog clean each time.
- Any launch's FAIL ⇒ the type verdict FAILs (flaky = failed), owner named.
- A runtime/build change RESETS the 3-launch count (labels `-r1..-r3` per
  build id, LADDER-V2 §3.4 law carried over).

## 4. Failure-triage rule (the blind-test core law)

EVERY failure is fixed at ENGINE/VERSION level — in the generic runtime or
the per-version layout table — NEVER as a Blade special case. No Blade
names, offsets, sha keys, or branches may enter any repo file as a result of
a failure (Rule 10 audit after every fix).

| Failure shape | Owner (fix location) | Never |
|---|---|---|
| profile REFUSED (seed row) | per-version table: measurement campaign on this binary per the row's confirm_when list, then the row is upgraded for ALL games of that version | hand-writing a Blade profile |
| live base resolve fails | generic runtime base-resolution stage (ASLR/ET_SCE_EXEC_ASLR handling) | hardcoding Blade's base |
| slot arming refusal (GUO/GMalloc/FName ctor validators V1/V2, pool shape) | generator derivation arms + validator thresholds; if a version-shape mismatch: the table entry's shape fields | per-image label pins |
| ueselftest FAIL in a critical family | the port-side owner of that API family (same routing as Townfall; the fix lands for every game) | skipping the gate to run mods anyway |
| CheatManager hook never fires | hook-registration/dispatch family (engine-level) | a Blade-only hook path |
| ConsoleEnabler write does not stick | TArray NewIndex / struct-field Set family + FName ctor Find-variant | special-casing InputSettings |
| Stamina loop errors | LoopAsync carrier | — |
| crash (CE-108255-1) | harvest + klog symbolization first; fix the named engine seam; reboot; restart the 3-pass count | retrying into a paused-kstuff state |

If a failure cannot be attributed to an engine/version seam, the honest
outcome is UNPROVEN for that type on this game — recorded, not patched
around.

## 5. Rollback

1. Mod-side: remove the Blade mod-root loadlist rows + mod dir (GG
   mod-manager path), verify readback; UEHelpers row removal follows the
   manager's reconciliation (never leave orphan requires).
2. Payload-side: restore the previous frozen known-good PRX (Townfall
   lineage) with decrypt-off readback verification; empty ploader.ini = the
   unmodded-play configuration (never touch autoload.txt for rollback).
3. Runtime-side: any engine-level fix lands behind the normal
   build+manifest+readback gate, and the TOWNFALL 3-launch regression set
   re-runs (a generic-runtime change must not regress game 1).
4. Game-side: Blade needs nothing restored (no executable was ever modified
   or re-uploaded — decrypt-off law absolute; pak/asset mods were never in
   scope).
