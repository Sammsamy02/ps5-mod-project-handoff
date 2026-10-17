# REPORT — worker-launch11-vtable-prebuild

2026-10-04 · The `W_FAILS_R_WORKS` vtable-backend branch (LAUNCH10-MATRIX **U-SWAP-2**,
rows **R3/R4**) is now a **staging decision**: the R1 `DispatchDataDetour` backend is
authored, compile-proven under the exact composed toolchain, and staged as a
**PREPARED-UNARMED** composition candidate that is **byte-identical to the launch-10
armed candidate at every artifact level** (measured); the verdict-gated **R2** slot
capture is delivered **build-ready** with machine-verified anchors. Host build only;
no console contact, no nested subagents; no in-game claim anywhere.

Read `CLAIM.json` first (delivered first, updated with measured verdicts); this file
is the closing report. `build-ready/INTEGRATION-NOTE.md` is the matrix wiring note.

## 1. The honest pre-verdict assessment (task 1)

| question | measured answer |
|---|---|
| Does the R1 backend abstraction compile without the verdict? | **YES** — compiled in BOTH modes under the exact 190-arg composed `prospero-clang++` command (extracted from the launch-10 composed run). Disarmed: byte-identical to the launch-10 initializer object. Armed: +360 B of backend machinery, exit 0. |
| Is the design's `#ifdef __PROSPERO__` typedef live in composed builds? | **YES — `__PROSPERO__=1` is predefined by the toolchain** (measured `-dM -E`). An unconditional R1 typedef would have CHANGED runtime behavior of any candidate, so the seam defaults to 0 (PLH selected) and arming is a controller-owned flip. |
| Can R2 be pre-staged? | **NO by task definition and by honesty** — it feeds an armed backend and edits a header consumed across the composed unit set. Delivered build-ready instead (task 2's alternative), with anchors verified against the current port tree and both-mode probe compiles. |
| Can the arm step ride the frozen comp6 script? | **NO — measured blocker** (§4 below). Documented as the row's integration decision; the frozen script was NOT forked. |

## 2. The measured design corrections (load-bearing for VT §5 R1)

1. **"Single construction site" is factually FIVE sites** in the consumed tree
   (`CR1/source`, byte-identical to the design's `source-reconstruction-final-4`
   citation for this header): `DetourInstance.hpp:196` (TDetourInstance::InstallHook)
   + `DetourSubclasses.hpp` :119 (FCallFunctionByNameWithArguments), :265
   (StaticConstructObject), :351/:365 (StaticConstructObject 411-version arms). The
   armed compile fails at each until seamed — measured during the isolated proof.
   Plus `DetourSubclasses.hpp:49` `setDetourScheme(PLH::x64Detour::INPLACE_SHORT)` —
   a PLH prologue-patching detail with no swap meaning (not-on-path target). All
   seamed via a type-seam macro whose disarmed expansion is the exact original tokens.
2. **`__PROSPERO__` is compiler-predefined** — the design's conditional would be live
   everywhere; PREPARED-UNARMED requires the explicit default-0 switch.

## 3. What was built (the compile-safe subset)

- `staging/initializer-vtable-pre/` — the PREPARED-UNARMED vehicle: staged
  `UnrealInitializer.cpp` (c85c42f7... + exactly 2 include-line changes, VT-R1 S5/S6),
  seamed `DetourInstance.hpp`/`DetourSubclasses.hpp` siblings (quoted includes — the
  only header-shadow mechanism the frozen -I order permits, initializer TU only),
  `DispatchDataDetour.hpp` (backend: exact consumed-PLH surface, slot registry,
  identity pre-check, commit-order store, **refusing D-1 seam** — zero `0F 05` bytes),
  launch-10 armed `SeedTable.h`/`SeedValidation.hpp` carried over unchanged.
- `prx-candidate-composition6-l11-a/` (+ `-b` determinism twin): full d1fix-lineage
  composition-6 one-pass, twice. Driver copy `build_runtime_composition6_l11pre.py`
  (`ba0d899ed939286a...` = d1fix `27d85da3...` + docstring + L11PB E1 scope + L11PB E2
  staging path, diff-verified to contain nothing else).
- `apply_seam_vt_r1.py` (exact-anchored, ANCHOR FAILURE aborts) and
  `prove_compile_and_disarm.py` (re-runnable proof gate).
- `build-ready/`: `VT-R2-UNREALVIRTUALBASEVC.diff` + `apply_vt_r2.py` (verify/preview,
  both-mode probe compiles) + `INTEGRATION-NOTE.md`.

## 4. Verification results (all measured, evidence in this dir)

| check | result |
|---|---|
| Disarmed compile identity | staged initializer TU → `983b18e24d32...` (1,925,952 B) == launch-10 composed initializer object, **byte-identical** |
| Disarmed token identity | preprocessed staged chain vs port chain: **IDENTICAL** (token-normalized) — ODR-safe disarmed, measured not argued |
| Armed compile | exit 0, `0180d3743329...` (1,926,312 B); armed tokens differ (seam live) |
| Composition delta vs launch-10 | 368/368 unit names equal; exactly 1 unit differs in SOURCE and its OBJECT is sha-identical; composed runtime `d6214abd88ad167f...` (117,831,360 B) identical; bare-original `5884d88f...`, bare-sdk `b0353bf4...`, wrapped fself `d8cb4a76...` (30,818,247 B) — all == launch-10 |
| Determinism | run-b (fresh compose + package) byte-identical to run-a at composed-object and wrapped-PRX level; 17/17 static packaging checks PASS each pass |
| Gate v2 vs frozen r11 | **GO (offline gate), 15/15** — W1 W2 W3 I1 D1 F1 S1 P1 + M0 M1 I1+ B1 B2 B3 D2; import delta exactly `+[]/-[]` (196 == 196); `gate-receipt-launch11-vtpre-a-vs-l10.json`, failed_checks `[]` |
| R2 anchors + probes | decl-block 1/1, walk-tail capture 2/2 (drift aborts nonzero); disarmed + armed probe TUs compile with the composed `-I/-D` set |

## 5. The arm-step blocker (L11PB-6, blocking for arming only)

Arming changes the `TDetourInstance::PLHDetour` member type, so every TU including
`DetourInstance.hpp` must see the same armed state. Measured: the only two src
consumers are `unreal-unrealinitializer` (staged, replaceable) and `unreal-hooks`
(port source, RETAINED — the frozen fixed script admits only the 6 affected units +
retained `{program}`), and the replaced-unit `-I` order pins frozen shared include
dirs first (no staging shadow for angle includes; the vehicle's quoted sibling
includes reach the initializer TU only). One-TU arming is a genuine ODR violation.
**Consequence:** arming = an integration-time composition decision (new fixed-script
edition admitting `unreal-hooks` replacement or a shadow `-I` slot, with its own
receipts/ODR walk). MATRIX §3.1 R3/R4 already says "decide at integration time,
never bypass" — this note is that decision's input.

## 6. Incident disclosure

During the isolated proof, a compile with an unredirected `-MF` overwrote
`worker-launch10-prebuild/.../run-a/unreal-unrealinitializer.d` (that file only).
Restored the same hour by exact-argv reproduction (the pipeline's determinism is
proven): object reproduced **byte-identical**, `.d` dep list verified path-faithful
to the l10 staging. No other l10 artifact touched; their staging dirs were read-only
copied from.

## 7. Deliverables (this dir)

`CLAIM.json` (first) · this `REPORT.md` (last) · candidate `prx-candidate-composition6-l11-a/`
(deliverable; `-b` = byte-identical determinism reproduction) · receipts
(`composition6-replace-receipt.json` 102 files, `ce714bd8...`; extra `0528d3d6...`
byte-equal to launch-10's; first attempt preserved) · manifest
`composition-change-manifest-launch11-vtpre.json` (`8ee564672b784b6e...`) · gate
receipt (GO 15/15) · driver + seam/proof scripts · `build-ready/` set (R2 diff,
anchor gate, INTEGRATION-NOTE) · logs (compile/stderr/preprocess evidence, probe
objects).

## 8. Honesty bounds

- No claim any swap, thunk, mprotect-on-data, or hook has run in-game; VTABLE-SWAP-DESIGN
  §7 non-claims carry over unchanged.
- `W_FAILS_R_WORKS` does NOT prove D-1 (different page class); the refusing seam stays
  refusing until a controller-approved, separately-designed D-1 step exists.
- Gate S1/D1 and the byte-identities here are packaging-level statements, not quality
  or behavior claims about the console.
- The controller owns the probe reconciliation, the arming flip, and any staging.
