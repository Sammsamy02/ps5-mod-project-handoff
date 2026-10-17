# API PARITY TABLE — PS5 UE4SS port (L26-R4) vs upstream UE4SS

Agent FS, 2026-10-08. ANALYSIS ONLY (Mac on battery; read/grep only, zero console
contact, zero tree edits except this file). Companion to
`outputs/2026-10-08-l25-prep/mod-ladder/MANIFEST.md` before the 12-mod ladder.

## Sources & method

- **Port (composed console runtime)** = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`
  (LuaMod.cpp sha256 `c60debc1…`, 9,509 lines) **union** the units in the link
  receipt `outputs/2026-10-08-l24-build-r2/pass1/LINK-COMMAND.json` (374 ordered
  selected units + 391 full link inputs; LuaType files from
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/`).
- **Upstream** = vendored
  `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/`
  (`src/Mod/LuaMod.cpp` 7,331 lines, `src/LuaType/*`, `deps/first/LuaMadeSimple`,
  `deps/first/LuaRaw`).
- Method: full extraction of `register_function("…")`, `make_global("…")`,
  `lua_setglobal`, `add_pair("…")` per file in both trees, then per-name diff.
  **Result: the global-name set and every type member-name set are IDENTICAL
  between port and upstream** (add_pair multiset diff of LuaMod.cpp = empty;
  LuaModRef identical; all 15 checked LuaType files identical). Differences are
  in *behavior, thread/pump plumbing, and console-specific guards* — detailed
  below.

**Line-number caveat vs agent-FO**: FO's binding cites (agent-FO-mod-ladder.md
"binding corrections": FindObject :3956, FindObjects :4128,
RegisterConsoleCommandHandler :3865, LoadAsset :3906) are from an earlier
revision; in the current candidate they are uniformly **+64 lines** (FindObject
:4020, FindObjects :4192, RegisterConsoleCommandHandler :3929, LoadAsset :3970).
Names remain correct. All cites below are the CURRENT candidate.
`MANIFEST.md`'s RegisterHook ":1182" is likewise stale → current :5705.

Legend: **BOUND** = registered in the composed runtime (file:line given, port
then upstream). **BEHAVIOR-DIFFERS** = bound but port behavior deviates.
**UNEXERCISED** = bound, never console-fired (MANIFEST grade (b)/(c1)).
**PORT-ONLY** = superset addition not in upstream.

---

## 1. GLOBALS

### 1a. Functions (register_function — mod main state unless noted)

| Global | Port (candidate LuaMod.cpp) | Upstream (src/Mod/LuaMod.cpp) | Status |
|---|---|---|---|
| RegisterHook | :5705 (main state); hook-thread copies :1246/:1272/:1298 (inside commented block, same as upstream) | :4199; :803/:829/:855 (commented) | BOUND — UNEXERCISED (MANIFEST c1, P6 first) |
| NotifyOnNewObject | :2918 (globals) + :1251/:1277/:1303 (commented hook-thread) | :2303 + :808/:834/:860 | BOUND — UNEXERCISED (RUN-6/7) |
| UnregisterHook | :2324 | :1747 | BOUND — UNEXERCISED (ConsoleEnabler L79-87 pattern) |
| print | :1751 (`LuaLibrary::global_print`) | :1236 | BEHAVIOR-DIFFERS (payload routed through LuaFNameBridge widen — candidate/src/LuaLibrary.cpp:56-60; upstream uses Unicode::DecodeUtf8; net behavior equal) |
| LoadExport | :1752 | :1237 | BOUND |
| CreateInvalidObject | :1754 | :1239 | BOUND |
| StaticFindObject | :1759 | :1244 | BOUND — PROVEN (spawner long+short names) |
| FindFirstOf | :1870 | :1347 | BOUND — PROVEN |
| FindAllOf | :1921 | :1382 | BOUND — PROVEN |
| IsKeyBindRegistered | :2002 | :1450 | BOUND — UNEXERCISED |
| RegisterKeyBindAsync | :2064 | :1512 | BOUND — UNEXERCISED |
| RegisterKeyBind | :2182 | :1630 | BOUND — PROVEN; BEHAVIOR-DIFFERS: input source is PS5Pad.ini chord mapping, not PC keys (MANIFEST pad-chord budget law; SplitScreen CTRL chords have no row) |
| DumpAllObjects | :2420 | :1840 | BOUND — backends linked (ObjectDumper/ObjectToString.cpp in receipt); console function untested. **Corrects FO: present, not absent** |
| GenerateSDK | :2431 | :1851 | BOUND — SDKGenerator/Generator.cpp+2 units linked; untested |
| GenerateLuaTypes | :2442 | :1862 | BOUND — untested |
| GenerateUHTCompatibleHeaders | :2453 | :1873 | BOUND — **corrects FO "absent"** |
| DumpStaticMeshes | :2459 | :1879 | BOUND (GUI/DataDumpers.cpp linked) — **corrects FO** |
| DumpAllActors | :2464 | :1884 | BOUND — **corrects FO** |
| DumpUSMAP | :2469 | :1889 | BOUND (USMapGenerator/Generator.cpp linked) — **corrects FO** |
| DumpJMAP | :2479 | :1899 | BOUND (native-jmap-port-1 JMapGenerator.cpp linked) — **corrects FO** |
| StaticConstructObject | :2495 | :1915 | BOUND — PROVEN (2/3/10+ arg forms used by ladder mods) |
| RegisterCustomProperty | :2630 | :2025 | BOUND — ActorDumper uses it with hardcoded 0x98 (SKIP per MANIFEST, offset law) |
| ForEachUObject | :2897 | :2282 | BOUND — UNEXERCISED (ModMenu :1515) |
| RegisterCustomEvent / UnregisterCustomEvent | :2986 / :3029 | :2368 / :2408 | BOUND — UNEXERCISED (BPML LOAD-ONLY) |
| RegisterLoadMapPreHook / PostHook | :3048 / :3075 | :2426 / :2453 | BOUND — port queues callbacks, arms only when marshal world live (LuaMod.cpp:8573 note); BEHAVIOR-DIFFERS (soft-refused until proven live) |
| RegisterInitGameStatePre/Post, BeginPlayPre/Post, EndPlayPre/Post (6) | :3102-:3242 | :2480-:2620 | BOUND — BEHAVIOR-DIFFERS: eager detour arms soft-refused in R3-marshal worlds (LuaMod.cpp:8573: stay unregistered this process lifetime, loud refusal) |
| IterateGameDirectories | :3270 | :2648 | BOUND — BEHAVIOR-DIFFERS/UNVERIFIED: needs root-dir knowledge; on PS5 likely warns "Could not locate the root directory" (only BPModLoader, already SKIP) |
| CreateLogicModsDirectory | :3604 | :2982 | BOUND — untested; only BPML (SKIP) |
| ExecuteAsync | :3675 | :3053 | BOUND — UNEXERCISED (ModMenu fallback path) |
| ExecuteWithDelay | :3696 | :3074 | BOUND — UNEXERCISED |
| LoopAsync | :3727 | :3105 | BOUND — PROVEN (spawner) |
| RegisterProcessConsoleExecPre/PostHook | :3759 / :3780 | :3137 / :3158 | BOUND — same detour-refusal caveat as above |
| RegisterCallFunctionByNameWithArgumentsPre/PostHook | :3801 / :3822 | :3179 / :3200 | BOUND — same caveat |
| RegisterULocalPlayerExecPre/PostHook | :3843 / :3864 | :3221 / :3242 | BOUND — same caveat |
| RegisterConsoleCommandGlobalHandler | :3885 | :3263 | BOUND — UNEXERCISED |
| RegisterConsoleCommandHandler | :3929 | :3298 | BOUND — UNEXERCISED (RUN-4; FO's :3865 stale) |
| LoadAsset | :3970 | :3333 | BOUND — UNEXERCISED (ConsoleCommands summon:5) |
| FindObject | :4020 | :3381 | BOUND — UNEXERCISED (set.lua:18; FO's :3956 stale) |
| FindObjects | :4192 | :3546 | BOUND — UNEXERCISED (set.lua:28; FO's :4128 stale) |
| GetCurrentThreadId / GetMainModThreadId / GetAsyncThreadId / GetGameThreadId | :4355 / :4366 / :4378 / :4390 | :3705-:3740 | BOUND |
| IsInMainModThread / IsInAsyncThread / IsInGameThread | :4401 / :4413 / :4425 | :3751-:3775 | BOUND |
| ExecuteInGameThread | :5836 | :4324 | BOUND — PROVEN (game-thread dispatch; pump law notes :6067/:6071) |
| ExecuteInGameThreadWithDelay | :6093 | :4407 | BOUND — UNEXERCISED (P7) |
| RetriggerableExecuteInGameThreadWithDelay | :6310 | :4553 | BOUND — UNEXERCISED |
| ExecuteInGameThreadAfterFrames | :6488 | :4660 | BOUND — UNEXERCISED |
| LoopInGameThreadWithDelay | :6564 | :4715 | BOUND — UNEXERCISED (MiniMap 50 ms tick :4268; P7) |
| LoopInGameThreadAfterFrames | :6715 | :4795 | BOUND — UNEXERCISED |
| Delayed-action handle family: ResetDelayedActionTimer :6792, SetDelayedActionTimer :6856, PauseDelayedAction :6926, UnpauseDelayedAction :7005, CancelDelayedAction :7055, IsValidDelayedActionHandle :7102, IsDelayedActionActive :7144, IsDelayedActionPaused :7186, GetDelayedActionTimeRemaining :7228, GetDelayedActionTimeElapsed :7311, GetDelayedActionRate :7396, ClearAllDelayedActions :7454, MakeActionHandle :7491 | upstream :4851-:5550 | BOUND — UNEXERCISED |
| RestartCurrentMod :7502 / UninstallCurrentMod :7516 / RestartMod :7531 / UninstallMod :7548 | upstream :5561-:5607 | BOUND — BEHAVIOR-DIFFERS: hot-reload unavailable on PS5 (CLAUDE-SUMMARY-20261009-EVENING.md:62) |
| FString :7710 / FUtf8String :7746 / FAnsiString :7761 (ctors) | upstream :5769/:5784/:5799 | BOUND — FString ctor PROVEN (SetText path) |

### 1b. Tables / class globals (make_global, lua_setglobal)

| Global | Port | Upstream | Status |
|---|---|---|---|
| Key | LuaMod.cpp:839 (key_table) | :606 | BOUND — key VALUES feed PS5Pad.ini chord map (behavior differs at input layer only) |
| ModifierKey | :851 | :618 | BOUND (same caveat) |
| EObjectFlags | :897 | :664 | BOUND — **corrects FO: NOT absent** (set.lua:11 resolves) |
| EInternalObjectFlags | :917 | :684 | BOUND (SplitScreen PendingKill :25) |
| EFindName | :927 | :694 | BOUND — UNEXERCISED FNAME_Find roundtrip (P3) |
| PropertyTypes | :1347 | :904 | BOUND |
| UE4SS (GetVersion) | :7618 | :5677 | BOUND (member set identical) |
| UnrealVersion (GetMajor/GetMinor/IsEqual/IsAtLeast/IsAtMost/IsBelow/IsAbove) | :7689 | :5748 | BOUND (LineTraceMod IsBelow L26/28 — (b) grade, pure table) |
| FName class table + NAME_None | :7696/:7698 | :5755/:5757 | BOUND — PROVEN (ctor + FName returns) |
| FText class table | :7705 | :5764 | BOUND — PROVEN (SetText(FText)) |
| FPackageName (IsValidLongPackageName :7803, IsShortPackageName, …) | :7827 | :5850 | BOUND (member set identical; Prospero bridge widening :7814-7821) |
| ModRef | :7848 (LuaType::LuaModRef::construct) | :5871 | BOUND (member set identical: GetSharedVariable/SetSharedVariable/type) |
| __OriginalReturnValue | :7858 | :5881 | BOUND (hook return-value contract) |
| EGameThreadMethod / EngineTickAvailable / ProcessEventAvailable | :5823/:5831/:5833 | :4311/:4319/:4321 | BOUND |
| MainThread / AsyncThread | :1293 / :1267 (lua_newthread of mod state) | :850 / :824 | BOUND (threads share globals — one io override per mod covers all) |
| ue4ss_loaded_modules | :1356 | :913 | BOUND |
| loadfile / dofile (native-source overrides) | :1400-:1407 via luaL_Reg loop | — (upstream uses stdlib) | **PORT-ONLY** — read through NativeModSource logical keys (:989-1026), not disk |

NOT present in either tree (task checklist items): `GetUnrealVersion` (it is the
`UnrealVersion` table), `GetEngine` (a UEHelpers.lua pure-Lua helper,
UEHelpers.lua:38-43 — not a C++ global), `SetValidInvalidCallbacks` (absent from
upstream too — vendored version predates it), `SpawnActor` global (it is a
**UWorld member**: port LuaUWorld.cpp:62 = upstream :62, identical line).

### 1c. Raw-test globals (upstream Signatures.cpp)

Upstream also registers `Print/print/DerefToInt32/dereftoint32/LoadExport/loadexport`
on the raw test lua state (src/Signatures.cpp:43-48). The port's composition has
no Signatures.cpp unit; equivalent surface (print/LoadExport) is on the mod
state via LuaMod.cpp:1751-1752. No mod impact (debug-only surface).

---

## 2. TYPES

### 2a. UObject userdata (LuaUObject.hpp member diff = IDENTICAL)

Members (port candidate include/LuaType/LuaUObject.hpp = upstream
include/LuaType/LuaUObject.hpp): `IsValid` :186/:789, `GetAddress` :173,
`GetFullName` :533 (PROVEN), `GetFName` :556 (→ construct_fname :363),
`GetClass` :561, `GetOuter` :566, `GetWorld` :648, `IsA` :659, `IsAnyClass`,
`IsClass` :638 (**corrects FO — bound**), `HasAllFlags` :663, `HasAnyFlags`
:687, `HasAnyInternalFlags` :711, `CallFunction` :655, `ProcessConsoleExec`
:735, `GetPropertyValue` :628, `SetPropertyValue` :633, `Reflection` :582 with
nested `GetProperty` :591 (**corrects FO — bound, not a global**), `Get`/`get`,
`Set`/`set`, `type` (RemoteUnrealParam pair), plus RemoteObject-base
`IsValid`/`IsValidField`/`GetAddress` (upstream deps/first/LuaMadeSimple
include/LuaMadeSimple/LuaObject.hpp:160-198). `GetName` is NOT a registered
member in either tree — mods get it as a reflected UFunction call (works through
the same dispatcher below).

**K2_/reflected call dispatch**: identical ProcessEvent route — port candidate
src/LuaUObject.cpp:365 / coherent port src/LuaType/LuaUObject.cpp:327
`calling_context->ProcessEvent(func, dynamic_unreal_function_data.data)`
= upstream src/LuaType/LuaUObject.cpp:257. PROVEN on console (spawner
K2_GetActorLocation FVector return :233, BeginDeferred/FinishSpawning
FTransform :295/:303, UMG WidgetBlueprintLibrary:Create :510, FName return
:163). Neither tree exposes a Lua-level `FindFunction`/`bpfn` global or member
(grep clean both trees; the MANIFEST crash-law note concerns mod-side usage
patterns, and no ladder mod uses them).

### 2b. Property __index/__newindex semantics (incl. the parity revert)

- `__index`/`__newindex` route through `prepare_to_handle` (port
  LuaUObject.hpp:799-860; upstream :795-839): custom-property list → class
  FindProperty → `handle_unreal_property_value`. Port differences are confined
  to the member-name conversion: ONE hand UTF-8→UTF-16 converter
  (`LuaFNameBridge::Bridge`) and the **l26 postmortem revert** to upstream
  `FNAME_Find` for member names on ALL platforms (LuaUObject.hpp:844-853;
  upstream :832 `Unreal::FName(member_name, Unreal::FNAME_Find)`). FindXOf keeps
  its separate Find-first fix (UObjectGlobals.cpp).
- **Parity revert (Claude PRIORITY-1, 2026-10-10)** — property READS return
  USERDATA like upstream, conversion lives inside the userdata surface:
  - FName: Get → `LuaType::FName::construct` (port src/LuaUObject.cpp:1910 =
    upstream :1679 verbatim); port ADDS a Prospero-only plain-string SET branch
    (:1922-1932) — a strict superset of upstream's userdata-only Set (:1682).
  - FText: Get → construct (port :1969 = upstream :1709); port adds
    plain-string Set (:1980-1986) over upstream userdata-only (:1712).
  - FString: Get → construct (port :2027 = upstream :1739); Set accepts
    string+userdata in both (upstream :1742-1756; port adds bridged converter
    + loud refusal :2043-2054).
  - FUtf8String/FAnsiString Get/Set identical (:2090/:2096 ff.).

### 2c. FName / FText / FString (member diffs IDENTICAL)

- FName: `ToString` port src/LuaFName.cpp:169 (upstream :119), `Equals` :192
  (upstream :135), `GetComparisonIndex`, `type`. Metamethods: `__call` ctor
  (port :82, upstream :48 — slot-2 ctor direct + bounded buffer, upstream
  semantics incl. empty-string→NAME_None), `__eq` (port :153, upstream :103).
  No `__lt`/`__le` in either.
- FText: `ToString` port src/LuaFText.cpp:223 (upstream :92), `type`;
  `__call` ctor :102/:47, `__eq` :177/:76.
- FString/FUtf8String/FAnsiString (shared template LuaUnrealString.hpp):
  `Append, Clear, Empty, EndsWith, Find, IsEmpty, Len, StartsWith, ToLower,
  ToString, ToUpper, type` — name sets identical to upstream LuaFString.hpp.

### 2d. Containers, structs, enums, other types (all member diffs IDENTICAL)

- TArray: `Empty, ForEach, GetArrayAddress, GetArrayDataAddress, GetArrayMax,
  GetArrayNum, IsValid, type` (coherent port LuaTArray.cpp; upstream same list).
  TMap: `Add, Contains, Empty, Find, ForEach, IsValid, Remove, type`. TSet:
  `Add, Contains, Empty, ForEach, IsValid, Remove, type`.
- UScriptStruct: `GetBaseAddress, GetProperty, GetPropertyAddress,
  GetStructAddress, IsMappedToObject, IsMappedToProperty, IsValid, type`
  (candidate src/LuaUScriptStruct.cpp = upstream list). Struct-OUT params
  (HitResult table OUT) UNEXERCISED — P5 probe step.
- UEnum / UFunction / UClass / UWorld (SpawnActor :62 both trees) / UDataTable
  / UStruct / AActor / UInterface / FWeakObjectPtr (weak `.Get()` = LineTraceMod
  P5) / FOutputDevice (`Log` — ConsoleCommands GlobalAr:Log, binding present,
  UNEXERCISED) / FURL / FSoftObjectPath / TSoftObjectPtr / ThreadId /
  LuaCustomProperty: member sets identical (15/15 files diffed clean).
- Enum access + `EObjectFlags`/`EInternalObjectFlags`/`EFindName` tables: bound
  (§1b).

---

## 3. METAMETHODS

| Metamethod | Port | Upstream | Status |
|---|---|---|---|
| UObject `__index`/`__newindex` | LuaUObject.hpp:799-860 (bridge + FNAME_Find revert) | :787-839 | BEHAVIOR-DIFFERS only in name-string conversion path; semantics identical |
| UObject `__eq` | uobject_equal_implementation (LuaUObject.hpp:510) | :413/:504 | BOUND |
| UObject `__call` (guard) | LuaUObject.hpp:517-525 | :519-527 | BOUND |
| FName `__call`/`__eq` | LuaFName.cpp:82/:153 | :48/:103 | BOUND (ctor internals bridged) |
| FText `__call`/`__eq` | LuaFText.cpp:102/:177 | :47/:76 | BOUND |
| **FName `__tostring`** | LuaFName.cpp:28-60 (raw C API attach on "FNameUserdata" registry metatable; payload = pool decoder → bridge UTF-16→UTF-8; same as :ToString) | **NONE — upstream keeps Lua's default "FName: 0x…" stringification** | **PORT-ONLY EXTENSION** — deliberate superset, required by MiniMap `str()`/ModMenu tostring() call sites; not an upstream-parity item (upstream mods never rely on the default hex form) |
| **FText `__tostring`** | LuaFText.cpp:52-76 (agent-FL) | NONE (default) | PORT-ONLY EXTENSION (same rationale) |
| **FString/FUtf8String/FAnsiString `__tostring`** | LuaUnrealString.hpp:44-104 (agent-FW; FString via push_fstring_to_lua, Utf8/Ansi push bytes) | NONE (default "FStringUserdata: 0x…") | PORT-ONLY EXTENSION |

Upstream-parity answer for the task's question: the bridged `__tostring`
metamethods are NOT upstream behavior — upstream UE4SS leaves userdata
stringification to Lua's default. The port's additions are safe supersets
(identical payloads to the `:ToString` members) and close the MiniMap
`str()`-on-FName gap. `__tostring` on UObject userdata remains default in both.

---

## 4. STANDARD LIBS

**Init site (exact)**: `LuaMod::prepare_mod` — port candidate src/LuaMod.cpp:7840
`lua.open_all_libs()` (= upstream src/Mod/LuaMod.cpp:5863), inside start_mod
(port :7987 = upstream :6008). `open_all_libs` → `luaL_openlibs` (port
LuaMadeSimple.cpp:681-684 in native-coherent-runtime-1/source/port/first;
upstream deps/first/LuaMadeSimple/src/LuaMadeSimple.cpp:680-683). linit.c
loadedlibs (vendored LuaRaw src/linit.c:42-54): base, package, coroutine,
table, **io**, **os**, string, math, utf8, debug. The console-executor state
gets the same call (port :8277 = upstream :6276).

**Link evidence (L26-R4 composition, LINK-COMMAND.json)**: ordered link inputs
include `coherent-full-1/luaraw-{linit,liolib,loslib,ldblib,loadlib,
lbaselib,lstrlib,lmathlib,ltablib,lcorolib,lutf8lib}.o` — i.e. **the io library
IS structurally linked**, and `luaraw-liolib.o` is the PATCHED full
fopen-backed implementation from
`.superpowers/native-dependency-spike/ue4ss-stdio-consumer-adaptation-1/patched/liolib.c`
(only Sony-macro-safe swaps: getc→fgetc, (clearerr)(f), (ferror)(f) — diff vs
vanilla liolib.c is 4 lines; io.open/lines/read/write all real fopen).
**os** (loslib.c: clock/date/time/execute/getenv/remove/rename) and **debug**
(ldblib.c: getinfo & co.) also linked. `system()` reachability on PS5 for
os.execute is unverified.

**Port-specific divergences that matter**:
1. Native-source mode (the modbridge route every PS5 mod takes): `package.path`
   and `cpath` are BLANKED (LuaMod.cpp:1589-1592); `package.searchers` replaced
   by [preload, native buffer searcher] (:1376-1389); `loadfile`/`dofile`
   replaced (:1400-1407) reading via `Platform::NativeModSource::read` — an
   in-memory immutable snapshot keyed by logical "ue4ss/…" paths
   (include/Platform/NativeModSource.hpp:29-44), populated by the modbridge
   activation (bootstrap/activated_mod_source.hpp:22-23, mfa_activation).
   `require("UEHelpers")` works through this (proven by the spawner boot).
2. **The "KNOWN: no io library" premise is NOT what the current tree shows.**
   The only in-tree console claim is CLAUDE-SUMMARY-20261009-EVENING.md:62
   ("no io lib in the sandbox", hot-reload context) with no raw artifact
   (grep of harvest logs finds no `global 'io'` error). Structurally, L26-R4
   opens a real io. What WILL fail today is not `type(io)` but the PATHS mods
   compute: mod files are addressed by NativeModSource logical keys and chunk
   names are "@<logical>/main.lua" (LuaMod.cpp:1009), so e.g. MiniMap's
   `debug.getinfo(1,"S").source`-derived settingsPath (minimap main.lua
   :241-245: regex over the chunk name, then `dir .. "\\settings.txt"`) yields a
   logical/relative Windows-separator path that fopen cannot resolve → read
   returns nil (non-fatal), write `assert` throws at save. This is the gap the
   io shim must close (§5).
3. `print` is overridden (LuaLibrary.cpp) with widened payload handling;
   io.write/io.read (stdin/stdout) route to the process stdout → UE4SS log/klog
   redirect — unverified, no ladder mod uses them.

**Upstream comparison**: upstream does NOT sandbox io at all — same
`luaL_openlibs`, real disk paths, package.path extended with mod Scripts +
Mods/shared (upstream LuaMod.cpp:1082-1131 setup_lua_require_paths; port keeps
the same function :1580-1644 for non-native mode but blanks it in native mode).

---

## 5. IO SHIM PLAN (design only — no code, no tree edits)

**Goal**: make `io.open`/`io.lines` read AND write work for a mod's OWN folder
only, and make logical (NativeModSource) paths resolvable, without touching any
mod file.

**Root prefix (per mod)**: `/data/modbridge/roots/PPSA33286/ue4ss/Mods/<ModName>/`
— derivable at registration time from the LuaMod instance (m_mod_name; native
mode: `NativeLuaMod.path/scripts_path`, LuaMod.cpp:1393-1399 precedent) —
carried as a light-userdata upvalue exactly like the existing
`custom_module_searcher`/`loadfile` closures (:1386-1388, :1403-1405).

**Registration site**: new `LuaMod::setup_lua_io_shim(const LuaMadeSimple::Lua&)`
called in `prepare_mod` immediately after `setup_lua_require_paths`
(LuaMod.cpp:7841-7842) — i.e. after `open_all_libs` (:7840) so the shim can
replace the stdlib `io` table (`lua_setglobal(L, "io")`), keeping `io.stdout`
passthrough by copying it from the original table first. Main/async/hook
"states" are `lua_newthread`s of the mod state (:1263/:1289) — globals shared,
one registration per mod suffices. Optionally repeat for the
`LuaStatics::console_executor` state (:8277).

**Path resolver (every entry point)**:
1. Accept the mod-supplied path; map `\` → `/`; strip a leading `@` (chunk-name
   leak) and any drive-letter prefix.
2. If it starts with the logical key `ue4ss/Mods/<ModName>/`, strip to the
   remainder (this is what MiniMap's settingsPath produces); any other absolute
   path must already be under the physical root.
3. Reject `..` components, NUL, empty. Join root + remainder; re-verify the
   canonical result still begins with `root + "/"` (prefix enforcement
   post-join — defeats `..`/symlink-style escapes that survive joining).

**Surface (minimal)**: `io.open(path, mode)` for r/rb/w/wb/a/ab returning a
file userdata with `:read("l"/"a"/"n")`, `:lines()`, `:write(s)`, `:close()`,
`__gc`-close; `io.lines(path)` sugar; `io.exists(path)` → boolean (explicit
non-standard addition, clearly a port extension). NOT provided: `io.popen`,
`io.tmpfile`, update modes (`r+`/`w+`), `io.input`/`io.output` redirection.
**Dual read route**: on open-for-read, try physical fopen first; on ENOENT, if
the logical key is retained in the NativeModSource snapshot (Scripts/main.lua
etc.), serve the snapshot bytes read-only (NativeFileBuffer views,
NativeModSource.hpp:12-16) — reads then work even if the physical tree is not
fopen-able from the game process, while writes always target the physical
folder. This mirrors how `native_lua_loadfile` already prefers the snapshot
(:989-1026).

**Sandbox rules**: prefix enforcement post-canonicalization (above); write
modes confined to the mod folder; no `os.execute` passthrough (verify `system()`
linkage first; if present on PS5, replace `os.execute` with an error-returning
stub at the same registration site); `os.remove`/`os.rename` either left as-is
(no RUN-list mod uses them) or gated through the same resolver (mod-folder
only). jsbProfi's os.clock/os.date need nothing (loslib linked, no syscalls).

**Acceptance evidence**: one zzdiag/ladder marker set — `[io] exists`, open
scratch file under the mod folder, write+readback roundtrip, lines iteration,
and a NEGATIVE test (path outside prefix rejected with error/nil). RUN-6
acceptance adds MiniMap settings round-trip (loadSettings :251 read,
saveSettings :293 write) — without the shim that save is the documented S5
failure owner (MANIFEST RUN-6 note).

**Impact on the 12 mods** (cross-checked vs MANIFEST + UEHelpers.lua grep = 0
io hits):
- **TownfallMiniMap (RUN-6)** — the ONLY RUN/LOAD mod that needs io
  (:251 read pcall-tolerant, :293 write assert-throws). Shim = full settings
  function.
- **BPModLoader (SKIP — unchanged)**: io.open rb :29 + io.lines :44 would now
  read, but pak mounting stays out of scope; verdict remains SKIP.
- **jsbLuaProfiler (SKIP — unchanged)**: jsbProfi.lua io.open :71, os.clock/
  os.date :95-122, debug.getinfo :146 — shim would enable it but profiler risk
  verdict stands.
- Other 9 mods + resident UEHelpers: zero io usage — no behavior change; shim
  is invisible to them.

---

## 6. VERDICT — top gaps by mod impact (ordered)

1. **Hook family unexercised** — RegisterHook/UnregisterHook/NotifyOnNewObject
   (+`__OriginalReturnValue` contract). Gates 4 of 7 RUN mods (RUN-2/3/6/7);
   ModMenu has UNGUARDED RegisterHook sites (:1234/:1250/:1539+) where an
   install-time failure errors the whole script. P6 must pass first. Bound:
   :5705/:2324/:2918.
2. **Property __newindex Set family unexercised** — object-ref Set
   (CheatManager = … RUN-2), bool Set (RUN-5), array-element struct-field Set +
   FName roundtrip (ConsoleEnabler RUN-3). Reads are proven; Sets are not.
3. **Delay/loop game-thread family unexercised** — ExecuteInGameThreadWithDelay
   :6093, LoopInGameThreadWithDelay :6564 (+ handle API). MiniMap's whole tick
   (:4268) and ModMenu's debounce paths depend on them. P7.
4. **io path identity (not io absence)** — io is structurally linked on L26-R4
   (patched liolib + vanilla linit listing io + luaL_openlibs at :7840), but
   mod-computed paths are NativeModSource-logical, so MiniMap RUN-6 settings
   read/write fail at the path layer. Ship the §5 shim; treat the "no io lib"
   console note as stale-until-P0 (`type(io)` + roundtrip probe already in the
   ladder's P0 env report).
5. **Struct-OUT params + weak `.Get()`** — LineTraceSingle HitResult table OUT
   (RUN-1) and K2_SetActorLocationAndRotation HitResult OUT (RUN-5); P5 probe
   step is exactly this.
6. **Detour-arm soft-refusals** — InitGameState/BeginPlay/EndPlay/
   ProcessConsoleExec/ULocalPlayerExec/CallFunctionByNameWithArguments hooks
   stay unregistered for the process lifetime in R3-marshal worlds
   (LuaMod.cpp:8573). No RUN mod currently needs them (LoadMap hooks queue);
   revisit only if a ladder mod's hook never fires.
7. **RegisterKeyBind input layer** — chords must exist in PS5Pad.ini
   (SplitScreen CTRL chords have none; ModMenu nav binds co-fire with the
   spawner — spawner-free boot law).
8. **MANIFEST corrections from this pass (re-grade, no new work)**:
   Reflection/:GetProperty (LuaUObject.hpp:582/:591), IsClass (:638),
   ImportText/ContainerPtrToValuePtr (coherent LuaXProperty.cpp:215/:186),
   EObjectFlags (:897), and all five "absent" Dump/Generate globals
   (:2442-:2492) ARE bound on L26-R4 → ConsoleCommandsMod `set` deep path moves
   (c2) binding-gap → (c1) probe-first; Keybinds' five "missing" globals are
   present (function-on-console still untested). FO's file:line cites are one
   revision (-64 lines) stale; names were right.
9. **os.execute/system() reachability** — linked via loslib but unverified on
   PS5; no RUN mod uses it; stub it in the io-shim registration if present.
10. **IterateGameDirectories root-dir knowledge** — BPModLoader-only (already
    SKIP); zero RUN impact.

**Bottom line**: the port's Lua API surface is name-for-name complete vs the
vendored upstream (globals, type members, metamethods; plus three deliberate
`__tostring` supersets and native loadfile/dofile overrides). Every remaining
gap for the 12-mod ladder is an EXERCISE gap (never console-fired) or a PATH/
INPUT-layer gap — not a binding gap — with the single real API-layer gap being
mod-relative io paths, addressed by the §5 shim design.
