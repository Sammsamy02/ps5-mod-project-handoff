# agent-EW — Lua→FName seam trace (READ-ONLY)

Task: find the exact hop where the 16-char Lua query "PlayerController"
becomes the 1-char FName "P" (console: `[FFO] query-fallback [n:1DA13 num:0
ts:'P']`, PRX 5ea2c845 session evidence). Tree traced:
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` plus its composed
base (per l26 `build_runtime.py` / l25-A `COMPOSITION-RECEIPT.json`) and the
SHIPPED header lineage (`outputs/2026-10-08-l26-package-A/lineage/`).

## VERDICT (one paragraph)

No single typed hop converts 16→1. It is a COMBINED TYPE-CONFUSION in two
stages. Stage 1 (data corruption, length COUNT stays 16): on Prospero
(`CharType == wchar_t`, 2 bytes via -fshort-wchar), the query string is
built by `ensure_str` → `to_wstring` → `Unicode::DecodeUtf8`, and the
SHIPPED code called out-of-line members of the PS5 prebuilt libc++
`basic_string<wchar_t>` extern-template instantiation that was compiled for
a 4-BYTE wchar_t — each per-unit write is a 4-byte store, so the 2-byte-unit
buffer ends up holding `P,0,l,0,a,0,y,0,...` (embedded NUL at unit index 1)
while `size()` still says 16. Stage 2 (length becomes 1): the SHIPPED
`FName(StringViewType)` ctor forwards `str_name.data()` with NO length into
`construct_with_string(const CharType*)`, which hands the pointer to the
Seeds-bound ENGINE ctor `FName(const WIDECHAR*, EFindName)` (image
0x23D590) whose ABI NUL-scans 2-byte units — it stops at unit[1]==0 and
interns the 1-unit name "P", yielding the REAL id 0x1DA13. The
length-collapsing hop is `NameTypes.hpp:312-315` (shipped,
`.data()`-without-length) feeding the engine NUL-scan; the corrupting hop
is `Helpers/String.hpp` to_wstring / `Helpers/Utf16Utf8.hpp` DecodeUtf8 in
their SHIPPED (pre-EV) forms.

## Hop-by-hop (type, code units, drop risk)

Unless noted, file:line are in `what/`; SH = shipped 5ea2c845 lineage,
CD = typescan-r4 candidate (current, contains staged-but-unbuilt EV/ET
fixes dated 2026-10-09).

### Hop 1 — Lua binding call site
- CD `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp:1781,1800`
  (`lua.register_function("FindFirstOf", ...)`;
  `FindFirstOf(ensure_str(lua.get_string()))`).
  SH original `outputs/2026-10-08-l23-runtime/thread/src/LuaMod.cpp:1726,1745`
  (same expression; the "~1651" in the brief is neither file's binding line).
- Input: Lua string "PlayerController", narrow `char`, 16 bytes + Lua's own
  terminator. No cast.

### Hop 2 — `LuaMadeSimple::Lua::get_string()`
- Decl `.../native-coherent-runtime-1/source/port/first/include/LuaMadeSimple/LuaMadeSimple.hpp:536`
  `[[nodiscard]] auto get_string(int32_t force_index = 1) const -> std::string_view;`
- Def `.../src/LuaMadeSimple/LuaMadeSimple.cpp:755-759`:
  `std::string_view string = lua_tostring(...); lua_remove(...); return string;`
- TYPE: `std::string_view` over `char` — NARROW, 16 code units. It is NOT
  wide and NOT std::string. The "2-byte-per-char buffer" does not exist at
  this hop; nothing reinterprets here. (Side note, not the bug: the view
  points at the GC-owned TString after `lua_remove` — stays alive until GC.)

### Hop 3 — `ensure_str()` (the narrow→wide conversion)
- CD `candidate/include/Helpers/String.hpp:538-541`
  `ensure_str(T&&) → ensure_str_as<CharType>(...)`; `:531-534`
  `ensure_str_as → to_charT<TargetCharT>`; `:504-527 to_charT` (a
  `basic_string_view<char>` is string-like-not-of-CharType → `:516-519`
  `to_charT_string<wchar_t>`); `:400-415 → to_wstring(arg)`;
  `:213-237 to_wstring(std::string_view)` (non-Windows →
  `to_wstring(std::string)`); `:188-205 to_wstring(const std::string&)` →
  `Unicode::DecodeUtf8` (`candidate/include/Helpers/Utf16Utf8.hpp:59-103`).
- This is a genuine CONVERT (UTF-8 → UTF-16 units), not a reinterpret_cast;
  it widens narrow→wide (the brief's question: wide direction is correct).
- **SHIPPED bytes** (`outputs/2026-10-08-l26-package-A/lineage/Helpers/String.hpp`,
  sha256 c76be502fbe6... line ~197-198):
  `auto tmp = Unicode::DecodeUtf8(input); return std::wstring{tmp.begin(), tmp.end()};`
  — ITERATOR-PAIR construction. And the SHIPPED DecodeUtf8 (pre-EV form,
  preserved at `outputs/2026-10-04-zcode-team/worker-lua-api-conversion/
  fixture/extraction/include-staged/Helpers/Utf16Utf8.hpp`) built its result
  with `Result.push_back(static_cast<RC::CharType>(Code))` per unit.
- **CORRUPTION HERE.** On Prospero the PS5 SDK's prebuilt libc++ locale/str
  objects carry a `basic_string<wchar_t>` extern-template instantiation
  compiled for sizeof(wchar_t)==4. `push_back` and the `(first,last)`
  constructor are out-of-line members of that instantiation; called from
  -fshort-wchar PRX code they write/read 4-byte strides over 2-byte-unit
  buffers. Result: units `P,0,l,0,a,0,y,0,...` with `size()==16` — an
  embedded NUL at unit index 1. This exact shape is documented by the EV
  fixes now in the candidate: String.hpp:195-204 ("iterator-pair
  construction ... silently re-strides the copy (host-reproduced
  NUL-interleave under -fshort-wchar; the PS5 'ctor received P' field
  shape)") and Utf16Utf8.hpp:61-70 ("push_back ... writes 4-byte strides,
  interleaving a NUL after every character ('P',0,'l',0,...; the PS5 field
  shape '[FFO] query-fallback ts:P'").
- CD state (unbuilt): DecodeUtf8 now uses `resize + operator[]` only
  (Utf16Utf8.hpp:71-101) and to_wstring uses `std::wstring{tmp.data(),
  tmp.size()}` (String.hpp:203-204) — both claimed inline/stride-correct
  (host-audited per comments).

### Hop 4 — StringType/CharType typedefs on Prospero
- `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include/String/StringType.hpp:12-28`:
  `#ifdef __linux__` → FORCE_U16 (`char16_t`); else `CharType = wchar_t`,
  `StringType = std::basic_string<CharType>`, `StringViewType =
  std::basic_string_view<CharType>`, `STR(x)=L##x`. Prospero is BSD-based
  (`__linux__` undefined) ⇒ `CharType = wchar_t`, and the PS5 build uses
  -fshort-wchar ⇒ **2-byte wchar_t; StringType == std::wstring (2-byte
  units)**. There is NO `.c_str()`→`const char*` handoff anywhere on this
  path — every pointer seam passes `const CharType*` (wide). So the
  brief's hypothetical ".c_str() into const char* overload" does NOT
  exist; the confusion is at the C++ library ABI level (hop 3), not a
  source-level cast.

### Hop 5 — FindFirstOf overload + verified path
- CD `candidate/src/UObjectGlobals.cpp:1038-1045`
  `FindFirstOf(const StringType&)` binds the `ensure_str` temporary
  (identity beats the StringViewType user-defined conversion) →
  `FindFirstOfVerified(StringViewType)` `:938-955` (SH DA-era code had the
  same Verified chain; the SH pre-DA overloads are
  `outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp:528-548`).
  Length still counted 16; content already corrupt.
- `MakeFindXOfClassName` CD `:700-713` → `NulTerminatedNameCopy` CD
  `:195-199`: `StringType{View.data(), View.size()}` — WIDE→WIDE,
  (ptr,count), no narrowing, no reinterpret, length carried. It handles
  wide input correctly (it copies units); it cannot fix units already
  corrupted in hop 3, and the SHIPPED build did not even have this helper
  (ET fix, 2026-10-09) — SH went view→FName directly.

### Hop 6 — FName ctor overload resolution (the length-dropping seam)
- Call `FName(ClassNameCopy, FNAME_Find)` with an `StringType` lvalue.
  `std::basic_string` has NO implicit conversion to `const CharType*`
  (removed in C++11) and an implicit `operator basic_string_view`; so
  resolution picks **`FName(StringViewType, EFindName, void*)`**:
  - SH `.../native-coherent-runtime-1/source/port/unreal/include/Unreal/NameTypes.hpp:312-315`:
    `construct_with_string(str_name.data(), FindType, FunctionAddressOverride);`
    — **`.data()` with `.size()` DROPPED**.
  - `:239-246 construct_with_string` → `FName Name = ConstructorInternal(StrName, FindType);`
    — the Seeds-bound engine thunk, engine ABI
    `FName(const WIDECHAR*, EFindName)` at image 0x23D590 (DECISION-MEMO
    §2: RDI=sret out, RSI=UTF16 name — NUL-scanned input, no length reg).
- **THE ENGINE NUL-SCAN IS WHERE EFFECTIVE LENGTH BECOMES 1**: it reads
  2-byte units; unit[0]='P', unit[1]==0 (the hop-3 interleave) ⇒ stops ⇒
  interns/looks up the 1-unit name "P" ⇒ real id 0x1DA13 (`n:1DA13 num:0
  ts:'P'`; the ts field then honestly decodes the wrongly-interned name —
  corroborated by `outputs/2026-10-08-l25-prep/CLAUDE-SUMMARY-RUNTIME-DECODE.md:12-14`,
  also `1DA05:'I'` and `1D9FD:'C'`, first-letter-only across queries).
- CD (unbuilt) `candidate/include/Unreal/NameTypes.hpp:427-432` forwards
  `str_name.data(), str_name.size()` into `construct_with_sized_string`
  `:331-370` (memcpy + own NUL + embedded-NUL refusal `[FN] seam refused`)
  — this seam would now REFUSE the corrupted buffer loudly instead of
  truncating; `:318` static_assert pins CharType/TCHAR width parity.

## Why init passed but runtime failed (discriminator)

Init self-tests (`[ST] fname PlayerController PASS ... decoded='PlayerController'`)
built FNames from C++ wide LITERALS (rodata, no basic_string member
writes). Only runtime Lua queries traverse lua_tostring → ensure_str →
narrow→wide conversion. Stated verbatim in
CD `candidate/src/LuaMod.cpp:116-127` ("The existing init battery only
exercises C++ literals, which never touch the narrow->wide conversion
seam — exactly why it passed while every runtime Lua query truncated to
one character").

## Correction to the brief's framing

Nothing on this path reads the wide buffer "as NUL-terminated narrow
(char)". The engine ctor reads it as WIDECHAR (2-byte) units. The
truncation is identical in effect because the embedded NUL is a whole
2-byte unit (a 4-byte-stride write leaves the high half of each store
zero): `P \0 l \0 a \0 ...` where every other UNIT is NUL.

## Cross-check notes for the parallel fix agent

The candidate tree already stages fixes for every hop (NOT in the shipped
5ea2c845 PRX): Utf16Utf8.hpp:71-101 (resize/operator[] only),
String.hpp:203-204 ((ptr,count) ctor), NameTypes.hpp:331-370+427-432
(sized construction + embedded-NUL refusal), UObjectGlobals.cpp:775-802 +
943-947 (query-side embedded-NUL tripwire before any construction,
`[FFO] query-embedded-NUL`). A fix that only patches the FName ctor seam
without eliminating the out-of-line wide-string members in the conversion
hop still leaves corrupted data reaching every other consumer (StaticFindObject,
LuaUObject member names, etc.) — hop 3 is the root, hop 6 is the collider.

— agent-EW (ZCode), 2026-10-09. READ-ONLY; only this report file written.
