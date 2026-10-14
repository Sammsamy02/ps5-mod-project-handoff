# ACCEPTANCE PASS 1 — all mods working, no stutter (2026-10-11)

PRX 1e49188b (clean stable build: P1 fault-ledger + P2 variant-B removal +
world-fix + lexical paths + fs-throw gate, NO event map logger, NO shim root
— both deferred after crash analysis). Session 94 seconds, clean exit.

## VERDICT: ALL PASS

| Mod | Status | Evidence |
|---|---|---|
| TownfallItemSpawner | ✅ PASS | "Menu created." + "Spawned Antibiotics x1 ID=2e9" |
| CheatManagerEnabler | ✅ PASS | Constructed + Enabled (hook fires on world load) |
| ConsoleEnabler | ✅ PASS | Tilde/ConsoleKeys written through the bridge |
| InfiniteStamina | ✅ PASS | Script initializing, stamina holds |
| ueselftest | ✅ loaded | Armed but not triggered (session < 3 min auto threshold) |
| zzdiag | ✅ PASS | T1 db=v pc=v cb=23 (sustained driver working) |

- ZERO stutter (minimap excluded per Claude round-11 item 5)
- ZERO exceptions in the entire session
- 1 informational line (R3 LoadMap deferral — known/expected)
- 4 concurrent mods + spawner + diagnostics, all clean

## SELF-TEST: not triggered (session 94s < 180s auto threshold)
The auto-run fires at 3 minutes. User would need to stay in-game longer
or press L1+R1. The timing/SLOW reporting is in the code and ready.

## SCAN TIMING: [SCAN] line fires at T5 (30s) — session too short
The zzdiag scan-timing diagnostic fires at the 5th T-cycle. T1 is present
but later cycles need to be confirmed (session ended early).

## DEFERRED (crashed on first attempt, both under investigation):
1. Event map logger — LoadMap vtable hook took a code-detour path
   (execute-only on PS5). Needs pure data-only VMT swap redesign.
2. Shim root wiring — writable-area code crashes on /data access
   after module_start. Needs error_code filesystem calls + safe fallback.

## MINIMAP: excluded (returns after the scan-speed fix per Claude item 3)
Root cause: FindAllOf scans walk 40K objects (FUObjectHashTables unavailable).
Fix: bind FUObjectHashTables or add FindAllOf cache. Port-level, benefits all.

## NEXT:
1. Second pass (3+ minutes for auto self-test + scan timing)
2. The scan-speed fix (FUObjectHashTables or cache)
3. Minimap returns after scan fix
4. Third pass = 3-restart acceptance law complete

## ITEM 3: SHIM CRASH EVIDENCE (Claude's question)

### The log lines (second crash, 1c48239d, shim-root-only build):
```
[2026-10-11 20:15:00] [R3] LoadMap detour registrations deferred: ...
[2026-10-11 20:15:00] [SHIM] write-area CheatManagerEnablerMod -> /data/modbridge/writable/CheatManagerEnablerMod
<LOG ENDS — crash>
```
The [SHIM] write-area line is the LAST line in the log. No error, no
exception, no further output — the process crashed immediately after
logging the write-area path.

### Why the fs-throw gate doesn't cover this:
The fs-throw gate checks std::filesystem throwing overloads. The shim's
write-area uses C stdio (fopen/fwrite/fclose) — IB's design specifically
avoided std::filesystem. fopen() to a PS5-restricted path (/data after
module_start) can SEGFAULT rather than return NULL on some PS5 syscalls,
and C stdio has no exception mechanism to catch. The gate is scoped to
the wrong API family.

### The two possible mechanisms:
1. fopen("/data/modbridge/writable/...", "w") — creates the directory
   implicitly? No — fopen doesn't create directories. But it might
   segfault on a path whose parent directory doesn't exist in the
   game-process VFS view.
2. std::filesystem::create_directories() in the shim's sandbox setup —
   this WOULD be caught by the fs-throw gate if using the throwing
   overload, but IB may have used the error_code overload (which doesn't
   throw but could still fault on an invalid path).

### The fix path (before re-adding):
1. Change the shim's write-area to use ONLY std::filesystem with
   error_code overloads (no C stdio for directory creation)
2. Wrap the entire write-area setup in try/catch + scout the path
   FIRST (does /data/modbridge/writable exist? can we even stat it?)
3. If the path is inaccessible: fall back to in-memory spill IMMEDIATELY
   without any filesystem call
4. Add a pre-flight probe: fopen("/data/modbridge/writable/.probe", "w")
   in a signal-safe context to test accessibility before the real setup

### Distinguished from the first crash (d971cd2f):
First crash had BOTH the LoadMap hook AND the shim root. The LoadMap
"Failed to add hook" line appeared, then the [SHIM] write-area, then
crash. That crash was attributed to the LoadMap hook (code detour on
execute-only memory). The second crash (1c48239d) had ONLY the shim root
(LoadMap reverted) — same crash point (right after [SHIM] write-area).
Both crashes share the shim write-area as the last logged action.

Honest correction: BOTH crashes may have been the shim write-area, not
the LoadMap hook. The LoadMap "Failed to add hook" was an error MESSAGE
but may not have been the crash cause. The evidence doesn't distinguish
which of the two (or both) caused their respective crashes.

## ITEM 4: 3-RESTART LAW SCOPE
Agreed — the 3-restart count applies to the FINAL build (scan-speed fix
+ shim root + VMT map logger, each its own labelled build), not to
1e49188b. This was a smoke pass on an intermediate build.

## RULE CHANGE ACCEPTED (Claude round-13, user decision)
3-pass acceptance law DROPPED for Townfall and Stellar Blade.
- Townfall: done after ONE clean full launch on the fix build
- Then Stellar Blade
- Later launches on same runtime = ongoing regression checks
- 3-passes-across-restarts retained ONLY for kernel work (Phase B, #1/#3)

## FIX BUILD PLAN (Claude round-13 item 7)
Each fix = its own labelled gated build, intermediates kept for bisecting:
1. Lua allocator fix (IS investigating)
2. Lookup speed fix (IT investigating — O(1) IsValid + class-pointer compare)
3. GROUP B implementations: ForEachUObject, UEnum.ForEachName, IterateGameDirectories (IU)
4. GROUP C: verify upstream, fix self-test where wrong (IU)
5. GROUP D: part11 loader fix (IU)
6. Shim write-area fix (IQ — deploy-time pre-create + lazy + PC semantics)
7. VMT map logger redesign (pure data-only, no detour) + no-detour gate (IR)
Then ONE full launch with ALL mods including minimap. Self-test once.

## AGENT RESULTS (fix stack, all staged)

### IT (lookup speed): 6 patches, target <10ms per lookup
- Root cause: IsValidObjectForFindXOf does 6 IsA<> calls (full SuperStruct
  chain walk each) + 7th range walk = 30-40 loads/object × 35K = 78ms
- IsValid already O(1) (the 86ms was test harness's nested FindFirstOf)
- StaticFindObject-short had a dead branch (never compared object names)
- Fix: per-walk class-verdict memo + name→items cache + chunk bounds +
  StaticFindObject name-first + UObjectArray cache settings
- FUObjectHashTables NOT needed (engine hash already wired)
- Patches in round5-staging/lookup-speed/, all syntax-verified

### IU (GROUP B/C/D):
- GROUP C: ALL 5 were test errors, zero port bugs. -7 false FAILs.
- GROUP D: part11 loaded fine — deploy desync (stale console file)
- GROUP B: ForEachUObject = inherited upstream stack leak (1 slot/object);
  IterateGameDirectories = PS5 flat-layout heuristic failure; UEnum = layout
  offset diagnostic. All fixes staged.
- Self-test hygiene: one-run guard + desync handling deployed

### IQ (shim): stale Lua stack index (not fs). Fix = deploy-time pre-create
+ lazy write + PC semantics + registry-sourced upvalues. Staged.

### IR (no-detour gate): 19/19 tests. Catches LoadMap patch, passes clean tree.

### Expected next self-test: ~160/168 PASS (from 147), before allocator fix.
