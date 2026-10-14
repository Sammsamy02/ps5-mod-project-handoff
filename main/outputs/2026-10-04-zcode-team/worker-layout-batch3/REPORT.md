# REPORT — worker-layout-batch3: the remaining layout path, commissioned end-to-end

Team 2026-10-04-zcode-team. Output area (only place this worker wrote):
`outputs/2026-10-04-zcode-team/worker-layout-batch3/`. `git status` confirms a
single untracked directory for this worker; every modified file outside it
(e.g. `outputs/ps5-mod-bridge/*`) carries mtimes ~21h before this session
(other workers' work, untouched). No network, no console, no nested subagents;
sibling files consumed read-only (`#include` / `import` only); the vendored
parser was compiled, never modified.

## 1. Deliverables

| File | Purpose |
|---|---|
| `CLAIM.json` | ownership + bounds (written first) |
| `src/chain_walk_predicates.{hpp,cpp}` | batch-3 rung-3 predicates: `probe_ustruct_children_walk`, `probe_ustruct_childproperties_walk`, `probe_uclass_property_links`; same Validated/Inconclusive/Refuted + stable-code conventions (new code blocks 0x55xx/0x56xx/0x57xx), explicit LE decoding, no allocation (Brent cycle detection), no OS calls; every offset cited (SET-5_06 setters verified this session: PropertyLink 0x70 / RefLink 0x78 / DestructorLink 0x80, UField.Next 0x28) |
| `tests/test_chain_walk_predicates.cpp` (+ committed binaries + run logs) | 58-check adversarial host suite: owner back-pointer mismatch, self-cycles, round-trip mismatch, flag-mask violation, non-canonical heads/next, unreadable windows (Inconclusive, never Refuted — batch2 verdict-class parity), invalid GUA views, negative-only codes that must NOT flip verdicts, and a parity test against batch2's `probe_ustruct_chain` on the same synthetic object. 58/58 PASS; clean under `-fsanitize=address,undefined` |
| `bridge/width_contract.py` | WIDTH AUTHORITY: loads widths-1 + widths-2 delta (`UFunction.Func`→16), cross-checks the candidates profile transcription (38 disagreements found, see §3) |
| `bridge/assemble_staged_pair_v2.py` | bridge over layout-minimum's assembler (imported read-only): batch2/batch3 predicate inventory + UE5/UE4 variant gate + contract-width staging. Grammar/identity/duplicate/fit gates inherited verbatim |
| `bridge/test_bridge.py` | 14 host tests: maximal emission shape, variant refutation, unsettled-variant refusal, missing-predicate refusals, identity refusal, inconclusive-GUA refusal, strengthening-not-gating. 14/14 PASS |
| `PROMOTABLE-SET.md` + `dryrun/promotable-set.json` | the complete promotable-set table, per member, cited (deliverable 1) |
| `WIDTH-GATES.md` | width-gate analysis from the port tree, experiments only (deliverable 3) |
| `dryrun/` | end-to-end dry run: `dryrun_chain_fixture.cpp` (executes the REAL batch1→batch2→batch3 chain on one synthetic image), `synthetic-verdicts.json`, `run_dryrun.py`, `parser_verify_main.cpp` (linked against the VENDORED parser), `emission/` (maximal staged pair + manifest), `dryrun-receipt.json` with SHA-256s (deliverable 4) |

## 2. The promotable set (hypothetical validated GUA anchor) — headline

Per-member decisions over the candidates profile (machine truth:
`dryrun/promotable-set.json`): **55 STAGEABLE_TODAY**, 8 BLOCKED_NO_WIDTH,
9 BLOCKED_NO_PREDICATE (UFunction members), 1 BLOCKED_NO_PREDICATE_AND_WIDTH
(UFunction.FunctionFlags). Class-level gates that open them:
`guobject_array` (worker-5) → `uobject_item_chain` (batch2 round-trip
accounting) → `ustruct_chain` (batch2) → `ffield_variant Validated =
UE5_FField` (batch2) for FField/FProperty. UFunction can NEVER be staged
today: batch2's `probe_ufunction` is Inconclusive-classifier-only by design.

## 3. Key findings (this worker's additions to the ladder)

1. **CRITICAL — `UStruct.SuperStruct` was over-transcribed.** The
   layout-minimum candidates profile records width 8 for SuperStruct@0x40,
   but the frozen contract refuses it (widths-1 `unsupported_reason`; widths-2
   `coverage-delta.json` explicitly `"UStruct.SuperStruct": null` — the legacy
   `TNonAccessTrackedObjectPtr` alternative is forward-only). The ORIGINAL
   assembler, fed the candidates file, would stage an unbacked member. The
   batch3 bridge makes the contract the staging authority; the dry-run
   emission records `UStruct::SuperStruct` in `skipped_no_width`.
2. **38 candidates-vs-contract transcription disagreements** in total:
   4 overstated (SuperStruct; UClass ClassFlags/ClassCastFlags/ReferenceSchema
   have no contract width), 5 width_diff (UClass ClassUnique 8→4, FuncMap
   4→80, Interfaces 4→16, NativeFunctionLookupTable 4→16, NetFields 8→16),
   29 understated (contract-backed members the candidates document omits —
   addable candidates with setter-template origin, listed in
   PROMOTABLE-SET.md).
3. **Batch3 predicates close the derivable rung-3 chains** without
   manufacturing widths: Children/UField.Next walk (owner back-pointer is a
   hard refute), ChildProperties/FField.Next walk (Owner==anchor; ArrayDim==0
   refutes; ElementSize==0 / Offset>PropertiesSize are negative-only codes,
   batch2's negative-vs-refute split), UClass CDO back-pointer +
   PropertyLink/RefLink/DestructorLink walks (owner equality deliberately NOT
   asserted on link chains — they include inherited properties owned by super
   structs). All verdict classes mirror batch2 exactly (unreadable window ⇒
   Inconclusive, never Refuted); the parity test pins the mirrored
   UObjectBase classification to batch2's on identical inputs.
4. **Width gates are closable, not permanent** (WIDTH-GATES.md): every
   blocked member has an existing, type-completable getter in the port tree
   (PropertyFlags: `uint64& GetPropertyFlagsBase()`, EPropertyFlags:uint64_t
   at UnrealFlags.hpp:106; FunctionFlags: uint32/EFunctionFlags at :343;
   FField Owner/Next: `FFieldVariant&`/`FField*&` with FFieldVariant fully
   defined at FField.hpp:37; FField.NamePrivate: `FName&`, and FName is fully
   defined at NameTypes.hpp:215-263 — "FName 0/0" is a contract-vocabulary
   fact, not an unknowable size; UObjectBase.NamePrivate already carries
   compiled width 8). The closing mechanism is the widths-2 precedent
   (focused SDK controls + source-proved delta); bit-plausibility narrowing is
   documented as an evidence experiment only. All experiments, zero defaults.

## 4. End-to-end dry run — the maximal staged pair provable TODAY

Executed for real (receipt: `dryrun/dryrun-receipt.json`, all hashes recorded):
synthetic image → worker-5 `probe_guobject_array` (layout A matched, single
hypothesis) → batch2 `probe_ustruct_chain` (4/4 plausible, 4 round-trips,
2 struct-like, first_uclass_like hit) + `probe_ffield_variant` (Validated,
UE5_FField) → batch3 walks (Validated; UC_I_LINK_HEAD_NULL recorded for the
empty RefLink/DestructorLink heads) → verdict records → v2 assembler →
**8 sections, 66 member entries** (FUObjectArray 9, TUObjectArray 6,
FUObjectItem 5, UObjectBase 5, UStruct 7, UClass 12, FField 2, FProperty 9)
with identity `SYNTHETIC-DRYRUN-REVISION/BUILDID` — byte-verified through the
VENDORED `ue4ss-native-layout-parser-1` `ParsePair`: exact bytes accepted,
identity round-trips field-for-field, all 66 (section,key,offset) triples
match the manifest, vtables identity-only, and the negative control
(BuildId line removed) is REFUSED. 5/5 byte-verify checks PASS.

## 5. Honest remaining gates

- Everything above is SYNTHETIC-HOST-FIXTURE evidence: predicate correctness
  on host images, not target facts. ACCEPTED.md stays empty until worker-5
  probes + these predicates run in-process on the real mapped snapshot
  (RUNBOOK path unchanged).
- Identity (EngineRevision/BuildId/axes) remains a controller/scan
  responsibility; the dry-run identity is literally labeled SYNTHETIC.
- Publication wiring (LAYOUT-GATE blocker 2) and FName construction (P2 gap)
  are untouched by this work.
- UFunction: no Validated predicate exists or may exist under the
  classifier-only discipline; rung-5 staging additionally needs the FuncMap
  container internals (worker-6 scope) to even name an anchor.
- The 8 width-blocked members stay unstaged until a widths-3 campaign
  (experiments specified in WIDTH-GATES.md) lands compiled widths in the
  contract; the SuperStruct policy delta is separate and policy-level.
- The `uobject_item_chain` assembler record is satisfied from batch2's
  round-trip accounting (worker-5 has no separate probe by that name) — the
  mapping is documented in the verdict records themselves and in
  PROMOTABLE-SET.md.

## 6. Verification summary

- C++ batch3 suite: 58/58 PASS (`tests/test-run-normal.txt`), ASan/UBSan clean
  (`tests/test-run-asan.txt`, binary + dSYM committed like batch2's).
- Bridge suite: 14/14 PASS (`tests/test-run-bridge.txt`).
- Dry run: predicate chain 11/11 fixture checks, assembler emits, vendored
  parser byte-verify 5/5 PASS (`dryrun/dryrun-receipt.json`).
- Reproduce: `python3 bridge/test_bridge.py`;
  `python3 dryrun/run_dryrun.py`; the promotable table regenerates via
  `python3 bridge/width_contract.py --widths1 <widths-1>/coverage.json
  --delta <widths-2>/coverage-delta.json --candidates
  ../worker-layout-minimum/candidates/layout-candidates.profile.json --out
  dryrun/promotable-set.json`.

Tool-call budget: ~55 of 120 used at REPORT time.
