# REPORT — worker-executedrain-trace (2026-10-05)

## What was done

Traced `ExecuteInGameThread` end-to-end through the ACTUAL composed launch-10 units and resolved
the mod-load-readiness vs pc-ps5-comparison disagreement. Verdict: **CONDITIONAL** (full chain +
decision table in `DRAIN-TRACE.md`; every hop pinned file:line in `CLAIM.json`).

Headline answers:

1. **Q2 (the decisive question): the drain requires ONLY the EngineTick closure.** The queued
   artifact is a Lua registry ref; the drain is `process_simple_actions` (notify-cancel
   LuaMod.cpp:4113) -> `Lua::call_function` -> `lua_pcall` (LuaMadeSimple.cpp:971). ProcessEvent /
   CallFunction appear nowhere in enqueue->drain. ProcessEvent is needed only by the OTHER method
   branch (never taken — settings pin `DefaultExecuteInGameThreadMethod=EngineTick`, ini line 8,
   no `[Hooks]` override, `HookEngineTick` default true) and by the mod's own `:` UFunction calls
   ABOVE the drain (menu UI, and the actual spawn `BeginDeferredActorSpawnFromClass`).
2. **Q3: the armed build takes the Scan-branch vtable fallback** (default resolve method Scan,
   scan structurally dead on the armed path — slot 10 deliberately unseeded), and **the
   registration path DOES need exec-page-readable content — flagged as instructed**: the vtable
   macro's thunk check Zydis-reads the target's first bytes (ASMHelper.cpp:9-22 via
   UnrealVirtualBaseVC.hpp:133; LoadMap at final-units:1819 walks FIRST), and PolyHook's
   `hook()` disassembles up to 100 bytes (x64Detour.cpp:325) before the FLAG-1 RWX patch write.
   The walk-site RX guard mitigation is NOT in launch10-final-a (separate walkguard candidate).
3. **Q4: target confirmed at tier-1 without Ghidra.** Capstone decode of the sha-pinned retained
   eboot (`3b6cfad4…`) at linked vaddr `0x2fac8c0` shows a valid x86-64 function prologue;
   runtime `0x33ac8c0 = 0x400000 + 0x2fac8c0` reconciles the layout profile, the CP2 pin, and
   oracle F29's expected log value. Ghidra 12.1.4 full analysis was NOT run (limits below).
4. **Q5 verdict: CONDITIONAL**, on C1 (F29 exec-page reads survive), C2 (FLAG-1 detour write
   accepted — fails LOUD via the named throw at final-units:1916), C3 (first-tick
   game-thread publication, final-units:172 — silently gates the drain), and, for menu-open and
   actual spawn, C4 (UFunction-call machinery works — the known adapter gap; DIFF-1
   funcptr-marshal fix was proven NOT composable into launch-10 by worker-launch10-r3-armed).
   The exact launch-10 log lines that decide and that prove the first successful drain are
   listed at the end of DRAIN-TRACE.md (the load-bearing one: `[Lua] [TownfallItemSpawner]
   Menu created.` at main.lua:643, which executes inside the drained F7 closure).

The two disagreeing workers were each right at a different layer: the pump IS registered and the
hook defaults DO hold (source-verified), and the path DOES sit on unproven exec-page behavior —
just one hop lower (pump arm), plus the UFunction layer one hop higher (menu/spawn execution).

## Honest limits

- **No runtime evidence.** Everything is source read at receipt-pinned shas plus static byte
  decode of the retained image. Only the real launch-10 sinks (UE4SS.log, runtime-status) can
  move CONDITIONAL to READY/BLOCKED; the C1/C2 verdicts (IGW `W_WORKS`/`W_FAILS_R_WORKS`) are
  still missing — the exec-write probe never attempted a row (`rows_attempted:0`).
- **Ghidra full analysis NOT run.** The 194MB headless analyze (10-40 min) was skipped in favor
  of the tier-1 capstone decode, which answers the exact question asked (target exists at the
  expected address class). What remains UNKNOWN without it: semantic identity of the function
  body beyond the profile's `validated-static` survey and the oracle's address expectation. A
  later Ghidra pass could corroborate (call-graph shape near the pinned address, DeltaSeconds
  handling) but cannot decide C1-C4 — those are runtime properties.
- **"READY" claims remain source-level.** `TickInternal.is_ready()`, detour fire, and the Lua
  call path are verified as code, not as behavior; no in-game tick dispatch has ever been
  observed on this composition lineage (launch-9 refused upstream of the initializer at the PLH
  lease read + facts adapter; Item Spawner activation has never been observed in-game).
- **Unit provenance confidence is high but inherited.** The luamod unit identification comes from
  the launch-10 prebuild run receipt (`run-a/receipt.json`, 368 units); the final-a controller
  edition (EDITION.json) does not change the luamod/program/hook units vs the prebuild (its 4
  edits touch lmf/initializer/diagnostic/header). If the controller's final one-pass run shifts
  unit pins, re-verify `luamod` = `0f0697a3…` and initializer = `c3a1c9eb…` before reusing these
  line numbers. Final-units initializer line numbers were verified directly against the
  c3a1c9eb file; the readpath-guard staging line numbers quoted by the acceptance oracle (F29/F30)
  are the same block shifted -69.
- **Keybind/input path is a precondition, not traced.** F7 must actually fire (PS5 input port /
  keybind lifetime lineage) for the enqueue to happen; this trace starts at main.lua:694 and does
  not independently verify the keybind delivery chain.
- **The exact null-render form of `scan: 0x0`** in the F29 line is unpinned (oracle says the same);
  match on the vtable value and the resolve line, not on the null rendering.
- **The spawn's own success additionally depends on the item database / cooked pickup classes /
  ground-trace logic in main.lua** (load-a-save gating, `LoadAsset` behavior on this port) — out
  of scope; the trace covers only the native execution substrate.
- **No files outside this worker directory were written.** No console contact, no dispatches, no
  replays; the retained eboot and all staged inputs were read-only.

## Files

- `CLAIM.json` — every hop with file:line pins, unit provenance, answers q1-q5, deciding runtime
  evidence, cross-checks.
- `DRAIN-TRACE.md` — the end-to-end diagram (F7 press -> binding -> queue -> pump arm -> detour
  fire -> drain -> lua_pcall -> menu/spawn), the Q2 process-event answer, the Q3 resolve-branch +
  exec-read flag, the Q4 address evidence, and the CONDITIONAL verdict table with the exact
  launch-10 log lines.
- `REPORT.md` — this file.
