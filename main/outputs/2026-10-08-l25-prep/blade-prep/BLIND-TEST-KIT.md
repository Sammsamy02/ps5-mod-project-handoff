# BLIND-TEST-KIT — the mod-per-type set for Stellar Blade, game-agnostically

Agent GJ, 2026-10-08. Addendum A2 law: Stellar Blade is the BLIND TEST of the
generic runtime — profile from the GENERATOR only, GB's game-agnostic
ueselftest unchanged, ONE PC UE4SS Lua mod per type, any failure fixed at
engine/version level, Rule 10 extended to games (no Blade names/offsets/
special cases anywhere). Mods are NEVER edited (never-edit-mods law).

## 1. The 13 staged mods: game-agnosticity assessment

The 13 = EF's multimod-prep inventory (12, incl. the shared/UEHelpers lib)
+ StaminaMod (GC LADDER-V2's 13th candidate). "Engine-level" = touches only
`/Script/Engine.*` (or Kismet) classes that exist in every UE game.

| # | Mod | Size (staged copy) | Class coverage | Verdict for Blade |
|---|---|---|---|---|
| 1 | LineTraceMod | 2,642 B TREE-B LF | KismetSystemLibrary/KismetMathLibrary/PlayerController/FHitResult | ENGINE-LEVEL — first reserve (F3 keybind needs a pad row; blind test targets ZERO pad-row changes) |
| 2 | CheatManagerEnablerMod | 1,407 B | Engine.PlayerController:ClientRestart, Engine.CheatManager, 2-arg SCO | ENGINE-LEVEL — **PICK (event/hook)** |
| 3 | ConsoleEnablerMod | 3,173 B | Engine/GameViewport/ViewportConsole, Engine InputSettings ConsoleKeys array | ENGINE-LEVEL — **PICK (value/array-write; partial menu proxy)** |
| 4 | SplitScreenMod | 4,270 B | Engine.GameMapsSettings CDO, PlayerController, GameplayStatics | ENGINE-LEVEL — reserve (CreatePlayer side effects if chords fired; load-time bool writes clean) |
| 5 | ActorDumperMod | 593 B | Level.Actors via RegisterCustomProperty | **REJECT** — hardcoded offset 0x98 (offset law; layout-dependent, never blind-test-safe) |
| 6 | jsbLuaProfilerMod | 565 B (+shared jsb 14,113/Types 55,672) | profiler | **REJECT** — staged main.lua is fully commented out (inert); profiler/debug-hook exclusion law (LADDER-V2 §3.3.4) |
| 7 | Keybinds | 3,980 B | C++ dumper tools | **REJECT** — measures the C++ surface, not Lua marshaling (EF S6) |
| 8 | BPML_GenericFunctions | 2,883 B | RegisterCustomEvent/OutParam | **REJECT** — only fires from BP mods via BPModLoaderMod (pak machinery out of scope) |
| 9 | ConsoleCommandsMod | 286+2,007+12,777+479 B (4 files) | RegisterConsoleCommandHandler; set/dump_object/summon on engine objects; dump_object has `io.open(...,"w+")` | ENGINE-LEVEL — **PICK (config-io, reachability-caveated)** |
| 10 | shared/UEHelpers | 9,962 B | engine getters/FindFName | ENGINE-LEVEL shared lib — **STAGE as dependency** (resident copy lives in the Townfall root; Blade's root starts empty) |
| 11 | TownfallMiniMap v3.1 | 196,743 B | Townfall game classes (InteractComponent, TownfallEnemyCharacter…) | **GAME-SPECIFIC** — not a Blade candidate (its settings-io part is generic but the mod is load-silent on success; acceptance-by-absence is too weak for a blind test) |
| 12 | TownfallModMenu v0.66 | 128,009 B | Townfall game classes, 9 hooks | **GAME-SPECIFIC** — not a Blade candidate |
| 13 | TownfallItemSpawner | 21,415 B | Townfall item classes | **GAME-SPECIFIC** — the Townfall baseline itself; no Blade Item Spawner exists |
| +1 | StaminaMod | 2,811 B (sha 7667fed7e4d7b4cf…) | LoopAsync(1500) registration; Townfall class names ONLY inside permanently-gated dead branches (`InfiniteStaminaActive=false`; Notify never called) | **EFFECTIVELY ENGINE-LEVEL at load** — **PICK (sustained-tick)** with the honest limit below |

Reading of the pool: 7 of 13 are engine-level by design (the stock UE4SS
set — LineTrace/CheatManager/ConsoleEnabler/SplitScreen/Keybinds/BPML/
ConsoleCommands + UEHelpers); 3 are Townfall-specific; ActorDumper/jsb fail
exclusion laws; StaminaMod is engine-level in everything it executes.

## 2. The five type picks (engine-level preferred; ZERO new pad rows)

| Type | Pick | Trigger | Pass evidence (markers) | Honest limits |
|---|---|---|---|---|
| event/hook | **CheatManagerEnablerMod** | save load / pawn respawn (ClientRestart) — no key, no user step beyond playing | `[CheatManagerEnabler]` lines; every failure marker names its owner (hook registration / CheatClass resolution / SCO) | shipping-config games may legitimately lack a CheatClass — the mod prints that as a MARKER (`Couldn't find default CheatClass…`), which is still hook-FIRED evidence: distinguish "hook never fired" (silence) from "hook fired, engine declined" |
| value/array-write | **ConsoleEnablerMod** | load-time `ExecuteInGameThread(CreateConsole)` + ClientRestart re-hook | `[ConsoleEnabler] ConsoleKey[%d]: %s` read-back print = write-sticks proof; `ConsoleKeys[#ConsoleKeys+1].KeyName =` array-growth struct-field Set + FindFName + FName ToString | the CONSOLE UI itself may not open (needs a synthesized key) — the WRITE verdict is log-based (EF S3 note), not user-visible |
| config-io | **ueselftest io-family rows + ConsoleCommandsMod (staged)** | self-test combo / console command | self-test io rows (type(io), open-read-write-close round-trip in a mod folder) = ENGINE-LEVEL io PASS; `dump_object <path>` performs the mod-level `io.open(Parameters[1],"w+")` write | **reachability caveat**: the mod's handler fires only on an entered console command, which needs the console surface (pick #2's UConsole + a synthesized key). If the console cannot open on Blade: mod-level config-io = **N/A-for-blind-test**, gap recorded; engine-level io still proven by the self-test. No mod is ever edited to "fix" this |
| sustained-tick | **StaminaMod** | load | `[Stamina] Mod script initializing...` + ZERO `[Stamina]` error/async-error chunks for the whole session = LoopAsync(1500) registered + survived | loop body no-ops BY DESIGN (gated `InfiniteStaminaActive=false` — identical on Townfall); per-tick counts come from the self-test delay-family rows; Townfall class names live only in dead branches (never-edit-mods keeps them) |
| menu-class | **N/A-for-blind-test (honest option) + engine-level proxy** | — | proxy evidence: ConsoleEnabler's `StaticConstructObject(UConsole, GameViewport)` + `GameViewport.ViewportConsole =` object-ref Set (the exact SCO+Set machinery a UMG menu uses) + ueselftest rows | **there is no game-agnostic UMG menu mod in the 13** (Spawner/ModMenu/MiniMap are Townfall-class). Do NOT stage a Townfall menu on Blade. Menu type on Blade = PARTIAL(engine-surface) until a game-agnostic UI mod is fetched (EF FETCH-PLAN exists for widening the pool later) |

First reserve: LineTraceMod (engine-level, read-only; needs one pad row F3 —
stage only in a follow-up cycle that re-pins PS5Pad.ini per FV/MFA laws).
Second reserve: SplitScreenMod (load-time `UseSplitScreen: true` value-write
read-back is self-contained; avoid its CreatePlayer chords).

Staging set for the blind-test launch (TREE-B LF copies; `wc -c` the exact
uploaded bytes and pin them in the loadlist rows, MFA size-exact law):
CheatManagerEnablerMod (1407), ConsoleEnablerMod (3173), StaminaMod (2811),
ConsoleCommandsMod (4 files, 15,549 payload), shared/UEHelpers (9962) —
plus each mod's empty `enabled.txt`. Mod root for the new title follows the
manager's per-title root pattern (GG's mod-manager owns it). UEHelpers row
is REQUIRED — LineTrace/ConsoleEnabler/ConsoleCommands/Stamina all
`require("UEHelpers")` and Blade's mod root starts empty.

## 3. Blind-test integrity laws (this kit, restated from A2/LADDER-V2)

1. The runtime/PRX that runs on Blade is the SAME generic build shape as
   Townfall's, re-pointed ONLY at the generated profile (engine-version
   facts); no Blade-specific code path may exist (Rule 10 audit: grep the
   composition for the title id/game name before launch — must be absent
   from runtime source; it may appear only in configuration paths).
2. Self-test = GB's ueselftest, unchanged, combo-press in-session; the
   critical-family bar (hook / write / io / delay-loop families, 0 FAIL)
   gates the mods exactly as LADDER-V2 §3.2.
3. Mods never edited; a FAIL routes to the port-side owner named by the
   failing marker — engine/version-level fix in the runtime or the
   per-version table, NEVER a Blade special case.
4. Zero new PS5Pad.ini rows in the blind-test launch (all five picks are
   keybind-free); pad file byte-identical, no MFA re-pin.
5. Pak/asset mods are OUT (capabilities #7/#8) — this kit is Lua-only.
