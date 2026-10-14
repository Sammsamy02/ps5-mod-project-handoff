# worker-supplier-ingame-update — supplier retargeted to OBSERVED in-game loader behavior

Date: 2026-10-04. Worker `worker-supplier-ingame-update` of the ZCode team
(`outputs/2026-10-04-zcode-team/`), PS5 mod-manager / UE4SS critical path.
Output area (only place written):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/`
Spike tree was treated as READ-ONLY: patches + vendored copies only, controller applies.

Basis: checkpoint `zcode_ingame_v1_results`
(`outputs/2026-10-04-zcode-controller/console/ingame-v1-facts-2026-10-04.jsonl`,
build igf-f4a91d8123b0, inside the running game):

| Observed fact (record) | Consequence applied |
|---|---|
| g5: KERN_PROC_VMMAP rc 0, 1252 entries, kve 1160B | Promoted to PRIMARY mapping + identity source; VMMAP queried FIRST in `capture()`; TU buffer 512→1408 entries; snapshot capacity 512→1024 with honest truncation |
| g1: `sceKernelGetExecutableModuleHandle` raw 0 | Ops slot now BOUND to an explicitly-disabled refusal: `kRefusalExecHandleRawZeroIngame` (-4001), kernel not called; real `int64_t(void)` call survives behind `LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES` |
| g2: DT_DEBUG absent/unfilled (r_map 0) | `link_map_snapshot` refuses by default with `kRefusalDtDebugUnfilledIngame` (-4002); the `_DYNAMIC`/DT_DEBUG walk survives behind the same gate; a refused walk no longer aborts capture |
| g4: module-list OK (7 modules) but GetModuleInfo on handles rc 0x7FFFFFFF, empty names | Recorded in facts header as unresolved domain/ABI mismatch; module-list route still unused by the supplier |
| g7: firmware string r229358/releases/13.60 | Recorded (`firmware_string_ingame`) |
| g3 (retained): no path source, dl handle null | Degraded dl retention (`LMF_DISABLE_DL_RETENTION`) stays EXACTLY as-is; VmScan captures record `retention.kind == None` honestly instead of faking or aborting |

## Files (vendored patched copies + patches)

Vendored (this output area; the spike/s1 trees untouched):

| File | Role |
|---|---|
| `include/LiveLoaderObservedFacts.hpp` | In-game values + the two distinct refusal codes (-4001/-4002) + VMMAP sizing constants (patch 0001) |
| `include/live_module_os_ops.hpp` | New OPTIONAL `scan_image_base` ops slot; demotion docs for exec-handle/DT_DEBUG routes (patch 0002) |
| `include/live_module_facts.hpp` | `IdentitySource` (None/LinkMap/VmScan), `VmScanFacts` snapshot fields, `CapabilitySet::vm_scan`, `kMaxVmMappings` 512→1024 (patch 0003) |
| `src/live_module_facts.cpp` | VMMAP-first capture; refused walk non-fatal; VmScan identity route; structurally-degraded (non-fatal) retention on that route; rtld NAME route keeps fatal RetainFailed; exec-handle refusal code recorded in `provider_code` (patch 0004) |
| `src/live_module_os_ops_sce.cpp` | Default refusals citing the observed records; gated real implementations; scan op via the vendored scan TU; VMMAP buffer 1408; `make_sce_live_module_os_ops` binds scan + refusal slots (patch 0005) |
| `include/live_module_base_scan.hpp` + `src/live_module_base_scan.cpp` | NEW TU: in-memory ELF base scan vendored from worker-ingame-probe-v2's V2 logic (`scan_image_candidates`/`phdr_table_read`/`dyn_walk`/span-validated reads), restructured to a pure function over (spans, injected raw-read callback) with no global state (patches 0006/0007) |
| `src/lmf_to_native_loader_facts.{hpp,cpp}` | Vendored s1 adapter + new explicit opt-in `allow_vm_scan_base_as_module_image_base` and `LmfImageBaseProvenance::VmScanBase`; all other gates unchanged (patch 0008, base = worker-s1-adapter/src) |
| `host_tests/test_lmf_adapter_host.cpp` | The 102-check s1 suite (kept verbatim) + N1..N5 in-game checks = 169 checks |
| `verify.py`, `build-receipts/run-1/*`, `patches/*`, `CLAIM.json` | Evidence + deliverables |

Patches (unified diffs, apply from workspace root; new files via /dev/null):

```
patches/0001-spike-LiveLoaderObservedFacts-ingame-values.patch          94 lines
patches/0002-spike-live_module_os_ops-scan-slot-and-demotion-docs.patch 107
patches/0003-spike-live_module_facts-identity-vm-scan.patch             172
patches/0004-spike-live_module_facts-vmmap-first-capture.patch          280
patches/0005-spike-live_module_os_ops_sce-demoted-routes-scan.patch     264
patches/0006-spike-new-include-live_module_base_scan.patch              108
patches/0007-spike-new-src-live_module_base_scan.patch                  290
patches/0008-s1-adapter-lmf-vmscan-optin.patch                          159
```
sha256 of each patch is in the receipt-adjacent listing in the worker area
(`shasum -a 256 patches/*.patch`).

## Identity semantics after this update

1. `capture()` order: pid → **VMMAP (first, primary)** → executable path →
   link-map walk (may refuse; recorded verbatim, non-fatal) → identity
   resolution → retention (name route only) → module-info bridge →
   correspondence (reuses the single VMMAP pass; never re-queried).
2. Identity resolution: link-map name anchor when the walk succeeds
   (unchanged semantics, incl. explicit-name corroboration); otherwise, when
   a `scan_image_base` op is bound and VMMAP succeeded, identity =
   `IdentitySource::VmScan` from the scan's chosen candidate
   (`vm_scan.chosen_base`, provenance `"v2_elf_base_scan"`). An uncorroborated
   explicit name only anchors a refused-walk capture when NO scan route
   exists (legacy path preserved for older compositions).
3. Scan internals (vendored V2, unchanged behavior): RX-span starts +
   readable-spans-adjacent-to-exec-span candidates; span-validated Ehdr
   read; phdr table fully read or refused; PT_DYNAMIC parsed under two
   recorded address hypotheses (mode 1 base+vaddr, mode 2 absolute, better
   score wins, tie→1); ranking = LARGEST PT_LOAD extent among images whose
   dynamic parses and that have a PF_X PT_LOAD; ties to lowest span. The
   helper's ONLY dereferences are raw reads through the injected callback,
   invoked strictly after the range is proven inside one readable span.
4. No scan route + refused walk + no name → honest `NoImageAnchor` (never
   dlopen(NULL); N5).
5. Adapter: VmScan-identity snapshots are `ModuleInfoNotObtained` and still
   refused by default; behind the new explicit opt-in the scan result plants
   `modules.images[MainExe].base` (extent = chosen_base + PT_LOAD extent),
   with all other gates (path validation, mappings, geometry) unchanged.

## Test evidence (host, `/usr/bin/clang++ -std=c++23`; spike tree untouched)

`python3 outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/verify.py`
→ **`ALL S1 ADAPTER HOST TESTS PASSED (169 checks)`** (102 original checks
re-run VERBATIM against the patched vendored supplier/core/adapter + 67 new
assertions), then pinned-SDK compiles clean. Full argv/objects/hashes in
`build-receipts/run-1/receipt.json`
(status `SUPPLIER_INGAME_UPDATE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED`).

New checks prove exactly the three required behaviors:

- **VMMAP-first capture path (N1)**: order log shows `query_vmentry` called
  before `link_map_snapshot` and exactly once per capture (the
  correspondence stage reuses the first pass).
- **Both dead routes explicitly refused with distinct codes citing the
  observed facts (N2a/N2b)**: link-map refusal
  `kRefusalDtDebugUnfilledIngame == -4002` surfaces as
  `capabilities.link_map_walk == Failed` + `provider_code`; exec-handle
  refusal `kRefusalExecHandleRawZeroIngame == -4001` surfaces as
  `capabilities.exec_handle == Failed` + the (later-write) `provider_code`;
  distinctness asserted; constants' values asserted against the header.
- **Base-scan helper feeding a successful conversion (N3/N4)**: real mmap'd
  fixture pages carry a synthetic ET_DYN ELF64 (r-- header span adjacent to
  an r-x span, PT_DYNAMIC with DT_DEBUG/DT_NULL), plus a smaller valid decoy
  image and a non-ELF rx span. The vendored scan (driven through the ops
  slot by the REAL core) classifies `r_adjacent_exec`, parses phdrs/dynamic,
  picks the LARGEST extent, records mode 1 + provenance; the adapter then
  converts behind the opt-in with `resolved_image_base == scan base`,
  `image_base_provenance == VmScanBase`, one clamped readable range, and
  `ModuleSnapshot::Create` succeeds. Default options still refuse
  (`SupplierModuleInfoNotObtained`). Span-validation refusals (unreadable
  image via refusing read callback; non-readable span never becomes a
  candidate) are asserted; nothing is guessed.

Original T1..T16 all still pass: the rtld-name route keeps
`RetainFailed`-on-null-retain (T3), `NoImageAnchor` without scan (T4),
degraded refusal (T5), ABI rejection (T6), etc. Factory probes A/B/C behave
identically to the s1 baseline (same three recorded throws).

Target (pinned SDK cross-compile, `prospero-clang++ -std=c++17 -Wall -Wextra
-Werror -fPIC -c`): 0 errors/0 warnings for the patched SCE TU in BOTH
variants (default + `-DLMF_DISABLE_DL_RETENTION`), plus core + base-scan TUs;
all objects verified ELF64 LE ET_REL AMD64. SCE TU undefined symbols
(default): `lmf_scan_image_bases` (bound by the new TU), `dlopen`, `dlclose`,
`getpid`, `memcpy`, `memset`, `sceKernelGetModuleInfo`, `sysctl` — every one
exported by the pinned libkernel.so or provided by the new TU. dl-disabled
variant drops `dlopen`/`dlclose` as expected. Note: the default (non-gated)
SCE TU no longer references `_DYNAMIC` at all (the DT_DEBUG walk is compiled
out), which also keeps V2-style PRX forbidden-symbol gates happy; the
symbol returns only under `LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES`.
No link to a PRX, no target execution, no console contact.

## Recomposition note (controller action)

The extra-unit list is UNCHANGED IN SHAPE: re-run
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py`
with the same `--extra-unit name=source_path` mechanism
(+ `--extend-undefined-baseline`, `--extra-receipt`, and the existing
`--extra-define LMF_DISABLE_DL_RETENTION` when composing the degraded dl
retention), pointing the lm* unit sources at this worker's vendored files:

- lmf supplier unit: `.../worker-supplier-ingame-update/src/live_module_facts.cpp`
- NEW second unit for the scan TU:
  `.../worker-supplier-ingame-update/src/live_module_base_scan.cpp`
  (satisfies the SCE TU's `lmf_scan_image_bases` undefined; same argv
  template; its `-I` set needs this worker's `include/` first so the patched
  headers shadow the spike copies)
- SCE TU: `.../worker-supplier-ingame-update/src/live_module_os_ops_sce.cpp`
- adapter unit: `.../worker-supplier-ingame-update/src/lmf_to_native_loader_facts.cpp`
- source pins: hashes in `build-receipts/run-1/receipt.json` (`source_files`
  + `objects`); patch sha256 list above.

The `--extra-unit` COUNT grows by one (the base-scan TU); no build_runtime.py
edit is required for that beyond adding the unit spec, exactly like the
adapter unit addition the s1 worker documented. Runtime wiring at composition
time: `make_sce_live_module_os_ops()` now binds `scan_image_base` (primary
in-game identity) and the two refusal slots by default; the conversion call
site must pass `LmfConversionOptions{.allow_vm_scan_base_as_module_image_base
= true}` to accept scan-fed identity (opt-in, never default).

## Unresolved / honest limits (no guessing)

- In-game `KERN_PROC_PATHNAME` remains UNOBSERVED (payload-env refuted): the
  adapter still refuses an empty `executable_path`, so a target conversion
  additionally needs a verified in-game path observation (or an explicit
  joined input) — not papered over here.
- `text_base` stays unresolved end-to-end (D2 of the s1 adapter): probes A/C
  still throw at the mapped-text stages on host, now with the scan able to
  SUPPLY phdr-derived facts to a future text-base owner.
- Module-list/GetModuleInfo domain mismatch (g4) unresolved: profile flag
  `allow_loader_handle_as_module_handle` stays false; module info therefore
  stays `NotBound` in-game and scan-fed conversions are provenance-tagged
  (`VmScanBase`), never presented as `ModuleInfoSpan` full facts.
- Scan ranking is the V2 heuristic (largest parsed executable image). It
  matched the real game process in the V2 run's evidence shape; if a title
  ever maps two similarly-sized ELF images, ranking may need a corroborating
  signal — candidates + provenance are recorded so the controller can audit.
- VMMAP scratch stays single-threaded/.bss (1408 entries ≈ 1.63 MiB) per the
  unchanged synchronous-capture contract; the in-game run used 1252.

## Deviations

- Brief said "demote exec-handle and DT_DEBUG routes to explicitly-disabled
  ... keep code paths runtime-gated, not deleted": implemented as default
  refusals + `LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES` compile gate in the SCE
  TU (the ops-table slots remain bound so refusals are observable codes, not
  silent nulls).
- Core changes were required ("core if needed"): VMMAP-first ordering,
  non-fatal refused walk, and the scan identity route are core semantics;
  they were implemented while provably preserving every original 102-check
  behavior (all still green).
- The extended suite lives vendored in this worker's `host_tests/` (the s1
  worker's own tree is read-only for me); its diff base is their test file,
  additions only.

Toolchain note: the prospero wrapper needs `LLVM_CONFIG` from
`outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`
(same as worker-1's verify); `verify.py` sets it automatically.
