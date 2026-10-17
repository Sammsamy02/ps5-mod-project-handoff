# HAZARD-TABLE — composition-5 r6, every failure point AFTER window-anchor success

Worker: worker-post-anchor-hazards, 2026-10-04. Static source reading only; every
claim cites `file:line`. Build identity: bridge r6
(`worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_start_bridge.cpp`,
byte-identical to `worker-module-start-wiring/runtime_start_bridge_r6.cpp`), entry r3
(`worker-module-start-wiring/runtime_module_entry_r3.c`), adapter
(`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp`),
ack supplier (`worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp`).
Include resolution proven from run-3 commands.json include order + `lmf-adapter-new.o.d`
(`worker-composition-prep/build-receipts/template-run-1/`): NativeProgramContext.hpp =
`native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp`;
OwnedMappedImageCaller.hpp = `ue4ss-native-ps-scan-provider-1/include/`; PsMappedImageCaller.hpp =
`ue4ss-native-ps-scan-abi-1/include/`; OwnedMemory.cpp (ReadLease) = `ue4ss-native-read-lease-1/src/`
(linked as `native-owned-memory.o`, coherent-full-1/native-owned-memory.command.json).

## Channels (abbreviations used below)

| Ch | What | Exact format | Source |
|---|---|---|---|
| D | diagnostic_event counter stream | tag + u32 value (per-event records) | entry r3:102-104; bridge r6:223-224,261-279,390-394 |
| L | UE4SS.log append line | `modbridge module_start failed: <stage> code=<n> detail=<sanitized>` | entry r3:166-192 (path `/data/plugins/UE4SS.log`, r3:68) |
| S | `<mod root>/runtime-status.txt` | head `modforge-runtime-status\t1\nsession\t<pid>\t<now_ms>\n` then `state\t<seq>\t<state>\t<code>\t<detail>\n`; open is O_TRUNC fresh-session | modforge_activation.c:599-642 |
| B | bridge `detail` out-buffer | printable-ASCII, NUL-terminated, capacity 192 from entry | bridge r6:69-90; entry r3:279-283 |

Result-code mapping at the entry (entry r3:284-299):
`MRS_OK=0`, `MRS_ERR_CONVERSION=1`, `MRS_ERR_CONTEXT=2`, `MRS_ERR_START=3`, `MRS_ERR_INTERNAL=4`
(runtime_start_bridge.h:36-41). Codes 1/2/4 -> `runtime_failure` = D + L + S (fresh O_TRUNC is safe:
ack sink not open yet). Code 3 -> D + L ONLY (r3 P2-1: the RuntimeAckSink may already own the session;
entry r3:289-299). Enum values for interpreting D: ProbeResult NotBound=0/Failed=1/Ok=2
(live_module_facts.hpp:98); IdentitySource None=0/LinkMap=1/VmScan=2 (live_module_facts.hpp:74-78);
Status Ok=0/InvalidArgument=1/NoImageAnchor=2/RetainFailed=3/ModuleInfoNotObtained=4/ModuleInfoRejected=5/
NotCaptured=6/AlreadyReleased=7/RetentionProbeFailed=8 (live_module_facts.hpp:52-64).

## Geometry planted by a verified anchor (baseline for every row below)

- Window anchor verify (bridge r6:237-282): requires primary window E (AES S-box, 64 B at
  `0x400000+0x7dd2a40=0x81d2a40`, bridge r6:125-129) covered by a `prot&1` VMMAP span (r6:136,255-259)
  and byte-identical (r6:266-270). Secondary A is check-and-log only (r6:246-248). On success the scan
  wrapper returns `chosen_index=0, chosen_base=0x400000, chosen_extent=0xbc602c8, candidate_count=1,
  truncated=0` (r6:360-366).
- Conversion then plants (lmf_to_native_loader_facts.cpp:167-183,222-256):
  `extent=[0x400000, 0xC0602C8)` (end = base+0xbc602c8), provenance `VmScanBase`;
  `images[MainExe] = {base=0x400000, size=0xbc602c8 (197,438,152 B)}`;
  `readable_ranges` = anchor mappings CONTAINING 0x400000 (live_module_facts.cpp:443-458 — that is the
  single exec-only span `[0x400000,0x787c000)`, prot=4, bridge r6:113-117) clamped/merged ->
  one range `{0x400000, 0x737c000}`; protection bits are NOT re-checked by the adapter (it clamps all
  anchor mappings, lmf_to_native_loader_facts.cpp:77-103).
- `mapped.text_base=0` and `mapped.image_base=0` are FIXED by design (lmf_to_native_loader_facts.cpp:226-235;
  hpp:46-50,33-39). `find_export` unset (cpp:248-249). `input_source` empty; `scan_policy=OriginalOnly`
  (hpp:67-69; NativeLoaderContext.hpp:13).
- `game_executable_path` = `snapshot.executable_path` = sysctl `CTL_KERN/KERN_PROC/KERN_PROC_PATHNAME/-1`
  (live_module_os_ops_sce.cpp:110-119; adapter cpp:141-152). It is NOT derived from the anchor.

---

# HAZARD TABLE (execution order; "PASS" rows are pre-proven, hazards marked *)

Legend: stage -> pass condition -> failure text (exact, from source) -> channel -> remedy hint.

## Stage 0 — anchor itself (boundary; launch-5 premise)

| # | Stage / site | Pass condition | Failure text (exact) | Channel | Remedy hint |
|---|---|---|---|---|---|
| 0.1 | bridge r6:241-243 `window_anchor_verify` fresh query | `bridge_query_vmentry()==0 && total>0` | none returned; verify() = false | D: `bridge_vmentry_entries`, `bridge_vm_walk_stop`; scan falls through, capture status NoImageAnchor -> refusal 0.2 text | r5 walker already proven launch-4; inspect stop tag |
| 0.2 | capture identity gate live_module_facts.cpp:313-316 | `anchor!=nullptr \|\| identity_source==VmScan` | conversion: "no image anchor (status NoImageAnchor); refusing conversion; dlopen(NULL) is never a fallback" (lmf cpp:48-49) | D `lmf_*` caps (bridge r6:390-394) + B + L/S code=1 | E-window coverage/match tags decide |

## Stage 1 — supplier capture + conversion (bridge r6:337-397)

| # | Stage / site | Pass condition | Failure text (exact) | Channel | Remedy hint |
|---|---|---|---|---|---|
| 1.1 | required ops + profile live_module_facts.cpp:179-189 | all op slots non-null; abi_size 352 == sizeof(ModuleInfo); capacity in 1..4 | supplier status InvalidArgument -> conversion: "supplier rejected capture arguments (status InvalidArgument); refusing conversion" (lmf cpp:44-46) | D caps all 0/NotBound + L/S code=1 | static; cannot regress |
| 1.2 | vmentry bridge r6:208-235 | stop_reason!=1, entries>0; truncation (stop 2) is SUCCESS with `count=entries+1` (r6:226-234) | malformed -> rc -6 -> capture vmentry Failed -> NoImageAnchor (0.2 text) | D `bridge_vm_walk_stop`=1 marks it | variable-record walk proven launch-4 |
| 1.3 | *executable path probe live_module_facts.cpp:216-225 via sce ops :110-119 | sysctl KERN_PROC_PATHNAME returns non-empty | **conversion: "observed executable path is empty; refusing to fabricate game_executable_path"** (lmf cpp:141-145); lexically invalid -> "observed executable path is not an absolute normalized file path" (:147-152) | D `lmf_identity_source`=2 but caps show executable_path Failed; L/S code=1, detail = text | first unproven in-game fact after the anchor; if seen, sysctl pathname needs an in-game probe before any code change |
| 1.4 | link-map walk live_module_facts.cpp:231-293 | refusal is EXPECTED in-game (g2); demoted, non-fatal when scan route exists (:286-287) | n/a (records `provider_code=walk_rc`) | D `lmf_linkmap_cap`=1, `lmf_provider_code` | none |
| 1.5 | scan route live_module_facts.cpp:295-311 + bridge wrapper r6:346-368 | base scan OR window anchor yields `chosen_index>=0 && chosen_base!=0` | vm_scan Failed -> NoImageAnchor (0.2 text) | D `anchor_E_covered/_match`, `lmf_vm_scan_cap` | launch-5 premise: E verifies |
| 1.6 | retention (vm-scan route) live_module_facts.cpp:342-348 | degraded-by-design: `RetainKind::None`, `loader_retain=Failed`; NOT fatal | n/a (never fatal on this route; name-route RefainFailed text differs) | none | honest absence |
| 1.7 | module-info bridge live_module_facts.cpp:354-381 | exec handle raw 0 (g1) -> `module_info_degraded=true`, `module_info.obtained=false` | -> status ModuleInfoNotObtained (4) (:462-467) — NOT a refusal behind the opt-in | D `lmf_provider_code` | none |
| 1.8 | status gate lmf cpp:126-137 | `status==ModuleInfoNotObtained && opt-in && identity==VmScan && chosen && chosen_base!=0` | any other degraded status -> verbatim status_error strings (cpp:36-67), e.g. RetainFailed "retention re-probe failed…", ModuleInfoRejected "kernel module info violated pinned ABI rules…" | D caps + L/S code=1 | opt-in already set (bridge r6:380) |
| 1.9 | mappings mandatory lmf cpp:154-159 | `anchor_mapping_count>0` (the [0x400000,…) span contains observed_base) | "snapshot has no observed mappings; ModuleSnapshot requires non-empty readable ranges" | L/S code=1 | would mean VMMAP no longer lists 0x400000 |
| 1.10 | extent resolve lmf cpp:167-183, 208-212 | extent_bytes 0xbc602c8 in (0, representable] | "vm-scan image span is empty or unrepresentable" / "module image span is empty or unrepresentable" | L/S code=1 | static; eboot-specific constant |
| 1.11 | ranges derive lmf cpp:214-219 | >=1 mapping intersects extent | "no observed mapping interval intersects the module image extent with valid geometry" | L/S code=1 | static |
| 1.12 | *path argument lmf cpp:112-117 | `/data/plugins/modbridge_supplier_composition.prx` absolute+normalized (entry r3:67 constant) | "module_file_path must be an absolute normalized file path with a filename and no NUL" | L/S code=1 | static; cannot fire with the pinned constant |

Stage 1 refusal channel shape (bridge r6:385-397): `MRS_ERR_CONVERSION` (1), detail = error message,
plus D events `lmf_vmentry_cap`, `lmf_vm_scan_cap`, `lmf_linkmap_cap`, `lmf_provider_code`,
`lmf_identity_source`; entry then emits D `runtime_start_failed`=1, L line, S `failed` line
(entry r3:296-298). On success: D `mfa_activated_low/high` (entry r3:270-273) precede the bridge call.

## Stage 2 — MakeNativeProgramContext (bridge r6:399-448; NativeLoaderContext.cpp:4-21)

| # | Stage / site | Pass condition | Failure text (exact) | Channel | Remedy hint |
|---|---|---|---|---|---|
| 2.1 | format gate NativeLoaderContext.cpp:10-11 | adapter sets `PS_MAPPED_IMAGE_ELF64` (lmf cpp:225) — PASS | "Native PS5 program requires caller-supplied ELF64 mapped image facts" | B + L/S code=2 | static |
| 2.2 | ModuleSnapshot::Create port/…/Unreal/PlatformModuleSnapshot.hpp:72-114 | lifetime non-null (owning supplier alias, lmf cpp:289-295), MainExe present, monolithic==1 image, ranges non-empty/sorted/in-extent/merged, extent representable | "Native modules require a lifetime owner and MainExe image" / "A monolithic snapshot must describe only MainExe" / "Module or readable range has an invalid extent" / "Module has no supplied readable ranges" / "Readable ranges overlap or lie outside their module" | B + L/S code=2 | all pre-proven by adapter geometry (S1 host tests T1/T1b) |
| 2.3 | lexical paths NativeProgramContext.hpp:85-92 (coherent port; .d-proven) | both paths already adapter-validated — PASS | "Native program paths must be absolute normalized file paths without NUL" | B + L/S code=2 | static |
| 2.4 | module_source non-null NativeProgramContext.hpp:93-94 | PASS (snapshot just built) | "Native program requires an owning loader module snapshot" | B + L/S code=2 | static |
| 2.5 | *file existence NativeProgramContext.hpp:97-99 | `is_regular_file("/data/plugins/modbridge_supplier_composition.prx")` AND `is_regular_file(<sysctl executable_path>)` at module_start time; then `file_size()` :100 | **"Native module and executable paths must name existing regular files"** (std::filesystem::file_size may also throw filesystem_error, :100) | B + L/S code=2; L line shows it verbatim | the sysctl string must be a stat-able regular file in the game sandbox — verify what KERN_PROC_PATHNAME returns in-game before trusting this row away |
| 2.6 | **PLH read lease** OwnedMappedScanSource.cpp:10 -> OwnedMappedImageCaller.hpp:55 `lease_(ranges_)` -> OwnedMemory.cpp:106-138 | EVERY page of `readable_ranges` (`[0x400000,0x787c000)` of LIVE GAME memory) must be REGISTERED in the PLH owned-memory service | **`std::invalid_argument("Owned read range includes unregistered bytes")`** (OwnedMemory.cpp:122; lease ctor precedes the caller member, OwnedMappedImageCaller.hpp:55-56) | B + L: `… code=2 detail=Owned read range includes unregistered bytes`; S: `state\t1\tfailed\t2\truntime_start_failed Owned read range includes unregistered bytes` | **EXPECTED launch-5 terminal if all earlier rows pass.** Nothing in the composed object registers game memory: ScopedRegion/record_mapping appear only in host tests (grep whole tree; link has native-owned-memory.o from read-lease-1). Fix = register the snapshot ranges via the service at lease scope, or a lease variant that admits loader-facts ranges |
| 2.7 | (if 2.6 were fixed) span perms PsMappedImageCaller.hpp:44-53 | every lease span carries PS_MAPPED_READ (flags derive from registration protection, OwnedMappedImageCaller.hpp:24-28) | "Invalid supplied mapped span" | B + L/S code=2 | registration must assert R even for exec-only pages (exec pages read fine in-game — anchor proved it) |
| 2.8 | (if 2.7 passed) exact coverage PsMappedImageCaller.hpp:55-67 | segments (lease spans) exactly cover snapshot ranges | "Mapped spans must exactly cover supplied snapshot ranges" / "Mapped span extends beyond supplied range" | B + L/S code=2 | spans partition ranges by construction — PASS |
| 2.9 | (if 2.8 passed) **text_base containment** PsMappedImageCaller.hpp:68-70: `contains({text_base=0,1})` is FALSE (0,1 fails the `!address` guard :23-25) | needs a real contained text_base — cannot pass with the adapter's mandated 0 | **"Mapped metadata or spans exceed supplied readable bytes"** | B + L/S code=2 | S1 deviation D2, host-proven probe A (worker-s1-adapter/REPORT.md:102-108,149): "MakeNativeProgramContext cannot complete from adapter output alone" — a target-verified .text base must be joined (e.g. from the window-anchor phdr parse) before Stage 3 is reachable |
| 2.10 | (if 2.9 passed) ELF64 image_base==0 PsMappedImageCaller.hpp:71-73 | adapter stores 0 — PASS | "Mapped metadata does not match format" | B + L/S code=2 | D1 deviation already aligned (S1 REPORT:91-101) |
| 2.11 | (if 2.10 passed) permits_code OwnedMappedImageCaller.hpp:57 | text_base registered R\|X | "Mapped text base must be registered readable executable bytes" | B + L/S code=2 | unreachable while text_base=0 (contains fails first); matters only after a D2 fix |

Stage 2 channel shape (bridge r6:439-448): `MRS_ERR_CONTEXT` (2), detail = exception what();
entry r3:296-298 full runtime_failure (D `runtime_start_failed`=2 + L + S).

## Stage 3 — StartActivatedRuntimeWithAck (runtime_ack_supplier.cpp:68-95)

Reachable ONLY after a D2/lease fix. Ordered hops:

| # | Hop / site | Pass condition | Failure text (exact) | Channel | Remedy hint |
|---|---|---|---|---|---|
| 3.1 | MakeRuntimeAckConfig runtime_ack_supplier.cpp:44-54 | no-throw; status_root = mod_root verbatim, pid, now_ms | none (empty status_root would silently disable the ack only) | S head would be absent | mod_root is a proven-writable constant (entry r3:66) |
| 3.2 | InstallDefaultEngineBindings NativeQueryHarness.cpp:75-83 -> InstallEngineBindings :62-73 | first install in this loaded lifetime; four static target probes non-null (constructor returns them, :35-59) | "QueryHarness engine bindings already installed for this loaded lifetime" / "QueryHarness engine bindings require all four probes" (std::logic_error) | bridge -> MRS_ERR_START(3); entry r3:292-295 = D + L ONLY (no S write); no ack sink open yet, so no S failed line at all | PRX reload in the same process is the only realistic double-install |
| 3.3 | mod_source gate ue4ss_backend.cpp:88-89 | context.mod_source empty (bridge passes fresh context) | "Native UE4SS context already has a mod source" | MRS_ERR_START; D + L only if thrown before sink open | static |
| 3.4 | MakeActivatedUE4SSModSource bootstrap/activated_mod_source.cpp:18-46 (composed 2-arg version, run-3 exports.stdout) | title `PPSA33286` == mfa_title; engine=="unreal"; mfa_wants_framework "ue4ss-ps5"; every ue4ss/* file role in {lua_mod,config,script}; every file preloaded | "UE4SS activation must match the supplied game title" / "Activation does not request the native UE4SS framework" / "Unsupported UE4SS activation file role: <path>" / "UE4SS file was not preloaded: <path>" | thrown BEFORE sink open -> D + L only (no S) | load-list content fix under the mod root |
| 3.5 | RuntimeAckSink::Open runtime_owner.cpp:130-153 | opens `<status_root>/runtime-status.txt` O_TRUNC; never throws; inert on failure (last_error recorded) | none (startup continues unacknowledged) | S head line present on success | root writable proven |
| 3.6 | UE4SSBackend re-validation ue4ss_backend.cpp:24 | same 2.3-2.5 checks again | same texts as 2.3-2.5 | ack->failed(1,msg) (ue4ss_backend.cpp:106-117) then MRS_ERR_START; entry: D + L only — ack sink owns the S session | both paths must keep existing across the two validations |
| 3.7 | ConfigureRetainedPS5PadInput ue4ss_backend.cpp:25-26 -> ue4ss-native-pad-config-1/src/NativePadInput.cpp:10-26 | absent `ue4ss/PS5Pad.ini` -> no-op (input stays null); if present: provider must open+report available | "PS5Pad.ini conflicts with the supplied native input source" (impossible: input null) / "PS5Pad provider factory returned no source" / "PS5Pad input unavailable: phase=<n>, error=<n>" | ack->failed(1,msg) in S; D + L at entry | drop/fix PS5Pad.ini or pad service init |
| 3.8 | UE4SSProgram ctor ue4ss_backend.cpp:47-50 | constructor completes with empty native_startup_error_message (settings/path/log setup; mod autostart Deferred :27) | **"Native UE4SS construction rejected: " + stored error** (runtime_error) | ack `failed` line in S; D + L at entry (code=3) | read the stored message from the L/S detail — it is the program's own error text |
| 3.9 | start_native_mods (buffered inventory) ue4ss_backend.cpp:51-57 | Lua inventory accepted | "Native UE4SS inventory rejected: " + stored error | same as 3.8 | load-list/Lua content |
| 3.10 | ack activated ue4ss_backend.cpp:103 -> runtime_owner.cpp:171 | emits `state\t<n>\tactivated\t0\t` | write failure recorded, never fatal | S | — |
| 3.11 | owner ctor runtime_owner.cpp:12-16 | backend non-null — PASS | "Native runtime backend is required" | ack failed + MRS_ERR_START | static |
| 3.12 | owner->start() runtime_owner.cpp:23-42; bridge r6:93 IGNORES the return | thread spawn succeeds | std::system_error resource failure caught :32-40 -> status Failed + ack `failed` + start() returns false — **bridge still reports MRS_OK** ("a refused start already reflected in RuntimeStatus - the caller owns that check", bridge r6:91-92) | D `runtime_start_ok` (entry r3:286) + S `failed` line; L shows only the success path (nothing) | interpret "runtime_start_ok D + S failed" as spawn refusal |
| 3.13 | init thread ue4ss_backend.cpp:59-71 -> program_->init(); ready callback :33-46 | engine init completes; on_engine_initialized fires ack engine_ready + (mods verified) mods_started (runtime_owner.cpp:171-176) | any throw -> ack `failed(1,error.what())` + status Failed (:111-124); init returning without stop -> "Native runtime init returned without a stop request" (:106-109) | S only (module_start already returned 0 with D `runtime_start_ok`); plus UE4SS's own log output | async — never visible in module_start's return |
| 3.14 | stop path (module_stop, bridge r6:286-306) | owner->stop joins, ack stopped once (runtime_owner.cpp:71-75) | stop exception -> `MRS_ERR_STOP_EXN`(2) (bridge r6:303); stop()==false -> entry runtime_failure("runtime_stop_failed") (entry r3:355-357) | D `runtime_stop_result` + L/S | loader-control thread only |

## Bottom-line byte-interpretation for launch-5

With the anchor verified (premise), the first unproven gates in order are 1.3 (sysctl
executable path) -> 2.5 (stat of both paths) -> **2.6 (PLH lease: "Owned read range includes
unregistered bytes")** -> 2.9 (D2: "Mapped metadata or spans exceed supplied readable bytes").
Most-probable terminal if capture/paths hold: `MRS_ERR_CONTEXT` (2) with detail
"Owned read range includes unregistered bytes" — UE4SS.log line
`modbridge module_start failed: runtime_start_failed code=2 detail=Owned read range includes unregistered bytes`
and runtime-status.txt `state\t1\tfailed\t2\truntime_start_failed Owned read range includes unregistered bytes`
(after `modforge-runtime-status\t1\nsession\t<pid>\t<ms>`). Stage 3 is unreachable until both 2.6 and 2.9
are fixed; no ModuleInfoNotObtained-degraded fact refuses anything downstream (see REPORT.md Q5).
