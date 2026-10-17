# worker-s1-adapter — lmf → NativeLoaderFacts conversion adapter (S1)

Date: 2026-10-04. Worker `worker-s1-adapter` of the ZCode team
(`outputs/2026-10-04-zcode-team/`), PS5 mod-manager / UE4SS critical path.
Output area (only place written):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-s1-adapter/`

## Files created (nothing else touched)

| File | Role |
|---|---|
| `src/lmf_to_native_loader_facts.hpp` | Adapter API: `RC::Platform::MakeNativeLoaderFactsFromSupplier` (throwing), `TryMakeNativeLoaderFactsFromSupplier` (result wrapper, ref + shared_ptr overloads), `LmfConversionResult`/`LmfConversionError`/`LmfImageBaseProvenance`/`LmfConversionOptions`. |
| `src/lmf_to_native_loader_facts.cpp` | Conversion implementation. Integer-only geometry; never dereferences any reported address. |
| `host_tests/test_lmf_adapter_host.cpp` | 102 host checks driving the REAL supplier core (fake ops) and the REAL immutable factory chain. |
| `build-receipts/run-1/*` | Objects, stderr captures, run stdout, `receipt.json`, sha256s. |
| `host_tests/fixtures/{eboot.bin,ue4ss.so}` | Regular files required by `ValidateNativeProgramContext`'s host-runnable `is_regular_file` checks. |
| `CLAIM.json` | Written first, per worker rules. |

Immutable inputs compiled (not copied): spike
`ue4ss-native-tick-file-owner-1` `include/NativeLoaderContext.hpp` + full
chain (`OwnedMappedScanSource.hpp`, `OwnedSerializedElfSource.hpp`,
`OwnedMappedCandidateSelection.hpp`) and `src/{NativeLoaderContext,
OwnedMappedScanSource,OwnedSerializedElfSource}.cpp`;
`ue4ss-unreal-native-build-1/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp`
(`ModuleInput{lifetime,monolithic,images,find_export}`,
`ModuleImage{base,size,readable_ranges}`); `PsScanAbi.h`
(`PS_MAPPED_IMAGE_ELF64=1`, `PsMappedExtent`, ELF image_base zero rule);
`PsMappedImageCaller.hpp` / `OwnedMappedImageCaller.hpp` validations; PLH
`OwnedMemory.hpp`; worker-1 `src/live_module_facts.{hpp,cpp}`,
`live_module_os_ops.hpp`, `sce_module_abi.hpp`.

## Refusal-code table (every path fires; distinct code; no partial output)

| Code | Fires when | Test |
|---|---|---|
| `SupplierNotCaptured` | `facts().status == NotCaptured` (supplier ctor guarantees this pre-capture; `Ok==0` hazard avoided by worker-1's ctor) | T2 |
| `SupplierAlreadyReleased` | status `AlreadyReleased` | (status switch; not reachable without release-then-convert, code path identical to T2-T7) |
| `SupplierInvalidArgument` | status `InvalidArgument` (null ops members) | T7 |
| `SupplierNoImageAnchor` | status `NoImageAnchor` (no link-map match, no explicit name; never dlopen(NULL)) | T4 |
| `SupplierRetainFailed` | status `RetainFailed` (RTLD_NOLOAD probe null → zero facts) | T3 |
| `SupplierModuleInfoNotObtained` | status `ModuleInfoNotObtained` (degraded retention+identity only; may NOT be presented as full facts). The observed-base opt-in flag does NOT bypass this gate (asserted). | T5 (x2) |
| `SupplierModuleInfoRejected` | status `ModuleInfoRejected` (kernel violated pinned ABI) | T6 |
| `SupplierRetentionProbeFailed` | status `RetentionProbeFailed` | (switch) |
| `SupplierStatusNotFullFacts` | any other non-Ok status (future statuses) | (default switch) |
| `GameExecutablePathMissing` | Ok snapshot but `executable_path` empty — refuse, never fabricate, never fall back to `image_name` | T8 |
| `GameExecutablePathInvalid` | observed path not absolute/normalized/filename (`app0/eboot.bin`, `/app0/../eboot.bin`, `/app0/`) | T9 (x3) |
| `ModuleFilePathInvalid` | caller-supplied module path `""`, relative, non-normalized, or no filename | T10 (x4) |
| `ObservedBaseNotPermitted` | Ok snapshot without obtained module info and `allow_observed_base_as_module_image_base == false` (default) | defensive: unreachable through the current supplier because `Ok ⇒ module_info.obtained` (`live_module_facts.cpp` lines 372-376); kept per brief |
| `NoReadableMappings` | `anchor_mapping_count == 0` (ModuleSnapshot requires non-empty ranges) | T11 |
| `InvalidModuleImageSpan` | span empty/null/unrepresentable | (internal checks) |
| `InvalidReadableMappings` | mappings present but none intersects the module image extent | T12 |
| `InternalFailure` | unexpected exception inside the adapter (noexcept wrapper) | (defensive) |

On refusal, `LmfConversionResult::facts` stays value-initialized (asserted in
T2) and `error_message` carries a static explanatory string.

## Conversion mapping (verified field-by-field in T1)

- `module_file_path` ← caller argument (required; validated absolute +
  normalized + filename, mirroring `ValidateNativeProgramContext`'s lexical
  rules; file *existence* left to the factory on target).
- `game_executable_path` ← `snapshot.executable_path` (sysctl
  KERN_PROC_PATHNAME, authoritative; empty → refuse).
- `mapped.format = PS_MAPPED_IMAGE_ELF64`.
- `mapped.text_base = 0` — NEVER supplied, stays unresolved (documented in
  header; see deviation D2).
- `mapped.image_base = 0` — see deviation D1.
- `mapped.eh_frame/eh_frame_hdr/pe_exception_directory = {0,0}` (not
  observed by the supplier; never invented).
- `modules.lifetime` — shared_ptr aliasing the supplier (non-owning alias for
  the by-ref overloads; genuine owning alias for the shared_ptr overload,
  proven in T15: supplier survives `reset()` of the original pointer while an
  alias lives). Contract: supplier (and its RTLD_NOLOAD loader reference)
  must outlive the whole scan lease; `release()` only after.
- `modules.monolithic = true` (single MainExe image).
- `modules.images[MainExe]` = `{base=resolved span begin, size=span size,
  readable_ranges=anchor mappings clamped to the span, sorted, overlap/adjacency
  merged}`. Note discovered in test: only mappings CONTAINING `observed_base`
  are recorded as anchor mappings by worker-1's core, so `[0x500000,0x700008)`
  (T1 fake) is not among them.
- `modules.find_export` left unset → downstream `ModuleSnapshot::FindExport`
  throws `std::logic_error("Native module export resolver is not configured")`
  on first use. Documented, not worked around.
- `input_source` empty; `scan_policy` default `OriginalOnly`.
- Result additionally records `resolved_image_base/_end` +
  `image_base_provenance` (`ModuleInfoSpan`, or `ObservedBaseOptIn` only when
  the explicit opt-in flag is set).

## Deviations from the brief (actual headers win, per bounds)

- **D1 — `mapped.image_base`.** Brief: map `image_span_begin` (or opt-in
  `observed_base`) into `mapped.image_base`. Actual immutable chain:
  `PsScanAbi.h` line 86 documents `image_base` as "PE actual image base; ELF
  zero", and `PsMappedImageCaller.hpp` lines 71-73 THROW
  ("Mapped metadata does not match format") for ELF64 with nonzero
  `image_base`; `OwnedMappedScanSource`'s constructor passes facts straight
  into that check. The adapter therefore stores 0 there and instead records
  the resolved base in `LmfConversionResult::resolved_image_base` (+
  provenance) and plants it as the geometric base of
  `modules.images[MainExe]`, which is where the chain actually consumes it.
  Empirically host-proven: probe B.
- **D2 — factory completion with unresolved `text_base`.** Brief requires
  `text_base` never supplied. Actual chain requires a contained, registered
  readable+executable `text_base` (`PsMappedImageCaller::contains` then
  `OwnedMappedImageCaller::permits_code`), so `MakeNativeProgramContext`
  cannot complete from adapter output alone; a target-verified `.text` base
  (or a future pinned program-header parse) must be joined later. Empirically
  host-proven: probes A and C.
- **D3 — worker-1 REPORT suggested `image_name` as a fallback executable
  path.** The brief's sysctl-authoritative rule was implemented instead
  (empty → refuse). No deviation from the brief; deviation from worker-1's
  suggestion, recorded for the controller.
- **D4 — the brief's `PsMappedExtent`/`PS_MAPPED_IMAGE_ELF64` spellings and
  `ModuleInput` field names matched the actual headers exactly** (no change);
  `ScanTarget` lives at `RC::ScanTarget` (not `RC::Unreal::ScanTarget`).

## Host evidence (this machine, `/usr/bin/clang++`; pinned LLVM not used for host C++ per worker-3)

Compile (per TU; full argv in `build-receipts/run-1/receipt.json`):
`/usr/bin/clang++ -std=c++23 -O1 -g -fno-omit-frame-pointer -DPLH_OWNED_MEMORY_HOST_TEST -DRC_HELPERS_BUILD_STATIC -DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC <include dirs> -c <tu> -o <obj>`
Include dirs (order matters; spike first): worker-s1-adapter/src,
tick-file-owner-1/include, tick-witness-1/include,
mapped-candidate-selection-1/include, mapped-candidates-1/include,
mapped-candidate-selection-1/tests, ps-scan-provider-1/include,
mapped-collector-properties-1/include (+tests), build-properties-1/include,
read-lease-1/include, ps-scan-abi-1/include, unreal-native-build-1
port/{unreal,ue4ss,first}/include, deps/first/{Helpers,SinglePassSigScanner}/include,
polyhook-platform-port-1/patched/include, source-dependencies/polyhook,
worker-1-live-supplier/src.

Link:
`/usr/bin/clang++ -Wl,-undefined,dynamic_lookup <8 objects> .superpowers/.../ue4ss-native-mapped-candidate-selection-1/host-harness-1/memory.o -o s1-adapter-host-tests`
(`-undefined,dynamic_lookup` covers six Rust resolver symbols
`_ps_*_v1` that only the Rust test harness provides; no executed path reaches
them — the probes throw inside the C++ validator chain first.)

Run: `ALL S1 ADAPTER HOST TESTS PASSED (102 checks)`, exit 0.

`MakeNativeProgramContext` validation ladder, host-verified with adapter
facts over real mmap'd pages registered via `PLH::OwnedMemory::ScopedRegion`
(probes, exact observed messages):

| Stage (in factory order) | Host result |
|---|---|
| ELF64 format gate | PASSED (probe A progressed past it) |
| `ModuleSnapshot::Create` geometry (lifetime non-null, MainExe present, monolithic==1 image, ranges in-extent, sorted, non-overlapping) | PASSED (also asserted directly in T1/T1b) |
| `ValidateNativeProgramContext` path checks incl. `is_regular_file` on both paths | PASSED (fixture files) |
| ReadLease over registered readable pages + exact span coverage | PASSED (probe A reached the mapped-metadata check) |
| Mapped metadata containment for `text_base` | **NEEDS TARGET** — throws "Mapped metadata or spans exceed supplied readable bytes" while text_base stays unresolved (probe A) |
| ELF64 `image_base == 0` ABI rule | PASSED with adapter facts; nonzero rejected with "Mapped metadata does not match format" (probe B — D1 proof) |
| `permits_code(text_base)` readable+executable registration | **NEEDS TARGET** (host pages registered R-only) — throws "Mapped text base must be registered readable executable bytes" (probe C) |
| Everything past the scanner-source constructor (scans, candidates, serialized-ELF tick) | **NEEDS TARGET** + Rust harness; not claimed here |

## Integration notes for the controller (build_runtime.py NOT edited, per bounds)

- Add two pinned units to `build_runtime.py`'s composition (e.g.
  `native-lmftoloaderfacts`): sources
  `outputs/2026-10-04-zcode-team/worker-s1-adapter/src/lmf_to_native_loader_facts.cpp`
  (+ its header + worker-1 `src/` headers as pinned inputs). Base the argv on
  the pinned `native-loadercontext` unit's argv with two additions:
  `-I <spike>/include` is already first in the fresh-compile path (line 57
  prepends it); additionally add `-I outputs/2026-10-04-zcode-team/worker-1-live-supplier/src`
  (for `live_module_facts.hpp`/`sce_module_abi.hpp`) and this worker's
  `src/`. Target compile uses `prospero-clang++` with `-fms-extensions` etc.
  as in the existing unit argv; the adapter TU is standard C++23 with no new
  dependency beyond `<filesystem>`/`<memory>`.
- `build_runtime.py` line 43 pins `W/include/*` and `W/src/*`; the adapter
  sources live under `outputs/`, so the controller must add them to `pins`
  (source + header hashes from `build-receipts/run-1/receipt.json`) and to
  `new_sources`/`selected_runtime_units`, then extend the line-41
  affected-unit inventory set accordingly (it currently asserts an exact set).
- Link composition: the adapter TU has no new undefined symbols beyond
  libcpp/worker-1's supplier (`_ZN3lmf...` ctor/dtor/capture/facts), so it
  composes with the existing relocatable link; the supplier SCE TU
  (`live_module_os_ops_sce.cpp`, already object-verified by worker-1) must be
  added by the controller when the live path is wired, not by this adapter.
- Ordering/lifetime at runtime: construct supplier → `capture()` → convert
  (ref or owning overload) → build program context (after a target-verified
  text_base is joined, see D2) → keep the supplier alive for the entire scan
  lease → `supplier.release()` only after the lease ends (the non-owning
  alias never does this for you).

## Unresolved / target-only facts

- No PS5/console/network evidence. No proof that a live capture yields
  status Ok (worker-1 experiments 1-7 still gate that), so today's honest
  expectation on target is a refusal (`SupplierModuleInfoNotObtained` in
  degraded mode) unless the exec-handle bridge is verified.
- `text_base` resolution and executable-page registration remain open
  (D2); `find_export` remains unconfigured (documented throw path).
- The `ObservedBaseNotPermitted` refusal is currently unreachable through
  the real supplier (Ok ⇒ obtained) and is covered by code path only.
