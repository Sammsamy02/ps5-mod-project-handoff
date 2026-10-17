# RUNBOOK — in-game validation of the minimum layout set

For the controller, to execute INSIDE the game module after composition (and
only there). Every read is `validated_file_image_vaddr + live_image_base`.
Nothing below may run against the saved capture file instead of the live
process; nothing may be graded Validated without its exact evidence record.

## 0. Preconditions (per attempt)

1. Live module facts snapshot from worker-1 (`live_module_facts.hpp`):
   `image_base`, readable `VmMapping` spans, module identity, and the image
   sha check against the pinned capture
   `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
   (E-capture in worker-2's profile). A mismatch stops everything: new image
   => new candidate pass, old candidates are NOT grandfathered.
2. Freeze the span snapshot (`span_digest` covers start/size/bytes) so verdict
   records are reproducible against exactly those bytes.
3. Game thread quiescent enough for a read-only pass (predicate reads are
   non-mutating; racing writer threads can only yield Inconclusive/Refuted,
   never a false Validated, because every chain member is cross-checked).

## 1. Rung 1 — game-thread callback witness

Reads: 16 bytes at `image_base + 0x2fac8c0` (UGameEngine::Tick,
validated-static label).
Classification: worker-5 `tlp::probe_code_shape` (host mirror
`min_predicates.probe_code_shape`). EXPECTED verdict: `Inconclusive`
(CODE_I_PROLOGUE_LIKE +/- CODE_I_PRE_PADDING) — by design it can NEVER be
Validated; a CODE_E_UNREADABLE or CODE_I_NOT_PROLOGUE_LIKE is recorded as
negative evidence, not a refutation.
PASS CRITERIA (rung 1 itself): a real callback dispatched on the game thread
through the tick hook, delivered to our code, with the label mapping recorded.
The shape classifier only corroborates; it is not the rung-1 proof.
Also here: resolve EngineRevision/BuildId/NonShipping/Stats from the live scan
and hand them to the assembler as identity facts (never defaults).

## 2. Rung 2 — GUObjectArray chain (the gate)

Probe: worker-5 `tlp::probe_guobject_array(spans, image_base, 0x93ba110)`
(authoritative in-process). Host cross-check: dump the same spans and run
`python3 min_predicates.py spans.json` (CONVERGENCE RULE: if compiled and
mirror disagree, both results are preserved and NOTHING is accepted).
Verdict criteria, exactly the worker-5 vocabulary:
- `Validated` under exactly ONE hypothesis (A: label->FUObjectArray outer with
  embedded TUObjectArray at +0x10; B: label->TUObjectArray direct) — the
  candidate offsets for the winning side + FUObjectItem + UObjectBase become
  EMITTABLE (not yet accepted; see step 5).
- `Refuted` (any GOA_R_*) — the contradicting candidate hypothesis is dead for
  this snapshot; record which codes fired.
- `Inconclusive` (GOA_E_*) — no emission; extend spans (more readable regions
  from worker-1) and retry the read-only pass; never loosen a predicate.
Then: `tlp`/mirror `probe_uobject_item_chain` over the first 8 items:
EXPECT Object canonical non-null, ClassPrivate canonical non-null,
InternalIndex_Private == item position for every checked item
(UOB_R_INDEX_MISMATCH refutes the UObjectBase candidate offsets as written).

## 3. Rung 3 — reflection chain + the FName CONSTRUCTION gap

Commission predicates (none exist yet — worker-5 built only the object-array
chain; assembler refuses FField/FProperty/UStruct/UClass/UFunction until they
exist): walk UClass via UObjectBase::ClassPrivate, check FField::ClassPrivate
non-null, FProperty::Offset_Internal < UStruct::PropertiesSize, UClass::FuncMap
readable container. Same verdict discipline.
FName CONSTRUCTION stays a separately-tracked gap: the only candidates are the
wchar-ctor call-site label `0x23d590` (candidate-only) and engine-owned paths.
Proving construction requires constructing FName values under controlled input
in-process and observing stable entry ids — never bundled with name-to-string
(plan of record: engine-owned Kismet Conv_NameToString).
Query alternative while the gap is open: port
`StaticFindObject_InternalSlow` CharType path (UObjectGlobals.hpp:219) which
does not need FName construction.

## 4. Rung 4 — activation

With steps 1-2 (minimum) or 1-3 (full) verdicts recorded:
1. `python3 assemble_staged_pair.py --candidates ../candidates/layout-candidates.profile.json
   --verdicts <verdicts.json> --identity <scan-facts.json> --out-dir <dir>`
   REFUSAL is the expected outcome until every gate condition holds — treat a
   refusal as "not ready", never edit the candidates to force emission.
2. Controller wires `PublishStagedNativeLayoutMaps` (INTEGRATION-MAP SEAM 2),
   then worker-activation-bundle's builder substitutes the two emitted files
   into the bundle keys (replacing `blocked_on_layout_data: true`).
3. PASS CRITERIA: `setup_unreal()` proceeds past
   `load_unreal_offsets_from_file()` / `StageNativeLayoutTexts` (the old
   hard-stop, LAYOUT-GATE step 3) and the Item Spawner mod starts unchanged.

## 5. Acceptance (write to ../ACCEPTED.md, nothing else counts)

For each value actually used: append a ledger row with the exact verdict
record (codes + observed), span_digest, image identity sha, worker-5 vs mirror
agreement statement, hypothesis that won, and the rung served. Values whose
chain is only Refuted/Inconclusive stay candidates; unchanged re-rating
without new evidence is prohibited (no-resubmission rule).

## 6. Rung 5 — spawn (after rung 4)

Needs UFunction predicates (Func=0xD8 conservative-16 etc. per candidates
file; FunctionFlags has NO compiled width and cannot be staged — flags reads
must go through a validated alternative or a new compiled-width entry).
Visible-spawn proof needs the real input provider (worker-1 scope) delivering
actual input that the unchanged Item Spawner lua turns into a visible spawn.
GMalloc/StaticConstructObject labels (`0x9350950` / `0x490aa0`) are only
labels until separately proven callable in-process.
