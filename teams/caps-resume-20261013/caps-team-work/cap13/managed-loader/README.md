# cap13/managed-loader — staged source for the managed DLL mod route

Agent JI (ZCode), 2026-10-13. **SOURCE STAGE ONLY — nothing here has been
compiled, linked, or run.** Mac-only work; no console contact; no main-tree or
cap10 edits. Design rationale: `../DESIGN/managed-dll-route.md` (read it
first). Composes on top of cap10; JG owns PE analysis (`pe_analyzer` facts
record, design section 2a), JH owns the native loader hooks (ready-callback /
tick entries, design section 6d).

## Files

| File | Role |
|---|---|
| `../DESIGN/managed-dll-route.md` | The design: real-mod landscape, discovery, runtime integration, honest gaps, minimal path, support matrix |
| `abi/MbmManagedAbi.h` | Frozen v1 managed<->native ABI (7 symbols), status codes, buffer contract |
| `managed/Mbm.ModSdk.cs` | Mod-facing SDK: `MbmMod` base, `[MbmMod]`, `ModHost` facade, `__Internal` P/Invokes. mscorlib-only |
| `managed/ModLoader.cs` | Managed loader: binds admitted assemblies by name, reflection discovery, lifecycle dispatch, per-mod exception isolation. mscorlib-only |
| `managed/HelloMod.cs` | Labeled minimal fixture mod (OUR fixture — never a "real mod" claim) |
| `native/MbmManagedBridge.hpp/.cpp` | Resident Mono host bridge: init, probe rung 1, image admission, provider, tick, shutdown |
| `native/ManagedLifecycle.hpp` | Phase machine + framework→lifecycle policy table (what is active vs gated vs refused, and why) |
| `native/MbmLoaderImage.hpp` | FAIL-CLOSED placeholder for hash-pinned ModSdk/ModLoader image bytes (regeneration recipe inside) |

## Validation ladder (Mac rungs before any integration claim)

- **M1** ResidentMonoOwner in `MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION` mode:
  boot + cap10 Run/throw/recovery passes *inside* the resident runtime; runtime
  STAYS UP; explicit shutdown passes. New receipt class (cap10's owner is
  one-shot).
- **M2** Compile ModSdk + ModLoader with the cap10 recipe; mscorlib-only
  preflight (System.Core/System.dll refs must FAIL); regenerate
  `MbmLoaderImage.hpp`; rung-1 + loader boot; settle
  GetAssemblies-name-matching vs Assembly.Load(byte[]) (design section 8.1).
- **M3** HelloMod through the whole path: OnLoad/OnTick/OnUnload, engine mock
  surface, per-mod exception isolation (throwing fixture disabled after 3),
  all refusal classes exercised with precise receipts.

## Integration review points (for the integrator, staged not applied)

1. **Resident owner delta** — cap10's `MonoOwner` is probe-shaped (cleans up
   before returning). The bridge mirrors it with lifetime moved to
   `host_shutdown` on the init thread. cap10 files untouched.
2. **Fallback-handler precedence** — `mono_dl_fallback_register` PREPENDS. The
   bridge's provider answers the cap13 ABI plus the two cap10 probe symbols;
   reconcile with any cap10 callback-patch provider so exactly one source
   answers each symbol.
3. **Engine surface** — bind `EngineSurface` to the current #9 admitted mod
   services; unbound = `MBM_CS_ENGINE_UNBOUND` (fail-closed). Binding is
   integrator work against Claude's read-only candidate tree.
4. **Discovery** — admitted `ModImageInput` list comes from the integrator's
   discovery step over JG's facts records (dependency closure, topological
   order, ImplMap whitelist per design section 2c). The bridge re-verifies
   image identity but not PE facts.
5. **Compile guards** — target builds need `__PROSPERO__` +
   `MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1` + `MODBRIDGE_CAP13_RESIDENT_OWNER=1`
   + `MODBRIDGE_CAP13_CORLIB_DIR/CONFIG_DIR` (PREFIX semantics). Never define
   `MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION` in a target build.
6. **NEW_UNITS** — bridge + header set enter the driver's pinned dependency
   metadata like the cap10 handoff's two units.

## Standing honesty

Target port status: artifact-proven (runtime archive, static corlib AOT,
standalone link), in-game-UNPROVEN. No managed claim in this tree extends past
the cap10 Probe proof. A real downloaded managed mod fitting the v1 admitted
class is an open question (design section 8.3); HelloMod is our labeled
fixture, never presented as a real-mod pass.
