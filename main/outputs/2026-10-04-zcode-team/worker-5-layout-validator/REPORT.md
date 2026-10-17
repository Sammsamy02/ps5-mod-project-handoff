# worker-5-layout-validator — REPORT (layout-DATA gate)

Role: worker-layout-validator, 2026-10-04-zcode-team (ZCode). CLAIM.json was
written before any implementation; this REPORT is the last artifact.

## What was built

| Artifact | Content |
|---|---|
| `src/townfall_layout_probes.hpp` | Pure read-only predicate API + full provenance block (every constant cited) |
| `src/townfall_layout_probes.cpp` | Implementation: no OS calls, no allocation, no exceptions; explicit little-endian decode |
| `tests/test_layout_probes.cpp` | 23 host tests: Validated / Refuted / Inconclusive, each citing predicate + rationale code |
| Build + evidence | `c++ -std=c++17 -Wall -Wextra -Werror` → ALL TESTS PASSED (0 failures); re-run clean under `-fsanitize=address,undefined` |

Nothing outside `outputs/2026-10-04-zcode-team/worker-5-layout-validator/` was
created or modified. No network, no console, no nested subagents.

## Honest evidence status

These predicates are DESIGN + HOST-TEST evidence only. They prove the
predicate logic is correct on synthetic little-endian mapped images. Target
validation (running them against the live game) is later controller work and
is NOT claimed here. Nothing here makes any file-image vaddr a native fact,
and no callable-ABI claim exists anywhere in this code.

## Input contract (how they run later in-process)

Inputs (both produced by others, never by this code):
1. worker-1 live-module facts: `image_base` (rtld-reported base of the anchor
   image) + readable mappings. The controller converts each
   `lmf::VmMapping`/`ModuleInfoFacts.segments` entry into a
   `tlp::MemSpan {start, size, data}` where `data` points directly at the
   mapped memory (in-process this borrows the live mapping; the predicates
   never dereference beyond `start+size`).
2. worker-2 profile vaddrs (arguments, never embedded):
   GUObjectArray `0x93ba110`, UGameEngine::Tick `0x2fac8c0`,
   StaticConstructObject `0x490aa0`.

Outputs: `Verdict {Validated, Inconclusive, Refuted}` + stable
machine-readable `RCode`s (+ per-layout sub-verdicts and observed field
values for GUObjectArray) feeding the layout bundle. Call sequence:

```cpp
tlp::TownfallProfileVaddrs vp{0x93ba110ull, 0x2fac8c0ull, 0x490aa0ull};
auto gua = tlp::probe_guobject_array(spans, n, image_base, vp.guobject_array);
auto tick = tlp::probe_code_shape(spans, n, image_base, vp.ugameengine_tick, nullptr);
auto sco  = tlp::probe_code_shape(spans, n, image_base, vp.static_construct_object, nullptr);
```

## GUObjectArray probe design (0x93ba110 + image_base)

The port's 5.x knowledge gives TWO retained layout hypotheses (profile
unresolved_list: member offsets "never supplied/resolved" for the build), so
both are evaluated and the combined verdict claims Validated only when
exactly one survives:

- Layout A — label is the FUObjectArray outer: ObjFirstGCIndex +0x0,
  ObjLastNonGCIndex +0x4, ObjObjects (embedded TUObjectArray) +0x10
  (cited: `5_00/5_06_MemberVariableLayout_DefaultSetter_FUObjectArray.cpp`
  under `.superpowers/native-dependency-spike/native-coherent-runtime-1/
  source/port/unreal/generated_include/FunctionBodies/`).
- Layout B — label is a TUObjectArray directly: Objects +0x0,
  PreAllocatedObjects +0x8, MaxElements +0x10, NumElements +0x14,
  MaxChunks +0x18, NumChunks +0x1C
  (cited: `5_00/5_06_..._TUObjectArray.cpp`; identical across all retained
  5_00..5_08 setters).
- Structure constants: NumElementsPerChunk = 64*1024 and chunk-pointer bound
  [512] from `.../include/Unreal/UObjectArray.hpp:88` + generated
  `Chunks_Type`; FUObjectItem size 0x18 (UEP_TotalSize, 5_00..5_06;
  5.6 only adds RefCount at +0x14 inside the same size).

Predicate chain (per hypothesis, first hard result wins):
P1 pointer fields canonical (48-bit canonical, 8-aligned, null allowed) and
inside readable ranges; both-null refuted →
`GOA_R_PTR_NONCANONICAL / GOA_R_PTR_OUTSIDE_READABLE / GOA_R_PTRS_BOTH_NULL`.
P2 count sanity: 0 < Num <= Max; 1 <= MaxChunks <= 512; NumChunks <=
MaxChunks; Max <= MaxChunks*64Ki → `GOA_R_COUNTS_INSANE`.
P3 GC order (Layout A only): 0 <= ObjFirstGCIndex <= ObjLastNonGCIndex <=
Max → `GOA_R_GC_ORDER`.
P4 capacity cross-check: NumChunks >= ceil(Num / 64Ki) →
`GOA_R_CAPACITY_MISMATCH`.
P5 chunk array: every needed chunk pointer readable, non-null, distinct
(`GOA_R_CHUNK_NULL_WHILE_NEEDED / GOA_R_CHUNK_UNREADABLE /
GOA_R_CHUNK_DUPLICATE / GOA_R_CHUNK_OVERLAP`); the chunk's needed extent
(remaining elements * 0x18) must be covered or the result is honestly
`GOA_E_CHUNK_EXTENT_UNVERIFIABLE` (Inconclusive, never Refuted — the task's
"unreadable chunk" refute applies when the chunk pointer itself points at no
readable byte).

Combination: exactly one Validated → Validated (layout recorded);
both Validated → Inconclusive `GOA_E_LAYOUT_AMBIGUOUS`; both Refuted →
Refuted (union of codes); otherwise Inconclusive (one hypothesis refuted
does not refute the label — the other may be the live one).

Cross-hypothesis discrimination is real, not assumed: a true Layout-A image
makes Layout-B read the GC pair as an Objects pointer that is non-canonical
or outside every readable range (T1 vs T3/T4 evidence), and vice versa
(T2). Objects[0]/Objects[1] are covered by P5 via the first scanned chunk
slots (observed.chunk0_ptr/chunk1_ptr).

## Code-shape probes (Tick 0x2fac8c0, StaticConstructObject 0x490aa0)

INCONCLUSIVE-CLASSIFIER ONLY, by design. They read up to 16 bytes at the
vaddr and greedily decode a conservative x86-64 frame-setup grammar
(endbr64, push rbp/rbx/rsi/rdi, push r12–r15, mov rbp,rsp, sub rsp imm8/imm32,
mov [rsp+d8],r), plus an optional supporting `CODE_I_PRE_PADDING` signal
(0xCC/0x90 filler immediately before the entry). Results:
`CODE_I_PROLOGUE_LIKE`, `CODE_I_NOT_PROLOGUE_LIKE` (negative body shapes:
alignment nops, int3, ret, jmp), or `CODE_E_UNREADABLE`. The verdict is
ALWAYS Inconclusive: a prologue-like shape never validates a callable
function, and a non-prologue shape is recorded as negative evidence for the
controller, never a refutation. No disassembler is bundled; the retained
iced_x86 CFG decoder
(`ue4ss-native-tick-relocation-resolver-1/crate/src/cfg_disassemble.rs`)
remains the deeper technique and is deliberately not reimplemented here.
The static-survey vaddrs for these two labels come from
`run-release-1/stdout.txt` lines 9 and 8.

## FName pool: NO predicate (explicit)

`kFNamePoolPredicateExcluded = true` with `kFNamePoolExclusionReason` records
the P2 rule as data: FName storage width and FNamePool/FNameEntry layout are
entirely unresolved for this build (worker-2 profile unresolved_list), the
FName ctor original and FNameToString resolvers found no match, and the
Kismet Conv_NameToString fallback is the preserved plan of record. No
name-pool heuristic exists in this code; the construction gap is its own
fact. Checked in as test T19.

## Per-family elevation table (candidate -> validated)

| Family (profile vaddr) | Evidence THIS worker adds | What would still be needed to call the target fact VALIDATED |
|---|---|---|
| GUObjectArray 0x93ba110 | Predicate chain P1–P5 over mapped bytes; Validated iff exactly one 5.x layout hypothesis survives; per-layout verdict + observed scalars/pointers | worker-1 live facts at probe time (image identity, segment correspondence, retained lifetime) + a `Validated` run in-process at a quiescent moment; a second probe pass with consistent Num/NumChunks (temporal stability) would strengthen it; Refuted bytes would demote the profile label itself |
| UGameEngine::Tick 0x2fac8c0 | Prologue-shape classification only (never Validated) | Mapped-owner proof + a genuine instruction decode (retained iced_x86 CFG path) + hook-delivery evidence under separate authorization; callable-ABI claims stay out of scope for this gate |
| StaticConstructObject 0x490aa0 | Same Inconclusive-classifier discipline | Same as Tick; additionally a call-path/xref proof from the engine's object-construction site |
| FName pool (no vaddr) | Exclusion recorded as data (P2) | Its own construction-gap fact must be resolved first (Kismet fallback path); never via heuristics here |
| GMalloc / FProperty / UFunction / FField | Out of this gate's scope (no predicates written) | Separate evidence tasks; profile marks them candidate-only/not-surveyed |

## Integration notes

- Serialized output: verdict enum values + `rcode_string()` names + the
  fixed-size POD results (GuaProbeResult/CodeProbeResult) are directly
  JSON-able by the controller; rationale codes are stable uint16 values.
- Ordering: run only after worker-1 capture succeeded and the anchor image
  is retained; probes are instantaneous and allocation-free so they can run
  on the game thread gate or from a payload thread that holds the lease.
- Bounds discipline: predicates consume ONLY the three profile-listed
  vaddrs as arguments; every offset is cited (header provenance block);
  `kMaxChunkScan`=64 needed-chunk pairwise bound is operational, and when
  NumChunks exceeds it the verdict is honestly downgraded to Inconclusive
  (`GOA_E_CHUNK_EXTENT_UNVERIFIABLE`), never silently passed.
- Known limits, stated plainly: readable-span granularity can produce
  Inconclusive where vmmap splits a chunk (correct, honest behavior); a
  deliberately crafted dual-layout image yields ambiguity (T14) rather than
  a false Validated; `pointer_canonical_or_null` accepts the canonical
  kernel form, which the readable-range check then rejects — kernel-range
  rejection is a bounds fact, not a canonicality fact.
