# REPORT — worker-post-anchor-hazards (2026-10-04)

Task: predict every remaining failure point AFTER composition-5 r6 window-anchor success
(E: AES S-box at 0x400000+0x7DD2A40) so launch-5's result is interpretable to the byte.
Deliverable: `HAZARD-TABLE.md` (same dir). This report records the method, the decisive
findings, and the answers to the two specifically-asked questions. No execution, no
target/console contact; all conclusions are from static source reading with file:line
citations, cross-checked against the composition-4 run-3 build metadata (commands.json,
exports.stdout, dep files) so the cited headers are the ones actually compiled into the
composed object.

## What was read (canonical for the composed PRX)

- Bridge r6: `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_start_bridge.cpp` — byte-identical (diff-verified) to `worker-module-start-wiring/runtime_start_bridge_r6.cpp`; staged header `lineage-overlay-1/runtime_start_bridge.h`.
- Entry r3: `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry_r3.c` (the staged `lifecycle/runtime_module_entry.c`).
- Adapter: `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` + `include/lmf_to_native_loader_facts.hpp` (composition extra-unit `lmf-loader-adapter`).
- Supplier core + SCE ops: same tree, `src/live_module_facts.cpp`, `src/live_module_os_ops_sce.cpp`, `include/live_module_facts.hpp`.
- Context/validation chain (include resolution proven via run-3 `commands.json` -I order and `worker-composition-prep/build-receipts/template-run-1/lmf-adapter-new.o.d`):
  `native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp` (102-line variant with mod_startup_policy),
  `native-coherent-runtime-1/source/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp`,
  `ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp` + `src/OwnedMappedScanSource.cpp`,
  `ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp`,
  `ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp`,
  `ue4ss-native-read-lease-1/src/OwnedMemory.cpp` (linked as `native-owned-memory.o`, coherent-full-1 receipt).
- Stage 3: `worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp`,
  composition-3 bootstrap units actually linked: `ue4ss_backend.cpp` (`worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/`) and `runtime_owner.cpp` (`.../native-coherent-runtime-1/source/bootstrap/`),
  `native-coherent-runtime-1/source/bootstrap/activated_mod_source.cpp` (composed 2-arg symbol verified in run-3 exports.stdout),
  `ue4ss-native-pad-config-1/src/NativePadInput.cpp`, `worker-query-harness/src/NativeQueryHarness.cpp` + W/include/NativeQueryHarness.hpp,
  `native-coherent-runtime-1/source/activation/modforge_activation.c` (status file format).

## Finding 1 — the anchor does NOT clear the path; two hard gates remain in Stage 2

1. **PLH read-lease registration gate (expected launch-5 terminal).**
   `MakeNativeProgramContext` ends with `make_shared<OwnedMappedScanSource>` (NativeLoaderContext.cpp:19).
   Its constructor builds an `OwnedMappedImageCaller` whose member-init builds a
   `PLH::OwnedMemory::ReadLease` over the snapshot's readable ranges (OwnedMappedScanSource.cpp:10;
   OwnedMappedImageCaller.hpp:55). On __PROSPERO__ the lease requires EVERY page to be registered in the
   owned-memory service and throws `std::invalid_argument("Owned read range includes unregistered bytes")`
   at the first unregistered page (ue4ss-native-read-lease-1/src/OwnedMemory.cpp:121-126). The ranges here
   are live game memory (`[0x400000,0x787c000)`, planted by the conversion from the anchor mapping that
   contains `observed_base`, live_module_facts.cpp:443-458), and nothing in the composed object registers
   game memory: `ScopedRegion`/`record_mapping` callers exist only in host tests (tree-wide grep; the S1
   host test explicitly registered its fixture pages, worker-s1-adapter/REPORT.md:139-141). The lease is
   constructed before the PsMappedImageCaller member, so this throw precedes everything else in 2.6-2.11.
   Bridge catch -> `MRS_ERR_CONTEXT` (2) (bridge r6:439-448); entry r3 does a FULL runtime_failure for
   code 2: diagnostic `runtime_start_failed`=2, UE4SS.log line
   `modbridge module_start failed: runtime_start_failed code=2 detail=Owned read range includes unregistered bytes`,
   and a fresh `runtime-status.txt` session ending `state\t1\tfailed\t2\truntime_start_failed Owned read range includes unregistered bytes`.

2. **The known D2 text_base gate (second, only after 1 is fixed).** The adapter mandates
   `mapped.text_base=0` (lmf cpp:226-227; hpp:46-50), and `PsMappedImageCaller` requires
   `contains({text_base,1})` (PsMappedImageCaller.hpp:68-70); `contains({0,1})` is false by the
   `!address` guard (:23-25) -> throw "Mapped metadata or spans exceed supplied readable bytes".
   This is S1 deviation D2, host-proven by probe A: "MakeNativeProgramContext cannot complete from
   adapter output alone; a target-verified `.text` base … must be joined later"
   (worker-s1-adapter/REPORT.md:102-108, ladder table :149). `permits_code` (:57) is the third gate,
   unreachable while text_base stays 0.

Both throws surface identically at the entry (`MRS_ERR_CONTEXT`=2), so launch-5's UE4SS.log
`detail=` string is the discriminator between them (plus row 1.3/2.5 details if the earlier
gates fire).

## Finding 2 — game_executable_path provenance (Task 2 question)

The bridge does NOT set `game_executable_path`, and the anchor does not fill it either.
The conversion fills `facts.game_executable_path` from `snapshot.executable_path`
(lmf_to_native_loader_facts.cpp:141-152), which is captured by the SCE op
`lmf_sce_executable_path` = `sysctl{CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1}`
(live_module_os_ops_sce.cpp:110-119) during `capture()` (live_module_facts.cpp:216-225).
What it must BE, in order:
1. non-empty (else Stage-1 refusal "observed executable path is empty; refusing to fabricate game_executable_path", MRS_ERR_CONVERSION with the bridge's `lmf_*` diagnostic block, bridge r6:385-397);
2. lexically absolute/normalized with a filename (else "observed executable path is not an absolute normalized file path");
3. an EXISTING REGULAR FILE at validation time — `ValidateNativeProgramContext` stats BOTH paths
   (NativeProgramContext.hpp:97-99) and re-validates again inside the UE4SS backend ctor
   (ue4ss_backend.cpp:24) -> else "Native module and executable paths must name existing regular files" (MRS_ERR_CONTEXT).
Whether (1)/(3) hold in-game is the main unproven empirical fact after the anchor; the sysctl path
never having been consumed past Stage 1 before, its exact string is unknown to this worker.

## Finding 3 — Q5: does the ModuleInfoNotObtained-degraded snapshot refuse ANYTHING downstream?

**No.** The degradation is consumed entirely by the conversion's status gate:
- `lmf::Status` is read exactly once, in `convert_impl` (lmf cpp:129-137); behind
  `allow_vm_scan_base_as_module_image_base` (set at bridge r6:380) a `ModuleInfoNotObtained`
  (4) + `IdentitySource::VmScan` (2) + `vm_scan.chosen` + `chosen_base!=0` snapshot converts.
  Nothing downstream ever sees `lmf::Status` again — `NativeLoaderFacts` carries no status field.
- Retention `RetainKind::None` (live_module_facts.cpp:342-348) is harmless: `ModuleSnapshot`
  only needs a NON-NULL `lifetime` shared_ptr (PlatformModuleSnapshot.hpp:74-75), and the owning
  conversion overload aliases the supplier itself into that token (lmf cpp:289-295) — a genuine
  non-null owner with no loader reference, exactly as documented (hpp:55-62). Nobody in the chain
  calls `validate_retention()`/`release()` (grep: no call sites in bridge/adapter), so the degraded
  retention is never re-checked.
- `NativeModSource` is built purely from the mfa activation's preloaded files
  (bootstrap/activated_mod_source.cpp:28-45; ue4ss_backend.cpp:90) — it never reads observed module
  names, module info, or anything scan-derived. Title/framework gates there are activation facts
  (`PPSA33286`, "unreal", "ue4ss-ps5"), not degradation facts.
- The scan_policy stays the default `CandidateSelectionPolicy::OriginalOnly` (NativeLoaderContext.hpp:13;
  hpp:67-69). OriginalOnly is a valid policy for `OwnedMappedScanSource` (OwnedMappedScanSource.cpp:9)
  and merely means the serialized-ELF tick selection branch (:18) never activates; it does not refuse
  construction.
- Residual degraded-route facts that matter later, but refuse nothing at startup: `find_export`
  left unset -> `ModuleSnapshot::FindExport` throws
  `std::logic_error("Native module export resolver is not configured")` on first call
  (PlatformModuleSnapshot.hpp:50-57; documented at lmf cpp:248-249) — a latent hazard for any
  export-name lookup after the runtime is running, independent of degradation; and
  `vm_mappings_truncated`/1024-entry supplier view are honest-bookkeeping only
  (live_module_facts.hpp:82-85, live_module_facts.cpp:208-214).

So: after a vm_scan anchor, the degraded snapshot plants full geometric facts and NOTHING
downstream re-runs the degraded gate — the remaining refusals are the lease/D2/path gates above.

## Interpreting launch-5 to the byte (expected outcome tree)

Given the premise (E verifies: D events `anchor_E_covered`=1, `anchor_E_match`=1; wrapper returns
base 0x400000/extent 0xbc602c8, bridge r6:360-366):
1. If sysctl KERN_PROC_PATHNAME fails -> `MRS_ERR_CONVERSION` (1), detail "observed executable path is empty; refusing to fabricate game_executable_path"; D shows `lmf_identity_source`=2 with `lmf_provider_code`=<walk rc>; S gets a code-1 failed line.
2. Else if either path fails stat -> `MRS_ERR_CONTEXT` (2), detail "Native module and executable paths must name existing regular files".
3. Else -> `MRS_ERR_CONTEXT` (2), detail "Owned read range includes unregistered bytes" (most probable terminal).
4. Only after a lease fix -> "Mapped metadata or spans exceed supplied readable bytes" (D2).
Stage 3 (ack config, engine bindings, prepare, owner start) is unreachable until 3 and 4 are fixed;
its failure texts/channels are pre-enumerated in HAZARD-TABLE rows 3.1-3.14 (note: a `runtime_start_ok`
diagnostic WITH a `failed` line in runtime-status.txt means `owner->start()`'s false return was
ignored — bridge r6:91-93 — i.e. thread-spawn refusal, not success).

No claims of engine readiness, quality, or completion are made; all of the above is static
prediction for interpreting the next run's artifacts.
