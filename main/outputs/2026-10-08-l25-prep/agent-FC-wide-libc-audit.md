# agent-FC — runtime-wide wide-libc / std::wstring census (l26 build)

Date: 2026-10-09 (CDT). READ-ONLY audit; no source, build, or console changes.
Evidence base: the l26-A composition object
`outputs/2026-10-08-l26-build-variants/l26-A/pass1/runtime-l26-A.o`
(120,292,992 B, Oct 9 19:09 build; twin pass-2 byte-identical), its 199 fresh TU
objects in `pass1/objects/`, the link order in `pass1/LINK-COMMAND.json`, the
static archives named therein, and the port/candidate sources selected by
`SOURCE-SELECTION.json` / `headers.vfs.json`.

NOTE ON ARTIFACT AVAILABILITY: mid-audit, the parallel cleanup moved the live
`l26-A` tree into
`outputs/2026-10-08-l26-build-variants/archive-20261009T151128Z/l26-A`
(880 objects survive there). All per-TU numbers below were captured from the
Oct 9 19:09 objects before the move; the archived copy was used for
relocation-level spot checks and matches.

## 0. Premise correction (important — verified by disassembly)

"Any out-of-line std::wstring member resolves to the PREBUILT library" is **not
true for the l26 static link**. The composition links
`ps5-payload-sdk/target/lib/libc++.a` (sdk-v0.43), whose `string.cpp.o`
explicit-instantiation members were compiled **with 2-byte wchar_t**:

- `_ZNSt3__1...basic_stringIw...9push_backEw` in libc++.a/string.cpp.o:
  `cmpl $0x14` short-cap check (20 = 10 elems x2), `addl $0x2`, `movl $0xa`
  (__min_cap 10) — 2-byte arithmetic.
- `...6__initEPKwm`: `leal (%rbx,%rbx)` (size*2), `addq $0x2`, `cmpq $0xb`
  (11-elem short cap) — 2-byte.

All 68 out-of-line wstring member symbols (60 weak + 8 strong) are present in
the composed object and ALL are 2-byte. 149 fresh TUs call them (top:
`D1Ev` dtor 148 TUs, `__initEPKwm` 143, copy-ctor 86, `assignEmw`/`__initEmw`
77 each, `resizeEmw` 40) — these are **stride-safe** in l26. The prior
Lua→FName corruption came from the *dynamic* imports below plus the (now
replaced) wstring_convert/codecvt path, not from these members.

The 4-byte hazard surface in l26 is exactly the **dynamic imports from the
console's libSceLibcInternal** (confirmed binding in
`2026-10-08-l26-package-A/pass-1/EXACT-IMPORT-IDENTITIES.json`, 207 entries),
plus one trap: the "rebuilt" `fixed-deps-native-rebuild-1/libc.a` is 2-byte
code, but its locale wrappers (`mbrtowc_l`, `mbsrtowcs_l`, `wcrtomb_l`,
`wcscoll_l`, `mbtowc_l`, `___mb_cur_max_l` in `no-locale.o`) are **GOT thunks
that tail-call the plain console functions** (relocation evidence:
`.text.mbrtowc_l → mbrtowc-0x4` etc.). The whole locale/codecvt conversion
stack, including "internal" definitions, executes console 4-byte code.

## 1. Task 1 — wide-libc import census (composed object undefined symbols)

30 wide/locale-family undefined symbols out of 212 total, all bound to
libSceLibcInternal (4-byte wchar_t):

Wide proper (26): `wcslen wmemcmp swprintf vswprintf mbrtowc mbsrtowcs mbtowc
wcrtomb mbrlen towlower towupper wcstol wcstoll wcstoul wcstoull wcstod wcstof
wcstold wcscoll wcsxfrm fgetwc fputwc ungetwc iswdigit iswspace iswxdigit`
Locale-family (4): `setlocale strcoll strxfrm __mb_cur_max`

No `_ZN...basic_stringIw...` symbol is undefined in the composed object (0
undefined `_ZN` at all) — the wstring-member import family the task
hypothesized does not exist in l26 (see §0).

### Importers by origin

A. **Our fresh TUs (199 objects, per-object `llvm-nm -u`):** 144 TUs import at
least one wide function — `wcslen` 143 TUs, `wmemcmp` 90, `towlower` 9,
`iswspace` 3, `vswprintf` 2, `towupper` 2, `iswxdigit` 1+1, `iswdigit` 1.
Relocation-level attribution (archived objects): essentially all wcslen refs
come from one libc++ inline helper per TU,
`std::__1::__constexpr_wcslen` ← `char_traits<wchar_t>::length` (i.e. any
pointer-only `wstring`/`wstring_view` construction or traits length call);
wmemcmp likewise via `__constexpr_wmemcmp` ← `char_traits<wchar_t>::compare`.
`towlower` sits inside `RC::String::iequal<wchar_t>`'s per-unit lambda (value
pass, stride-safe). `vswprintf` is exactly
`RC::Unreal::FProsperoPlatformString::GetVarArgs(WIDECHAR*, ...)`.

B. **Retained parent objects (coherent-full-1 + luaraw):** `ini.o`,
`base-tokenizer.o` (wcslen wmemcmp); `luaraw-loslib.o` (setlocale — Lua
`os.setlocale`); `luaraw-lvm.o` (strcoll — Lua string collation).

C. **Pulled static-archive members (their INTERNAL calls import console
functions):**
- libc++.a `string.cpp.o` (2-byte code): imports `swprintf wcslen wmemcmp
  wmemchr wcstod wcstof wcstol wcstold wcstoll wcstoul wcstoull` — reachable
  via wstring find*(const wchar_t*)/compare/stod-family with NUL-terminated
  wide pointers (traits::length again).
- libc++.a `locale.cpp.o` (845 strong syms linked; pulled by `use_facet`/
  `locale::~locale` refs in ~80 of our TUs via `std::wstring_convert`/
  codecvt remnants and `std::locale` uses): imports `setlocale wcslen` +
  the `_l` family (which thunk to console: see §0).
- libc++.a `iostream.cpp.o`: imports `fgetwc fputwc ungetwc` (wcin/wcout
  `__stdinbuf<wchar_t>` machinery — never called by our code).
- rebuilt libc.a `no-locale.o`/`mbsnrtowcs.o`: plain-name console imports
  `mbrlen mbrtowc mbsrtowcs mbtowc wcrtomb wcscoll wcsxfrm wcsnrtombs
  strcoll strxfrm __mb_cur_max`.

Key sources for the seams (l26-selected candidate versions unless noted):
- `candidate/include/Helpers/String.hpp` — `to_wstring(std::string)` (FIXED:
  DecodeUtf8 + (ptr,count)); `to_string(std::wstring)` (FIXED: (ptr,count) +
  EncodeUtf8); `to_wstring/to_string(const wchar_t*)` (**UNFIXED:
  `std::wstring{pInput}` pointer-only → console wcslen**); `iequal` pointer
  overloads (:693-701, construct `wstring_view{bare_ptr}` → console wcslen);
  wstring_convert remnants are `#ifdef PLATFORM_WINDOWS` only (dead on
  Prospero). `#include <codecvt>` at :3.
- `candidate/include/Helpers/Utf16Utf8.hpp` — DecodeUtf8 FIXED (resize +
  operator[]); EncodeUtf8 is narrow-only (safe).
- `candidate/include/Unreal/NameTypes.hpp` + `src/NameTypes.cpp` — FName sized
  choke point + embedded-NUL refusal (EV fix, console-proven); ToString is the
  in-port pool decoder (no wide-libc).
- `unreal/include/Unreal/Core/Prospero/ProsperoPlatformString.hpp` —
  Strlen/Strcmp/Strcpy are hand loops (SAFE); `GetVarArgs(WIDECHAR*)` :155 →
  `::vswprintf` (CRITICAL); `Signed64/Unsigned64` :30-47 → `::wcstoll/
  wcstoull`; `Atod` :133 → `::wcstod` (WIDECHAR branches).

## 2. Task 2 — caller classification

### ON-LUA-PATH — between Lua input and FName/FString/FText (CRITICAL)
Primary seam (Lua string → FindXOf query → FName ctor) is BYPASSED by the EV
converter (DecodeUtf8 + sized choke point) — recorded, not re-litigated.
Residual edges still importing console wide functions:
1. `unreal-uobjectglobals` (candidate UObjectGlobals.cpp; towlower/wcslen/
   wmemcmp): DA verified-query uses `String::iequal(view, view)` — sized views
   are safe; the TU's wcslen/wmemcmp imports come from remaining pointer-based
   traits calls in StaticFindObject/FindFirstOf glue.
2. `lua-luauobject` (wcslen): member-name `FNAME_Find` resolver glue (CN) —
   names arrive via ensure_str (safe decode) but pointer-shaped FString/wstring
   interop sites still call traits::length.
3. `luamod` (towlower/wcslen/wmemcmp): Lua mod-name/keybind compare paths
   (`iequal` is safe; pointer traits edges remain).
4. `lua-luacustomproperty` + `lua-custom-property` (wmemcmp/wcslen): property
   list lookups keyed by strings coming from Lua.
5. `unreal-fstring` (iswspace + vswprintf + wcslen): FString parse/format
   helpers shared by input and output paths (see also output).
6. `unreal-nametypes` (wcslen): FName seam edges (ToString itself is the pool
   decoder — safe).

### ON-OUTPUT-PATH — engine string data → Lua push / logs (CRITICAL)
1. `unreal-flat-foutputdevice` (vswprintf/wcslen/wmemcmp):
   `FProsperoPlatformString::GetVarArgs(WIDECHAR*)` — every wide-formatted
   engine-log/Printf write; console vswprintf writes 4-byte-stride units into
   2-byte FString buffers → the interleaved-NUL garble (the "c o n s t r u c t
   o r" log class). This is the **live garbled-output writer** remaining in
   l26 (CL's logwriter fix covered the Lua mod-log path, not engine-side
   wide formatting).
2. `program-object-to-string` (wcslen): EI's ToString bind pushing engine
   strings to Lua.
3. `lua-luaftext` (wcslen/wmemcmp), `lua-luamodref`, `lua-luaaactor`,
   `lua-luauclass`, `lua-luauworld`, `lua-luatarray/tmap/tset`,
   `lua-luauenum/udatatable/uscriptstruct/ustruct` (wcslen each): Lua type
   ToString/GetFullName pushes.
4. `unreal-uobject`, `unreal-uclass`, `unreal-ufunction`, `unreal-ustruct`,
   `unreal-ffield`, `unreal-fproperty`, `unreal-src-property-*` (14 TUs),
   `unreal-src-versionedcontainer-unrealvirtual407..508` (24 TUs) — all
   (wcslen+wmemcmp): GetName/GetFullName/property-to-string machinery feeding
   both Lua output and diagnostics.

### INTERNAL-ONLY — logging/diagnostics/config (garble possible, lookups unaffected)
`ini-tokens` (iswdigit/iswxdigit/towlower — value-passed, safe), `settings` +
`SettingsManager.cpp` iequal (16 sites), `input-keydef` (towlower, safe),
`input-handler`/`input-queue` (wcslen), `program` (towlower/wcslen/wmemcmp),
`program-signatures` (wcslen/wmemcmp), `output-device` (wcslen),
`unreal-uobjectarray` (wcslen/wmemcmp — init type-scan string compares; the
same compares back the FindXOf string-fallback, so garble here would mislead
walk diagnostics), `unreal-unrealinitializer`, `unreal-hooks`,
`unreal-fmemory`, `unreal-signatures`, `native-jmap`,
`native-layoutbinding`, `native-programfactory`, `native-script-hook-thunks`,
`asmhelper`, `asset_registry`, `cppmod*`, `ue4ss_backend`, flat-API glue TUs
(canvaspanel, ukismetsystemlibrary, …) — wcslen/wmemcmp via traits edges.
These produce garbled diagnostics at worst; the 4-byte wcslen read yields
length≈len/2 (or overruns on odd literals) — garble, not lookup breakage,
EXCEPT where a diagnostic STRING is then compared (uobjectarray above).

### DEAD on console
1. SDK-generator family — `generator-sdkgenerator-common/generator/
   ueheadergenerator`, `generator-usmapgenerator-generator`,
   `csv_data_commands` (DataDumpers): towupper/towlower/iswxdigit/wcslen/
   wmemcmp — executed only on an SDK-dump hotkey that console runs never
   press.
2. `luaraw-loslib` setlocale — only if a Lua mod calls `os.setlocale` (none
   shipped does).
3. `luaraw-lvm` strcoll — Lua collation compare, effectively unused.
4. libc++.a `iostream.cpp.o` fgetwc/fputwc/ungetwc — wcin/wcout machinery;
   imported because the member got pulled, but no code path calls it.
5. `String.hpp` wstring_convert in `#ifdef PLATFORM_WINDOWS` branches;
   `MicrosoftPlatformString.hpp` vswprintf (host-only header).

Category tally (by TU family): ON-LUA-PATH 6, ON-OUTPUT-PATH 41 (17 direct +
24 UnrealVirtual), INTERNAL-ONLY ~90, DEAD 10; wide-importing TUs total 144 of
199. All 149 wstring-member callers land on 2-byte static code (safe).

## 3. Task 3 — removal priority ranking

Top-5 (each also removes its entries from the 207-entry frozen
EXACT-IMPORT-IDENTITIES set — every removal is a declared import delta under
the packaging contract):

1. **`FProsperoPlatformString::GetVarArgs(WIDECHAR*)` vswprintf**
   (`unreal/include/Unreal/Core/Prospero/ProsperoPlatformString.hpp:155`).
   Replace with narrow `vsnprintf` into a scratch buffer + the existing
   hand UTF8→wide converter (String.hpp DecodeUtf8). Eliminates the remaining
   garbled-log writer class on the engine-output path (highest live user
   impact; single call site, one import).
2. **Bare-pointer wstring/wstring_view construction overloads** —
   `String.hpp:281-284 to_string(const wchar_t*)`, `:207-210
   to_wstring(const wchar_t*)`, `:693-701 iequal(const CharT*, ...)`.
   Each `std::wstring{pInput}` / `wstring_view{ptr}` routes through
   `char_traits<wchar_t>::length` → console wcslen. Replace with an in-port
   2-byte strlen loop + (ptr,count) construction (the pattern the FName seam
   already uses), or delete the overloads to force sized arguments. This is
   the root of the wcslen import in 143 TUs.
3. **Pointer-shaped traits compares** — `wstring == wide_ptr`,
   `compare/find(const wchar_t*)` patterns across ue4ss/flat-API sources
   (90 wmemcmp TUs, incl. the 24 UnrealVirtual property-to-string units and
   the init-scan comparators in uobjectarray). Replace with sized compares /
   `String::iequal` views. Kills the wmemcmp import family.
4. **Wide numeric parsing in ProsperoPlatformString** — `Signed64/Unsigned64`
   (wcstoll/wcstoull, :30-47) and `Atod` (wcstod, :133). Route WIDECHAR
   through `to_string` → narrow `strtoll/strtod` → return. Removes 7 wcsto*
   imports (string.cpp.o's internal wcsto* refs go dormant once wstring
   numeric APIs are unused). Silent wrong-value class (engine property
   numbers), INTERNAL-ONLY today but cheap to kill.
5. **Detach the locale/codecvt pull chain** — remove the codecvt ctor
   references (unreal-hooks, unreal-nametypes, unreal-unrealinitializer,
   unreal-uobjectarray, native-script-hook-thunks; `#include <codecvt>` at
   String.hpp:3, `std::tolower(c, locale)` at first/include/Helpers/
   String.hpp:557) and any `std::locale`-touching remnants. Unpulling
   libc++.a locale.cpp.o + iostream.cpp.o drops the whole conversion-family
   import set at once: setlocale, fgetwc/fputwc/ungetwc, mbrlen/mbrtowc/
   mbsrtowcs/mbtowc/wcrtomb, wcscoll/wcsxfrm, strcoll/strxfrm, __mb_cur_max
   — 14+ imports and every 4-byte thunk behind them.

Not worth removing: `towlower/towupper/iswdigit/iswspace/iswxdigit`
(value-passed wint_t — no stride hazard, ASCII semantics identical on the
console tables) — DEAD/INTERNAL low-risk; touch only if the import-contract
cleanup wants a zero-wide-import binary.

## 4. Verification notes for the record

- The EV host-harness result ("assign(ptr,count) inline everywhere") holds on
  l26 only because the pulled libc++.a instantiation is itself 2-byte; the
  assumption is about the ARCHIVE, not about header inlining, and should be
  re-checked if the payload SDK libc++.a is ever replaced with a stock
  4-byte one (that would silently revive every wstring-member call).
- Import counts: undefined total 212 (identical across l26 pass-1/pass-2;
  207 frozen identities vs l24 baseline). Wide/locale family 30/212 ≈ 14%.
- Per-TU raw data preserved in this session's scratch:
  `/tmp/fc_wide_per_tu.txt` (144 rows), `/tmp/fc_wstring_member_callers.txt`
  (149 rows), `/tmp/fc_tu_sources.txt` (199 TU→source), `/tmp/fc_wide_retained.txt`,
  plus extracted archive members under `/tmp/fc_ar/` for re-verification.
