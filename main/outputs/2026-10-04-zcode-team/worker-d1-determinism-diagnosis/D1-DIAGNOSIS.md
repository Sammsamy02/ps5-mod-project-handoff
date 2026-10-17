# D1-DIAGNOSIS — composition-6 candidate-1 vs candidate-2 (worker-d1-determinism-diagnosis)

2026-10-04. Verdict up front: **D1 did not fail because of toolchain nondeterminism
or `__FILE__` path embedding. Candidate-2 was built from the WRONG composed object.**
The original driver's `--reuse-composition-run` falls back to the stale
composition-4/run-3 object instead of re-verifying the composition-6/run-1 object
that candidate-1 was built from. The ~19 KB delta is exactly the composition-6
content (program-unit replacement, initializer merge, Resolution-B vintages) that
candidate-2 is missing. Fix applied and proven: three consecutive packaging runs
(one-pass, fresh-compose, fixed-reuse) now produce bit-identical artifacts and the
gate returns **GO, 8/8 PASS including D1** — see §5.

## 1. Inputs and identity

| candidate | wrapped PRX sha256 (fself) | bytes | bare-original sha | bytes |
|---|---|---|---|---|
| cand-1 (`worker-comp6-driver/prx-candidate-composition6-1`, one-pass full run) | `76ee6af39b407ab7…` | 30,773,943 | `843a94d9600dbe2b…` | 30,758,168 |
| cand-2 (`worker-comp6-driver/prx-candidate-composition6-2`, `--reuse-composition-run`) | `074b5af48259aefc…` | 30,793,159 | `a44165184ee0ee42…` | 30,774,360 |
| delta | DIFFERENT | **+19,216** | DIFFERENT | **+16,192** |

## 2. Build-tree diff (systematic)

**Object inventory (`build/*.o`): identical set, 14 files each; nothing extra/missing.**

- All **12 overlay objects are byte-identical** between candidates
  (sha256 match on constructor_boundary, diagnostic, import_check,
  import_references, lifecycle, memory_lua, probe, runtime_module_entry,
  runtime_start_bridge, sentinels, startup_input, supplier_import_extension).
  This exonerates the lineage-overlay recreate (candidate-2 recreated
  `staging/lineage-overlay-2` in place at 23:41): the recreated overlay produced
  the same compiled units.
- `combined-supplier.o` differs: cand-1 `116f1d702434df33…` 117,793,896 B vs
  cand-2 `4c89229ceb77d299…` 117,871,408 B — **+77,512 B**, consistent with the
  input delta below.

**build-receipt.json (19 commands each; same unit list):**
- `pinned.composition_dir` is the ONLY pinned difference:
  - cand-1: `…/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-1`
  - cand-2: `…/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3`
- Every other argv difference is output-path style only (cand-1 absolute `-o`
  paths vs cand-2 ROOT-relative). Proven non-load-bearing: the 12 overlay
  objects compiled with different `-o` styles are byte-identical. cand-2's own
  build log shows `llvm-readelf -h` and the `ld.lld -r` input list terminating in
  `…/ue4ss-native-supplier-composition-4/run-3/coherent-tick-file-witness-runtime.o`.

## 3. Root cause (proven, with the exact mechanism)

`worker-comp6-driver/build_runtime_composition6.py` line 627:

```python
args.composition_run = RUN4 if args.reuse_composition_run else RUN6
```

`--reuse-composition-run` selects **RUN4 (composition-4/run-3)** — the stale
pre-comp6 composed object (`45dd9901db7d626e…`, 117,779,552 B, built 16:35) —
while candidate-1's Phase A composed **RUN6 (composition-6/run-1)**
(`4dd4f309075cdc3e…`, 117,702,184 B, built 23:29). The reuse path's sha check
(`compose_phase`, lines 345–355) only verifies RUN4's object against **RUN4's own
receipt** — self-consistent, so it printed "reused composed object OK", but it
never compared against the composition-6 object. The reuse fallback is a
composition-5-era leftover; the driver's own help text still documents it
("Phase A, composition-4 run-3 with --reuse-composition-run").

**Why RUN6 ≠ RUN4 (the 19 KB content, itemized).** The two run receipts list the
same 368 unit names, but `source_hashes` differ for 13 paths — exactly the
composition-6 change-set:

- `program` unit: RUN6 has the §3.2 retained replacement
  (`worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`,
  `retained_units_replaced: ["program"]`); RUN4 has none.
- initializer: RUN6 compiles
  `worker-initializer-integration/staging/UnrealInitializer.cpp`
  (+ SeedTable.h, SeedValidation.hpp); RUN4 has the older
  `worker-query-s2-arm/staging/UnrealInitializer.cpp`.
- Resolution-B vintages only in RUN6: `NativeLoaderContext.cpp/.hpp`,
  `lmf_to_native_loader_facts.cpp/.hpp`, `PsMappedImageCaller.hpp`,
  `OwnedMappedImageCaller.hpp`, `Platform/NativeProgramContext.hpp`,
  `PS5PadConfig.hpp` (bootstrap pad unit).
- freshly compiled units: 16 (RUN6) vs 15 (RUN4).

Symbol census on the composed objects (llvm-nm, extern+defined): 56,089 (RUN6)
vs 56,143 (RUN4); 55 symbols only in RUN4 (the `RC::Output::send<…>` family and
friends — dead code the final link strips), 1 only in RUN6.

**Byte attribution chain (cand-2 − cand-1, bare ELF section table):**
`.text` +11,936; `.eh_frame` +9,104; `.eh_frame_hdr` +2,464; `.gcc_except_table`
+396; `.rodata` −4,400; `.data.rel.ro` −224; `.rela.dyn` −192; listed-section
sum +19,084, file-level delta **+16,192** after alignment/padding adjustments
(−2,892 across section boundaries); fself container overhead brings the wrapped
delta to **+19,216**. `.dynsym` identical (0x12a8): same exported surface,
different code behind it. This is genuine different code, not a toolchain
artifact.

**Hypotheses disposition:**
- (a) overlay recreate pulled different header content — REFUTED (12/12 overlay
  objects byte-identical; both receipts pin the same overlay path and manifest).
- (b) nondeterministic toolchain step (make_fself / packager) — REFUTED by
  experiment (§4/§5): three independent runs produce bit-identical artifacts.
- (c) different unit sets / argv — REFUTED as cause (same 19-command structure,
  same 13 inputs); the only real input difference is WHICH composed object the
  link consumed — CONFIRMED as the root cause.
- `__FILE__`-class path embedding — NOT PRESENT here: with identical inputs the
  outputs are bit-identical even though run dir, overlay dir and out dir paths
  all differ between cand-1 and cand-3/4 (paths don't leak into these artifacts;
  the dry-build's ~8 B class applied to its staging pattern, not this one).

**Consequence worth recording:** candidate-2 was not "candidate-1 plus noise" —
it silently lacked the composition-6 program/initializer content (a lineage
regression) while passing 12 of 13 checks. D1 is the check that caught it.

## 4. The fix

Procedure-B copy of the driver under this worker dir:
`outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/build_runtime_composition6_d1fix.py`
(sha256 `27d85da3826fdbf0090869af86f7094f0211b498e820c2d003aa4520a452d550`,
generated by `make_d1fix_driver.py` with uniqueness-asserted edits, each tagged
`D1FIX E#`):

- **E4 (THE FIX):** `--reuse-composition-run` now re-verifies the
  composition-6/run-1 object itself (`args.composition_run = RUN6`
  unconditionally). The stale RUN4 fallback is gone.
- E1/E2/E3 scope isolation (MY → this worker dir; receipts and the pinned fixed
  compose script resolve to the original driver's files, sha-checked at runtime).
- E6/E7 explicit `--overlay-dir` so consecutive packaging runs can coexist
  (refuse-if-exists guard retained).
- Host-python3.9 note: the E6 signature is intentionally unannotated
  (`overlay_dir=None`, no `Path | None`) — this Mac's python3 is 3.9.6.
- The original `worker-comp6-driver/build_runtime_composition6.py` was NOT
  modified (not my file); upstream should land the same one-line E4 change.

## 5. Proof (D1 PASS)

1. **candidate-3** — fresh Phase A compose under this worker dir
   (`ue4ss-native-supplier-composition-6/run-1`), packaging with overlay
   `staging/lineage-overlay-2-d1-c3`: composed object came out
   `4dd4f309075cdc3e…` — **bit-identical to RUN6** (compose determinism
   re-proven independently of the dry-build).
2. **candidate-4** — the FIXED reuse path: log line
   `[gate] reused composed object OK: 4dd4f309075cdc3e... (117702184 bytes)`.
3. Result — **three consecutive packaging runs are byte-identical at every
   artifact level** (bare-original, bare-sdk, wrapped fself), including against
   the original shipped candidate-1:

| artifact | cand-1 (one-pass) | cand-3 (fresh compose) | cand-4 (fixed reuse) |
|---|---|---|---|
| bare-original | `843a94d9600dbe2b…` | `843a94d9600dbe2b…` | `843a94d9600dbe2b…` |
| bare-sdk | `bce857c5f26b46f3…` | `bce857c5f26b46f3…` | `bce857c5f26b46f3…` |
| wrapped fself | `76ee6af39b407ab7…` | `76ee6af39b407ab7…` | `76ee6af39b407ab7…` |

Zero-byte delta: **no `__FILE__`-class exception is needed and the gate's
determinism.json alternative format is not required** — plain byte-identity
holds.

**Gate commands that now pass (exit 0, verdict GO, 8/8 PASS incl. D1):**

```sh
# fixed pair (candidate-3 vs candidate-4):
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir  outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-3 \
  --compare-dir outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-4 \
  --json outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/gate-receipt-d1fix-c3-vs-c4.json

# canonical pair (shipped candidate-1 vs reproduction candidate-3):
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir  outputs/2026-10-04-zcode-team/worker-comp6-driver/prx-candidate-composition6-1 \
  --compare-dir outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/prx-candidate-composition6-3 \
  --json outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/gate-receipt-d1fix-c1-vs-c3.json
```

Both receipts recorded under this worker dir; D1 line reads
"identical sha256 across two builds: 843a94d9600dbe2b… vs 843a94d9600dbe2b…".

## 6. Scope

Writes confined to `outputs/2026-10-04-zcode-team/worker-d1-determinism-diagnosis/`
plus the script-owned run/candidate dirs inside it. Frozen spike trees and the
real transport ledger read-only (mirror written here, as the dry-build
precedent). No console contact, no nested subagents. The original comp6 driver
file untouched.
