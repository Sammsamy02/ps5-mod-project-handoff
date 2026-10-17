# agent-EY: ONE hand-written Lua→FName converter (Claude bypass directive)

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (consumption law; CHANGES
rows synced). Date: 2026-10-09. No console contact, no packaging, no twin
build, no upload (root uploads zzdiag v8).

## 1. The converter (the whole mechanism)

New added header `include/LuaFNameBridge.hpp` (namespace
`RC::LuaType::LuaFNameBridge`, sha in `CHANGES.json added_header_hashes`).
Bypassed for the Lua path: `std::wstring`, `Helpers/String.hpp`
(ensure_str/to_wstring/ensure_str_const), `Helpers/Utf16Utf8.hpp`
(Unicode::DecodeUtf8/EncodeUtf8), and every wide libc call
(wcslen/wmemcmp/swprintf family) — none appear anywhere in the header or the
routed call sites. The helpers themselves stay for all non-Lua consumers
(Claude item 9).

Core forward shape (Claude item 1, verbatim intent):

```cpp
size_t n = 0;
const char* s = lua_tolstring(L, idx, &n);            // Lua C API only
// hand UTF-8 -> UTF-16 loop into CharType units_[kMaxUnits /*1023*/ +1]
// (lead ranges C2..DF/E0..EF/F0..F4, overlong minimums, scalar check,
// surrogate pairs when sizeof(CharType)==2, code-point-per-unit when 4);
// NUL-terminated; embedded NUL / invalid / incomplete / non-scalar /
// overlong inputs REFUSE loudly (NotifyFNameSeamRefusedOnce) -> NAME_None.
// Consumption parity: lua_remove(L, idx) exactly like
// LuaMadeSimple::Lua::get_string (bindings parse positionally).

// slot-2 native ctor DIRECT (no FName(StringType) seam, no FunctionAddressOverride):
const FName made = FName::ConstructorInternal(units_, find_type);  // image 0x23D590
```

- `Bridge(lua_State*, int idx=1)` — stack source; `Bridge(const char*, size_t)`
  — memory source (binding-stored strings). One class, one hand loop.
- `to_fname(EFindName)` — `is_ready()` gate + `NotifyFNameCtorUnreadyOnce()`
  parity with the seam, then the direct ctor call, then the first-20-uses log.
- `view()` (StringViewType, zero-copy), `string()` (engine StringType fill via
  resize+indexed writes — see §6 discovery), `utf8()/utf8_len()/input_len()`.
- Reverse (Claude item 4): `push_fname_to_lua(L, FName)` = `FName::ToString()`
  (the in-port pool decoder, agent ER, decoder-first) → `utf16_to_utf8` hand
  loop → `lua_pushlstring`. `push_wide_to_lua(L, units, n)` for arbitrary
  engine wide text (GetFullName). Bounded 4096 units; overlong pushes empty +
  one loud `[LFB] push refused` line; bad surrogates push empty.
- Logging (Claude item 3): first 20 `to_fname` uses, ONE line each:
  `[LUA-FN] in='<utf8>' len=N id=0x{:X} decoded='<pool decode>' OK/MISMATCH`
  — `decoded` via `FNamePoolDecoder::try_tostring` (the proven decoder in
  NameTypes.cpp; newly declared RC_UE_API in NameTypes.hpp:262, definition
  pre-existed TU-local), OK = decoded matches input case-insensitively
  (hand ASCII fold, unit counts equal). The line renders narrow-first
  (snprintf) and widens for `Output::send` through the bridge's OWN forward
  loop — the log route uses no banned conversion either.
- Composition marker: `[LFB] lua-fname-bridge composed: hand utf8<->utf16,
  slot-2 ctor direct` — once per process, a UTF-16LE literal in .rodata
  (PRX grep target, §5).

## 2. Every routed entry point (file:line, final state)

All paths absolute under
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/`.

`candidate/src/LuaMod.cpp` (18 sites; `using`-free qualified `LuaType::LuaFNameBridge`):
- :1720 StaticFindObject overload #1 (`name.view()`; per-part FNAME_Find
  inside StaticFindObject stays engine-path per item 9)
- :1782 StaticFindObject overload #2 P3 (`param_name = name.string()`)
- :1831 FindFirstOf — `to_fname(FNAME_Find)`; Prospero Add on NAME_None (CP
  law), host `to_fname(FNAME_Add)` (upstream law) → `FindFirstOf(FName)`
- :1886 FindAllOf — same construction → `FindAllOf(FName, out)`
- :2275 UnregisterHook path name (`hook_name.string()`)
- :2707/:2715 RegisterCustomProperty Name / BelongsToClass (memory source)
- :2769 RegisterCustomProperty OffsetInternal.Property
  (`to_fname(FNAME_Add)`)
- :2868 NotifyOnNewObject class name
- :2937 RegisterCustomEvent event name
- :2977 UnregisterCustomEvent event name
- :3908 LoadAsset asset path (`to_fname(FNAME_Add)`)
- :3961/:4006 FindObject string ClassName / ObjectShortName
- :4099 FindObject positional Name (memory source, `.string()`)
- :4149/:4189 FindObjects string ClassName / ObjectShortName
- :5629 RegisterHook path name

`candidate/include/LuaType/LuaUObject.hpp`:
- :813 `prepare_to_handle` — __index/__newindex member-name lookup
  (properties AND UFunctions): `member.to_fname(FNAME_Find)` (the l26
  postmortem FNAME_Find law preserved) + `member.string()` for the
  custom-property list + `member.view()` in the null-object error line
- :545 `GetFullName` — reverse: `push_wide_to_lua` (was
  `to_string(GetFullName()).c_str()`)
- :603 Reflection `GetProperty` — `property_key.to_fname(FNAME_Find)` at
  stack index 2

`candidate/src/LuaUObject.cpp`:
- :2321 `IsA` string overload — StaticFindObject full-name arg
  (`full_name.view()`)

`candidate/src/LuaFName.cpp` (NEW candidate; row keyed on the port-root
lua-luafname identity the l25-A BASE consumed; original staged under
`original/src/LuaFName.cpp` per the tree's new-file stub convention):
- :75 the `FName()` constructor exposed to Lua (Call metamethod string
  branch) — is_string tested BEFORE construction so the stack shape matches
  the original get_string flow; `name.to_fname(find_type)` (upstream
  FNAME_Add default kept; empty string keeps the index-0 integer branch)
- :143 `FName:ToString` member — reverse `push_fname_to_lua` (was
  `to_string(ToString())`); this is the `str()` path the mod reads ItemId
  through (ItemId is an FNameProperty → push_nameproperty → FName userdata)

Not routed (out of the engine-name class, disclosed): console-command map
keys, FString globals/StrProperty Set (FString, not FName), FPackageName
checks, UFunction param-table metadata listings (table-key API needs narrow
c-str; the named mod-facing routes are all converted).

Behavioral note: the Lua FindXOf bindings now call the FName overloads
directly, bypassing the DA verified-query round-trip/string-walk fallback
(that layer stays for non-Lua callers); the bridge's own per-use
construction check (§1 logging) is the Lua-path verification. Seam fixes in
NameTypes/String.hpp/Utf16Utf8.hpp untouched (Claude item 9); NameTypes.cpp,
UObjectGlobals.cpp, TypeChecker.cpp byte-identical to their CHANGES pins.

## 3. Init self-test + PRX composition guard (Claude items 6-7)

`candidate/src/LuaMod.cpp` L23GameThreadProbe game-thread self-test chunk
(:140, `k_seam_chunk`) extended from 4 to 7 Lua-originated probes on the same
names — FindFirstOf ×2, StaticFindObject ×2, **FindAllOf('PlayerController')**
and **FName('PlayerController')** / **FName('ItemDatabaseSubsystem')** new —
each pcall'd through the ACTUAL registered bindings (now bridge-routed), 14
return values, `[ST] lua-{ffo,sfo,fao,fname} ...` PASS/FAIL lines at
:163-170. Runs on the game thread before anything else (evidence only,
never throws, never blocks the drain). The previous build's Lua self-test was
composed out — the grep guard below proves composition after every build.

PRX grep strings (run on the built+decrypted-off PRX; all must be present):
- UTF-8 (narrow .rodata):
  - `[LUA-FN] in=` (bridge log format literal, snprintf in the header)
  - `pcall(FindAllOf,'PlayerController')` (chunk literal)
  - `pcall(FName,'PlayerController')` (chunk literal)
  - `[LFB] lua-fname-bridge composed` is NOT narrow — see UTF-16
- UTF-16LE (wide .rodata):
  - `[LFB] lua-fname-bridge composed` (the once-per-process marker literal)
  - `lua-fname use id=0x` (the log-render fallback literal)
  One-liner for the UTF-16LE side:
  `python3 -c "import sys;d=open(sys.argv[1],'rb').read();print('LFB-utf16', '[\x00L\x00F\x00B\x00]' in d);print('LFB-utf8', b'[LFB] lua-fname-bridge composed' in d or b'[LUA-FN] in=' in d)" <prx>`
  (UTF-16 marker bytes are ASCII interleaved with NULs; also check the
  `[ST] lua-fname` wide literals the same way.)

## 4. Host test with the REAL converter (Claude item 5)

`tests/actual_lua_fname_bridge.cpp` + `tests/run_lua_fname_bridge.py`
(receipt `results/LUA-FNAME-BRIDGE.json`). REAL: the entire
`include/LuaFNameBridge.hpp` included verbatim (the code under test), REAL
Lua 5.5 interpreter (homebrew, same recipe as the EV string-seam suite),
REAL `<String/StringType.hpp>` (CharType = wchar_t = 2 under
**-fshort-wchar**). STUB: `<Unreal/NameTypes.hpp>`/`<DynamicOutput/Output.hpp>`
shadowed from `tests/lfb-stubs/` — a recording slot-2 ctor (Claude's "stub
ctor") + a recording pool decoder that decodes the RECORDED ctor units back
(the OK/MISMATCH verdict is a real round trip, not hardcoded).

Suites green + mismatch × plain + ASan/UBSan, ALL PASS:
- `luaL_dostring "return 'PlayerController'"` → Bridge → **exactly 16 UTF-16
  code units at the stub ctor**, content == u"PlayerController", mode ==
  FNAME_Find, id nonzero, buffer NUL-terminated
- `[LUA-FN] in='PlayerController' len=16 id=0x10 decoded='PlayerController' OK`
  (mismatch suite: decoder lies by 'X' → `decoded='PlayerControllerX' MISMATCH`)
- consumption parity: [string, integer] → Bridge removes the string, integer
  slides to index 1 (refusals also consume)
- multi-byte: 'café' (0xC3 0xA9) → 4 units, unit[3]==0xE9; U+1D11E
  (0xF0 0x9D 0x84 0x9E) → surrogate pair D834 DD1E
- reverse: `push_fname_to_lua` → `lua_tolstring` == "PlayerController" (16
  bytes); `push_wide_to_lua({D834,DD1E})` → 0xF0 0x9D 0x84 0x9E
- refusals: embedded NUL / 2000-char overlong / 0xC3 0x28 invalid — NAME_None,
  ctor call count UNCHANGED, seam notices fired ("embedded-nul"/"overlong")
- memory-source ctor (FindObject binding shape) — 16 units, FNAME_Add
- first-20-uses bound: 25 more uses add ≤ 20 `[LUA-FN]` lines
- `[LFB] lua-fname-bridge composed` exactly once

## 5. zzdiag menu dry run (Claude item 8) — staged, root uploads

`outputs/2026-10-08-l25-prep/zzdiag-v8/` — `main.lua` (2977 B, sha256
67de36564763cb36e4331ba9cfc79fa890bac47e818082c5d3f81d01e581be15, luac -p
clean) + `LOADLIST-ROW.txt` + `loadlist-zzdiag-v8.txt` (14-row reference;
zzdiag main.lua row 2487→2977, TAB-separated, size-exact MFA law).

Code path: NO keybind. `LoopInGameThreadWithDelay(1000, cb)` polls on the
game thread from main.lua load; once `FindAllOf("PlayerController")` yields a
valid controller (main menu up; the pump is armed and ticking by then) the
four steps run ONCE and `cb` returns false (loop stops); 240-poll (~4 min)
timeout logs `W0 timeout` and stops. Steps (all pcall'd; **never
AddToViewport**), PASS/FAIL per step via `[LFB8]` tokens:
- M1 `FindAllOf("PlayerController")` count + `pcs[1]:IsA("/Script/Engine.PlayerController")`
  (the IsA string overload is itself bridge-routed)
- M2 `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")`
- M3 `widget_lib:Create(pc, StaticFindObject("/Script/UMG.UserWidget"), pc)`
- M4 `StaticConstructObject(StaticFindObject("/Script/UMG.TextBlock"), pc, FName("LFB8TextBlock"))`
  + `tb:SetText(FText("test"))`
Every name in the mod flows through the bridge; the console log should show
the matching `[LUA-FN] in=...` lines for the same names. Loadlist row
(verbatim, size-exact): `file<TAB>lua_mod<TAB>ue4ss/Mods/zzdiag/Scripts/main.lua<TAB>2977`
+ the `enabled.txt 0` row.

## 6. Compile + suite results

- **Target compile (real prospero-clang++, archived l24-build-r2 argv)**:
  `results/EY-TARGET-COMPILE.json` + `ey_target_compile.py` + per-unit logs
  — exit 0 for all five touched units: `luamod`, `lua-luauobject`,
  `lua-luafname`, `unreal-nametypes`, `unreal-uobjectglobals`, over the new
  `results/headers-ey.vfs.json` overlay (routes BOTH header replacements —
  `Unreal/NameTypes.hpp` AND `LuaType/LuaUObject.hpp` — to the candidates;
  E3 law) with tree `include/` + `candidate/include` first so
  `<LuaFNameBridge.hpp>` resolves for every unit.
- **New suite**: `run_lua_fname_bridge.py` — ALL PASS (§4).
- **Existing suites re-run, ALL PASS**: `run_ev_seam.py` (green+red ×
  plain+ASan — NameTypes.hpp region extractor unaffected by the new
  declaration), `run_et_verify.py` 16/16 (incl. er-pooldecoder
  happy/refuse/race/truncation × 2 modes), `run_ev_rerun.py`,
  `run_ev_target.py` (luamod + unreal-uobjectglobals exit 0), and the
  CHANGES contract verified inline for all 66 schema-complete rows.
- **Pre-existing, disclosed (NOT this work)**: `tests/run.py` →
  `test_scan.py` still fails — it extracts the l24 scan body against the l25
  TypeChecker guard API, exactly the NOT-YET-DONE condition documented in the
  l25 scope. TypeChecker.cpp / UObjectGlobals.cpp / NameTypes.cpp candidate
  shas match their existing CHANGES pins byte-exactly (verified).
- **Host discovery (ASan)**: the prebuilt host libc++
  `basic_string<wchar_t>(ptr, count)` overreads under -fshort-wchar (16-byte
  read from a 10-byte literal, proven in this harness) — so even the
  (ptr,count) ctor is unsafe on the host; the stub AND `Bridge::string()`
  build StringType with resize+indexed-writes only (EV discipline extended
  past push_back).

## 7. CHANGES sync

- `candidate_sha256` refreshed in BOTH `changes[]` and `files[]` for
  LuaMod.cpp, LuaUObject.cpp, LuaUObject.hpp, NameTypes.hpp.
- New rows (changes[] + files[]) for `candidate/src/LuaFName.cpp` keyed on
  the port-root lua-luafname identity (original_sha256 = port-root file; the
  tree's parallel originals-manager staged `original/src/LuaFName.cpp` as a
  new-file stub — followed).
- `include/LuaFNameBridge.hpp` added to `added_headers` + hashed.
- New top-level scope key `scope_ey_lua_fname_bridge` (full disclosure).
- Concurrent-agent note: a parallel changes-sync bot appended its own 4 rows
  (no `copied_original` field, note-styled) covering the same two new files;
  left in place untouched, disclosed here. Its LuaFName.cpp candidate hash
  matches mine. All 66 schema-complete rows verify against the
  ev-changes-sync contract; the EV checker script itself skips nothing and
  will trip on the bot's 4 rows until their owner completes them.
