# Compiler provenance: actual conversion call in the production runtime

Worker: worker-compiler-provenance · 2026-10-05 · LOCAL only
Source of truth: `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-1/receipt.json`
(status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, settings + program both freshly compiled and present in `runtime_object_order`; runtime object `coherent-tick-file-witness-runtime.o` sha256 `39c83a3c…8a30` verified on disk; `linked_prx: false` — this run produced the relocatable runtime object).

## Verdict (one paragraph)

Both fixes are proven present in the exact compilation the production runtime executes. The settings unit compiles the **patched SettingsManager.cpp** (sha `df1aaa8f…`, manual ASCII→wchar_t loop; no `ensure_str` at the deserialize site) against the **patched Helpers/String.hpp** (sha `7c983db4…`, byte-identical to the staged canonical), which shadows the upstream codecvt version via `-I` order (root index 2 vs 50). `settings.o` contains **zero** codecvt/`wstring_convert` symbols and defines `RC::Unicode::DecodeUtf8/EncodeUtf8`. The program unit selects the same patched header; `create_emergency_console_for_early_error` compiles the `__PROSPERO__` branch `copy_error_into_message(to_string(error_message).c_str())` where `to_string` provably resolves (AST dump) to `to_string(std::wstring_view)` → `to_string(const std::wstring&)` → `Unicode::EncodeUtf8`. The readable pid-601 error never passes through any conversion at all: it is a narrow ASCII `std::runtime_error` literal (program-wpd1/UE4SSProgram.cpp:777) copied via `copy_error_into_message(const char*)`; reaching it at all is itself evidence the settings text parsed into correct wide values.

## 1. Exact compile commands (recovered)

From `receipt.json → selected_runtime_units` — full argv saved to `settings.argv.json` / `program.argv.json` / `argv-dump.txt`.

- Compiler: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++` — a bash wrapper that execs LLVM 21.1.8 clang++ with `-target x86_64-sie-ps5`, `-isysroot <sdk>`, `-stdlib++-isystem <sdk>/target/include/c++/v1`. The wrapper requires `LLVM_CONFIG` env (recorded in the receipt as `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`); without it the inner wrapper execs a nonexistent `/llvm-clang++` (exit 126). Repro commands therefore set `LLVM_CONFIG` (see `settings-preprocess-command.sh`).
- Settings unit: **170 args**, source `.superpowers/.../native-coherent-runtime-1/source/port/ue4ss/src/SettingsManager.cpp`.
- Program unit: **176 args**, source `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp`.
- Shared notable flags: `-std=c++23 -fms-extensions -O0 -fno-lto -fPIC -ferror-limit=8`, `-DUE_BUILD_SHIPPING=1`, `-DOVERRIDE_PLATFORM_HEADER_NAME=Prospero`, `-D_LIBCPP_ENABLE_EXPERIMENTAL`, ~50 `-I` dirs, `-c -o <unit>.o -MD -MF <unit>.d`.
- **No** response files, **no** `-include`/forced includes, **no** PCH/module inputs, **no** `-fshort-wchar`, **no** explicit `--target`/`-D__PROSPERO__` (the target and `__PROSPERO__` come from the ps5-payload-sdk toolchain driver itself).

## 2. Settings unit (the conversion in SettingsManager.cpp)

Preprocessed with the exact production argv, `-c`→`-E -H` (`settings-preprocessed.i`, 6.96 MB; stderr `settings-preprocess.stderr.txt`).

- **Selected header**: line 8 of `settings-preprocessed.i` and the first line of the `-H` stack: `port/first/include/Helpers/String.hpp` — sha256 `7c983db4548b39b16625f979bfd2256647df0f57788e9ae27672af51b8302524`, **identical to the staged canonical** `outputs/2026-10-04-zcode-team/worker-iniparser-repro/staged/Helpers/String.hpp`. The upstream unpatched header (`deps/first/Helpers/include/Helpers/String.hpp`, sha `178080ae…`, whose `to_wstring` is `std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>`) sits at `-I` root index 50 and is **never opened**.
- **Patch present**: the preprocessed `SettingsManager::deserialize(path&, UE4SSProgram&)` body (line 153937+) contains the manual loop at **line 153967**:
  `for (const char c : settings->bytes) { contents.push_back(static_cast<CharType>(static_cast<unsigned char>(c))); }` → `parser.parse(contents)`. No `ensure_str` at this site (the only `ensure_str` texts in the file are the header's own definitions and unrelated `find_mod_by_name` inlines). Diff vs the immediate pre-patch backup is `settings-manager-codecvt-fix.diff`; vs the older settings-defaults-2 source, `settings-manager-patch.diff`.
- **Object identity**: `settings.o` sha256 `03e2db6b…f792` == receipt `object_sha256`. `nm` evidence: `RC::SettingsManager::deserialize(...)` defined; weak `RC::Unicode::DecodeUtf8(std::string_view)` and `RC::Unicode::EncodeUtf8(std::wstring_view)` emitted; **0** symbols matching `codecvt|wstring_convert|from_bytes|to_bytes` (defined or undefined). `settings.d` dependency file lists the patched header path.

## 3. Program unit (error-message formatting)

Preprocessed identically (`program-preprocessed.i`, 8.52 MB).

- Same patched header selected (`program-preprocessed.i:106566`; `-H` stack `evidence-program-header-stack.txt`).
- `create_emergency_console_for_early_error(File::StringViewType)` compiles the `__PROSPERO__` branch (line 191701): `copy_error_into_message(to_string(error_message).c_str());` — the console `#else` branch is compiled out (blank lines visible).
- Call chain proven in preprocessed source + AST (`probe-overload-ast.txt`):
  `create_emergency_console_for_early_error` → `to_string(error_message)` → **`to_string(std::wstring_view)`** (exact-match overload, AST: `DeclRefExpr … 'to_string' 'auto (std::wstring_view) -> std::string'`) → `to_string(const std::wstring&)` → `Unicode::EncodeUtf8` (hand-rolled UTF-16→UTF-8 bit twiddling, preprocessed lines 72861–72875; no codecvt) → `copy_error_into_message(const char*)` (`ProgramFeatures/CanError.hpp:57` → `ErrorObject::copy_error(const char*)`, pure narrow copy).
- The IniParser error path (UE4SSProgram.cpp:261) preprocesses to `fmt::format(L"The IniParser failed to parse: {}", ensure_str(e.what()))` — `STR()` → `L""`; `ensure_str(const char*)` AST-selects the `std::basic_string<wchar_t>` instantiation whose chain is `to_charT_string<wchar_t>` → `to_wstring(std::string_view)` → `to_wstring(const std::string&)` → `Unicode::DecodeUtf8`.
- `program.o` (sha `37871723…0944` == receipt): defines `RC::UE4SSProgram::create_emergency_console_for_early_error(std::wstring_view)`; **0** `codecvt_utf8_utf16|wstring_convert|from_bytes|to_bytes` symbols. Residual weak `std::codecvt<wchar_t,char,mbstate_t>` vtable/facet symbols + a `codecvt<char32_t,char>` vtable ref exist (std::locale/std::filesystem instantiation residue in this large TU) but nothing on the error path calls them and the garbling `codecvt_utf8_utf16<wchar_t>` helper is absent.

## 4. wchar_t vs char16_t

Macro dump with production flags (`settings-target-macros.txt`): `__PROSPERO__=1`, `__SIZEOF_WCHAR_T__=2`, `__WCHAR_TYPE__=unsigned short`, `__WCHAR_UNSIGNED__=1`, `__WCHAR_MAX__=65535`, `__CHAR16_TYPE__=unsigned short`.

static_assert probe compiled with the exact settings-unit flags (exit 0): `sizeof(wchar_t)==2`, `sizeof(char16_t)==2`, `!is_same_v<wchar_t,char16_t>`, `is_unsigned_v<wchar_t>` — **wchar_t is a distinct 2-byte unsigned type; char16_t is a different 2-byte type.**

Resolution: `RC::CharType = wchar_t` (preprocessed line 72522), `RC::StringType = std::wstring` (72526), `RC::StringViewType = std::wstring_view` (72527); `File::CharType/StringType/StringViewType` alias the RC ones (`File/Macros.hpp:22`). `ensure_str` → `ensure_str_as<CharType=wchar_t>` → `to_charT<wchar_t>`; for non-path args → `to_charT_string<wchar_t>` → `to_wstring`; for `std::filesystem::path` args → `path::wstring()`. The only remaining codecvt text in the patched header is the `char16_t/uint16_t` fallback branch of `to_charT_string_path`, which `CharType=wchar_t` never instantiates.

## 5. Provenance chain (production call → proof)

| Link | Evidence |
|---|---|
| Production runtime object | `coherent-tick-file-witness-runtime.o` sha `39c83a3c…8a30` (verified on disk; settings+program in `runtime_object_order`, both freshly compiled) |
| Settings unit compile | receipt `settings.argv` (170 args); source sha `df1aaa8f…` == patched file on disk; object sha `03e2db6b…` == file on disk |
| Settings conversion call | preprocessed line 153967: manual byte→`CharType` loop (patch present; `ensure_str` absent at site) |
| Selected header (settings) | `-H` stack line 1 + linemarker line 8: `port/first/include/Helpers/String.hpp`, sha `7c983db4…` (== staged canonical); upstream codecvt header shadowed at `-I` index 50 |
| Program unit compile | receipt `program.argv` (176 args); source sha `aaf35500…`; object sha `37871723…` == file on disk |
| Program conversion call | preprocessed line 191701 `__PROSPERO__` branch; AST: `to_string(std::wstring_view)` selected → `Unicode::EncodeUtf8`; `copy_error_into_message(const char*)` narrow copy |
| Selected header (program) | linemarker line 106566: same patched header |
| Type facts | `__PROSPERO__=1`; `sizeof(wchar_t)==2` (unsigned) ≠ `char16_t`; `CharType=wchar_t`, `StringType=std::wstring` — static_assert + `-dM` proof |
| Symbol-level close | `settings.o`: 0 codecvt symbols, defines DecodeUtf8/EncodeUtf8, deserialize present; `program.o`: 0 `codecvt_utf8_utf16`/`wstring_convert` refs, defines `create_emergency_console_for_early_error(std::wstring_view)` |

### Why pid 601's error was readable

`"Native configured InputSource does not match the supplied provider"` is thrown at staged `program-wpd1/UE4SSProgram.cpp:777` as a plain ASCII `std::runtime_error`, caught at :725, and copied by `copy_error_into_message(e.what())` (:737) — a path with **no wide/narrow conversion whatsoever**. Readability alone doesn't prove a fix; but *reaching* this check means `settings_manager.General.InputSource` was parsed from the INI into a correct wide string (garbled input would have failed earlier or mismatched differently), and that parse is the proven manual-loop + patched-header code path above. The check itself converts the wide setting back to narrow via the proven `to_string(const std::wstring&)` → `EncodeUtf8` route.

## Artifacts

All under `outputs/2026-10-04-zcode-team/worker-compiler-provenance/`:
`provenance.json` (machine-readable), `settings.argv.json`, `program.argv.json`, `argv-dump.txt`, `settings-preprocess-command.sh`, `settings-preprocessed.i`, `program-preprocessed.i`, `settings-preprocess.stderr.txt`, `program-preprocess.stderr.txt`, `settings-manager-codecvt-fix.diff`, `settings-manager-patch.diff`, `settings-target-macros.txt`, `probe-overload-ast.txt`, `evidence-*.txt` (header stacks, CharType typedefs, patched to_string/to_wstring bodies, settings conversion site, program emergency console).

Repro notes: preprocessing = exact receipt argv with `-c`→`-E -H`, `-o/-MD/-MF` dropped, run with `LLVM_CONFIG=<receipt LLVM_CONFIG>`; probes are additive-only (`-fsyntax-only`, `-dM -E`, `-Xclang -ast-dump`) on separate TUs — no engine/runtime file was modified.
