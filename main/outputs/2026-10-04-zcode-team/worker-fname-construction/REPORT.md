# REPORT.md — worker-fname-construction (FName CONSTRUCTION gap, P2-separate)

2026-10-04-zcode-team. CLAIM.json first, this REPORT last. Deliverables:
`CENSUS.md`, `IN-GAME-EXPERIMENT.md`, `ADAPTER-INTERFACE.hpp` (+ this
report), all under
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-fname-construction/`.
Nothing outside that directory was created or modified; no network, no
console, no nested subagents; **nothing was executed against the game image —
the experiment document is design only**. ~29 tool calls used of the 45 bound.

## 1. Findings

- The construction gap is real and narrow. The frozen static survey has
  `fname_ctor_original status=3` (ABSENT) while `fname_ctor_call_candidate
  status=0 linked=0x23d590` (`run-release-1/stdout.txt` lines 3, 5; census
  A-1/A-3). That single shared encoded call target — derived by the
  conservative 5-anchor classifier in
  `ue4ss-native-tick-relocation-resolver-1/crate/src/fname_call.rs`, whose
  module doc states it is "static evidence, never a callable/validated
  pointer" — is the ONLY construction lead anywhere in the workspace.
- The classifier's own contract doubles as independent ABI evidence: every
  accepted call-site encodes `RDI = stack output ptr` (LEA RDI,[RSP/RBP]),
  `RSI = anchor UTF-16 text`, `EDX = 1` within 64 branch-free bytes
  (fname_call.rs:24-49). This corroborates the checkpoint
  `fname_native_constructor_abi` correction — explicit-output lowering
  `void fn(ptr output, ptr text, i32 mode)` (census C-8, B-2, B-4) — from a
  second lineage; without it a naive by-value call would corrupt the stack.
- FName width remains the hardest sub-fact: 0 compiled-accessor entries
  (census D-1), so no slot sizing may ever be defaulted. FNamePool/FNameEntry
  layout is entirely unresolved and the P2 rule forbids pool heuristics
  (institutionalized as `kFNamePoolPredicateExcluded` in worker-5's header;
  NamePrivate is raw-bytes-only in worker-layout-batch2).
- `calls_by_anchor` (the per-anchor call-sites that would let FC-1 re-prove
  the target in-process) exists in the resolver struct but was not serialized
  by the survey stdout — materializing it is a bounded offline host re-run,
  flagged as a controller decision (census note under A-3).
- The retained port gives a complete "what happens after an address exists"
  path: `FName::ConstructorInternal` (NameTypes.hpp:228) with sret-consuming
  `construct_with_string`, `VerifyFNameConstructor`'s hook-based
  verification with cooperative deadline (UnrealInitializer.cpp:878-937), and
  the `FName("ByteProperty", FNAME_Find, address)` verification call under
  SEH (Signatures.cpp:222-270) — which is exactly the harmless-probe model
  FC-3 adopts (find-mode, guaranteed-existing name, non-mutating).

## 2. What gates rung-3 on this gap

- `FindFirstOf` / `StaticFindObject` (modreqs A3/A4): query keys are engine
  FNames — blocked until construction works AND lookups have live objects
  (worker-1) plus validated layouts (worker-2).
- `StaticConstructObject(class, outer, FName)` (A5, survey-validated
  `0x490aa0`): its FName parameter is unsuppliable without this seam.
- UFunction param marshalling (B4/B5/B6): every FName-typed parameter needs
  BOTH the construction seam (value) and a validated FName width (slot size,
  D-1) — two separately-tracked facts, deliberately not conflated.
- NOT gated by construction: FName->string conversion (B8,
  `Conv_NameToString` plan of record) — that is the P2 separation this worker
  enforced throughout (census keeps the families in distinct sections).

## 3. Recommended experiment order (IN-GAME-EXPERIMENT.md)

1. **FC-1** (read-only classifier): `probe_code_shape` at
   `image_base+0x23d590` plus five call-site rel32 re-validation when
   `calls_by_anchor` is materialized; code-shape alone caps at Inconclusive;
   any mismatch Refutes and stops the ladder.
2. **FC-2** (read-only): GUObjectArray-anchored layout corroboration
   (`0x93ba110`, sibling G6 predicate, <=16 items, structural invariants
   only, names as raw bytes); optionally records the validated ctor's
   RIP-relative data references as candidate-only pool anchors — never
   dereferenced as a pool (P2).
3. **FC-3** (single guarded call, its own launch, FC-1-Validated gating,
   flushed pre-records so a crash is honest absence evidence): explicit-output
   call with static UTF-16 "ByteProperty"/"Engine", mode = FNAME_Find only
   (Add refused forever — pool mutation); pass = sret writes confirmed,
   same-name double-call identical, different names differ; zero-fill,
   divergence or missing post-records => Refuted with ladder closure and no
   replay.
   FC-1+FC-2 can share one launch; every stage rolls back by removing the
   ploader entry + PRX and archiving the JSONL with its sha256.

## 4. Deviations / open items

- The checkpoint bodies `zcode_ue4ss_integration_rules.fname_rule` and
  `zcode_worker_2_layout_facts` were honored as relayed in the dispatch and
  cross-verified against their on-disk artifacts (worker-2 REPORT; survey
  stdout; widths facts) rather than located as standalone files — every
  relayed claim matched the artifacts.
- `std::array` is referenced in ADAPTER-INTERFACE.hpp's FNameBytes without
  including <array>; the header is PROPOSED-ONLY and uncompiled by design;
  integration must fix includes when it becomes real code.
- CLAIM.json status flipped to complete in the same edit window as this
  report; no other file changed after its write.
