# 01 — Diagnosis: FindFirstOf("World") "returns nothing usable" in-world (Claude round-8 item 4)

Agent IA, 2026-10-10. Evidence: `outputs/2026-10-08-l25-prep/evidence/round4-spawn-session-20261011.decoded.log`
(hereafter "the log"). Code: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (frozen round-4 tree).

## VERDICT (one line)

**FindFirstOf("World") WORKS** — it returns a valid World wrapper in-world; the mod-facing failure is one
step later: **`World:GetName()` is unservable** — `GetName` is not a reflected UFunction anywhere in the
UWorld class chain AND no native `GetName` metatable binding exists (port or upstream UE4SS), so the
request falls into UFunction dispatch, misses, and returns the non-callable invalid wrapper, which makes
`pcall(w:GetName, w)` fail and the zzdiag [MAP] tracker print "?" forever. A second, separate finding:
the framework's LoadMap hooks ARE registered but their detour never arms in the current console world, so
no map-load event of any kind reaches mods (townfall's `RegisterLoadMapPreHook` callback is queued and
undelivered).

## The five candidate hypotheses, settled by the log

| # | Hypothesis | Verdict | Deciding evidence |
|---|------------|---------|-------------------|
| a | match predicate `Equals` fails (case/Number) | **NO** | log:849 `[FFO2] q='World' ... exact=2` — two live objects matched the walk's `GetNamePrivate().Equals(ClassName)` (UObjectGlobals.cpp:1094). `num=0` rules the Number variant out; Equals is index-only by design (UObjectGlobals.cpp:980-984). |
| b | the [FFO2] oracle's own view | **Scan ran, found** | log:849: `scanned=39549 exact=2 viaSuper=0 first=0x18005194C0 arm=wa[lk]` — the plain ForEachUObject walk (not the hash arm) found and accepted a World instance at `0x18005194C0`. `exact=2` with a Break-on-accept means the first exact match failed `IsValidObjectForFindXOf` (filtered) and the second was accepted and returned. |
| c | World objects outside scanned range | **NO** | same line: two World instances were inside the 39,549-object walk. |
| d | wrapper valid but downstream fails | **YES (the real bug)** | log:850-852: after the World query, `val(w)` (`w:IsValid()`, native binding LuaUObject.hpp:789) PASSED — the tracker then attempted `w:GetName()`: `[LUA-FN] in='GetName' ... OK` → `[UF] miss w=[n:29D61] c=[n:1E6C3] v=4 1=[3DF2E,3DF22,3DF17]` → `[UF] nofn [n:29D61] cls=[n:1E6C3]`. 0x29D61='GetName', 0x1E6C3='World' (the UWorld CLASS walked via `ReceiverAsStruct`, UObject.cpp:747). The chain walk visited only **4** UFunctions (v=4) — UObject exposes none of its native methods as UFunctions — so 'GetName' can never be found this way. |
| e | FName "World" not interned (id=0, silent walk) | **NO** | log:848-849: `[LUA-FN] in='World' len=5 id=0x1E6C3 decoded='World' OK` then `[FFO2] q='World' id=0x1E6C3` — interned, non-zero, round-trip decoded OK. No `[FFO] add-fallback`, `[FFO] query-fallback`, or `[FFO] query-embedded-NUL` marker fired anywhere in the log (the CP/DA/EV tripwires stayed silent — the constructed index was right). |

## The full causal chain (log lines + code citations)

1. zzdiag v11 [MAP] tracker (`zzdiag-v11/main.lua:9-16`): `pcall(FindFirstOf,"World")` → `w ~= nil` →
   `val(w)` = `w:IsValid()` → true → `pcall(function() return w:GetName() end)`.
2. log:848-849 — the query constructs FName 'World' (id=0x1E6C3, FNAME_Find via the Lua FName bridge,
   `[LUA-FN]`), `FindFirstOfVerified` round-trip matches (no fallback marker), the FName walk scans
   39,549 objects, 2 exact class matches, returns `first=0x18005194C0`. **The lookup is not nil.**
3. `w:IsValid()` passes (native binding; `is_object_in_global_unreal_object_map` + `IsUnreachable` both
   OK — LuaUObject.hpp:789-801).
4. `w:GetName()` → metatable `__index` (`prepare_to_handle`, LuaUObject.hpp:819-886): member name
   constructed FNAME_Find (`[LUA-FN] in='GetName'`), custom-property list miss, `FindProperty` miss →
   `handle_unreal_property_value` (LuaUObject.cpp:2596) with `field=nullptr` →
   `base->GetFunctionByNameInChain(property_name)` (LuaUObject.cpp:2609).
5. `GetFunctionByNameInChain` walks `TFieldRange<UFunction>(Struct, IncludeInterfaces|IncludeSuper)` on
   the UWorld class: 4 UFunctions visited (ids 0x3DF2E/0x3DF22/0x3DF17 + 1 more), none Equals 'GetName'
   → miss logged (`[UF] miss`, UObject.cpp:719) → nullptr.
6. Miss branch (LuaUObject.cpp:2621-2631): `log_function_miss` (`[UF] nofn`) then constructs the
   **special_invalid_ptr** wrapper "for compatibility reasons" (upstream behavior, kept).
7. Lua then tries to CALL that invalid wrapper userdata → "attempt to call a userdata value" → the
   tracker's pcall fails → `mn()` returns `"?"` (zzdiag-v11/main.lua:13 only returns s2 if o2) →
   `m ~= lm` is never true → **0 [MAP] lines** for the whole session. Same at T2 (log:866-867, markers
   deduped).

Why `GetName` specifically: the UObject metatable's native surface is GetAddress, IsValid, GetFullName,
GetFName, GetClass, GetOuter, IsAnyClass, Reflection, GetPropertyValue, SetPropertyValue, IsClass,
GetWorld, CallFunction, IsA, HasAllFlags/HasAnyFlags/HasAnyInternalFlags, ProcessConsoleExec, type
(LuaUObject.hpp:173-816). **No `GetName`** — and upstream UE4SS has none either (verified against
`.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss` and the
RE-UE4SS research tree: only LuaXProperty has a GetFullName, LuaTArray an IsValid). `GetName` is simply
not a reflected UFunction in stock UE (UObject's methods are native C++; the Blueprint "Get Object Name"
node lives on KismetSystemLibrary), so the UFunction dispatch can never serve it. This is an upstream
paper-cut that this port's [MAP] milestone tripped over; the fix (02-core-fix.patch) adds a native
`GetName` binding using the exact GetFullName-proven rendering route
(`GetNamePrivate().ToString()` → pool decoder → `LuaFNameBridge::push_wide_to_lua`).

## Corollary findings (load-map side)

- **LoadMap hooks ARE registered by the framework** (the task's question): LuaMod.cpp:8582-8584 registers
  `Unreal::Hook::RegisterLoadMapPreCallback/PostCallback` eagerly in W-works worlds; in the R3 marshal
  world the r3 gate (:8561-8574) instead queues a one-shot deferred arm
  (`try_deferred_loadmap_arm`, :5054-5072) once the marshal world is proven. The log proves the deferred
  arm **refused** in this world: two `[UE4SS.LoadMap.LuaModImpl] Failed to add hook, detour installation
  likely failed!` pairs + `[R3] LoadMap deferred arm refused (pre id 0, post id 0): queued Lua
  RegisterLoadMapPreHook/PostHook callbacks stay undelivered for this process lifetime` (log:867-869).
  townfall's own `pcall(RegisterLoadMapPreHook, ...)` (townfall-main.lua:731) queues into
  `m_load_map_pre_callbacks` and never fires.
- **NotifyOnNewObject's engine seam is also dead in this world**: the Lua API stores into
  `m_static_construct_object_lua_callbacks` (LuaMod.cpp:2972), delivered by
  `RegisterStaticConstructObjectPostCallback` (:8692) — which sits in the `else` branch the r3 gate skips,
  so it is not registered in the R3 world (declared in the r3 refusal warning itself, :8573).
- **UEHelpers.GetWorld does not use FindFirstOf("World") at all**: staged
  `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua:96-107` — it does `PlayerController:GetWorld()` /
  `GameInstance:GetWorld()`, i.e. the native `GetWorld` binding (LuaUObject.hpp:648-653) → engine virtual
  `UObject::GetWorld` (UObject.cpp:316, vtable wrapper). That chain is UNTESTED on console (zzdiag never
  calls it; PlayerController resolves valid per `pc=v`). Not touched by this fix — see limits.
- The GUObjectArray **delete**-listener infrastructure is live and engine-called in this world
  (log:853 `[L25][DeleteListener] engine called FShutdownDeleteListener::NotifyUObjectDeleted`), but the
  **create**-listener registration (`AddUObjectCreateListener`, UObjectArray.cpp:760, direct unlocked
  TArray append at the corrected +0x58 offset) only runs under `bUseUObjectArrayCache` (off in this
  session — FFO2 `arm=walk`, no "Initializing searcher pools" line) and has no Variant-C engine-op
  admission; registering a fresh create listener for the map logger would expand the unlocked-write risk
  family, so the logger (03-map-logger.patch) deliberately does not.

## What this means for the fix set

- 02-core-fix.patch: native `GetName` on the UObject metatable (Prospero-guarded) — makes the mod idiom
  `FindFirstOf("World"):GetName()` (and `Actor:GetName()` etc.) work; this is what un-breaks the [MAP]
  tracker class of mod code.
- 03-map-logger.patch: the runtime-owned `[FL-MAP] order=<n> map=<name> phase=pre|loaded` logger —
  `[FL-MAP]` lines inside the framework's own LoadMap pre/post delivery bodies (fire wherever the detour
  arms, incl. a future successful deferred arm) PLUS a bounded 1 Hz World-identity watch on the proven
  game-thread marshal pump, which is the only source that delivers in the current console world. This
  replaces mod-side polling with runtime markers, per Claude's directive.
