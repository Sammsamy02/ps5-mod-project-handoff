# BISECT STATUS — for Claude review (2026-10-11, user away)

## CONFIRMED FINDINGS

### Stutter: SOLVED — the minimap
Round-4 + all mods except minimap = no stutter.
The minimap's 50ms tick is the stutter source.
Root cause TBD (likely the tick's engine calls, not the GetWorld fallback
which is an outer-chain pointer walk, not a scan — and doesn't exist on
round-4 anyway).

### F7 spawner: UNSOLVED — mod interaction (not build regression)
Round-4 (the proven binary) with all 5 ladder mods = F7 fails.
Round-4 with spawner + zzdiag + ueselftest only = F7 works.
=> The blocker is in {CheatManagerEnabler, ConsoleEnabler, InfiniteStamina}.

### GetWorld fallback: NOT the cause (Claude item 2 verified)
The IK 02b fallback is a 2-3 hop OuterPrivate pointer walk (AActor.cpp
semantics), NOT an object-array scan. O(depth), not O(40K).
Additionally: round-4 has NO fallback and still fails with all mods.

## BISECT LADDER (staged, one launch each, user returns to test)

| Step | Config | Loadlist file | Status |
|---|---|---|---|
| 1 | +CheatManagerEnabler | bisect-addCM-loadlist.txt | STAGED (deployed on console) |
| 2 | +ConsoleEnabler | bisect-addCE-loadlist.txt | Prepared |
| 3 | +InfiniteStamina | bisect-addST-loadlist.txt | Prepared |
| 4 | +CM+CE pair | bisect-addCMCE-loadlist.txt | Prepared |

## INPUT DISPATCH CHAIN (traced)
PS5PadNative (borrowed handle) → PS5PadSource::poll() → 
Input::Handler::process_event() → LuaMod keybind callback lambda →
Lua callback. The console-state gate on allow_input is COMMENTED OUT
(upstream note: "doesn't work properly"). The allow parameter IS still
checked in poll() — default state investigation needed if the bisect
doesn't resolve it.

## TOP THEORIES (ranked)
1. CheatManagerEnabler: the ClientRestart UFunction hook (Func-swap)
   interferes with the R3 carrier dispatch (the same game-thread pipeline
   that delivers keybinds). The CheatManager object also changes UE's
   input processing.
2. ConsoleEnabler: the ConsoleKeys TArray growth write (the deepest write
   in the pool per GC) corrupts adjacent memory — potentially the Handler's
   subscribed-keys or the pad source's binding table.
3. InfiniteStamina: its 1.5s LoopAsync calls UEHelpers.GetPlayer() which
   does FindFirstOf (full object scan) on the game thread — could starve
   the keybind dispatch queue. (But the zzdiag T-cycle also does
   FindFirstOf every 6s and works fine — weaker theory.)
4. Mutex contention: multiple mods' callbacks holding the Lua state mutex
   when the keybind tries to acquire it.

## WHAT'S READY FOR THE USER'S RETURN
- Step 1 (CheatManagerEnabler) is deployed and staged — just launch and press F7
- Steps 2-4 are one command each (loadlist swap + prep)
- All on round-4 (59b0528b) which is the proven binary — zero build variables

## CLAUDE ITEMS ADDRESSED
1. ✅ d90563f5 log harvested (0 F7 presses — user didn't press in that session)
2. ✅ GetWorld fallback analyzed (pointer walk, not scan; not the cause)
3. ✅ Round-4 + all mods launched (same failure — mod interaction confirmed)

## DEEP ANALYSIS RESULTS (agents landed while user away)

### IN: Minimap stutter root cause — PORT-LEVEL, not mod-level
The 1-second stutter is the minimap's `tickN%20==0` burst:
- FindAllOf("TownfallEnemyCharacter") — full ~40K object scan
- 23x GetNumItemsOfTypeInInventory — reflected calls
- All landing on the same 1-second boundary, on the game thread

WHY the scans are expensive: FUObjectHashTables is UNAVAILABLE on PS5,
forcing the GUObjectArray fallback (walk all objects, FName compare each).
~200-500K compares per scan, x4/s.

THE FIX (port-side, benefits every mod):
1. Bind FUObjectHashTables (the engine's hash-based object lookup)
2. Or: FindAllOf cache with NotifyOnNewObject invalidation
NOT: reduce tick rate or batch widgets (the scans are the problem)

Diagnostic ready: zzminimapdiag-v1 (rides zzdiag slot, 3 KB).

### IP: Shim root wiring — ONE 18-LINE PATCH
Root cause: setup_mod_directory_path pins RELATIVE "ue4ss/Mods"; the shim's
lexical derivation strips it to empty string -> self-disable (reason=no-root).

Fix: use m_native_context.configured_working_directory (the absolute
MODBRIDGE_MOD_ROOT, already carried from module entry) + "ue4ss/Mods".
- Patch: round5-staging/IP-shim-root/01-ue4ssprogram-shim-root.patch
- Syntax-verified, Rule 10 compliant, all 8 consumers audited
- Next launch: [SHIM] path-resolver installed (not no-root) + write-area markers


### IO: Event-driven map logger — LoadMap vtable hook (Option B')
Mechanism: LoadMap slot (console-proven 0x3c4d460) poked into the SAME
private vtable copy the VmtTickPump already builds — one atomic swap arms
both. phase=pre before the original call, phase=loaded after. Arms at
framework start -> catches Level_Splash.

WHY safe (launch-2 regression cannot repeat):
- Zero per-frame cost (only fires on map transitions)
- No scans, no Lua state, no dispatch mutexes
- Original vtable never written (the copy is published atomically)
- Fail-closed arming (RefusedClean -> Tick only)
- Patch: round5-staging/world-fix/04-event-map-logger.patch (352 lines, staged)

