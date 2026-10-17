# STAGED-FIX — worker-construction-error (2026-10-05, LOCAL ONLY, proposal-level)

## What is staged

`staged/Helpers/String.hpp` — sha256 `7c983db4548b39b16625f979bfd2256647df0f57788e9ae27672af51b8302524`.

This is the SAME file worker-iniparser-repro staged (byte-identical, provenance
`worker-iniparser-repro/staged/Helpers/String.hpp`), independently verified by THIS worker at
the instruction level and compiled in situ on the target toolchain. Two workers arriving at the
same fix by different methods (on-target disassembly vs host emulation) is the cross-check.

Base file: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp`
(the only `Helpers/String.hpp` on the composition include path — verified by enumerating every
`-I` dir of the run-l11-2 `retained-program`/`retained-settings`/`retained-luamod` argv).

## What it changes (4 conversion entry points, nothing else)

1. `to_wstring(const std::string&)` — was `wstring_convert<codecvt_utf8_utf16<wchar_t>>::from_bytes`
   (short-wchar stride corruption) → now `Unicode::DecodeUtf8` + element copy.
2. `to_wstring(std::string_view)` (non-Windows) — was `wstring_convert<codecvt_utf8<wchar_t>>::from_bytes`
   (NUL-interleave on the target; this is the call that corrupted the settings text at
   SettingsManager.cpp:66) → now `to_wstring(std::string{input})` (routes through 1).
3. `to_string(const std::wstring&)` — was `wstring_convert<codecvt_utf8_utf16<wchar_t>>::to_bytes`
   (the [::2] that produced `TeIiasrfie opre `) → now `Unicode::EncodeUtf8(StringType{...})`
   (this is the call at UE4SSProgram.cpp:863).
4. `to_string(std::u16string_view)` — was `wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t>`
   → now routed through the explicit codec for uniformity on FORCE_U16 host builds.

Windows paths unchanged. No call sites change; no other headers change.

## Why this and not patching wstring_convert / libc++

- `std::wstring_convert`'s facets (`__codecvt_utf8{,_utf16}<wchar_t>::do_in/do_out`) are
  PREBUILT in the SDK's `libc++.a` and mirrored by a strong copy in
  `coherent-tick-file-witness-runtime.o` — a header fix cannot reach them; only avoiding the
  facilities does.
- The port already owns a correct, locale-free, bounds-checked codec
  (`Helpers/Utf16Utf8.hpp`, `static_assert(sizeof(RC::CharType) == 2)`).

## Verification performed (this worker)

| check | result |
|---|---|
| Real wpd1 `UE4SSProgram.cpp`, staged header FIRST on `-I`, exact run-l11-2 retained-program argv (prospero-clang++, -std=c++23 -fms-extensions, all unit defines) | exit 0, 0 errors — `receipts/compile-program-wpd1-with-fix.log` |
| Real `SettingsManager.cpp` (parse-path TU), same argv, full `-c` compile | exit 0, 0 errors, object emitted — `receipts/compile-settings-with-fix.log` |
| iniparser-repro `fixed_proof` (rerun) | `ensure_str produced 1448 units (expected 1448, garbled would be 2896)`; `parse OK; MajorVersion=5 …` |
| iniparser-repro `final_pipeline` (rerun) | stored-garble reproduction `MATCH: YES (byte-exact)` incl. trailing space; recovered exception text |
| Staged ini `d8faa4ec…` | CLEAN — parses end-to-end through the real parser once the conversion is fixed; DO NOT restage/change it |

## Fold instructions (controller-owned compose)

1. Admit the staged header through the composition's staged-header-override path (the same
   per-unit argv discipline; only the include resolution order changes — the staged dir must
   precede `port/first/include` for every TU that includes `<Helpers/String.hpp>`:
   minimally `retained-settings`, `retained-program`, `retained-luamod`, `retained-cppmod`,
   `unreal-unrealinitializer`, `bootstrap-ue4ss_backend`, `retained-input-handler`,
   the three pad units — safest: prepend for ALL runtime TUs).
2. `Helpers/String.hpp` is inside the freshen closure — regenerate `freshen-deps.txt`, expect
   object-hash drift in settings/program/luamod (and any TU including the header), re-run the
   offline gates (ODR emitter, finalize, gate v2) and the comparator/admission discipline.
3. Restage + ONE launch. Expected: constructor passes settings (`runtime_start_ok` ladder),
   pad coexist proceeds to outcome-B retries; any future error message renders its REAL suffix.
4. Optional hardening (separate proposal, NOT staged here): early-startup self-check
   `to_string(ensure_str("probe")) == "probe"` emitting a `diag` tag — one-line detection of
   this failure class on any future toolchain.

## Non-claims

No console validation yet (LOCAL ONLY). No completion claim. The recovered `(8 : 2)` line:col
is host-derived (worker-iniparser-repro), high confidence, not target-verified. The suffix is
not recoverable from the console sink (masked by the interleave NUL); the recovery comes from
the host-emulation pipeline.
