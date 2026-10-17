# Final-code provenance: which conversion implementation is in the FINAL staged candidate

Worker: worker-final-code-provenance
Date: 2026-10-05
Target analyzed: `outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-2/build/combined-supplier.o`
sha256 `fd2d6eef58991470d5dbde737a1024870314888a6dc1e3e9b84af45f2d164669` (118,080,440 B, ELF64 x86-64 REL, 202,456 sections) — hash matches `artifacts-receipt.json` and `package-profile.json` `combined_object_sha256` exactly.

## Verdict (one line per question)

1. SettingsManager deserialize uses the **manual ASCII-to-wchar_t loop (the direct .cpp patch), inlined by the compiler** — not wstring_convert, not DecodeUtf8.
2. The emergency error path uses the **ORIGINAL `RC::to_string` → `wstring_convert<codecvt_utf8_utf16<wchar_t>>::to_bytes`** (not the patched DecodeUtf8). The readable output is NOT produced by our two fixes; this path never used the broken narrow→wide conversion, and its `do_out` is statically linked in the object (locale-independent for ASCII).
3. **No COMDAT/inline conflict.** Each conversion symbol has exactly ONE definition and ONE COMDAT group in the combined object (verified: 0 duplicate conversion symbols out of 145,805 defined symbols; 48,078 COMDAT groups total). The ORIGINAL wstring_convert-based versions won for to_string/to_wstring/ensure_str; the patched `Unicode::DecodeUtf8` IS present but has only 2 call sites, neither on the settings or error paths.
4. Hash chain to FSELF verified end-to-end (see chain below); staged PRX embeds the patched deserialize's UTF-16 section-key rodata.

## 1. SettingsManager::deserialize

Symbols (both defined, T):
- `_ZN2RC15SettingsManager11deserializeERNSt3__14__fs10filesystem4pathE` — pure thunk: calls `UE4SSProgram::get_program()` then forwards to the 2-arg overload.
- `_ZN2RC15SettingsManager11deserializeERNSt3__14__fs10filesystem4pathERNS_12UE4SSProgramE` — the real body.

In the 2-arg body (disasm-deserialize-2arg.txt), offsets 0x219–0x312 are the manual widening loop, exactly the SettingsManager.cpp direct patch:

```
0x219: callq  basic_string<wchar_t>::basic_string()          # dest wstring
0x24a: rsi   = basic_string_view<char>::size()               # file buffer view
0x254: callq  basic_string<wchar_t>::reserve(size_t)
0x289: callq  basic_string_view<char>::begin()
0x29c: callq  basic_string_view<char>::end()
0x2a8: cmpq   begin, end ; je 0x312                           # loop head
0x2b8: movb   (%rax), %al                                     # load byte
0x2c7: movzbl -0x189(%rbp), %esi                              # zero-extend byte -> wchar_t
0x2d5: callq  basic_string<wchar_t>::push_back(wchar_t)
0x2de: inc begin ; jmp 0x2a8
0x312: callq  RC::Ini::Parser::parse(basic_string<wchar_t>&)
```

No call to `wstring_convert::from_bytes`, no call to `Unicode::DecodeUtf8`, no `to_wstring` — the loop is the conversion. The patch is in the final binary. (The 2 calls to `RC::to_string(const wstring&)` inside deserialize are narrow-conversions of parsed values for logging/compare, not the file-content path.)

## 2. Emergency error message path

Full chain in the final object:

```
UE4SSProgram::UE4SSProgram(NativeProgramContext, initializer_list<BinaryOptions>)   [disasm-ctor.txt @0x747-0x78d]
  fmt::v11::format<basic_string<wchar_t>...>(basic_fstring<wchar_t>, ...)           # wide message, fmt internals
  wstring::operator basic_string_view<wchar_t>()
  -> UE4SSProgram::create_emergency_console_for_early_error(basic_string_view<wchar_t>)   [disasm-emergency-console.txt]
       -> RC::to_string(basic_string_view<wchar_t>)        [forwards: builds wstring, calls to_string(const wstring&)]
            -> RC::to_string(const basic_string<wchar_t>&) [ORIGINAL: guarded static 'converter'
                 = wstring_convert<codecvt_utf8_utf16<wchar_t,1114111,(mode)0>, wchar_t, ...>]
                 -> wstring_convert<...>::to_bytes(const wstring&)     # wide -> narrow UTF-8
       -> basic_string<char>::c_str()
       -> RC::CanError::copy_error_into_message(const char*)
            -> RC::ErrorObject::copy_error(const char*)    # strlen + memcpy into error object
  Later: modbridge::native::(anon)::UE4SSBackend::initialize(function<void()>)
       -> UE4SSProgram::init()
       -> UE4SSProgram::native_startup_error_message()     # has_error() ? ErrorObject::get_message() : nullptr
       -> if non-null: throw std::runtime_error(message)   # readable narrow text propagates to bridge error handling
```

- The conversion used is the ORIGINAL to_string (wstring_convert), NOT the patched DecodeUtf8.
- Why the text is readable anyway: `std::__1::__codecvt_utf8_utf16<wchar_t>::do_out` and `do_in` are DEFINED (strong T) inside combined-supplier.o — the codecvt implementation is statically linked, so wide→narrow to_bytes is deterministic for ASCII regardless of PS5 locale. The historical failure mode (locale-dependent narrow→wide conversion of arbitrary file bytes) is not on this path; this path converts wchar_t→char only, and its input was assembled by fmt, not by ensure_str.
- Conclusion: readable error output is NOT evidence that the String.hpp fix works; it is the expected behavior of the untouched original wide→narrow path with statically linked codecvt.

## 3. COMDAT / inline conflict scan

- Conversion-family duplicate check over nm-defined.txt (145,805 defined symbols): ZERO duplicates for any `DecodeUtf8|EncodeUtf8|Utf8Size|NextUtf16|to_string|to_wstring|ensure_str|ensure_str_as|wstring_convert` symbol.
- llvm-readelf --section-groups: exactly one COMDAT group per conversion symbol (e.g. `.group[_ZN2RC7Unicode10DecodeUtf8...]` at section index 20179; wstring_convert instantiations at 60645ff, 6720ff, 7384ff). 48,078 groups total in the object.
- The 3,240 duplicate symbol NAMES in the object are benign: `__cxx_global_var_init.*`, C5/D5 ctor-dtor clones (`_ZN2RC15SettingsManagerC5Ev` etc., local `a`/`n` symbols at offset 0), coroutine `.cleanup/.destroy/.resume`, local statics. None are conversion functions.
- Which version won the link: since combined-supplier.o already contains a single merged definition per symbol, the link winner is unambiguous:
  - `RC::to_string` (3 overloads), `RC::to_wstring` (3 overloads), `ensure_str`/`ensure_str_as`/`to_charT*` family: ORIGINAL wstring_convert-based bodies. Proven by disassembly:
    - `to_string(const wstring&)` @0x3f: constructs guarded static converter `wstring_convert<codecvt_utf8_utf16<wchar_t,...>>` and calls `to_bytes(const wstring&)` @0x7c.
    - `ensure_str<T> -> ensure_str_as<wchar_t,T> -> to_charT<wchar_t,T> -> (stringviewify) -> to_charT_string<wchar_t, view<char>> -> RC::to_wstring(view<char>)` (disasm-tocharT-to_wstring.txt).
    - `to_wstring(view<char>)`: guarded static `wstring_convert<codecvt_utf8_utf16<wchar_t>>` + `from_bytes(const char*, const char*)` @0xaa.
  - `Unicode::DecodeUtf8(view<char>)` is the real patched manual UTF-8 decoder (size guard 0x30000; ASCII <=0x7F / 2-byte 0xC2-DF / 3-byte 0xE0-EF / 4-byte 0xF0-F4; throws invalid_argument otherwise) — but whole-object relocation scan shows only TWO code callers: `RC::get_now_as_string(view<wchar_t>)` and `RC::CppMod::CppMod(UE4SSProgram&, wstring&&, wstring&&)`. `Unicode::EncodeUtf8` has 10 callers. Neither is called by SettingsManager::deserialize or the emergency path.
  - `wstring_convert<codecvt_utf8<wchar_t>>::from_bytes` (the classic ensure_str instantiation) is present as dead COMDAT with ZERO inbound code references; the live narrow→wide route uses the `codecvt_utf8_utf16<wchar_t>` instantiation.

## 4. Provenance chain (production call site -> staged FSELF)

Settings path:
```
UE4SSProgram ctor / startup
  -> RC::SettingsManager::deserialize(path&)                [thunk @0x20-0x30]
     -> deserialize(path&, UE4SSProgram&)                   [.text._ZN2RC15SettingsManager11deserializeE...RNS_12UE4SSProgramE]
        -> NativeModSource::read(view<char>) + inlined MANUAL ASCII loop (patch, offsets 0x219-0x312)
        -> Ini::Parser::parse(wstring)
     object providing definition: combined-supplier.o (sha256 fd2d6eef58991470d5dbde737a1024870314888a6dc1e3e9b84af45f2d164669)
     == artifacts-receipt.json combined_object.sha256 == package-profile.json combined_object_sha256
     -> parent_recipe: outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2
     -> marker "modbridge-supplier-composition/v1 build=fd2d6eef58991470"
     -> staged FSELF: modbridge_supplier_composition.prx (sha256 f1320b7513a51fe3657f8101735f7a632c2ae05438e148d6be0d5fd78c6e2f04, 30,848,951 B; FSELF magic 4f153d1d per static-checks.json)
     content spot-check in PRX: UTF-16LE keys "General\0" @0x128ae10 and "Hooks\0" @0x128af3e = deserialize section-key rodata from the patched function.
```

Error path:
```
UE4SSProgram ctor -> create_emergency_console_for_early_error(view<wchar>)
  -> ORIGINAL RC::to_string -> wstring_convert<codecvt_utf8_utf16<wchar_t>>::to_bytes
  -> CanError::copy_error_into_message -> ErrorObject::copy_error (strlen+memcpy)
  -> UE4SSBackend::initialize: native_startup_error_message() -> throw std::runtime_error(msg)
```

## Bottom line for the controller

- The SettingsManager.cpp DIRECT patch is provably in the final staged FSELF.
- The String.hpp HEADER patch is NOT effective on the ensure_str/to_string/to_wstring routes: those still resolve to the original wstring_convert implementations (now using the codecvt_utf8_utf16 instantiation with statically linked do_in/do_out, which is ASCII-safe). DecodeUtf8/EncodeUtf8 exist but are reached only from get_now_as_string and CppMod's constructor.
- The readable error text in the last run is explained by the untouched original wide→narrow path with statically linked codecvt — it is not proof of the header fix.
- Any claim that "the conversion fix is in the final binary" is true only for the SettingsManager direct patch; the header-level fix remains not on the hot paths.

## Evidence files in this directory

- nm-defined.txt / nm-undef.txt — full symbol tables
- disasm-deserialize-2arg.txt / disasm-deserialize-1arg.txt — patched loop + thunk
- disasm-tocharT-to_wstring.txt — original narrow→wide chain evidence
- disasm-emergency-console.txt / disasm-ctor.txt — error path
- relocs-conversion-refs.txt / relocs-with-sections.txt / relocs-towstring-ensurestr.txt — whole-object reference scans
- dup-symbols.txt — duplicate symbol audit (no conversion symbols)
