# agent-IT-lookupspeed — lookup-speed analysis + patch prep (2026-10-12)

Claude round-13 item 2. ANALYSIS + patch prep only: no builds, no console, no
live-tree edits. Full analysis: `IT-LOOKUP-SPEED.md`. Patches + apply notes +
syntax receipts: `round5-staging/lookup-speed/`.

## The per-object cost (answer to "what costs 2 µs?")

NOT per-object FName decoding — the Lua FindFirstOf/FindAllOf path uses the
FName overloads whose `Equals` is a u32 ComparisonIndex compare
(NameTypes.hpp:471), and the Prospero string wrappers decode once per CALL.
The cost is `IsValidObjectForFindXOf` (UObjectGlobals.cpp:288): SIX
`IsA<UClass/UFunction/UPackage/UEnum/UScriptStruct>` calls — each a full
SuperStruct chain walk — plus a virtual VC `HasAnyFlags`, plus the
`TSuperStructRange` query walk re-walking the same chain a 7th time. ~30-40
dependent scattered loads/object × 35,479 objects ([FFO2] scanned) = the
measured 78 ms. Amplifier: `ForEachUObject_Chunked` scans all 65,536 slots of
the last chunk (~30K dead reads past GetNumElements). StaticFindObject-short's
55 ms is worse-shaped: for a short name with no outer, the old predicate ran
`GetOutermost()` per object, compared it to NAME_None (never equal), and never
compared the object name at all — a guaranteed full-scan miss.

## IsValid correction

The Lua IsValid binding is ALREADY upstream's O(1) shape (null +
special_invalid_ptr + pointer-hash set + IsUnreachable flags read). The 86 ms
self-test line is the harness's nested second FindFirstOf
(`S.g("PlayerController")` in part5.lua:10, cache key not populated by entry
68). No IsValid change staged; the walk fix removes the measured number.

## The fixes staged (round5-staging/lookup-speed/, patch -p1, order matters)

1. `01-foreach-chunk-bounds.patch` — clamp chunked walk to NumElements.
2. `02-staticfindobject-name-first.patch` — name-first predicate (int compare
   rejects ~everything); short-name/no-outer now matches TOP-LEVEL objects
   (engine Hash semantics) instead of always missing; long-name package
   filter preserved.
3. `03-findxof-walk-memo.patch` — per-walk direct-mapped class-verdict memo
   for the 6-IsA filter + query-chain test; control flow preserved exactly
   (first direct match breaks; last chain match wins otherwise). Est. <10 ms
   at 40K objects.
4. `04-findxof-name-cache-*.patch` (3 files) — the name→items bucket cache
   (one bucket per DISTINCT class-name index in the CHAIN → subclass
   semantics; ObjectSearcherPool-style swap-remove; insert-filtered):
   populated by the existing init walk, maintained by the StaticConstructObject
   post-hook + the console-proven engine-side DELETE listener; create-listener
   body implemented but its registration REPLACED with a loud SKIP because
   `AddUObjectCreateListener` is still the GMalloc fault-family Append — the
   engine create-op pin (mirror of the variant-C delete pair) is the remaining
   half, prepared not invented. Retire path deactivates buckets. Query arms
   log `arm=namecache` in FFO2. Settings flip = the existing 1947 B
   `evidence/l25-settings-cacheon-1947.ini` (see 05-settings-cache-on.md).

## StaticFindObject

The engine hash path IS wired (FUObjectHashTables::StaticFindObjectByPath/
StaticFindObject, self-tested) but `IsAvailable()` is false on target (GATE)
— per the directive I did not bind it; patch 02 makes the fallback walk cheap
and correct for short names.

## Verification status

- prospero-clang++ `-fsyntax-only` on all 3 edited TUs (exit 0; UnrealInitializer
  checked with its own recorded l26-A unit command) — receipts staged.
- All six patches dry-run clean on the live-tree bytes and the chain
  roundtrips byte-identical (cmp OK).
- Timing claims are arithmetic from the measured 2.2 µs/object, not
  re-measured: acceptance = the next labelled build + ueselftest SLOW-ms
  lines (with 01-03 they should drop to single-digit ms; with 04+ini,
  `arm=namecache` and µs-scale lookups).
