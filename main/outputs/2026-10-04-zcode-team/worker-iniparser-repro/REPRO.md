# REPRO — the IniParser settings-deserialization failure, reproduced host-side (worker-iniparser-repro, 2026-10-05)

LOCAL ONLY. No console contact. Everything below was executed on this host; binaries and
sources are preserved under `harness/`, raw evidence copies under `evidence/`.

## 0. Verdict in one paragraph

The exception is real, deterministic, and fully recovered:

```
The IniParser failed to parse: Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey
```

The settings data is clean, the parser code is clean, and the launch-10 vs combined delta is
irrelevant: the **PS5 SDK's prebuilt libc++ corrupts every `std::wstring_convert` call on the
short-wchar (`sizeof(wchar_t)==2`) PROSPERO target**. `from_bytes` interleaves a NUL after
every character (the parser then genuinely throws on the garbage), and `to_bytes` returns every
OTHER character — the `[::2]` operation that turned the message into `TeIiasrfie opre ` in both
live runs. The suffix was never "lost by truncation": it was NUL-interleaved by the broken
`ensure_str`, then annihilated by the broken `to_string`, then cut by `strlen` in
`ErrorObject::copy_error`. Reproduced byte-exact.

## 1. Raw live evidence (copied to evidence/)

`/tmp/l11run-status.txt` (run 1, pid 466) and `/tmp/l11run2-status.txt` (run 2, pid 492) — hex:

```
00000060: 6564 3a20 5465 4969 6173 7266 6965 206f  ed: TeIiasrfie o
00000070: 7072 6520 0a                             pre .
```

Detail ends `opre ` + newline — 16 chars, trailing space, no NUL, nothing after. Both runs
byte-identical. (`'The IniParser failed to parse: '` has 31 units; `[::2]` of it is exactly
`TeIiasrfie opre ` — 15 chars + the trailing space at even index 30.)

## 2. The library bug, from the disassembly

Extracted `locale.cpp.o` from
`outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib/libc++.a`
(to `/tmp/l11cxx/`). Key symbols (x86-64 ELF, PS5 target):

- `_ZNSt3__1L13utf16_to_utf8EPKjS1_RS1_PhS3_RS3_mNS_12codecvt_modeE`
  (=`RC::utf16_to_utf8(const unsigned int*, …)`) — per unit: `movl (%r14),%r11d` (4-byte
  load) → `movzwl %r11w,%ebx` (truncate to 16) → emit UTF-8 → advance source **+4 bytes**
  (`addq $0x4,%r14`; surrogate low **+8**, with `cmpq $0x8` bounds). A 4-byte stride over a
  2-byte-element array = consumes elements 0,2,4,… = `[::2]`.
- `__codecvt_utf8_utf16<wchar_t>::do_out` calls that **uint32** helper (PLT32 reloc at 0x44)
  and then halves the `from_next` byte distance (`sarq $1`) — a wrapper compiled for 2-byte
  `wchar_t` calling a helper compiled for 4-byte units. Internally inconsistent pairing.
- `__codecvt_utf8<wchar_t>::do_in` likewise calls the **uint32-writing**
  `utf8_to_utf16(...Pj...)` (reloc at 0x44) with the same `sarq $1` `to_nxt` fixup: each unit
  is stored as 32-bit (`movl %ebx,(%r10)`) with the destination advancing +4 — writes land in
  the low half of each overlapping pair, so a 2-byte wstring reads back as
  `c0,0,c1,0,c2,0,…` — **a NUL after every character**.
- The `char16_t` specialization is CORRECT: `__codecvt_utf8_utf16<char16_t>::do_out` relocs to
  the uint16 helper (`...Pt...`). FORCE_U16 builds were never affected — which is why host
  test flows never saw this.

Why `wchar_t` at all: `deps/first/String/include/String/StringType.hpp` sets
`CharType = char16_t` only under `FORCE_U16` (`__linux__`); everything else — including
`__PROSPERO__` — uses `CharType = wchar_t`, and the PS5 clang is short-wchar (the port's own
`Utf16Utf8.hpp` line 13 `static_assert(sizeof(RC::CharType) == 2)`).

## 3. The harness (harness/)

Compiles the **real** composed-tree sources —
`native-coherent-runtime-1/source/port/first/src/IniParser/{Ini,TokenParser,Value}.cpp`,
`.../ParserBase/{Tokenizer,Token,TokenParser}.cpp` — with the real headers
(`deps/first/{IniParser,ParserBase,Helpers,String}/include`), FORCE_U16 for the host char type,
and drives the **real** staged ini `UE4SS-settings.ini`
(1448 B, sha256 `d8faa4ec…d67038`, ends `\n`, 1448 % 8 == 0 so the corruption chunking has no
partial-tail effects).

### CASE A — clean conversion (control): PARSES PERFECTLY

```
[A-clean] parse COMPLETED (no exception)
    General.bUseUObjectArrayCache = 0      (expected 0)
    General.SecondsToScanBeforeGivingUp = 120 (expected 120)
    EngineVersionOverride.MajorVersion = 5 (expected 5)
    Hooks.HookProcessInternal = 0          (expected 0)
    Debug.RenderMode = 'ExternalThread'    (expected ExternalThread)
```

Data verified good through the actual parser — discharge of "is the ini the problem" (it is not).

### CASE B — the PS5-broken conversion (disassembly-faithful emulation): REAL PARSE ERROR

```
[B-ps5garbled] parse THREW: what()='Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey'
```

Mechanism (bisect.cpp): with every character NUL-followed, `ParserBase::TokenParser::get_data`
truncates every Characters token at its first NUL (`data.erase(find('\0'), end)`); inside a
section header `characters_to_string`'s lookahead
(`m_current_state == CreateNewOrSetCurrentSection && peek()==']' && peek(2)==NewLine`) then
mis-fires whenever anything follows the `]` on the (garbled) line, the `]` is consumed into the
section name, **no section ever registers**, and the next `[` arrives in state
`CreateSectionKey` → the throw. Line numbers survive the corruption (only `\n` advances the
line counter), so the reported line 8 + the `[` column = the `[General]` header (9th text line,
after 7 comment lines + 1 blank).

### CASE C — the full target emergency pipeline: BYTE-EXACT MATCH

`final_pipeline.cpp` replays, in order: broken-`ensure_str(e.what())` (NUL interleave) →
`fmt::format(L"The IniParser failed to parse: {}")` → broken-`to_string` (`[::2]` stride) →
`ErrorObject::copy_error` (strlen-capped memcpy):

```
recovered e.what()         : Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey
stored ErrorObject message : 'TeIiasrfie opre ' (visible strlen 16, raw 103 bytes)
MATCH: YES (byte-exact)     [vs both /tmp/l11run{,2}-status.txt files]
```

The suffix's `[::2]` survivors are all NULs (every even suffix element is an interleave NUL),
so `strlen` cuts the stored message at 16 chars — exactly the live bytes including the trailing
space. This is why both runs, and any other message, garble to the identical prefix.

## 4. Why it fired in the combined candidate (and "launch-10 passed" was an inference)

- `settings` unit source is byte-carried: BOTH the l10 and l11 drivers pin
  `--replace-runtime-unit settings=CR1/source/port/ue4ss/src/SettingsManager.cpp`; the
  IniParser objects (`ini.o`, `ini-value.o`, `ini-tokens.o`, `base-*.o`) are prebuilt baseline
  objects shared by both compositions; `Helpers/String.hpp` is the same file (b9249d08…).
- LAUNCH10-RESULTS.md: every launch-10-era run (pids 317/341/355) refused BEFORE the program
  constructor (PS5Pad.ini control bytes → mfa size → pad unavailable phase=6). **Settings
  deserialize was never reached before the l11 combined runs.** The wpd2 pad-coexist deferral
  is the first change that lets `UE4SSProgram` construct — the latent library bug fired on
  first exercise, deterministically, and is VTableLayout-revert-proof by nature.
- Same-story corroboration: launch-9 (pid 273) refused even earlier (image-geometry), so it
  never reached the ctor either; its silent "no mods loaded" is consistent.

## 5. MemberVariableLayout.ini / VTableLayout.ini — EXCLUDED

`NativeLayoutBinding.cpp` (`StageNativeLayoutFiles` → `ReadBounded` → `ParsePair`) parses both
layout files with a dedicated narrow-string parser (own identity vocabulary:
`prospero-x86_64-sysv`, EngineMajor/5/6, BuildId…), and on the activated path the program reads
them from the mod source as raw bytes (`UE4SSProgram.cpp:901`) — no wide conversion anywhere.
They cannot produce this exception, and the observed throw is inside the try block that wraps
ONLY `settings_manager.deserialize(...)`.

## 6. Fix and proof

Staged: `staged/Helpers/String.hpp` (see DIFF.md) routes the four non-Windows conversion entry
points through `RC::Unicode::EncodeUtf8/DecodeUtf8`. Host proof with the staged header
(`fixed_proof.cpp`, `decode_proof.cpp`, `repro_fixed`):

```
ensure_str produced 1448 units (expected 1448, garbled would be 2896)
FIXED PIPELINE: parse OK; MajorVersion=5 HookProcessInternal=0 RenderMode=ExternalThread
DecodeUtf8: 1448 bytes -> 1448 units (1:1, no interleave); DecodeUtf8-fed parse: OK
stored error message (staged): 'The IniParser failed to parse: Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey'
```

Clean-content regression (CASE A) unchanged. The staged header compiles both FORCE_U16 host
builds and (by construction — the codec is plain C++ over 2-byte units) the PROSPERO target.

## 7. Honest limits

- The exact `(8 : 2)` line:col is the host reproduction's deterministic output under a
  disassembly-faithful emulation of the broken library; the error CLASS and the byte-exact
  stored garble are the hard, verified claims (the interleave pattern is chunk-size-independent
  for this input, so the derivation is strong but not a target run).
- No composition/packaging/console validation of the staged fix (proposal-level; the compose
  freshen/regen already lists `Helpers/String.hpp` in the settings/program `.d` closure, so a
  recompose picks it up).
- `Utf16Utf8` bounds (65536 units / 196608 bytes) now cap conversions; irrelevant for the
  settings and error paths, a real (documented) cap for hypothetical >64 Ki-unit strings.
