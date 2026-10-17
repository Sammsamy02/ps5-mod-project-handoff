# Agent CL — logwriter F1-F4 implementation in the l26 tree (2026-10-08)

Implemented CE's proven fix (logwriter-proposal/FIX-PROPOSAL.md) as staged
candidates under `outputs/2026-10-08-l26-runtime/typescan-r4/` per the Z9/O
consumption law. No port-root, l22/l23/l24/l25, profile/, thread/, or driver
file was edited. Port roots re-verified pristine against the l25-A
SOURCE-SELECTION pins after all work.

## 1. Files staged + CHANGES.json rows

Originals = the exact l25-consumed paths (verified: l25-A pins byte-identical,
and the l25 build has NO row/alias touching any of these four files — the only
regex-matching affected unit was `lua-luafoutputdevice`, via unrelated headers).
All rows in `files` AND legacy `changes`; `copied_original` byte-exact;
candidate under l26-runtime. Carried rows + dir keys were rebased
l25-runtime→l26-runtime (trees verified byte-identical first); 24 rows total
now validate against the driver contract (hashes, HERE-relative candidates,
no duplicate originals).

| original (l25-consumed, port root) | sha256 (head) | candidate |
|---|---|---|
| `…/port/first/include/DynamicOutput/FileDevice.hpp` | 199b6ae1b691 | `typescan-r4/candidate/include/DynamicOutput/FileDevice.hpp` → d223f9a26174 |
| `…/port/first/src/DynamicOutput/Output.cpp` | 7cf82edb4562 | `typescan-r4/candidate/src/DynamicOutput/Output.cpp` → 6d56fe356901 |
| `…/port/first/src/File/FileType/PosixFile.cpp` | ec60996019c6 | `typescan-r4/candidate/src/File/FileType/PosixFile.cpp` → 5faac290fcf1 |
| `…/port/ue4ss/src/LuaLibrary.cpp` | 1f9455ad38d4 | `typescan-r4/candidate/src/LuaLibrary.cpp` → 2cada608acf7 |

Consumption reach (from l24 receipts): FileDevice.hpp = 16 dep-universe units
(header row aliases by (name,digest)); Output.cpp = unit `output`; PosixFile.cpp
= unit `posix-file`; LuaLibrary.cpp = unit `lua-library`. A dated
`review_fixes` entry citing CE's proposal was added to CHANGES.json.

Deps/universe facts used: lua-library's dep set already contains port
`Helpers/Utf16Utf8.hpp` (transitively via port `Helpers/String.hpp`), so F4
adds NO include — include graph unchanged. `Unicode::DecodeUtf8` is visible
unqualified from namespace RC::LuaLibrary.

## 2. Implementation (per proposal; region shas in logwriter-impl-verify/EXTRACTION-SHAS.txt)

- F1 `FileDevice.hpp`: `#include <mutex>`, `mutable std::recursive_mutex
  m_write_mutex`, overrides `lock()/unlock()` (WriterDevice pattern), AND
  `receive()` holds the mutex across lazy `start_device()` + formatter +
  write (also fixes the lazy-open race itself). Recursive ⇒ deadlock-safe
  with F2.
- F2 `Output.cpp` `send(StringViewType)`: `const Lock lock{};` before the
  device loop (RAII Output::Lock locks/unlocks all default devices).
  DELIBERATE SCOPE NOTE: the template `send` overloads in the VENDORED
  `deps/first/DynamicOutput/.../Output.hpp` (189 dep-universe units) were NOT
  edited to avoid a 189-unit rebuild; instead F1's receive-level guard makes
  every send path (template and non-template) message-atomic at the device.
  Both writers in the evidence (LuaLibrary:102 non-template; [l22] template)
  are covered — harness proves it.
- F3 `PosixFile.cpp`: `encode_utf8` → appending `encode_utf8_into(text, out)`;
  `write_string_to_file` stages the COMPLETE UTF-8 image in a
  `static thread_local std::vector<uint8_t>` then one `write_all` under the
  device lock. thread_local (not plain static) so unrelated files never share
  one buffer; zero steady-state allocations once the per-thread capacity
  settles; message-atomicity holds for ANY ::write chunking.
- F4 `LuaLibrary.cpp` `global_print`: all three conversions
  (payload/nil/error) → `Unicode::DecodeUtf8(std::string_view{…})`
  (port Helpers/Utf16Utf8.hpp codec). Same output as the port-root
  to_generic_string chain for valid UTF-8; immune to any codecvt/
  wstring_convert resolution. F5 (optional try/catch at the send) NOT
  implemented — task scoped F1-F4.

## 3. Harness (logwriter-impl-verify/) — REAL edited regions

`build_and_run.sh` compiles the actual staged `Output.cpp` / `PosixFile.cpp`
(variant chosen by -I order, `-Dwrite=chunked_write` forces 32-byte short
writes) + port `OutputDevice.cpp`/`File.cpp`, links runnable binaries, and
extracts the conversion statement VERBATIM (whole-line + generated-inc shas
recorded). Include-resolution proof per binary in EXTRACTION-SHAS.txt (-H
shows candidate/…/FileDevice.hpp for bin-fixed). Host profile: FORCE_U16
(the tree's own 2-byte profile; a runnable -fshort-wchar binary SIGABRTs on
host libc++ — that flag is compile-only, see matrix). time_shim.cpp replaces
Time.cpp's fmt::chrono localized %X (host libc++ has no char16_t time_put
facet) with identical timestamp text — timestamps are not an F1-F4 unit.

Results (HARNESS-RUN.txt; scenario logs in out/):
- deployed-NUL sim (orig units + codecvt-stride sim): 925 corrupted lines,
  79600 NUL bytes, 0/400 full tails — console byte pattern reproduced
  (`[Lua] [\x00T\x00o\x00w\x00n…`).
- orig units race: 284 corrupted, 0 NULs (real code, real race).
- CANDIDATE units race: **0 corrupted, 400/400 [Lua], 800/800 [l22],
  400/400 full tails, 0 NUL bytes**; 5/5 stability reruns identical.
- Parity (serialized, timestamps normalized): original vs candidate logs
  byte-IDENTICAL (35640 bytes) ⇒ zero log-content change.
- Per-thread order: [l22] seq strictly increasing 1000000..1000799 in the
  fixed log (writer's own order preserved); the racing original's extracted
  seqs are non-monotonic (displaced tails).
- VERDICT line: PASS. (FORCE_U16 first run preserved in
  HARNESS-RUN-forceu16.txt / out-forceu16/: same PASS.)

An earlier harness build accidentally resolved the port FileDevice.hpp
(include-order bug) and "failed" — fixed by putting the variant -I FIRST;
the instrumented debug trace (6400 chunks, all contiguous) confirmed the
locking behavior before the final runs.

## 4. Compile matrix (COMPILE-MATRIX.txt, compile-matrix.sh)

18/18 PASS — for each of output / posix-file / lua-library × original /
candidate: host `-fsyntax-only`, host `-c` (Z13 recipe: -D__PROSPERO__
-D__SCE__ -fshort-wchar, stubs + l24 vfs overlay, source swapped), and
prospero-clang++ `-fsyntax-only` (exact l24 receipt argv, source swapped,
variant include prepended; LLVM_CONFIG env set). lua-library's prospero
compile resolves the CANDIDATE FileDevice.hpp (-H proof recorded), so the
header is target-compiler-clean through a real consumer.

## 5. Marker-string impact: none

`[Lua] `, `[l22] `, timestamps, payload text and per-thread order are
byte-identical (parity + order checks above). The only behavior deltas are
atomicity, the removed codecvt ambiguity (identical units for valid UTF-8;
invalid UTF-8 now surfaces through the existing catch as an
"Error converting value:" line instead of potentially NUL-interleaved bytes),
and zero steady-state write-side allocations.

## 6. Parent TODOs / honest limits

1. `l26-runtime/profile/CHANGES.json` and `l26-runtime/thread/CHANGES.json`
   still point candidates at l25-runtime paths — they FAIL
   `candidate.is_relative_to(HERE)` under an l26 driver run; their owner must
   rebase exactly as done for typescan-r4 (single owner per part).
2. `l26-runtime/build_runtime.py` is the l25 copy (BASE=l24-build-r2,
   OUT=l25-build-r2) — parent adjusts OUT to a fresh l26 dir before running;
   with BASE unchanged, both the carried rows and my new rows are live
   one-hop (originals keyed on l24/l25-consumed paths).
3. Expected affected units in the twin build: the 16 FileDevice.hpp
   dep-consumers + `output` + `posix-file` + `lua-library` (plus anything the
   driver derives); confirm they appear in CHANGE-SELECTION.json
   affected_units before packaging.
4. Console validation (rebuild, fake-sign per PS5 rules, zzdiag long-line
   re-run) remains for the next authorized build window; nothing touched the
   console in this work.
