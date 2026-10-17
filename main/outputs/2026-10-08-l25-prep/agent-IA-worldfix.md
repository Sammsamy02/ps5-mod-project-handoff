# Agent IA — World lookup fix + runtime map logger (Claude round-8 item 4)

2026-10-10. Deliverables: `outputs/2026-10-08-l25-prep/round5-staging/world-fix/`
(01-diagnosis.md, 02-core-fix.patch, 03-map-logger.patch, 2 syntax receipts).
Target tree (frozen round-4 state, NOT edited): `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`.
Both patches dry-run `git apply --check` clean against that tree. Not wired into CHANGES — root stages
them for their own labelled gated build.

## VERDICT (Claude's premise corrected by evidence)

**FindFirstOf("World") does NOT return nil — it works.** The round-4 log's own oracle proves it
(`round4-spawn-session-20261011.decoded.log:849`): `[FFO2] q='World' id=0x1E6C3 num=0 scanned=39549
exact=2 viaSuper=0 first=0x18005194C0 arm=walk` — query FName interned and round-tripped (no
add-fallback/query-fallback/embedded-NUL marker fired anywhere), the walk found 2 exact class matches,
one accepted. `w:IsValid()` then PASSED (the tracker only attempted GetName because val(w) was true).

**The actual mod-facing bug is `World:GetName()`.** Log:850-852: the metatable has NO native `GetName`
binding (port or upstream UE4SS — verified both), so the request fell into UFunction dispatch
(`handle_unreal_property_value` → `GetFunctionByNameInChain`), which walked the UWorld class chain,
visited only 4 UFunctions (`[UF] miss w=[n:29D61] c=[n:1E6C3] v=4`), missed ('GetName' is not a
reflected UFunction anywhere — UObject's methods are native C++), logged `[UF] nofn`, and returned the
non-callable special_invalid_ptr wrapper → Lua "attempt to call a userdata value" → zzdiag's `mn()`
returned "?" every cycle → **0 [MAP] lines**. So candidates (a),(b),(c),(e) are disproven; (d) is the
bug, one step downstream of where Claude's framing put it. Full chain with citations: 01-diagnosis.md.

Corollaries: the framework DOES already register LoadMap hooks (LuaMod.cpp:8582-8584 eager /
`try_deferred_loadmap_arm` :5054 one-shot), but in the current console world the detour arm REFUSED
(log:867-869, pre id 0 / post id 0) and the StaticConstructObject post registration (the seam behind Lua
NotifyOnNewObject) is R3-deferred — so townfall's `RegisterLoadMapPreHook` callback (main.lua:731) is
queued and never delivers. UEHelpers.GetWorld doesn't use FindFirstOf at all — it calls
`PlayerController:GetWorld()`, a native binding that routes the engine virtual (untested on console).

## What each patch changes

**02-core-fix.patch** — `include/LuaType/LuaUObject.hpp` (+35 lines, `__PROSPERO__`-guarded): adds a
native `GetName` metatable binding that mirrors the GetFullName binding 20 lines above it exactly
(`GetNamePrivate().ToString()` → pool decoder → `LuaFNameBridge::push_wide_to_lua`; nil for nullptr
receiver, same contract). Makes `FindFirstOf("World"):GetName()` and `Actor:GetName()` work for mods;
host builds keep upstream parity (binding compiled out).

**03-map-logger.patch** — `src/LuaMod.cpp` (+~200 lines, `__PROSPERO__`-guarded): the runtime-owned
`[FL-MAP] order=<n> map=<name> phase=pre|loaded` milestone logger, replacing mod-side polling:
1. `[FL-MAP]` lines inside the framework's own `lua_load_map_pre_callback` (phase=pre, map=URL.Map via
   the proven `fstring_to_narrow_string` family) and `lua_load_map_post_callback` (phase=loaded,
   World->GetName()) — fire wherever the LoadMap detour arms (W-works worlds, or a future successful
   deferred arm).
2. `FlMap::marshal_watch()` — the only source that delivers in the current console world: a 1 Hz-bounded
   World-identity check inside `funcptr_marshal_hook` (the proven game-thread pump, called after the
   deferred-arm attempt), resolving the World via the FFO2-proven `FindFirstOf("World")` walk and
   logging every NEW World instance phase=loaded src=marshal-watch. One order number per load; pre/post
   share it; detour-post and the watch share the last-world pointer so an armed detour is never
   double-counted. Claude's NotifyOnNewObject(World) intent is served this way in the R3 world; a fresh
   FUObjectCreateListener was deliberately NOT registered (unlocked TArray-append family, no Variant-C
   admission — out of risk scope; reasoning in the in-code comment and 01-diagnosis.md).

## Syntax checks (established Prospero recipe, staged copies in /tmp, live tree untouched)

- `prospero-clang++`, flags verbatim from `2026-10-08-l26-build-variants/l26-A/pass1/objects/
  luamod.command.json` (`-c/-o/-MD/-MF` → `-fsyntax-only`), staged TU + `-I` shadow dir first.
- Staged `LuaMod.cpp` (03 edit + 02 header edit via shadow): **RC=0, 0 errors**, 23 warnings — all
  pre-existing families at untouched lines (deprecated Register*Callback x11, operator unsigned int x6,
  UnrealType/Atomic third-party x5, u8path x1). Receipt: `syntax-receipt-LuaMod-both-edits.log`.
- `LuaUObject.cpp` (live TU) against the edited header: **RC=0, 0 errors**. Receipt:
  `syntax-receipt-LuaUObject-header.log`.

## Honest limits — only the next launch can confirm

- The `GetName` binding's runtime behavior (binding hit before property/UFunction dispatch, correct
  decoded string on console for a live World) — the rendering family is GetFullName/FFO2-proven but the
  binding itself is new.
- Whether `[FL-MAP]` marshal-watch lines actually appear: the watch runs only after identity
  publication + dispatch ownership on the marshal pump; if the carrier stalls this launch as before,
  no watch lines. Watch granularity is 1 s (map-load ordering is exact; sub-second phase timing is not).
- phase=pre remains unavailable in the current world (detour refused); only a future armed world gets
  pre lines. The watch cannot see same-address World reuse (seamless travel) or a GC-reused address
  (rare; would log once or falsely once, respectively) — pointer identity only.
- UEHelpers.GetWorld's `PlayerController:GetWorld()` → engine-virtual chain remains untested on console
  (out of scope here; the vtable-wrapper route is the risk, not FindFirstOf).
- W-works-world code paths (eager detour arm, pre/post markers) compile but did not execute here.
