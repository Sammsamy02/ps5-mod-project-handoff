# agent-FM — reverse property-read routing through the LuaFNameBridge

2026-10-10 (local), agent FM. Scope per dispatch: route every string-shaped
return (property reads, struct-field reads, callback name echoes) in
`src/LuaType/LuaUObject.cpp` (= `candidate/src/LuaUObject.cpp` in this flattened
tree), `src/CustomProperty.cpp`, `include/NativePropertyReadBackend.inl`,
`src/LuaMod.cpp` through the bridge's UTF-16->UTF-8 reverse converters.
`__PROSPERO__` guards, upstream preserved under `#else`, no wide libc anywhere
new. No console contact, no --execute, CHANGES.json untouched (root syncs).

## 0. Coordination with FL

FL's `FL-CONVERSION-INVENTORY.md` landed at ~01:21 local, after my 10-minute
fallback window; I had already completed my own census and the edits. I then
reconciled against FL's report:
- FL added the `fname-rev` family to the bridge pushers (`push_fname_to_lua` /
  `push_wide_to_lua`, header lines ~667-737). I REMOVED my provisional TU-local
  `prop-rev` site markers and now rely on FL's `fname-rev` (naming matched per
  dispatch). No dual markers remain.
- FL flagged two unrouted FM sites I had missed; both routed this round
  (LuaMod.cpp FPackageName x2 + console-executor `luaL_loadstring`).
- FL's transient in-flight bridge edits (`site_`/`Family` errors) briefly broke
  my lua-luauobject check; I poll-retried and the final checks below ran against
  FL's LIVE stabilized bridge.

## 1. Routed sites

### candidate/src/LuaUObject.cpp (sha256
4f368babf2b380d72d9222e6c6777ca395e567e89038da80ce4cb1a0993e3b34, 2803 lines)

| file:line | site | converter | marker family |
|---|---|---|---|
| LuaUObject.cpp:1729-1741 | `push_enumproperty` Get: enum global-table name (`Enum_<name>`) + enum-entry Lua table keys — was `to_string(GetName()/.ToString())` | `wide_to_narrow` | `fname-narrow` |
| LuaUObject.cpp:1901-1910 | **`push_nameproperty` Get/GetNonTrivialLocal: FName property read -> plain Lua string** — was `LuaType::FName::construct` userdata. THE `ItemId` fix: struct field reads dispatch through `m_property_value_pushers` (UScriptStruct `__index` -> handle_unreal_property_value -> this pusher) | `push_fname_to_lua` | `fname-rev` (FL bridge) |
| LuaUObject.cpp:1965-1973 | `push_textproperty` Get/GetNonTrivialLocal -> plain Lua string — was FText userdata | `push_ftext_to_lua` | `ftext-rev` |
| LuaUObject.cpp:2023-2033 | `push_strproperty` Get/GetNonTrivialLocal -> plain Lua string — was FString userdata (the zero-firing family) | `push_fstring_to_lua` | `fstring-rev` |
| LuaUObject.cpp:2264-2268 | `push_delegateproperty` Get table `FunctionName` value -> Lua string | `push_fname_to_lua` | `fname-rev` |
| LuaUObject.cpp:2369-2374 | `push_multicastdelegateproperty` Get entry `FunctionName` -> Lua string | `push_fname_to_lua` | `fname-rev` |
| LuaUObject.cpp:2444-2449 | `push_multicastsparsedelegateproperty` Get entry `FunctionName` -> Lua string | `push_fname_to_lua` | `fname-rev` |

Semantics note (deliberate, per Claude's "route every ... property-read
return"): implicit property reads of Name/Text/Str properties now return plain
Lua strings on Prospero; explicit accessors keep producing userdata
(`GetFName()`/`FName()`/`FString()` ctors), and the Set/param-push branches
(agent FD's forward families) are unchanged. Upstream behavior preserved
verbatim under each `#else`.

### candidate/src/LuaMod.cpp (sha256
aac03fbc5897f500905db50022ea27960ade5e736dc0d9a836b125b51238fd9b, 9509 lines)

| file:line | site | converter | marker family |
|---|---|---|---|
| LuaMod.cpp:62-119 | TU-local `modbridge::lfb_*` helpers: `lfb_set_string_from_wide` (TCHAR*, hand bounded unit count — no wcslen), `lfb_set_string_from_stringtype`, `lfb_narrow_from_stringtype`, `lfb_narrow_from_wide` | `wide_to_narrow` | `fname-narrow` |
| LuaMod.cpp:4981-4984, 5022-5025 | LoadMap pre/post hook `Error` FString echo to Lua callbacks — was `set_string(to_string(*Error))` | `push_fstring_to_lua` | `fstring-rev` |
| LuaMod.cpp:7718-7738 | Lua `FString(...)` global ctor — was `ensure_str` + `FString(TCHAR*)` Strlen/NUL-scan ctor seam (forward-family site inside my file; refused input throws the binding's normal error) | `Bridge::to_fstring` | `fstring-fwd` |
| LuaMod.cpp:7790-7796 | `FPackageName:IsShortPackageName` Lua string -> StringType (FL-flagged miss) | `Bridge{ptr,len}.string()` | (Bridge compose; no refusal expected) |
| LuaMod.cpp:7815-7820 | `FPackageName:IsLongPackageName` (FL-flagged miss) | same | same |
| LuaMod.cpp:8809-8813, 8872-8876 | ULocalPlayerExec pre/post `cmd` (TCHAR*) echo | `lfb_set_string_from_wide` | `fname-narrow` |
| LuaMod.cpp:8935-8940, 8985-8990 | CallFunctionByNameWithName pre/post `str` (TCHAR*) echo | `lfb_set_string_from_wide` | `fname-narrow` |
| LuaMod.cpp:9080-9086 | Console executor `luaL_loadstring(L, to_string(cmd))` (FL-flagged miss) | `lfb_narrow_from_wide` | `fname-narrow` |
| LuaMod.cpp:9136-9140, 9200-9204, 9275-9279, 9340-9344 | ProcessConsoleExec command echo x4 (global pre, custom pre, custom post x2) | `lfb_set_string_from_stringtype` | `fname-narrow` |
| LuaMod.cpp:9148-9153, 9212-9217, 9288-9292, 9353-9357 | Command-part table values x4 | `lfb_narrow_from_stringtype` | `fname-narrow` |

### src/CustomProperty.cpp — sha256
66be0042102f1140d38bb0f305a21a3b6791394c51a31dd0b4e02471e33ef938 (unchanged)
No Lua-facing string conversion exists in this file (property
construction/layout/memcpy only — confirmed independently and by FL's
inventory §6). Nothing to route.

### include/NativePropertyReadBackend.inl — sha256
5c01a254184b7f759d80cadccad8d7aa52d9c126366bd82f501e1be79e8db271 (unchanged)
Dispatch assumed FName/FString/FText read cases here; in THIS tree the file is
the PS5 memory-admission backend (sceKernelVirtualQuery + fault-ledger coverage
for raw property byte reads) with zero string-shaped returns. The actual
string-read equivalents are the LuaUObject.cpp pushers (routed above). Nothing
to route.

## 2. The ItemId failure chain after this round

`pickup.PickupInventoryItem.ItemId`:
1. UObject `__index` (candidate include/LuaType/LuaUObject.hpp, already
   bridge-aware) -> `handle_unreal_property_value` (LuaUObject.cpp, dispatch).
2. StructProperty -> struct userdata (port-tree LuaUScriptStruct.cpp).
3. `.ItemId` -> UScriptStruct `__index` -> `m_property_value_pushers[NameProperty]`
   -> `push_nameproperty` Get -> **`push_fname_to_lua` -> plain Lua string**
   (`fname-rev` marker fires; pool decoder -> hand UTF-16 -> UTF-8 ->
   lua_pushlstring).
4. Mod's `str(v)` sees a plain string and passes it through — no userdata
   ToString, no Helpers codec, no seam.

Complementary: FL's FName/FText userdata `__tostring` (their files) covers the
`tostring(x)`/`x:ToString()` surface for userdata obtained via
`GetFName()`/`FName()`.

## 3. Syntax evidence

Compiler: `prospero-clang++` (SDK wrapper chain), flags verbatim from
`2026-10-08-l26-build-variants/l26-A/pass1/objects/<unit>.command.json` with
`-c/-o/-MD/-MF` replaced by `-fsyntax-only`. The `-ivfsoverlay
l26-A/headers.vfs.json` was validated NOT stale (51/51 external-contents
exist) and retained. The SDK wrappers need the deps LLVM on PATH
(`llvm-config-21`): prepended
`ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`
(`LLVM_BINDIR` env is overwritten by the wrapper itself; PATH is what works).

- `lua-luauobject` -> exit 0, **0 errors**, 27 warnings — all pre-existing
  `operator unsigned int` deprecations at untouched lines; none in edited
  ranges.
- `luamod` -> exit 0, **0 errors**, 0 warnings at LuaMod.cpp source lines.
- Both re-verified against FL's live bridge header after FL's fname-rev
  addition stabilized (01:32 local).
- `__PROSPERO__` probe (same flags): exit 0 — the Prospero branches were the
  code compiled (upstream `#else` paths not compiled, as in pass1).

## 4. Gaps for FL / root

1. **Port-tree `LuaUScriptStruct.cpp`** (compiled unit `lua-luauscriptstruct`,
   outside both my and FL's editable sets): `prepare_to_handle` converts the
   Lua field key via `FName(ensure_str(lua.get_string()), FNAME_Find)` — a
   live forward seam ON the `.ItemId` path (works for plain ASCII keys today,
   but it is the unrouted Helpers codec), and its miss-branch error text uses
   `to_string(GetFullName())`. Needs a candidate override next round.
2. **Port-tree `include/LuaType/LuaUnrealString.hpp`** (FL bottom-line 2):
   FString userdata `:ToString` = `to_utf8_string` (Helpers seam), empty
   `setup_metamethods` (no `__tostring`). Outside the candidate tree — root
   decision needed (candidate override or port-tree edit).
3. **`include/LuaType/LuaUObject.hpp:735-760`** `ProcessConsoleExec` Lua
   binding `ensure_str(lua.get_string())` — in MY task's file family but
   outside my authorized edit list (header not in the 4 permitted files).
   Unrouted forward site; root to assign.
4. `push_utf8strproperty`/`push_ansistrproperty` Get still construct userdata
   (out of Claude's FName/FString/FText list; natively narrow types — a
   passthrough would be trivial if directed).
5. Error-text `to_string(...)` calls (class (c)-low, both my files) left on the
   Helpers codec per Claude item 9 — matches FL's census classification.
6. `construct_fname`/`GetFName` (LuaUObject.cpp:~563) intentionally still
   yields FName userdata — the explicit accessor for mods that want userdata
   (e.g. passing names into UFunction params). Split is deliberate.

## 5. Expected first-10 marker lines next launch (FM-covered sites)

- `[LFB-M] fname-rev in='...'/len=... ok=1` — NameProperty reads (ItemId), delegate FunctionName values (FL's bridge logger, my routed sites).
- `[LFB-M] fstring-rev ...` — StrProperty reads, LoadMap Error echoes.
- `[LFB-M] ftext-rev ...` — TextProperty reads.
- `[LFB-M] fname-narrow ...` — enum table keys, console command echoes/parts, console-executor chunks.
- `[LFB-M] fstring-fwd ...` — Lua `FString("...")` ctor.
