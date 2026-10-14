# LAUNCH-2 VERDICT — all mods working (2026-10-11)

PRX 67cafbd2 (root-fix build, snapshot 4a918dc1, stamp
l26-20261011T211306Z). Build: P1 fault-ledger + P2 variant-B removal +
world-fix (GetName/GetWorld/[FL-MAP]) + round-5 shim + Claude round-10
root-fix (lexical paths) + fs-throw gate. Zero exceptions in the entire
session.

## MOD VERDICTS (user eye-confirmed: "all mods seem like they work perfectly")

| Mod | Class | Evidence |
|---|---|---|
| CheatManagerEnabler | event/hook | Constructed CheatManager [0x...] + "Enabled CheatManager" at world load AND at quit — the hook fires on every ClientRestart |
| ConsoleEnabler | value/array-write | ConsoleKeys array growth written (Tilde + ConsoleKeys FNames resolved through the bridge); user confirmed console opens |
| TownfallMiniMap | sustained-tick + config-io | 1,372 widget constructions at map load (the biggest sustained UI build receipted); mod runs; "no map" = game content not unlocked (correct behavior) |
| InfiniteStamina | always-on | Mod script initializing; stamina holds (user-confirmed) |
| TownfallItemSpawner | menu (proven round-4) | Loaded; user spawned items |
| ueselftest | parity self-test | Loaded, armed (L1+R1) — user did not press the combo this session |
| zzdiag | diagnostics | T-lines with cb=23 sustained; [MAP] via [FL-MAP]; full probe ladder |

## ZERO EXCEPTIONS — the root-fix worked
The round-10 lexical-path fix (lexically_normal, no weakly_canonical)
resolved the launch-2 first-attempt failure ("filesystem error: in
weakly_canonical" for every mod). Every mod loads and runs clean.
The shim self-disables loudly (reason=no-root) — the derivation now
succeeds but the runtime-resolved mods dir isn't yet the modbridge root
path on this install; the io shim is a no-op this session, mods run
raw-io. That's the round-4 behavior — safe, correct.

## NEW CONSOLE PROOFS THIS SESSION
1. [FL-MAP] order=1 map='Level_Townfall' phase=loaded src=marshal-watch
   — the ordered map logger from IA's world-fix, working on its first
   launch. Level_Splash precedes it (config GameDefaultMap) but loads
   before the Lua layer's visibility — noted for the #4 milestone
   question (stamp-precedes-first-map stands).
2. FNAME-EQ: 'Tilde' len=5 id=0x1DAE9 + 'ConsoleKeys' + 'CheatManager'
   + 'CheatClass' — every lookup through the bridge decodes OK with
   correct indices. The ConsoleEnabler's array writes are the first
   receipted FName-array-growth on console.
3. FIVE concurrent mods + diagnostics, zero interference, zero errors.

## KNOWN ISSUE (non-blocking)
1 Hz [FL-MAP] marshal-watch = full 40K-object World scan every second =
the stutter the user felt. Fix: throttle to 10s or event-drive (rebuild
next session). Harmless diagnostic.

## WHAT'S NEXT (per the standing plan)
1. L1+R1 self-test didn't fire this session — rides the next launch.
2. Stellar Blade dump session (first console session after Townfall).
3. First-launch TU (CodeImageCache rework per GW) before Blade.
4. Stutter fix (map-watch throttle).
5. 3-restart acceptance law for the full ladder (this was pass 1).

Evidence: evidence/launch2-final-session-20261011{.log,.decoded.log}
