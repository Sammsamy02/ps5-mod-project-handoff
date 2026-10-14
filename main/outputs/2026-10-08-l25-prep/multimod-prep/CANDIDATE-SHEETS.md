# CANDIDATE-SHEETS — per-mod staging, flow, markers, and riskiest call

Agent-EF, 2026-10-08. Task 3 of the multimod-prep packet. One sheet per
candidate. Sizes are the TREE-B LF bytes (staging copies — see
CANDIDATE-INVENTORY.md); `wc -c` the exact upload bytes before pinning rows.
Loadlist row format is TAB-separated, MFA size-exact:
`file<TAB>lua_mod<TAB><path><TAB><bytes>`. Every mod also needs its
`enabled.txt` (0 B) row. UEHelpers row (`ue4ss/Mods/shared/UEHelpers/UEHelpers.lua 9962`)
is ALREADY resident and satisfies `require("UEHelpers")` for all of them.

Risk classes reference agent-CI-marshaling-audit.md §5 (GREEN/AMBER/RED rows
#1–#28). Entries marked **UNMAPPED** are API families the spawner never
exercises, so CI's table has no row; the nearest-row analog and the reason it
is scheduled are given.

Pad-chord law for new rows (PS5Pad.ini parser): no chord may contain another
configured chord. Currently configured: `L3+R3=F7`, `UP/DOWN/CROSS/LEFT/RIGHT`
= arrows/RETURN. Free single buttons: TOUCH_PAD, TRIANGLE, CIRCLE, SQUARE,
L1, R1, L2, R2 (OPTIONS stays deliberately unbound). PS5Pad.ini size changes
with every row added → its loadlist row must be re-pinned size-exact.

---

## S1. LineTraceMod — RECOMMENDED SLOT 1 (lowest-risk foreign codebase)

Source: TREE-B `…/assets/Mods/LineTraceMod/Scripts/main.lua` (2,642 B).

**Loadlist rows** (add to the post-zzdiag 7-file-row composition):
```
file	lua_mod	ue4ss/Mods/LineTraceMod/Scripts/main.lua	2642
file	lua_mod	ue4ss/Mods/LineTraceMod/enabled.txt	0
```
**Pad row**: `TOUCH_PAD = F3` (Key.F3 is the mod's only bind; no modifiers).

**Keybind → action flow**: load → `Init()` validates both Kismet CDOs via
UEHelpers (StaticFindObject of `Default__KismetSystemLibrary` /
`Default__KismetMathLibrary` — the same class family the initializer itself
waits on) and prints their full names. TOUCH_PAD → F3 keybind → (callback is
NOT wrapped in ExecuteInGameThread by the mod — it runs on the keybind
dispatch thread) → reads `PlayerController.Pawn` / `.PlayerCameraManager` →
camera location/rotation → `GetForwardVector`/`Multiply_VectorInt`/
`Add_VectorVector` → `LineTraceSingle` (12 params, HitResult OUT — the exact
signature the spawner will have proven) → on hit, resolves the actor through
the FHitResult out-table and prints its full name.

**Marker strings** (exact):
- load: `[LineTraceMod] KismetSystemLibrary: <fullname>` then
  `[LineTraceMod] KismetMathLibrary: <fullname>`
- per press: `[LineTraceMod] Doing line trace`
- outcome: `[LineTraceMod] HitActor: <fullname>` OR `[LineTraceMod] Nothing hit.`
- load failure (error() → mod fails to start, loud):
  `KismetSystemLibrary not valid` / `KismetMathLibrary not valid`

**Riskiest call**: `GetActorFromHitResult` → on UE5.4+
`HitResult.HitObjectHandle.ReferenceObject:Get()` — a NESTED struct member
read out of the refilled FHitResult table followed by a lazy/weak-object
wrapper `.Get()` dereference. Nearest CI row: #20 (nested UScriptStruct
chain, AMBER, nil-on-miss semantics) but it goes one layer beyond anything
the spawner reads (`hit.ImpactPoint` floats only), and the branch selector
`UnrealVersion:IsBelow(5, 4)` (UNMAPPED global, host-comparable logic) picks
the member name — a wrong version answer indexes a nil field inside a
callback with no pcall → loud Lua error, contained to the press. Second
risk: `:GetFullName()` prints ride CI #8 (AMBER-CRITICAL, first live
Conv_NameToString class — same class the spawner's logging already needs, so
no NEW exposure). `LineTraceSingle` itself is CI #12/#15/#18 — the proven-by-
then signature; running it from a foreign codebase is precisely the
generality evidence.

---

## S2. CheatManagerEnablerMod — RECOMMENDED SLOT 2 (adds RegisterHook + first non-widget SCO)

Source: TREE-B `…/assets/Mods/CheatManagerEnablerMod/Scripts/main.lua` (1,407 B).

**Loadlist rows**:
```
file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua	1407
file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/enabled.txt	0
```
**Pad row**: none (event-driven; no RegisterKeyBind in this mod).

**Event → action flow**: top-level `RegisterHook("/Script/Engine.PlayerController:ClientRestart", fn)`
(no pcall — a refusal fails the mod's start loudly). On every ClientRestart
(save load, pawn respawn): `self:get()` → check `CheatManager` property →
if absent, `CheatClass` property Get (fallback `StaticFindObject("/Script/Engine.CheatManager")`)
→ `StaticConstructObject(CheatManagerClass, PlayerController)` (2-arg, no
FName — avoids CI #6) → `CheatManager =` ObjectProperty Set → log.

**Marker strings** (exact):
- `[CheatManagerEnabler] CheatManager already exist, skipping restoration`
- `[CheatManagerEnabler] Controller:CheatClass is nullptr, using default CheatClass instead`
- `[CheatManagerEnabler] Couldn't find default CheatClass, therefore, could not enable Cheat Manager`
- `[CheatManagerEnabler] Constructed CheatManager [0x…]`
- `[CheatManagerEnabler] Enabled CheatManager`
- `[CheatManagerEnabler] Was unable to construct CheatManager, therefore, could not enable Cheat Manager`

**Riskiest call**: `RegisterHook` on a named UFunction — **UNMAPPED** in CI's
table (the spawner never hooks). It is the arbitrary-UFunction pre/post hook
dispatch family; the R3 marshal internally uses one carrier UFunction
post-hook, but a mod-requested hook on ClientRestart is the first
user-facing use. Failure shapes: (a) hook registration throws at script load
→ `[l22]` mod-start line missing (clean admission-visible failure), (b) hook
registered but never fires → silence (distinguishable: no marker after save
load), (c) hook fires with bad context marshaling → crash-class, klog-gated.
Second risk: `StaticConstructObject` = CI #22 AMBER ("native success not
observed" per the initializer's own log) — this mod delivers the first live
SCO OUTSIDE the widget path, with no FName argument (dodges CI #6 RED).

---

## S3. ConsoleEnablerMod — RECOMMENDED SLOT 3 (adds array-element struct writes + FindFName + a second user-visible surface)

Source: TREE-B `…/assets/Mods/ConsoleEnablerMod/Scripts/main.lua` (3,173 B).

**Loadlist rows**:
```
file	lua_mod	ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua	3173
file	lua_mod	ue4ss/Mods/ConsoleEnablerMod/enabled.txt	0
```
**Pad rows**: none for UE4SS keybinds (the mod registers none). For the
USER-VISIBLE rung add a game-input synthesis row: `SQUARE = F10` (the mod
adds Tilde AND F10 to the game's ConsoleKeys; F10 is an exact KeyDef name,
avoiding OEM-key naming ambiguity for Tilde).

**Flow**: load → `ExecuteInGameThread(CreateConsole)` (mod-wrapped) →
`UEHelpers.GetEngine()` (FindFirstOf("Engine")) → `Engine.GameViewport` → if
no `ViewportConsole`, `StaticConstructObject(ConsoleClass, GameViewport)` →
assign `GameViewport.ViewportConsole` → `RemapConsoleKeys()`: reads
`Default__InputSettings` CDO, iterates `ConsoleKeys` array, appends
`{KeyName = FindFName("Tilde"/"F10")}` elements, prints the resulting key
list. ALSO re-fires on ClientRestart via RegisterHook (same family as S2).
**User-visible outcome**: pressing SQUARE (synthesized F10) opens the game's
own console — a second, game-native UI surface built entirely by reflected
calls (no UMG).

**Marker strings** (exact):
- `[ConsoleEnabler] ConsoleKey[%d]: %s` (one per key — the success line)
- `[ConsoleEnabler] InputSettings not found, could not change console key`
- `[ConsoleEnabler] Was unable to find an instance of UEngine`
- `[CreateConsole] Was unable to construct an UConsole object`
- `ConsoleClass, GameViewport, or ViewportConsole is invalid`

**Riskiest call**: `ConsoleKeys[#ConsoleKeys + 1].KeyName = KeyName` —
appending an element to a reflected ArrayProperty-of-struct and setting a
nested NameProperty through it. Nearest CI rows: #4 (string→NameProperty,
AMBER-STRICT) + #18 (array param machinery, AMBER) but both only cover
param marshaling — an array ELEMENT WRITE with struct-field Set is UNMAPPED
and is this rung's new class. Mitigations already in the mod: the value is
`UEHelpers.FindFName` output = proper FName userdata (FName ctor with
FNAME_Find — pool READ, not the CI #6 RED admission path; miss returns
NAME_None silently, which the mod checks via `~= NAME_None`). Second risk:
`KeyName:ToString()` in the success print = CI #8 AMBER-CRITICAL class
(same Conv_NameToString first-live dependency as S1/S2 logging).

---

## S4. SplitScreenMod — reserve (game-state writes; second FindAllOf consumer)

Source: TREE-B, 4,270 B. Rows: main.lua 4270 + enabled.txt 0.
**Pad rows**: `TRIANGLE = CONTROL+Y` (create), `SQUARE = CONTROL+U`
(destroy), `CIRCLE = CONTROL+I` (teleport) — modifier overload of
RegisterKeyBind (bound in the composed runtime; pad profile supports
MODIFIERS+KEY synthesis).
**Flow**: load → bool property Sets on GameMapsSettings CDO
(bUseSplitscreen=true) + logs; per chord → FindAllOf("PlayerController")
cache → `CreatePlayer`/`RemovePlayer`/`K2_SetActorLocationAndRotation`
(HitResult OUT) inside ExecuteInGameThread.
**Markers**: `[SplitScreenMod] UseSplitScreen: true`,
`[SplitScreenMod] OffsetPlayerGamepadIds: true`,
`[SplitScreenMod] Creating player..`, `Player %d created.`,
`Player could not be created.`, `[SplitScreenMod] Removing <fullname>`,
`Players teleport to Player 1.`, `No players could be teleported`,
`[SplitScreenMod] Player could not be destroyed, only 1 player exists.`
**Riskiest**: `GetGameplayStatics():CreatePlayer(pc, id, true)` — creates a
real player/controller (game-state write, spawn-class side effects; nearest
CI rows #14/#16 AMBER but with game-side consequences a diagnostic mod
never has). Held in reserve because a mis-fire alters the save session.

## S5. ActorDumperMod — REJECT for this milestone

593 B. `RegisterCustomProperty` hardcodes `Level.Actors` offset 0x98 —
manual layout offsets are crash-grade if the console image differs from the
PC layout the mod assumed (the mission already carries a 0xD0-vs-0xD8
ClassCastFlags discrepancy warning). Also array-wrapper `:ForEach` +
CTRL-modified keybind. Scheduled: never, unless a dedicated custom-property
rung is designed.

## S6. Keybinds — REJECT for this milestone

3,980 B. Binds the C++ dumper tools (DumpAllObjects/GenerateSDK/DumpUSMAP…).
Those are host-side C++ features; on console they are expected no-ops or
unimplemented — the mod measures our C++ surface, not Lua marshaling
generality.

## S7. BPML_GenericFunctions — REJECT for this milestone

2,883 B. `RegisterCustomEvent` + `OutParam:set` only ever fire from Blueprint
mods loaded via BPModLoaderMod (not resident; paks out of scope). Nothing
observable without that stack.

## S8. ConsoleCommandsMod — later (pairs with S3)

4 files (main 286 + set 2,007 + dump_object 12,777 + summon_unloaded_assets
479 = 15,549 B payload; FIVE loadlist rows incl. enabled.txt). Handlers hang
off `RegisterConsoleCommandHandler` — needs the game console that S3
constructs, plus synthesized console-key typing (a much deeper input-surface
question). Natural follow-on once ConsoleEnabler's console is up.

## S9. TownfallMiniMap v3.1 — the capstone (real Nexus mod; separate milestone)

196,743 B (CRLF staging copy; sha256 d8f8d64f… — full hash in
CANDIDATE-INVENTORY). Rows: main.lua 196743 + enabled.txt 0 (re-pin exact
bytes of whatever copy is uploaded).
**Flow**: no keybinds at all — self-driving: `LoopInGameThreadWithDelay`
50 ms tick (`if LoopInGameThreadWithDelay then`-guarded: absent global =
mod silently inert, only its one failure print would NOT appear — its sole
print is the too-old-runtime error, so acceptance is USER-VISIBLE-first:
minimap panel visible in a screen corner during play). Scans via FindAllOf
of game classes (InteractComponent, TownfallEnemyCharacter,
PickupInteractable, HidingPlaceInteractable, MapCornerBounds), builds 20+
widgets via StaticConstructObject, RegisterHook pcall'd once,
NotifyOnNewObject guarded.
**Marker strings**: load-silent on success; failure:
`[TownfallMiniMap] this UE4SS is too old, install the experimental build`.
Acceptance must lean on screen capture + UE4SS.log absence-of-errors +
tick-liveness evidence (e.g. its scan logs if any are added — none found).
**Riskiest call**: the sustained `LoopInGameThreadWithDelay` tick at 20 Hz —
UNMAPPED scheduler family (the spawner used one-shot ExecuteInGameThread
only); plus `GetClass` (8×) which is the zzdiag-v2 crash family, and
`SetColorAndOpacity` (27×) squarely in CI #13 RED (silent zero-render
FSlateColor hazard). This mod concentrates every remaining unknown at once —
exactly why it is LAST, after S1–S3 have each added one class at a time.

## S10. TownfallModMenu v0.66 — reserve (RegisterHook-saturated; nav collision)

128,009 B (sha256 2ff1fa53…). Load marker:
`[TownfallModMenu] Loaded v0.66-weight-invisibility. BACKSPACE opens/closes the menu.`
Binds BACKSPACE/UP_ARROW/DOWN_ARROW/RETURN — the last three CO-FIRE with the
spawner's nav binds (keybinds are non-exclusive notifications): testing it
with the spawner resident means both menus navigate simultaneously. Must run
in a boot without the spawner, or accept documented co-fire. Raw RegisterHook
sites (some unguarded) + `ExecuteInGameThreadWithDelay` guarded fallbacks.
**Riskiest**: unguarded `RegisterHook` installs (script-load failure class)
— same family as S2 but at 10× count. Reserve for after S2 proves the
RegisterHook class.

## Recommended first three

S1 LineTraceMod → S2 CheatManagerEnablerMod → S3 ConsoleEnablerMod.
Rationale: one NEW unproven class per slot (S1: nested-struct + weak-object
read of a familiar out-table; S2: RegisterHook + first non-widget SCO; S3:
array-element struct write + FindFName + second user-visible surface), each
with loud markers and a size-exact two-row staging footprint. S9 MiniMap is
the milestone capstone AFTER these three pass.
