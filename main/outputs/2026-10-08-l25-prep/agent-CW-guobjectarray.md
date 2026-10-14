# Agent CW — GUObjectArray two-path asymmetry audit (init type-scan vs runtime FindXOf walk)

Date: 2026-10-08. READ-ONLY; only this file written. Port root (PORT/)
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`.
The actually-compiled editions were verified where they differ from PORT:
`outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp` (the edition
every l25 build compiled — CP §3) and
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (the l26 candidate).
Built on: CO (walk audit), CP (Find-first fix), CR (per-object read audit,
N1 ranking), CB (FName machinery), AA (offset verification).

---

## VERDICT (one paragraph)

**The iteration code is the SAME function over the SAME GUObjectArray in both
contexts — and that is exactly why the asymmetry is NOT in the iteration.**
Init type-scan and runtime FindXOf both end in
`ForEachUObject` → `ForEachUObject_Chunked` (PORT unreal/src/UObjectGlobals.cpp:980-1000,
:882-911) over the one port global `FUObjectArray* GUObjectArray` (PORT
unreal/src/UObjectArray.cpp:453), seed-bound once to the engine's own static
object at runtime 0x97BA110 and never rebound. The hash-table arm of
`FindFirstOf(FName)` (PORT UObjectGlobals.cpp:438) is **dead on PS5** —
`ShouldUseHashTables()` is permanently false because the PS5 build compiles
`FUObjectHashTables::IsAvailable()` as a stub returning false
(`ue4ss-native-hash-tables-1/source/UObjectHashTablesNativeUnavailable.cpp:7-14`
+ `NativeHashTablesUnavailablePolicy.hpp:9-11`; confirmed present in the link
as `unreal-uobjecthashtablesnativeunavailable.o` in the l24-build-r2 runtime.map).
What differs between the two paths is **what the walk is asked to compare**:
the init scan never converts the query to an FName — it string-compares
`GetFullName()` output; the runtime walk converts the Lua string to an FName
through the bound engine ctor and compares **ComparisonIndex only**
(NameTypes.hpp:345-347). **Prime suspect: the runtime-constructed query FName's
ComparisonIndex (FNAME_Find/shard-Find mode through the bound wide-char ctor)
does not equal the ComparisonIndex the engine stored in each class's
NamePrivate — so `Equals` misses for every object while the walk itself is
mechanically perfect.** The decisive cross-session evidence: the l25 evening
build (same walk, same offsets, same filter, same seed — only difference was
default FNAME_Add query construction) found
`FindFirstOf("ItemDatabaseSubsystem")` IN-GAME (CONSOLE-RESULTS-20261008.md §2.2);
the l26 build (only change: CP's Find-first construction) returns nil for every
class with zero `[FFO] add-fallback` markers. Fix shape: leave the walk, seeds
and GUObjectArray binding untouched; make the runtime query verified —
ToString round-trip the constructed FName against the query string and fall
back to the init-style string scan on mismatch (or fix the Find id derivation
in the FName adapter). Not a GUObjectArray bug.

---

## 1. The two paths side-by-side (task 1)

### 1a. INIT type-scan (works: classes found, scan-complete, core-ready)

```
UE4SSProgram startup
  └─ UnrealInitializer (seed path)
       ├─ [Seeds] slot 0 bind (see §3)
       └─ UnrealInitializer.cpp:1419  TypeChecker::store_all_object_types()
            └─ L26R4 TypeChecker.cpp:129-205  per core class:
                 UObjectGlobals::StaticFindObject_InternalSlow(nullptr,nullptr,
                     "/Script/CoreUObject.Class" etc)      [L26R4 :152-159]
            └─ L26R4 TypeChecker.cpp:795-836, :241-262  find_all_property_types(
                 "/Script/Engine.Actor" etc) → same InternalSlow [:261]
                 └─ PORT unreal/src/UObjectGlobals.cpp:137-159
                      StaticFindObject_InternalSlow:
                        ForEachUObject( [&]{                       [:141]
                          ObjFullName   = Object->GetFullName();    [:144]
                          iequal(ObjFullNameNoType, OrigInName)     [:147]
                        } )
                 └─ ForEachUObject → ForEachUObject_Chunked         [:980-1000/:882-911]
```

Also on the init chain: required-object lookup
`AddRequiredObject` (PORT unreal/src/Hooks/Hooks.cpp:514-525) builds query
FNames **FNAME_Find-first with Add fallback** (the port's own documented
workaround for "a bug with FNAME_Add ... a new name ... with a number instead
of retrieving the existing one", Hooks.cpp:518) and compares them against
stored NamePrivate FNames in `StaticFindObject_InternalNoToStringFromNames`
(PORT UObjectGlobals.cpp:161-208, ForEachUObject at :175). The missing-keys
log itself: L26R4 TypeChecker.cpp:1016 `[TypeScan] scan-complete
missing-keys=N core-ready=…` (l24 run: missing-keys=3, core-ready=true,
evidence/l24-ue4ss-4.log:603).

**Key property:** the InternalSlow predicate never interns the QUERY. It
converts each object's own stored name to a string (engine-side, proven
working at init and used again below) and compares strings. An incorrect
FName id can't affect it.

### 1b. RUNTIME FindFirstOf/FindAllOf (empty in-game)

```
Lua mod (e.g. zzdiag-v3/main.lua:29-35)
  RegisterKeyBind(F7) → ExecuteInGameThread(function()
      local db = FindFirstOf("ItemDatabaseSubsystem") ... end)
  └─ LuaMod.cpp:1632-1657 (PORT ue4ss/src/Mod/LuaMod.cpp)
       FindFirstOf( ensure_str(lua.get_string()) )            [:1651]
  └─ l22-diag UObjectGlobals.cpp:548-550 (PORT :501-503, pre-CP)
       FindFirstOf( FName(ensure_str(ClassName)) )
       — FName ctor default FindType = FNAME_Add (NameTypes.hpp:307,312)
       [l26 candidate: MakeFindXOfClassName = FNAME_Find-first, Add fallback
        + '[FFO] add-fallback' marker — L26R4 UObjectGlobals.cpp:481-525]
  └─ FindFirstOf(FName)  PORT UObjectGlobals.cpp:434-479
       :438  if (UnrealInitializer::ShouldUseHashTables())  — FALSE on PS5 (§1c)
       :454-476  ForEachUObject( [&]{
           Class = Object->GetClassPrivate();                        [:455]
           Class->GetNamePrivate().Equals(ClassName)                 [:457]
             — Equals compares ComparisonIndex ONLY (NameTypes.hpp:345-347)
           IsValidObjectForFindXOf(Object)                           [:220-224]
           TSuperStructRange(Class) ancestor name Equals             [:466-473]
         } )
  └─ ForEachUObject → ForEachUObject_Chunked  [:980-1000/:882-911]  — SAME function
```

FindAllOf(FName) is the same shape (PORT :546-590; the bUseCache overload
:506-539 goes through `FindObjectSearcher<AnyInstance,AnyInstance>` whose
Slow arm is the same ForEachUObject — ObjectSearcher.cpp:104-113 — and whose
Fast pool arm shares the same per-object reads minus the exclusion filter,
CR §4).

### 1c. Why the hash-table arm is irrelevant on PS5

`ShouldUseHashTables()` (PORT unreal/src/UnrealInitializer.cpp:108-110) =
`!bForceGUObjectArrayForIteration && FUObjectHashTables::IsAvailable()`. On
Prospero the linker consumes
`UObjectHashTablesNativeUnavailable.cpp` (spike dir
`ue4ss-native-hash-tables-1/source/`), where `IsConfigured/IsAvailable/
RunSelfTest` all delegate to `HashTablesNativePolicy` stubs returning
**false** (`NativeHashTablesUnavailablePolicy.hpp:9-11`); `Get()`/`Lock()` are
`[[noreturn]]` refusals (:19-30). Verified compiled in: l24-build-r2
pass1/runtime.map shows `_ZN2RC6Unreal18FUObjectHashTables11IsAvailableEv`
etc. defined in `unreal-uobjecthashtablesnativeunavailable.o`. So the claim
"FindXOf uses the SAME ForEachUObject as the type-scan" is TRUE for this
build — the :438-452 hash arm never executes. (UE4SSProgram's self-test wiring
at L26R4 UE4SSProgram.cpp:1353-1384 correspondingly logs "[HashTables]
FUObjectHashTables unavailable, object lookups use GUObjectArray".)

### 1d. The only differences that exist

| Aspect | Init type-scan | Runtime FindXOf | Same? |
|---|---|---|---|
| Iteration function | ForEachUObject_Chunked (UObjectGlobals.cpp:882-911) | identical | **SAME** |
| GUObjectArray source | port global, seed slot 0 (§3) | same global | **SAME** |
| Chunk bound | NumChunks read per call (:893); GetObjects() re-read per item (:900) | identical | **SAME** |
| Index range | all chunks × 64K slots (NumElementsPerChunk, UObjectArray.hpp:88); slots past NumElements are engine-zeroed → skipped by `!Object` (:822/:850/:903) | identical | **SAME** |
| Flag mask | `IsUnreachable()` skip (:903) only | same skip + `IsValidObjectForFindXOf` (RF_ClassDefaultObject|RF_ArchetypeObject exclusion + IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct>, :220-224) | differs, but l25-evening success proves the filter passes live objects (§5) |
| **Query representation** | **never an FName — `iequal` on GetFullName() STRING (:144-147)** | **FName via bound engine ctor; `Equals` = ComparisonIndex only (:457, NameTypes.hpp:345-347)** | **DIFFERENT — prime suspect** |
| Thread/timing | startup thread, game thread locked/paused (early-scan comment PORT ue4ss/src/UE4SSProgram.cpp:1325-1328) | game thread itself, via ExecuteInGameThread hook closure (§4) | differs, no permission effect (§4) |

## 2. The prime suspect in detail (task 2)

There is no cached-pointer-vs-reread, index-range, flag-mask, or chunk-bound
difference between the two walks — those are byte-identical (table §1d; CR §1
binary-verified every offset/bit the walk reads). The difference that survives
is upstream of the walk:

1. `FindFirstOf(const std::string&)` converts the query string to an FName
   through `FName::ConstructorInternal` — the ENGINE ctor bound from seed
   slot 2 (linked 0x23d590 → runtime 0x63d590; SeedTable.h slot 2). Pre-CP it
   did this with default **FNAME_Add**; the l26 candidate does **FNAME_Find**
   first with Add fallback (L26R4 UObjectGlobals.cpp:514-521).
2. The walk then requires `Class->GetNamePrivate().Equals(ClassName)` —
   **ComparisonIndex equality only** (NameTypes.hpp:345-347).
3. CB's binary decode: Add-mode and Find-mode are DIFFERENT engine call paths
   (Store 0x23c0b0 vs shard-Find 0x23c340) that share an id space only "on
   paper" (`id & 0x1fffffff` both). Whether the shard-Find hit, as invoked
   through the bound wide-char ctor (0x23d590→0x23d600, `_NN`-less,
   ASCII-collapsed), returns the identical id the class names were interned
   under in the grown post-asset-load pool — never proven on console.
4. Session evidence brackets it exactly:
   - l25 evening, cache-on session, PRE-CP build (Add-mode query): the same
     walk, filter, offsets and seed FOUND the object in-game
     (CONSOLE-RESULTS §2.2). So walk + GUObjectArray + Equals + filter are all
     proven ALIVE at runtime in that binary.
   - l26 (Find-first query): nil for EVERY probed class, no crash, and zero
     `[FFO] add-fallback` markers — i.e. Find "hit" (returned non-None) yet
     Equals missed every stored name.
   The ONLY variable that changed between those two outcomes is the query
   construction mode Add→Find (CP §2: walk bodies byte-identical; AA's
   FUObjectArray edits touched only the listener zone ≥+0x30 which this path
   never reads — CR §2).

Conclusion: init-scan-works + runtime-walk-empty is the signature of
**query-id mismatch, not walk failure**. The stored side is proven readable
(init GetFullName matched; engine compares the same slots in its own code);
the constructed side is the one unverified link.

## 3. GUObjectArray pointer lifecycle (task 3)

- **What the pointer is.** The engine's `GUObjectArray` is a static
  FUObjectArray OBJECT in the main image BSS, not a pointer slot. Seed slot 0:
  linked 0x93BA110 → runtime 0x97BA110 (= base 0x400000 + label), derivation
  "1565 rip-relative lea/mov code sites target linked 0x93ba110 exactly"
  (l26-package-B/lineage/SeedTable.h:31; matches AA's 0x97BA110). The runtime
  address sits in the image BSS extent (SeedValidation.hpp comments:
  LOAD7 BSS tail [0x970c000,0x9a34000) runtime).
- **When bound.** Once, during `consume_required_slot_seeds`
  (l25-runtime/copyin-probe/candidate/src/UnrealInitializer.cpp:1084-1098):
  address-class check (DataReadable) + pointer probe, then
  `UObjectArray::SetupGUObjectArrayAddress(reinterpret_cast<void*>(0x97BA110))`
  → assigns the port global `FUObjectArray* GUObjectArray{}` (UObjectArray.cpp:344-347,
  :453). The scan path can also bind it from a Lua AOB override
  (ue4ss/src/Signatures.cpp:187) but on PS5 the seed arms
  (`config.g_uobject_array = !ScanOverrides.guobjectarray`,
  UnrealInitializer.cpp:568/:1287-line-family) and the second-pass scan is
  skipped when the seed result is present (PORT UnrealInitializer.cpp:850-873).
- **Could it be stale by mod-runtime?** No mechanism found:
  - The engine never moves its own static object; PS5 does not relocate a
    loaded module's BSS. Heap relocation affects the array's INTERNALS, not
    the object's address.
  - The internals are re-read live on every call: `ForEachUObject_Chunked`
    dereferences `GUObjectArray->GetObjObjects()` per call (:892), reads
    `NumChunks` per call (:893), and re-reads `GetObjects()` (the chunk
    pointer array) INSIDE the per-item loop (:900) — so even mid-walk chunk
    growth is picked up. `UObjectArray::GetNumChunks/GetNumElements`
    (UObjectArray.cpp:396-402) likewise deref the global fresh.
  - No writer rebinds the pointer after init (grep: only UnrealInitializer.cpp:852
    and Signatures.cpp:187), and nothing ever writes `nullptr` back.
  - A dead/stale walk would also kill the init-time InternalSlow probes and
    any full-name string probe at runtime; the l25 evening session proves the
    same binding found objects at runtime.
- **Does the walk re-read the global per call?** It re-reads the port's cached
  pointer VARIABLE per call (`if (!GUObjectArray)` then member derefs) but
  never re-derives it from the seed/base — by design, because the target is an
  object, not an indirection.

## 4. Thread context (task 4)

- Init: the type-scan runs inside `UnrealInitializer::Initialize` on the
  mod's startup thread; the optional early scan explicitly runs "while the
  game thread is locked after UE4SS is attached" (PORT
  ue4ss/src/UE4SSProgram.cpp:1325-1328). The engine is quiesced.
- Runtime: `ExecuteInGameThread` (LuaMod.cpp:4690+) queues the closure into a
  hook-Lua state drained by the EngineTick/ProcessEvent hook — i.e. the walk
  executes ON the game thread (on this PS5 runtime the drain pump is the R3
  FuncPtr marshal; EngineTick detour refused — ACCEPTANCE-LADDER.md:95).
- Permissions/TLB: no asymmetry exists that matters. Both threads are in the
  same process and PS5 address space; the main image's BSS/pages holding
  GUObjectArray, the chunks, and all UObjects are ordinary readable data from
  any thread (the seed arming already verified DataReadable class from the
  mod thread, and the init scan read the same pages from the startup thread).
  If anything, running ON the game thread is the SAFER context — it is the
  array's own mutator, so it cannot observe mid-mutation tears that an
  off-thread reader could.
- The one thread-context difference with teeth is TIMING: at init the FName
  pool is small/early-state; at runtime it has grown/repacked after asset
  load. That feeds the §2 suspect (Find-mode id divergence in the grown pool),
  not any permission story.

## 5. Predicted v6 signature reconciliation (task 5)

What EXACTLY makes init-scan-works + runtime-walk-empty, and how v6 should
read out (extends CR §4 with the GUObjectArray-lifecycle proof):

| v6 observation | Reconciliation |
|---|---|
| FindAllOf 2-arg counts all zero, cache-path zero, BUT a full-name string probe (`StaticFindObject_InternalSlow`-family / `FindObject("/Script/Engine.PlayerController")`) resolves at runtime | **Confirmed N1 (this report's prime suspect):** walk alive over a healthy seed-bound GUObjectArray; query FName's Find-mode ComparisonIndex ≠ stored indices. Expected outcome given §2's bracketing evidence. |
| Everything zero INCLUDING the full-name string probe / InternalSlow | Walk-dead: NumChunks==0 or chunks pointer corrupted at runtime. Would contradict §3 (no rebind writers, per-call live re-reads, l25 runtime success) — treat as memory corruption, re-check `[Seeds] slot 0` bind line and enable ObjectSearcherProfiler `ObjectsIterated` (0 vs ~35K splits it instantly — CR §1 note). |
| Counts non-zero but FindFirstOf still nil/invalid | Lua wrapper/validity layer (A2) — outside the walk. |
| 2-arg zero but cache-path non-zero | Exclusion filter (E1: IsA storages) — log the six StaticClass storages' full names in-game. |
| `[FFO] add-fallback [n:X]` lines for real class names | Find genuinely missed the pool (name absent) — different family (GATE-1/pool admission), not id-space. |

The exact mechanism sentence for v6: **the init scan compares STRINGS
(engine ToString of stored names, proven) so it is immune to FName id
derivation; the runtime walk compares an INDEX constructed through the bound
ctor's Find path against indices the engine stored through its Store path —
if those two paths disagree in the grown pool (different packing, shard
subset, or display-vs-comparison entry), every Equals misses, the callback
runs to the end of a perfectly healthy walk, and FindFirstOf/FindAllOf
return empty with no crash and no marker.**

## 6. Fix shape

Do NOT touch the walk, the seeds, or the GUObjectArray binding — they are the
best-verified links (CR §1 binary ground truth; §3 lifecycle here).

1. **Verify-then-fallback (minimal, recommended):** after
   `FName(ClassName, FNAME_Find)`, round-trip `Name.ToString()` and compare to
   the query string; on mismatch (or on IsNone before using Add), fall back to
   an init-style scan — iterate ForEachUObject comparing
   `Object->GetClassPrivate()->GetNamePrivate().ToString()` (or full name) to
   the query string. This uses exactly the mechanism proven at init and in the
   l25 evening session, converts the unproven id-equality into a checked
   operation, and keeps Add's side effects off the hit path.
2. **Or fix the id derivation at the FName adapter** (CB's decoded sites:
   Find/shard-Find 0x23c340 vs Store 0x23c0b0, `id & 0x1fffffff` packing) so
   the bound ctor's Find result equals stored ids — narrower but requires
   proving the engine's exact Find/Store id relationship in the grown pool.
3. **v6 instrumentation to carry:** per-probe log of constructed query id vs a
   known stored class's NamePrivate id + ToString of both; ObjectSearcherProfiler
   `ObjectsIterated` counter. These four numbers pin the reconciliation table
   above in one F7 press.

— agent CW (ZCode), 2026-10-08. Read-only; only this report file written.
