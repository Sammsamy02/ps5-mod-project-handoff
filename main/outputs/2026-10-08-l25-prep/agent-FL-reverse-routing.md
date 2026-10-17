# agent-FL — reverse-direction routing (Lua FName/FString/FText -> Lua string)

2026-10-10, agent FL, Claude round-3 reverse directive. Tree:
`outputs/2026-10-08-l26-runtime/typescan-r4/` (header `include/LuaFNameBridge.hpp`,
sources `candidate/src/`). Companion inventory: `FL-CONVERSION-INVENTORY.md`.

## Files edited (ONLY FL-owned files touched)

| file | sha256 (post-edit) | sha256 (pre-edit) |
|---|---|---|
| typescan-r4/include/LuaFNameBridge.hpp | e47f3b3d8545c7911ffbfb17dd35e9779d432846a4bcde37118a71619a73a858 | 9d3c0c646d658e786bf2e16ce0a3115b4f2a1bc880db37ed9395c2f0f7ff68ab |
| typescan-r4/candidate/src/LuaFNameBridge.cpp | 94bc605702e326781dc65c5254b8f6df9e7b102caac1f60a77dfa9fad12d0c14 | 48d3c9044240c2e5fed56ac2b69be33722c63d1e1a7aa022bbddf76dcde12f0e |
| typescan-r4/candidate/src/LuaFName.cpp | 97551759c7b0782e98f8412f5342e50995dc61a0d079934e468c26d2b8da3397 | dd7a86499e2868c2ecab8f49345e82757c619a76dd7128f04999d96d3dc7fb55 |
| typescan-r4/candidate/src/LuaFText.cpp | 1160a196187e4874d337f50f738ea55599de1f8cdd3997122b3baa1e9ee8c17e | 61250bfd77c176649c788d5a7cabc8dca80479afe4664cd4b339df53d2c89596 |
| typescan-r4/candidate/src/LuaLibrary.cpp | 5d9a97e11723426e7e9f21d0c64237449677e7ce13609f0124080f620b68178e | 2cada608acf79b9be73afa2c1ce3fa4d7cbec6febefb9456be19f267ac049454 |

NOT touched (per rules): LuaUObject.cpp / CustomProperty.cpp / LuaMod.cpp /
include/LuaType/LuaUObject.hpp (FM), NameTypes.cpp (FK), CHANGES.json (root),
port-tree LuaUnrealString.hpp (FString userdata — documented in the inventory for FM/root).

## Why fstring-rev/ftext-rev never fired, and the str() failure

`str(pickup.PickupInventoryItem.ItemId)` / `str(obj:GetName())` stringify FName
USERDATA through Lua's `tostring()`. No Lua type in this tree registered a
`__tostring` metamethod — the LuaMadeSimple `MetaMethod` enum
(LuaMadeSimple.hpp:131-139) has no Tostring entry — so userdata fell to Lua's
default `FNameUserdata: 0x...` garbage, and the `:ToString` members (routed) were
never what `str()` calls. Fix: attach `__tostring` to the registry-named
metatable via the raw Lua C API from the owning .cpp (`luaL_getmetatable` +
`lua_pushcfunction` + `lua_setfield`), guarded `__PROSPERO__` (upstream keeps
default stringification under `#else` semantics).

## Sites routed this round (file:line, post-edit)

1. `candidate/src/LuaFName.cpp:20-60` — FName userdata metatable "FNameUserdata"
   gains `__tostring` -> `LuaFNameBridge::push_fname_to_lua(self->get_local_cpp_object())`
   (userdata block is the placement-new'ed `LuaType::FName`; `get_local_cpp_object()`
   is the public LocalObject accessor). Registers once at metatable creation.
2. `candidate/src/LuaFName.cpp:169-180` — `:ToString` member unchanged in body;
   comment updated; now observable because the pusher logs (see bridge changes).
3. `candidate/src/LuaFText.cpp:44-81` — FText userdata metatable "FText" gains
   `__tostring` -> `LuaFNameBridge::push_ftext_to_lua(...)` (existing `ftext-rev`
   family). `:ToString` member was already routed (agent FD), unchanged.
4. `candidate/src/LuaLibrary.cpp:60-76` — `global_print`'s payload widening
   (the `luaL_tolstring` result of ANY Lua value, incl. the new metamethods)
   switches from `Unicode::DecodeUtf8` to
   `LuaType::LuaFNameBridge::widen_narrow_to_stringtype(raw_string, std::strlen(raw_string))`
   under `__PROSPERO__`; upstream branch keeps `Unicode::DecodeUtf8`. Narrow
   `std::strlen` only — no wide libc.
5. `include/LuaFNameBridge.hpp:729-738` — `push_wide_to_lua` (GetFullName-shaped
   reads, used by LuaUObject.hpp:545) and `push_fname_to_lua` (FName userdata
   :ToString, __tostring, name/delegate property reads at LuaUObject.cpp:1933/2292/2399/2475)
   now delegate to the shared family pusher and log the NEW `fname-rev` family.

## Bridge upgrades (include/LuaFNameBridge.hpp + candidate/src/LuaFNameBridge.cpp)

New APIs (the reverse variants the inventory showed missing):
- `push_wide_units_to_lua(lua_State*, const CharType*, size_t, Family)` —
  exported shared family pusher for arbitrary engine wide units
  (`candidate/src/LuaFNameBridge.cpp:105-135`); header decl at hpp:727.
- `widen_narrow_to_stringtype(const char*, size_t) -> StringType` — narrow ->
  StringType reverse filler for LuaLibrary print payloads
  (`candidate/src/LuaFNameBridge.cpp:226-279`); resize + indexed writes only
  (EV discipline); input bounded to kMaxPushUnits bytes with a hand
  codepoint-boundary walk-back; header decl at hpp:846.

Refusal-logging upgrade (task item 1, same upgrade class as FK's NameTypes.cpp
change):
- Family refusals are now FIRST-10 per family
  (`notify_family_refused`, hpp:796-825) with:
  - `site=0x%03llX` — low 12 bits of `__builtin_return_address(0)` captured at
    each exported bridge API's entry (= the Lua binding that called the bridge);
    the forward Bridge ctor stores the same tag as a member (hpp:296/313).
  - `units=%04X,...` — first 8 offending units hex (`render_units_hex`,
    hpp:146-193): UTF-16 units for reverse refusals, raw input bytes for
    forward ones.
  - Prefix kept: `[LFB-M] <family> refused reason=... len=... at=... site=...
    units=... (yielding empty; no wide-libc fallback)`.
- Forward Bridge refusals gain a bridge-local FIRST-10
  `'[FN] seam refused:' reason='...' len=... at=... site=0xNNN units=...`
  channel (`Bridge::log_forward_refusal_site`, hpp:527-561) with the ctor's
  call-site tag; the shared once-per-process
  `Unreal::NotifyFNameSeamRefusedOnce` delegation (NameTypes.cpp, FK's file)
  is untouched and still fires — FK's parallel upgrade owns that side.
- `notify_family_refused_once` kept as a compatibility shim (captures site at
  caller, no units).

Family enum additions (hpp:676-681): `FNameRev = 6` -> `"fname-rev"`,
`PrintWiden = 7` -> `"print-widen"`; `FamilyCount = 8`. `FamilyLog` gains a
per-family `refusals` counter. `to_fstring`/`to_ftext`/`push_fstring_to_lua`/
`push_ftext_to_lua`/`wide_to_narrow`/`fname_to_narrow` refusal call sites in
the .cpp now pass site + units (bytes for forward, UTF-16 units for reverse).

## Markers

Existing shape preserved: `[LFB-M] <family> in='<payload>'/len=<N> ok=<0|1>`
(first 10 per family). Families after this round:
`fstring-fwd, ftext-fwd, fstring-rev, ftext-rev, fname-narrow, fname-param,
fname-rev (NEW), print-widen (NEW)`.

FOR ROOT — manifest declarations needed (markers-declared.json):
- `[LFB-M] fname-rev` — declared here per task instruction (family name was
  absent from markers-declared.json). Composable literals: the narrow
  `"fname-rev"` string in `family_name` (UTF-8) plus the runtime-rendered
  marker line.
- `[LFB-M] print-widen` — same class, new this round.
- `[FN] seam refused:` bridge-fwd variant (`"... (bridge fwd; NAME_None
  produced; no wide-libc fallback)"`) — narrow snprintf literal, UTF-8; note
  FK's NameTypes.cpp side has its own parallel upgrade, coordinate the
  canonical id with FK/root.
Expected console firings for the mod's `str(pickup.PickupInventoryItem.ItemId)`:
`[LFB-M] fname-rev in='...'/len=... ok=1` (FName-shaped reads) and, when the
value reaches `print(...)`, `[LFB-M] print-widen ... ok=1`.

## Syntax evidence

prospero clang++ `-fsyntax-only`, flags from
`2026-10-08-l26-build-variants/l26-A/pass1/objects/<unit>.command.json` with
`-ivfsoverlay/-o/-MD/-MF` stripped,
`LLVM_CONFIG=.../ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`
(feeds the wrapper's bindir). No console contact; REST MODE respected; no --execute.

Edited TUs — rc=0, 0 errors:
- lua-lfb (LuaFNameBridge.cpp) rc=0
- lua-luafname (LuaFName.cpp) rc=0 — only pre-existing warnings (deprecated
  `GetComparisonIndex` unsigned-int conversion, lines untouched by FL)
- lua-luaftext (LuaFText.cpp) rc=0
- lua-library (LuaLibrary.cpp) rc=0

Header dependents re-checked (all include LuaFNameBridge.hpp via LuaUObject.hpp
or directly) — 37/37 rc=0, 0 errors: lua-custom-property, lua-lfb, lua-library,
lua-luaaactor, lua-luacustomproperty, lua-luafname, lua-luafoutputdevice,
lua-luafsoftobjectpath, lua-luaftext, lua-luafurl, lua-luafweakobjectptr,
lua-luamodref, lua-luatarray, lua-luathreadid, lua-luatmap, lua-luatset,
lua-luatsoftobjectptr, lua-luauclass, lua-luaudatatable, lua-luauenum,
lua-luaufunction, lua-luauinterface, lua-luauobject, lua-luauscriptstruct,
lua-luaustruct, lua-luauworld, lua-luaxarrayproperty, lua-luaxboolproperty,
lua-luaxdelegateproperty, lua-luaxenumproperty, lua-luaxfieldclass,
lua-luaxinterfaceproperty, lua-luaxobjectproperty, lua-luaxproperty,
lua-luaxstructproperty, luamod, unreal-nametypes, unreal-uobjectglobals.

## Notes / interpretation decisions

- "hex units (first 8)": forward refusals render the first 8 raw input BYTES
  (the offending payload is UTF-8 there); reverse refusals render the first 8
  UTF-16 units. Both labeled `units=`.
- `push_wide_to_lua` was mapped to the `fname-rev` family (name-shaped wide
  text; same precedent as FM mapping name-shaped wide buffers to
  `fname-narrow`). It previously logged nothing.
- The old once-per-process `[LFB] push refused:` overlong notice in
  `push_wide_to_lua` is superseded by the family refusal channel (that literal
  was NOT a declared manifest marker; removal is manifest-safe).
- LuaLibrary's `ensure_str` at line 139 (exported-function error handler) and
  the narrow-literal status strings are class (c) log/device text — left
  upstream per Claude item 9 (helpers stay for non-Lua consumers).
- FString/FUtf8String/FAnsiString USERDATA surface lives in the port-tree
  header `LuaUnrealString.hpp` (empty setup_metamethods, `:ToString` via
  `to_utf8_string`) — documented for FM/root in FL-CONVERSION-INVENTORY.md
  §2; not edited by FL.
