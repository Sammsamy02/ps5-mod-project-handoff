# REPORT — worker-re-spawn-chain

Worker: worker-re-spawn-chain · 2026-10-05 · local only. No console contact, no target execution,
no canonical edits. Tools used: workspace reads + Ghidra 12.1.4 headless over the ANALYZED project
`/tmp/wrfp_ghidra_proj/fnamepool` (program `decrypted-eboot.bin`, imageBase=0) with scripts saved
under `/tmp/wrfp_spawn_trace/` (Probe.java, SpawnChainTrace/2/3/4/5/6.java; every claim below is
duplicated into `CLAIM.json` with file:line pins). Mod bytes untouched (MD5
`4f15f2c79955c636eeaa38d0bf49433d` re-read from the canonical v3 package).

## 1. Deliverables

| File | Content |
|---|---|
| `CLAIM.json` | Every pin: main.lua spawn sequence (5 stages, class-path strings, out-param marshalling), PS5-side implementations (LuaMod.cpp / port sources / staged initializer), seed-table slots, Ghidra trace results |
| `SPAWN-READINESS.md` | §0 exact spawn execution trace, §1 per-dependency analysis (LoadAsset / SCO / FindFirstOf / LineTraceSingle), §2 READY/DEPENDS-ON/GAP table, §3 honest verdict |
| `REPORT.md` | this file |

## 2. Answer to the mission, compressed

**What runs on spawn-confirm** (main.lua:215-348, one drained game-thread closure):
player query (`FindFirstOf("PlayerController")`/UEHelpers + pawn reads) → ground trace
(`KismetSystemLibrary.Default__` CDO + `LineTraceSingle` with an FHitResult OUT-param read back as
`hit.ImpactPoint`/`hit.ImpactNormal`) → load pickup class (`StaticFindObject` → `LoadAsset` fallback
→ re-query; classes = the six AlwaysCook `BP_*` paths + live-DB paths, normalized to
`<path>.<Asset>_C`) → spawn actor (`GameplayStatics.Default__` +
`BeginDeferredActorSpawnFromClass(pawn, class, FTransform, 2, pawn, 0)` + `FinishSpawningActor`) →
configure pickup (`PickupInteractable` component reads/writes, `SetEnabled`, unhide, enable
collision; auto-destroy on any failure, main.lua:344-346).

**Is the PS5 side ready?**
- In the CURRENT staged launch-10 candidate: **NO — the menu rung itself is predicted silent**
  (drain blocker; SPAWN-CAPABILITY.md "LOAD-CAPABLE, NOT SPAWN-CAPABLE"), so this rung is
  unreachable there. That prior verdict stands unchanged; this work covers the rung AFTER it.
- Given a launch-11-armed drain and a proven menu-open (POST-POPUP rungs 1-3): the spawn chain is
  **structurally complete** — every Lua API registered, all four seed slots armed (GUA in-game
  validated, GMalloc V1/V2-gated, FName pool+ctor, SCO call-only), ProcessEvent route fully wired
  with all param-type pushers registered and an implemented FHitResult out-param readback. Menu
  success already proves live: GObjects queries, reflected property access, ProcessEvent UFunction
  calls (UMG), and the port→SCO(0x490aa0) ABI (every menu widget is built through it).
- Remaining between menu-open and a visible actor — three named live-evidence gaps, none structural:
  **GAP-L2a** FHitResult struct out-param readback (`ground trace` stage; historically highest-risk);
  **GAP-L2b** the ProcessEvent vtable slot 0x260 is Win64-PDB-derived (PS5 slot-shift caveat,
  core-port :617) — but rung-3's UMG calls check it before any spawn; **GAP-SPAWN** the engine-internal
  SpawnActorDeferred→SCO path never executed under the port (engine→engine; consumes only seeded
  addresses); plus **GAP-A7** the LoadAsset AssetRegistry fallback (ProcessEvent-based, likely unused
  for the six cooked classes). Each fails soft with a distinct mod-visible stage string.

## 3. Key new findings (beyond the prior docs)

1. **LoadAsset is 100% ProcessEvent** — the whole chain (GetAssetRegistry, GetAssetByObjectPath,
   GetAsset) goes through `StaticReflectedFunction::operator()` → `ProcessEvent`
   (ReflectedFunction.hpp:22-33); the port mounts no paks and needs none — it queries the live
   registry of the shipping game. It is strictly weaker than PE-LIVE, not an independent subsystem.
2. **The spawn path never consumes LoadAsset's return** — the class always comes from
   `StaticFindObject` over the seeded GUA (main.lua:283/:286); LoadAsset only materializes.
3. **Slot-4 SCO identity corroborated in the analyzed image**: bytes at 0x490aa0 are byte-identical
   to the seed-table prologue anchor; body = 469 bytes, 8 direct calls, one RELRO pointer read
   (`[0x9131f08]`). A 3-level / 140-function DFS found no direct rip-relative refs to the seeded
   GUA/GMalloc/FNamePool labels — construction reaches those deeper than 3 levels and via
   RELRO-indirect globals; population evidence stays the seed derivation (1565 GUA sites; sample
   `0x2b1e57 LEA R13,[0x93ba110]` re-verified directly).
4. **On the spawn path SCO is called by engine code** (SpawnActorDeferred → StaticAllocateObject →
   SCO), so the port's "identity and ABI unverified" caveat for slot 4 is exercised for the PORT's
   own calls at MENU build time (Lua `StaticConstructObject` → `StaticConstructObjectInternal426`,
   UObjectGlobals.cpp:21/:70-77) — rung 3 (`Menu created.`) would close that ABI risk before any
   spawn.
5. **The hash-table fast path for FindFirstOf is off by design** (`fuobject_hash_tables_get`
   unbound, staged initializer :981-983, graceful per UObjectHashTables.cpp:133) — the player/DB
   queries iterate the seeded GUObjectArray directly (UObjectGlobals.cpp:434+). Menu rungs 1-2
   already exercise the identical mechanism.
6. **ProcessEvent slot for 5.6 = 0x260** (`5_06_VTableOffsets_UObject_FunctionBody.cpp:571-578`),
   PDB-derived; the engine-ready log line (`Finding ProcessEvent` / `ProcessEvent address`,
   initializer :1909-1915) plus rung-3's UMG calls are the live checks — a PS5 slot shift would
   surface at rung 3, not at spawn.

## 4. Limits (honest)

- All findings are source + static-analysis derived. **Zero live in-game ProcessEvent, ground-trace,
  or spawn evidence exists for this candidate**; nothing here closes a rung — it names exactly what
  would close each one.
- The Ghidra project has 4,021 previously analyzed functions; no auto-analysis exists at/near
  0x490aa0, so this work used bounded manual disassembly (windows, CC-pad body bounds, rel32 calls,
  Ghidra-created references) rather than whole-image xref enumeration. The no-direct-seed-refs
  result is a bounded-DFS result (140 functions), not a proof of absence image-wide.
- `EngineVersionOverride Major=5 Minor=6` (settings v3) governs which generated layout tables the
  composed runtime uses; slot 0x260 comes from the 5.06 PDB-derived body — if the override ever
  changes, the slot re-derivation is out of scope here.
- The unnamed RELRO pointer `[0x9131f08]` read by SCO and the 0x493580 wrapper is recorded but not
  identified; identifying it would need either deeper analysis or the relocation tables, and it has
  no bearing on the readiness verdict (engine-internal reference).
- This worker made no edits outside `outputs/2026-10-04-zcode-team/worker-re-spawn-chain/` and
  `/tmp/wrfp_spawn_trace/` (scripts) and `/tmp` Ghidra project writes (script-created functions at
  0x490aa0/0x493580/0x493530 saved into the disposable analysis project).
