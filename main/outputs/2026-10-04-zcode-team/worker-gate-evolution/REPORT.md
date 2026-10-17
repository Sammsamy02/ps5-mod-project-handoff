# REPORT — worker-gate-evolution (gate evolution for composition-5 changes)

2026-10-04 · PROPOSALS + tooling confined to
`outputs/2026-10-04-zcode-team/worker-gate-evolution/`; the live gate
(`worker-composition5-gate/gate_check_composition5.py`) and
`worker-prx-link-gaps/package_prx.py` were NOT modified. No console contact,
no nested subagents. Read-only on all build/spike trees.

## 1. What happens today if the composition legitimately changes (task 1)

Chain: composed object UND set -> `derive_contract()` -> patched Packager
harness -> static checks -> gate I1/P1.

- **The extended contract is NOT derived from the composed object's UND set.**
  `package_prx.py:derive_contract()` (worker-prx-link-gaps, ~line 455) appends
  exactly the five HARDCODED `EXTENDED_IMPORTS` rows (4× libkernel sce* +
  qsort, lines 152-158) to the PINNED 191-import contract, cloning each
  identity from a donor row. The patched Packager harness then pins
  `required_strong == 196` and `names.Count != 196` (`patch_packager`, ~line
  478+: literal string substitutions `191 -> 191+added`). The composed run's
  UND set is only MEASURED as evidence (`extended_imports_undefined_in_composed_object`),
  never used to derive the contract.
- **Legitimate ADDITION of a strong import** (e.g. a configured-roots unit
  calls a new sce* function): either the build refuses at package time
  (harness count pins / unresolved strong import) or, if packaged,
  `imports_equal_extended_contract` fails -> gate **P1 FAIL** (failing list)
  and **I1 FAIL** (`imports != 196`).
- **Legitimate REMOVAL** (a unit now resolves a former import internally):
  final import table < 196 -> `imports_equal_extended_contract` FAIL ->
  **P1 + I1 FAIL**, and the gate's only justification path can NEVER approve
  it: `check_i1` accepts a delta only when
  `justified and delta and delta > 0` (gate line ~486) — negative deltas are
  structurally inadmissible today.
- **The existing escape hatch is a rubber-stamp-shaped hole.**
  `IMPORT-DELTA-JUSTIFIED.md` in the build dir approves ANY positive count
  delta without naming the symbol, without receipts, and without itemizing
  removals. `GATE-CHECKS.md` claims "delta names listed there" but the code
  only tests file existence.
- **D1** (double-build determinism) and **S1** (±5% vs the 30,771,688-byte
  comp4 baseline) behave correctly under kilobyte-scale legitimate changes —
  they just need fresh evidence per candidate (D1) and stay silent otherwise.
- **Side finding (attestation drift):** the archived
  `worker-composition5-gate/gate-receipt-composition5-r5.json` no longer
  matches the current `prx-candidate-composition5-1` contents: it recorded
  bare PRX sha `3695d100…` / 30,772,008 bytes; the dir now holds
  `7774b663…` / 30,774,360 bytes. The dir was rebuilt after r5 was written.
  The gate re-run today (live and v2 identically) still says GO on the
  current bytes, but the controller should NOT treat r5 as describing the
  bytes currently on disk — re-run the gate before any staging decision.

## 2. Evolved contract (task 2)

`MANIFEST-SPEC.md` in this directory is the normative spec. Summary:

- Per-run **composition-change manifest** (manifest_version 1): unique
  `manifest_id`, pinned `baseline_id`, non-empty justification, design
  receipts, `changed_units[]` (unit, role, per-source `sha256_after`,
  link to the replace/extra receipt JSON that attests those shas), and
  `import_delta` with additions AND removals itemized as exact name sets.
- **I1 in manifest mode**: the base check's expectation is re-targeted to
  `len(baseline contract names) + |added| - |removed|`; a separate **I1+**
  computes the combined object's UND set (pure-python ELF64 parse,
  extended-section-numbering safe — the 117 MB combined object has
  `e_shnum == 0`) and demands `actual_added == declared_added` AND
  `actual_removed == declared_removed` EXACTLY, refusing undeclared drift in
  either direction and naming offenders in the receipt; the derived contract
  name set must equal `(baseline ∪ added) \ removed`, and
  `imports_equal_extended_contract` must still pass.
- **Resolution B checks**: B1 nm-verifies every declared configured-roots
  symbol in its declared artifact (combined object or per-unit .o, exact or
  prefix, defined/undefined); B2 proves the bridge still refuses substitution
  by comparing the bridge object's EXTERNAL CALLEE SET (call-range relocs vs
  symbols not defined in the object; local `.L*`/`.LCPI*` labels and
  data-address GOT refs `_ZTI*`/`_ZTV*`/`__dso_handle` excluded — this stays
  equal across pure recompilations and changes iff the bridge is rewired)
  against the frozen r11 baseline; B3 requires `game_identity_facts.
  path_untouched: true` plus the identity refusal marker
  ("observed executable path is empty; refusing to fabricate
  game_executable_path", lmf_to_native_loader_facts.cpp:144) still present in
  the combined object and the `game_executable_path` string count not
  regressed (r11: 3 occurrences).
- **D2 analog** (option c per worker-d2-textbase-design): guard strings
  ("Mapped metadata or spans exceed supplied readable bytes",
  "Mapped text base must be registered readable executable bytes") must
  survive in the combined object (the diffs keep the messages; their absence
  would mean a guard was deleted), plus sha256 pins of every changed
  source/header (e.g. the three option-(c) diffs A/B/C), verified against
  files reachable from the manifest. Object-level nm evidence for D2 is
  inherently thin (the diffs are control-flow/comment changes with no new
  symbols); the shas + guard strings + unchanged import delta are the
  honest, checkable subset — declared as such, not overstated.

## 3. Implementation + host tests (task 3)

`gate_check_composition5_v2.py` — fork of the live gate. Without `--manifest`
every original check function is byte-for-byte the live code; the only
additions are the manifest-mode machinery and two receipt fields
(`gate_variant`, `manifest`, `baseline`). Baseline:
`baseline-composition5-r11.json` (frozen r11 UND 211 / contract 196 / bridge
external callees 40 / identity + D2 strings), generated by read-only parsing
of `prx-candidate-composition5-1`.

Host tests: `host_tests/run_host_tests.py` — receipts in
`host_tests/receipts/` (final durable log: `host-test-run-1.log`, 30 PASS
assertions, 0 FAIL), fixtures in `host_tests/synth/` (synthetic ELF64
"combined objects" compiled with the pinned LLVM clang,
`--target=x86_64-unknown-linux-gnu`; real entry/bridge/PRX artifacts
hardlinked). Every assertion PASS:

- **A (parity)**: no-manifest run on the real r11 dir vs the LIVE gate in the
  same moment: exit codes equal, stdout byte-identical (modulo the trailing
  `receipt: <path>` line), receipt `checks` arrays equal, verdict GO equal to
  the archived r5 receipt. NOTE: the checks-array comparison is against a
  same-moment live run, not the r5 file — see the side finding above.
- **B (matching manifest passes)**: synthetic combined object with UND delta
  exactly `{sd}`, synthetic frozen baseline, manifest declaring `+sd`, a
  receipted changed unit, Resolution B (defined symbol in combined, bridge
  unchanged, identity untouched), D2 guard strings + a real overlay-header
  sha pin -> full **GO** with M0 M1 I1+ B1 B2 B3 D2 all PASS.
- **C (undeclared extra import FAILS)**: object also defines UND `se`,
  manifest declares only `+sd` -> exit 1, I1+ FAIL naming
  `UNDECLARED ADDITIONS: ['se']`, base I1 FAIL (5 != manifest-expected 4).
- **D (wrong declared delta FAILS)**: manifest declares `+sx` which never
  appears -> exit 1, I1+ FAIL naming `sx` as declared-but-absent.
- **E (manifest mode on the real r11 dir)**: empty-delta manifest +
  Resolution B (real defined symbol `_ZN2RC8Platform17NativeLoaderFactsC2…`
  prefix in the real combined object) + D2 pins against the real overlay
  header -> **GO**, all manifest checks PASS, base I1 expectation still 196.
- Negative control: `--manifest /nonexistent` -> M0 FAIL, exit 1 (fail-closed).

## 4. Migration note for the controller (task 4)

When the changed composition (Resolution B configured roots, D2 text_base,
replacement mechanics) lands as a new candidate build dir:

1. **Freeze the new lineage identity first**: run the composition + packaging
   twice (D1 needs both), keep the replace/extra receipts
   (`files: {path: sha256}` maps, same shape as
   `worker-query-s2-arm/integration-patch/receipts/composition4-*-receipt.json`)
   and the packaging `build-receipt.json` + `verification-evidence/`.
2. **Write the manifest** (JSON, anywhere durable; reference it by path):
   - `baseline_id: "composition5-r11"` (the current frozen baseline shipped
     with v2). Do NOT regenerate the baseline for this change.
   - `import_delta`: compute nothing by hand — set `added`/`removed` from the
     DESIGN (e.g. Resolution B units calling new sce* functions adds those
     names; D2 option-(c) adds NONE). I1+ will fail closed and NAME the
     discrepancy if design and build disagree; fix whichever is wrong, never
     widen the manifest to match an unexplained build.
   - `changed_units[]`: one entry per replaced/added unit and per header
     replacement, each with `sha256_after` for every changed source/header
     and the path to its replace/extra receipt.
   - `resolution_b` (when B lands): `expected_symbols` = the mangled names of
     the new configured-roots fields/accessors per unit (nm-level, e.g.
     prefix `_ZN2RC8Platform…` entries); `bridge_unchanged: true` (the r11
     bridge's external callee set is frozen in the baseline; any bridge edit
     must instead declare `bridge_callee_delta` explicitly);
     `game_identity_facts.path_untouched: true`.
   - `d2_text_base` (when D2 option-(c) lands): `guard_strings` = the two
     messages above; `source_pins` = the three diff sites
     (`lmf_to_native_loader_facts.cpp` + `PsMappedImageCaller.hpp` +
     `OwnedMappedImageCaller.hpp`, both variants if the read-lease unit is in
     the affected set) with their post-change sha256.
3. **Re-run receipts**: the gate re-run REPLACES the check portion of the old
   receipts — run BOTH, keeping the old ones for provenance:
   - live gate (unchanged contract view, will FAIL I1/P1 if the import set
     changed — expected and honest),
     `worker-composition5-gate/gate_check_composition5.py --build-dir <new> \
      --compare-dir <second> --json …/gate-receipt-<new>.json`
   - v2 manifest gate:
     `worker-gate-evolution/gate_check_composition5_v2.py --build-dir <new> \
      --compare-dir <second> --manifest <manifest.json> \
      --json …/worker-gate-evolution/receipts/gate-receipt-<new>-manifest.json`
   The staging decision follows the v2 receipt (GO requires ALL of W1..P1,
   M0, M1, I1+, and every declared B*/D2 section PASS). The live-gate FAIL on
   I1 is EXPECTED under a declared delta and is superseded — document that
   supersession by receipt pair, never by editing the live gate.
4. **Human review obligations are unchanged** (GATE-CHECKS.md GO/NO-GO
   section): W2 disassembly review, wiring order, size explanation, staging
   notes with PRX sha256. The manifest automates drift exactness, not review.
5. **Rolling the baseline forward**: only AFTER a changed composition is
   accepted, generate the next frozen baseline with
   `worker-gate-evolution/tools/make_baseline.py --build-dir <accepted dir> \
   --baseline-id composition5-r<next> --out <new baseline>.json` (read-only on
   the build dir; verified to reproduce the shipped r11 baseline key-for-key)
   and pass it via `--baseline`; manifests must then use the new
   `baseline_id`. Never edit a frozen baseline in place.
6. **Immediate housekeeping**: re-run the live gate on
   `prx-candidate-composition5-1` and save a fresh receipt — the archived r5
   receipt references PRX bytes that are no longer on disk (side finding).

## 5. Explicit non-claims

- No claim about the target/console: everything here is offline host-level
  static verification. D1 remains the only run-to-run evidence, and on-console
  behavior is out of scope exactly as GATE-CHECKS.md states.
- The synthetic fixtures (test B/C/D) prove the manifest DELTA MACHINERY, not
  a real composition change; the real-chain proof happens when the controller
  adopts v2 per section 4 (test E already exercises the full real-dir path
  with the real baseline, real bridge, real symbols and real header pins).
- v2 does not make `IMPORT-DELTA-JUSTIFIED.md` disappear from the live gate;
  until the controller adopts v2, the live gate keeps its existing (weaker)
  semantics. This report recommends treating v2 as the acceptance authority
  for any manifest-bearing run.
