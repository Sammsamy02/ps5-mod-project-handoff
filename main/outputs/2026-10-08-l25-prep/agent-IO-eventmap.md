# agent-IO — event-driven [FL-MAP] logger (Claude round-10 item 2)

Deliverables (staged only; no builds, no console, no live-tree edits):
- `outputs/2026-10-08-l25-prep/round5-staging/world-fix/04-event-map-logger.patch`
- `outputs/2026-10-08-l25-prep/round5-staging/world-fix/04-event-map-logger-design.md` (full design + safety analysis)
- `outputs/2026-10-08-l25-prep/round5-staging/world-fix/syntax-receipt-UnrealInitializer-04.log` (EXIT=0)

## Recommended mechanism — Option B′: LoadMap vtable-slot hook on the VmtTickPump copy

The engine's LoadMap is a virtual on UEngine and its slot is console-proven
(round-4 log:616 "GameEngine::LoadMap address 0x3c4d460", resolved from the live
GameEngine vtable). The PLH detour is dead in this world (owned-memory refusal +
bHookLoadMap unmapped), so the patch pokes a LoadMap logger entry into the SAME
private vtable copy the accepted VmtTickPump (Claude l26 fix 4) builds — BEFORE its
single aligned vptr publish. One atomic swap arms the Tick pump AND the LoadMap
event hook together.

Why the alternatives lost:
- A (engine AddUObjectCreateListener): unlocked direct-TArray append, engine op
  never pinned/admission-verified, fires on every object creation, create-listener
  ABI unverified (the delete listener's "unchanged since 4.12" claim was the l25
  slot-4 bug). Rejected — the exact family l25 is still stabilizing.
- B (LoadMap detour): kept for W-works worlds (src=detour markers stay), dead here.
- C (per-frame cached-pointer compare): no offset-proven cheap World read exists
  (UEngine has zero member layout in this port; the only proven resolver is the
  banned FindFirstOf scan) — and it is "just throttling", which Claude rejected.
- NotifyOnNewObject: its seam (RegisterStaticConstructObjectPostCallback) sits in
  the R3-gated else branch, unregistered in this world; not revivable without the
  detour layer.

Event fidelity + coverage: the entry logs phase=pre (URL.Map) before forwarding to
the original slot word EXACTLY once, then phase=loaded with ok=<engine return>
(false = failed load — the save-load failure signature the mission chases). It
reuses IA's entire FlMap contract (order counter, s_pending, s_last_world,
world_name/sanitize/url_name) with src=vmt-loadmap, so sources never double-count.
Arms at framework start → catches Level_Splash (the map the 1 Hz watch missed).

## Dispatch-safety proof (the launch-2 regression cannot repeat)

1. funcptr_marshal_hook and VmtEngineTickDrain bodies are byte-identical under the
   patch; the dead marshal_watch stub is not called. The launch-2 kill was a
   40K-object FindFirstOf scan INSIDE the keybind-dispatch call stack — no scan
   exists anywhere in this design.
2. The entry runs ONLY inside genuine engine LoadMap calls (map transitions,
   seconds+ apart): two bounded sanitized log lines + atomic increments. Zero
   per-frame cost; zero work between keybind dispatch and its actions.
3. Original-call law: pre isolated → original forwarded once, bit-identical
   receiver/args (by-value FURL is the caller's temporary under Itanium; both TUs
   compile the same header) → post isolated → return value passed through.
4. No Lua state, no GUObjectArray walks, no dispatch mutexes; FlMap shared state is
   game-thread-confined (all LoadMap calls are engine game-thread calls).
5. Fail-closed arming: poke validation refusals are loud and leave the pump exactly
   as today (Tick-only), a DirtyCopy forbids the publish entirely; the original
   vtable is never written; teardown is the existing wholesale vptr restore.

## Verification

- patch -p1 --dry-run on a pristine candidate copy: clean (both TUs).
- One valid prospero-clang++ -fsyntax-only on the patched UnrealInitializer.cpp
  (the novel-code TU): EXIT=0, 0 errors, 6 pre-existing vendor warnings, zero
  diagnostics in patched regions. It caught and I fixed 2 real bugs (PumpCore
  namespace qualification; slot store must be void*-typed like PumpCore::arm's own
  idiom). Invocation history + 4 recipe deltas documented in the receipt.
- LuaMod.cpp hunks (FlMap event pair, bridge, trampolines, one comment) follow
  in-file proven patterns; not separately syntax-checked (one-check budget).

## Honest limits

GetThisCurrentWorld failure blanks the loaded name but keeps the url= marker;
both-sources worlds log one pair per src per load; qualified engine-internal
LoadMap calls would bypass the slot (none known in stock UE); logging only — Lua
RegisterLoadMapPreHook/PostHook delivery is NOT resurrected by this patch.
