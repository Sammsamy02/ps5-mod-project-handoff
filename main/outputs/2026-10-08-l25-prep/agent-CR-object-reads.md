# Agent CR — FindFirstOf per-object reads pre-audit vs the shipping binary (pre-v6)

Date: 2026-10-08. READ-ONLY; only this file written (scratch scanners under
/tmp/cr-audit/). Premise under audit: tonight names resolve (zero `[FFO]
add-fallback` markers ⇒ CP's FNAME_Find-first construction hits the pool for
every probe) yet FindFirstOf returns nil/invalid in-game for EVERY class
probed, no crash. If v6 FindAllOf counts are all zero, parent says the bug is
in the walk's per-object reads or its exclusion filter.

Port root (PORT/): `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
Walk audited: `UObjectGlobals.cpp` :220-224 (IsValidObjectForFindXOf), :454-476
(FindFirstOf fallback walk), :546-589 (FindAllOf(FName) — same reads+filter),
:882-911 (ForEachUObject_Chunked). Binary: `outputs/2026-10-04-zcode-team/
worker-game-version/console-decrypted-eboot.bin`, sha256 3b6cfad5… (AA's pin,
re-hashed today). All binary addresses below are IMAGE vaddr (runtime = +0x400000).
Prior audits built on: CO (walk formula), AA (offset-correction precedent),
CB (FName machinery), CP (Find-first fix).

---

## 1. Per-object read table (task 1 + 2)

Path for ONE visited object in FindFirstOf/FindAllOf(FName) (hash tables are
OFF on this build — "FUObjectHashTables unavailable" — so the :434 hash arm
never runs; the :454-476 / :565-589 bodies always run):

```
ForEachUObject_Chunked (:882-911):
  item = Objects[ChunkIndex] + ItemIndex*0x18                      [CO-verified]
  R1: obj   = item->Object            // FUObjectItem+0x0
  R2: skip if item->IsUnreachable()   // FUObjectItem+0x8, bit 28
Predicate (:455-476 / :568-588):
  R3: Class = obj->GetClassPrivate()  // UObjectBase+0x10
  R4: Class->GetNamePrivate().Equals(ClassName)  // UObjectBase+0x18 on Class,
                                                  // ComparisonIndex only
  R5: obj->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)  // UObjectBase+0x8, bits 0x30
  R6..R10: obj->IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct>()
       = obj->GetClassPrivate() (R3 again) ->IsChildOf(T::StaticClass())
       = pointer walk over UStruct+0x40 (SuperStruct) chain
  R11 (else-branch): super_struct->GetNamePrivate().Equals(ClassName)  // +0x18 per ancestor
```

| # | Read | Offset | Port source | Binary-verified |
|---|------|--------|-------------|-----------------|
| R1 | item->Object | +0x0 | generated `MemberVariableLayout_SrcWrapper_FUObjectItem.hpp` (profile `FUObjectItem.Object=0`), `UObjectArray.cpp:127-143 GetUObject` (5.6 → `GetObject()`) | **YES** 0x4840e1 `mov r9,[r12]` + `cmp r9,r15` (FreeUObjectIndex verifying item→object identity); 0x4ace65 `mov r13,[r10]` feeding the name compare |
| R2 | item->Flags (Unreachable 1<<28) | +0x8 | `UObjectArray.cpp:83-86 IsUnreachable` → `GetFlagsInternal` → `GetFlags()` at profile `FUObjectItem.Flags=8`; `UnrealFlags.hpp:184 Unreachable=1<<28` | **YES** 0x373fe0-0x373ff7 GC-purge `mov eax,[rbx+8]; test eax,0x10000000; lock cmpxchg [rbx+8]` (engine SETTING Unreachable); 0x3b208a-0x3b20ac chunked idx → `[item+8]` → `test eax,0x10000000`; 0x484118 `movzx eax,word[r12+0xa]; test eax,0x6e50` (high-word internal mask incl. 28) |
| — | FUObjectItem stride / chunk formula | 0x18, idx>>16 | `UObjectArray.hpp:88 NumElementsPerChunk=64*1024`, `UEP_TotalSize()=0x18` | **YES** 0x4840cd-0x4840dd `movzx ecx,bx; lea r12,[rcx+rcx*2]; shr eax,0x10; shl r12d,3; add r12,[rdx+rax*8]`; same shape 0x4ace55-0x4ace61, 0x373fbd-0x373fc1 |
| R3 | UObjectBase::ClassPrivate | +0x10 | generated `MemberVariableLayout_SrcWrapper_UObjectBase.hpp` (profile `UObjectBase.ClassPrivate=0x10`) | **YES** 0x4912e1 `mov rcx,[rbx+0x10]` then 0x4912e5 `cmp rax,[rcx+0x110]` (engine comparing obj vs `UClass::ClassDefaultObject` — also binary-confirms the profile's `UClass.ClassDefaultObject=0x110` pin); 0x3fbd05-0x3fbd0d pair-compare; 0x3ae57e `mov r14,[rax+0x10]` |
| R4 | UObjectBase::NamePrivate (class name FName) + `Equals` | +0x18 | generated getter (profile `NamePrivate=0x18`); `NameTypes.hpp` `Equals` = ComparisonIndex only | **YES** 0x4ace68 `cmp qword [r13+0x18],r12` (engine name lookup: item→object→name in one breath); 0x3fbd0f-0x3fbd13; 0x3ae5a0 `cmp qword [rdx+0x18],rsi`. Engine compares the FULL 8-byte FName; port compares only the index dword — a weakening, never a mismatch source |
| R5 | UObjectBase::ObjectFlags (CDO 0x10 \| Archetype 0x20) | +0x8 | **NOT via profile**: `UObject.hpp:267-270 HasAnyFlags` → `Container::UnrealObjectVC` (5.6 branch = `UObject425`, Container.cpp) → `Flags425::to_impl_flags` (bit-for-bit identity, Flags425.cpp) → `Flags.hpp:11-14 get_flags` **hard-coded `ptr_cast(p_this, 0x8)`** | **YES** 0x4912b8-0x4912bd `mov eax,[rbx+8]; test al,0x10` (RF_ClassDefaultObject) + same on the outer at 0x4912d0-0x4912d5; 0x48fc49-0x48fc4f `mov rax,[rbx]; mov eax,[rax+8]; test al,0x10`; 0x4876cf-0x4876d3 `mov eax,[r14+8]; test eax,0x10000000` (RF_HasExternalPackage — matches Flags425's 0x10000000, cross-checking the whole EObjectFlags identity map) |
| R6-R10 | IsA<T> → IsChildOf → SuperStruct walk | UStruct+0x40 | `UObject.cpp:103-110 IsA` → `Class.hpp:177-209 IsChildOfInternal` (`GetSuperStruct()` do/while pointer compare); `UStruct.SuperStruct=0x40` profile pin | **YES** 0x3ae5a0-0x3ae5ad `cmp qword[rdx+0x18],rsi; …; mov rdx,[rdx+0x40]; test rdx,rdx; jne` (engine name-search walking the super chain); 0x2e99c3-0x2e99ca `mov rsi,[rsi+0x40]` chain loop |
| R11 | Ancestor NamePrivate | +0x18 | same getter as R4 | **YES** (same sites as R6: the 0x3ae5a0 loop is literally "walk +0x40, compare name at +0x18") |
| — | UObjectBase::InternalIndex | +0xC | generated getter (profile `InternalIndex_Private=0xc`); NOT read by the walk (only by item-based helpers like `UObject::IsUnreachable`) | **YES** 0x4840bb `mov ebx,[rsi+0xc]` (FreeUObjectIndex head; rsi = the UObject) |
| — | UObjectBase::OuterPrivate | +0x20 | generated getter (profile `OuterPrivate=0x20`); read by GetFullName (the resolver used to seed the IsA storages), not by the walk predicate | **YES** 0x4876b0-0x4876f7 engine outer-walk: `mov rdi,[rdi+0x20]` loop with `[r14+8]` flags checks between hops |
| — | GUObjectArray anchors (ObjObjects+0x10, NumChunks+0x1C, NumElements+0x24) | — | profile + `[Seeds]` slot-0 bind 0x97BA110 | **YES** (AA §2 + re-seen today: 0x4840c4 `cmp [rdi+0x24],ebx`, 0x4840c9 `mov rdx,[r14+0x10]` with rdi=&GUObjectArray) |

Bottom line of the table: **every per-object read the walk performs is
confirmed from the shipping image's own code, at multiple independent sites,
with the exact bit values the port uses.** There is no wrong offset, wrong
stride, wrong bit, or wrong constant left in the filter+compare path.

One structural note for v6: `GUOBJECTARRAY_PROFILE_ITER_COUNT()`
(ObjectSearcherProfiler.hpp:402-416) counts visited objects per walk but is
gated on `ObjectSearcherProfiler::IsProfilingEnabled()` — enabling it gives
v6 a "visited" number for free (0 visited vs 35K visited instantly separates
walk-dead from filter-dead from name-dead).

## 2. How the port resolves `UClass` for IsA<UClass>() (task 4)

Not a static FName, not a seed. Three layers:

1. `IMPLEMENT_EXTERNAL_OBJECT_CLASS(UClass)` (UObjectMacros.hpp:149-167;
   instantiated UClass.cpp:9) gives `UClass::StaticClassStorage`, initialized
   to `STATIC_CLASS_INVALID` (-2).
2. At UE4SS init, `TypeChecker::store_all_object_types()` (TypeChecker.cpp:70-126,
   called from UnrealInitializer.cpp:1419) sets it via
   `StaticFindObject_InternalSlow(nullptr, nullptr, "/Script/CoreUObject.Class")`
   — same ForEachUObject walk, but a **GetFullName STRING compare**
   (UObjectGlobals.cpp:141-156), i.e., it consumes OuterPrivate+0x20 and
   NamePrivate+0x18, NOT FName::Equals. Same for UFunction/UPackage/UEnum/
   UScriptStruct (":Function/:Package/:Enum/:ScriptStruct"). Note
   `store_all_object_types()` ends with unconditional `return true`
   (TypeChecker.cpp:823) — a null resolution is NOT caught there.
3. On use, `StaticClass()` re-resolves lazily if storage is null/INVALID, and
   **throws** `"[UClass::StaticClass] StaticClassStorage is nullptr"` if the
   re-resolve misses (UObjectMacros.hpp:161-163).

Could THAT be the broken link post-AA? **No mechanism found.** The storages'
resolution consumes only UObjectBase{+0x18,+0x20} and the object-array
zone {+0x10,+0x1C} — AA's correction began at FUObjectArray+0x30 (listener
zone), which this path never reads. And the failure shape is wrong for the
observed symptom: a null/unresolvable storage does not silently exclude — it
THROWS out of the walk predicate, through ForEachUObject, into the Lua C
boundary (no try/catch on the LuaMod:1632-1665 binding or the EIGT drain
path) → crash/terminate. Tonight has clean nils and no crash, so every
storage IsA consults is a live, resolvable UClass*. The only way a storage
produces all-exclude silently is if it pointed at an ANCESTOR of every class
(e.g. UObject's class) — but it can only be assigned from an exact
full-path-string match, so a wrong non-null assignment is not constructible
from this code. (It remains the textbook "everything excluded" mechanism and
is cheaply falsified by v6 — see §4.)

## 3. Failure-mode analysis vs tonight's symptom (task 3)

Symptom to match: (a) names resolve via FNAME_Find (no [FFO] markers);
(b) FindFirstOf nil/invalid for EVERY probed class in-game; (c) no crash;
(d) the same binary+profile found objects in-game during the l25 evening
round (`FindFirstOf("ItemDatabaseSubsystem")` WORKS, CONSOLE-RESULTS §2.2 —
through the OLD default FNAME_Add path); (e) in that same l25 session, every
documented runtime FNAME_Find CONSUMER failed (UFunction-by-name nullptr,
CB §5 verdict 1) while init-time FNAME_Find names matched fine.

### "Everything excluded" family (filter kills all matches)

- **E1 — IsA storages poisoned to an ancestor class**: IsValidObjectForFindXOf
  false for every object ⇒ nil for every class; FindAllOf(FName) all-zero;
  names resolve; no crash. PERFECT symptom match. Mechanism: ~nil (§2) — the
  only writers assign from exact full-name string matches; null → throw →
  crash, which did not happen. **Symptom-rank 1, plausibility-rank last;
  v6's cache-path arm (below) kills or confirms it in one shot.**
- **E2 — ObjectFlags(+0x8) garbage with CDO/Archetype bits**: same signature
  as E1 plus cache-path FindAllOf also zero (the skip is inside the shared
  walk). Binary rules it out completely (+0x8 and bits 0x10/0x20 are the
  engine's own). Also note the port's read is hard-coded +0x8 (Flags.hpp:14,
  "TODO" comment) — it matches the binary; nothing profile-derived can shift it.
- **E3 — item Flags(+0x8) misread as Unreachable for all items**: walk visits
  zero objects ⇒ all-zero everywhere INCLUDING any GetFullName-based probe;
  names still resolve. Binary rules the offset/bit out; for it to fire, every
  live item would need bit 28 set (they would all be mid-purge — impossible
  in steady play).

### "Nothing matches" family (compare never true)

- **N1 — the walk's INPUT FName: runtime-constructed FNAME_Find ComparisonIndex
  ≠ the stored NamePrivate index for the same textual name.** Names "resolve"
  (shard-Find hit, non-None ⇒ no [FFO] line), yet `Equals` (index-only) misses
  for every class ⇒ FindFirstOf nil everywhere, FindAllOf all-zero, no crash,
  full-name STRING lookups still work. This is the only hypothesis consistent
  with ALL of (a)-(e), and it sits exactly on CB's flagged gap: "FNAME_Find of
  an asset/BP name at runtime: never proven on console" while Store(Add)-mode
  is proven working in-game from our thread (l25 §2.2 success) and init-era
  Find names are proven matching (ByteProperty id 0x3 == pool id). CB's binary
  decode shows Find-mode (shard-Find 0x23c340) and Add-mode (Store 0x23c0b0)
  are DIFFERENT engine call paths sharing an id space on paper (`id & 0x1fffffff`
  both) — the unproven part is whether the shard-Find hit as invoked through
  the bound wide-char ctor (0x23d590→0x23d600, `_NN`-less, ASCII-collapsed)
  returns the identical id the class names were interned under, in the
  grown/packed pool state that exists after asset load. **RANK 1 overall.**
- **N2 — NamePrivate slot wrong**: binary-verified +0x18 at three engine
  sites; init-time name reads used the same getter and matched. Ruled out.
- **N3 — SuperStruct(+0x40) wrong**: would break only DERIVED-class matching;
  exact-name probes would still hit ⇒ partial failure, not all-fail. Also
  binary-verified. Ruled out by shape.

### Adjacent (not per-object reads, listed for completeness)

- **A1 — walk visits nothing (GUObjectArray/NumChunks state)**: would zero
  EVERYTHING including full-name string scans; init succeeded and nothing
  rebinds these. Near-zero, and any v6 InternalSlow probe discriminates.
- **A2 — FindFirstOf returns a live object but Lua reports nil/invalid**
  (wrapper/validity layer, e.g. `IsValid` → InternalIndex+0xC → item flags):
  would show as FindAllOf counts NON-ZERO while FindFirstOf probes still
  read as invalid. Outside this audit's read table; v6 counts split it out.

### The l25→tonight delta (why ranking favors N1)

Between the in-game SUCCESS (l25 evening) and tonight's all-fail, the walk,
its offsets, and the exclusion filter did not change (CP touched ONLY name
construction: default-Add → Find-first + [FFO] marker; AA touched only the
FUObjectArray listener zone ≥0x30, which nothing on this path reads). The one
thing that DID change on the failing path is exactly the input FName's
construction mode: Store/Add → shard-Find. Tonight's evidence says Find-mode
hits (non-None, no markers) — and the l25 session already showed every other
runtime Find-mode consumer failing while Add-mode succeeded. The per-object
reads are the most strongly verified links in the chain (§1); the Find-mode
input is the least-verified link that changed. That is the ranking.

## 4. Predicted v6 signatures → cause (task 5)

Key v6 design fact from the code: the Lua-exposed `FindAllOf(name)` (LuaMod.cpp:1693
→ `FindAllOf(const std::string&)` → `FindAllOf(FName)`, UObjectGlobals.cpp:612-615/546)
uses the SAME walk + SAME IsValidObjectForFindXOf + SAME Equals as FindFirstOf.
The 3-arg `bUseCache=true` overload (:506-539, cache-on sessions) goes through
`FindObjectSearcher<AnyInstance,AnyInstance>` → with hash tables off and the
cache pool, `ObjectSearcherFastInternal`/`SlowInternal` (ObjectSearcher.cpp) —
which does the SAME R1-R4/R11 reads but **NO IsValidObjectForFindXOf, NO IsA,
NO Unreachable-skip difference** (it skips Unreachable too, :71/:110 — it shares
R1/R2; it drops R5-R10). So: 2-arg arm = reads+filter; cache arm = reads only.
That pair IS the discriminator between "everything excluded" and "nothing matches".

| v6 signature | Diagnosis | Cause |
|---|---|---|
| 2-arg FindAllOf all-zero AND cache-path FindAllOf all-zero AND a full-name string probe (`StaticFindObject_InternalSlow`-family, e.g. FindObject("/Script/Engine.PlayerController")) still resolves | "Nothing matches", walk alive, filter not even consulted | **N1 rank-1**: Find-constructed ComparisonIndex never equals stored indices (shard-Find vs Store id-space / pool-state). Per-object reads exonerated (§1); fix belongs in the FName adapter/ctor path, not the walk |
| 2-arg all-zero BUT cache-path NON-zero for the same classes | "Everything excluded" | **E1** (IsA storages pointing at an ancestor — the only reader unique to the 2-arg arm is R5-R10; binary has already cleared E2/E3, so the storages are all that's left). Next step: log the six storages' full names in-game |
| All arms zero INCLUDING full-name string probes / InternalSlow | Walk visits nothing | **A1** (GUObjectArray/NumChunks state). Enable `ObjectSearcherProfiler` and read `ObjectsIterated`; 0 ⇒ chunk/state problem, 35K ⇒ revisit |
| Mixed counts (some classes non-zero) | Reads fine, input fine, exclusion fine | Per-class semantics (BP subclass instances not yet spawned at probe time) — CO's #2 territory; not a walk bug |
| Counts NON-zero while FindFirstOf probes still nil/invalid | Walk + filter + names all fine | **A2**: Lua object wrapper / validity layer — outside this read table |
| Any error/exception line or crash during the probes | Throw family | Storage re-resolve miss (UObjectMacros.hpp:163) or missing-profile-member throw from a generated getter — would contradict tonight's clean nils; treat as a new, different bug |

## 5. Provenance

- Port sources read: `unreal/src/UObjectGlobals.cpp` (:137-159, :220-224,
  :434-476, :481-515, :546-615), `unreal/src/UObjectArray.cpp` (:83-260),
  `unreal/src/UObject.cpp` (:65-110), `unreal/src/TypeChecker.cpp` (:58-126,
  :823), `unreal/src/UClass.cpp` (:9-11), `unreal/src/VersionedContainer/
  {Container,UObject425,Flags425}.cpp`, `unreal/include/Unreal/{UObject,UObjectArray,
  UObjectMacros,NameTypes,UObjectGlobals}.hpp`, `unreal/include/Unreal/CoreUObject/
  UObject/{Class,UnrealType}.hpp`, `unreal/include/Unreal/VersionedContainer/Flags.hpp`
  (hard-coded +0x8), `unreal/include/Unreal/Searcher/ObjectSearcherProfiler.hpp`,
  generated `MemberVariableLayout_{HeaderWrapper,SrcWrapper}_{UObjectBase,FUObjectItem}.hpp`,
  `ue4ss/src/Mod/LuaMod.cpp` (:1632-1665, :1667-1704).
- Profile: `outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp`
  ([UObjectBase] 8/0xc/0x10/0x18/0x20/0x28; [FUObjectItem] 0/8/0x18; [UStruct]
  SuperStruct 0x40; [FUObjectArray] AA-corrected 0x58/0x68/0x90/0x94/0x98) and
  `units/RuntimeBuildProfile.hpp` (same values as LayoutPins with evidence strings).
- Binary: decrypted eboot re-hashed 3b6cfad5…; PH0 exec [0, 0x7479b1c)@file
  0x4000; capstone 5.0.7 pattern scans + window disassembly, scratch at
  /tmp/cr-audit/scan*.py. Sites cited per row in §1; no writes outside /tmp
  and this file.
- Prior audits: agent-CO-walk-audit.md (walk formula), agent-AA-offset-verify.md
  (FUObjectArray engine-code ground truth; FUObjectItem {0x0,0x8,0x10} + 0x18
  stride corroborated today at new sites), agent-CB-fname-machinery.md (§3-§4:
  ctor chain 0x23d590→0x23d600→0x242470, Add→Store 0x23c0b0, Find→shard-Find
  0x23c340, id space `block<<16|offset & 0x1fffffff`, "runtime Find of
  asset/BP names never proven"), agent-CP-findfirstof-fix.md ([FFO] marker
  semantics), CONSOLE-RESULTS-20261008.md §2.2 (l25 in-game Add-mode
  FindFirstOf SUCCESS + Find-mode UFunction failure).

— agent CR (ZCode), 2026-10-08, read-only; only this file written.
