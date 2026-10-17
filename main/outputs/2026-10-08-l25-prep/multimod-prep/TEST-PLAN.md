# TEST-PLAN — multimod proof-of-generality console sequence

Agent-EF, 2026-10-08. Task 4 of the multimod-prep packet. Pattern mirrors
agent-CK's ACCEPTANCE-LADDER.md / RUNBOOK-MOD-MENU-UP.md (rung ladder +
failure-owner table + phased console sequence), extended per candidate mod.

PRECONDITION (gate for everything here): the "MOD MENU UP" milestone is
ACCEPTED — CK rungs 0–4 all green for TownfallItemSpawner (spawner menu
opens, spawn works, klog clean). Only then does this sequence begin, and it
begins AFTER ZZDIAG-RETIREMENT.md has been executed (diag mods gone; the
composition is the clean 7-file-row base below).

## Base composition at start (post-retirement, 12 lines / 7 file rows)

```
modforge-loadlist	1
title	PPSA33286
engine	unreal
framework	ue4ss-ps5
mod	TownfallItemSpawner	1.0.0
file	config	ue4ss/UE4SS-settings.ini	1947
file	config	ue4ss/MemberVariableLayout.ini	588
file	config	ue4ss/VTableLayout.ini	216
file	config	ue4ss/PS5Pad.ini	3718
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962
```
LAW: pull the LIVE loadlist first and reconcile against this reference before
every launch; if any live row differs, keep live values and re-derive sizes
(the same reconciliation rule as zzdiag-v7's sidecar). The spawner stays
resident in every launch as the known-good CONTROL (its `Menu created.`
behavior must keep working while foreign mods run — co-residency is itself
part of the generality claim).

## Order and staging deltas (cumulative; one new mod per launch)

| Launch | Adds | New loadlist rows | PS5Pad.ini change | New size to pin |
|---|---|---|---|---|
| M1 | LineTraceMod | 2 (2642 + enabled 0) | add `TOUCH_PAD = F3` | PS5Pad.ini row grows → re-`wc -c` + re-pin its config row |
| M2 | CheatManagerEnablerMod | 2 (1407 + enabled 0) | none | none |
| M3 | ConsoleEnablerMod | 2 (3173 + enabled 0) | add `SQUARE = F10` | re-pin PS5Pad.ini row |

Why cumulative rather than all-at-once: CK's discipline of one new unproven
class per run — a failure in M2 is attributable to RegisterHook/SCO, not to
an interaction. M1's class is nearly-proven (lowest risk first), each later
launch adds exactly one new marshaling class (S1 nested-struct/weak-object
read → S2 RegisterHook+SCO → S3 array-element write/FindFName). Rollback of
any mod = its two rows (see per-mod rollback below).

## Per-mod acceptance ladders (CK pattern: rung 0 admission, rung 1 first
functional log, rung 2 action + corroboration, rung 3 user-visible, klog
gates the run)

### M1 — LineTraceMod

- Rung 0 (admission+start): `[l22] Lua mod started: LineTraceMod` +
  `[Lua] [LineTraceMod] KismetSystemLibrary: <fullname>` +
  `[Lua] [LineTraceMod] KismetMathLibrary: <fullname>` in the MAIN MENU.
  FAIL → FM1 (admission: size-exact rows, enabled.txt, LF byte count).
  FAIL with `KismetSystemLibrary not valid` → FM2 (CDO StaticFindObject —
  GATE-1-family regression; control: spawner Rung-1 markers still pass?).
- Rung 1 (dispatch): press TOUCH_PAD once (in-game, save loaded, aim at a
  nearby wall/object): `[l22] keybind callback entry key=<F3 keycode>` +
  `[Lua] [LineTraceMod] Doing line trace`.
  FAIL (no keybind entry) → FM3 (pad chord/PS5Pad row; verify the row, the
  chord-containment law, and that PS5Pad.ini's config row was re-pinned).
- Rung 2 (trace + marshal): EITHER `[Lua] [LineTraceMod] HitActor: <name>`
  (aimed at something) OR `[Lua] [LineTraceMod] Nothing hit.` (aimed at
  sky). BOTH outcomes pass — the point is a coherent boolean + refilled
  out-table read. HitActor additionally proves the nested
  HitObjectHandle.ReferenceObject:Get() chain and the #8 FName→string print.
  FAIL (Lua error mentioning a nil index / `.Get`) → FM4 (UnrealVersion
  branch or nested-struct nil-on-miss — marshaling owner; capture the exact
  error line). FAIL (silent after "Doing line trace") → FM5 (LineTraceSingle
  return marshaling — compare against the spawner's working ground-trace).
- Rung 3 (user-visible): none required for this mod (it is log-evidenced by
  design; the on-screen corroboration duty stays with the spawner control:
  its menu must still open/close in the SAME session).
- Rung 4: klog clean, clean exit. Crash → FM6 (stop, note step, reboot).

### M2 — CheatManagerEnablerMod (LineTraceMod still resident)

- Rung 0: `[l22] Lua mod started: CheatManagerEnablerMod` in the MAIN MENU
  (its RegisterHook runs at script load). FAIL (mod start missing, error
  near RegisterHook) → FM7 (RegisterHook registration itself — the new
  class; the error text decides hook-machinery vs UFunction-resolution).
- Rung 1 (hook fires): load the save → within the restart window ONE of:
  `[Lua] [CheatManagerEnabler] Enabled CheatManager` (+
  `Constructed CheatManager [0x…]`) OR `…already exist, skipping restoration`.
  FAIL (silence after save load) → FM8 (hook registered but never dispatched
  — the ClientRestart hook path; cross-check spawner still works).
- Rung 2 (SCO proof): `Constructed CheatManager [0x…]` with a nonzero
  address = first live non-widget StaticConstructObject (CI #22 checkpoint).
  `Was unable to construct CheatManager…` → FM9 (slot-4/SCO owner; the
  spawner's widget fallback path is the comparison evidence).
- Rung 3 (user-visible): none direct (CheatManager is invisible); the
  control rung is the spawner + minimap-free session continuing to behave.
- Rung 4: klog clean. Crash on save load → FM6.

### M3 — ConsoleEnablerMod (M1+M2 still resident)

- Rung 0: `[l22] Lua mod started: ConsoleEnablerMod`. Its load-time
  ExecuteInGameThread may fire in-menu; ClientRestart re-fire expected at
  save load. FAIL → FM1/FM7 (admission, or its RegisterHook).
- Rung 1 (engine path): `[Lua] [ConsoleEnabler] ConsoleKey[%d]: %s` lines
  appear (game defaults + appended Tilde/F10). FAIL
  `Was unable to find an instance of UEngine` → FM2-family (FindFirstOf
  regression — control: spawner scan markers). FAIL
  `InputSettings not found…` → FM10 (CDO path for Default__InputSettings).
- Rung 2 (array-element write): the Tilde/F10 rows present in the printed
  ConsoleKey list = the array append + nested KeyName Set landed.
  FAIL `ConsoleClass, GameViewport, or ViewportConsole is invalid` → FM11
  (GameViewport timing — retry after full load; if persistent, viewport
  property read owner).
- Rung 3 (USER-VISIBLE, the milestone's second UI proof): press SQUARE
  (synthesized F10, added as a GAME console key by the mod): the game's own
  console opens on screen. FAIL (nothing) → FM12 (synthesized-key → game
  input routing; distinguish from keybind notifications via
  `[l22] keybind callback entry` absence — F10 is not a UE4SS bind here).
- Rung 4: klog clean, clean exit.

## Failure-owner table (mirrors CK F1–F9 numbering style)

| # | Shortfall | Owner |
|---|---|---|
| FM1 | mod-start line missing / mfa_activate_failed | ADMISSION (row sizes vs uploaded bytes — LF re-measure, enabled.txt present) |
| FM2 | CDO StaticFindObject invalid | RESOLUTION/FindXOf (GATE-1 family; spawner control decides regression vs mod-specific path) |
| FM3 | no keybind entry on chord | PAD (PS5Pad row, chord law, config row re-pin) |
| FM4 | nil-index/.Get Lua error in GetActorFromHitResult | MARSHALING nested-struct (UnrealVersion branch selector + weak-object read) |
| FM5 | silent after "Doing line trace" | MARSHALING return/out-refill (LineTraceSingle foreign-call return path) |
| FM6 | any crash | klog + reboot; attribute by last marker (CK F8/F9 rules) |
| FM7 | RegisterHook load failure | HOOK machinery (new class — S2/S10 evidence) |
| FM8 | hook registered, never fires | HOOK dispatch (ClientRestart path; compare R3 carrier behavior) |
| FM9 | SCO construct failed | slot-4/SCO owner (CI #22) |
| FM10 | Default__InputSettings not found | RESOLUTION (CDO path for engine-settings class) |
| FM11 | GameViewport invalid at fire time | VIEWPORT timing (retry after load; property-read owner if persistent) |
| FM12 | console does not open on SQUARE | INPUT SYNTHESIS to game layer (pad → game input, not UE4SS keybinds) |

Precedence: FM1 blocks everything; FM6 gates the run's validity; otherwise
each launch has exactly one new class so attribution is direct.

## Rollback (per mod — loadlist row removal is the whole mechanism)

For mod X with rows R1(main.lua)/R2(enabled.txt):
1. Pull live loadlist; delete R1+R2 (and ONLY those rows); upload; `cmp`
   readback against the edited local copy; `grep -c $'\t'` must show the
   expected file-row count (7 for full rollback to base; 9 after M1 while
   keeping M1's mod, etc.).
2. Delete remote files (harmless if DELE is refused — unlisted lua files
   are never admitted; note it in the receipt):
   `curl -sS -Q "-DELE /data/modbridge/roots/PPSA33286/ue4ss/Mods/<X>/Scripts/main.lua" ftp://ps5-host-not-configured.invalid:2121/`
   (same for enabled.txt; the Scripts/Mods dirs may remain empty).
3. If the mod added a PS5Pad.ini row (M1 TOUCH_PAD, M3 SQUARE): remove that
   row too, re-`wc -c`, re-pin the PS5Pad.ini config row, readback `cmp`.
4. Next launch confirms: mod-start line absent, no new keybind entries, no
   new markers, spawner control unaffected.
Unloading is admission-level only — no executable changes are involved in
this milestone at any point (Lua + ini only), so no fake-signing/decrypt
workflow applies beyond the standing PRX already resident.

## Session laws (unchanged from CK runbook)

One launch per clean boot; crash → stop, preserve UE4SS.log + klog, reboot
before anything; single spaced chord presses; harvest with
`harvest.py --label mm-<launch>`; every phase appends to the receipt
(row sizes, readback cmp results, timestamps, user-visual confirmations,
harvest path). Raw logs are never edited.

## Exit criteria

M1+M2+M3 all green (each mod's rungs 0–4) = "generality proven for three
unmodified PC Lua mods beyond the developed-for-one": the runtime ran
foreign codebases using FindFirstOf/FindAllOf/RegisterKeyBind/
ExecuteInGameThread/basic UFunction calls, plus one new class each
(nested-out-table read / RegisterHook+SCO / array-element write).
Then S9 TownfallMiniMap is proposed as the separate capstone milestone
(real Nexus mod, sustained tick + full widget surface), per
CANDIDATE-SHEETS.md S9.
