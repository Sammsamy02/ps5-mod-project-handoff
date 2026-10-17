# REPORT — worker-all-signatures-inventory

2026-10-04. Task: the COMPLETE list of every AOB/signature/pattern-scan dependency on the
startup -> engine-ready -> mods-started -> first-F7 path of the composed PS5 runtime, each with
consumer chain, scan-target class, composed binding status, and verdict — so the one-pass
composition build faces no further scan surprises. Static source reading only; no console
contact; no nested subagents; nothing modified outside this directory. Deliverable order:
CLAIM.json first, SIGNATURES-INVENTORY.md, this report last.

## Method (about 46 tool calls)

1. Located and read COMPLETELY, with md5 cross-checks across all tree copies:
   - `SRC:port/ue4ss/src/Signatures.cpp` (507 lines — 12 Lua scan overrides + `scan_from_lua_script` + `setup_lua_scan_overrides`; identical md5 `2ee019cd…` everywhere)
   - `SRC:port/unreal/src/Signatures.cpp` (157 lines — both SinglePassScanner impl passes; md5 `0b07233c…`)
   where `SRC:` = `W:.superpowers/native-dependency-spike/native-coherent-runtime-1/source-reconstruction-final-4/`.
2. Read the consumed staged initializer `W:outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp` end to end for the regions that matter: ScanGame config/loop/consumers (:576-927), Initialize vtable/hook/derivation blocks (:1020-1574), PostInitialize GMalloc throw (:387-392).
3. Read the scan-source stack: `PsScanAbi.h` (lineage overlay, 13 slots), `OwnedMappedResolverCollector` (required mask `0x5d`, `provider_slots[]{6,0,2,1,3,4,10}`), `OwnedMappedScanSource`/`OwnedMappedImageCaller` (G1 ctor throw), ported `x64Detour::hook()` (PLH patch mechanics), `ASMHelper.cpp` (jmp-probe), `UnrealVirtualBaseVC.hpp` (vtable-walk macros), `NameTypes.cpp` (ToString fallback), `FFrame.cpp`/`UFunctionStructs.cpp`/`UObjectGlobals.cpp`/`UObjectHashTables.cpp` (graceful degradation sites), `SettingsManager.hpp/.cpp` + staged `UE4SS-settings.ini` (hook-flag and version-override defaults), `UE4SSProgram.cpp` (GNatives warning, overrides install, layout ini loads).
4. Consumed prior verified analyses rather than re-deriving: SLOTS-FEASIBILITY.md (slot geometry G0-G4, measured anchors), worker-fname-ctor-audit (ctor candidate + detour-verifier exec-block), worker-tick-address-validation (tick derivation chain, "nothing refuses a wrong Tick address"), worker-engine-tick-fallback (EngineTick structural gate, ack ordering), worker-tick-witness-design (route B framing), worker-fname-runtime-probe (FC-1 mprotect-first validator design).
5. Verified absence facts: no `UE4SS_Signatures/*` anywhere in the staged roots; no `[Hooks]`/`[EngineVersionOverride]` sections in the staged ini; no GEngine/GWorld extern bindings in the port; `console_manager_singleton` has zero consumers.

## Findings

The full dependency table (33 rows, sorted startup / init-thread / event-loop / F7-press) with
file:line citations is `SIGNATURES-INVENTORY.md` §1-§6. Headline structure:

- **Three pipelines**: ps_scan (13 slots, structurally dead G0-G4 per SLOTS-FEASIBILITY), Lua AOB override passes (inert — no scripts shipped), and non-scan binders (vtable walks, reflection, layout INIs, settings forces).
- **The known triad confirmed and placed**: ps_scan slots {0,2,3,4,6} required+dead (remedies: seeds for 0/3, direct-consumer constants for 2/4, settings for 6); FName ctor binding derived (`base+0x23d590` chain) but its VERIFIER is detour-based; tick scan arm dead with a works-data-side vtable fallback.
- **Five flags**, three of them new or materially expanded (details + remedy sketches in SIGNATURES-INVENTORY.md §7):
  - **FLAG-1 (new, loudest)**: every hook is a `PLH::x64Detour` code READ+WRITE (`x64Detour.cpp:322-397`), and two engine-ready-structural hooks arm at init (FName verification throw `SINI:954-958`; EngineTick registration throw `SINI:1384-1387`). No in-game evidence any code write succeeds; probes only ever read via mprotect-add-READ. The seed work covers bindings but NOT these patches. Remedies sketched: mprotect-RWX feasibility probe, vtable-patch alternative for the tick, FC-1 validator replacing the FName detour.
  - **FLAG-2 (expanded)**: all ten vtable walks (`UnrealVirtualBaseVC.hpp:104-134`) end in `resolve_function_address_from_potential_jmp` whose 16-byte first-instruction decode (`ASMHelper.cpp:9-22,49-69`, no PROSPERO variant) is a code read — crash-shaped under the trusted prot-0x4 record, in tension with the "route B needs no readable text" framing.
  - **FLAG-3 (new)**: ProcessInternal/ProcessLocalScriptFunction built-in derivation runs at init (all hook flags default true, no overrides) and Zydis-reads 164 code bytes (`SINI:1497-1557`) — a scan dependency nobody had enumerated; remedy = two ini keys or offline-derived constants.
  - **FLAG-4 (new, minor)**: `ftext_fstring` is never set (`SINI:585-596`), so the second-pass Lua DoScan gate (`SINI:907-911`) is always true — inert today, a trap if signature scripts ever ship.
  - **FLAG-5 (status)**: nothing seed-related is staged yet (`worker-seed-table-derivation` holds CLAIM.json only; no `[EngineVersionOverride]` in the staged ini), so as-composed the required mask is full and startup dies at A1/A2 exactly as predicted.

Verdict roll-up: bound-by-seed — slots 0/2/3/4 bindings and slot 6 via settings; works-data-side — slot 1 (Conv_NameToString), 7 (hash-tables fallback), 10 tick vtable binding, 11/12 unknown-defaults, all 10 vtable-walk address derivations, layout INIs, reflection, Lua mod start; not-on-path — slots 5/8/9, all 12 signature scripts, CallFunctionByNameWithArguments/ProcessConsoleExec/ULocalPlayerExec chain, GEngine/GWorld; dead-needs-remedy — A1 scan-source construction (G1), A2 ps_scan arm, C1 FName verifier, C4/E4/E5 detour patches, C10 ProcessInternal derivation.

## Explicit non-claims

- No runtime execution, no console contact; all verdicts are static classifications of the composed trees as consumed by the composition-4/5 lineage.
- Seed constants remain identity-unverified per their deriving workers; FLAG-1/2/3 are conditional on the trusted exec-only text record and must be re-evaluated if a feasibility probe shows text readable.
- No frozen or wiring file was modified; deliverables confined to this directory.
