# Agent AL — FUObjectArray listener-offset consumer map (blast radius, fix-ready)

Date: 2026-10-08. READ-ONLY pre-mapping for Claude review 2 item 1 (possible
correction of `FUObjectArray::UObjectDeleteListeners=0x78` /
`UObjectCreateListeners=0x68`). Parallel agent AA determines TRUE offsets from the
engine binary; this report is the consumer map so the fix is instant. All paths
relative to `/WORKSPACE/.codex/worktrees/translation-core/what/` unless absolute.
No file outside this report was written.

HEADLINE ANSWERS:
- Consumer count: **4 direct offset-consuming functions** (2 create + 2 delete,
  in one compiled TU), **11 registration/removal call sites** across port + l25
  trees, **3 in-listener self-removals**, **4 offset-VALUE pin locations** (facts
  JSON, INI declarations, RuntimeBuildProfile pins, host preflight test) plus the
  digest chain that ties them together.
- Flow chain (one line): Track-B/image evidence -> l23 facts JSON `required_members`
  -> l25 overlay `profile/profiles/townfall-build-facts.json` -> generator
  `generate_overlay.py` emits INI in `units/CompiledNativeLayoutDeclarations.hpp`
  (`[FUObjectArray] UObjectCreateListeners=0x68 / UObjectDeleteListeners=0x78`) ->
  baked into the runtime via `-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER` ->
  `UE4SSProgram::load_unreal_offsets_from_file` (l25:971-981) ->
  `NativeLayoutBinding::StageNativeLayoutTexts`/`ParsePair`/`Prepare` ->
  schema rows map `[FUObjectArray]::UObject*Listeners` into
  `Unreal::FUObjectArray::MemberOffsets` -> published at engine-axes resolution ->
  generated `SrcWrapper_FUObjectArray` getters reinterpret_cast `this+offset` ->
  `UObjectArray::Add/RemoveUObject{Create,Delete}Listener` -> the 11 call sites.
- THREADSAFE_UOBJECTS=1 vs =0: **the two listener offsets DO NOT MOVE.** The only
  member guarded by `#if THREADSAFE_UOBJECTS` is `UObjectDeleteListenersCritical`,
  which is declared AFTER both listener arrays (UObjectArray.h:1432-1435). Flipping
  the define moves only `PrimarySerialNumber` (0xB0->0x88), `bShouldRecycleObjectIndices`
  (0xB4->0x8C) and `UEP_TotalSize` (0xB8->0x90). The lock that DOES set the listeners'
  position is `ObjObjectsCritical` (unconditional, UObjectArray.h:1421): listeners sit
  at `0x40+S` / `0x50+S` where `S = sizeof(FTransactionallySafeCriticalSection)`.

---

## 1. How the offsets flow (each hop cited)

Build-time chain (what a correction edits is bolded):

1. **Measurement origin** — every listener fact row carries source
   `"retained authenticated image/static evidence"` (l25
   `profile/profiles/townfall-build-facts.json:1098-1109`; decimal 104 / 120).
   The l24/l25 generators only promoted FMap/FSet facts; the FUObjectArray values
   are inherited frozen from the l23 lineage
   (`outputs/2026-10-08-l24-runtime/profile/generate_overlay.py`: copies l23 facts,
   asserts `prior_facts_retained==93`).
2. **Profile JSON** — `outputs/2026-10-08-l25-runtime/profile/profiles/townfall-build-facts.json`
   `required_members` rows: `FUObjectArray::UObjectCreateListeners` offset 104
   (:1098-1103), `UObjectDeleteListeners` offset 120 (:1104-1109).
3. **Generated declarations INI** —
   `outputs/2026-10-08-l25-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp:4`
   single string, `[FUObjectArray]` section: `UObjectCreateListeners=0x68`,
   `UObjectDeleteListeners=0x78` (also `ObjObjects=0x10`, `PrimarySerialNumber=0xb0`,
   `bShouldRecycleObjectIndices=0xb4`, `UEP_TotalSize=0xb8`). Emitted by
   `outputs/2026-10-08-l25-runtime/profile/generate_overlay.py` (strings[2]=
   'compiled-audited-townfall-l25', strings[3]=digest), which also writes
   `units/RuntimeBuildProfile.hpp` (`kRequiredMembers` pins incl. 104/120 at :12;
   `kDigest` at :5) and asserts against the frozen l23 originals.
4. **Compile-time bake-in** — the `program` unit (UE4SSProgram.cpp) is compiled with
   `-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER=".../outputs/2026-10-07-l19-local-profile-r4/units/CompiledNativeLayoutDeclarations.hpp"`
   (l24-build-r2 `pass1/COMPOSITION-RECEIPT.json:7486`; the l23 units copy is the
   same lineage content). `NativeLayoutDeclarations.hpp:19-25` static_asserts the
   declarations' `profile_digest == rc::runtimeprofile::kDigest` (digest chain:
   facts JSON -> generate_profile digest -> declarations strings[3] ==
   RuntimeBuildProfile kDigest; host-checked by `profile/tests/digest_gate.cpp`).
5. **Runtime staging** — l25 candidate `typescan-r4/candidate/src/UE4SSProgram.cpp:971-981`:
   `GetCompiledNativeLayoutDeclarations()` -> `StageNativeLayoutTexts(compiled->members,
   compiled->vtables)`. (Port-root shape at `port/ue4ss/src/UE4SSProgram.cpp:894-909`;
   non-Prospero path instead reads `MemberVariableLayout.ini` + MacroSetter — not
   compiled for PS5.) Consumed binding TU:
   `outputs/2026-10-07-l19-local-profile-r4/units/NativeLayoutBinding.cpp`
   (`StageNativeLayoutTexts` :144-146; `GetStagedNativeLayoutFacts` :173;
   `PublishStagedNativeLayoutMaps` :188).
6. **Schema routing** — `NativeLayoutSchema.inc` rows
   `{"FUObjectArray","UObjectCreateListeners",...}` (:93) and `UObjectDeleteListeners`
   (:94) target `&Unreal::FUObjectArray::MemberOffsets` (MemberTargets :666)
   (canonical copy: `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc`;
   same content staged in the l19-r4 units lineage). `Prepare` validates and
   `PublishStagedNativeLayoutMaps` swaps the staged map into
   `Unreal::FUObjectArray::MemberOffsets` (port copy
   `port/ue4ss/src/NativeLayoutBinding.cpp:60-118,175-199`; publication armed from
   `typescan-r4/candidate/src/UnrealInitializer.cpp:1693-1714` after engine-axes
   resolution).
7. **Wrapper + generated accessors** — port `unreal/include/Unreal/UObjectArray.hpp:107-141`
   (FUObjectArray class, includes `MemberVariableLayout_HeaderWrapper_FUObjectArray.hpp`
   declaring `GetUObjectCreateListeners/GetUObjectDeleteListeners` at :27-32);
   bodies in `port/unreal/generated_include/MemberVariableLayout_SrcWrapper_FUObjectArray.hpp:118-152`
   — each getter caches `static const int32_t offset` from `MemberOffsets.find(...)`
   on first call (throws std::runtime_error if the key is absent) and returns
   `*ptr_cast<TArray<...>*>(this, offset)`.
8. **Call sites** — section 2.

### Exact edit point (Z9/O consumption law)

A VALUE-ONLY correction edits the **l25 profile overlay part**, nothing else:
- `outputs/2026-10-08-l25-runtime/profile/profiles/townfall-build-facts.json` (the two
  `required_members` offsets, :1098-1109),
- `outputs/2026-10-08-l25-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp:4`
  (INI hex values),
- `outputs/2026-10-08-l25-runtime/profile/units/RuntimeBuildProfile.hpp:12` (the two
  `kRequiredMembers` pins — consumed at runtime by
  `typescan-r4/candidate/src/UnrealInitializer.cpp:1370-1372` `require_layout`, which
  REFUSES startup on mismatch with the staged INI),
- `outputs/2026-10-08-l25-runtime/profile/tests/preflight.cpp:40-41` (host test pins
  104/120),
- then REGENERATE via `python3 outputs/2026-10-08-l25-runtime/profile/generate_overlay.py`
  (or extend it) so the digest chain (`strings[3]=digest`, `kDigest`, CHANGES.json
  `profile_digest`, per-row `candidate_sha256`) and `tests/run_tests.py` stay
  consistent — `digest_gate.cpp` static_asserts declarations-vs-kDigest.

Consumption mechanics (Z9 verified, `outputs/2026-10-08-l25-prep/agent-Z9-edit-path.md`):
`build_runtime.py:38` reads only parts `['profile','typescan-r4','thread']`
`CHANGES.json` `files` rows; each row's `original` must be the exact path the l24
composition consumed (one-hop; no chaining). The profile rows' originals are the
**l23** profile files (`outputs/2026-10-08-l23-runtime/profile/...`) — frozen, never
edit those. The declarations header reaches the compiler through the
`-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER` include (l19-r4 units path) and is aliased
by (basename, original digest) via the VFS header overlay. Never edit the port root,
l19/l22/l23/l24 originals — the l24 pin assertions (build_runtime.py:36) would fail
the build. JoinedLayoutPreflight.hpp has **no listener rows** (only
`FUObjectArray::ObjObjects` at :12) — no edit needed there for a listener correction;
its 66-row static_assert is unaffected. No port-root or typescan-r4 source change is
needed because the accessors read the offsets dynamically from the published map
(rebuild + redeploy required — the SrcWrapper caches the offset in a function-local
static, so there is no runtime-patch shortcut).

## 2. Every consumer of the listener offsets (file:line)

### A. Direct engine-memory consumers (the only code that dereferences GUObjectArray+offset)

Port root (pinned original; NOT compiled into l25 — replaced by the hardened copy):
1. `port/unreal/src/UObjectArray.cpp:364-372` — `AddUObjectCreateListener`
   (`GetUObjectCreateListeners()` :366, Contains + Append).
2. `port/unreal/src/UObjectArray.cpp:374-377` — `RemoveUObjectCreateListener`
   (RemoveSingleSwap :376).
3. `port/unreal/src/UObjectArray.cpp:379-387` — `AddUObjectDeleteListener`
   (`GetUObjectDeleteListeners()` :381).
4. `port/unreal/src/UObjectArray.cpp:389-392` — `RemoveUObjectDeleteListener` (:391).

l25 hardened callee (the compiled one; l24 unit `unreal-uobjectarray` consumed the
port root, this candidate replaces it via typescan-r4 row):
5. `typescan-r4/candidate/src/UObjectArray.cpp:564-572` — `AddUObjectCreateListener`
   (STILL UNHARDENED — no sane-header gate on the CREATE side).
6. `typescan-r4/candidate/src/UObjectArray.cpp:574-577` — `RemoveUObjectCreateListener`
   (unhardened).
7. `typescan-r4/candidate/src/UObjectArray.cpp:579-711` — hardened
   `AddUObjectDeleteListener`: header dump `GetData/Num/Max` :603-605, sane gate
   `l25_listener_header_sane` (def :425-433, `MaxPlausibleDeleteListeners=256` :415,
   bound `L25CanonicalUserMax=2^47-1` :477), duplicate refusal, GMalloc snapshot
   :485-516, growth refusal codes 1-6 (:419-423), Append :669, post-dump :670-676,
   try/catch around the layout-key throw :698-710.
8. `typescan-r4/candidate/src/UObjectArray.cpp:713-757` — hardened
   `RemoveUObjectDeleteListener` (pre-dump :731-733, sane gate :735-740,
   RemoveSingleSwap :744).

### B. Registration / removal call sites routed through A

Port root:
9. `port/unreal/src/UnrealInitializer.cpp:424` — `AddUObjectCreateListener(&FClassCreateListener::ClassCreateListener)` (startup, searcher pools).
10. `port/unreal/src/UnrealInitializer.cpp:425` — `AddUObjectDeleteListener(&FClassDeleteListener::ClassDeleteListener)`.
11. `port/unreal/src/UnrealInitializer.cpp:429` — `AddUObjectDeleteListener(&FShutdownDeleteListener::ShutdownDeleteListener)` (the "shutdown listener"; the observed PS5 init stall stopped exactly between the log line and this call's return).
12. `port/unreal/src/UnrealInitializer.cpp:124-125` — `RetireUObjectArrayCache` game-thread removals.
13. `port/ue4ss/src/Mod/LuaMod.cpp:6896` — `LuaMod::on_program_start`: `AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener)`.

l25 tree (compiled):
14. `typescan-r4/candidate/src/UnrealInitializer.cpp:545` — create listener add (inside `#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` only for the DELETE adds; create is unconditional).
15. `typescan-r4/candidate/src/UnrealInitializer.cpp:546-555` — class delete-listener add with `l25_dl_class_begin/done` markers.
16. `typescan-r4/candidate/src/UnrealInitializer.cpp:561-575` — shutdown delete-listener add with `l25_dl_shutdown_begin/done` markers.
17. `typescan-r4/candidate/src/UnrealInitializer.cpp:155-156` — RetireUObjectArrayCache removals.
18. `typescan-r4/candidate/src/LuaMod.cpp:8053` — FLuaObjectDeleteListener registration (LuaMod's FLuaObject listener path).

### C. Listener structs whose behavior depends on the offsets being right (engine -> us dispatch; they self-remove via A)

19. `port/unreal/src/ClassListener.cpp:76-83` — `FClassCreateListener` (+ `OnUObjectArrayShutdown` self-remove :80-83).
20. `port/unreal/src/ClassListener.cpp:85-125` — `FClassDeleteListener` (NotifyUObjectDeleted :85; self-remove :124).
21. `port/unreal/src/ClassListener.cpp:127-135` — `FShutdownDeleteListener` (self-remove :135). l25 hardened copy with throttled engine-dispatch proof markers (`l25_dl_first_notify_*`, `l25_dl_shutdown_cb_*`): `typescan-r4/candidate/src/ClassListener.cpp:90-93,169-243`.
22. `port/ue4ss/include/LuaType/LuaUObject.hpp:31-41` — `FLuaObjectDeleteListener` (OnUObjectArrayShutdown self-remove :39). NOTE: the copy the build actually compiles is the **l22-diagnostics** one: `outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp:32-40` (Z9 section 2; port-root LuaUObject.hpp is consumed by no unit).
23. `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp:92-96` — `FLuaObjectDeleteListener::NotifyUObjectDeleted` body (`remove_from_global_unreal_objects_map`) — dead if the write lands outside the engine's real array.

### D. Offset-VALUE consumers (pins / gates that compare the number itself)

24. `outputs/2026-10-08-l25-runtime/profile/profiles/townfall-build-facts.json:1098-1109` — facts (104/120). l24 mirror: `outputs/2026-10-08-l24-runtime/profile/profiles/townfall-build-facts.json:1098-1109`.
25. `outputs/2026-10-08-l25-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp:4` — INI string (0x68/0x78) — THE value the runtime actually loads.
26. `outputs/2026-10-08-l25-runtime/profile/units/RuntimeBuildProfile.hpp:12` — `kRequiredMembers` pins; enforced at runtime by `typescan-r4/candidate/src/UnrealInitializer.cpp:1370-1372` (`require_layout` refuses startup on mismatch vs staged INI) and cross-checked by the joined preflight `evaluate` at :1389 (JoinedLayoutPreflight has no listener rows — only `FUObjectArray::ObjObjects` :12 — so it neither blocks nor validates the listener values).
27. `outputs/2026-10-08-l25-runtime/profile/tests/preflight.cpp:40-41` — host test pins 104/120 (run via `tests/run_tests.py`).
28. Dormant on PS5: versioned default setters emplace the same 0x68/0x78 for 4.27 and 5.06 (`port/unreal/generated_include/FunctionBodies/4_27_MemberVariableLayout_DefaultSetter_FUObjectArray.cpp:26-38`, `5_06_...:31-38`) — only reachable through the non-Prospero MacroSetter path (`UE4SSProgram.cpp` `#else` branch), listed for completeness; do NOT edit (native binding requires the target map empty at publish, and the PS5 build never executes this path).

### E. GC/delete-notify iteration WE perform

NONE. Only the engine iterates the listener arrays (vendored UObjectArray.h:1460-1477
under `UE::TScopeLock ListenersLock`). Our side only ever Append/RemoveSingleSwap
single elements. `ForEachUObject` walks `ObjObjects` (offset 0x10), not listeners.

## 3. Breakage analysis if the offsets are wrong (ranked)

TArray wire layout on this ABI (UE4SS port, `TSizedDefaultAllocator<32>`): 16 bytes —
`{ ElementType* Data; int32 Num; int32 Max; }` (`port/unreal/include/Unreal/Core/Containers/Array.hpp:731+`).
The l25 callee reads exactly this triple (:603-605, :731-733) and Append/RemoveSingleSwap
write through it.

Scenario per the review item — true delete offset 0x80, we use 0x78 (8 low):
- Our Data word at +0x78 reads the 8 bytes preceding the true array. If the whole
  layout shifted +8 (e.g. `sizeof(FTransactionallySafeCriticalSection)`=0x30, create
  true 0x70), that word is the true create-array's `{Num,Max}` pair ->
  Data = `(uint64)CreateMax<<32 | CreateNum` (e.g. 0x4_00000003): a small, non-heap,
  usually misaligned "pointer". `Num()/Max()` then dereference it -> SIGSEGV, and
  `Contains()` would walk it. l25 screen: misalignment (`%8!=0`) or
  `data_address<=2^47-1` catch most combinations, but an even/even pair passes the
  arithmetic screen and faults on the read — the callee itself documents
  readability as unprovable (this TU cannot prove page readability). Port original:
  completely unguarded crash — consistent with the observed init stall right after
  "Adding shutdown listener".
- If create stays truly 0x68 and ONLY delete is +8 off (extra/missing member between
  the two arrays — means the vendored 5.6.1 member order does not match the binary):
  +0x78..+0x87 would read whatever truly lives between them; zeros give the
  canonical empty header (Data=null,Num=0,Max=0) which PASSES the sane gate, so
  Append takes the fresh-alloc path and WRITES `{new_ptr,1,cap}` at +0x78 —
  clobbering 16 bytes of live engine state (likely the true inter-member field or
  the head of the true delete array / critical section) — and our listener never
  enters the engine's real array.

Ranked consequences (worst first):
1. **Engine-state corruption via the wrong-address Append/RemoveSingleSwap write.**
   16-24 bytes of GUObjectArray adjacent state get a TArray header or listener
   pointer written over them. If the clobbered storage is `UObjectDeleteListenersCritical`
   (pthread mutex bytes) or the create-array header, the engine's next locked
   iterate/lock deadlocks or crashes — a plausible root cause of the observed
   init hang class, and it survives our own clean-refusal logging.
2. **Arbitrary-address write through a garbage-but-plausible Data pointer.** Growth
   path (`l25_data_heap_plausible` failure mode aside): `Data[Num] = Listener` writes
   our vtable-bearing object pointer to an arbitrary heap address. l25 screens bound
   this to canonical-user, aligned, non-wrapping headers with Max<=256; the port
   original is unbounded. Residual: page readability is unprovable, so even the
   screened path can fault.
3. **Crash inside our own callee** (SIGSEGV reading Num/Max/Data through a bogus
   pointer). Pre-l25: uncaught -> init abort/stall. l25: arithmetic screen + try/catch
   reduce but cannot eliminate (a fault is not a C++ exception).
4. **Silent feature loss with delayed UAF** (offset lands in harmless-but-wrong
   storage): engine never dispatches NotifyUObjectDeleted/OnUObjectArrayShutdown ->
   FLuaObject global-unreal-objects map keeps dangling UObject* (Lua-side UAF at next
   GC), class/actor searcher pools never invalidated (stale FUObjectItem*),
   shutdown self-removals never run. Variant B's skip-define already documents the
   same stale-pointer hazard.
5. **Startup refusal (fail-closed) if the correction is applied inconsistently:**
   facts JSON / INI / RuntimeBuildProfile pins are cross-checked at
   UnrealInitializer.cpp:1370-1372 — editing one without the others refuses startup
   with `[BuildProfile] ... REFUSE`. The digest static_assert (NativeLayoutDeclarations
   :22-24) likewise fails the build if declarations and kDigest diverge.

Wrong critical-section address: the port NEVER takes `UObjectDeleteListenersCritical`
or `ObjObjectsCritical` — no published offset, `LockGUObjectArray/UnlockGUObjectArray`
are commented-out stubs (port UObjectArray.cpp:442-451; l25 :811-818, block comment
:383-434). So a wrong listener offset cannot send us locking a wrong mutex; the
critical-section hazard is only the corruption direction (we write INTO mutex bytes,
rank 1).

## 4. Offset-adjacent fields and the threadsafe computation

Other FUObjectArray members the port consumes at runtime: `ObjObjects` (0x10) via
`GetObjObjects()` in `GetNumElements/GetNumChunks/IndexToObject` (port
UObjectArray.cpp:396-422; l25 :759-787; also UObjectGlobals ForEachUObject paths,
`TUObjectArray` chunked access uses `TUObjectArray::Objects/MaxElements/NumElements/
MaxChunks/NumChunks` = 0x0/0x10/0x14/0x18/0x1c) and `UEP_TotalSize` (0xB8) via
`GetObjectArraySize()` (port :410-413). `GetObjFirstGCIndex/GetObjLastNonGCIndex/
GetMaxObjectsNotConsideredByGC/GetOpenForDisregardForGC/GetObjAvailable/
GetObjAvailableList*/GetMasterSerialNumber/GetPrimarySerialNumber/
GetbShouldRecycleObjectIndices` are declared (HeaderWrapper_FUObjectArray.hpp:5-57)
but have NO callers in port/l25 source — their only live role is the profile pins
(A/D above) and the schema/binding width validation.

Same measurement batch: YES — all ten `[FUObjectArray]` facts carry the identical
`"retained authenticated image/static evidence"` source string; they move together
or not at all in the profile.

Source-level layout, vendored UE 5.6.1
`outputs/2026-10-08-l23-layout-generic/official-source/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h:1410-1442`
(x86-64 SysV, 8-byte alignment), with `THREADSAFE_UOBJECTS` hard-defined 1 at
`ObjectMacros.h:33`:

```
0x00 int32 ObjFirstGCIndex                 (4)
0x04 int32 ObjLastNonGCIndex               (4)
0x08 int32 MaxObjectsNotConsideredByGC     (4)
0x0C bool OpenForDisregardForGC            (1, pad 3)
0x10 TUObjectArray ObjObjects              (0x20: ptr,ptr,i32,i32,i32,i32)
0x30 FTransactionallySafeCriticalSection ObjObjectsCritical   (size S)  [UNCONDITIONAL]
0x30+S  TArray<int32> ObjAvailableList     (0x10)
0x40+S  TArray<...*> UObjectCreateListeners (0x10)
0x50+S  TArray<...*> UObjectDeleteListeners (0x10)
[THREADSAFE_UOBJECTS==1 only]
0x60+S  FTransactionallySafeCriticalSection UObjectDeleteListenersCritical (size S)
0x60+S(+S if TS=1) FThreadSafeCounter PrimarySerialNumber (4)
...+4  bool bShouldRecycleObjectIndices (1, pad 3)
total  0x68+S(+S if TS=1)
```

`S` = `sizeof(FTransactionallySafeCriticalSection)`: `UE_AUTORTFM=0` ->
`FCriticalSection` -> `pthread_mutex_t` by value (CriticalSection.h:53,
PThreadsRecursiveMutex.h:58); `UE_AUTORTFM=1` -> `TSharedPtr<FState>` = 0x10
(TransactionallySafeCriticalSection.h:9-119).

Consistency check of the CURRENT profile: with S=0x28 (40-byte pthread_mutex_t) and
THREADSAFE=1 the formula reproduces every pinned value exactly — Create=0x68,
Delete=0x78, DeleteCrit=0x88, Primary=0xB0, bShould=0xB4, Total=0xB8. The profile is
internally self-consistent with the vendored source; only binary verification (AA)
can confirm S and the member order.

- THREADSAFE_UOBJECTS 1 -> 0 (S=0x28): Create/Delete and everything before them
  UNCHANGED; `PrimarySerialNumber` 0xB0->0x88 (-0x28), `bShouldRecycleObjectIndices`
  0xB4->0x8C, `UEP_TotalSize` 0xB8->0x90.
- S=0x10 (AutoRTFM) with THREADSAFE=1: Create 0x50, Delete 0x60, DeleteCrit 0x70,
  Primary 0x80, bShould 0x84, Total 0x88 — i.e. if AA finds 0x50/0x60 the culprit is
  the mutex storage size, not the threadsafe define.
- A "lock insertion" BEFORE the listeners (bigger S) shifts BOTH create and delete
  by the same delta: Delete=0x80 with Create=0x70 implies S=0x30. **Delete=0x80 with
  Create=0x68 is impossible from any prefix-size change** — it would mean an extra
  or reordered member between the two arrays (vendored source != binary) and both
  consumers plus the inter-member assumption must be re-examined.

## 5. FIX-READY table

Binary-verified column to be filled from agent AA. "Consumers to touch" = value-pin
files (all four + regenerate digest); code edits only in the noted structural cases.

| field | current profile | source calc TS=1, S=0x28 | source calc TS=0, S=0x28 | source calc TS=1, S=0x10 (AutoRTFM) | binary-verified (AA) | consumers to touch |
|---|---|---|---|---|---|---|
| FUObjectArray::ObjFirstGCIndex | 0x0 | 0x0 | 0x0 | 0x0 | PENDING | none (no code caller; pins only if AA contradicts) |
| FUObjectArray::ObjLastNonGCIndex | 0x4 | 0x4 | 0x4 | 0x4 | PENDING | none |
| FUObjectArray::MaxObjectsNotConsideredByGC | 0x8 | 0x8 | 0x8 | 0x8 | PENDING | none |
| FUObjectArray::OpenForDisregardForGC | 0xC | 0xC | 0xC | 0xC | PENDING | none |
| FUObjectArray::ObjObjects | 0x10 | 0x10 | 0x10 | 0x10 | PENDING | if changed: pins + JoinedLayoutPreflight.hpp:12 (Required,16) + UObjectGlobals/TUObjectArray paths re-audit |
| FUObjectArray::UObjectCreateListeners | 0x68 | 0x68 | 0x68 (no move) | 0x50 | PENDING | pins: facts JSON :1098, INI :4, RBP :12, preflight.cpp :40; code: Add/RemoveCreate (l25 :564-577; create side UNGUARDED — consider porting the delete-side sane gate if AA changes anything) |
| FUObjectArray::UObjectDeleteListeners | 0x78 | 0x78 | 0x78 (no move) | 0x60 | PENDING | pins: facts JSON :1104, INI :4, RBP :12, preflight.cpp :41; code: hardened Add/Remove (l25 :579-757) re-audit `MaxPlausibleDeleteListeners` rationale; call sites :546-575, :155-156; LuaMod :8053; l22-diag LuaUObject.hpp:32-40 + LuaUObject.cpp:92-96 stay as-is (dynamic offset) |
| (engine) UObjectDeleteListenersCritical | not published | 0x88 | absent | 0x70 | PENDING | none — port never locks it (stubs :442-451/:811-818); do NOT publish without FTransactionallySafeCriticalSection layout proof |
| FUObjectArray::PrimarySerialNumber | 0xB0 | 0xB0 | 0x88 | 0x80 | PENDING | pins only (no code caller) |
| FUObjectArray::bShouldRecycleObjectIndices | 0xB4 | 0xB4 | 0x8C | 0x84 | PENDING | pins only |
| FUObjectArray::UEP_TotalSize | 0xB8 | 0xB8 | 0x90 | 0x88 | PENDING | pins + `GetObjectArraySize()` (l25 :765-768); NativeLayoutBinding::Prepare enforces member < UEP_TotalSize with 8-byte accessor width — a listener offset >= TotalSize would be REJECTED at staging (hard fail), keep TotalSize consistent |

Correction procedure (value-only case): edit the four l25 profile files ->
regenerate (`profile/generate_overlay.py` -> new digest across
CompiledNativeLayoutDeclarations/RBP/CHANGES.json + row hashes) -> `tests/run_tests.py`
(host preflight + digest_gate) -> rebuild via `build_runtime.py`/`build_variants.py`
(parts list already includes `profile`) -> twin-package via `l25_packaging_driver.py`
(remaps the profile lineage overlays + digest). Never edit l23/l24 originals or the
port root (pin asserts, build_runtime.py:36; Z9 one-hop rule). If AA's values imply a
structural divergence (Delete moves without Create), additionally re-audit the
vendored-source member-order assumption baked into the l25 callee's block comment
(UObjectArray.cpp:375-434) and the `MaxPlausibleDeleteListeners=256` bound rationale
(:410-416).

## Appendix: verification receipts used

- l24-build-r2 pass1 COMPOSITION-RECEIPT.json: units `unreal-uobjectarray` (port root),
  `unreal-unrealinitializer` (l24 typescan-r3 candidate), `lua-luauobject`
  (l22-diagnostics), `luamod` (l23 thread), `unreal-flat-classlistener` (port root),
  `native-layoutbinding` (l19-r4 units) — confirms which copies the build consumes.
- Z9 report (agent-Z9-edit-path.md) sections 1-3: parts list, one-hop replacement,
  LuaUObject.hpp l22-diagnostics root, header alias by (name,digest).
- Vendored official 5.6.1 source: UObjectArray.h:1410-1442 (members), :1272-1283
  (Lock/Unlock), :1460-1477 (engine iterate under UObjectDeleteListenersCritical);
  ObjectMacros.h:33 (THREADSAFE_UOBJECTS=1); TransactionallySafeCriticalSection.h:9-122;
  CriticalSection.h:53; PThreadsRecursiveMutex.h:58.
- l25 CHANGES.json (typescan-r4) rows confirm originals: port root for
  UObjectArray.cpp/.hpp + ClassListener.cpp, typescan-r3 for UnrealInitializer.cpp,
  l23-thread for LuaMod.cpp/UE4SSProgram.cpp; no LuaUObject.hpp row (l22-diagnostics
  copy is already the compiled root via inherited VFS overlay).
