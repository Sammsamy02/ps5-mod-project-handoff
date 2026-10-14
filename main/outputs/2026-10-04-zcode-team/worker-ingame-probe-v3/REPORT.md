# REPORT — worker-ingame-probe-v3-resume (2026-10-04)

## Status: DELIVERED AND FROZEN (local build + host verification; NO console run)

FROZEN DELIVERY (controller acceptance check passed; per directive the probe
is NOT held open for optional hardening):

- **Build ID `igf3-de0c0cb644d3`**
- **PRX sha256 `2555b946e73b38f1744a036770103916bffb3b7397e0f7e9ef9dd101e448cd75`
  (53165 B)** — `modbridge_ingame_facts.prx` (top level) ==
  `determinism/run1/` == `determinism/run2/` byte-identical
- Inner ELF (decryption-oracle target): 109776 B,
  sha256 `c52e582d95dff8665911355636049f848177953101009a7da6b48923597e5637`
- Contract sha256 `1591387c0809dae3c8066e3b4b952dc8a3828a7fb1cb6ca763eb89bc3bbd1fef`
  (7 rows byte-identical to the frozen 191-import contract)
- Gates at freeze: HOST-CHECKS-PASSED; LOCAL_PACKAGE_CHECKS_PASSED;
  frozen lineage hash verified; forbidden-symbol gate passed.
- Staging: STAGE-PLAN-V3.md (controller stages immediately; one launch).

Deliverables, all inside this output dir only:

- `modbridge_ingame_facts.prx` — built fself, **igf3-de0c0cb644d3**, 53165 B,
  sha256 `2555b946e73b38f1744a036770103916bffb3b7397e0f7e9ef9dd101e448cd75`.
- `RECEIPT.md` — artifact hashes + the 7-row import-identity proof table
  (every row BYTE-IDENTICAL to the frozen 191-import contract; independently
  re-verified post-build by sorted-JSON string equality + packaged-dynsym gate).
- `STAGE-PLAN-V3.md` — same-file replacement plan, ftpsrv decryption-oracle
  expected values, launch ritual, post-run P1–P6 analysis procedure, rollback.
- `determinism/run1/`, `run2/` — full double-build of the FROZEN source set;
  final fself, inner ELF, combined object, and contract all BYTE-IDENTICAL.
- `src/`, `tests/`, `build_ingame.py`, `check_ingame_artifact.py`, `harness/`
  — finished sources + host checks + packaging pipeline (frozen v2 recipe).

## Resume assessment (what I inherited)

Prior worker died (rate limit) mid-build: sources + build scripts + harness
were present (src through 12:55), `tests/writer_host` had been compiled once,
but NO .prx existed, `determinism/` was empty, and no
receipt/STAGE-PLAN-V3/REPORT existed. Host checks were RED on three items.

## Depth extension outcome (120-call budget; closed early per acceptance check)

LANDED in the frozen artifact:
- Bias-hypothesis base enumeration widened: `tier_a_bias_anchors()` records
  the anchor result at the bias-predicted base `0x400000 + p_vaddr` for ALL
  FIVE LOAD specs (ph 0/1/3/7/8) as `bias_anchor_checks` in v2_base_hunt
  (weak anchors flagged `strong:0` — mismatch is not disproof for padding).
- Global Tick/SCO anchor + code-shape evidence is now emitted in v2_base_hunt
  even when ZERO GUA candidates exist — the L0 correspondence is checked and
  recorded in every run, not only when a match reaches tier-c.
- Every promising candidate is cross-checked against all three anchors with
  per-candidate records: Tick byte anchor + code shape, SCO byte anchor +
  code shape (globals, per-match copied), and L7 segment extent consistency
  under BOTH hypotheses with the bias-matching span bounds now recorded
  (`segc_bias_span_start/end`) alongside the free-interpretation window.
- kve_structsize arbitration evidence (first_record_structsize,
  buffer_mod_exact, ksz_144_records) and the 16384-span cap (P1–P6 fields
  emitted per STAGE-PLAN-V3.md §4).

DROPPED (optional hardening, per the acceptance-check directive "do not keep
the probe open"): adversarial writer stress suite beyond the current green
cases, P4 mechanism-replay host test, synthetic GUA bridge smoke test. These
are future hardening, not staging requirements; no open defect is deferred
with them.

## Resume deltas (conflicts between partial sources and amendments, fixed)

1. **Writer 36-bit "failure" was a harness arithmetic bug, not a writer bug.**
   `tests/run_host_checks.py` expected array end `0x510000090` where the
   emitted value `0x500000090` is exact (0x100000090 + 16·0x40000000). The
   formatter (`ig_buf_u64_hex`, fixed 16 nibbles) is correct; fixed the test
   constant. The formatter is unit-tested for 36-bit values (kv + array +
   nested shapes) and all round-trips pass exactly.
2. **Terminator visibility in `tests/vmmap_walk_host.c`.** The ksz=0
   terminator was written as 4 bytes but the walk requires a full 136-byte
   header before inspecting structsize, so stop_reason stayed 0. The test now
   pads the terminator to a full header and counts it into len; stop_reason=1
   at the recorded terminator offset is actually exercised now.
3. **stop_reason semantics made honest (vmmap_walk.c + factstore.h).** New
   code 2 = "partial record at buffer end", distinguished from 1 = "insane/
   short structsize". The fixed-stride mode-2 fallback ends mid-record BY
   CONSTRUCTION and now reports 2 (was conflated with 1; test updated).
   Mode 1 landing on 2 in production means the sysctl buffer itself was cut
   mid-record — worth knowing, now recorded.
4. **Trust gate vs test scale.** `ig_vmmap_trustworthy` (>=8 sane, <=20%
   insane) is right for production (hundreds of real records); the synthetic
   buffer had 5. Added three full-stride sane records so the gate's PASS side
   is exercised (8 sane/1 insane passes; 400%-insane and <8-sane refuse).
5. **v4_base_summary compile blocker.** `uint32_t listed`/`int promoted`
   were redeclared mid-function (uncompilable under -Werror). Removed the
   duplicates; pre-count remains the emitted promoted_count; the matches
   array is evidence. Added `segc_free`/`segc_bias` to the array.
6. **Language-linkage compile blocker.** factstore.h declared
   ig_vmmap_walk/ig_vmmap_trustworthy with C++ linkage in C++ TUs while
   experiments.h re-declared them inside extern "C". Wrapped all factstore.h
   declarations in extern "C" (guard placed at the top of the declaration
   region — first placement landed after the walk declarations and was moved).
7. **Two failed builds recorded and superseded**: build-a7c036d8ac6c (prior
   worker's debris, removed) and build-5e5b6fcf83cd first attempt (linkage
   error, removed and rebuilt clean). Final build dir:
   `determinism/run1` + `run2` (identical twins).

## Controller corrections applied

- **Correction #2 (single-load-bias hypothesis stays OPEN — co-official with
  per-segment correspondence).** Implemented in code, not just prose:
  - The prior draft's seg_consistency "S2" branch checked L7's spec at
    `probed - GUA_vaddr` (the implied bias BASE = L0's start), which can never
    pass and did not test the bias prediction at all. Reworked to test the
    actual bias prediction: L7 expected at `0x400000 + 0x913c000 = 0x953c000`
    and probe at `0x400000 + 0x93ba110 = 0x97ba110`; pass requires the probe
    to land exactly at the predicted address AND the span at the expected L7
    base to match L7 extent/anchor. Free-mapping interpretation (implied base
    `probed - 0x27e110`) unchanged, bit 1; bias bit 2; both = 3; no-span-check
    possible = 4 — recorded separately per candidate, NEVER collapsed.
  - (a) A hit at the W-span-covered ~0x97ba110 is candidate evidence only:
    the tier-b h2s grid/segment probes emit trials whose `segc_*` fields carry
    both interpretations; no arbitration happens on predicate evidence.
  - (b) Every v2_match_page entry now records `segc_free_l7_start`,
    `segc_free_pass`, `segc_bias_l7_start`, `segc_bias_probed_pred`,
    `segc_bias_probed_match`, `segc_bias_pass`; v4 matches carry
    `segc_free`/`segc_bias`.
  - (c) Arbitration requires L7's own correspondence evidence (extent/prot/
    anchor; shared-page 0x9633000 caveat carried in code comments) — the GUA
    predicate pass never arbitrates.
  - (d) Promotion still requires Tick/SCO byte-anchor matches at the proven
    runtime addresses 0x33ac8c0/0x890aa0 AND segc != 0 (code shape verdicts
    recorded alongside, Inconclusive-classifier only).
  - v2_base_hunt emits `bias_l7_start_pred`, `bias_gua_probe_pred`, and an
    explicit `hypotheses_open` note stating both hypotheses are kept open.

- **worker-span-arbitration VERDICT (urgent input, read in full).**
  - STRIDE: v3's mode-1 walk already strides by each record's own
    kve_structsize (the per-record field the arbitration identifies as 144);
    ADDED the arbitration's assertion evidence: `first_record_structsize`
    (record 0's own structsize, expect 144), `buffer_mod_exact`
    (len % first_ksz == 0; both observed v1/v2 buffers divide exactly:
    10017×144 / 10088×144) — a modulo failure now REFUSES trust (needs-map),
    never guessed strides. Added `ksz_144_records` counter (P1) to the walk
    stats and host test (exactly the synthetic 144-byte record counts 1).
  - CAP: `IG_SPAN_MAX` raised 2048 → **16384**, covering the observed full
    record counts (10017 / 10088) with margin; total/retained/truncated are
    recorded explicitly in v1_vmmap_full (the prior 1024-cap + corrupted
    pages are NOT treated as a complete map — v3 supersedes them per-run).
  - High family kept: no 36-bit assumption exists in the v3 walk (u64 spans,
    sanity bounds only: start<=end, size <= 2^44, prot <= 0xff) — the 49
    high-family spans up to 0x306ae60000 pass through verbatim.
  - P1/P3/P5/P6 are recorded as emitted fields/notes; P2/P4 are offline
    analysis steps in STAGE-PLAN-V3.md §4 (mode-2 replica for P4 runs on the
    same buffer and is emitted when the fallback path triggers).

## Evidence summary (all local; nothing native executed)

- HOST-CHECKS-PASSED: writer 5/5 lines (4 parse, 1 flagged-truncated as
  designed), all field expectations exact; vmmap mode-1 exact on 9 synthetic
  variable-size records incl. 36-bit addresses; mode-2 flagged fallback
  stop=2; trust gates exercised both sides.
- LOCAL_PACKAGE_CHECKS_PASSED (7 encoded dynsym imports, 0 weak; forbidden
  symbol gate passed over the whole combined object; init/fini slots 1/1;
  explicit DT_INIT/DT_FINI; SDK 0x11000000; fself segment bytes equal inner
  ELF; bare sha pinned in fself info).
- Determinism: run1 == run2 byte-identical at fself, inner ELF, combined
  object, contract.
- Frozen lineage hash re-verified before building (3b998f09…).

## Not done / not claimed

- No console staging, no launch, no in-game records — STAGE-PLAN-V3.md is the
  controller's runbook. No L7 base, no promoted layout, no validated GUA:
  candidate != validated; the run's outputs decide.
- Old 018 art-merge integration and other queues: out of my scope.
- I did not touch anything outside this output dir; sibling inputs
  (SEGMENT-CORRESPONDENCE.md provenance via the embedded segment table,
  mapping-recovery/vmmap-v3-records.jsonl, span-arbitration VERDICT.md) were
  consumed as specified. The segment table's capture sha citation
  (3b6cfad4…) is carried verbatim from the prior worker's vendored code; I
  did not re-derive the capture this session (recorded as inherited
  provenance, not newly verified).
