# agent-FD: Lua<->engine string marshaling audit + FString/FText extension (Claude Q1)

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/`. Date: 2026-10-10. No
console contact, no `build_variants.py --execute`, no CHANGES.json edits
(root syncs). All new call-site paths are `#ifdef __PROSPERO__`-guarded with
the upstream code preserved verbatim under `#else` (host behavior stays
upstream; the port's platform headers mandate the Prospero target, so the
`#else` arms are dead on this toolchain by construction).

## 1. EY audit result (task item 1 verification — all present, byte-intact)

All 18 agent-EY `to_fname`/`.string()`/`.view()` sites re-verified present in
the candidate tree (this audit did not modify any of them):

- `candidate/src/LuaMod.cpp`: :1720 :1782 :1831 :1886 :2275 :2707 :2715 :2769
  :2868 :2937 :2977 :3908 :3961 :4006 :4099 :4149 :4189 :5629
- `candidate/include/LuaType/LuaUObject.hpp`: :545 (GetFullName
  `push_wide_to_lua`), :603 (GetProperty key), :813 (`prepare_to_handle`
  __index/__newindex member-name lookup)
- `candidate/src/LuaUObject.cpp`: :2321-pre-edit (IsA string overload, now
  shifted; still present)
- `candidate/src/LuaFName.cpp`: :75 (FName ctor) and :143 (FName:ToString)
  — LuaFName.cpp untouched by FD (sha below proves it)

FindFirstOf / FindAllOf / StaticFindObject / LoadAsset / FindObject(s) /
RegisterHook name args: all covered by the above and confirmed routed.
`LuaLibrary.cpp` and `CustomProperty.cpp` contain no Lua<->engine string
marshaling (LuaLibrary's `Unicode::DecodeUtf8` sites are the logwriter F4
output-payload widening, already the explicit hand codec, not engine
marshaling; CustomProperty.cpp is array-property plumbing) — both untouched.

## 2. Site inventory (newly routed by FD; file:line FINAL, direction, before -> after)

| file:line | direction | before -> after |
|---|---|---|
| `candidate/src/LuaUObject.cpp:1885-1904` (push_nameproperty Set) | Lua string -> FName param/property | was: FName-userdata-only (`get_userdata<LuaType::FName>` threw on plain strings, so the mod's `db:GetInteractablePickupIDActorPath(id)` with a Lua string id could never marshal) -> added Prospero string branch: `Bridge{data,size}.to_fname(FNAME_Add)` (slot-2 ctor; refusal -> NAME_None + seam notice) + `fname-param` marker; userdata branch unchanged |
| `candidate/src/LuaUObject.cpp:1936-1952` (push_textproperty Set) | Lua string -> FText param/property (SetText-adjacent) | was: FText-userdata-only -> added Prospero string branch: `Bridge::to_ftext()` (hand UTF-16 -> `FString::AppendChars` -> engine `Conv_StringToText`); refusal -> empty FText + notice |
| `candidate/src/LuaUObject.cpp:1984-2005` (push_strproperty Set) | Lua string -> FString param/property (THE UFunction string-arg path: `__call` param fill routes string params through `Operation::Set` pushers) | was: `ensure_str(lua_string).c_str()` + `FromCharTypePtr` + `FString(TCHAR*)` Strlen/NUL-scan ctor -> `Bridge::to_fstring()` (hand UTF-8->UTF-16 -> `AppendChars`, no Strlen); refusal -> loud `throw_error` after family notice |
| `candidate/src/LuaUObject.cpp:385-393` (UFunction Out-param table key) | engine FName -> Lua string (GetName return read) | was: `to_string(param->GetName())` (Helpers codec) -> `param->GetFName()` -> `fname_to_narrow` (pool decoder -> hand UTF-16 -> UTF-8) -> `add_key` |
| `candidate/src/LuaUObject.cpp:774-800` (convert_lua_table_to_struct, 2 sites) | engine name -> Lua table-lookup key (struct field names; mis-conversion = silent field-miss) | was: `to_utf8_string(m_name)` / `to_utf8_string(field->GetName())` -> `wide_to_narrow` on the StringType units (custom-property names are bridge-built; GetName is pool-decoder-built) |
| `candidate/src/LuaUObject.cpp:890-916` (convert_struct_to_lua_table, 2 sites) | engine name -> Lua table key (RETURN direction: struct -> Lua table) | same `wide_to_narrow` routing as above |
| `candidate/src/LuaFText.cpp:81-99` (FText __call ctor) — NEW candidate file | Lua string -> FText (`FText("test")`, the mod-menu SetText source) | was: `ensure_str(lua.get_string())` + `Unreal::FText(StringType)` -> `FText(const CharType*)` -> `FString(TCHAR*)` Strlen seam -> `Bridge::to_ftext()`; L22Diagnostics try/catch wrappers preserved |
| `candidate/src/LuaFText.cpp:150-175` (FText __eq) | FText -> FText compare (no narrow) | was: `FText::ToString() == FText::ToString()` — `FText::ToString` returns `StringType(*ToFString())`, the `basic_string(const wchar_t*)` PREBUILT-4-byte-libc++ ctor seam -> both sides `ToFString()` (engine `Conv_TextToString`) + hand unit loop compare, no StringType construction |
| `candidate/src/LuaFText.cpp:189-197` (FText:ToString member) | engine FText -> Lua string (return read; the mod's `str()` helper calls `v:ToString()`) | was: `to_string(*fstring)` (Helpers codec) -> `push_ftext_to_lua` (engine `Conv_TextToString` -> hand UTF-16 -> UTF-8 -> `lua_pushlstring`); `FText::ToString` never called |
| `candidate/src/LuaMod.cpp:3832-3841` (RegisterConsoleCommandGlobalHandler) | Lua string -> engine-name-shaped StringType (console dispatch map key matched against engine console names) | was: `ensure_str(lua.get_string())` -> `Bridge{...}.string()` (EY's `.string()` shape/precedent) |
| `candidate/src/LuaMod.cpp:3876-3884` (RegisterConsoleCommandHandler) | same | same |

Deliberately NOT routed (disclosed, with reasons):
- FString userdata `:ToString`/`Append`/`Find`/... members in port header
  `ue4ss/include/LuaType/LuaUnrealString.hpp` — header-inline port file NOT
  in FD's editable set (uses seam-fixed `to_utf8_string`/`ensure_str`, so
  stride-correct, but not bridge-routed). Owner can adopt
  `push_fstring_to_lua` / `Bridge::to_fstring` one-for-one.
- throw_error/Output::send diagnostic payloads (`LuaUObject.cpp:351-352`
  :418 :449 :922-923 :1010 :1026 :1686 :1690(pre-shift) :2541 :2666;
  `LuaMod.cpp:386 :583 :1072 :1096 :1119 :1129 :1577 :2030 :2154 :3317+` and
  the pre-existing `__PROSPERO__` L25 diagnostics) — display text, not
  marshaling; seam-fixed codec, per EY's item-9 line.
- `push_utf8strproperty` / `push_ansistrproperty` Set — byte-wise narrow
  copies (no wide stride hazard, no conversion exists to route).
- UFunction param-table metadata listings in `LuaMod.cpp` (the mod-facing
  routes are all converted; unchanged from EY's disclosure).

## 3. Bridge additions (`include/LuaFNameBridge.hpp` + NEW `candidate/src/LuaFNameBridge.cpp`)

- `Bridge::to_fstring() -> Unreal::FString` (decl hpp:430): hand UTF-16
  units appended into the FString's OWN CharType array via
  `FString::AppendChars` — header-inline `TArray::AddUninitialized` +
  `std::memcpy` + terminator, the engine allocator. NOT the
  `FString(const StringType&)` ctor (ensure_str_as) nor `FString(const
  TCHAR*)` (Strlen NUL-scan). static_assert TCHAR == CharType.
- `Bridge::to_ftext() -> Unreal::FText` (decl hpp:440): FString fill exactly
  as above, then the header-inline in-port `FText(const FString&&)` shape ->
  `construct_with_string` -> `UKismetTextLibrary::Conv_StringToText`
  (engine call, verified `UE_BEGIN_NATIVE_FUNCTION_BODY("/Script/Engine.
  KismetTextLibrary:Conv_StringToText")`). Never the `FText(const
  CharType*)` seam.
- `push_fstring_to_lua(L, const FString&)` (decl hpp:709): `Len()` +
  `GetCharArray().GetData()` (header-inline, no NUL scan) -> hand UTF-16 ->
  UTF-8 -> `lua_pushlstring`, bounded by kMaxPushUnits (4096).
- `push_ftext_to_lua(L, const FText&)` (decl hpp:710): `GetTextData()` null
  -> empty string; else `ToFString()` (engine `Conv_TextToString`) then the
  shared reverse loop. `FText::ToString()` deliberately NEVER called (its
  `StringType(const CharType*)` return resolves to the prebuilt 4-byte-wchar
  libc++ ctor on the short-wchar target).
- `fname_to_narrow(FName, char*, cap)` (decl hpp:711): pool decoder
  (`FName::ToString`, agent ER) -> hand UTF-8 into caller buffer (Lua table
  keys that need a narrow c-str).
- `wide_to_narrow(const CharType*, count, char*, cap)` (decl hpp:717): same
  reverse read for StringType payloads (GetName results, bridge-built
  custom-property names).
- The header forward-declares `Unreal::FString`/`Unreal::FText` and stays
  free of `<Unreal/FString.hpp>`/`<Unreal/FText.hpp>`/Helpers includes; all
  real-type definitions live in the new .cpp only.
- No wide libc anywhere new: no wcslen/wmemcmp/swprintf/mbrtowc, no
  std::wstring construction, no fmt wide API on any new path (grep-audited;
  only `std::memcpy`/`std::snprintf` narrow + `lua_*` C API + engine calls).

## 4. Markers declared (task rule: one per newly routed family, first N=10)

Format (ASCII, narrow .rodata, rendered narrow-first then widened by the
bridge's own loop, `Output::send`):

- `[LFB-M] fstring-fwd in='<utf8>'/len=N ok=B` — to_fstring
- `[LFB-M] ftext-fwd in='<utf8>'/len=N ok=B` — to_ftext
- `[LFB-M] fstring-rev in='<utf8-out>'/len=N ok=B` — push_fstring_to_lua
- `[LFB-M] ftext-rev in='<utf8-out>'/len=N ok=B` — push_ftext_to_lua
- `[LFB-M] fname-narrow in='<utf8-out>'/len=N ok=B` — fname_to_narrow /
  wide_to_narrow
- `[LFB-M] fname-param in='<utf8>'/len=N ok=B` — push_nameproperty string
  branch (to_fname already logs [LUA-FN]; this marker proves the NEW SITE)

Refusal discipline (same as to_fname; one line per family, then empty value
forever after): `[LFB-M] <family> refused reason=<embedded-nul|invalid-utf8|
incomplete-utf8|non-scalar|overlong|bad-utf16> len=N at=N (yielding empty; no
wide-libc fallback)` — then NAME_None (fname-param, via to_fname's seam
notice), empty FString (fstring-fwd; the strproperty binding additionally
throws its normal loud Lua error), empty FText (ftext-fwd), empty Lua string
(reverse families), out[0]=0 (narrow family). Binary grep targets:
`[LFB-M] ` (wide .rodata via the send path) and the narrow format literals
`[LFB-M] %s in=` / `[LFB-M] %s refused reason=`.

## 5. Files touched / created (sha256)

| file | sha256 | status |
|---|---|---|
| `typescan-r4/include/LuaFNameBridge.hpp` | 9d3c0c646d658e786bf2e16ce0a3115b4f2a1bc880db37ed9395c2f0f7ff68ab | edited (735 lines; EY content preserved, additions only) |
| `typescan-r4/candidate/src/LuaFNameBridge.cpp` | 48d3c9044240c2e5fed56ac2b69be33722c63d1e1a7aa022bbddf76dcde12f0e | NEW (187 lines) — out-of-line families, explicitly authorized |
| `typescan-r4/candidate/src/LuaFText.cpp` | 61250bfd77c176649c788d5a7cabc8dca80479afe4664cd4b339df53d2c89596 | NEW candidate (217 lines) |
| `typescan-r4/candidate/src/LuaUObject.cpp` | e3a24509aa68c769ede4bbb946f20b4d99621c8513d5bb21b277a70a0e8e63d6 | edited (11 routed points) |
| `typescan-r4/candidate/src/LuaMod.cpp` | 61f6cb98c5162868ef2b55882b8b79d9963aa69bcfed46346b832ab7641116b8 | edited (2 routed points) |
| `typescan-r4/candidate/src/LuaFName.cpp` | dd7a86499e2868c2ecab8f49345e82757c619a76dd7128f04999d96d3dc7fb55 | UNTOUCHED (verified) |
| `typescan-r4/candidate/src/LuaLibrary.cpp` | 2cada608acf79b9be73afa2c1ce3fa4d7cbec6febefb9456be19f267ac049454 | UNTOUCHED (verified) |
| `typescan-r4/candidate/src/CustomProperty.cpp` | 66be0042102f1140d38bb0f305a21a3b6791394c51a31dd0b4e02471e33ef938 | UNTOUCHED (verified, no marshaling present) |

## 6. Compile + suite evidence

- **Target syntax-only, REAL prospero-clang++ (LLVM 21 via
  `LLVM_CONFIG=.../LLVM-21.1.8-macOS-ARM64/bin/llvm-config`), `__PROSPERO__`
  on (builtin), full archived lfb argv per unit (218 flags incl.
  `-ivfsoverlay l26-A.lfb-203415/headers.vfs.json` + every -I), `-c -o -MD
  -MF` swapped for `-fsyntax-only`:** exit 0 for ALL six —
  `lua-luauobject` (candidate/src/LuaUObject.cpp), `luamod`
  (candidate/src/LuaMod.cpp), `lua-luafname` (candidate/src/LuaFName.cpp),
  `lua-luaftext` (candidate/src/LuaFText.cpp), `lua-library`
  (candidate/src/LuaLibrary.cpp), `lua-lfb` (candidate/src/
  LuaFNameBridge.cpp, luamod include set). Only pre-existing
  `-Wdeprecated-declarations` warnings (GetComparisonIndex), 0 errors.
  Runner: /tmp/fd_syntax_check.py (host-local; reproducible from the
  l26-A.lfb-203415 command.json receipts).
- `-U__PROSPERO__` host-arm check is NOT possible on this toolchain (the
  port's ProsperoPlatform headers `#error` without the Prospero target);
  the `#else` arms are verbatim upstream code that compiled in the lfb base.
- **EY suite re-run against the EXTENDED header**:
  `python3 tests/run_lua_fname_bridge.py` -> `lua-fname-bridge: ALL PASS`
  (green + mismatch x plain + ASan/UBSan) — the header extension is
  host-harness clean and to_fname behavior unchanged.
- Behavioral deltas disclosed: (a) push_nameproperty/push_textproperty now
  ACCEPT plain Lua strings (previously an error path) — required for the
  mod's string-id call shapes; (b) invalid-UTF-8 Lua strings on FString
  property Set now throw the binding error instead of lossy-converting;
  (c) FText __eq no longer routes through FText::ToString (semantically
  identical for real text, and it kills a live corruption seam).

## 7. Root sync notes (CHANGES.json is root-owned)

New/changed rows needed: (1) NEW `candidate/src/LuaFText.cpp` keyed on the
lua-luaftext unit's ACTUAL original identity
`outputs/2026-10-08-l22-runtime/diagnostics/src/LuaFText.cpp` (sha
af7c9bd05b801dfb2bc4152730f072f92fec8bd1d21961f3240cef84336df193, verified;
NOT the port-root path — the lfb unit compiles the l22-diagnostics copy); (2) NEW
`candidate/src/LuaFNameBridge.cpp` (no original — companion to the
added-header row `include/LuaFNameBridge.hpp`); (3) refreshed
candidate_sha256 for LuaUObject.cpp, LuaMod.cpp, include/LuaFNameBridge.hpp.
The lua-lfb compile above proves the new unit compiles under the existing
include set, so it can join the build as a new unit row without -I changes.
