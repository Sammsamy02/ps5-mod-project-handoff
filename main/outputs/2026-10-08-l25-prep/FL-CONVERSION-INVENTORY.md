# FL-CONVERSION-INVENTORY — every Lua <-> engine string conversion site (candidate tree)

Agent FL, 2026-10-10. Scope: full grep of `2026-10-08-l26-runtime/typescan-r4/candidate/{src,include}`
plus the shared out-of-tree Lua headers the build resolves through the vfs overlay
(`.../native-coherent-runtime-1/source/port/ue4ss/include/LuaType/LuaUnrealString.hpp`,
`.../ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaType/LuaFName.hpp`,
`LuaFText.hpp`, and `LuaMadeSimple` metamethod plumbing).
Grep families covered: `to_string(`, `to_wstring(`, `ensure_str(`, `DecodeUtf8(`, `EncodeUtf8(`,
`::Strlen(`, `to_utf8_string(`, `__tostring`, `lua_pushstring/lua_pushlstring`, `luaL_tolstring`,
`lua_tolstring`, `set_string(`, `push_*_to_lua`, `fname_to_narrow`, `wide_to_narrow`,
`GetFullName`/`GetName` Lua returns.

Line numbers measured 2026-10-10T06:21Z (pre-FL-edit shas recorded in
`agent-FL-reverse-routing.md`); agents FM/FK are editing their files concurrently, so
line drift is possible — cite the quoted code, not just the line.

Classification:
- **(a)** already bridge-routed — marker/family cited.
- **(b)** NOT routed — this round's targets. Owner marked **[FL]** (my files), **[FM]** (LuaUObject.cpp / CustomProperty.cpp / LuaMod.cpp / LuaUObject.hpp / NativePropertyReadBackend path — agent FM), **[FK]** (NameTypes.cpp), **[ROOT]**.
- **(c)** unreachable from Lua / log-only narrow<->wide for Output::send devices, error-message text, init/boot diagnostics — low priority, retain Helpers codec (Claude item 9: helpers stay for non-Lua consumers).

## 1. The bridge itself (owner FL)

| file:line | site | class |
|---|---|---|
| include/LuaFNameBridge.hpp:143-226 | `utf8_to_utf16` hand forward core | (a) the converter |
| include/LuaFNameBridge.hpp:232-285 | `utf16_to_utf8` hand reverse core | (a) the converter |
| include/LuaFNameBridge.hpp:410-421 | `Bridge::to_fname` (slot-2 ctor direct) — `[LUA-FN]` first-20 log | (a) |
| include/LuaFNameBridge.hpp:430,440 / src/LuaFNameBridge.cpp:48-81 | `Bridge::to_fstring` / `to_ftext` — `[LFB-M] fstring-fwd` / `ftext-fwd` | (a) console-proven |
| src/LuaFNameBridge.cpp:87-138 | `push_wide_family` -> `push_fstring_to_lua` / `push_ftext_to_lua` — `[LFB-M] fstring-rev` / `ftext-rev` (families exist; **zero console firings** because no Lua-visible site called them yet — the userdata `__tostring`/`:ToString` surface was unrouted; see §2) | (a) API exists |
| src/LuaFNameBridge.cpp:147-186 | `fname_to_narrow` / `wide_to_narrow` — `[LFB-M] fname-narrow` | (a) console-proven |
| include/LuaFNameBridge.hpp:580-584 | `push_fname_to_lua` (FName::ToString pool decoder -> hand UTF-16 -> UTF-8 -> `lua_pushlstring`) — **carries NO family marker**; FM had to add its own site markers (LuaUObject.cpp:100-110 `log_prop_rev_site`) | **(b) [FL]** this round: add `fname-rev` family |
| include/LuaFNameBridge.hpp:547-578 | `push_wide_to_lua` (GetFullName-shaped wide text) — overlong notice is once-per-process, no family, no call-site tag | **(b) [FL]** wire to family logger |
| include/LuaFNameBridge.hpp:443-457 + src/NameTypes.cpp:63-77 | forward refusal -> `NotifyFNameSeamRefusedOnce` — `'[FN] seam refused:'` once-per-process, no call-site tag, no hex units | **(b) [FL]** bridge-local first-10 + site tag + hex units (FK does the NameTypes.cpp side); **[FK]** owns NameTypes.cpp:63 |
| include/LuaFNameBridge.hpp:682-701 | `notify_family_refused_once` — once per family, no site tag/units | **(b) [FL]** first-10 + site + hex units |

## 2. Lua userdata ToString / __tostring surface (the mod's `str(...)` failure)

| file:line | site | class |
|---|---|---|
| candidate/src/LuaFName.cpp:136-146 | FName userdata `:ToString` member -> `push_fname_to_lua` (routed, but no family marker in the pusher) | **(b) [FL]** gets `fname-rev` via bridge upgrade |
| candidate/src/LuaFName.cpp:47-131 | FName `setup_metamethods`: only `Call` + `Equal`. **NO `__tostring`** -> Lua `tostring()`/`str()` prints `FNameUserdata: 0x...` | **(b) [FL]** add `__tostring` -> bridge |
| candidate/src/LuaFName.cpp:171 | `set_string(ClassName::ToString())` — narrow literal, no conversion | trivial-narrow |
| candidate/src/LuaFText.cpp:187-203 | FText userdata `:ToString` -> `push_ftext_to_lua` (`ftext-rev` family) under `__PROSPERO__`; `#else` keeps `to_string(*fstring)` (upstream host) | (a) Prospero / (c) host |
| candidate/src/LuaFText.cpp:64-181 | FText `setup_metamethods`: only `Call` + `Equal`. **NO `__tostring`** | **(b) [FL]** add `__tostring` -> bridge |
| candidate/src/LuaFText.cpp:112-115 | host `#else` branch `ensure_str(lua.get_string())` | (c) host-only upstream |
| candidate/src/LuaFText.cpp:208 | `set_string(ClassName::ToString())` — narrow literal | trivial-narrow |
| **port-tree** `.../port/ue4ss/include/LuaType/LuaUnrealString.hpp:60-63 | `TLuaStringBase::setup_metamethods` is EMPTY — FString/FUtf8String/FAnsiString userdata have **NO `__tostring`** | **(b) [FM/ROOT]** out of FL's file list |
| **port-tree** `LuaUnrealString.hpp:198-210 | FString userdata `:ToString` -> `to_utf8_string(*str)` (Helpers codec; on Prospero = the wide-codec seam) — should be `push_fstring_to_lua` | **(b) [FM/ROOT]** |
| **port-tree** `LuaUnrealString.hpp:212-254 | `get_string_from_object`/`append_string`/`find_substring`/`starts_with`/`ends_with` use `to_utf8_string`/`ensure_str` | **(b)-low [FM/ROOT]** (Lua-reachable FString userdata members) |
| **port-tree** `LuaUnrealString.hpp:257-332 | FUtf8String/FAnsiString `push_string_to_lua` — natively narrow, no wide conversion | trivial-narrow (a) |

## 3. Property-read returns / struct field reads / Lua table keys (LuaUObject.cpp + LuaUObject.hpp — agent FM)

| file:line | site | class |
|---|---|---|
| src/LuaUObject.cpp:1917-1933 | `push_nameproperty` Get -> `push_fname_to_lua` plain Lua string (was FName userdata) + FM site marker `log_prop_rev_site` | (a) [FM] |
| src/LuaUObject.cpp:1975-1996 | `push_textproperty` Get -> `push_ftext_to_lua` plain Lua string (was FText userdata) | (a) [FM] |
| src/LuaUObject.cpp:2046-2056 | `push_strproperty` Get -> `push_fstring_to_lua` plain Lua string (was FString userdata) | (a) [FM] |
| src/LuaUObject.cpp:2284-2292, 2391-2399, 2467-2475 | delegate function-name reads -> `push_fname_to_lua` + site markers | (a) [FM] |
| src/LuaUObject.cpp:385-417 | out-param table keys -> `fname_to_narrow` (`fname-narrow`) | (a) |
| src/LuaUObject.cpp:796-818, 912-934 | custom-property / struct-field Lua table keys -> `wide_to_narrow` (`fname-narrow`) | (a) |
| src/LuaUObject.cpp:1749-1769 | custom map key/elem names -> `wide_to_narrow` | (a) |
| src/LuaUObject.cpp:815-821, 931-937 | `handle_property(field, to_utf8_string(field->GetName()))` on the **`#else` miss branch** — unrouted Helpers codec feeding property lookup | **(b) [FM]** |
| src/LuaUObject.cpp:344-352, 414-418, 445-449 | `to_string(param->GetName()/.ToString())` in UFunction call error text | (c)-low [FM] error text |
| src/LuaUObject.cpp:760-765, 876-882 | `to_wstring(field_name)` Verbose log | (c)-low [FM] |
| src/LuaUObject.cpp:958-965 | `to_string(script_struct->GetName())` Verbose log | (c)-low [FM] |
| src/LuaUObject.cpp:1723-1732 | `to_string(params.property->GetName())` Set-branch error text | (c)-low [FM] |
| src/LuaUObject.cpp:1979-2003 | Lua string -> FName/FText/FString property push (`Bridge` memory-source variant; `fname-param`/`fstring-fwd`/`ftext-fwd`) | (a) |
| src/LuaUObject.cpp:2197-2230 | delegate table keys `lua_pushstring(L, "Object"/"FunctionName")` | trivial-narrow |
| src/LuaUObject.cpp:2577-2583, 2702-2708 | `to_string(property_type.ToString())` error text | (c)-low [FM] |
| src/LuaUObject.cpp:2680 | `set_string("RemoteUnrealParam")` literal | trivial-narrow |
| include/LuaType/LuaUObject.hpp:533-545 | `GetFullName` Lua binding -> `push_wide_to_lua` (routed; no family marker — covered by FL bridge upgrade) | (a) marker pending [FL] |
| include/LuaType/LuaUObject.hpp:594-612 | `GetProperty` name -> `Bridge` (`[LFB]`, FNAME_Find) | (a) |
| include/LuaType/LuaUObject.hpp:735-760 | `ProcessConsoleExec` cmd -> `ensure_str(lua.get_string())` — forward, unrouted | **(b) [FM]** |
| include/LuaType/LuaUObject.hpp:330-338 | `to_string(property->GetFullName())` in `PusherParams::throw_error` | (c)-low [FM] error text |
| include/LuaType/LuaUObject.hpp:955-966 | `to_string(property_type_name)` error text | (c)-low [FM] |
| include/LuaType/LuaUObject.hpp:205-215, 780-790, 925-935 | `set_string("RemoteObjectBase"/"UObject"/"LocalUnrealParam")` literals | trivial-narrow |
| src/LuaUObject.cpp:545-567 | UObject `:GetName` Lua binding -> `LuaType::FName::construct(GetNamePrivate())` — returns **FName userdata**; `str(obj:GetName())` depends on FL's `__tostring` | **(b) [FL]** fixed by FName `__tostring` |

## 4. LuaMod.cpp (agent FM; FM already landed `modbridge::lfb_*` helpers this round)

| file:line | site | class |
|---|---|---|
| src/LuaMod.cpp:62-106 | FM's `lfb_set_string_from_wide` / `lfb_set_string_from_stringtype` / `lfb_narrow_from_stringtype` -> `wide_to_narrow` (`fname-narrow`) / `push_fstring_to_lua` (`fstring-rev`) | (a) [FM] |
| src/LuaMod.cpp:4966, 5007 | LoadMap Error FString -> `push_fstring_to_lua` (`fstring-rev`) | (a) [FM] |
| src/LuaMod.cpp:8776-8782, 8839-8845, 8903-8909, 8953-8959 | ULocalPlayerExec / CallFunctionByNameWithArguments cmd echo -> `lfb_set_string_from_wide` | (a) [FM] |
| src/LuaMod.cpp:9097-9101, 9161-9165, 9236-9240, 9301-9305 | ProcessConsoleExec command StringType -> `lfb_set_string_from_stringtype` | (a) [FM] |
| src/LuaMod.cpp:~9047 (`luaL_loadstring(L, to_string(cmd).c_str())` in the console executor) | engine StringType command -> narrow for `luaL_loadstring` — **unrouted** | **(b) [FM]** |
| src/LuaMod.cpp:7769-7773, 7786-7790 | `FPackageName:IsShortPackageName` / `IsValidLongPackageName` — `ensure_str(lua.get_string())` forward — **unrouted** | **(b) [FM]** |
| src/LuaMod.cpp:~7700-7750 | FUtf8String / FAnsiString Lua ctors — natively narrow `str.data()` | trivial-narrow |
| src/LuaMod.cpp:~7639-7660 | Lua `FString(...)` ctor -> `Bridge` memory-source (`fstring-fwd`) | (a) [FM] |
| src/LuaMod.cpp:2283-2345, 5679-5721, 2775, 386, 583, 8269 | `to_string(function_name_no_prefix / property_type)` — Lua `throw_error` message text only | (c)-low [FM] |
| src/LuaMod.cpp:3839-3880 region, 1693-1830, 4100-4120 | FindFirstOf/StaticFindObject/RegisterConsoleCommandGlobalHandler Lua strings -> `Bridge` | (a) |
| src/LuaMod.cpp:159, 211, 1072-1129, 1577, 2030-2154, 3317-3605, 7750-7916, 8318, 8956, 9251-9257 | `ensure_str`/`to_wstring` on log text, paths, `e.what()` | (c) |
| src/LuaMod.cpp:872, 960-1016, 1340, 1393-1570 | `lua_tolstring`/`lua_pushstring` on already-narrow Lua/module bytes (require/searcher machinery) | narrow-only, no engine string |
| src/LuaMod.cpp:9297-9306 + comments 2274, 4113 | `luaL_tolstring`-shaped self-test probes / memory-source bridge uses | (a)/(c) mixed |

## 5. LuaLibrary.cpp (owner FL)

| file:line | site | class |
|---|---|---|
| src/LuaLibrary.cpp:35-116 | `global_print` — `luaL_tolstring` (the Lua `print` global; invokes userdata `__tostring`, so FName/FText payloads get bridge-routed once FL's metamethods land) then `Unicode::DecodeUtf8` x3 (lines 64, 78, 92) widens the narrow payload for Output/FOutputDevice | **(b) [FL]** route the widen through the bridge hand loop (`print-widen` family) under `__PROSPERO__`; `luaL_tolstring` itself is narrow-native |
| src/LuaLibrary.cpp:139 | `ensure_str(e)` — exported-function error handler log text | (c) |
| src/LuaLibrary.cpp:151-176 | `exported_function_status_to_string` — narrow literal table | trivial-narrow |
| src/LuaLibrary.cpp:127, 190-220, 233-266 | narrow `std::string`/`lua_tostring` plumbing for exports API | narrow-only |

## 6. Engine-side / internal-only (retain Helpers codec; no Lua reachability for engine-string marshaling)

| file:line | site | class |
|---|---|---|
| src/NameTypes.cpp:36-77 (+ include/Unreal/NameTypes.hpp:323-364) | pool decoder `ToString`, seam refusal `'[FN] seam refused:'`, ctor-unready notices | **[FK]** (a) decoder-owned |
| src/UE4SSProgram.cpp:185-1729 (24 sites) | `ensure_str`/`to_string`/`EncodeUtf8` on config paths, build stamps, log banners, input source | (c) |
| src/UnrealInitializer.cpp:353-2540 (28 sites) | module/export resolution, diagnostics, build-profile lines | (c) |
| src/UObjectGlobals.cpp:814, 1040-1046 | FFO verified-query diagnostic log | (c) |
| src/UObjectGlobals.cpp:1158-1379 | `FindFirstOf/FindAllOf(ensure_str(ClassName))` — C++-side callers (Lua path goes through LuaMod's `Bridge`); internal overload only | (c) internal |
| src/UObjectGlobals.cpp:1994 | `to_string(FunctionFullNameNoType)` in "Unable to find function" exception text | (c)-low |
| src/UObjectArray.cpp:725-1019 | listener instrumentation logs | (c) |
| src/TypeChecker.cpp:968-1000 | narrow-first diagnostics (`Unicode::DecodeUtf8(narrow_line)` — log device) | (c) |
| src/ScriptHookThunks.cpp:76-85 | thunk exception logs | (c) |
| src/ToolchainSelfTest.cpp:45 | build-stamp narrow->wide self-test | (c) |
| src/File/FileType/PosixFile.cpp:395 | `std::to_string(pid)` | (c) |
| include/Helpers/String.hpp:544+ / include/Helpers/Utf16Utf8.hpp | the converter implementations themselves (`ensure_str`/`to_string`/`to_wstring`/`to_utf8_string`/`DecodeUtf8`/`EncodeUtf8`) — retained for every non-Lua consumer (Claude item 9) | definitions |
| src/CustomProperty.cpp | no Lua-facing string conversion (layout/memcpy only) | none |
| src/Property/*.cpp, generated_include, VmtTickPumpCore.inc | no Lua-facing string conversion | none |

## Bottom line for this round

1. **[FL]** FName userdata + FText userdata need `__tostring` (and FName's pusher needs the
   `fname-rev` family logger) — that is the `str(pickup.PickupInventoryItem.ItemId)` /
   `str(obj:GetName())` failure: FName/FString/FText userdata stringification falls to Lua's
   default `userdata: 0x...` because NO Lua type in this tree registers `__tostring`, and the
   MetaMethod framework enum (LuaMadeSimple.hpp:131-139) has no Tostring entry — the metamethod
   must be attached to the named metatable via the raw Lua C API from the owning .cpp.
2. **[FM/ROOT]** the FString userdata surface lives in the port-tree header
   `LuaUnrealString.hpp` (outside the candidate tree and FL's file list): empty
   `setup_metamethods` (no `__tostring`) and `:ToString` via `to_utf8_string` (Helpers codec).
3. FM has already routed the implicit property READS (name/text/str) and the hook/callback
   echoes; remaining FM-side unrouted: `to_utf8_string(field->GetName())` miss-branches
   (LuaUObject.cpp:821, 937), `ensure_str` in ProcessConsoleExec (LuaUObject.hpp:747) +
   FPackageName members (LuaMod.cpp:7772, 7789), console-executor `to_string(cmd)`
   (LuaMod.cpp:~9047).
4. Everything else is log/init/device text (class (c)) — keep on the Helpers codec per
   Claude item 9.

## Postscript (post-implementation)
FL's own (b)-class targets from the tables above were landed the same day:
fname-rev family + first-10 site-tagged/hex-unit refusal upgrade (bridge),
FName/FText userdata __tostring, LuaLibrary print-widen routing. See
agent-FL-reverse-routing.md for file:line, shas, and syntax evidence.
