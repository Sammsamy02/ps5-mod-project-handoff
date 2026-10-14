# MINIMUM-SET.md — minimum real layout values + callables per acceptance-ladder rung

Team 2026-10-04 / worker-layout-minimum. Every numeric offset below is a
CANDIDATE copied from a cited port template (Windows-derived upstream default)
or the frozen static survey; it is NOT validated for the Townfall build until
the in-process predicate chain (worker-5 `tlp` predicates + the
`validation/min_predicates.py` gate mirror here) passes on target and the value
is written into ACCEPTED.md with evidence. Validated-static vaddrs from
worker-2 are marked [validated-static, file-image label only].

Candidate source shorthands (full paths in candidates/layout-candidates.profile.json):
- SET-5_06 = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/FunctionBodies/5_06_MemberVariableLayout_DefaultSetter_<Class>.cpp`
  (TUObjectArray setter is byte-identical to 5_00; verified by diff this session)
- UOA-HPP  = `.../port/unreal/include/Unreal/UObjectArray.hpp`
- SURVEY   = `.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/run-release-1/stdout.txt`
- WIDTHS   = `.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/coverage.json` (compiled accessor widths; TUObjectArray 7/7, FUObjectArray 12/13, UObjectBase 5/5, FUObjectItem 10/10 available)

## Rung 1 — genuine game-thread callback

Minimum data (NO member offsets strictly required for the callback witness):
1. UGameEngine::Tick file-image label `0x2fac8c0` [validated-static, SURVEY line 9,
   worker-2 fact GameEngineTick.gameengine_tick]. Still only a label: needs
   image_base (worker-1 live snapshot) + `probe_code_shape` classification
   (worker-5, Inconclusive-classifier-only) + the actual delivered callback as
   the rung-1 proof.
2. Callable (port, in-process): the tick-relocation hook path — not a game
   function call; the ladder proof is a real dispatched callback on the game
   thread, not a shape match.
3. Identity publisher (controller GAP, INTEGRATION-MAP SEAM 2): wiring
   `PublishStagedNativeLayoutMaps(major, minor, non_shipping, stats)` to the
   scan result. A staged pair with a COMPLETE identity and ZERO member entries
   is legal (binding `Prepare` skips empty member classes), so the smallest
   rung-1 bundle needs only:
   - `Format=1`, `Target=prospero-x86_64-sysv`, `PointerSize=8`,
     `EngineMajor=5`, `EngineMinor=6` [validated-static, SURVEY line 1]
   - `EngineRevision`, `BuildId` — UNRESOLVED facts, must come from the live
     scan (UnrealInitializer), never invented;
   - `NonShipping`, `Stats` axes — UNRESOLVED until scan pass 2
     (`SetResolvedNativeLayoutAxes`, UnrealInitializer.cpp:832).
4. Also candidate-corroborating (not required for the callback): GUObjectArray
   label `0x93ba110` [validated-static, corroborated by two endpoints] with
   cfg-alloc `0x483b40` / cfg-free `0x4840b0` [candidate-only].

## Rung 2 — valid object query

Requires rung 1 identity + publication, then the GUObjectArray chain. TWO
retained layout hypotheses (worker-5 probes both; the assembler emits only the
winner):

- Hypothesis A — label `0x93ba110` -> FUObjectArray outer [SET-5_06 FUObjectArray]:
  ObjFirstGCIndex=0x0, ObjLastNonGCIndex=0x4, MaxObjectsNotConsideredByGC=0x8,
  OpenForDisregardForGC=0xC, ObjObjects=0x10 (embedded TUObjectArray at +0x10),
  UEP_TotalSize=0xB8.
- Hypothesis B — label -> TUObjectArray directly [SET-5_06 TUObjectArray; 5_00 identical]:
  Objects=0x0, PreAllocatedObjects=0x8, MaxElements=0x10, NumElements=0x14,
  MaxChunks=0x18, NumChunks=0x1C, UEP_TotalSize=0x20.
- FUObjectItem stride [SET-5_06 FUObjectItem]: Object=0x0, Flags=0x8,
  ClusterRootIndex=0xC, SerialNumber=0x10, UEP_TotalSize=0x18 (stride).
- Structure constants [UOA-HPP:88 + generated member wrapper `Chunks_Type = UObjectBase**[512]`]:
  NumElementsPerChunk = 64*1024; chunk-pointer bound MaxChunks <= 512.
- Cross-check minimum (query sanity): UObjectBase [SET-5_06 UObjectBase]
  ClassPrivate=0x10, InternalIndex_Private=0xC (a returned item's Object must
  have canonical non-null ClassPrivate and an index that maps back to the same
  FUObjectItem).

Callable (port, in-process — no game function address needed):
`FUObjectArray::SetupGUObjectArrayAddress(base+0x93ba110)`,
`IndexToObject(int32)`, `GetNumElements()`, `GetNumChunks()`,
`GetObjectItemSize()`, `GetObjectArraySize()`, `FUObjectItem::GetUObject()`,
`TUObjectArray::GetObjectPtr(int32)` [UOA-HPP lines 97-144].

## Rung 3 — harmless reflected access

Requires rung 2 plus read-only reflection layout:
- UObjectBase full [SET-5_06]: ObjectFlags=0x8, InternalIndex_Private=0xC,
  ClassPrivate=0x10, NamePrivate=0x18, OuterPrivate=0x20, UEP_TotalSize=0x28.
- FField [SET-5_06]: ClassPrivate=0x8, Owner=0x10, Next=0x18, NamePrivate=0x20,
  FlagsPrivate=0x28, UEP_TotalSize=0x30.
- FProperty [SET-5_06]: ArrayDim=0x30, ElementSize=0x34, PropertyFlags=0x38,
  RepIndex=0x40, Offset_Internal=0x44, PropertyLinkNext=0x48, NextRef=0x50,
  DestructorLinkNext=0x58, PostConstructLinkNext=0x60, RepNotifyFunc=0x68,
  UEP_TotalSize=0x70. (PropertyFlags HAS a template offset but NO compiled
  accessor width in WIDTHS -> cannot be staged; see gaps.)
- UStruct [SET-5_06]: SuperStruct=0x40, Children=0x48, ChildProperties=0x50,
  PropertiesSize=0x58, MinAlignment=0x5C, UEP_TotalSize=0xB0.
- UClass (inherits UStruct) [SET-5_06]: ClassDefaultObject=0x110, FuncMap=0x128,
  NativeFunctionLookupTable=0x1F0, UEP_TotalSize=0x200.

Callable (port, in-process): `StaticFindObject` /
`StaticFindObject_InternalNoToStringFromNames` / `StaticFindObject_InternalSlow`
[UObjectGlobals.hpp:219-235] — walks GUObjectArray (rung-2 dependency), does
NOT call into the game. ObjectSearcher pool/hash variants [Searcher/ObjectSearcher.hpp].

SEPARATELY-TRACKED GAP — FName CONSTRUCTION (never bundled with name-to-string):
- Needed to build query FName keys from strings. Survey facts: wchar ctor
  original ABSENT (status 3); call-site candidate `0x23d590` [candidate-only,
  SURVEY line 5]; FName storage width has ZERO compiled accessor entries
  (WIDTHS FName 0/0) and FNamePool/FNameEntry layout is entirely unresolved
  (worker-2 unresolved_list). Until an in-process predicate proves a ctor
  (or the controller authorizes an engine-owned construction path), rung 3
  query-by-name is blocked EXCEPT via the InternalSlow CharType path.
- Name-to-string: plan of record stays engine-owned Kismet
  Conv_NameToString (worker-2 plans_of_record); do not re-run the
  SkySphereMesh-anchor investigation.

## Rung 4 — unchanged Item Spawner activation

Requires rungs 1-3 PLUS the complete staged pair substituted into the
activation bundle (worker-activation-bundle build_bundle.py keys
`ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini`, currently
`blocked_on_layout_data: true`):
- Identity complete (revision/build id/axes) as in rung 1;
- ALL member classes whose accessors UE4SS init actually touches validated
  (minimum known set = rung 2+3 classes; `setup_unreal_properties` may touch
  more — controller closes per evidence using NativeLayoutSchema.inc as the
  vocabulary, never as a value source);
- VTableLayout.ini may legally remain identity-only for this ladder (the
  Prospero hook route is relocation-based, not vtable-index). No vtable
  candidates are part of the minimum set; port `5_00_VTableOffsets_*`
  templates exist if a later rung needs them.
- Publication wiring GAP must be closed first (LAYOUT-GATE blocker 2).

## Rung 5 — visible spawn through real input

Requires rung 4 plus spawn-path layout/callables:
- UFunction [SET-5_06]: NumParms=0xB4, ParmsSize=0xB6, ReturnValueOffset=0xB8,
  RPCId=0xBA, RPCResponseId=0xBC, FirstPropertyToInit=0xC0,
  EventGraphFunction=0xC8, EventGraphCallOffset=0xD0, Func=0xD8,
  UEP_TotalSize=0xE0. Func width is the conservative 16 (alternatives
  [16,8], WIDTHS-2 delta). FunctionFlags=0xB0 has a template offset but NO
  compiled width -> cannot be staged (gap; if flags must be read, use a
  validated alternative or get an accessor width added to the compiled
  contract first).
- UClass::FuncMap (rung 3) for UFunction lookup; AActor/UGameViewportClient
  templates exist in SET-5_06 if the input path needs them (not minimum).
- StaticConstructObject label `0x490aa0` [validated-static, SURVEY line 8;
  label only] — candidate for the native construction path; proving it
  callable is an in-process step, out of scope for layout data.
- GMalloc label candidate `0x9350950` [candidate-only, SURVEY line 7] — only
  if the spawn/detour path needs allocator facts; not minimum.
- Real input provider facts are worker-1 scope, not layout data.

## Dependency edges (arrows = "requires")

scan(identity revision/build_id/axes) -> staged identity-only pair (rung 1)
rung 1 -> publication -> GUObjectArray chain predicates (hypothesis A/B +
FUObjectItem + UObjectBase cross-check) -> rung 2 (IndexToObject query)
rung 2 -> reflection chain (UObjectBase full + FField + FProperty + UStruct +
UClass) -> rung 3 (StaticFindObject reflected read)
FName construction [SEPARATE GAP] feeds rung-3 name queries and rung-5 param names
rung 3 -> full pair into activation bundle + publish wiring -> rung 4
rung 4 -> UFunction/FuncMap spawn callables (+ optional StaticConstructObject /
GMalloc labels) -> rung 5 (visible spawn via real input)

## Smallest subset that unblocks rung 1 -> rung 2

1. Controller obtains EngineRevision + BuildId + NonShipping/Stats from the
   live scan (in-process facts; no invention).
2. Assemble + stage the identity-ONLY pair (zero member entries) and publish.
3. Run the GUObjectArray predicate chain at base+0x93ba110 (both hypotheses);
   on a single-hypothesis Validated verdict, assemble the pair adding ONLY:
   the winning hypothesis's FUObjectArray/TUObjectArray entries + FUObjectItem
   (Object, Flags, ClusterRootIndex, SerialNumber, UEP_TotalSize) +
   UObjectBase (ClassPrivate, InternalIndex_Private, ObjectFlags,
   NamePrivate, OuterPrivate, UEP_TotalSize).
   That is exactly 6(+11 A) or 7(B) + 6 + 6 entries — the rung-2 query
   subset. Everything else (FField/FProperty/UStruct/UClass/UFunction) waits
   for rung 3/5 predicates.
