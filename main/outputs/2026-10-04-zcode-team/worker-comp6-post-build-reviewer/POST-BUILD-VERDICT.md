# POST-BUILD-VERDICT — composition-6 one-pass (independent post-build review)

Worker: worker-comp6-post-build-reviewer · 2026-10-04, review window ~23:00–23:35 local.
Reviewer is one layer ABOVE the automated gates: every claim below was re-derived
from the artifacts themselves (nm / llvm-objdump / strings / byte-level ELF section
parsing), not taken from gate prose. Baseline for all deltas: composition-4 run-3
(`…/ue4ss-native-supplier-composition-4/run-3`, object sha `45dd9901…`, 117,779,552 bytes).

## 0. Build state at review close (honest timeline)

| time | event |
|---|---|
| 23:00–23:06 | controller pre-flight passes; attestations regenerated (final: 98 rr files / 6 extra files) |
| 23:06–23:11 | attempts 6–7 fail CLOSED in the fixed script's host-receipt input loop (`Changed input:`); diagnosed by this reviewer: host-final-1 pins 4 files that now diverge; 2 rr-covered, 2 D2 headers (`OwnedMappedImageCaller.hpp`, `PsMappedImageCaller.hpp`) were NOT rr-admitted at their canonical paths. Controller admitted them (rr 96→98 files) and relaxed the loop. |
| 23:16 | **Phase A (composition) COMPLETES** → `ue4ss-native-supplier-composition-6/run-1` with receipt.json + composed object; post-compose chain-symbols re-check 14/14 |
| 23:20 | attempt 8 fails CLOSED in **Phase B packaging**: `staged header drift: runtime_start_bridge.h` — measured `4bb02413…` vs driver pin `ecada77e…` (header was edited 22:49, after the pin was recorded). **No candidate PRX produced.** |
| 23:22 | run-1 preserved as `run-1-f1791174131` (forensics); attempt 9 now fails at verify-only on a bridge-source pin (`10385e22… != None`) — controller still iterating at review close |

The composed object at `run-1-f1791174131/coherent-tick-file-witness-runtime.o` hashes
`4dd4f309075cdc3e6391947e4111842bb991935144cd8ee97c61ded32a0358b5` (117,702,184 bytes),
matching its receipt. All findings below are against those exact bytes.

## 1. What the build actually contains (independently verified)

### 1.1 Composition delta vs run-3 = EXACTLY the expected unit set ✓

Receipt-level (`receipt.json`, status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`):
- `retained_units_replaced == ["program"]` — exactly the allow-list.
- `freshly_compiled_units`: run-3's 15 + `program` = 16; delta vs run-3 is `{added: [program], removed: []}`. Nothing else moved.
- Unit counts identical to run-3 (368 compiled / 353 retained); bootstrap triple
  (`activated_mod_source`, `runtime_owner`, `ue4ss_backend`) is receipt-sha AND object-sha
  bit-identical to run-3's — the "bootstrap pair replaced exactly as composition-4/5" held.
- Per-unit provenance, each measured:
  - `program`: source = driver staged copy `worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`,
    sha `75abf2ab…` — byte-identical to the controller `comp6-integration/UE4SSProgram.cpp`;
    argv source redirected to the staged copy; `program.o` on disk re-hashes to the receipt's
    `135392038b21a9d8…`; exit 0.
  - `unreal-unrealinitializer`: source = `worker-initializer-integration/staging/UnrealInitializer.cpp`
    sha `c85c42f7…` (the merged seed-table unit). Its object sha `fe639a859b2f33d1…` is
    **BIT-IDENTICAL** to the compile-gate object recorded in
    `worker-initializer-integration/REPLACE-ARGV.md` (1,676,944 bytes exactly) — the reviewed
    artifact and the shipped artifact are the same bytes.
  - `lmf-loader-adapter` (extra unit, `extra_unit: true`): source = edited canonical
    `W/src/lmf_to_native_loader_facts.cpp` sha `b63f482a…` — byte-identical to the controller
    `comp6-integration` copy. One benign warning (`nodiscard` on void, adapter :111) — noted, cosmetic.
  - `native-loadercontext` (affected): source = edited canonical `W/src/NativeLoaderContext.cpp`
    sha `bc982594…` = controller copy.
- `undefined_delta` vs notify-cancel base: `added: [sceKernelGetModuleInfo]`, `removed: []`,
  `changed_bindings: {}`, count 198 — **identical to run-3's own delta**.

Symbol-level, composed object vs run-3 (nm, itemized; evidence in `evidence/`):
- UNDEFINED set: **identical, zero delta** (198/198 names). No new import entered the composition.
- DEFINED (global) set: 56,143 → 56,089; delta +1 / −56, itemized:
  - **+1**: `_ZZN2RC8Platform28ValidateNativeProgramContextE…UlRKNSt3__14__fs10filesystem4pathEE_clES9_`
    — the lambda of the NEW configured-roots Validate fork. Expected and declared territory.
  - **−56**: all unreachable-tail template instantiations orphaned by the Shape-A initializer
    swap (Output::send<I0/J…> arms, `Signatures::ScanResult` move-assign, ScanError vector
    helpers, FMalloc bit_cast, optional<bool> machinery, libc++ `B8ne180100` internals).
    Since the UND set is unchanged, nothing referenced any of them — pure dead-weight loss
    (object shrank 77,368 bytes). No function of another unit is affected.
- **No delta beyond the expected units.** The −56 set is the composed-level footprint of the
  declared initializer replacement; every removed name is initializer-tail private machinery.

### 1.2 Shape-A initializer claim holds IN THE ARTIFACT ✓ (objdump-level)

- Composed `ScanGame` (`_ZN2RC6Unreal17UnrealInitializer8ScanGameEv`) is **exactly 0x4a4 bytes**
  (`llvm-objdump -h`, section `.text._ZN2RC6Unreal17UnrealInitializer8ScanGameEv`), matching the
  reviewed compile-gate measurement.
- Its complete relocation target list: `fmt::v11::vformat` + 2 fmt value ctors, std::string
  building, `__cxa_allocate_exception` + `__cxa_throw` against `_ZTISt13runtime_error` and the
  refusal literal (`.L.str.26`), `_Unwind_Resume`, string destructors. **No scan-path call of any
  kind.**
- ps_scan relocations, whole composed object: exactly **1** — `.eh_frame` FDE pointer
  (`R_X86_64_PC32 .text.ps_scan` @0x65ffb8), unwind furniture from the psscanbinding unit that
  still defines `ps_scan`. run-3's composed object had **2** (FDE + the ScanGame call site);
  the call site is gone, the FDE remains. In the composed object **zero** executable-section
  relocations target `ps_scan`.
- The compiled seed table `.data.rel.ro.…ScanGameEv.kAllSlots` (0x80 bytes = 4 × 0x20 SlotSeed)
  parses as: `present=1 addr=0x93ba110` / `present=1 addr=0x23d590` / **`present=0 addr=0x0`
  (gmalloc)** / `present=1 addr=0x490aa0` — the landed {0,2,4} table with gmalloc unseeded,
  folded into the binary exactly as designed.
- Refusal string present verbatim: `[Seeds] Required-slot seed table incomplete: present [{}]
  missing [{}]. Arming requires all four REQUIRED slots (SHAPE A safe default,
  LAUNCH9-PREDICTION)…` (present/missing names are runtime-formatted via vformat, hence the
  `{}` placeholders — consistent with the single added initializer import `fmt::vformat`).
- Legacy contrast (run-3 initializer object): `ps_scan` in UND, ScanGame carries 1 ps_scan
  call reloc + 17 DoScan lambda sections; new initializer object: 0 ps_scan, 0 lambdas,
  241 UND vs legacy 254.

### 1.3 Configured-roots path present in the program object ✓

- Both Validate-fork error strings present verbatim in the composed object:
  `Native program requires an observed game executable path or an explicit configured game directory`
  and `Native configured roots must be absolute normalized directory paths without NUL`.
- `ValidateNativeProgramContext` defined (weak) + the new path-validation lambda (the +1 symbol above);
  both `MakeNativeProgramContext` overloads defined (`(NativeLoaderFacts)` and
  `(NativeLoaderFacts, vector<byte>`) — the sole expected definer set (fresh loadercontext unit).
- Program-side usage in the artifact: the startup banner string
  `unobserved on this platform; configured game directory: {}` is present (UTF-16, @0x4680e) —
  from the replaced program unit's guarded banner (no stat of unobserved path) and
  `m_native_context.configured_game_directory` derivation sites (staged copy lines 433/775-776).
- ODR gate evidence: `odr-gate.json` `byte_identical: true` at the EDITED port header sha
  `0a068bb9…` (≠ pre-landing `66499615…`); chain-symbols 14/14 DEFINED against the run-1
  composed object (re-checked post-compose, not only pre-flight).

### 1.4 Automated gate receipts for the REAL pass — mostly NOT YET DELIVERED ✗

| gate | claimed GO | independently read |
|---|---|---|
| run `receipt.json` status/units/delta | GO | **VERIFIED** (§1.1) |
| chain gate 14/14 | GO | **VERIFIED** (staging/chain-symbols.json @23:16, all true) |
| ODR gate v2 byte-identical | GO | **VERIFIED** (odr-gate.json @23:22, true at edited sha) |
| PRX 17/17 static checks | claimed for the REAL candidate | **NOT DELIVERED** — `prx-candidate-composition6-1` does not exist; only `…rehearsal-1` (dry stand-ins). Packaging REFUSED at 23:20 on `runtime_start_bridge.h` drift (measured `4bb02413…` vs pinned `ecada77e…`; header edited 22:49). No `build-receipt.json` for the real pass. |
| ODR emitter check `receipt-one-pass.json` | expected post-build | **ABSENT** — `worker-odr-emitter-check/` still holds only the script + run-3 receipts. |
| gate v2 manifest verdict | expected | **ABSENT** — `comp6-integration/change-manifest.json` is NOT finalized (`sha256: "PENDING-FROM-ONE-PASS"` on program/adapter/loadercontext, bridge sha `feed5254-pending-final`), has no `resolution_b` section (so gate v2 B1 would report "did not run"), and no gate-v2 receipt exists anywhere for the real pass. |

### 1.5 Delta beyond the expected set (loud items)

1. **Packaging incomplete: no candidate PRX.** Phase B refused (bridge header pin drift).
   Nothing may be staged; the Phase A object is NOT a shippable artifact by itself.
2. **Bridge header moved after pinning**: `runtime_start_bridge.h` edited 22:49
   (sha `4bb02413…`) while the driver still pins `ecada77e…`; attempt 9 also shows a bridge
   cpp pin mismatch (`10385e22… != None`). The §2.7 bridge packaging unit must be re-pinned
   consciously with measured shas (checklist P6 named r12 `feed5254…`, which is unchanged);
   the header drift is an out-of-band edit the manifest does not declare.
3. **Manifest/entry inconsistency (declared but self-contradictory)**: change-manifest
   `packaging_units` lists entry **r4** (`c97055f4…`) while its own `fold_ins.entry_decision`
   parks r4 and ONE-PASS-CHECKLIST P7 pins entry r3 (`51771ab5…`). The build actually
   pre-flighted with r4 (`entry-preflight.json`). One of the two manifest sections is stale;
   must be reconciled before gate v2 can meaningfully pass.
4. **Two in-place-edited D2 headers were initially outside the rr** (`OwnedMappedImageCaller.hpp`,
   `PsMappedImageCaller.hpp`) and were admitted only after fail-closed refusals (rr 96→98 files).
   Final state is consistent; the first attempts are preserved in `/tmp/comp6_onepass_run6/7.log`.
5. Internal-inconsistency note (receipt schema): ONE-PASS-CHECKLIST §4 expected
   `retained_units_replaced` plus program `source`/`source_sha256` in the receipt — both
   present and verified; no schema gap found in the end.
6. Adapter compile warning (`nodiscard` on void-returning `convert_impl`, adapter :111) —
   benign, but it is a new warning the composition-4 pass did not emit.

## 2. GO / NO-GO for staging

**NO-GO for staging.** The Phase A composition is real, internally consistent, and matches the
declared change set exactly — including the Shape-A initializer and configured-roots — verified
at byte/symbol/instruction level. But the pass that would authorize staging is not finished:
- no candidate PRX (packaging refused on bridge header drift),
- no emitter check receipt for the real object,
- no gate v2 manifest verdict (manifest not finalized, PENDING shas, unresolved entry r3/r4
  self-contradiction),
- and a fresh one-pass must complete after the bridge pin reconciliation (attempt 9 still
  failing at review close; run-1 is forensic, per P9 a completed pass needs its own run/candidate).

Restated: what exists is a verified composition candidate, not a verified build pipeline
output. Staging may proceed only after a full one-pass completes with candidate + 17/17 +
emitter receipt + finalized manifest + gate v2 GO, and this reviewer's §1 checks re-run on
the new run dir.

— worker-comp6-post-build-reviewer · evidence: `evidence/` (ScanGame disassembly, distilled
relocation census, run-3 baselines), `verify_out.json` (full verifier dump),
`verify_comp6_build.py` (re-runnable).
