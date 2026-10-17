# Status summary for Claude — 2026-10-09 evening (copy-paste ready)

## Where we stand
The unmodified PC Lua mod (TownfallItemSpawner) runs on PS5 through the full
pipeline: admission → init → mod start → controller input (L3+R3→F7) →
game-thread dispatch (R3 marshal) → Lua execution. The engine calls our
registered delete-listeners during save-load GC without crashing. The one
remaining blocker for the mod menu is FName→string conversion (ToString).

## What today's console runs proved (5 launch cycles)
1. **Crash family SOLVED** (since l25): registration with corrected offsets
   survives save-load GC storms (listener calls=35,419 in one session).
2. **The ToString dependency chain, isolated by A/B controls:**
   - Build 5ea2c845 (verified-query fix, NO engine bind): in-world with save
     loaded, FindFirstOf returns nil for EVERY class. The [FFO] query-fallback
     markers fire (the fast-path index mismatch is real) but the string-compare
     fallback ALSO fails — because it compares via ToString, which returns
     empty (the port's fname_tostring is unbound by design; its Kismet
     fallback never executes on console).
   - Build 10b11e43 (+ EI's engine ToStringInternal bind): the bind's
     admission REFUSED — root cause found: our Z15 copyin-gating patch
     releases the fault-check owner gate early; bridge_copyin_range_faulted()
     fails CLOSED when the gate is released, so every post-release admission
     gate phantom-fails with "constructor prefix has recorded copyin fault".
     Slot 2 passed only because it ran before the release.
   - Build a3c87be9 (+ EP sequencing fix: complete() moved after Phase 6):
     the phantom-fault is gone, but the bind now refuses one gate earlier on
     an image-identity precheck ("constructor startup image identity/base
     unavailable or mismatched") — the signature gate trusts registered seed
     addresses (0x63D590) but rejects the unregistered 0x63FA00. AND the
     runtime self-uninstalls: a raw exception between the L15 summary and
     Phase 5/6 admission skips all logging (no seed_refused line) and unwinds
     into "Stopping mod for uninstall". Both under fix.

## The two fixes now being built (per your directives)
1. **Pool-decoder ToString (your directive 2)**: ComparisonIndex →
   block/offset → entry header (len + wide flag) → chars + '_' + (Number-1).
   No engine call, no admission gate, any thread. Uses the exact decoder the
   L14/L15 slot-2 admission proves on every console run (pool words →
   "ByteProperty" verified). This replaces the engine bind entirely — the
   whole admission/identity/refusal layer drops out of the ToString path.
2. **Phase 5/6 catch-and-log armor**: both admission phases wrapped so any
   exception logs before unwinding — the silent uninstall can't recur without
   naming itself.

## Expected result when the pool-decoder build lands
FindFirstOf resolves (fast path OR fallback — both arms work with real
ToString) → the mod's scan finds ItemDatabaseSubsystem → menu opens on the 6
hardcoded extraPickupClasses (no ToString needed per your analysis) →
spawn works too (the decoder provides the ItemId str()). The acceptance
ladder (scan→menu→spawn, automated in the harvest tool) fires
"ACCEPTANCE: MOD MENU UP".

## Also done per your earlier directives
- kstuff_no_pause=PPSA33286 added to ShadowMount config (backup saved);
  NOTE: the console's sleep killed the ShadowMountPlus daemon once
  (CE-105773-3 on launch; restarting the plugin fixed it) — sleep also rolls
  back PRX uploads twice now (re-upload verified each time; we check on every
  wake).
- fast_cycle.sh: one-command prep/watch/verdict/next tooling (offline-tested).
- Single-console-actor rule in force (root only; all agents Mac-only).
- Hot-reload confirmed unavailable on PS5 (both settings throw; no io lib in
  the sandbox) — launch cycles remain the boundary for Lua changes.
- Parallel no-bind control candidate designed + patched (EO) — mooted by
  today's control run which already isolated ToString as the blocker.
- Full state + provenance: STATE-L26-WAVE.md, agent reports A-EQ (30+),
  l26 review packet assembled (attachments-l26/).

## Open items after the menu
- Spawn-path marshaling (CI's amber table: FHitResult, struct tables, the
  db pair's param types — the reflection dump CI recommended once reads work)
- Variant C production shape (integrated, gated on the current admission
  armor; its engine-Add targets are pinned and prologue-verified offline)
- The GetPackageNameFromLongName StaticFindObject bug (EC's fix staged)
- Multi-mod generality tests (12 candidates staged, escalation plan M1-M3)
