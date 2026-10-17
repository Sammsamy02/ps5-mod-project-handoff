# REPORT — worker-manifest-finalizer

2026-10-04/05 · proposal-only tooling · no console contact, no nested subagents,
writes only under `outputs/2026-10-04-zcode-team/worker-manifest-finalizer/`.

## What was delivered

- `CLAIM.json` (first) · `finalize_manifest.py` (sha256
  `48654a7b0f88d941a37db80cee33ea958c5aebc44ea2a533caceeccc879ec1b0`) ·
  `host_tests/` fixtures · `receipts/` (4 finalization receipts + 3 finalized
  manifests) · this report (last).
- `finalize_manifest.py` fills the controller change manifest's PENDING fields
  from a completed one-pass run's actual artifacts, refuses anything it cannot
  attest, writes the finalized manifest (default: alongside the input manifest)
  plus a diff-summary printout and a full finalization receipt.

## Fill matrix (all fail-closed)

| manifest field | measured from | cross-checks |
|---|---|---|
| `changed_units[i].sha256` | run receipt `selected_runtime_units[unit].source` (re-measured on disk) | measured == run-receipt `source_sha256`, else refuse; post-run drift only recoverable via `--consumed-source-override UNIT=PATH`, accepted ONLY if the snapshot sha == the run attestation |
| `packaging_units[i].sha256` | staged copy in the candidate `pinned.lineage` overlay (`overlay/<rel>`) | `overlay-manifest.json` attested sha + `.sha256` sidecar, mismatch refuses |
| `import_delta.added/removed` | UND of candidate `build/combined-supplier.o` vs `baseline-composition5-r11.json` `combined_und`, using the SAME pure-python census as gate v2 I1+ | fill ONLY if measured == declared sets; expected-empty fills empty lists + a `measurement` record; anything undeclared/declared-but-absent refuses |

Declared-value rules: PENDING markers and truncated `abcdef12...` prefixes are
filled and diff-recorded; a declared FULL 64-hex sha that disagrees refuses.

Additional refusals: run status drift, composed-object hash drift, candidate
`gaps` non-empty, candidate `pinned.composition_dir` != given run, `baseline_id`
mismatch, packaging rel not in the overlay, any PENDING remaining after
processing (final scan — nothing is written on refusal; exit 2 + refusal
receipt with itemized reasons).

Supplementary (never filled into `import_delta`): with `--defined-baseline
<reference composed object>`, the tool reports the external (non-LOCAL,
defined) symbol delta — the llvm-nm `--extern-only --defined-only` semantics
the dry-build receipt's delta table uses.

## What the tool reports on the dry artifacts (the +3/-0 question)

The dry-build's known "+3/−0 markers" are a **defined-symbol** delta vs
composition-4 run-3, **not** an UND import delta. Measured, not assumed:

- UND delta of dry `build/combined-supplier.o` (c485cc6a…) vs the r11 baseline:
  **added [] / removed []** (211 == 211) — exactly the manifest's expected-empty
  case; the tool fills empty lists and records the measurement.
- With `--defined-baseline` run-3, the tool reports defined-symbol delta
  **+3/−0**: `_ZN2RC8Platform14comp6_drybuild22adapter_staging_markerEv`,
  `_ZN14comp6_drybuild28WCD_PS_DIFF_B_STAGING_MARKERE`,
  `_ZN14comp6_drybuild31WCD_OWNED_DIFF_C_STAGING_MARKERE` — precisely the three
  declared rehearsal markers (local `.LCPI*`/`GCC_except_table*` churn is
  excluded by the external-symbol binding filter). These are rehearsal symbols
  and are correctly NOT part of `import_delta`.

## Host tests (all receipts embed the final tool sha 48654a7b…)

| test | inputs | result |
|---|---|---|
| positive | `host_tests/change-manifest.dry-fixture.json` (sources = what dry1 actually consumed; all 5 shas left PENDING) vs dry1 `run-1` + candidate-a + `--defined-baseline` run-3 | `receipts/finalize-dry-positive.json` — exit 0; fills: program `dfdd5014…`, adapter `0352a615…`, loadercontext `2a17b66c…`, bridge `ebef44bf…` (r11), entry `51771ab5…` (r3); UND +[]/−[] filled with measurement; defined +3/−0 markers; `receipts/change-manifest.dry-fixture.finalized.json` written |
| positive-B | same vs dry1 `run-2` + candidate-b | `receipts/finalize-dry-positive-b.json` — exit 0, identical fills (D1 determinism) |
| negative | fixture + `bogus-no-such-unit` carrying a PENDING sha with no artifact source | `receipts/finalize-dry-negative.json` — exit 2, "no artifact source — unit not in the run receipt selected_runtime_units (368 units known)"; NO finalized manifest written |
| pristine probe | unmodified `comp6-integration/change-manifest.json` vs the same dry artifacts | `receipts/finalize-dry-pristine.json` — exit 2 (see finding below) |

### Real finding surfaced by the host test

The dry1 runs attest the **pre-landing** canonical
`NativeLoaderContext.cpp` (`2a17b66c…`); the live canonical file was
legitimately edited in place afterwards and now measures `bc982594…` —
byte-identical to the controller scaffold copy. The pristine manifest therefore
correctly REFUSES against the dry artifacts (the run cannot attest the current
file), and the positive test used `--consumed-source-override
native-loadercontext=<worker-composition-prep preserved copy>` whose bytes
match the run attestation exactly (the override refuses any snapshot that does
not). At the REAL trigger this refusal path should not fire: the one-pass run
will attest the post-landing content it actually compiled.

The pristine probe also printed two CONTENT DIVERGENCE warnings (program,
adapter): the dry rehearsal consumed stand-in sources, not the controller
scaffold — recorded, never silently accepted. The REAL one-pass finalization
must show zero content divergences.

## Honest limits

- Fill-only mandate: the finalizer does not run gate v2 and does not invent
  MANIFEST-SPEC-v1/M0 fields. The controller manifest's dialect lacks literal
  `manifest_version`/`justification`/`manifest_id` and uses `source`/`sha256`
  rather than `receipt`/`sha256_after`; this is reported in the finalized
  manifest's `finalization.gate_v2_dialect_note` for a conscious reconciliation
  step before gate-v2 manifest mode.
- The finalized manifest embeds a `finalization` provenance block (tool sha,
  artifact shas, per-unit fill/divergence records, measurements); gate v2 reads
  only its known keys, so the block is inert to it.
- The dry fills attest the REHEARSAL lineage (bridge r11, entry r3, stand-in
  sources). They prove the mechanism; they are not the real comp6 values.

## One-line usage for the one-pass runner (its step 3, after packaging, before gate v2)

```bash
python3 outputs/2026-10-04-zcode-team/worker-manifest-finalizer/finalize_manifest.py --manifest outputs/2026-10-04-zcode-controller/comp6-integration/change-manifest.json --composition-run .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-1 --candidate outputs/2026-10-04-zcode-team/worker-comp6-driver/prx-candidate-composition6-1 --defined-baseline .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/coherent-tick-file-witness-runtime.o
```

Exit 0 writes `change-manifest.finalized.json` alongside the input manifest
(plus `…finalized.json.receipt.json`); hand that finalized manifest to
`gate_check_composition5_v2.py --manifest`. Exit 2 = REFUSED: nothing written,
reasons itemized on stderr and in the refusal receipt — reconcile, never
hand-edit around it. (Adjust `--composition-run`/`--candidate` if the runner
uses fresh dirs; drop `--defined-baseline` to skip the supplementary
defined-symbol report.)
