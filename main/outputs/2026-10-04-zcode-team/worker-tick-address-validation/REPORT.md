# REPORT.md — worker-tick-address-validation

Date: 2026-10-04. Scope: derivation chain for `UGameEngine::Tick` in the composed
composition-5 runtime, validations before the PLH patch, launch-2 predictions, and the minimal
serialized-ELF fix if needed. Deliverables in this directory: `CLAIM.json` (written first),
`TICK-VALIDATION-PATH.md`, `LAUNCH2-PREDICTIONS.md`, this report (written last).
No console contact, no nested subagents, writes confined to this directory.

## Verdict summary

1. **The composed runtime does not use a stored or file-derived Tick address.** After the anchor
   plants base `0x400000` (runtime_start_bridge_r3.cpp:113-156, 219-241), Tick is derived live:
   primary = patternsleuth reference scan (UTF-16 `causeevent=`/`CAUSEEVENT ` xrefs -> unique root
   function, game_loop.rs:4-10, dispatched as core resolver id 7, mapped_api.rs:50, slot 10,
   OwnedMappedResolverCollector.cpp:30-36); secondary = live `UGameEngine` vtable entry
   (UnrealVirtualBaseVC.hpp:118-131, staged UnrealInitializer.cpp:1315-1378, scan preferred by
   default, UnrealInitializer.hpp:146). Not a signature scan of a byte pattern, not an export
   (export resolver deliberately unset and would throw, lmf_to_native_loader_facts.hpp:65-67).
2. **The serialized-ELF witness is triply dormant**: (a) the 1-arg `MakeNativeProgramContext` the
   bridge calls passes empty ELF bytes (NativeLoaderContext.cpp:4-6, bridge :277-278); (b) the LMF
   conversion pins `CandidateSelectionPolicy::OriginalOnly` while the witness path additionally
   requires `SelectUniqueAfterNoMatch` (NativeLoaderContext.hpp:13, lmf_to_native_loader_facts.hpp:68-69,
   OwnedMappedScanSource.cpp:18); (c) even then it fires only on scan `NoMatch`/endpoint_status 3,
   never on `Ambiguous`/4 (OwnedMappedScanSource.cpp:18).
3. **Validations before the patch write are structural, not semantic.** Exists: `permits_code`
   span/permission on the scan candidate (OwnedMappedResolverCollector.cpp:45-47); `ensure_one`
   uniqueness; nullness via `is_ready()` (Function.hpp:57-64, 103-105; DetourInstance.hpp:158-162);
   PLH decodability/prologue-size/protection refusals (x64Detour.cpp:354-357, 413-427, 502-503).
   Does NOT exist: any prologue byte-comparison, function-boundary assertion, or identity proof that
   the target is UGameEngine::Tick. A wrong-but-plausible address is patched, `mods-started` still
   fires, and the outcome is an in-game crash (hot code) or a silent no-ticks park at `mods-started`
   (cold code). This is the loudest finding: **nothing refuses a wrong Tick address**.
4. **Predicted derivation result**: `0x400000 + 0x33ac8c0 = 0x37AC8C0`, inside exec span
   `[0x400000,0x787c000)` and inside the anchored extent `0xbc602c8`. If any record lists
   `0x33ac8c0` as a RUNTIME address, it is below the exec span start — those records should be
   re-read as LINKED addresses (translate semantics: runtime = base + (linked - lowest
   flagged PT_LOAD vaddr), OwnedSerializedElfSource.cpp:34, 62-64).
5. **Failure of both live routes is LOUD, not stuck-at-mods-started**: `ERROR_ID` from
   `RegisterEngineTickPreCallback` throws (staged :1384-1387), init catches, backend rethrows
   (ue4ss_backend.cpp:66-70), `RuntimeOwner` emits the `failed` ack (runtime_owner.cpp:112-123,
   154-175). `engine-ready`/`mods-started` are only emitted after tick registration succeeded
   (ordering: setup_unreal -> Initialize -> PostInitialize at UE4SSProgram.cpp:1265 vs
   on_engine_initialized at :652-661). The only "stuck at mods-started" shape is outcome #7
   (detour over never-executed code).
6. **r4 serialized-ELF fix is cheap on the runtime side and does not need new units**: the 2-arg
   overload is already defined and exported in the composed object —
   `_ZN2RC8Platform24MakeNativeProgramContextENS0_17NativeLoaderFactsERKNSt3__16vectorIhNS2_9allocatorIhEEEE`
   (exports.stdout of run-3, 2 symbols). The bridge change is: set
   `converted.facts.scan_policy = CandidateSelectionPolicy::SelectUniqueAfterNoMatch` (plain struct
   field, NativeLoaderContext.hpp:13), load the staged ELF file into `std::vector<uint8_t>` while
   /data is reachable in module_start (the bridge already relies on that window, bridge :272-274),
   and call the 2-arg overload. Existing gates stay satisfied (the symbol is defined in-object, so
   no new import).
7. **Trimmed ELF feasibility**: YES with strict slices. The two consumers read:
   - ELF header fields + program headers only (`OwnedSerializedElfSource.cpp:20-31`; witness side
     `FileImage::parse` refuses >128 phdrs, object_cfg.rs:24; C++ side allows <=4096, :22);
   - the full `p_filesz` bytes of EVERY `PT_LOAD` at `p_offset` (both parsers slice
     `bytes[p_offset..p_offset+p_filesz]`: elf_tick.rs:33-42, object_cfg.rs:26-33) — contents must
     byte-match the live image modulo R_X86_64_RELATIVE slots (OwnedSerializedElfSource.cpp:68-77,
     82-88);
   - `PT_DYNAMIC` (<=64KB, :26) and the complete `DT_REL`/`DT_RELA` tables (<=2,000,000 rows,
     elf_tick.rs:14 + OwnedSerializedElfSource.cpp:50-57).
   Nothing else is read: section headers, symbol/string tables, and all non-loaded bytes can be
   dropped; offsets may be compacted only preserving `p_offset ≡ p_vaddr (mod p_align)` or by
   lowering `p_align` (congruence check at OwnedSerializedElfSource.cpp:28; the Rust side checks no
   align at all). Since decrypted-eboot.bin is 194MB and its PT_LOAD spans dominate the file, the
   achievable trim is modest (estimate: low single-digit percent — NOT measured against the actual
   phdr table; do not claim a size win without measuring). 8192-witness and 2M-row bounds are far
   above what a shipping eboot needs.
   Caveat: even wired, the witness only fires when the live scan returns NoMatch — it is a fallback
   resolver, never a validator of a wrong unique scan match. For address VALIDATION the stronger
   r4 lever is the vtable-vs-scan agreement log plus requiring the resolved value to equal the
   file-witness `linked_tick` translation when the ELF is staged.

## Key files (absolute paths)

- Bridge/entry: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r3.cpp (r4 exists, changes only the VMMAP buffer size, :38,138,261 — the ELF wiring is NOT yet in it)
- Context factory: /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp and src/OwnedMappedScanSource.cpp, src/OwnedSerializedElfSource.cpp
- Scan collector: /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedResolverCollector.cpp and include/OwnedMappedResolverCollector.hpp
- Tick resolvers: /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-scan-snapshot-1/crate/src/game_loop.rs and /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/crate/src/elf_tick.rs (+ src/object_cfg.rs)
- Initializer (consumed unit): /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp
- Hook/detour: /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp and /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-owned-default-detour-1/port/x64Detour.cpp
- Owner/backend: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp and .../native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp

## Honest limits

- worker-tick-callback-audit's directory was empty at read time; DetourInstance references are from
  the exact header recorded in run-3's `unreal-unrealinitializer.d` (its InstallHook spans :149-171,
  ActivateHook :174-186, EnsureHooked :232-238; the "164-204" range in the task corresponds to the
  x64Detour construction + activation block in this copy).
- The 0x37AC8C0 prediction is derived from recorded facts plus code semantics; it is not verified
  against a live run of launch-2 (in flight elsewhere).
- Trim percentage for the 194MB eboot is an unmeasured estimate (no phdr dump of decrypted-eboot.bin
  was consulted in this pass).
- The Rust witness resolver's runtime cost over a 194MB image (CFG proofs bounded 64KB radius /
  8192 nodes per candidate, elf_tick.rs:216-229) is unmeasured; module_start latency will grow.
