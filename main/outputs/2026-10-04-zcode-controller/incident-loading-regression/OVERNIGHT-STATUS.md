# Overnight Status — 2026-10-06 (user sleeping, PS5 in rest mode)

## What happened before you slept
Session 1084 was the breakthrough boot: ALL FOUR batched IO controls passed,
and the copyin from the pool's DM-class address transferred 64 bytes of real
data. The pool admission mechanism is PROVEN WORKING in-game.

The only remaining issue: the seeds-phase probe (which fires seconds after
Control D) re-probed instead of using Control D's already-captured bytes.
The capture-first fix (initializer 782dce71) is composed (GO ×2, determinism
6f215d89) and the candidate FSELF 12fcc41f is READY TO STAGE.

## What's ready for you when you wake up
1. **The pool-validation candidate** (12fcc41f): staged locally, needs one
   FTP upload when the PS5 is on. This should validate the pool and proceed
   to the walkguard gate (the expected next stop).

2. **The in-session diagnostic service** (worker-insession-integrate):
   being integrated so you can keep Townfall open and run IO probes/pool
   probes from the command file without relaunching.

3. **The T2 probe** (worker-t2-probe-prep): everything prepared for the
   walkguard unlock experiment (the console probe that tests X→RX mprotect
   on the exec-only text page).

4. **The R3Marshal import fix** (worker-r3-import-fix): diagnosing why the
   r3-ladder+gate5 merged LuaMod's cross-TU references don't resolve in the
   composed runtime object. Fixing this unlocks the marshal-first
   ExecuteInGameThread routing (needed for the first F7 press).

5. **The post-pool candidate manifest** (worker-postpool-candidate): the
   complete set of changes for the candidate AFTER the pool validates,
   combining input-tag + game-identity + in-session service.

6. **The mod compatibility matrix** (worker-mod-compat): what it takes
   for ALL mods to work, not just the Item Spawner.

## The morning checklist (ONE COMMAND)
1. Power on the PS5 (rest mode → active)
2. Tell ZCode you're awake — ZCode runs:
   ```
   bash outputs/2026-10-04-zcode-controller/incident-loading-regression/morning_stage.sh
   ```
3. Launch Townfall once
4. Tell ZCode when both popups appear
5. ZCode pulls evidence and reports the pool's verdict

## Critical T2 safety finding (overnight)
The walkguard fold-1 object uses a RAW SYSCALL for mprotect (inline asm,
confirmed by disassembly) — this is process-fatal under session-934 law.
The guard MUST be revised to NID-routed sceKernelMprotect before any fold
flip. The T2 probe (when run) will test whether the sceKernelMprotect NID
resolves at all. The xwp probe's own 8-import contract includes this
derived NID but flags it as "NOT console-proven".


## Additional overnight preparations (updated)
- NID-routed walkguard revision created (750fde30): replaces the raw-syscall
  mprotect with sceKernelMprotect — safe under 934 law, ready for when T2
  proves the NID resolves. Staged at worker-t2-probe-prep/nid-revision/.
- App survey completed: the app vertical (UI → installer → loadlist → console)
  is host-complete but never ran on-console. The Lua mod family is the only
  complete pipeline. After the runtime works, the app can drive mod installs.
- Mod compatibility survey in progress: analyzing what APIs common UE4SS
  mods need vs. what the PS5 port provides.


## Mod compatibility findings (worker-mod-compat)
- 83 live Lua API functions + ~20 tables registered — API surface is COMPLETE
- Item Spawner rated ~FULL compatibility (all its APIs are registered)
- The bottleneck is execution, not registration: the R3 marshal world skips
  8 callback families (LoadMap, InitGameState, BeginPlay, etc.)
- Most impactful addition: in-drain LoadMap/world-change shim (ranked #1)
- Full matrix in worker-mod-compat/MOD-COMPAT.md


## R3Marshal resolution (worker-r3-import-fix, 2026-10-06)
- Root cause: IO-controls worker's initializer base (4a2b9400) lacked the
  entire R3 marshal implementation → symbols were U in the composed object
- Fix: controller edit 21 (initializer 782dce71) took the correct base
- VERIFIED end-to-end: R3Marshal T×3/U×0 in the production composed object
  (6f215d89); the merged LuaMod (facf993a) can be integrated with NO special
  handling — just the standard pin cascade
- Negative control reproduced the incident exactly (R3 compiled out with
  -DUE4SS_R3_MARSHAL_MODE=0 → both U)
- The current pool-validation candidate (12fcc41f) ALREADY has the fix


## FINAL overnight status (2026-10-06 ~06:00)
The morning candidate is the PROVEN pool-validation set (NOT the max-evidence
variant — the in-session service needs proper overlay wiring, deferred to
candidate 2):
- Bridge: 497348b8 (IO controls + Sony-native copyin + close-before-readback)
- Initializer: 782dce71 (capture-first + audit fixes + R3 definitions)
- LuaMod: c92c92ff (proven; merged facf993a deferred)
- SeedValidation: c2e6e663
- Composed: GO×2, determinism 6f215d89
- Staging: morning_stage.sh (one command when PS5 is on)

In-session service (bridge 8b5d546d + entry 40b4b2a4) is HOST-TESTED and
ready for candidate 2 — needs overlay include path wiring for the service
headers. The merged LuaMod (facf993a, comparator 37eece2d) is verified
compatible (R3Marshal T×3) and ready for candidate 2 as well.
