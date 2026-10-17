# agent-FT-static-scan — receipt

Agent-FT, 2026-10-10. Task: per-mod static API scan of the 12-mod generality
ladder (+ extras) against the composed PS5 port, with predicted first failures
and an aggregate offline fix queue. ANALYSIS ONLY — no builds, no console, no
tree edits other than the two deliverables below.

## Deliverables

- `outputs/2026-10-08-l25-prep/mod-ladder/STATIC-SCAN.md` — the full scan
  (per-mod inventories with file:line cites, mappings, predicted first
  failures, aggregate gap list of 24 items, crash-law re-verification).
- This receipt.

Inputs consumed: MANIFEST.md (FO), all 12 mod sources read in full or
grep-censused (TREE-B LF + TREE-A CRLF; both TREE-A hashes re-verified identical
to manifest rows), UEHelpers.lua + jsbProfi.lua, FL-CONVERSION-INVENTORY.md
(+postscript). Agent-FS's API-PARITY-TABLE.md was absent at both polls; per task
order I ran my own registration-surface grep: candidate
`2026-10-08-l26-runtime/typescan-r4/candidate/{src,include}` (current
post-FL/FM/FK/FR/FW tree — line numbers have drifted from FO's) PLUS the
port-tree TUs the build actually composes (verified via
l25-A/pass1/COMPOSITION-RECEIPT.json: lua-luaxproperty, lua-luafoutputdevice,
lua-luamodref, lms-luamadesimple, luaraw-liolib/loslib from
ue4ss-stdio-consumer-adaptation-1/patched).

## Key corrections to FO's manifest (binding facts, current tree)

1. set.lua's "Reflection API absent" (c2) is WRONG: IsClass (LuaUObject.hpp:638),
   Reflection():GetProperty (:582-616, bridged), Property:ImportText and
   ContainerPtrToValuePtr (port LuaXProperty.cpp:215/:186), Property:IsValid
   (RemoteObject base, null-safe) are all compiled in. Only remaining seam:
   ImportText's buffer is still `ensure_str` (LuaXProperty.cpp:226) — new FW
   queue item.
2. Keybinds' "five absent dump globals" is stale: all seven registered as real
   registrations (LuaMod.cpp:2420-2479).
3. BPModLoaderMod does not use a Dirs global; it uses IterateGameDirectories
   (:3270) / CreateLogicModsDirectory (:3604) — both bound. RegisterBeginPlay/
   LoadMapPostHooks also bound.
4. io/os/debug are NOT missing: luaL_openlibs is called (port
   LuaMadeSimple.cpp:683) with the Sony-adapted liolib. Open question is console
   path semantics/writability (P0), not library presence.
5. Property reads return userdata again (FR 2026-10-10 parity revert) with
   bridged :ToString + FName/FText __tostring; FString __tostring still missing
   (FW TODO, no direct caller in the 12).
6. IsA(string) overload exists (LuaUObject.cpp:2482-2511) — MiniMap/ModMenu use
   it.
7. ModMenu: ALL RegisterHook/NotifyOnNewObject sites are pcall-guarded in the
   hash-pinned revision (FO's "unguarded :1234/:1250/:1539+" cites don't match
   the pinned file).
8. NEW latent MISSING: port ExecuteAsync takes (LuaFunction) only; ModMenu's
   fallbacks call ExecuteAsync(delayMs, fn) at :887/:1005/:2453/:2566/:2644 —
   dead branch while EITWD is registered; documented, not fixed.
9. jsbProfi's io.open is nil-safe → jsbLuaProfilerMod loads as a no-op even with
   broken io (mechanism correction).

## Extras found beyond the 12

- **stamina_main.lua** (`real-mod-lua-runner-1/inputs/`) — a REAL 13th mod
  (Townfall Infinite Stamina, 106 lines): UEHelpers + FindFirstOf + property
  reads + LoopAsync + PrintString with plain-string FString param + FName ctor +
  bool property Set. Lowest-risk real mod in the pool; natural early rung.
- ExampleItemSpawner stub (claude-takeover example, print-only, 17 lines).
- menu/minimap inputs in real-mod-lua-runner-1 are hash-identical to TREE-A
  copies; smoke.lua is a 1-line probe. No other Mods/ trees exist.

## Per-mod predicted-first-failure one-liners

1. **LineTraceMod** — LineTraceSingle struct-OUT table (main.lua:56): if OUT
   unfilled, HitResult.HitObjectHandle chain errors in the keybind callback; P5
   gates.
2. **CheatManagerEnablerMod** — RegisterHook at load (:3) is the first
   never-fired global (P6); then object-ref Set `CheatManager =` (:27) on fire.
3. **ConsoleEnablerMod** — array growth write `ConsoleKeys[#ConsoleKeys+1].KeyName=`
   (:36), the ladder's deepest write; add one growth-write to P3/P4 template
   before RUN-3.
4. **ConsoleCommandsMod** — RegisterConsoleCommandHandler registrations
   (set:1/summon:1/dump_object:283) + GlobalAr:Log on dispatch; load-time is
   low-risk; P0+P2 gate.
5. **SplitScreenMod** — bool property Set at load (:13, Init at :31) fails S1 if
   the Set machinery is broken; then CTRL-chord PAD-GAP makes keys silent.
6. **TownfallMiniMap** — unguarded-at-load LoopInGameThreadWithDelay (:4034/
   :4268) kills S1 if P7 red; second tier: str()→:ToString bridged chains and
   CRT canvas OUT (:533).
7. **TownfallModMenu** — same P7 exposure via LoopInGameThreadWithDelay(100,
   gameplayTick) (:2431); then RegisterHook machinery on first fire (all sites
   pcall-guarded → degrade, not abort).
8. **Keybinds** — loads clean (all 7 tool globals registered); on press,
   DumpAllObjects' Windows `\\` output path (LuaMod.cpp:2423-2425).
9. **BPML_GenericFunctions** — none at load; handlers unreachable (no BP
   caller); 10-arg SCO (:60) never fires.
10. **BPModLoaderMod** — IterateGameDirectories() on PS5 (:115) returns nil /
    nil-chains → the mod's own clean error; core function unreachable; SKIP
    stands on corrected mechanism.
11. **ActorDumperMod** — Level.Actors read through hardcoded offset 0x98 (:13) →
    garbage walk → native crash; offset law; never run.
12. **jsbLuaProfilerMod** — none at load (nil-safe io); SKIP on
    no-generality-value + hook-budget grounds.
13. **(extra) Stamina** — PrintString's plain-string FString param (:70),
    bridged-never-fired; otherwise all-proven surface; best cheap early rung.

## Aggregate priority (full table in STATIC-SCAN.md §F)

ToString chains (5 mods) > RegisterHook (4) > struct-OUT (3) > array
metamethods/growth (4) > io-on-console (4) > RemoteUnrealParam get/set (3) >
object-ref Set (3) > LIGWD/EITWD delay family (2, but kills both capstone rungs
at load) > NotifyOnNewObject (2) > FName ctor-FNAME_Find/Equal (2) > weak-object
Get (2) > IsA(string) (2) > SCO 2/10-arg (4) > console-handler family (1) >
ImportText ensure_str seam (FW) > ProcessConsoleExec (1) > ForEachUObject (1) >
IterateGameDirectories (1, SKIP) > ExecuteAsync 2-arg MISSING-latent (1) >
dump-path Windows separators (1) > RegisterCustomProperty offset (SKIP) >
RegisterCustomEvent (1) > stdlib debug/os (2) > PAD-GAP chords (6 mods, launch
config).

Crash-law sweep: zero FindFunction/bpfn/GetDefaultClass/GetAllClasses hits
across all 18 source files — no auto-SKIPs.

Gameplay remains a prototype; nothing here claims console success without
evidence. Console in REST MODE, zero contact this pass.
