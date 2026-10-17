# REPORT — worker-comp6-driver

2026-10-04 · composition-6 driver + §3.2 bounded fixed-script edition +
attestation tooling, rehearsed end-to-end in dry form. Proposal/tooling only:
no console contact, no target execution, no nested subagents; the only writes
outside this worker dir are the script-owned fresh run dir
(`ue4ss-native-supplier-composition-6/rehearsal-1`) and candidate dir
(`prx-candidate-composition6-rehearsal-1`), per the composition-5 precedent.

## 1. Deliverables (this dir: `outputs/2026-10-04-zcode-team/worker-comp6-driver/`)

| file | what |
|---|---|
| `CLAIM.json` | scope + claims (written first) |
| `build_runtime_composition6.py` | the driver — fork of worker-composition-5-rebuild's driver implementing §3.2/§3.3; sha-pins the edition below |
| `build_runtime_composition6_fixed.py` | the §3.2 bounded fixed-script edition (sha256 `49fe2684258229cfe853f695d633a05bd653c71ef111b947ca397205cb3f62e7`), fork of worker-composition-4-fix's script `36cc8199…`; lineage + SEC32-tagged deltas in its docstring |
| `regen_extra_receipt.py` / `regen_replace_receipt.py` | §3.3 step-2 attestation generators |
| `receipts/rehearsal-composition6-{replace,extra}-receipt.json` | rehearsal receipts (92 + 6 files) |
| `REHEARSAL-EVIDENCE.json` | machine-readable rehearsal summary |
| `ONE-PASS-CHECKLIST.md` | exact controller command sequence for the REAL pass |
| `staging/` | gate evidence: `odr-gate.json`, `receipt-preflight.json`, `entry-preflight.json`, `chain-symbols.json`, `overlay-manifest.json`, compose logs, `odr-negative/` (the deliberately diverged ODR copy) |
| `prx-candidate-composition6-rehearsal-1/` | rehearsal PRX candidate + build-receipt (17/17 PASS, gaps `[]`) |

## 2. §3.2 implementation (edition deltas, each rehearsed)

- **SEC32-1** replace-receipt map built BEFORE `validate_inputs` and the units
  loop; pins pre-seeded with exactly the rr files; a replicated rr-aware
  `validate_inputs` admits the edited port header against its attested hash
  (the port header IS in the snapshot-plan pin set — verified — so this
  relaxation is required and is bounded to rr files only).
- **SEC32-2** `:59` strict unit-source check skip-relaxed for exactly rr paths
  (pins carry the attested hash); originals loop skip-relaxed identically.
- **SEC32-3** `--replace-runtime-unit` extended ONLY for the attested retained
  allow-list `{"program"}`: replacement source must be rr-covered; recompiles
  with its OWN pinned argv (asserted: only source/`-o`/`-MF` differ); fresh
  `.d` walk must be fully covered by pins ∪ rr (`Unpinned
  retained-replacement dependency` otherwise).
- **SEC32-4** affected-set assertion unchanged; receipt gains
  `retained_units_replaced` (old receipts stay diffable).
- **SEC32-5** program.o replacement flows through the existing `replacements`
  map → link-order identical to a runtime-unit replacement.
- **ODR gate (§3.3.1, driver pre-flight)**: port-tree
  `Platform/NativeProgramContext.hpp` vs the staged overlay copy must be
  sha-equal or the driver refuses before anything runs. Routing facts
  verified: every composed TU that includes the header resolves it to the
  port copy (run-3 `.d` closures; W/include has no `Platform/` dir); the flat
  overlay copy is consumed by the packaging bridge/factory TUs.
- **Unchanged mechanisms**: extra-unit adapter redirect, 30-header default
  overlay staging (28 pins frozen at the measured composition-5-2 shas; the
  three Resolution-B headers unpinned-but-measured, optional `REL=SRC=SHA`
  pinning added), entry r3/bridge pins (bridge default bumped to r12,
  measured `feed5254…` — reconcile with the controller REPORT), bootstrap
  pair, extra/replace receipts, 14-symbol chain gate, unmodified
  package_prx.py with the same `-std=c++20` in-memory injection,
  checkpoint/ledger bumped to `zcode_supplier_composition_6`.

## 3. Rehearsal evidence (dry form: UNMODIFIED originals as stand-ins)

Driver exit 0; every gate fired:

- `--verify-only` PASS: edition sha + ODR + receipt coverage (92 replace / 6
  extra files) + entry pre-flight + 14/14 chain symbols vs run-3.
- **ODR negative**: a deliberately diverged staged copy (`staging/odr-negative/…`)
  was REFUSED at pre-flight, exit 1, nothing built.
- **Full pass**: composition-6 edition run into
  `ue4ss-native-supplier-composition-6/rehearsal-1` — status
  `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 368 units,
  `retained_units_replaced == ["program"]`, `program` freshly compiled through
  the new mechanism with its own argv. Undefined delta vs the notify-cancel
  base exactly run-3's shape (`added: [sceKernelGetModuleInfo]`,
  `baseline_extended: true`, nothing removed/changed). Packaging: 17/17 static
  checks PASS, gaps `[]`, candidate PRX 30,792,151 bytes,
  sha256 `0911af0cd5b4a1f2…`.
- **Bit-identity (stronger than the R2 prediction)**: the recompiled
  `program.o` is byte-identical to the base `coherent-full-1/program.o`
  (`cae3279b…`), and all 368 objects — including the path-redirected adapter —
  are bit-identical to run-3; the composed object sha equals run-3's
  (`45dd9901…`). The task's expected "receipt + `__FILE__`-only diff" did NOT
  reproduce as a diff: under this argv none of these TUs embeds their source
  path, so same-content recompilation is byte-stable. Consequence for the REAL
  pass: object deltas there will come from the §2 CONTENT edits only, which
  makes any unexpected path-induced noise impossible to confuse with them.
- Edited-landed gates: skipped in the rehearsal via `--allow-unedited`
  (stand-ins are unmodified by design); the rehearsal refuse-path for these
  gates is code-reviewed and the ODR refusal (same fail-closed class) was
  exercised for real.

## 4. Honest limits

- This rehearsal proves the MECHANISM and the gates, not Resolution-B
  behavior: no §2 edit was exercised with changed content, and
  `native-loadercontext`'s edited-in-place path was only partially exercised
  (the rr branch of `:59` ran, but with equal hashes — the canonical file was
  not modified; nothing outside my dir was written).
- Bridge r12's sha was measured at preparation time; the controller must
  reconcile it (checklist P6/F-table) before the real pass.
- The rehearsal receipts attest UNMODIFIED content; the real pass MUST
  regenerate both receipts after the edits land (checklist §1) — stale
  receipts refuse by design.
- No in-game, completion, or quality claim of any kind follows from this
  work; the candidate PRX is statically verified only.

## 5. Controller hand-off

`ONE-PASS-CHECKLIST.md` holds the exact sequence: land/confirm §2.1–§2.7 →
regen two receipts → `--verify-only` → the one `build_runtime_composition6.py`
pass → post-build acceptance table → controller-owned in-game launch.
