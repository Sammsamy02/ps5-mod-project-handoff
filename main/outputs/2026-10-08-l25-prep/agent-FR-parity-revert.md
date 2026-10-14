# agent-FR — PRIORITY-1 parity revert: property reads return FName/FText/FString USERDATA

2026-10-10, agent FR, Claude PRIORITY-1 directive. Tree:
`outputs/2026-10-08-l26-runtime/typescan-r4/` (flattened `candidate/src/`).
Edited ONLY `candidate/src/LuaUObject.cpp` (3 Get branches) + created the two
host-test files. No console contact, no builds beyond one `-fsyntax-only`,
CHANGES.json untouched (root syncs), no bridge-file edits (FW's round).

## 1. Reverted sites (candidate/src/LuaUObject.cpp)

FM's plain-Lua-string property reads (agent-FM-property-rev.md §1, sites
LuaUObject.cpp:1901-1910/1965-1973/2023-2033 pre-edit) reverted to the UPSTREAM
userdata construction — verbatim the `#else` arms / l22-diagnostics original
(`outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp:1835-1838,
1865-1868, 1895-1898`). The whole `#ifdef __PROSPERO__/#else/#endif` block in
each Get branch was removed; upstream never had a Prospero branch there. Each
site carries a short provenance comment (`agent-FR`, PRIORITY-1) so the
plain-string behavior is not reintroduced.

| post-edit file:line | pusher (Get + GetNonTrivialLocal) | before (FM) | after (upstream restored) |
|---|---|---|---|
| LuaUObject.cpp:1898-1912 | `push_nameproperty` (fn at :1888) | `LuaFNameBridge::push_fname_to_lua(...)` plain string | `LuaType::FName::construct(params.lua, *name);` (:1910) |
| LuaUObject.cpp:1959-1970 | `push_textproperty` (fn at :1949) | `push_ftext_to_lua(...)` plain string | `LuaType::FText::construct(params.lua, *text);` (:1969) |
| LuaUObject.cpp:2013-2028 | `push_strproperty` (fn at :2003) | `push_fstring_to_lua(...)` plain string | `LuaType::FString::construct(params.lua, string);` (:2027) |

File: 2803 -> 2794 lines. `#ifdef`/`#endif` balance 20/20. Set branches
(agent-FD forward families: `Bridge::to_fname`/`to_ftext`/`to_fstring`),
`GetParam` (RemoteUnrealParam), and every other line untouched.

## 2. Kept FM improvements (verified present post-edit)

- enumproperty keys: `wide_to_narrow` x2 (:1736, :1747 — `push_enumproperty`
  Enum_<name> global + entry keys).
- out-param table keys: `fname_to_narrow` (:392).
- custom-property / struct-field table keys: `wide_to_narrow` x4 (:780, :796,
  :896, :912).
- delegate `FunctionName` values: `push_fname_to_lua` x3 (:2258, :2364, :2439).
- Set branches (FD forward families) in all three pushers.

NOTE for root (no action by FR — task explicitly keeps these): upstream
l22 original built delegate `FunctionName` as FName USERDATA
(`FName::construct(params.lua, delegate_value->GetFunctionName())`, l22
:2104/:2202). FM's plain-string table values were kept per dispatch ("Keep
... delegate FunctionName values ... intact"). If strict PRIORITY-1 pass-back
semantics are later wanted for `prop.FunctionName` too, that is a separate
root-assigned change (same one-line construct swap).

## 3. Userdata-return proof chain (end-to-end, on paper)

### `.Prop` -> FName userdata
1. `pickup.PickupInventoryItem.ItemId` -> UScriptStruct `__index` ->
   `m_property_value_pushers[NameProperty]` -> `push_nameproperty` Get ->
   **`LuaType::FName::construct(params.lua, *name)`** (LuaUObject.cpp:1910).
2. `FName::construct` (candidate/src/LuaFName.cpp:14-67, the definition the
   `lua-luafname` unit compiles — verified via
   l26-A/pass1/objects/lua-luafname.command.json `-c` source): gets/creates the
   registry metatable `"FNameUserdata"` (`lua.get_metatable` /
   `lua.new_metatable<FName>`).
3. `LuaMadeSimple::Lua::transfer_stack_object`
   (port/first/include/LuaMadeSimple/LuaMadeSimple.hpp:636-690): creates the
   value with `lua_newuserdatauv`, then `luaL_getmetatable(L, metatable_name)`
   + `lua_setmetatable` — the userdata's metatable IS the registry
   "FNameUserdata" table. `type(prop)` is therefore `'userdata'`.

### `prop:ToString()` -> bridged string
`__index` -> member table `ToString` (LuaFName.cpp:169-182) ->
`LuaFNameBridge::push_fname_to_lua(lua.get_lua_state(), ...)` — pool decoder ->
hand UTF-16 -> UTF-8 -> `lua_pushlstring` ('fname-rev' family).

### `tostring(prop)` -> bridged string
FL's metamethod (LuaFName.cpp:28-60): at metatable creation,
`luaL_getmetatable(lps, "FNameUserdata")` + `lua_pushcfunction` +
`lua_setfield(-2, "__tostring")`; the closure calls the SAME
`push_fname_to_lua(self->get_local_cpp_object())`. Because the metatable is
registry-cached and attached to every userdata by `transfer_stack_object`,
every FName property read stringifies through the bridge. Identical payload to
`:ToString`.

### FText
Same pattern, fully wired: `push_textproperty` -> `LuaType::FText::construct`
(LuaUObject.cpp:1969) -> LuaFText.cpp:36-87 (metatable "FText", `__tostring`
at :50-80 -> `push_ftext_to_lua`) + `:ToString` member at :224-238 ->
`push_ftext_to_lua` (both under `__PROSPERO__`). `lua-luaftext` unit compiles
candidate/src/LuaFText.cpp (verified via its command.json).

### FString
`push_strproperty` -> `LuaType::FString::construct(params.lua, string)`
(LuaUObject.cpp:2027) -> `TLuaStringBase::construct`
(candidate/include/LuaType/LuaUnrealString.hpp:27-47; the `-H` include trace of
my syntax check PROVES `<LuaType/LuaUnrealString.hpp>` resolves to this
candidate header, not the port-tree one) -> `transfer_stack_object` with
metatable "FString". `:ToString` (LuaUnrealString.hpp:69-73 -> `push_string_to_lua`
:199-220) -> **bridged** under `__PROSPERO__` via
`LuaFNameBridge::push_fstring_to_lua` (root's 2026-10-10 wiring).
**GAP for agent FW (file belongs to FW this round; FR did not touch it):**
`TLuaStringBase::setup_metamethods` (LuaUnrealString.hpp:61-64) is EMPTY — the
"FString" metatable has NO `__tostring`, so `tostring(fstring_prop)` still
prints Lua's default `FString: 0x...` until FW attaches it. The exact pattern
to copy is LuaFName.cpp:28-60: at metatable creation,
`luaL_getmetatable(lps, "FString")` + `lua_pushcfunction` (closure ->
`push_fstring_to_lua(l, str)` — note the userdata block is a
placement-new'ed `TLuaStringBase`, so recover via `lua_touserdata` +
`get_local_cpp_object()` as LuaFName.cpp does for `LuaType::FName`) +
`lua_setfield(-2, "__tostring")`, guarded `__PROSPERO__`. Until then,
`p:ToString()` works (bridged) but `tostring(p)`/`print(p)` does not — the
FName ItemId surface (the PRIORITY-1 case) is complete either way.

## 4. Host test (battery-cheap, pure Lua 5.5.1, no engine)

Files: `outputs/2026-10-08-l25-prep/host-tests/prop-parity-test.lua` +
`run-prop-parity.sh` (executable). Runner picked `lua5.5`
(Lua 5.5.1, /opt/homebrew/bin/lua5.5; no luajit). The test stubs the
UE4SS FName userdata contract (metatable with `__tostring` + `__index`
member table carrying `ToString`/`type`, both string routes returning the same
marker). Lua 5.5.1 has no `newproxy`, so userdata itself cannot be constructed
in pure Lua: the test uses a metatable-carrying value and asserts the
UE4SS-visible `:type()` name, DELEGATING the strict `type()=='userdata'` check
to the C++ construct sites below (real-userdata mode auto-activates if the
interpreter ever offers `newproxy`).

Output (exit 0):

```
PASS tostring(prop) via __tostring == marker — got "BRIDGED:ItemId_Dummy_Pickup_001"
PASS prop:ToString() via member == marker — got "BRIDGED:ItemId_Dummy_Pickup_001"
PASS tostring(prop) == prop:ToString()
PASS prop:type() == 'FNameUserdata' (userdata-type check delegated to C++ construct sites) — interpreter Lua 5.5 has no newproxy; got "FNameUserdata"
PASS round-trip pass-back obj:ToString() == marker
PASS round-trip pass-back tostring(obj) == marker
PASS plain-string shape FAILS the p:ToString() contract (revert is required) — ...: attempt to call a nil value (method 'ToString')
PASS print-shaped concat uses __tostring
ALL PASS: 0 failure(s) [stub=table-emulation, interpreter=Lua 5.5]
```

Check 7 is the regression guard: it demonstrates WHY the revert is required —
the FM plain-string shape errors on the PC-mod `p:ToString()` pattern.

## 5. C++-side note — production sites that MUST produce userdata (console grep list)

Primary (this revert), `candidate/src/LuaUObject.cpp`:
- :1910 `LuaType::FName::construct(params.lua, *name);` — push_nameproperty Get/GetNonTrivialLocal
- :1969 `LuaType::FText::construct(params.lua, *text);` — push_textproperty Get/GetNonTrivialLocal
- :2027 `LuaType::FString::construct(params.lua, string);` — push_strproperty Get/GetNonTrivialLocal

Companion userdata producers (unchanged, confirm the full surface):
- :545 `LuaType::FName::construct(lua, ...->GetNamePrivate());` — UObject `:GetName` binding
- :2090 `LuaType::FUtf8String::construct(...)`, :2132 `LuaType::FAnsiString::construct(...)` — untouched upstream pushers

Console-session runtime verification: `type(pickup.PickupInventoryItem.ItemId)`
must print `userdata`; `pickup.PickupInventoryItem.ItemId:ToString()` and
`tostring(pickup.PickupInventoryItem.ItemId)` must print the same id, with
`[LFB-M] fname-rev` firing on both (first-10 per family).

## 6. Syntax evidence (single file, battery-safe)

`prospero-clang++ -fsyntax-only -H` on candidate/src/LuaUObject.cpp, flags
verbatim from `outputs/2026-10-08-l26-build-variants/l26-A/pass1/objects/lua-luauobject.command.json`
with the `-ivfsoverlay` pair stripped (per dispatch) and `-c/-o/-MD/-MF`
replaced by `-fsyntax-only`; `LLVM_CONFIG=.../ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`
(+ that bin on PATH for the SDK wrappers). One invocation, no output files.

- **rc=0, 0 errors, 26 warnings.** All 19 warnings citing LuaUObject.cpp are
  the pre-existing `operator unsigned int` deprecations at untouched lines
  (336, 400, 432, 742, 830, 842, 849, 855, 856, 1012, 1065, 1245, 1304, 1475,
  1479, 1513, 1517, 2642, 2765) — none in the edited ranges
  (1898-1912, 1959-1970, 2013-2028). Remaining 7 are port-tree header
  deprecations (`-Wswitch`, enum-enum conversions).
- `-H` trace: `<LuaType/LuaUnrealString.hpp>` ->
  **candidate**/include/LuaType/LuaUnrealString.hpp (root-wired, bridged
  :ToString); `<LuaType/LuaUObject.hpp>` -> candidate's; LuaFName/LuaFText
  declarations from ue4ss-unreal-native-build-1 (definitions linked are the
  candidate .cpps, see §3). Full log: `/tmp/fr-syntax-luauobject-final.log`.

## 7. Hashes

| file | sha256 |
|---|---|
| candidate/src/LuaUObject.cpp (post-edit, 2794 lines) | aa10e4eadabe2f324fce2f95487bff9c76c225dfab22e0809dfc440235e16a32 |
| 2026-10-08-l25-prep/host-tests/prop-parity-test.lua | e917f2ef09a6e51a305692c76fb4b4c2e80b8d010471f4842509400c3c44d92e |
| 2026-10-08-l25-prep/host-tests/run-prop-parity.sh | 58f0b259ba5901cd8308e09406b34d2dcc1c4622c9eb6780cc3a9850fede073e |

Pre-edit LuaUObject.cpp was 2803 lines (FM's report sha
4f368babf2b380d72d9222e6c6777ca395e567e89038da80ce4cb1a0993e3b34); my three
Edit old-strings matched FM's comment blocks verbatim at FM's cited lines, and
nothing else in the file changed.
