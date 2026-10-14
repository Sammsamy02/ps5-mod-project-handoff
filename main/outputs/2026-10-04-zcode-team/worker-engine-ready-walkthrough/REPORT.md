# REPORT — worker-engine-ready-walkthrough (2026-10-04)

Scope: everything between "context created successfully" and the `engine-ready` ack in the composition-5 (r9) composed runtime. Deliverables in this directory: `CLAIM.json` (written first), `ENGINE-READY-PATH.md` (main artifact), `REPORT.md` (this file, last). Proposal/review only; no source, staged-root, or build edits; no console contact; no nested subagents. ~45 tool calls, all reads plus the three writes.

## What was done

1. Established the authoritative source set instead of trusting the task's line hints blindly:
   - `runtime_owner.cpp` = `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp` (204 lines; `:173` matches the task hint). The shorter 107-line copy under `native-coherent-runtime-1/source/bootstrap/` is an older variant.
   - `ue4ss_backend.cpp` = the composition-3 bootstrap-stage copy that `build_runtime_composition5.py:163` actually links (`:41-45` = engine-ready then conditional mods-started). Verified its diff vs the older `outputs/ps5-mod-bridge` copy is only the 4-arg `PrepareActivatedUE4SSRuntime` signature; the ctor/ack-chain lines are identical.
   - `UE4SSProgram.cpp` = `native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp`; re-hashed (`dfdd5014…`) and matched against BOTH `frozen-source-hashes.json` and `program.source_sha256` in `ue4ss-native-supplier-composition-4/run-3/receipt.json` — the object composition-5 reuses. This makes line citations binding for the shipped binary, not just the tree.
   - `UnrealInitializer.cpp` = the composition-5 replacement unit `worker-query-s2-arm/staging/UnrealInitializer.cpp` (1627 lines); diffed against the port base and re-derived all hop line numbers in the staged file (+23-ish shift, plus QueryHarness tick dispatch and layout-gate arming).
2. Walked the full hop chain with per-step inputs, filesystem touches, deadlines, and exact throw strings (ENGINE-READY-PATH.md §1–§4): `start()` → `initialize()` → `Backend::initialize` → `UE4SSProgram::init` guards (`:600-618`) → `setup_unreal` (`:930-1321`) → staged `UnrealInitializer::Initialize/PreInitialize/ScanGame/VerifyFNameConstructor/PostInitialize` → `setup_unreal_properties` (`:1702`) → `share_lua_functions` (`:1323`) → ack block (`:654-666`) → BACKEND chain (`:33-46`) → OWNER emits (`:171-173`).
3. Audited every directory-derivation consumer (`m_legacy_root_directory`, `m_game_executable_directory`, `m_working_directory`, plus exe path) under active `mod_source` (§5).
4. Reconciled the loader-thread main.lua claim against engine-ready ordering and pinned what the Item Spawner F7 (OPTIONS) handler actually needs (§6–§7).

## Key findings

- **No surprise blocks remain between context-created and engine-ready.** With `mod_source` active, the entire init-thread sequence consumes only: the retained module snapshot (scan views), `scanner_source`, logical `ue4ss/` keys (settings, layouts, signature scripts, mods), the pad input source, settings flags, and the 5-minute StartupWait deadline. No on-path step reads the game-exe directory derivations; `CachePath` is dead config (no consumer in the linked UnrealInitializer); the VTableLayout.ini disk read is `#ifndef __PROSPERO__`.
- **game_executable_path's only hard gate is `ValidateNativeProgramContext`** (absolute, normalized, has filename, is an existing regular file, `file_size` OK — `NPC:85-100`), re-touched once by a constructor diagnostic `file_size` (`PROG:427`). If it becomes an explicit configured root (worker-configured-roots-design), scan/properties/init behavior is unchanged; a directory-style root is rejected before any init work, and only mod-runtime Lua helpers (`IterateGameDirectories`, `CreateLogicModsDirectory`, LogicMods reconstruction) plus the SDK generator would change meaning.
- **The only deadlines on the path:** StartupWait 5 minutes (exact throws "Unreal initialization cancelled" / "Unreal initialization startup deadline exceeded", `StartupWaitControl.hpp:20/26/102`) and `SecondsToScanBeforeGivingUp` (settings default coerced 30→120) → "PS scan timed out". Everything else is unbounded but single-pass.
- **CORRECTION to worker-lua-bootstrap-audit C1 (high confidence, hash-backed):** main.lua does NOT run on the loader thread and NOT before the init thread in composition-5. `start_native_mods` instantiates `start_mods<CppMod>` whose native branch returns without starting Lua mods (`PROG:1946-1956`'s `if constexpr (… LuaMod)` guard); the only reachable `start_lua_mods()` is `on_program_start` on the event-loop thread (`PROG:1488` via `:673`/`:1514`). True order: `engine-ready` → `mods-started` → event loop starts → main.lua. The audit's C2 (ack write site) and C3 (enablement from snapshot) stand; its C5 "FindFirstOf silently nil at load time" scenario does not apply — GUObjectArray is bound on the init thread (staged `UI:895`) before engine-ready, so by the time main.lua registers/uses FindFirstOf the array is live. Practical consequence for e2e observers: `mods-started` certifies inventory, not script execution, and a failed main.lua leaves status at `mods-started` with only UE4SS.log error lines ("Exception during mod startup: …", `LuaMod.cpp:6478-6480`) — no negative status line.
- **Failure-mode inventory with exact strings** (§4) includes two observer traps: the required-objects throw has an intentionally empty message (`UI:1485`, after logging "Fatal error! The following objects were never constructed:"), and a `StartupCancelled` arriving without a stop request surfaces as `failed(1,"Native runtime init returned without a stop request")` (`OWNER:106-108`).

## Honest limitations

- Static analysis only; no runtime latency measurement, no disassembly of the composed object (linkage proven via receipt source hashes instead).
- ps_scan internals over the mapped snapshot are treated as a bounded black box (wall-clock bounds verified, internals not).
- `startup_mod_names()` content depends on the staged activation snapshot; emptiness would silently mean no main.lua and no OPTIONS — not verifiable from source alone.
- Thread-scheduling deltas (start→engine-ready) and the exact loop cadence of the 5 ms event loop are unmeasured.

## Files

- `CLAIM.json` — six evidence-backed claims (E1–E6) with confidence and risks.
- `ENGINE-READY-PATH.md` — source legend + linkage proof, precondition table, ordered step table (13 hops), consolidated inputs table, 17-row failure-mode table with exact strings, directory-derivation verdict, ordering reconciliation + F7 needs table, and the 14-point OPTIONS-readiness checklist.
