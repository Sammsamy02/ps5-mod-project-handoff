# REPORT — worker-layout-batch2: reflection-family layout predicates (rung 3)

Team 2026-10-04-zcode-team. New files only, in this directory; sibling
worker-5 sources and all other worktree files untouched (sibling header is
`#include`d read-only for `tlp::MemSpan`/`Verdict`/helpers so pointer
semantics are byte-identical across both predicate sets).

## Deliverables

- `src/reflection_layout_predicates.hpp` — predicate contracts + full
  provenance block (every offset cited; nothing guessed).
- `src/reflection_layout_predicates.cpp` — pure implementations; no OS
  calls, no allocation, no console/network; explicit LE decoding.
- `tests/test_reflection_predicates.cpp` — host
  synthetic-image suite; `-Wall -Wextra -Werror` clean; 43/43 PASS, also
  clean under `-fsanitize=address,undefined` (binaries committed alongside,
  mirroring the sibling's artifacts).

Build:
```
c++ -std=c++17 -Wall -Wextra -Werror -I src tests/test_reflection_predicates.cpp \
    src/reflection_layout_predicates.cpp \
    ../worker-5-layout-validator/src/townfall_layout_probes.cpp \
    -o tests/test_reflection_predicates
```

## Inputs consumed (profile-listed/validated only)

- `tlp::MemSpan` live spans (worker-1 facts).
- `rfl::ValidatedGuaView{objects_ptr,num_elements,num_chunks}` — assembled
  EXACTLY from a **Validated** worker-5 `probe_guobject_array` result's
  `observed` fields; the A/B layout question is never re-derived here.
- Caller-supplied absolute addresses (walk items, struct/function/property
  anchors). No hardcoded vaddr; no guessed offsets anywhere.

## Per-family elevation table

Predicates elevate the worker-layout-minimum CANDIDATE entries below from
candidate-only to "Validated for the mapped snapshot" **only** after the
in-process chain passes on target (host tests prove predicate correctness,
not target facts). Origin column = cited port-tree evidence.

| Family / member set (offsets) | Predicate | Elevation semantics | Origin (all cited in hpp) |
|---|---|---|---|
| UObjectBase 0x8/0xC/0x10/0x18/0x20/0x28 | `probe_ustruct_chain` base layer (per object, ≤64 walk cap) | Validated when vtable-in-readable-spans + ObjectFlags⊆RF_AllFlags(0x0FFFFFFF) + InternalIndex bounds & FUObjectItem round-trip + ClassPrivate non-null readable + Outer null-or-readable all hold; Refute codes 0x51xx | SET-5_06 UObjectBase; RF mask UnrealFlags.hpp:95; FUObjectItem SET-5_06 (stride 0x18); UOA-HPP:88 chunk math |
| UStruct 0x40/0x48/0x50/0x58/0x5C (+0xB0 size) | `probe_ustruct_chain` struct layer | Validated when ≥1 plausible object has SuperStruct null-or-readable + acyclic (≤64-hop visited-list) and Children/ChildProperties readable-or-null. Struct-region contradictions on OTHER objects are recorded as negative evidence only (see noise rule below) | SET-5_06 UStruct (4_07 UStruct differs — SuperStruct 0x30, no ChildProperties — retained as adversarial alternative, not asserted) |
| UClass 0x110/0x118/0x120 (+raw u32 at 0x128 FuncMap, 0x1F0 NativeFunctionLookupTable) | `probe_ustruct_chain` UClass extension | Info code `STR_I_UCLASS_EXT_OK` + `first_uclass_like` when CDO/SparseClassData/SparseClassDataStruct are null-or-readable. Container internals deliberately NOT interpreted (worker-6 scope); raw u32 recorded only | SET-5_06 UClass |
| UFunction 0xB0/0xB4/0xB6/0xB8/0xC0/0xC8/0xD8 (+0xE0 size) | `probe_ufunction` — **Inconclusive-classifier ONLY** | Support/negative codes (never Validated: no callable claim; never Refuted): Func-ptr readability at 0xD8 (width-agnostic: low 8 bytes under both [16,8] alternatives), FunctionFlags u32 mask (undefined-bits-only ⇒ implausible; width structurally bounded to 4 by compiled-width NumParms@0xB4), NumParms/ParmsSize/RVO coherence, FirstPropertyToInit/EventGraphFunction sanity | SET-5_06 UFunction; EFuncFlags UnrealFlags.hpp:381 + enum holes 0x10/0x20; WIDTHS (no FunctionFlags width) |
| FField 0x8/0x10/0x18/0x20/0x28/0x30 and FProperty 0x30/0x34/0x40/0x44/0x48/0x50/0x58/0x60/0x68/0x70 | `probe_ffield_variant` / `probe_ustruct_child_variant` | Dual-hypothesis discriminator (UE5 FField vs UE4 UProperty). **Validated iff exactly one hypothesis survives structurally** (H5: ClassPrivate@0x8 non-null canonical readable, Owner/Next null-or-readable, ArrayDim≥1, UE5 link ptrs 0x48..0x60 null-or-readable; H4: vtable + RF mask + InternalIndex bounds AND GUObjectArray round-trip + Class@0x10 + Outer@0x20 + UE4 link ptrs 0x50..0x68). Ambiguous ⇒ Inconclusive `FLD_E_AMBIGUOUS` with machine-readable `settling_evidence` naming the exact missing facts | SET-5_06 FField/FProperty vs SET-4_07 FProperty/UStruct (absence of any 4_xx FField setter is itself discriminator evidence); WIDTHS FName 0/0 |

Noise rule (documented in code): without FName/type identification (P2 gap)
a non-struct UObject's bytes in the UStruct region are foreign layout — they
produce recorded negative codes but can never alone refute the struct
family. Only UObjectBase/FUObjectItem-layer contradictions (rung-2-validated
structure) refute.

## Verdict discipline carried over

Validated = full chain passed for the mapped snapshot (existence proof for
the shape; per-snapshot, not per-build). Refuted = cited layout forbids the
bytes. Inconclusive = evidence unavailable or ambiguous — never success or
failure. The UFunction probe and the H4/H5 containment support codes are
classifier-class only.

## What remains width-gated (per worker-layout-minimum)

Unchanged by this work — predicates never manufacture widths:

- FName storage width (WIDTHS FName 0/0): NamePrivate (UObjectBase+0x18,
  FField+0x20) and RepNotifyFunc (FProperty+0x68) are copied RAW and never
  interpreted (P2; FName construction stays its own excluded gap).
- FProperty::PropertyFlags@0x38 — template offset only, no compiled width:
  raw-record only; cannot be staged.
- UFunction::FunctionFlags@0xB0 — template offset only: read as u32 for the
  classifier because compiled-width NumParms@0xB4 bounds it structurally;
  still not stageable.
- UFunction::Func width alternatives [16,8]: pointer extraction is
  width-agnostic; staging still needs the conservative-16 policy from
  MINIMUM-SET rung 5.
- FField::Owner/Next/NamePrivate and several FProperty members carry
  `width_bytes: null` in the candidates profile — offsets are exercised by
  the predicates, staging remains blocked until widths exist.
- Exact settling evidence for `FLD_E_AMBIGUOUS` (emitted verbatim by the
  predicate): (a) a compiled FName width to test +0x20 as UE5 FName vs UE4
  OuterPrivate round-trip; (b) a compiled PropertyFlags width + mask
  semantics; (c) a containment scan longer than the 64-object cap.

## Integration note (for the controller / next rung)

1. Run worker-5 `probe_guobject_array` first; feed the winner's
   `GuaObserved` into `rfl::ValidatedGuaView`. If rung 2 was ambiguous, all
   batch-2 predicates degrade to Inconclusive by construction.
2. `probe_ustruct_chain` (≤64 objects) → on Validated, the rung-3 member
   subset (UObjectBase full, UStruct, UClass pointer members) may be staged
   per MINIMUM-SET rung 3, with `first_ustruct_like` as the live anchor.
3. Feed that anchor (or its ChildProperties/Children head via
   `probe_ustruct_child_variant`) into `probe_ffield_variant`; a
   single-survivor Validated settles the UE5/UE4 child-list variant for the
   snapshot; AMBIGUOUS returns the settling-evidence list instead.
4. `probe_ufunction` codes are advisory for rung 5 only; they never gate.
5. No FName predicate exists here by rule; name-dependent query paths stay
   on the InternalSlow CharType route per MINIMUM-SET.

Call budget: 28/45 used. No network, no console, no nested subagents; no
existing files modified (verified: only CLAIM.json, REPORT.md, src/ and
tests/ under this worker directory are new).
