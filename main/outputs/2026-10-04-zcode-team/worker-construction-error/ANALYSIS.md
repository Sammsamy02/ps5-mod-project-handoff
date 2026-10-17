# ANALYSIS — worker-construction-error (2026-10-05, LOCAL ONLY)

Subject: the pid-466 (and 492/512) construction rejection
`Native UE4SS construction rejected: TeIiasrfie opre ` — the first in-game run of the
l11 COMBINED candidate (launch11-combined, gate GO candidate `prx-candidate-l11-2`,
built from composition run `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-2`;
lineage confirmed by `luamod.o` sha `813b249d…` matching `EDITIONS.json`).

Every claim below was derived from the SHIPPED objects and the exact toolchain, not from
source reading alone. An independent worker (worker-iniparser-repro) converged on the same
root cause via a host emulation and recovered the full exception text; the two investigations
are consistent and are cross-referenced where relevant.

---

## 1. Decode of the stored error (byte-exact)

`l11run2-ue4ss.log` line 60, byte-dumped:

```
...runtime_start_failed code=3 detail=Native UE4SS construction rejected: TeIiasrfie opre \n
```

The stored message is `TeIiasrfie opre ` — 16 bytes INCLUDING a trailing space.
The console format literal is `"The IniParser failed to parse: {}"` (33 chars).
`"The IniParser failed to parse: {}"[0::2]` = `T,e,I,i,a,s,r,f,i,e,' ',o,p,r,e,' '` =
**`TeIiasrfie opre ` — exact, character for character, including the trailing space.**

So the stored string is the [::2] of the FULL 33-char literal region (the `'{}'` braces at
indices 31/32 are odd/even: index 31 `{` is ODD → dropped; index 32 `}` is not reached — see
section 3). The substituted argument contributes NOTHING visible for any argument length.

## 2. The target's real wide-char shape (probe-verified)

Compiled `int x = sizeof(L"AB");` with the EXACT `retained-program` argv from
`run-l11-2/commands.json` via the pinned `prospero-clang++`:

```
Contents of section .rodata.str2.2:
 0000 41004200 0000    A.B...
```

`L"AB"` is stored as **2-byte UTF-16 units** (`41 00 42 00 00 00`). Byte-searching the shipped
objects confirms it at scale: `program.o`, `settings.o`, the frozen `coherent-full-1/program.o`,
and all three shipped `prx-candidate-l11-2-aside-*` PRX binaries contain the settings format
literal ONLY in 2-byte-unit encoding (u16 count 1, u32 count 0 everywhere), while every mangled
symbol still names `wchar_t` (`Iw`) because the type IS `wchar_t` — it is simply 2 bytes wide
on this toolchain. `CharType = wchar_t`, `StringType = std::wstring` with `value_size == 2`.

## 3. The broken converter (instruction-level, in the shipped link)

`program.o` (sha `1c9fed67…`) relocation order inside
`RC::UE4SSProgram::UE4SSProgram(NativeProgramContext, initializer_list<BinaryOptions>)`:

```
0x526  setup_paths(path)
0x540  SettingsManager::deserialize(path&, UE4SSProgram&)      <- the settings-only try block
0x5ca  fmt::v11::format<...>(basic_fstring<wchar_t>, wstring)  <- in the CATCH block
0x5fe  UE4SSProgram::create_emergency_console_for_early_error(wstring_view)
0x678  UE4SSProgram::validate_native_services()
```

`create_emergency_console_for_early_error` (wpd1 UE4SSProgram.cpp:859-869, `__PROSPERO__`
branch) is, verbatim in the object:

```
call RC::to_string(basic_string_view<wchar_t>)      -> std::string
call std::string::c_str()
call RC::CanError::copy_error_into_message(const char*)   -> ErrorObject::copy_error (memcpy)
```

`RC::to_string(wstring_view)` -> `RC::to_string(const std::wstring&)` ->
`std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(wstr)` — an EXPORTED
(_LIBCPP_EXPORTED_FROM_ABI) call into the SDK's prebuilt `libc++.a` (sha `b1480e6e…`).

**The only definition of `__codecvt_utf8_utf16<wchar_t>::do_out` and
`__codecvt_utf8<wchar_t>::do_in` in the entire link is the STRONG (T) copy in
`coherent-tick-file-witness-runtime.o` (sha `fb52c7f8…`); all other objects (program, settings,
luamod, unrealinitializer, bootstrap) carry `U` references only, and libc++.a's archive copy is
weak. The witness copy is byte-identical to the archive copy — both disassemble to:**

```
do_out<wchar_t>:  tail-call utf16_to_utf8(const unsigned int* frm, ...),
                  then  frm_nxt = frm + ((frm_nxt - frm) >> 1)      <- sarq %rcx (÷2 fixup)
utf16_to_utf8:    loop: r11d = (u32)*frm;  ebx = (u16)r11w;         <- movzwl %r11w,%ebx
                  ... ASCII -> 1 byte, <=0x7FF -> 2, <=0xD7FF -> 3, high surrogate +
                  next-element low half -> 4 ...
```

This is a converter built for **4-byte `wchar_t`** (each element = one UTF-16 code unit,
32-bit storage). The composition's TUs have **2-byte** `wchar_t`. Consequences, per call:

- **`to_string` (do_out)**: the helper reads the 2-byte-unit buffer two units at a time as one
  32-bit element and keeps only the LOW 16 bits — i.e. emits units 0, 2, 4, … — the [::2].
- **`to_wstring` (do_in, mirror-image)**: helper `utf8_to_utf16(unsigned char* in, …,
  unsigned int*& to_nxt, …)` writes one 4-byte element per input character into the 2-byte-unit
  buffer: every character is followed by a 0x0000 unit (NUL-interleave). The `sarq` fixup then
  halves the byte advance so the returned length "looks" right.

Both facets are therefore ABI-broken for this composition, deterministically, for ANY input.

### Element arithmetic for the exact observed bytes

Formatted message wstring (2-byte units): `[prefix 31 units: "The IniParser failed to parse: "]`
then the `ensure_str(e.what())` argument — itself NUL-interleaved by the broken `to_wstring`:
`[c0, 0, c1, 0, …]`.

`do_out` consumes `floor(N/2)` 4-byte elements and emits the LOW unit of each pair:

| element | units consumed | low unit emitted |
|---|---|---|
| 0 | 0,1 | 'T' |
| 1 | 2,3 | 'e' |
| 2 | 4,5 | 'I' |
| … | … | … |
| 14 | 28,29 | 'e' |
| 15 | 30,31 | ' ' (unit 30; unit 31 = arg's first char, in the HIGH half → dropped) |
| 16 | 32,33 | **0x0000** (the interleave NUL) → output byte 0x00 |

`copy_error` memcpys `strlen(message)` bytes → **exactly 16 visible bytes `TeIiasrfie opre `**,
terminated by the interleave NUL. The suffix content and length are UNRECOVERABLE from this
sink for ANY argument — which is why the element-16 NUL, not the argument, ends the string.

## 4. Where the throw comes from (settings path trace)

`SettingsManager.cpp` (sha `3e618c4c…` — byte-identical to launch-10's settings unit):
`deserialize(file_name, receiver)` on `__PROSPERO__` with an activated mod source:

```cpp
if (const auto& source = receiver.native_mod_source()) {          // <- taken in this candidate
    auto settings = source->read("ue4ss/UE4SS-settings.ini");
    if (!settings) throw std::runtime_error("Native activation is missing ue4ss/UE4SS-settings.ini");
    if (settings->bytes.find('\0') != settings->bytes.npos)
        throw std::runtime_error("Native UE4SS settings contain NUL");
    auto contents = ensure_str(std::string(settings->bytes));     // <- NUL-interleave (section 3)
    parser.parse(contents);                                       // <- real IniParser throws here
}
```

`ensure_str(std::string)` -> `to_wstring` -> broken `do_in` -> interleaved wstring ->
the retained IniParser rejects the garbage. worker-iniparser-repro drove the REAL port
IniParser/ParserBase sources + the REAL staged ini through a disassembly-faithful host
emulation of these two facets and recovered the exception byte-exactly (their
`harness/final_pipeline`, `MATCH: YES` against both live run logs):

```
The IniParser failed to parse: Syntax error(8 : 2): Expected state NewLineStarted or
StartOfFile, got CreateSectionKey
```

(`(8:2)` = where the interleaved `[General]` header lands after corruption; host-derived,
high confidence.) **The staged ini itself is CLEAN** (sha `d8faa4ec…`, the pinned v3 file):
through the fixed conversion it parses end-to-end with correct values (their `fixed_proof`:
`parse OK; MajorVersion=5 HookProcessInternal=0 RenderMode=ExternalThread`).

### Why NOW (l10 → l11 delta)

Nothing in the combined delta (merged luamod, v4r3/armed initializer, wpd1 pad-defer,
wpd2 coexist + handler) touches the parse path: settings unit, IniParser objects and
`Helpers/String.hpp` are byte-identical between the compositions. The premise that
"launch-10 passed settings" was wrong: **every launch-10-era run refused BEFORE the program
constructor ever ran** — pid 317 at the activation ASCII check, 341 at MFA size, 355/370 at
the BACKEND's eager pad activation (`phase=6` was emitted before the ctor; the 235
"constructor pairs return 0" are the bridge's module-prologue ctors, not `UE4SSProgram`).
The wpd2 coexist deferral is simply the FIRST candidate whose construction reaches
`settings_manager.deserialize` — the latent ABI break fired deterministically on first
exercise (pid-466 epoch: `pad_input_deferred` seq 538 t=76163232 → `runtime_start_failed`
seq 540 t=76163238).

### VTableLayout.ini (coordinator task 3) — EXCLUDED

`VTableLayout.ini`/`MemberVariableLayout.ini` are parsed by `NativeLayoutBinding.cpp`
(`ReadBounded`/`ParsePair`, a dedicated narrow parser over mod-source raw bytes) — not the
IniParser — and the throw sits inside the settings-only try block (section-4 relocation
order). A partial/cancelled VTableLayout.ini upload therefore cannot produce THIS error.
Its console state is still unverified from local evidence — restage it cleanly at the next
window as hygiene — but the settings ini needs no restage (verified clean).

## 5. Construction-path ErrorObject writers (original task 1, complete)

Every writer reachable between ctor start and the `native_startup_error_message()` check in
program-wpd1:

| site | call | reachable in ctor? | note |
|---|---|---|---|
| UE4SSProgram.cpp:234 catch → `create_emergency_console_for_early_error` → `copy_error_into_message(to_string(...).c_str())` (:863) | ErrorObject copy | **YES — the one that fired** | settings deserialize only |
| :1813 `set_error("is_directory ran into error %d", ec.value())` | snprintf, narrow, clean | no — mod discovery, post-ctor | |
| :2166/:2176 `set_error(error_message.c_str())` | narrow std::string, clean | no — start_native_mods/inventory | |
| validate_native_services (:733-797) | `throw std::runtime_error("…")` only | yes, but AFTER settings; throws are caught by the BACKEND try (ue4ss_backend.cpp:47-50), never written to the ErrorObject | all clean narrow strings |
| CanError::set_error variants (CanError.hpp:25-55) | not called on the ctor path | — | MPGRAM_USE_EXCEPTIONS not defined |

So the only possible stored-error content during construction is the settings-catch message —
consistent with the decode. The backend then concatenates
`"Native UE4SS construction rejected: " + native_startup_error_message()` at
ue4ss_backend.cpp:50 and `module_start` refuses with `code=3` (seq 540).

## 6. wpd2 coexist audit (original tasks 2-3, secondary)

- `input-handler-coexist/Handler.cpp` (compiled as `retained-input-handler`): no raw pointer
  arithmetic, no buffer writes, no formatting into char arrays; the only string ops are
  std::string/std::function plumbing; it never touches the ErrorObject. `Handler.hpp` is
  byte-unchanged (no new members).
- `PS5PadSource.hpp` append-only members (`m_polls_since_retry`, `m_retry_interval{200}`,
  `m_retry_attempts` + `maybe_retry()`): in the composed PRX the two provider TUs
  (`retained-native-pad-PS5PadSource` = staged PS5PadSource.cpp, `retained-native-pad-PS5PadNative`
  = staged byte-identical relocation of PS5PadNative.cpp, the `make_shared` ALLOCATOR) compile
  side-by-side in the staged port directory and resolve `PS5PadSource.hpp` via quoted-include
  self-directory — one header, ODR-consistent, allocation size in step with the appended
  members. The composition's unit list contains NO other TU that allocates PS5PadSource (no
  pad-config unit is compiled in run-l11-2); `retained-native-pad-input` has the BASE provider
  dir on its `-I` (resolving the base header for views) but does not allocate the object.
  worker-pad-coexist DIFF.md §4's heap-buffer-overflow was a host-TEST recipe issue (fixed
  inside their test); it does not exist in the composed PRX.
- Caveat: verified via the run's unit list + include-resolution map, not by symbol-level
  proof of every PS5PadSource reference. The construction failure is fully explained by
  sections 1-4 and does not involve the pad units (their deferral worked: seq 538-539).

## 7. Fix (coordinator task 5) — verification summary

The staged fix (this dir, `staged/Helpers/String.hpp`, sha `7c983db4…`, byte-identical to
worker-iniparser-repro's independently derived fix) routes all four non-Windows conversion
entry points of `Helpers/String.hpp` (`to_wstring(string)`, `to_wstring(string_view)`,
`to_string(wstring)`, `to_string(u16string_view)`) through the port's own explicit
`RC::Unicode::EncodeUtf8/DecodeUtf8` (Helpers/Utf16Utf8.hpp — explicit 2-byte-CharType codec,
correct by construction, `static_assert(sizeof(RC::CharType) == 2)`), bypassing
`std::wstring_convert` entirely.

My verification (in addition to iniparser-repro's host behavioral proofs):

- `receipts/compile-program-wpd1-with-fix.log` — real wpd1 UE4SSProgram.cpp, staged header
  FIRST on the include path, exact run-l11-2 retained-program `-I`/`-D`/std flags:
  **exit 0, 0 errors** (syntax+semantic full TU check on the target compiler).
- `receipts/compile-settings-with-fix.log` — real SettingsManager.cpp (the parse-path TU):
  **exit 0, 0 errors, object emitted**.
- iniparser-repro `fixed_proof` (rerun by me): `ensure_str produced 1448 units (expected 1448,
  garbled would be 2896)`; `FIXED PIPELINE: parse OK; MajorVersion=5 …`; stored message becomes
  the full clean text with the real parser suffix.

One fix covers BOTH coordinator asks: the [::2] stored-error garble AND the settings parse
failure — the latter is CAUSED by the former's mirror (NUL-interleave). No ini restage needed.

## 8. Fold notes for the compose chain (controller-owned)

- Fold the staged header via the composition's staged-header-override path (or a CR1-tree
  revision). `Helpers/String.hpp` is in the freshen closure: `settings`, `program`, `luamod`
  (and any TU including it) object hashes WILL drift — run the freshen-deps regen, then the
  offline gates (ODR emitter, finalize, gate v2, comparator discipline), then restage.
- Do NOT edit the pinned staged ini (`d8faa4ec…`).
- Optional hardening (proposal, not staged): early-startup self-check
  `to_string(ensure_str("probe")) == "probe"` with a diagnostic_event tag — would surface this
  failure class in one line instead of a masked 16-char garble.
- Expected next-run evidence: ctor passes settings (Tier-0 `[R3]` lines follow); if any
  future error fires, its FULL message (real suffix) appears instead of the 16-char prefix.
