# IT-LOOKUP-SPEED — per-object lookup cost + O(1) fixes (agent IT, 2026-10-12)

Claude round-13 item 2. Battery-safe ANALYSIS + patch prep: no builds, no
console, no live-tree edits. Patches staged at
`round5-staging/lookup-speed/` (see its APPLY-NOTES.md). Live tree analyzed:
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (the tree under the
1e49188b acceptance-pass-1 build line). All line numbers below are live-tree.

---

## 1. THE PER-OBJECT COST — measured, and what it actually is

Evidence (`evidence/acceptance-pass2-20261012.log`, repeated across runs):

```
68/168 FindFirstOf PASS valid=true SLOW-ms=78
69/168 FindAllOf  PASS n=1     SLOW-ms=78   (86 in another run)
71/168 StaticFindObject-short PASS valid=false SLOW-ms=55
73/168 uobj.IsValid PASS valid SLOW-ms=86
[FFO2] q='GameEngine' id=0x47036 num=0 scanned=35479 exact=2 viaSuper=0 ... arm=walk
```

78 ms / 35479 objects ≈ **2.2 µs per object**. The task's prime suspect —
per-object FName string decoding — is NOT what the walk does:

- The Lua `FindFirstOf`/`FindAllOf` globals call the **FName overloads**
  (`LuaMod.cpp:1904`, `:1959` → `FindFirstOf(FName)` / `FindAllOf(FName,…)`,
  `UObjectGlobals.cpp:1055`/`1357`-area).
- The Prospero string wrappers decode exactly ONCE per call
  (`MakeFindXOfClassName` + `FindXOfQueryMismatch`, `UObjectGlobals.cpp:700`/
  `:842`), not per object. Per-object decode exists only in the
  `FindFirstOfStringWalk`/`FindAllOfStringWalk` FALLBACK arms (query-index
  corruption cases; the FFO2 log shows the fast FName walk ran, `arm=walk`,
  id=0x47036 resolved).

The per-object cost is `IsValidObjectForFindXOf` (`UObjectGlobals.cpp:288`):

```cpp
return !object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
    && !object->IsA<UClass>() && !object->IsA<UFunction>()
    && !object->IsA<UPackage>() && !object->IsA<UEnum>()
    && !object->IsA<UScriptStruct>();
```

For the COMMON object (a normal actor/component — none of the six checks are
true, so nothing short-circuits), each `IsA<T>()` is
`GetClassPrivate()->IsChildOf(T::StaticClass())` — a FULL
`UStruct::SuperStruct` chain walk (`Class.hpp:177-188`, `GetSuperStruct()` is
a cached-offset read but through scattered heap lines), and
`HasAnyFlags` is a virtual versioned-container call. The walk then runs
`TSuperStructRange(Class)` — a SEVENTH walk of the same chain — for the query
match. That is **~30-40 dependent, scattered loads per object** (6 × chain
depth ~5, plus StaticClass() loads); at 60-100 ns per dependent miss on the
console it fully accounts for 2.2 µs.

Amplifier: `ForEachUObject_Chunked` (`UObjectGlobals.cpp:1648`) scans the
FULL `64*1024` slots of every chunk — with 35,479 live elements in chunk 0
that is ~30K dead flag-reads per walk beyond `GetNumElements()`.

Secondary sites with the same shape:
- `FindFirstOf`'s super walk + filter: same body (patched).
- `FindObject` scan predicate (`UObjectGlobals.cpp:556-613`): per object it
  ran `Object->IsA(Class)` (chain walk) and, for short names with no outer,
  `Object->GetOutermost()` (a full OuterPrivate chain walk) compared against
  `PackageName` — which is `NAME_None` for every short-name query, and a live
  object's outermost is its UPackage (never None) → **the branch continued
  for every object and the object NAME was never compared at all**. That is
  the 55 ms `StaticFindObject-short` line: a guaranteed full-scan miss. (The
  long-name path is healthier: it resolves through the FNAME_Find part walk,
  `UObjectGlobals.cpp:460-508`, agent-EC's l26 fix.)

## 2. THE O(1) FIX — class-pointer verdicts, integer name compare first

The name compare itself was already the right shape: `FName::Equals` is
`ComparisonIndex == Other.ComparisonIndex` (`NameTypes.hpp:471-474`), a u32
compare against the once-per-call resolved query — no decode. What was missing
is making everything ELSE O(1):

**Patch 03 (memo):** both expensive predicates depend ONLY on the object's
class pointer, and a session has a few thousand DISTINCT classes for ~35K
objects. A per-walk direct-mapped `{UClass* → verdicts}` table (2048 entries,
32 KB) memoizes (a) the IsValidObjectForFindXOf verdict, (b) "does this
class's chain contain the query index". Per object the walk becomes: 1 class
read + 1 int compare + 1-2 hot probes. Verdicts are stable (the flags are
construct-time and never cleared; IsA verdicts are per-class; classes don't
move in chains at runtime). Control flow is preserved exactly — including
upstream's quirk that a chain-match records and KEEPS SCANNING (last chain
match wins when no direct match exists). Est. 78 ms → <10 ms at 40K objects.

**Patch 01 (chunk clamp):** bound the final chunk by `GetNumElements()`
semantics-identical (slots ≥ NumElements are unallocated), removing ~30K dead
slot reads per walk.

**Patch 02 (StaticFindObject name-first):** compare the name FIRST (1 read +
1 int compare rejects ~all objects), then class/outer checks only on name
hits. The dead short-name/no-outer branch now matches TOP-LEVEL objects only
(`GetOuterPrivate() == nullptr`) — the engine's own name-only Hash semantics —
so `StaticFindObject("GameEngine")` actually resolves. The long-name fallback
keeps its package filter (outermost name == package part), now evaluated only
on name-matched candidates.

## 3. THE IsValid FIX — it was never the binding

The task asked to fix IsValid "if our IsValid calls anything more than the
O(1) index-chunk-pointer check". It does not: the live binding
(`include/LuaType/LuaUObject.hpp:865-877`) is byte-for-byte upstream's shape —
null check + `special_invalid_ptr` check + `is_object_in_global_unreal_object_map`
(an `unordered_set<size_t>` of pointer hashes, `LuaUObject.cpp:72-76`; HashObject
is a pointer cast, `UObject.cpp:570`) + `IsUnreachable()` (=
`IndexToObject(GetInternalIndex())->IsUnreachable()`, one flags read via the
cached-offset getters). All O(1).

The 86 ms "uobj.IsValid" self-test line is explained by the test harness, not
the binding: entry 73 is `S.g("PlayerController")` (`ueselftest/part5.lua:10`
via `main.lua:14`) — `S.g` caches under `"g.PlayerController"`, which entry
68 never populated (it stored `S.h.gpc`), so entry 73 ran a WHOLE SECOND
FindFirstOf (the 78 ms walk) plus the O(1) IsValid. Fix the walk and IsValid's
measured number collapses with it. **No IsValid code change staged** — this
document is the correction to the round-13 premise ("IsValid ... walks the
object array per call" — it doesn't; its self-test wrapper does).

## 4. THE CACHE — name→objects buckets, listener-maintained

This port's `bUseUObjectArrayCache` machinery is NOT name-keyed: it builds
four CATEGORY pools (`ObjectSearcherPool<UClass/…>`, `UnrealInitializer.cpp:
562-614`) — even fully enabled, `FindFirstOf(name)` still scans a whole pool.
Upstream's cache concept is name→objects. Patch 04 stages that, on facts
already console-proven:

- **Buckets**: one bucket per DISTINCT class-name FName comparison index in
  an object's class CHAIN (so `FindAllOf("Actor")` finds subclass instances —
  exactly the walk's semantics). Bucket = `vector<const FUObjectItem*>` +
  swap-remove index map (the ObjectSearcherPool pattern). Insert filter =
  IsValidObjectForFindXOf evaluated ONCE per object at insert (flags never
  clear; IsA verdicts stable), so queries re-check only IsUnreachable.
- **Population**: the existing single init walk in the cache block (extended,
  not replaced).
- **Removal**: `FClassDeleteListener::NotifyUObjectDeleted` — the
  console-proven variant-C engine-side delete listener already runs this
  body; it now also removes the dying object from every bucket of its chain.
- **Addition**: the existing StaticConstructObject post-hook (covers
  essentially all engine construction — it is THE UObject allocation path)
  AND a now-implemented `FClassCreateListener::NotifyUObjectCreated` body.
- **Registration gap named honestly**: `UObjectArray::AddUObjectCreateListener`
  (`UObjectArray.cpp:760-768`) is still the hand-rolled engine-TArray Append —
  the GMalloc virtual-call fault family that stalled every PS5 init while the
  cache was last on. Patch 04 REMOVES that registration from the cache block
  (loud `[LookupCache] ... SKIPPED` line) instead of re-arming the fault;
  maintenance runs through init walk + construct hook + delete listener. The
  engine-side CREATE op (pin + prologue admission, mirroring
  `SetupEngineDeleteListenerOps`) is the remaining "missing half" to add when
  a new binary pin is allowed — the listener body is ready for it.
- **Lifecycle**: buckets deactivate+clear inside `RetireUObjectArrayCache`
  with the pools and the global object cache.
- **Query**: FindFirstOf/FindAllOf try hash tables first (still the engine's
  own path when GATE ever clears), then the buckets (`arm=namecache` in the
  FFO2 oracle), then the memoized walk. FindFirstOf preserves the walk's
  result rule (first direct match outranks; else LAST chain match).
  FindAllOf returns the same set; order can differ after swap-removes
  (upstream pools have the same property).
- **Settings**: the 1947 B cache-on ini already exists
  (`evidence/l25-settings-cacheon-1947.ini`,
  sha 5847362e…9031); flip only on the labelled fix build — see
  `round5-staging/lookup-speed/05-settings-cache-on.md`.

Name-index-reuse soundness: a bucket id can only be reused by the pool after
the FName entry's refcount drops to zero; any object in that bucket implies
its class (holding that name) is alive, so non-empty buckets cannot alias a
reused id; emptied buckets are harmless. Same residual-risk class as the
existing pools.

## 5. THE STATICFINDOBJECT PATH

The port DOES wire the engine's hash-based path — `FUObjectHashTables::
StaticFindObjectByPath/StaticFindObject` run first when
`ShouldUseHashTables()` (`UObjectGlobals.cpp:396-414`; tables exist in the
base tree with a full self-test, `UObjectHashTables.hpp:175-209`). On this
target they are unavailable (GATE family: `IsAvailable()` false →
"[HashTables] FUObjectHashTables unavailable, object lookups use
GUObjectArray", `UE4SSProgram.cpp:1400`), so every lookup falls to the walk.
Per the directive I did NOT bind the tables (no new two-source layout facts);
patch 02 makes the fallback walk cheap and makes short-name lookups actually
resolvable.

## 6. Expected result

- Cache off (patches 01-03 only): FindFirstOf/FindAllOf est. <10 ms at 40K
  objects (memo ≈5-8x + chunk clamp), StaticFindObject-short ≈ sub-ms
  (integer-compare reject) and it can now HIT.
- Cache on (+ patch 04 + ini): FindFirstOf/FindAllOf = hash lookup + small
  bucket walk — µs scale; IsValid unchanged O(1) and its self-test line drops
  with the nested FindFirstOf.
- Every lookup well under one 16 ms frame in both configurations, satisfying
  the directive's target without FUObjectHashTables and without the
  create-listener fault family.

## 7. Verification status (honest bounds)

- Syntax-only compiled ALL three edited TUs with the prospero toolchain
  (receipts in `round5-staging/lookup-speed/syntax-receipt-*.log`); the
  UnrealInitializer check used its own recorded l26-A unit command.
- Patch chain dry-run clean + byte-identical roundtrip against the live tree
  (shas in `unpatched-sha256.txt`).
- NO build, NO console run, NO engine execution: the timing estimates are
  derived from the measured per-object arithmetic (78 ms / 35,479 ≈ 2.2 µs ×
  load-count reduction), not re-measured. The next labelled build + self-test
  (the SLOW-ms= lines are the ready-made probe) is the acceptance step.
