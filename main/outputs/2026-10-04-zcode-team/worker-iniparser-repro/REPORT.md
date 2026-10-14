# REPORT — worker-iniparser-repro (2026-10-05)

LOCAL ONLY: no console contact, no dispatches, no target execution, no composition/packaging
runs. Writes confined to `outputs/2026-10-04-zcode-team/worker-iniparser-repro/` plus /tmp
scratch (`/tmp/iniparser-repro/`, `/tmp/l11cxx/`). Canonical trees untouched.

## 1. Deliverables

| file | what |
|---|---|
| `CLAIM.json` | bounded claims, root-cause chain, recovered exception, non-claims |
| `REPRO.md` | full reproduction + recovered exception + live-evidence match |
| `DIFF.md` + `staged/Helpers/String.hpp` (`7c983db4…`), `staged/String.hpp.diff` | staged fix (proposal-level) |
| `evidence/` | byte copies: both live status files, l11run2 log, staged ini (`d8faa4ec…`) |
| `harness/` | sources + built binaries: real-parser repro, bisect, byte-exact pipeline proof, fixed-pipeline proofs |

## 2. Answers to the mission, compressed

1. **Host reproduction: DONE, byte-exact.** The real port IniParser/ParserBase sources + the
   real staged ini, driven through the disassembly-faithful emulation of the target's broken
   conversions, reproduce the stored message `TeIiasrfie opre ` EXACTLY as in both live runs
   (pids 466/492) — including the trailing space.
2. **Code-difference question: answered by correction.** Nothing in the l10→l11 delta
   (luamod merge, v4r3/armed initializer, wpd2 pad units + handler) touches the parse path:
   `settings` unit, IniParser objects, and `Helpers/String.hpp` are byte-identical between the
   compositions. The premise "launch-10 passed settings parsing" was an inference —
   LAUNCH10-RESULTS.md shows every launch-10-era run refused BEFORE the program constructor.
   The wpd2 pad-coexist deferral is simply the first candidate that reaches
   `settings_manager.deserialize` on target; the latent bug fired on first exercise,
   deterministically. (Hypotheses (a)/(b)/(c) are thereby discharged: the extra deserialize
   parameter, handler pre-state, and R3-mode changes are all downstream of the failure point or
   byte-carried.)
3. **MemberVariableLayout.ini / VTableLayout.ini: EXCLUDED.** Parsed by
   `NativeLayoutBinding.cpp` (`ReadBounded`/`ParsePair`, dedicated narrow parser, mod-source
   raw bytes on the activated path) — not the IniParser; and the throw is inside the
   settings-only try block.
4. **Full exception RECOVERED:**
   `The IniParser failed to parse: Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey`
   — thrown by the real TokenParser on NUL-interleaved settings content; `(8:2)` = the
   `[General]` header under the corruption (line numbers survive it). Hard claims: the error
   class and the byte-exact stored garble; the exact line:col is a high-confidence derived
   value (host-deterministic under the faithful emulation).
5. **to_string conversion bug: FOUND AND FIXED (staged).** It is not in the port's
   `String.hpp` logic per se — the PS5 SDK's prebuilt `libc++.a` locale.cpp.o pairs short-wchar
   wrappers with 4-byte-unit helpers (`utf16_to_utf8` reads with a 4-byte stride = `[::2]`;
   `utf8_to_utf16` writes 4-byte units into 2-byte buffers = NUL-interleave; both with a
   `sarq $1` fixup proving the wrapper/compiler split). Staged fix routes the four non-Windows
   entry points of `Helpers/String.hpp` through the port's own explicit
   `RC::Unicode::EncodeUtf8/DecodeUtf8`; host-verified end-to-end.

## 3. Root cause (one line)

On `__PROSPERO__` (`CharType = wchar_t`, `sizeof==2`), `std::wstring_convert` is corrupted by
the PS5 SDK libc++ width mismatch — `ensure_str(settings_bytes)` garbles the ini (parser
throws), and `to_string(emergency_message)` garbles the stored error to
`TeIiasrfie opre ` — first exercised by the l11 combined candidate because earlier candidates
never reached the constructor.

## 4. Fix status and next steps (for the loop)

- `staged/Helpers/String.hpp` is proposal-level; fold via staged-header override or CR1-tree
  revision; recompose (settings/program/luamod object hashes WILL drift through the freshen
  closure), rerun offline gates, restage, ONE launch.
- Expected if complete: constructor passes settings; if any future error fires, its real
  suffix appears instead of the 16-char prefix garble.
- Do NOT change the staged ini — verified clean end-to-end through the real parser.
- Optional hardening (not staged): a launch-time self-check (e.g. compare
  `to_string(ensure_str("probe"))` == "probe" during early startup) would have surfaced this
  class of failure in one diagnostic line; left as a proposal.

## 5. Honest limits

- No console validation of the fix; no target run of the harness (PS5 binaries can't execute
  on this host — the library bug was proven by disassembly + byte-exact behavioral match, not
  by running target code).
- The recovered line:col carries the documented confidence caveat (see CLAIM.json non-claims).
- `Utf16Utf8` size bounds (65536 units / 196608 bytes) become conversion caps — irrelevant
  here, documented for future callers.
- No >8 quality claim, no completion claim; this is one failure's root cause + proposal fix.
