# REPORT — worker-staged-pair-emission (PREPARATION, synthetic-validated assumption)

Team 2026-10-04-zcode-team, 2026-10-04. Budget used: ~20 of 80 calls. CLAIM.json
written first; this report last. **Headline: the emission path is fully staged
and proven end-to-end on the synthetic-validated assumption — nothing in this
directory is a validated or promoted fact.**

## 1. Vendored (byte-identical, read-only sources preserved)

From `worker-layout-minimum` (into `vendored/worker-layout-minimum/`, relative
layout kept so the v2 bridge's read-only import works unmodified):
- `validation/assemble_staged_pair.py` — gated staged-pair assembler
  (identity completeness, hypothesis matching, predicate gate, width gate,
  size-fit, refusal-without-output)
- `validation/min_predicates.py` — host mirror validator of the in-process
  predicates (worker-5 verdict vocabulary, hypotheses A/B)
- `validation/test_assembler.py` — 18 host tests; **pass from the vendored tree**
- `validation/RUNBOOK.md` + `validation/example-emission/` (fixture)
- `candidates/layout-candidates.profile.json` — the offsets source the
  assembler consumes (identity: engine 5.6, prospero-x86_64-sysv, pointer_size 8;
  revision/build_id/axes deliberately null)

From `worker-layout-batch3` (into `vendored/worker-layout-batch3/`):
- `bridge/` promotable-set bridge: `assemble_staged_pair_v2.py` (batch2/batch3
  predicate inventory + UE5_FField variant gate), `width_contract.py` (compiled
  width authority + promotable-set logic), `test_bridge.py` (14 tests; **pass
  from the original location** — its ROOT paths are depth-sensitive, vendored
  copy kept byte-identical for provenance)
- `dryrun/promotable-set.json` (55 STAGEABLE_TODAY / 8 NO_WIDTH / 9+1 NO_PREDICATE)
  and `dryrun-receipt.json`
- `dryrun/parser_verify` + `parser_verify_main.cpp` — the vendored
  `RC::Platform::NativeLayouts::ParsePair` harness (binary sha256
  `e9ef3d36…52d6`, matches batch3's dryrun receipt)

## 2. Input data prepared (`input/`)

- `verdicts-synthetic-validated.json` — byte-copy of batch3's dryrun verdicts
  (sha256 `cee1f06e…a267`, **hash-matches batch3's dryrun receipt**): REAL
  retained predicate chain (worker-5 GUA, batch2 ustruct_chain + ffield_variant
  UE5_FField, batch3 walks) on a SYNTHETIC HOST IMAGE, standing in for the
  in-flight anchor probe.
- `anchor-assumptions.json` — the mapping facts being assumed (base 0x400000,
  GUA runtime 0x97ba110 = 0x400000 + 0x93ba110, L7 0x953c000, single-load-bias;
  per worker-span-arbitration the exec spans are CONFIRMED, the W span covering
  0x97ba110 is PREDICTED) + what the anchor probe must return.
- `identity-synthetic.json` — SYNTHETIC-labeled identity (axes 0/0 per the
  batch3 dryrun convention, revision/build_id say NOT-A-SCAN-FACT in the value).
- `scope-hypothesis-A.json`, `identity-unresolved-REFUSAL-DEMO.json` (nulls),
  `parsepair-result.json`, `parsepair-negative-control.json`,
  `negative-control-no-buildid.ini`.

## 3. Full chain executed (synthetic-validated assumption ONLY)

`guobject_array=Validated(A)` + `uobject_item_chain` + `ustruct_chain` +
`ffield_variant=UE5_FField` (+ batch3 strengthening walks) → batch3 bridge →
`emission-synthetic/` (exit 0): **8 sections / 63 member entries** —
FUObjectArray 0xB8(10), TUObjectArray 0x20(7), FUObjectItem 0x18(6),
UObjectBase 0x28(6), UStruct 0xB0(8), UClass 0x200(13), FField 0x30(3),
FProperty 0x70(10); VTableLayout.ini identity-only. Gate integrity held:
UFunction REFUSED (classifier-only predicate, batch2 discipline); 8 members
skipped for absent contract widths (incl. the critical `UStruct::SuperStruct`
— the transcription-overstated member the v2 bridge exists to refuse); 9
width-contract mismatches recorded; strengthening verdicts recorded as evidence,
never gates. Hashes in `emission-receipt.json`.

## 4. ParsePair verification

- Vendored parser accepts the exact emitted bytes: exit 0, `ok:true`, 63
  member entries, 0 vtable entries, identity round-trip exact.
- Negative control (BuildId line removed): REFUSED, exit 3, "Required
  NativeLayout metadata field is missing".

## 5. Explicit refusal points (never faked) — demonstrated live

With today's true identity state (`engine_revision`, `build_id`,
`non_shipping`, `stats` all unresolved), the assembler exits 2: "identity
incomplete: missing engine_revision, build_id, non_shipping, stats — these
must come from the live in-process scan (controller), never defaults", and
creates no output. The mapping does NOT resolve the axes; resolvers are the
live scan or explicit user confirmation (EMISSION-RUNBOOK.md section 2).

## 6. EMISSION-RUNBOOK.md

Exact controller commands for the moment validation lands: preconditions (real
anchor verdict records into `input/verdicts-anchor.json`), emission via the
vendored v2 assembler (note `--scope` takes a file path), the identity/axes
resolution requirement with its refusal points, ParsePair + negative-control
verification, expected per-gate emission subsets, and the activation hand-off
(`PublishStagedNativeLayoutMaps`, then the minimum RUNBOOK steps 4–5).

## 7. Honest limits

The prepared pair is a PREPARATION artifact: verdicts are synthetic-host-fixture
records, identity values are SYNTHETIC-labeled, and the GUA runtime anchor is a
predicted span not yet observed live. If the anchor probe returns anything but
`Validated` (hypothesis A expected), the runbook's step 0 fails and nothing may
be emitted; a `Refuted` kills the winning-side candidates for that snapshot; an
`Inconclusive` requires more spans, never a loosened predicate.
