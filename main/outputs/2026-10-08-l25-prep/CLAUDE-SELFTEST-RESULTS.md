# SELF-TEST RESULTS — 3 runs, timing data, SLOW/FAIL triage (2026-10-12)

PRX 1e49188b (clean stable build). Session ~3 minutes. User pressed L1+R1
(triggering run 1), then L1+R1 again (run 2), then auto or manual (run 3).
ZERO exceptions/crashes in all 3 runs.

## PASS RATES
| Run | PASS | FAIL | SKIP | SLOW |
|---|---|---|---|---|
| 1 | 147/168 (87.5%) | 12 | 9 | 5 |
| 2 | 147/168 (87.5%) | 12 | 9 | 10 |
| 3 | 133/168 (79.2%) | 26 | 9 | 13 |

Run 3 degradation: the "not enough memory" failures expanded (see below).

## SLOW CALLS (the scan-cost evidence — Claude item 3's measurement)

| Call | Time | Budget (16ms/frame) | Over |
|---|---|---|---|
| FindFirstOf | 78ms | 16ms | **4.9x** |
| FindAllOf | 78-86ms | 16ms | **4.9-5.4x** |
| StaticFindObject | 55ms | 16ms | **3.4x** |
| ForEachUObject | 164ms | 16ms | **10.3x** |
| IsValid (on found object) | 86ms | 16ms | **5.4x** |

CONFIRMED: every object-lookup call is 50-86ms — the FUObjectHashTables
gap. At the minimap's 20Hz tick with multiple lookups per tick, this is
4-8 seconds of game-thread blocking per wall-clock second = the stutter.

## FAIL TRIAGE (grouped by fix)

### GROUP A: "not enough memory" (worsens with each run)
Calls 129-140: RegisterULocalPlayerExec*, RegisterConsoleCommand*,
IterateGameDirectories, RegisterKeyBindAsync, ExecuteInGameThread,
ExecuteAsync, ExecuteWithDelay.

These all REGISTER callbacks into the Lua registry. Each self-test run
registers new callbacks without unregistering. By run 3, the registry
is full. LIKELY NOT a real memory issue — it's callback accumulation.

**Fix**: unregister callbacks after testing (the self-test should clean
up after itself), or increase the Lua registry limit. Not a port bug.

### GROUP B: "iterate-err" (port gap)
- 100/168 UEnum.ForEachName — enum name iteration not implemented
- 133/168 ForEachUObject — object array iteration not implemented
- 134/168 IterateGameDirectories — game directory iteration not implemented

**Fix**: these are port-level features. ForEachUObject is the most impactful
(it's the foundation for several other APIs). The iterator needs the
UObjectArray walk (which we know works — the FFO2 oracle uses it).

### GROUP C: type mismatches (port bugs)
- 8/168 PropertyTypes.ObjectProperty returns table instead of expected type
- 20/168 __OriginalReturnValue returns nil
- 28/168 FName.Equals returns function instead of boolean
- 58/168 GetCurrentThreadId returns userdata instead of number
- 59/168 GetMainModThreadId returns userdata instead of number

**Fix**: each needs individual investigation. FName.Equals (28) is the most
likely to affect mods (it's a comparison operator).

### GROUP D: part11 load failure
part11.lua didn't load (returned a hex address instead of a table).
The part contains the io-roundtrip + hygiene tests + UEHelpers.GetWorld.
**Fix**: check part11.lua's require path and content.

## THE MENU ISSUE
The spawner's menu was created once (20:47:13) but got stuck when the user
"glitched" it. After the self-test ran (which does write-restore property
tests), the spawner couldn't open its menu again. Likely: the spawner's
`busy` flag was set when the menu glitched and never cleared.

**Not a port bug** — it's the mod's internal state management. The
write-restore tests in the self-test may have triggered the busy flag.

## SCAN TIMING ([SCAN] line)
The zzdiag [SCAN] line didn't fire — it triggers at T5 (30 seconds), but
the session may have been shorter than expected for the T-cycle counter.
The self-test's SLOW data provides the same measurement (FindAllOf = 78ms).

## GENERIC FIX LIST (ranked by mod impact — Claude item 3)
1. **FUObjectHashTables binding** (fixes FindFirstOf 78ms → <16ms,
   FindAllOf 78ms → <16ms, IsValid 86ms → <16ms) — affects every mod
   that looks up objects (ALL of them). This is THE fix.
2. **ForEachUObject iterator** (fixs 133 + enables downstream APIs) —
   affects the minimap's enemy scan, ActorDumper-class mods
3. **FName.Equals fix** (28) — affects any mod comparing names
4. **Lua registry cleanup** (the "not enough memory" class) — affects
   long-running sessions with many hook registrations
5. **part11 load fix** — affects the self-test completeness
