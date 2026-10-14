# ONE-PASS-CHECKLIST — composition-6 (Resolution B, configured roots)

Worker: worker-comp6-driver · 2026-10-04. For the CONTROLLER, to be executed as
ONE pass once the controller signals that the §2.1–§2.7 source edits are ready.
Everything below was rehearsed end-to-end in dry form (see REPORT.md); every
gate named here fired in the rehearsal, including the ODR refusal path.

Run everything from the workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what`. No console contact,
no bypass flags exist anywhere in this chain.

## 0. Preconditions (all must hold)

| # | item | expected |
|---|---|---|
| P1 | §2.1 port header edited in place | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp` sha256 DIFFERS from pre-landing `664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868` |
| P2 | §2.2/§2.4 headers edited in place | `ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp` ≠ `18c76cdc…`; `…/include/lmf_to_native_loader_facts.hpp` ≠ `a5bc3b70…` |
| P3 | §2.3 loadercontext TU edited in place | `ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp` ≠ `2a17b66c…` |
| P4 | §2.5 adapter TU edited in place | `ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` ≠ `b6d13bf9…` |
| P5 | §2.6 program TU available as a staged copy | e.g. `outputs/2026-10-04-zcode-controller/comp6-integration/…/UE4SSProgram.cpp`, sha ≠ pre-landing `dfdd5014a0bf7466…`. The port-tree `UE4SSProgram.cpp` should stay UNMODIFIED under this workflow (if it is also edited in place, see F3) |
| P6 | §2.7 bridge r12 delivered | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp` present; its sha256 in the controller comp6-integration REPORT (preparation-time measurement: `feed52544023360326c17f7015336f90c4906789b344a805cfb6debb67bdd7a9`) |
| P7 | entry unchanged | `worker-module-start-wiring/runtime_module_entry_r3.c`, sha `51771ab5c5dce60334740af9e8808e69865690eb4cfbcaee9ba752518fc5a04f` (driver default pin) |
| P8 | rehearsal leftovers moved aside | `worker-comp6-driver/staging/lineage-overlay-2` must NOT exist for the real pass (the dry rehearsal created it): `mv worker-comp6-driver/staging/lineage-overlay-2 worker-comp6-driver/staging/lineage-overlay-2-rehearsal` |
| P9 | fresh run + candidate dirs | `.superpowers/.../ue4ss-native-supplier-composition-6/run-1` and `worker-comp6-driver/prx-candidate-composition6-1` must NOT exist (the dry rehearsal used `rehearsal-1` / `prx-candidate-composition6-rehearsal-1`) |
| P10 | fixed scripts intact | edition sha `49fe2684258229cfe853f695d633a05bd653c71ef111b947ca397205cb3f62e7`, package_prx.py sha `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313` (driver re-checks both) |

The driver REFUSES a no-op rebuild: P1–P5 are enforced by its edited-landed
gates (there is no `--allow-unedited` on the real pass).

## 1. Regenerate the attestations (§3.3 step 2) — AFTER the edits land

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
MY=outputs/2026-10-04-zcode-team/worker-comp6-driver

# extra-receipt (adapter §2.4/§2.5 + unchanged harness/ack carry-forward)
python3 $MY/regen_extra_receipt.py
#   -> $MY/receipts/composition6-extra-receipt.json

# replace-receipt (staged program copy + edited port header + loadercontext TU
#                 + carry-forward of the composition-4 rr + every dep-walk file
#                 not already pinned, incl. the whole fresh program.d closure)
python3 $MY/regen_replace_receipt.py \
  --program-source outputs/2026-10-04-zcode-controller/comp6-integration/<program-dir>/UE4SSProgram.cpp
#   -> $MY/receipts/composition6-replace-receipt.json
```

Notes:
- Defaults point at the canonical in-place-edited paths (P1–P4); only
  `--program-source` normally needs the controller copy path (P5).
- Carry-forward drift REFUSES: if any composition-4-attested file changed,
  stop and reconcile — composition-6 does not authorize those changes.
- If the port-tree `UE4SSProgram.cpp` was ALSO edited in place beside the
  staged copy, add `--file <port UE4SSProgram.cpp>` so `:59` admits it.
- Re-run BOTH tools whenever any attested file changes; the driver re-measures
  and refuses stale receipts.

## 2. Pre-flight (cheap gates only, seconds)

```bash
python3 $MY/build_runtime_composition6.py --verify-only \
  --program-source outputs/2026-10-04-zcode-controller/comp6-integration/<program-dir>/UE4SSProgram.cpp \
  --bridge-sha256 <r12-sha-from-REPORT>
```

Expected: `fixed build_runtime_composition6_fixed.py sha OK`, `ODR
sha-equality OK` (evidence → `staging/odr-gate.json`), `receipt coverage OK`,
`entry attestation + export surface OK`, `startup-chain symbols … 14 of 14`
against the existing composition-4 run-3 object. Any refusal here is the
designed fail-closed path — fix the cause, never bypass.

## 3. The ONE composition-6 pass

```bash
python3 $MY/build_runtime_composition6.py \
  --program-source outputs/2026-10-04-zcode-controller/comp6-integration/<program-dir>/UE4SSProgram.cpp \
  --bridge-sha256 <r12-sha-from-REPORT>
```

Defaults (all rehearsed): fresh run dir
`.superpowers/.../ue4ss-native-supplier-composition-6/run-1`; adapter redirect
left at the canonical (in-place-edited) W/src path; the ODR-gated overlay copy
of `Platform/NativeProgramContext.hpp` restaged from the port header; the full
30-header overlay staging set (28 pins frozen at the composition-5-2 shas, the
three Resolution-B headers unpinned-but-measured); entry r3 + bridge r12 as
packaging lifecycle units; checkpoint `zcode_supplier_composition_6` (mirror:
`worker-comp6-driver/transport-ledger.composition6-mirror.json`); candidate
`worker-comp6-driver/prx-candidate-composition6-1`.

Optional hardening (recommended): pin the two edited W headers explicitly by
appending their measured shas from the receipts:
`--stage-header NativeLoaderContext.hpp=<W path>=<sha> --stage-header lmf_to_native_loader_facts.hpp=<W path>=<sha>`.
If the controller keeps the overlay ODR copy as a separate file instead of
restaging from the port header, pass
`--odr-staged-header <controller copy>` — the gate then demands byte-equality
with the port header (this refusal was rehearsed; exit 1, nothing built).

Phase A runs the bounded edition: §3.2 retained-unit replacement of `program`
from the staged copy with its OWN pinned argv (only source/-o/-MF redirected,
asserted) and the fresh `.d` walk fully covered by pins ∪ replace-receipt;
affected units recompile from the edited canonical sources; adapter recompiles
as an extra unit; bootstrap pair replaced exactly as in composition-4/5.
Phase B runs the unmodified fixed package_prx.py (17/17 static checks).

## 4. Post-build acceptance (all in the run/candidate artifacts)

| check | expectation |
|---|---|
| run `receipt.json` | status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`; `retained_units_replaced == ["program"]`; `program` in `freshly_compiled_units`; program entry `source` == the staged copy path, `source_sha256` == its attested sha; `undefined_delta` shape vs the notify-cancel base identical to run-3's (`added: [sceKernelGetModuleInfo]`, `baseline_extended: true`, nothing removed/changed) |
| chain gate | `staging/chain-symbols.json` — 14/14 DEFINED |
| ODR evidence | `staging/odr-gate.json` — `byte_identical: true` at the EDITED port sha (≠ `66499615…`) |
| packaging | 17/17 static checks PASS, `gaps: []`, `PRX candidate produced and statically verified (no in-game claim)`; record `prx-candidate-composition6-1/modbridge_supplier_composition.prx` sha256 + bytes |
| ledger | provisional mirror `worker-comp6-driver/transport-ledger.composition6-mirror.json` carries `zcode_supplier_composition_6`; controller reconciles the real ledger |

Then, and only then: controller-owned in-game launch (separate rung steps).
This pass produces a candidate PRX; no in-game or completion claim follows
from it.

## 5. Failure playbook (fail-closed everywhere; no bypasses exist)

| refusal | meaning / fix |
|---|---|
| `ODR GATE REFUSED` | port vs staged `NativeProgramContext.hpp` diverge → make them byte-identical (§3.3.1); rehearsed refusal |
| `… is UNEDITED (matches pre-Resolution-B baseline …)` | that §2 edit has not landed → land it (edited-landed gate; rehearsed via `--allow-unedited` only) |
| `replace-receipt does not cover …` / `drifted after receipt generation` | receipts stale or incomplete → re-run the regen tools (§1) |
| `Unpinned retained-replacement dependency` | the edited program/header added a NEW include → re-run regen (its walk now covers it) or pass `--file`; use a FRESH run dir (the failed dir is kept for forensics) |
| `carry-forward file CHANGED since composition-4` | an out-of-scope file changed → reconcile provenance; composition-6 does not authorize it |
| `Changed input: <port UE4SSProgram.cpp>` | port program edited in place but not rr-admitted → add `--file <port UE4SSProgram.cpp>` to regen (F3) or keep the port file unmodified (P5 workflow) |
| `bridge source drift vs pin` | r12 moved → use the attested sha from the controller REPORT (`--bridge-sha256`) |
| `composition run dir already exists` / `output dir exists` / `overlay already exists` | fresh-dir semantics → new names or conscious removal (P8/P9) |
| `Unpinned extra project dependency` | adapter gained a `.superpowers`-local dep not in any receipt → regenerate the extra-receipt with `--file` |
