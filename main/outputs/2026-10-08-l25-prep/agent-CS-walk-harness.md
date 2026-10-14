# Agent CS — l26 walk harness: FindFirstOf/FindAllOf all-zero reproduction + fix-validator

Built and run 2026-10-08 (late). Work dir `/tmp/l26-walk-harness/` (self-contained;
source trees READ-ONLY — only read, sliced, sha-pinned). All scenarios PASS, exit 0.
Results: `/tmp/l26-walk-harness/results/run.stdout.txt` (per-scenario JSON);
provenance: `results/manifest.json` (15 sources, 37 verbatim regions, per-region sha256).

RUN COMMAND (extract verbatim regions → compile → run the 14-scenario matrix):

```
python3 /tmp/l26-walk-harness/extract_and_build.py     # exit 0 = all checks pass
/tmp/l26-walk-harness/bin/harness --scenario S0_GOOD   # any single scenario, JSON on stdout
```

Console symptom reproduced (Lua nils = empty results for EVERY class): the walk
executes, names resolve, `[FFO] add-fallback` stays silent — and every probe
returns zero — under FIVE configurations, ranked below by console consistency.

---

## 1. REPRODUCTION STATUS — ranked misreads

The mock is console-parity: 1 chunk × 65536 FUObjectItem slots (stride 0x18:
Object+0x0 / Flags+0x8 / SerialNumber+0xC), NumElements=35550 (tonight's live
Index), GUObjectArray→ObjObjects+0x10→Objects+0x0/NumElements+0x14/NumChunks+0x1C,
UObjectBase 0x28 (ObjectFlags 0x8 / InternalIndex 0xC / ClassPrivate 0x10 /
NamePrivate 0x18 / OuterPrivate 0x20), UStruct::SuperStruct 0x40, UClass 0x200 —
all per the l26 profile overlay (AA-corrected). The FName pool follows the
rc_seed layout (id = block<<16|slot, stride-2 entries, "None"=0,
"ByteProperty"=3 — CD's console-parity anchor, asserted). Fixture: real class
graph (Object←Field←Struct←Class metaclass; Actor←Controller←PlayerController;
EngineSubsystem←ItemDatabaseSubsystem←BP child; BP_PlayerController_C;
components/widgets), 4 CDOs, package, 2 UFunctions, UEnum, UScriptStruct, 6
probed live instances, ~35.5K numbered-instance fillers, 3 Unreachable slots.
Every read goes through the SAME offset tables the console build uses
(MemberOffsets pins + the Flags.hpp hard-code + StaticClass storages) — the
scenario knob IS a wrong profile pin / wrong pointer, applied before any
static offset cache warms (one process per scenario = one console boot).

| # | scenario | knob | result | console-exact all-zero? | init-parity* |
|---|---|---|---|---|---|
| 1 | **SB3_SUPERCLASS** | SuperStruct read @0x10 (ClassPrivate slot) | `IsA<UClass>` rejects 34832/35550 | **YES** | ok |
| 2 | **SB1_ROOTPTR** | `UClass::StaticClass()` → root class object | `IsA<UClass>` rejects 34831 (718 flags-first) | **YES** | ok |
| 3 | **SC1_NUMBERSLOT** | class NamePrivate @0x1C (Number field) | Equals never matches | **YES** | **BROKEN** |
| 4 | **SC2_PTRBYTES** | class NamePrivate @0x14 (pointer bytes) | Equals never matches | **YES** | **BROKEN** |
| 5 | **SA4_PS4ARENA** | flags read @0x14 = PS5 object-arena ptr high dword (0x3A model) | flags arm rejects 35550/35550 | **YES** (conditional) | ok |
| — | SA3_PTRHIGH | flags read @0x14 = HOST ptr high dword | 35550/35550 rejected | all-zero on this host (0x1X heap); PS5's observed 0x128…-arena high dword is 0x1 → **would NOT reproduce there** | ok |
| — | SA2_NAMELOW | flags read @0x18 (NamePrivate-low = pool id) | 35538/35550 rejected | all-zero on THIS pool only because interned ids 16–31 all carry bit 0x10; fragile to interning order | ok |
| — | SA1_IDX / SA1T_IDX | flags read @0xC (InternalIndex) | 26654/35550 rejected (75.0%) | **NO** — 4/5 positive classes zero at natural placement but SceneComponent-class fillers survive; SA1T proves re-placing the probed instances cannot fix it — never every-class-zero on a full array | ok |
| — | SA2B_NAMEHIGH | flags read @0x1C (Name Number) | ~no rejection | NO | ok |
| — | SB2_STRUCTPTR (control) | `UClass::StaticClass()` → UStruct class | positive probes still found | NO (wrong-but-not-root pointer doesn't reproduce) | ok |
| — | SIF1_SERIAL / SIF2_OBJPTR (controls) | item-level Flags @0xC / @0x0 | positive probes found | NO (the walk-level IsUnreachable read is not the failure surface) | ok |

*init-parity = does the init-time full-name scan (`StaticFindObject_InternalSlow`,
verbatim, GetFullName string compare) still find `/Script/Engine.PlayerController`
and `/Script/Engine.Class` under the misread — tonight's console PROVED this
path works at boot (type-scan missing-keys=0, StaticClass resolution live).

### Ranking rationale (which reproducer fits ALL console facts)

Console facts to satisfy: (i) zero objects for EVERY class in-game; (ii) names
resolve — no `[FFO]` markers (CP's Find-first fix works, the pool admits the
names); (iii) the traversal runs — user-feels-lag; (iv) the SAME walk succeeded
at init (type-scan; agent CO §3); (v) no crash/throw.

1. **SB3 (misread (b), profile-pin form) — BEST FIT.** A SuperStruct pin that
   lands on the ClassPrivate slot makes every `IsChildOf` walk's first hop the
   metaclass pointer: EVERYTHING is-a UClass → `IsValidObjectForFindXOf` excludes
   all 35550 → zero for every class, mechanically perfect walk (lag), names
   untouched, and — decisively — the init path never walks SuperStruct, so
   boot-time successes are unaffected. Both FindXOf arms (direct Equals and
   TSuperStructRange) are killed by the same wrong pointer.
2. **SB1 (misread (b), literal wrong-pointer form) — same signature.** Requires
   the UClass StaticClass storage to hold the wrong pointer at runtime while the
   init resolution (exact full-name scan, proven working) filled it — i.e. post-
   init corruption or a substitution at the filter site (note: the port itself
   replaces IsA(StaticClass()) with cast-flag checks elsewhere —
   UnrealType.hpp:2989 — so a cast-flag-based variant of this filter would have
   exactly this failure shape if the flag table were wrong).
3. **SC1/SC2 (misread (c)) — exact zero but CONTRADICTED by console evidence.**
   A wrong NamePrivate slot breaks `GetNamePrivate().ToString()` → the init
   full-name scan would have failed at boot. Harness shows init-parity BROKEN
   under both variants. If tonight's type-scan truly passed, (c) is out —
   unless the offset error appeared between init and gameplay (single static
   table — not plausible).
4. **SA4/SA3 (misread (a), deterministic form) — conditional.** All-zero ONLY
   when the garbage dword carries 0x10/0x20 (RF_ClassDefaultObject|RF_ArchetypeObject).
   A pointer half-dword read does it when the arena's address bits 4/5 are set
   (this host's 0x1X heap did it accidentally; the ONE PS5 address family in
   tonight's logs — 0x1280174d00 — has high dword 0x1 = bit 0 only = would NOT
   reject). Critically, on console this read is HARD-CODED `0x8` in
   Flags.hpp `get_flags` — a profile pin cannot misalign it; only a code-level
   offset error can. The task-named (a) variants measure as partial, never
   every-class-zero: InternalIndex read rejects exactly 75.0% (bits 4/5 of a
   linear index), NamePrivate-low rejected 99.97% here only by interning-order
   luck, Number-slot rejects nothing. **(a) is the weakest suspect family.**

LAG model: zero-result configurations cannot `LoopAction::Break` — every
FindXOf call walks all 35550 live items (measured visited=35547 with the 3
Unreachable slots skipped) — vs GOOD-config FindFirstOf breaking at the first
match. The user-felt hitching is itself evidence the console sits in a
zero-result configuration, not a no-walk configuration.

## 2. The chain, extracted verbatim (37 regions, sha-pinned in manifest.json)

Port root `.superpowers/.../source/port/` + CP's l26 candidate. Sources
(sha256 prefix): UObjectGlobals.cpp 49951ac8…, UObject.cpp 93825320…,
UObjectArray.cpp 171560a9…, UStruct.cpp 28ec32cb…, UObject.hpp e3fdd1b0…,
Class.hpp 0f5f4f9d…, NameTypes.hpp 5fa8d93e…, UnrealType.hpp ae18e8c0…,
UnrealFlags.hpp f25d3dcf…, Flags.hpp ecfed956…, Flags425.cpp e9b13fb1…,
UObject425.cpp daa27fcc…, MemberVariableLayout_SrcWrapper_UObjectBase.hpp
ff54fc12…, MemberVariableLayout_SrcWrapper_FUObjectItem.hpp 76fa38b6…,
CP candidate UObjectGlobals.cpp 4abf743b….

| region | source lines | role |
|---|---|---|
| walk_chunked | UObjectGlobals.cpp 882-911 | **the linear walk** (NumChunks × 65536, stride 0x18, IsUnreachable/!Object skip) |
| foreachuobject_dispatch | :980-1001 | 5.6 → Chunked |
| findfirstof_fname | :434-479 | **FindFirstOf predicate**: `Class->GetNamePrivate().Equals(ClassName)` + IsValidObjectForFindXOf + TSuperStructRange super arm |
| findallof_fname | :546-590 | FindAllOf twin (same arms, no Break) |
| is_valid_object_for_findxof | :220-224 | **the filter**: HasAnyFlags(RF_ClassDefaultObject\|RF_ArchetypeObject) + IsA\<UClass/UFunction/UPackage/UEnum/UScriptStruct\> |
| staticfindobject_slow | :137-159 | init-path parity discriminator (GetFullName string scan) |
| isA / ischildof | UObject.cpp 103-110 / Class.hpp 177-209 | IsA = ClassPrivate→IsChildOf pointer walk (GetSuperStruct per hop) |
| getsuperstruct | UStruct.cpp 198-210 | SuperStruct read via GetSuperStruct507 (offset knob = SB3) |
| has_any_flags / uobject_has_any_flag_425 / to_impl_flags_425 / flags_has_any_flag / flags_get_literal8 | UObject.hpp 267-270 / UObject425.cpp 23-26 / Flags425.cpp 5-41 / Flags.hpp 36-41, 10-15 | the flags read chain; literal-0x8 body compiled verbatim alongside the knob and FV0-asserted identical at 0x8 |
| member_objectflags / member_class_name_private | SrcWrapper_UObjectBase 4-21, 42-97 | **profile-pinned member reads** (ObjectFlags/ClassPrivate/NamePrivate/OuterPrivate) — the (c) injection surface |
| member_item_object / member_item_flags / fuobjectitem_* | SrcWrapper_FUObjectItem 4-22, 61-79; UObjectArray.cpp 83-86, 127-176 | item Object/Flags reads + IsUnreachable/GetUObject/GetFlagsInternal |
| fname_* (7 regions) | NameTypes.hpp | Equals (ComparisonIndex only!), ==/!= via ToUnstableInt, IsNone, EntryId ops |
| tsuperstruct | UnrealType.hpp 3270-3330 | super-struct range |
| getpathname_getfullname | UObject.cpp 530-593 | full-name builder (init-parity path) |
| eobjectflags / einternalobjectflags | UnrealFlags.hpp 59-96, 174-194 | RF_/internal flag values |
| cp_makefindxof | CP candidate 498-526 | tonight's Find-first name construction + `[FFO] add-fallback` marker |
| flagsandrefcount_helpers | UObjectArray.cpp 73-81 | dead-5.7-branch helpers (compile parity) |

Documented shims (mock environment only; every semantic read is verbatim or
knob-configured verbatim): FName pool/ToString, HashObject bodies come with the
GetPathName region, `Version` = 5.6, hash tables off (console D2), TUObjectArray
accessors at binary-verified pins, StaticClass storages return the fixture's
class objects (GOOD values = the graph's own), SA4's PS5 pointer high dword via
a shadow table (a host process cannot host dereferenceable 0x3Axxxxxxxx
pointers). Full list in template.cpp.in header comment.

## 3. Fix-validation assertions (what a fixed build must pass — S0_GOOD, green)

- **FV1** `FindFirstOf("PlayerController")` returns the first PlayerController-class
  INSTANCE (`PlayerController_0`, flags 0x41) — not the UClass object, not the CDO.
- **FV2** `FindFirstOf("ItemDatabaseSubsystem")` returns the subsystem instance.
- **FV3** `FindAllOf("PlayerController")` = exactly {native instance, BP-derived
  instance} — BP found through the TSuperStructRange super arm; zero CDOs/classes.
- **FV4** `FindAllOf("ItemDatabaseSubsystem")` = {native, BP-child instance};
  `Default__ItemDatabaseSubsystem` (CDO) excluded.
- **FV5** `FindAllOf("Class")` and `("Function")` EMPTY — the IsA exclusions must
  survive any fix (a fix that removes the filter would pass FV1-FV4 wrongly).
- **FV6** unknown name → null AND exactly ONE `[FFO] add-fallback` marker (CP's
  design; proves the marker path is alive and names-resolve is a real signal).
- **FV8** init-parity: the verbatim startup scan still finds
  `/Script/Engine.PlayerController` and `/Script/Engine.Class`.
- **FV9** zero `[FFO]` markers across all known-name probes (names resolve).
- **FV10** filter census on the fixture: 718 CDO/archetype + 20 class + 5
  function/package/enum/script-struct objects rejected.
- **FV0** knob flags-read @0x8 pointer-identical to the port-verbatim literal 0x8.
- **Regression guards for the suspect sites** (run the same command): SB1/SB3/SC1/SC2
  must each stay console-exact-zero (they encode the failure signatures the fix
  must NOT introduce); SIF1/SIF2/SB2 must stay non-zero (controls).

## 4. Console discriminators for the next diag (cheap, read-only, no new walks)

1. One line in the FindXOf failure path logging WHICH arm rejected the first
   PlayerController instance (flags-value vs is-a-class vs no-match) — cleanly
   separates (a)/(b)/(c) live.
2. Dump once at diag time: `UClass::StaticClassStorage` pointer, one class's
   `[class+0x40]` SuperStruct value and `[class+0x10]` ClassPrivate value, and
   `[instance+0x8]` ObjectFlags for one live instance. SB3 predicts
   SuperStruct-read == ClassPrivate-slot content (== metaclass pointer for every
   class object).
3. An IsA consumer outside FindXOf (e.g. any `IsA<AActor>`-gated diag) — SB1/SB3
   predict it also misbehaves; (c) predicts it is unaffected.

## 5. Caveats / honesty

- Host mocks prove control-flow/condition reachability with the REAL chain code
  and console-parity layouts — not console memory behavior. SA4's PS5 arena
  value (0x3A) is a MODEL: the console object-arena address family is not in
  tonight's logs; the observed array-arena high dword (0x1) would make (a3)
  NOT reproduce. SA2's all-zero is intern-order luck on this fixture (ids
  16–31 carry bit 0x10); it is reported as fragile, not as proof.
- The InternalIndex variant (a1) is the task's named example and measurably
  CANNOT produce every-class-zero on a full array (75% rejection, survivors in
  every filler class) — SA1T (premise placement) still leaves SceneComponent
  instances findable. The honest conclusion is that (a) in its named forms is
  NOT the console bug; only its pointer-half-dword form can be, and that read
  is hard-coded (not profile-pinnable) on console.
- SC1/SC2 reproduce the symptom exactly but are inconsistent with the
  init-time walk successes logged tonight — ranked below (b) on that evidence.
- The harness's GUObjectArray is the mock chunk array; FUObjectHashTables are
  off, matching the console D2 branch. NumElements is not a walk bound (CO §1);
  slots ≥ NumElements are engine-zeroed and skip via `!Object`.

— agent CS (ZCode), 2026-10-08. Only /tmp/l26-walk-harness/** and this report written.
