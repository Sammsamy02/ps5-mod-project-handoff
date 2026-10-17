# REPORT — worker-bridge-r13-polish

2026-10-04 · Bridge revision chain owner. Applied the r12 GO-WITH-FIXES review
diffs as **r13**, refreshed the production host test, syntax-gated with the
prospero toolchain, and moved the comp6 driver BRIDGE pins to r13. No console
contact, no nested subagents. Deliverable order honored: `CLAIM.json` (first)
→ r13 + refreshed test + receipts → this file (last).

## Shipped bytes (measured 2026-10-04)

| File | sha256 | State |
|---|---|---|
| `worker-module-start-wiring/runtime_start_bridge_r13.cpp` (734 lines) | `10385e2265af0a12236bc7f713ed343a7f13796644aac0de511f6663f579209a` | NEW — the r13 bridge |
| `worker-module-start-wiring/runtime_start_bridge.h` | `4bb024134385fd548e31fa736264ffc4014ddf12e3813137f140c4c6be88e930` | EDITED — P3-4 renumber only |
| `worker-module-start-wiring/runtime_start_bridge_r12.cpp` | `feed52544023360326c17f7015336f90c4906789b344a805cfb6debb67bdd7a9` | UNCHANGED (byte-frozen base) |
| `worker-module-start-wiring/runtime_module_entry_r4.c` | `c97055f40f5a6dd94dd6360c4459538b8130e8b63a806a84d975d7c7604db470` | UNCHANGED |
| `worker-module-start-wiring/host_test_r13/test_r13.cpp` (467 lines) | `4b4c62c140625c5cf5d1a0b1cc713a3c6198a1c83dd419632e54a07e4526a378` | NEW — refreshed production test |
| r13 target object (`worker-bridge-r13-polish/build/runtime_start_bridge_r13.o`) | `16331ebe299e514f8fa2b59e7a77673d920179c4c820eb53a652d5ee40d6c75a` | prospero -c compile product |

The r12→r13 source diff is exactly: the r13 revision-note header comment, the
five review fixes below, and nothing else (verified by `diff`).

## 1. The five review diffs (all applied, all with receipts)

- **P2-1 (activation adopted FIRST)** — `modbridge::native::ActivatedMods
  mods{activation}` now precedes both defensive early returns, per the header's
  ALWAYS-TRANSFERRED contract. Unit-proven in section 5: null-args refusal and
  already-started refusal each release the activation exactly once (r12 leaked
  both; these paths are unreachable from the shipped entry, so this is
  contract hygiene, not a live bug).
- **P3-1 (lease-cap saturation diagnostic)** — the registration loop now
  counts every readable span intersecting the extent (`readable_spans`) and
  emits `bridge_lease_reg_spans` before `bridge_lease_reg`; `registered >= 128`
  continues the count while stopping registrations. Unit-proven: a 129-span
  fixture yields reg=128 / spans=129 with no array overflow.
- **P3-2 (static_assert)** — `static_assert(sizeof(kinfo_vmentry) ==
  kve_sizeof_)` pins the duplicated 1160 constant; it also pins the host
  test's `kinfo_vmentry` shim at host-compile time.
- **P3-3 (probe[8] init)** — `char probe[8] = {0}` in the ladder; unit-proven
  with a base op that returns 0 without writing (falls through honestly,
  source=0) and one that writes (probe rung serves, source=1).
- **P3-4 (MRS_ERR_STOP_EXN 2→5)** — applied to the SHARED header exactly as
  the review proposed. Entry-coherence note: no consumer change was needed.

## 2. Entry-coherence note for the MRS renumber (task item 1's open question)

`MRS_ERR_STOP_EXN` was 2, colliding with `MRS_ERR_CONTEXT`. Consumers checked:

- **Entry r4, module_start**: compares only `MRS_OK` and `MRS_ERR_START` —
  untouched by the renumber (and entry r4 is unchanged on disk, sha re-verified).
- **Entry r4, module_stop**: `stop_result != 0` → `runtime_failure` — value
  passes through; only the diagnostic number changes (2 → 5).
- **post_launch_check.py / rung_check.py / console_batch.py**: key on the
  `runtime_stop_result` / `runtime_stop_failed` TAG names and "nonzero" prose —
  no numeric equality against 2 anywhere (grepped).
- **Backward compat**: frozen historical overlay vintages
  (worker-composition-5-rebuild/lineage-overlay-1, comp6-driver rehearsal
  overlay, comp6-drybuild overlays) still carry `STOP_EXN=2` paired with their
  own frozen bridge TUs — left byte-frozen on purpose; old logs remain
  readable in their own context. Future log readers must re-baseline: a stop
  exception now reads `runtime_stop_result=5`.
- **Compile proof**: entry r4 compiled clean with `prospero-clang -std=c11
  -Wall -Wextra -Werror` against the renumbered header
  (object `4567744d1fe82c7beb0ba8892fdff242fa25b0e1bfeb4c1c2e6113cd0c1c9fd5`).

## 3. Refreshed production test (host_test_r13 supersedes host_test_r8)

host_test_r8 stays on disk untouched; the new `host_test_r13/test_r13.cpp`
includes `../runtime_start_bridge_r13.cpp` and runs **46 checks, ALL PASS,
0 FAIL** (receipts/run-main.txt 39 checks + 7 lease checks in
receipts/run-lease.txt, both rc=0):

- Removed the r9-era `source=3` mkstemp comment debris (rung deleted in r10).
- Replaced the vacuous `bridge_kve_prefix` check (`a==X || a!=Y` could never
  fail) with exact r9-scan assertions (`bridge_kve_distinct0 == 0x7070612f`
  for the "/app" LE prefix, `bridge_kve_eboot` 0→1 across the two fixtures).
- NEW r12/r13-delta section: the REAL `modbridge_runtime_start_with_activation`
  runs end-to-end on host with the conversion chain defined as recording
  mocks (the same symbols the target resolves from the composed object).
  Asserts: both opt-ins seen by the conversion; the PRX stays the conversion
  input; facts carry EMPTY game_executable_path + `configured_game_directory
  == /app0` + `configured_working_directory == mod_root`; `bridge_cfg_gamedir/
  bridge_cfg_workdir == 1` via the diagnostic capture; MRS_OK; ack wiring;
  conversion-refusal path emits `lmf_*` and releases; stop/throw-stop paths
  incl. `MRS_ERR_STOP_EXN == 5`.
- Lease section (fresh child process, REAL OwnedMemory service) finally runs
  green — it had been PENDING since r8. Two root causes found and fixed on the
  test side: (a) the r8-era fixture span [0x8018c000, 0x90000000) lies ABOVE
  the anchored extent end `0x400000 + 0xbc602c8 = 0xc0602c8`, so nothing could
  ever register; (b) the r8-era `ReadRange` length `0x9000000 - 0x8018c000`
  underflowed. The new fixture proves page quantization behaviorally through
  the real service: exact-byte lease passes, the FINAL PARTIAL PAGE passes
  (r11 regression), the page after the quantized end throws, 128-cap
  saturation is visible (reg=128 / spans=129).
- The service TU must be compiled with `-DPLH_OWNED_MEMORY_HOST_TEST` (its
  `#if defined(__PROSPERO__) || defined(PLH_OWNED_MEMORY_HOST_TEST)` guard
  otherwise preprocesses to nothing and the lease symbols link nil).

## 4. Syntax gate + driver pins

- Fast syntax-only gate AND the full `-c` compile pass with the EXACT dry-build
  bridge argv (`-std=c++20 -Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
  -fvisibility=hidden -ffreestanding -fno-builtin`, prospero SDK clang++,
  LLVM-21 on PATH) against a faithful overlay replica
  (`staging/lineage-overlay-2-r13`, manifest `staging/overlay-manifest-r13.json`,
  builder `receipts/make_overlay_r13.py` — 30 canonical-vintage headers + entry
  r4 + bridge r13 as lifecycle units). Receipt:
  `receipts/syntax-gate-r13.json`.
- Import-set comparison vs the controller's `/tmp/r12-preflight/r12.o`:
  undefined sets IDENTICAL (44 == 44; zero new, zero dropped) —
  `receipts/undefined-r13.txt`.
- `worker-comp6-driver/build_runtime_composition6.py` updated (pin block only):
  `BRIDGE_DEFAULT` → r13 path, `BRIDGE_MEASURED_SHA` →
  `10385e22…9209a`, `DEFAULT_STAGE_HEADERS` `runtime_start_bridge.h` pin →
  `4bb02413…e930`, docstring + `--bridge-source/--bridge-sha256` help text.
  `py_compile` clean; pin-vs-disk consistency re-verified programmatically.
- Note for the controller: ONE-PASS-CHECKLIST P6's `--bridge-sha256 <r12-sha>`
  example still builds r12 if passed explicitly; passing NOTHING now ships r13
  (defaults). The checklist's historical P6 row records the r12 delivery event
  and was left as-is.

## 5. Observations recorded, not changed (out of r13's diff scope)

- The r11-era comment inside `bridge_register_readable_ranges` says the extent
  clamp is `0xfc602c8`; the true computed end is `0x400000 + 0xbc602c8 =
  0xc0602c8` (hex slip in the comment only — the code derives it from
  `kImageExtentVaddr` and is correct; the r12 review repeated the slip). The
  page-alignment argument is unaffected. Left untouched to keep the r13 diff
  exactly review-scoped.
- `#include "ue4ss_backend.hpp"` remains unused in the bridge (review P3-4 of
  the entry-review list; harmless, not part of the five accepted diffs).

## Honest limits

- The host start-path test runs the REAL production start function but with
  the conversion chain mocked; it proves wiring/order/diagnostics/ownership,
  not the composed runtime's own conversion semantics. First in-game exercise
  of r13 remains the next launch's evidence (predicted observables unchanged
  from r12 plus `bridge_lease_reg_spans` and `runtime_stop_result=5` in the
  stop-exception case only).
- No in-game, completion, or quality claim of any kind follows from this work.
- The comp6 driver was not executed end-to-end by this worker (its gates write
  into worker-comp6-driver's own dirs); pin edits are compile- and
  consistency-checked only.

## Files

- Canonical (chain): `worker-module-start-wiring/runtime_start_bridge_r13.cpp`,
  `runtime_start_bridge.h`, `host_test_r13/{test_r13.cpp,test_r13}`.
- Delivery copies + evidence: `worker-bridge-r13-polish/delivery/`,
  `build/` (target objects), `receipts/` (host-test-r13.json, syntax-gate-r13.json,
  run-main.txt, run-lease.txt, undefined-r13.txt, undefined-r12-ref.txt,
  make_overlay_r13.py), `staging/` (overlay replica + manifest).
- `CLAIM.json` (first), `REPORT.md` (this file, last).
