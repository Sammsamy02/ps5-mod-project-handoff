# LAUNCH2-PREDICTIONS.md — composition-5 r3 outcomes at the tick stage

Given validated facts: Townfall eboot `UGameEngine::Tick` at linked/p_vaddr `0x33ac8c0`
(prologue byte-checked in-span), single-load-bias base `0x400000`, exec VMMAP span
`[0x400000, 0x787c000)`. Image extent used by the bridge anchor: `0xbc602c8`
(runtime_start_bridge_r3.cpp:125).

## Prediction of the derivation result (task 3)

The resolution path does NOT need the serialized ELF. Two live routes exist and both should
converge on the same runtime address:

- Route A (primary, used by default): `ps_resolve_mapped_core_v1` id 7 — UTF-16 `"causeevent="` /
  `"CAUSEEVENT "` xrefs -> unique root function (`game_loop.rs:4-10`). For a standard UE4 shipping
  build these log strings live in exactly `UGameEngine::Tick`, so the predicted result is the root
  containing them = `base + p_vaddr = 0x400000 + 0x33ac8c0 = 0x37AC8C0`, which is inside the exec
  span `[0x400000,0x787c000)` and inside the anchored image extent. Note the runtime address is
  `p_vaddr + base`; the record "Tick=0x33ac8c0" is a LINKED address, and the in-span check must
  have been against `0x37ac8c0` runtime (or equivalently the exec LOAD's p_vaddr window). If the
  segment correspondence records instead recorded `0x33ac8c0` as a RUNTIME address, it would sit
  BELOW `0x400000`, outside the exec span — re-check that record; the derivation above is what the
  code computes (`OwnedSerializedElfSource.cpp:62-64` translate semantics: `runtime_base_+(a-low_)`,
  `low_` = lowest flagged PT_LOAD vaddr, normally 0 for this layout).
- Route B (fallback, also computed): live `UGameEngine` vtable entry for `Tick` — same `0x37AC8C0`
  for an unmodified vtable.
- Route C (serialized ELF witness): double-dormant — no bytes (`NativeLoaderContext.cpp:4-6` via
  bridge `:277-278`) and `OriginalOnly` policy (`NativeLoaderContext.hpp:13`,
  `lmf_to_native_loader_facts.hpp:68-69`), plus it only triggers on scan status NoMatch/3
  (`OwnedMappedScanSource.cpp:18`). Rung-1 therefore does NOT "fail at detour setup" because of the
  dormant ELF route.

Both A and B are logged at staged `UnrealInitializer.cpp:1318`
("GameEngine::Tick address (vtable: ...; scan: ...)"); disagreement only warns (:1321-1323).

## Outcome table for launch-2 at the tick stage (task 5)

| # | Scenario | Tick derivation result | Detour | Observable (exact) |
|---|----------|------------------------|--------|--------------------|
| 1 | Scan unique + vtable agree (expected) | `0x37AC8C0` (scan, preferred by `EngineTickResolveMethod=Scan`, UnrealInitializer.hpp:146) | patched at `0x37AC8C0` | log `GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)`; ack line `engine-ready` then `mods-started`; HookedEngineTick + QueryHarnessDispatch registered (staged :1383-1397); query-harness tick counter advances on every real frame |
| 2 | Scan NoMatch(3), vtable good | vtable `0x37AC8C0` (staged :1360-1364 fallback) | patched at vtable address | log `Scan failed, falling back to vtable address for GameEngine::Tick`; `mods-started`; ticks flow |
| 3 | Scan NoMatch(3) AND vtable missing (no `"Tick"` in `VTableLayoutMap` / no GameEngine object -> throws earlier at staged :1264-1269) | both null | none installed | `EnsureHooked` false -> `ERROR_ID` -> throw "Native game-thread initializer callback registration failed" (staged :1384-1387) -> UE4SSProgram.cpp:693-714 catch -> ue4ss_backend.cpp:66-70 rethrow -> ack `failed` code 1 with that detail (runtime_owner.cpp:112-123, :171-175). LAST ack = `failed`; `engine-ready`/`mods-started` never emitted. LOUD |
| 4 | Scan Ambiguous(4) (>=2 root functions reference causeevent strings) | scan null; ELF route NOT attempted (needs status 3, OwnedMappedScanSource.cpp:18) | vtable fallback as in #2, else #3 | warning-free scan logs with slot-10 absent; `mods-started` if vtable rescued; else `failed` ack. NOT silent |
| 5 | PLH structurally refuses the resolved address (undecodable/too-small/unprotectable; x64Detour.cpp:354-357, 413-427, 502-503) | address found but unhookable | NOT written (or rolled back byte-exact, :513-541) | same as #3: `failed` ack, error detail from `AddCallback` failure path (DetourInstance.hpp:284-288). LOUD |
| 6 | Wrong-but-plausible address (weak/unlucky unique match or stale vtable slot), hot code | plausible non-Tick address | patched; registration succeeds | `mods-started` FIRES, then the game crashes on the next execution of the patched bytes. No `stopped` ack; console crash report. The runtime cannot distinguish this from #1 before the crash. RISK case |
| 7 | Wrong-but-plausible address, cold/never-executed code | plausible non-Tick address | patched; registration succeeds | `mods-started` fires, game runs normally, but HookedEngineTick/QueryHarness dispatch never run: no tick observations, status parked at `mods-started`. This is the ONLY "stuck at mods-started" shape, and it is a wrong-address outcome, not a setup failure |
| 8 | Required scan slots never satisfied (e.g. g_uobject_array/fname_ctor/gmalloc/SCO/engine_version all missing; `MappedRequiredSlots=0x5d`) | n/a | none | `ps_scan` returns false every attempt -> staged :628-629 throws "PS scan timed out" -> ack `failed` detail "PS scan timed out". LOUD |
| 9 | Bridge/context failures BEFORE the tick stage (anchor/conversion/context) | n/a | none | `MRS_ERR_CONVERSION`/`MRS_ERR_CONTEXT`/`MRS_ERR_START` returns (runtime_start_bridge.h:36-41) + `diagnostic_event` tags `lmf_vmentry_cap`/`lmf_vm_scan_cap`/`lmf_linkmap_cap`/`lmf_provider_code`/`lmf_identity_source` (runtime_start_bridge_r3.cpp:262-267). Not tick-stage |

## Can r3 distinguish "right Tick" from "mods-started without ticks"?

Not from the ack stream alone: `mods-started` is emitted by the `on_engine_initialized` callback
(ue4ss_backend.cpp:31-45) which runs after registration succeeded — it says "detour installed",
not "detour installed at the true Tick". Tick LIVENESS is only observable via the query-harness
tick dispatch counter (staged :215-219, :1391-1397) / `HookedEngineTick` side effects. Recommended
launch-2 read: after `mods-started`, require (a) query-harness tick counter strictly increasing and
(b) the logged scan/vtable Tick addresses equal to `0x37ac8c0`; the pair separates outcome #1 from
#6/#7.
