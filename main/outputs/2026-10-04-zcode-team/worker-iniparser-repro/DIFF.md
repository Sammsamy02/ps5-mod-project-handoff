# DIFF — staged fix for the IniParser / wide-conversion failure (worker-iniparser-repro, 2026-10-05)

PROPOSAL-LEVEL STAGED COPY. The canonical tree
(`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp`,
sha256 `b9249d0819f117c3…808df4ec`) is UNTOUCHED. Deliverable:
`staged/Helpers/String.hpp` (sha256 `7c983db4548b39b1…b8302524`), full-file diff at
`staged/String.hpp.diff` (5 hunks: 1 include + 4 function bodies; zero signature changes).

## 1. What changed and why

The PS5 SDK's prebuilt `libc++.a` (sdk-v0.43, `target/lib/locale.cpp.o`) pairs 2-byte-wchar
`do_in`/`do_out` wrappers with 4-byte-unit helpers (`utf8_to_utf16`/`utf16_to_utf8` on
`uint32_t`, plus a `sarq $1` to/from_next fixup). On `__PROSPERO__` (`CharType = wchar_t`,
`sizeof==2`) every `std::wstring_convert` call therefore corrupts data:

| direction | corruption | live consequence |
|---|---|---|
| `from_bytes` (narrow→wide, `ensure_str`) | NUL after every character | settings ini content garbled → real `Ini::Parser` syntax error (reproduced: `Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey`) |
| `to_bytes` (wide→narrow, `to_string`) | every OTHER character = `[::2]` | stored error message = `TeIiasrfie opre ` (byte-exact vs pids 466/492) |

Fix: stop using `std::wstring_convert` on non-Windows platforms entirely; route through
`RC::Unicode::EncodeUtf8 / DecodeUtf8` (`Helpers/Utf16Utf8.hpp`) — already part of the port
(SettingsManager.cpp includes it on PROSPERO), explicit, locale-free, correct by construction
for the project's 2-byte `CharType` (its own `static_assert` line 13).

## 2. Exact hunks

1. **Include** `<Helpers/Utf16Utf8.hpp>` after `<String/StringType.hpp>` (no cycle: it only
   includes StringType + <string> etc).
2. **`to_wstring(const std::string&)`** (was `codecvt_utf8_utf16<wchar_t>::from_bytes`):
   → `Unicode::DecodeUtf8(input)` + element-copy bridge to `std::wstring` (identity on target
   where `CharType==wchar_t`; FORCE_U16 host shim carries UTF-16 units).
3. **`to_wstring(std::string_view)`** non-Windows branch (was `codecvt_utf8<wchar_t>::from_bytes`):
   → `to_wstring(std::string{input})` (delegates to 2). PLATFORM_WINDOWS branch unchanged.
4. **`to_string(const std::wstring&)`** (was `codecvt_utf8_utf16<wchar_t>::to_bytes`):
   → element bridge to `StringType` + `Unicode::EncodeUtf8`.
5. **`to_string(std::u16string_view)`** (was `codecvt_utf8_utf16<char16_t>::to_bytes` — the one
   even-correct-on-host path): → `Unicode::EncodeUtf8` for uniform behavior.

Untouched: `to_string(const wchar_t*)` / `to_string(std::wstring_view)` /
`to_wstring(const char*)` (delegate through patched overloads), all `to_u16string` variants,
`ensure_str*`, explode/quote helpers, Windows `#ifdef PLATFORM_WINDOWS` branches.

## 3. Call sites this repairs (the two that produced the live failure)

- `SettingsManager.cpp:66` `auto contents = ensure_str(std::string(settings->bytes));`
  → now decodes 1:1 → the real IniParser parses the real ini (host-proven: all five staged
  values come back exact).
- `UE4SSProgram.cpp:844` (PROSPERO `create_emergency_console_for_early_error`)
  `copy_error_into_message(to_string(error_message).c_str())` → now stores the FULL message
  (host-proven with the recovered text).
- Everything else using `ensure_str`/`to_string`/`to_wstring` in the composed closure
  (Lua bridges, Output, mod source paths) inherits the same repair — honest note: that is a
  WIDE behavior change for every caller on target (from corrupt to correct), not a settings-only
  patch.

## 4. Host verification (all binaries in harness/)

| test | result |
|---|---|
| staged header compiles (FORCE_U16 host harness, real port sources) | PASS |
| clean-content regression through real parser (CASE A) | PASS — 5/5 staged values exact |
| `Unicode::DecodeUtf8(ini)` → real parser | PASS — 1448 units 1:1, parse OK |
| full fixed target pipeline (`ensure_str` → parse) | PASS — parse OK, values exact |
| staged `to_string` on the recovered wide message | PASS — full `Syntax error(8 : 2)…` suffix retained |
| pre-fix baseline (same harness, unpatched header) | parse of garbled content THROWS at (8:2); stored message = `TeIiasrfie opre ` byte-exact vs live |

## 5. Integration path (for the loop, not executed here)

1. Fold this staged header into the compose as a staged-header override or a CR1-tree
   revision — the freshen/regen `.d` closure ALREADY lists
   `native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp` (settings.o and
   program.o both depend on it), so a normal recompose picks it up; new unit hashes must flow
   through the usual receipts/gates (ODR, determinism, comparator pins will shift where the
   header is in the closure — expect luamod/program/settings object-hash drift).
2. Recompose → offline gates → restage → ONE launch.
3. Expected runtime evidence if the fix is complete: constructor passes settings
   (`[EngineVersionOverride]` etc. consumed), `validate_native_services` passes with the staged
   ini, and the error path — if it ever fires — now surfaces its real suffix instead of
   `TeIiasrfie opre `.
4. Do NOT stage the ini again — the data was verified clean end-to-end; nothing data-side
   changed.

## 6. Honest limits

- Not compiled into a composed candidate; not console-validated (mission bound: LOCAL ONLY).
- FORCE_U16 host semantics of the `std::wstring` overloads change from "UTF-32-ish code points
  via codecvt" to "UTF-16 code units widened" — matches the target's contract; nothing in the
  PROSPERO closure compiles the wchar-literal overloads against 4-byte wchar.
- `Utf16Utf8` bounds (65536 units / 196608 bytes) now cap `ensure_str`/`to_string` inputs —
  irrelevant for the settings/error paths, a real documented cap for hypothetical huge strings.
- The library itself stays broken for any OTHER code that calls `std::wstring_convert` directly
  (grep of the port closure: none outside String.hpp) and for `fmt`'s own wide filesystem
  paths (fmt does not use codecvt in this configuration).
