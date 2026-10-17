# FULL STATUS FOR CLAUDE — 2026-10-09 late evening — everything, copy-paste ready

## THE HEADLINE
The readfix build (PRX b9ac61fe) just ran on console and achieved the
healthiest state ever: type scan PASSED (core-ready=true, 0 refusals, 0
missing types — the Z15 fault-check removal from read paths fixed the scan),
the pool decoder is VERIFIED AND LIVE ([FN] pool-decoder verified
(ByteProperty)), variant C's engine-call registration WORKED (the engine's
own AddUObjectDeleteListener called and returned in 0ms via
admission-published pointer — the first successful engine-call registration),
the listener receives engine callbacks (NotifyUObjectDeleted firing), the
event loop starts, and the runtime SITS THERE — alive, stable, no uninstall,
no exceptions, no crashes.

## THE ONE REMAINING BLOCKER
**Mods never start.** runtime-status: activated + engine-ready + mods-started=0.
The event loop runs ("Event loop start" logged) but on_program_start never
fires — no "Starting mods", no Lua mod start, no keybinds (the user's presses
do nothing, not even lag — the mod's scan code never runs). The runtime stays
in this state indefinitely (listener callbacks continue arriving).

## WHAT MOD-START NEEDS (per agent G's trace, l25-era)
The chain: event loop jthread -> update() -> on_program_start() ->
install_lua_mods -> "[l22] Lua mod started". The gate between update() and
on_program_start was NOT dependent on hooks or the game thread. BUT: the
R3 marshal carriers ALL failed to arm this session ("marshal carrier
candidate not found: Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation
(not interned, or class not constructed yet)") — on previous builds the
UserWidget:Tick carrier DID arm. If the update-loop or the start-mods path
now depends on a carrier (or on the previously-present LoadMap hook firing),
that would explain the stall. ALSO NEW: EC's loud refusal fired once:
"Error: [StaticFindObject] long-name part '/Script/Engine' n[ot in pool]"
— the StaticFindObject long-name arm ran during startup (the searcher pool /
Hook StaticConstructObject path) and the /Script/Engine PACKAGE name did not
resolve via FNAME_Find. If mod enumeration or the update path calls
StaticFindObject and the refusal blocks it, that's the gate.

## TODAY'S COMPLETE ARC (for context)
1. ToString solved: in-port pool decoder (no engine call, no admission) —
   VERIFIED on console, resolves all names incl. Number suffixes, OOB-guarded.
2. The Z15 gating side-effect chain solved: fault-check removed from read
   paths; copyin cleanup sequenced after all admission; Phase 5/6 armored
   with catch-and-log; image-identity pins registered (tostring/add/remove).
3. Variant C proven on console: engine AddUObjectDeleteListener called via
   admission-published pointer, listener registered, engine calls it back.
4. The log writer: message-atomic, narrow-string fixes — full readable logs.
5. Type scan: passes with 0 refusals for the first time since the decoder
   landed (the read-path fix).
6. Previous blockers all verified fixed: no silent uninstall, no phantom
   copyin faults, no admission exceptions.

## THE EVIDENCE FILES
- evidence/readfix/session-full.log — the complete session (742 lines)
- The runtime-status: mods-started=0 (the gate)
- The R3 carrier failures + the StaticFindObject refusal are the two leads

## QUESTIONS FOR CLAUDE
1. What gates on_program_start in this port's update() loop? Is it the
   R3 marshal arming (all carriers failed this session vs. previous
   sessions where UserWidget:Tick armed)?
2. Is the StaticFindObject long-name refusal (/Script/Engine package part
   not in pool via FNAME_Find) blocking a startup path that previously
   silently fell through EC's fix? The refusal is loud now — was the old
   behavior (throw "Name wasn't long") masking a fallback that mod start
   needed?
3. Should the next build instrument the update() loop directly (a marker
   per update tick + the on_program_start gate condition) to see exactly
   where it waits?

## CURRENT CONSOLE STATE
PRX b9ac61fe resident, game closed by user, settings original (cache-off),
ploader armed, klog tap running. The runtime is proven stable — the game
runs indefinitely with the plugin active, listeners firing, no crashes.
