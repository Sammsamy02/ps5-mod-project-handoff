# APPLY ORDER + VERIFICATION — lookup-speed patches (agent IT, 2026-10-12)

Claude round-13 item 2: "Profile the per-object cost in FindAllOf/FindFirstOf
(likely per-object FName decoding or safety checks; compare class pointers
instead) and fix that first. Prefer the engine's own StaticFindObject path and
upstream's UObjectArray cache (listeners are console-proven) over binding
FUObjectHashTables. Target: every lookup well under one frame."

Full analysis: `outputs/2026-10-08-l25-prep/IT-LOOKUP-SPEED.md`.

## 1. Files in this directory

| File | Contents |
|---|---|
| `01-foreach-chunk-bounds.patch` | UObjectGlobals.cpp — clamp `ForEachUObject_Chunked` to `GetNumElements()` (stop scanning the ~30K dead slots of the last 64K chunk) |
| `02-staticfindobject-name-first.patch` | UObjectGlobals.cpp — FindObject walk predicate: integer name compare FIRST; class/outer checks only on name hits; fixes the dead short-name/no-outer branch (55 ms guaranteed miss → resolves top-level objects, engine Hash semantics) |
| `03-findxof-walk-memo.patch` | UObjectGlobals.cpp — per-walk class-verdict memo for `IsValidObjectForFindXOf` + the query super-chain test (the ~2.2 µs/object cost: 6 IsA chain walks + a 7th chain walk, ~30-40 scattered loads → 2 hot probes) |
| `04-findxof-name-cache-uobjectglobals.patch` | UObjectGlobals.cpp — the name-keyed bucket cache (one bucket per DISTINCT class-name FName index in the chain, ObjectSearcherPool-style add/remove) + the `arm=namecache` query arms in FindFirstOf(FName)/FindAllOf(FName,…) |
| `04-findxof-name-cache-classlistener.patch` | ClassListener.cpp — implements `FClassCreateListener::NotifyUObjectCreated` (body only; registration stays skipped), delete listener now also removes buckets |
| `04-findxof-name-cache-unrealinitializer.patch` | UnrealInitializer.cpp — populate buckets in the existing cache-block walk; ACTIVATE after population; REPLACE the hand-rolled `AddUObjectCreateListener` registration with a loud SKIP (GMalloc fault family); StaticConstructObject hook + `RetireUObjectArrayCache` maintain the buckets |
| `05-settings-cache-on.md` | deploy-time ini reference (the 1947 B cache-on artifact already exists; when it may be flipped) |
| `syntax-receipt-*.log` | one `-fsyntax-only` receipt per edited TU (see §4) |
| `unpatched-sha256.txt` | live-tree shas the patches were generated against |

## 2. Apply order (strict; each patch applies on top of the previous)

Apply with `patch -p1` from the candidate tree root
(`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`):

```
patch -p1 < 01-foreach-chunk-bounds.patch
patch -p1 < 02-staticfindobject-name-first.patch
patch -p1 < 03-findxof-walk-memo.patch
patch -p1 < 04-findxof-name-cache-uobjectglobals.patch
patch -p1 < 04-findxof-name-cache-classlistener.patch
patch -p1 < 04-findxof-name-cache-unrealinitializer.patch
```

Verified: all six dry-run clean on the live-tree bytes (unpatched-sha256.txt),
and the chained application reproduces the syntax-checked files byte-identical
(`cmp` OK for all three TUs).

Per the mission law (one labelled gated build per fix), 01-03 are shippable
without 04; 04 without the settings flip is inert (buckets populate only when
`bUseUObjectArrayCache=true`).

## 3. Behavior contract (reviewers)

- 01: semantics-identical (slots ≥ NumElements are unallocated, can never hold
  a live UObject).
- 02: name-first reorder is result-identical for every shape that could match
  before (explicit outer / ANY_PACKAGE / long-name package filter). ONE
  intentional change: a SHORT name with NO outer used to be a guaranteed miss
  (the old predicate compared `GetOutermost()->GetNamePrivate()` against
  `NAME_None` before ever comparing the name); it now matches TOP-LEVEL
  objects only — the engine's own name-only Hash semantics — so
  `StaticFindObject("GameEngine")` resolves the package instead of scanning
  35K objects and returning nil.
- 03: control flow preserved exactly (first direct match breaks; a super-chain
  match records and keeps scanning, so the LAST chain match wins when no
  direct match exists — upstream's quirk, kept). Only predicate EVALUATION is
  memoized; verdicts depend solely on the class pointer and are stable for a
  class's lifetime. Collisions in the direct-mapped table recompute (slow,
  never wrong).
- 04: FindFirstOf/FindAllOf consult (a) engine hash tables when available,
  then (b) the name buckets when active, then (c) the (now memoized) walk.
  FindAllOf may return the same SET in a different ORDER than the walk after
  bucket swap-removes (upstream's pools have the same property). The engine
  create-listener registration is NOT restored — see the UnrealInitializer
  hunk; that is the remaining "missing half" to pin later exactly like the
  variant-C delete pair.

## 4. Syntax receipts (one check per edited TU, prospero-clang++ -fsyntax-only)

- `syntax-receipt-UObjectGlobals.log` — staged copy, UE4SSProgram-derived
  unit flags (superset include set), exit 0, 0 errors.
- `syntax-receipt-ClassListener.log` — same command family, exit 0, 0 errors.
- `syntax-receipt-UnrealInitializer.log` — the TU's OWN recorded unit command
  (l26-A pass1 `unreal-unrealinitializer.command.json`, `-c -o -MF` swapped
  for `-fsyntax-only`), run on a /tmp full-tree copy (the TU has file-relative
  includes), exit 0, 0 errors.
- Wrapper invocation requires `LLVM_CONFIG=<...>/LLVM-21.1.8-macOS-ARM64/bin/
  llvm-config` (the sdk wrapper resolves its host compiler through it).

## 5. Expected console evidence on the fix build

- With 01-03 only (cache still off): [FFO2] `arm=walk` lines keep firing but
  the self-test `SLOW-ms=` suffixes should drop from ~78/86/55 to single-digit
  ms (est. 5-10x from the memo + chunk clamp + name-first reject).
- With 04 + the ini flip: `[LookupCache] name-keyed FindXOf buckets populated
  and activated`, `[LookupCache] engine create-listener registration SKIPPED
  ...`, [FFO2] `arm=namecache`, and lookups at µs scale.
- If a regression appears, bisect by labelled build: 01 → +02 → +03 → +04
  (the four concerns are independent; 04's UnrealInitializer hunk is the only
  one that removes a call — the create-listener registration — and that call
  was already never made on the frozen cache-off configuration).

## 6. Out of scope / untouched

- Lua `IsValid` binding: already O(1) upstream parity (see IT-LOOKUP-SPEED §3);
  the measured 86 ms is the self-test helper's NESTED FindFirstOf, not the
  binding. No change staged.
- FUObjectHashTables binding (GATE): untouched, per the directive.
- The engine create-listener op pin (binary disassembly of
  AddUObjectCreateListener + prologue admission): NOT done here (no console,
  no new two-source layout facts invented); patch 04 leaves the prepared body
  and the loud skip so the pin slots in later.
