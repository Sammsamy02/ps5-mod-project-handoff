# Launch-10 candidate — spawn-capability assessment (functional blocker record)

Verdict: **LOAD-CAPABLE, NOT SPAWN-CAPABLE.** Recorded per user directive
2026-10-05; no user launch is needed to confirm this — it is established from
source (worker-executedrain-trace, every hop file:line-pinned).

## The chain that works at launch-10

Composition → conversion (geometry fix) → context → initializer seeds (GUA,
GMalloc V1/V2, FName pool + ctor bind, SCO) → engine-ready → Lua mod loading →
`[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens
the menu.` Every hop is present in the composed units with zero absent Lua APIs
(worker-mod-load-readiness).

## The blocker (source-established)

The Item Spawner's real work is deferred through ExecuteInGameThread
(main.lua:695) into `m_engine_tick_actions`, drained by
`process_simple_actions` (LuaMod.cpp:4488→4113) — pure Lua VM calls, no
ProcessEvent needed IN the drain. But the drain only runs if the EngineTick
pump is armed, and the armed build's pump arm path (scan dead, slot 10
unseeded → vtable fallback, UnrealInitializer final-units:1892) requires an
instruction read of exec-only text (the vtable-macro Zydis thunk-check at
ASMHelper.cpp:9-22, then PolyHook disassembly x64Detour.cpp:325). The game's
text is exec-only (prot=4, no R bit) — those reads are the proven igf6
hardware-blocked/crash class.

With the walkguard fold-0 guard now in the build: the read is REFUSED LOUDLY
(walk-refusal string) instead of crashing — the NO-CRASH invariant holds, the
mod loads and logs, but the pump is not armed. Consequence: OPTIONS/F7 opens
nothing (`Menu created.` never prints — it executes inside the drained
closure); no spawn path runs. First-OPTIONS-press silence after a successful
load is EXPECTED for this candidate, not a drain stall.

## The working paths (owned, in order)

1. **Launch-11 R3 FuncPtr-marshal (primary)**: DIFF-1's SetFuncPtr marshal
   arms the pump with ZERO exec-page writes; variant-B rebase proven
   (byte-identical objects), admission path proven end-to-end by a real
   scratch compose (worker-luamod-l11-dryrun: GO). Remaining: the
   drain-consumer linkage decision (their VERDICT.md §6) + integration.
2. **D-1 vtable-swap route (secondary, unproven)**: swap the Tick entry in
   the RELRO data vtable (R→RW flip on a DATA page via mprotect) — no exec
   writes; requires the D-1 flip proof. RESOLVED by worker-igw-unlock-trace:
   launch-9's all-skip was the compile-time design-confirmation flag
   (TRANSITIONS.json sets no `confirmed:1` on any row — exec_write_probe.c:166,
   a deliberate safety contract, not a runtime gate), so no launch can produce
   the IGW/D-1 verdict without a probe rebuild; the 2-plugin ini disables the
   xwp regardless. The minimal launch-11 proposal exists (confirm row 0 only,
   the sole verdict-deciding X→RX row; machinery untouched, new build id) and
   would pair with a probe-loading ini at a user-coordinated staging.
   Launch-10's exec-page story rests on its own F29/C1/C2 log evidence.

## What launch-10 still buys (its acceptance goal)

The armed seeds chain, conversion+geometry fix, seed diagnostics, mod-load
success line, and the loud walk-refusal itself — the GOOD-class oracle
outcome. Visible spawn is NOT a launch-10 verdict input (oracle G4).
