# Track ISA: fast name lookups (StaticFindObject / FindObject / IsA by path)

All of this is Mac-only. Nothing was built, uploaded or launched.

## What it does (plain)

When a mod asks "find the object called X" or "is this a Y?", the runtime looks the
object up by name. On PC the engine keeps a name table for that. On the PS5 that table
can't be used (log: `[HashTables] FUObjectHashTables unavailable`), so every lookup
read all ~150,000 objects one by one. A hit stops early, but a **miss** (the name doesn't
exist) always reads all of them: about 20-40 ms each. Launch 10 shows `m:IsA` at about
0.5 ms per call, 40-90 calls per second, and `StaticFindObject` at up to 23 ms per call.

The fix adds a "by name" list to the object index we already have (the r9idx index that
made FindFirstOf/FindAllOf fast). For each object slot it stores the object's own name
number, grouped into buckets. A lookup now:

1. brings the index up to date, exactly like FindFirstOf does (new objects and freed slots);
2. takes only the objects whose own name matches (usually 1 to a few, at most a few thousand);
3. puts them in slot order, which is the same order the old full walk used;
4. runs **the same test the old walk ran** on each one. The first one that passes wins,
   just like before.

If the index can't be used, the old full walk runs as before (fail closed), and one
`[R9-NAME] ... uses the full walk: <reason>` line says why.

Expected cost: a few microseconds for the lookup itself, plus a small rename check
(below). A miss on a name that no object has costs about the same as a hit.

## How it differs from PC

- PC uses the engine's own name hash table. We can't read it on PS5, so we keep our own copy,
  fed only by mechanisms already proven on the console (the delete listener plus the
  index's own sync). No new engine address, no hooks, no patching.
- One gap PC doesn't have: the engine sends no event when an object is **renamed in place**
  (same object, same slot, new name). To handle that, every name lookup re-checks the stored
  name of 256 more slots (a cursor that keeps going round the whole array). So a renamed object
  is found under its new name within about 600 lookups (150k / 256). An object that was only
  moved to a new outer keeps its own name, and outers are always checked live, so that case
  is exact right away.

## Where it is used (all in src/UObjectGlobals.cpp)

- `StaticFindObject_InternalNoToStringFromNames` (the full-path lookup behind
  `StaticFindObject("/Script/Engine.Actor")` and `IsA("<path>")`): candidates are the objects
  whose own name is any one of the path parts, because that is the walk's first test. The walk's
  test was moved into a shared helper (`R9PathPartsMatch`), so the walk and the index run the
  same code.
- `FindObject` searcher walk (short names, ANY_PACKAGE, explicit outer, exact class, and the
  long-name fallback): only when the searcher is the plain whole-array walk (not a cached pool,
  not hash tables, and not a searcher the caller passed in). Candidates are the objects whose
  own name Equals `ShortName`. The walk's test is now a named lambda (`FindPredicate`), and the
  index and the walk both call it. That test can call GetFullName, so it runs outside the index lock.
- `LuaUObject.cpp is_a_implementation` is unchanged. It calls StaticFindObject, so it gets the
  speed-up for free.

## Proof at launch (log markers, exact literals)

- `[R9-NAME] object-name index active: {} named slots, {} buckets, rename sweep 256 slots/lookup (StaticFindObject/FindObject/IsA no longer walk the whole array)`
  (once, the first time the index answers a name lookup)
- `[R9-NAME] perf win_ms={} find_calls={} find_us={} find_avg_us={} find_max_us={} idx_lookups={} idx_cands={} walks={} misses={} renames={}`
  (at most once every 5 s while lookups happen. `find_avg_us` should be single or low double
  digits. `walks` should be 0 after start-up. `renames` counts in-place renames the sweep caught.)
- `[R9-NAME] miss {}/32 '{}' class={:X} outer={:X} exact={}`
  (the first 32 distinct names nobody could find. outer=0 means none, 1 means ANY_PACKAGE.)
- `[R9-NAME] miss list full (32 distinct names); later misses are only counted`
- `[R9-NAME] name lookup uses the full walk: object index disabled`
- `[R9-NAME] name lookup uses the full walk: delete listener not proven live yet` (expected once, early in start-up)
- `[R9-NAME] name lookup uses the full walk: index sync failed`
- `[R9-NAME] object index disabled after an exception; name lookups use the full walk`

Compare against launch 10: the `m:IsA=` and `StaticFindObject=` entries in `[PERF]` should
drop from about 0.5 ms or more per call to microseconds. The self-test `StaticFindObject-short`
should go from SLOW 39 ms to PASS.

## Same-result reasoning table (walk vs index)

| Case | Old walk | Index | Same? |
|---|---|---|---|
| Full path hit `/Script/Engine.Actor` | first object in slot order whose outer chain names are all path parts and whose chain length equals the part count | candidates = objects whose own name is a part (the walk's first test); same test, slot order, first wins | yes |
| Short name, no outer | first object named X with no outer (name-first test) | candidates named X, same test, slot order | yes |
| ANY_PACKAGE | first object named X | same | yes |
| Explicit outer | first object named X under that outer | same test | yes |
| Exact class / class filter | the plain walk also tests IsA(Class) per object; the predicate tests exact class + IsA | same predicate (bQuickSearch is false for the plain walk, so IsA still runs) | yes |
| Cached pool / hash searcher / caller searcher | unchanged | not used (index skipped) | yes (untouched) |
| Miss | reads every object, returns null | no candidate passes, returns null | yes |
| Name not in FName pool | decoded full-name walk (unchanged) | not used | yes (untouched) |
| Freed slot reused at the same address | delete listener marks the slot; sync re-reads it and re-links its name | slot re-observed, old name unlinked | yes |
| Object created after the last sync | appended slots are read at lookup time | exact sync first (new slots observed) | yes |
| Unreachable object (GC in flight) | skipped | treated as empty, re-checked every sync, live check at lookup | yes |
| Outer changed (Rename with a new outer, same name) | outer read live | own name unchanged; outer read live by the same test | yes |
| Name changed in place (Rename to a new name) | read live | stored name fixed by the sweep within about 600 lookups (or sooner if the slot is re-observed). Until then a lookup by the **new** name can miss that object, and a lookup by the **old** name re-tests it live and rejects it | **eventually** (the only gap) |
| Index disabled / delete listener not yet seen / early init | walk | walk (fail closed, one log line) | yes |

Host model: `model/name_index_model.py` (random create/free/reuse/unreachable/outer-move
workloads, both lookup shapes, index vs walk). Result is in `model/model-run.txt`:
6807 lookups with 0 mismatches without in-place renames. With in-place renames there was
1 mismatch in 6811 lookups. That is the known, bounded rename gap above.

## Cost and memory

- Per new object (Observe): one name read from the cache line the class read has already
  loaded, plus 3-4 writes into our own arrays. One-time first build: about +5-10 ms (estimate).
- Memory (GMalloc, like the rest of r9idx): about 13 bytes per slot plus 4 bytes per bucket.
  That is about 4.3 MB at 262,144 slots. When the slot arrays double, the buckets are rebuilt
  once (rare).
- Per lookup: exact sync (about 1 us when idle, per launch 10's `idx_ms`), a rename sweep of 256
  slots with prefetch (a few us, estimate), the bucket scan, and a sort of the candidates.
- The temporary candidate list uses GMalloc too (r9idx Vec).

## Checks run (counts)

- `syntax_check_tree.py`: **checked 6 units, 0 failed**. Unreal-uobjectglobals has 5 warnings,
  all from headers. That is the same 5 the unchanged base gives.
  Note: running the check with `--root isa/tree` fails with "redefinition of NAME_SIZE" etc.
  even on an **unmodified base copy**. The tree's include/ and the candidate's include/ both get
  pulled in, so NameTypes.hpp is defined twice. I touched no header, and isa/tree/include is
  byte-identical to the candidate include/. So I ran the check on `isa/check-root` (my src only).
  That gives the same include set as the real candidate.
- `field_gate.py --root isa/tree`: **PASS** (scanned 41 files, 461 member-reading calls,
  0 new unproven reads).
- `apply_isa.py` applied to a fresh copy of base-tree gives a tree byte-identical to `isa/tree`
  (11 exact-once edits). Running it a second time refuses (anchor matched 0 times, nothing written).

## Risks

- **G7 import gate not run** (it needs a compile, which this task doesn't allow). New code uses
  only things this unit already uses: std::chrono::steady_clock::now (R9WalkTimer), std::mutex,
  std::function (ObjectSearcher::ForEach), std::sort (Query), Output::send with integer and
  string-view arguments, and __atomic_* / __builtin_prefetch builtins. Every log number is
  printed as an integer (no float formatting). New statics stay inside this unit. G7 must still
  confirm this at build time.
- In-place renames are only caught eventually (see the table). It never returns an object
  that fails the walk's test. It can return a different valid match only if a renamed object
  with a lower slot number now also matches.
- `R9SearcherPeek` reads ObjectSearcher's protected members through member pointers. That is
  legal C++ and depends on the header layout: a plain walk is the one with no pool and no hash.
- Merge: only src/UObjectGlobals.cpp is touched, in r9idx, FindObject, and the parts-walk
  function. If FIELDS also edits r9idx (FastSlots / Observe), re-run apply_isa.py after it. The
  anchors are short and refuse on any drift.
- Not covered (follow-ups): `FindObjects(ClassName, ObjectShortName, ...)` (Lua FindObject
  with a class and a short name) and the decoded full-name fallback (never seen in launch 9/10 logs).

## Files

- `apply_isa.py`: the patch (argv[1] = root, default is the real candidate)
- `delta.diff`: `diff -ru base-tree isa/tree`
- `tree/`: patched tree. `check-root/`: src-only copy used for the syntax check. `syntax/`: syntax logs.
- `model/`: host-side model and its output
