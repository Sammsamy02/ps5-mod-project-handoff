# REPORT — worker-construction-error (2026-10-05)

LOCAL ONLY: no console contact, no dispatches, no composition/packaging runs, no lineage edits.
Writes confined to this dir (+ /tmp scratch for disassembly/probe work). CLAIM.json written
first, ANALYSIS.md + STAGED-FIX.md + receipts follow, this REPORT last.

## 1. Verdict

**ROOT CAUSE FOUND AND PROVEN AT INSTRUCTION LEVEL; STAGED FIX COMPILE-VERIFIED IN SITU.**
The pid-466/492/512 `Native UE4SS construction rejected: TeIiasrfie opre ` failure is a
short-wchar vs 4-byte-wchar ABI break inside the PS5 SDK's prebuilt libc++ conversion facets,
firing on the first candidate that ever reached `UE4SSProgram`'s settings deserialize.

## 2. Root cause (one line)

On the PS5 composition, `wchar_t` is 2 bytes (probe: `L"AB"` → `.rodata.str2.2 = 41 00 42 00`
under the exact run-l11-2 argv; every STR literal in the shipped objects is UTF-16-encoded),
but the prebuilt `libc++.a` — and its byte-identical STRONG copy in
`coherent-tick-file-witness-runtime.o`, the only do_in/do_out definition in the link —
implements `codecvt_utf8{,_utf16}<wchar_t>::do_in/do_out` with 4-byte-element helpers
(`movzwl %r11w,%ebx` low-16 extraction; `sarq $1` to_next fixup). Every
`std::wstring_convert` call therefore corrupts data: `to_wstring` NUL-interleaves
(every character followed by a 0x0000 unit), `to_string` drops every other character ([::2]).

## 3. The failure chain (all steps verified in shipped binaries)

1. Backend ctor (activated path): `ConfigureRetainedPS5PadInput` → `pad_input_deferred`
   (seq 538, phase 6, 0x80920004) — **the wpd1 deferral worked**; geometry green (seq 532-537).
2. `make_unique<UE4SSProgram>` → ctor → `setup_paths` → `settings_manager.deserialize(path, *this)`
   → mod-source branch (SettingsManager.cpp:60-67): `ensure_str(std::string(settings->bytes))`
   NUL-interleaves the (clean, `d8faa4ec…` v3) settings text → `parser.parse(contents)` throws
   `Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey`
   (worker-iniparser-repro, disassembly-faithful host pipeline, byte-exact `MATCH: YES`).
3. Ctor catch (UE4SSProgram.cpp:232-236) →
   `create_emergency_console_for_early_error(fmt::format(STR("The IniParser failed to parse: {}"), ensure_str(e.what())))`
   → `to_string` [::2]-downconverts → `copy_error_into_message` stores `TeIiasrfie opre `
   (16 bytes; element 16 of the broken conversion is the interleave NUL, so strlen stops at 16
   for ANY suffix — the suffix is uninformative from this sink).
4. Backend (ue4ss_backend.cpp:50) throws `Native UE4SS construction rejected: …` →
   `runtime_start_failed code=3` (seq 540), mod never loads.

## 4. Why the combined candidate and not earlier

Nothing in the l10→l11 delta touches the parse path (settings unit, IniParser, String.hpp all
byte-identical). Every launch-10-era run refused BEFORE the program constructor existed
(activation ASCII / MFA size / backend pad phase-6 — the "235 ctor pairs" are bridge
module-prologue ctors). The wpd2 coexist deferral is simply the first candidate to reach
`settings_manager.deserialize` — the latent ABI break fired deterministically on first
exercise (three consecutive boots: 466/492/512).

## 5. Answers to the dispatched tasks

1. **to_string conversion bug**: found at instruction level (ANALYSIS.md section 3) —
   `utf16_to_utf8(const unsigned int*,…)` reads 4-byte elements keeping the low 16 bits; plus
   the mirror `utf8_to_utf16` 4-byte-unit writer behind `ensure_str`. Not a bug in the port's
   String.hpp logic — an ABI mismatch with the prebuilt libc++.
2. **Settings path**: mod-source branch, `read("ue4ss/UE4SS-settings.ini")`,
   `ensure_str(std::string)` interleave → parse throw; why-now answered (section 4); the staged
   ini is clean — no restage needed.
3. **VTableLayout.ini**: EXCLUDED — parsed by NativeLayoutBinding.cpp's dedicated narrow
   parser, not the IniParser; the throw is inside the settings-only try block (program.o
   relocation order: setup_paths@0x526 → deserialize@0x540 → catch{fmt@0x5ca → emergency@0x5fe}
   → validate_native_services@0x678). Restage the (possibly partial) console VTableLayout.ini
   as hygiene, but it did not cause this failure.
4. **Suffix recovery**: full exception recovered via worker-iniparser-repro's host emulation
   (section 3, step 2); not derivable from the console sink (masked by the interleave NUL).
5. **Staged fix**: `staged/Helpers/String.hpp` (sha `7c983db4…`, identical to
   worker-iniparser-repro's independent fix) — routes the four non-Windows conversion entry
   points through the port's explicit `RC::Unicode::EncodeUtf8/DecodeUtf8`. Compiled clean
   (exit 0, 0 errors) on the REAL wpd1 program TU AND the REAL settings TU under the exact
   run-l11-2 argv (`receipts/`). Fixes BOTH the parse failure and the stored-error garble.

Secondary (original mission): construction-path ErrorObject writers enumerated — the settings
catch is the ONLY one reachable during construction (ANALYSIS.md section 5); wpd2 Handler/
PS5PadSource audited clean for the composed PRX (Handler.hpp byte-unchanged; provider TUs
self-consistent via staged quoted-include pair; no other allocating TU in the unit list;
DIFF.md §4 hazard was host-test-only) — caveat: include-map level, not symbol-level proof
(ANALYSIS.md section 6).

## 6. Deliverables

| file | what |
|---|---|
| `CLAIM.json` | bounded claims CE1-CE9, non-claims, evidence hashes |
| `ANALYSIS.md` | full technical analysis (decode, probe, disassembly, element arithmetic, settings trace, why-now, VTableLayout exclusion, set_error enumeration, wpd2 audit, fold notes) |
| `staged/Helpers/String.hpp` | the staged fix (sha `7c983db4…`, provenance worker-iniparser-repro, independently verified here) |
| `STAGED-FIX.md` | fix content, verification matrix, fold instructions for the compose chain |
| `receipts/compile-program-wpd1-with-fix.log` | target-compiler syntax+semantic run of the real program TU with the staged header (exit 0) |
| `receipts/compile-settings-with-fix.log` | same for the real SettingsManager.cpp (exit 0, object emitted) |

## 7. Non-claims

LOCAL ONLY — no console validation of the fix; the next launch belongs to the controller's
compose chain (freshen-deps regen + offline gates + restage per STAGED-FIX.md §Fold).
The `(8 : 2)` position is host-derived, high confidence, not target-verified. No >8 quality,
no completion claim.
