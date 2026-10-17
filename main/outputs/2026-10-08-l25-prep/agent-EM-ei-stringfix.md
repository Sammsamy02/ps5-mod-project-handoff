# agent-EM — EI ToString-bind launch failure: string fixes + prologue mystery

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (candidate/src/UnrealInitializer.cpp + include/EngineToStringProfile.hpp)
Console evidence: `/tmp/bind1.log` (2026-10-09 10:37 run, deployed package-B rebuilt 10:34).
All line numbers below refer to the pre-fix tree state.

## 1. Root cause of the prefix refusal — the char16_t/sizeof suspect is DISPROVEN; the real cause is the L26 early-release owner-gate disarm

**Suspect verification (task fix 2).** `include/EngineToStringProfile.hpp:89-95` declares
`kToStringInternalPrefix` as `inline constexpr uint8_t[...]` with `static_assert(sizeof(...) == 32)`,
byte-exact vs the 32-byte pin, and the Phase 4c call passes
`sizeof(rc::engtostring::kToStringInternalPrefix)` == 32 — the identical size discipline as slot 2's
`rc::fnameprofile::kPrefix` (also `uint8_t[32]`, FNameAbiProfile.hpp:5). There is no char16_t array,
no 2x sizeof, no NULL/alignment precondition failure. **Corrected (unchanged) bytes = the pin:**
`55 48 89 e5 41 57 41 56 41 55 41 54 53 50 80 3d 8b 64 12 09 00 44 8b 3f 49 89 f6 44 8d 6a ff 48`
(byte-compared again in the fixture, `tests/actual_ei_stringfix.cpp` §2).

**Actual root cause.** `rc::fnameabi::signature` (resolved from
`outputs/2026-10-07-l19-local-profile-r4/units/NativeFNameChecks.hpp` — confirmed by the deployed
build's dep file) checks `ops.known_fault(address,size)` BEFORE `ops.query()`/VirtualQuery, with the
reason literal `"constructor prefix has recorded copyin fault"` — the only pre-VirtualQuery reason
starting "constructor prefix has", matching the console exactly (refusal with NO L14 VirtualQuery
line for 0x63FA00). `known_fault` → `bridge_copyin_range_faulted` (runtime_start_bridge.cpp:1198):
```c
if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no ledger read
```
The **L26 early-release gating patch** (agent-Z15 proposal, applied by agent EE; present in this
morning's deployed bridge, absent in yesterday's preDA — diff-verified) added
`g_copyin_initializer_owner.leave()` to `bridge_copyin_initializer_complete()`. UnrealInitializer.cpp
called `copyin_cleanup.complete()` at line 1228 — **between slot-2 admission and Phase 4c** — so:

1. Slot 2's signature at 0x63D590 ran while the gate was armed → VirtualQuery L14 line (log :550) →
   authenticated-disk-XOM cached path → matched (L18, log :551-552).
2. `copyin_cleanup.complete()` → owner gate disarmed.
3. EI's Phase 4c signature at 0x63FA00 → `known_fault` fails CLOSED → refused
   "constructor prefix has recorded copyin fault" BEFORE VirtualQuery (log :560 — the console
   mystery explained: it never reached the memory query).
4. Phase 5 SCO then failed `code_identity` the same way (bridge_code_cache_context is gated on the
   same owner) → `seed_refused` THREW → unwind printed the L15 summary (binding_log destructor,
   log :561) → terminal seed refusal → "Stopping mod … for uninstall" (log :562-563) → unmodded game.

**Fix (in-tree):** the concurrent agent-EP sequencing fix had the right intent — move
`copyin_cleanup.complete()` after Phase 6 — but its edit landed the two statements AFTER the
function's closing braces and **did not compile** (proven by my first Prospero compile: `error:
unknown type name 'copyin_cleanup'`). I repaired the placement: `complete()` now runs inside
`consume_required_slot_seeds` immediately after Phase 6's `#endif`, before the function closes, with
a comment documenting the 2026-09 phantom-refusal mechanics. All gated admissions (4c/5/6) now run
while the code cache/gate is live; the bank still retires before anything else; the RAII
CompletionCleanup destructor still covers early unwind. (Note for the controller: the L25
"gate remains armed until shutdown" console text is stale vs the L26 early-release behavior; the
gating assumption "no post-admission copyin consumer exists" is false since L27.)

## 2. The two string fixes

**Actual types:** `rc::fnameabi::Check { bool ok; const char* reason; const char* source; }` —
NARROW literals; `L15BindingLog::outcome` is `const char*`; `ts_signature.reason/source`,
`dual.reason`, `seed_class_name(cls)` are all narrow `const char*`. The reason was never a wide
literal — `ensure_str()` WIDENED it: `ensure_str` → `ensure_str_as<CharType>` (2-byte CharType) and
the PS5 SDK's prebuilt libc++ `std::wstring_convert` "consumes/produces every OTHER character on a
short-wchar target" (documented staged-fix note in Helpers/String.hpp:15-23). Hex dumps of the log
prove every `ensure_str(<narrow>)` argument rendered as raw UTF-16LE bytes truncated to EXACTLY half
its unit count: 'constructor prefix has' (22 of 44), 'outcome=verified-' (9 of 18 =
"verified-and-bound"), 'source=authenticated-di' (16 of 32), sha256 halved to 32 of 64 chars. The
"L18 success" line garbles identically — it never throws, which is why it looked successful.

**Fix:** new `noexcept`-equivalent helper `send_narrow_diag(as_error, fallback, make_narrow)` in
UnrealInitializer.cpp (before L14TotalAdmissionLog): format NARROW first (`fmt::format` on a narrow
literal), widen ONCE through the explicit locale-free codec `RC::Unicode::DecodeUtf8`
(Helpers/Utf16Utf8.hpp — the sanctioned codec per the staged-fix note), send via the no-format
`Output::send(StringViewType)` / `send<LogLevel::Error>` overloads (literal-only wide text is proven
clean on the console — the L25 line with integer args is byte-clean), and degrade to a plain narrow
literal fallback on ANY failure. Applied to:
- `L15BindingLog::~L15BindingLog` (the L15 outcome summary — full "verified-and-bound" now logs);
- Phase 4c class refusal (was :1270), prologue-signature refusal (was :1287, reason now logs in
  full), dual-check disabled line (was :1330), and both success summary lines (was :1336-1339).

## 3. Teardown safety

Every EI Phase 4c / L15 summary send now goes through `send_narrow_diag`: total-exception-free with
per-line literal fallback, so no formatting/encode failure can unwind between Phase 4c completion and
mod start. The strict encode stage (PosixFile::encode_utf8_into) now receives clean UTF-16 from
DecodeUtf8; even if it or fmt throws, the fallback literal is emitted and admission continues.
(Integer-only lines keep the plain STR() form — proven clean by the L25 line.)

## Verification

- **(a) Host compile (Z13 recipe, real Prospero toolchain):** official archived l26-B pass1 argv for
  `unreal-unrealinitializer` with `-c/-o/-MF/-ivfsoverlay` redirected to the edited tree candidate —
  **exit 0, 0 errors** (6 pre-existing port warnings). Receipt:
  `results/stringfix-verify/HOST-COMPILE.json` (+ .o/.d/.log alongside).
- **(b) Focused fixture** `tests/actual_ei_stringfix.cpp` (REAL EngineToStringProfile.hpp constants,
  REAL Unicode codec, verbatim helper, REAL signature() pre-VirtualQuery gate order with mock ops):
  (i) forced refusal (owner-gate fail-closed) reproduces refusal BEFORE any query
  (`query_calls == 0`) and the FIXED log path emits the FULL narrow reason with no NUL bytes and no
  throw, execution continues; (ii) corrected prefix: `sizeof == 32`, element size 1, byte-equal to
  the 0x243A00 pin, mock signature() passes 32-vs-32 and performs the VirtualQuery; (iii) L15 outcome
  logs full "verified-and-bound"; (iv) degradation: invalid-UTF-8 producer and throwing producer
  both fall back to the plain literal, nothing escapes. **plain + asan-ubsan: exit 0,
  "ALL L27 EI STRINGFIX FIXTURE CHECKS PASSED"** — receipt:
  `results/HOST-TESTS-L27-STRINGFIX.json`.
- **(c) CHANGES.json synced:** UnrealInitializer.cpp `candidate_sha256` refreshed
  (badde0927f8b4205be83aff772c956540d05fe730ce5539ead158d986b479541), `review_fixes` entry appended,
  `l27_ei_stringfix` metadata block added.

## Known remaining same-class garble (out of scope, no touch)

`[Seeds][L18]` slot-2 source line (UnrealInitializer.cpp, `ensure_str(signature.source)`) and the
`[Seeds][L14]` total-admission outcome still widen via ensure_str; they garble on console but are
throw-guarded. The same narrow-first conversion applies if wanted (not part of this assignment).
