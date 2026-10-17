# worker-layout-minimum REPORT

Date: 2026-10-04. Output area (the only place this worker wrote):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-layout-minimum/`

## 1. What was produced

| File | Purpose |
|---|---|
| `CLAIM.json` | ownership, inputs, bounds (written first) |
| `MINIMUM-SET.md` | per-ladder-rung exact layout values + callables, dependency edges, smallest rung-1->2 subset |
| `candidates/layout-candidates.profile.json` | all candidates in the worker-2 entry grammar; every entry status=candidate-only with cited origin (port 5_06 default setters / survey / widths); vtable candidates deliberately empty |
| `validation/min_predicates.py` | host mirror of worker-5's predicate contract (same verdicts/rcode names/hypotheses) + synthetic-image builder |
| `validation/assemble_staged_pair.py` | staged-pair assembler with hard gate; emits ONLY predicate-passed, width-backed, identity-complete entries in the exact ParsePair grammar |
| `validation/test_assembler.py` | 18 host tests, all pass (`python3 -m unittest discover -s validation -p 'test_*.py'` -> OK) |
| `validation/example-emission/` | SYNTHETIC fixture emission (grammar demo; identity values literally named SYNTHETIC-FIXTURE) |
| `validation/RUNBOOK.md` | exact in-game reads at validated-vaddr+base + verdict criteria per rung |
| `ACCEPTED.md` | EMPTY-by-design acceptance ledger, schema ready |

## 2. Key findings

1. **The sibling is `worker-5-layout-validator`** (task named it
   worker-layout-validator). Its predicate contract exists
   (`src/townfall_layout_probes.hpp/.cpp`, tests, compiled test binary) —
   consumed, not duplicated. Convergence point recorded in min_predicates.py
   and RUNBOOK step 2: identical verdict vocabulary (Validated/Refuted/
   Inconclusive), identical rationale codes (GOA_*, CODE_*), identical
   hypotheses (A: label->FUObjectArray outer; B: label->TUObjectArray
   direct), identical constants (NumElementsPerChunk 64*1024 @
   UObjectArray.hpp:88, chunk bound 512, item size 0x18). DISAGREEMENT RULE:
   if compiled probe and host mirror disagree, nothing is accepted.
2. **Candidate base extracted with exact provenance** (5_06 default setters;
   TUObjectArray setter verified byte-identical to 5_00 by diff): TUObjectArray
   7 entries; FUObjectArray (A) incl. ObjObjects=0x10 and UEP_TotalSize=0xB8;
   FUObjectItem stride 0x18; UObjectBase (vptr-front) ClassPrivate=0x10 etc.;
   plus rung-3/5 classes FField/FProperty/UStruct/UClass/UFunction.
3. **Width gating is a real second gate**: the binding refuses members without
   a compiled accessor width. Verified against coverage.json:
   FUObjectArray::ObjAvailableList, FField::Owner/Next/NamePrivate,
   FProperty::PropertyFlags and UFunction::FunctionFlags have template offsets
   but NO compiled width -> they can never be staged as-is and are excluded
   (recorded in the candidates file + manifest skip list). UFunction.Func
   carries the conservative width 16 (alternatives [16,8]).
4. **Smallest rung-1->2 subset** (MINIMUM-SET.md): identity-only pair (legal:
   binding skips empty member classes) once the live scan supplies
   EngineRevision/BuildId/axes; then the winning hypothesis's
   FUObjectArray/TUObjectArray + FUObjectItem + UObjectBase entries — exactly
   what the assembler's default scope emits after a single-hypothesis
   Validated chain.
5. **Rung 3's object query needs no game function address**: port
   `StaticFindObject*`/ObjectSearcher are in-process walkers over
   GUObjectArray (UObjectGlobals.hpp:219-235). The FName CONSTRUCTION gap
   (call-site candidate 0x23d590, storage width entirely unknown) is tracked
   separately and never bundled with name-to-string (Kismet
   Conv_NameToString stays the plan of record).
6. **Honest limits**: FField/FProperty/UStruct/UClass/UFunction have NO
   predicates anywhere yet; the assembler REFUSES them (test-proven) until
   rung-3/5 predicates are commissioned. The controller's publication-wiring
   GAP (LAYOUT-GATE blocker 2) is unchanged by this work.

## 3. Verification performed

- 18/18 host tests pass: synthetic images prove the assembler refuses
  unvalidated entries end-to-end (no verdicts, Refuted counts, GC-order
  violation, unreadable header -> Inconclusive, chunk pointer outside spans,
  null objects, ambiguous layout) and that grammatical gates hold (identity
  completeness incl. per-field, printable-ASCII identity, null-width skip,
  offset+width within UEP_TotalSize, duplicate verdicts, CLI writes nothing
  on refusal).
- Emitted pair grammar checked line-by-line against
  ue4ss-native-layout-parser-1 (identity key spellings taken from its test
  fixture) and the binding invariants in NativeLayoutBinding.cpp.
- `git status` confirms only the team outputs tree is new; no existing file
  was modified; no network; no console; credentials untouched.

## 4. Tool-call budget

31 of 50 calls used at REPORT time.
