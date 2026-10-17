# Agent CE — logwriter interleaving defect: diagnosis + fix design (2026-10-08)

READ-ONLY analysis; proposal only, no tree edits. Work products:
`outputs/2026-10-08-l26-runtime/logwriter-proposal/` (`FIX-PROPOSAL.md`,
`race_repro.cpp`, `harness-run.txt`, `race_repro` binary, scenario logs).
PORT = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`.

## 1. The two writers and the shared sink (citations)

- **Writer 1 — Lua print, wide payload, `[Lua]` prefix:**
  `PORT/ue4ss/src/LuaLibrary.cpp:35-110` (`global_print`): builds a 2-byte-unit
  `StringType` from `luaL_tolstring` results, then one call at `:102`
  `Output::send(formatted_string)`.
- **Writer 2 — `[l22]` normal-char lines:** the l22-diagnostics drain loop,
  `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp:4300,4305`
  (deployed overlay on PORT LuaMod; identical body in
  `outputs/2026-10-08-l23-package/review-l23-.../implementation/thread/src/LuaMod.cpp:4362,4367`),
  sending `Output::send(STR("[l22] ..."), args)` around `call_function`.
- **Shared sink (one file handle, one fd, no lock):**
  `Output::send` `PORT/first/src/DynamicOutput/Output.cpp:31-46` → the single
  default device `Output::NewFileDevice` (UE4SS.log) registered at
  `PORT/ue4ss/src/UE4SSProgram.cpp:256-257`, fd opened `O_WRONLY|O_APPEND` in
  `PosixFile::open_file` `PORT/first/src/File/FileType/PosixFile.cpp:244-273` →
  `FileDevice::receive` `PORT/first/include/DynamicOutput/FileDevice.hpp:79-90` →
  `default_format_string` (timestamp) `PORT/first/src/DynamicOutput/OutputDevice.cpp:35-38` →
  `PosixFile::write_string_to_file` `PosixFile.cpp:293-299` → `encode_utf8`
  `:138-156` → `write_all` `:70-80` (looped `::write`).
  **No mutex exists on this chain**: base `OutputDevice::lock/unlock` are no-op
  defaults, `FileDevice` doesn't override them, the RAII `Output::Lock`
  (vendored `DynamicOutput/Output.hpp:409-448`) is unused by `send`, and only
  `WriterDevice` (not this device) carries a recursive_mutex
  (`PORT/first/include/DynamicOutput/WriterDevice.hpp:28`).

## 2. Race mechanism (evidence + code; ~5 lines)

1. True two-thread race: the drain loop logs entry → `call_function` →
   `callback returned` on one thread, so those bytes cannot land inside the
   same callback's `[Lua]` message on a single thread — yet they do (evidence
   offset ~131601), and `ref=6`/`ref=12` Lua states interleave at 19:09:53.
   The concurrent writer is the mod async worker (LuaMod.cpp:6440 starts
   `start_async_thread()` before the main script runs, so async prints race
   loader/drain `[l22]` lines; the 19:09:37 "Loaded v1.0" print is cut by the
   loader thread's own "Lua mod started (main script returned)" line).
2. A message is not write-atomic: `write_all` splits on short `::write`
   returns; the other thread's complete lines land between segments. Evidence
   carries the displaced tails as orphan fragments (`\x00s\x00t\x00a`,
   `\x00\t\x00u`); tails are lost when `io_error` throws mid-message (escapes
   `global_print`'s conversion-only try and aborts the Lua callback).
3. The NUL-interleaved wide payload (`T\x00o\x00w\x00n…`) is a separate
   deterministic bug: the PS5 short-wchar codecvt stride corruption documented
   in `PORT/first/include/Helpers/String.hpp:15-22` (deployed binary still
   emits it — vendored codecvt header
   `deps/first/Helpers/include/Helpers/String.hpp:200-205/267-272` resolves in
   some TUs). It doubles message size (worse partial-write exposure) and
   destroys readability, but it is NOT the interleaver: conversion never calls
   the writer mid-message.

## 3. Fix shape (minimal, allocation-light, R3-safe; details + diffs in FIX-PROPOSAL.md)

- **F1** `FileDevice`: override `lock()/unlock()` with a
  `mutable std::recursive_mutex` (WriterDevice pattern).
- **F2** `Output::send` (all overloads): hold the existing `Output::Lock`
  around the device loop — one line per overload.
- **F3** keep whole-message `encode_utf8` staging; `write_all` now runs under
  the device lock ⇒ message-atomic. Optional: static staging buffer under the
  lock for zero steady-state allocations.
- **F4** `global_print`: convert the `luaL_tolstring` payload via the explicit
  `Unicode::DecodeUtf8` (already in tree) instead of `to_generic_string`, so
  the codecvt NUL-interleave disappears regardless of header resolution; also
  fix include order so the port-root fixed `Helpers/String.hpp` wins globally.
- **F5** optional: observe/try-catch around the send at LuaLibrary.cpp:102 so
  an output failure can't abort the mod callback (today's "unrecoverable" tails).
  No engine calls anywhere; works from game thread, drain contexts, async workers.

## 4. Host compile-proof status

`clang++ -std=c++20 -O2` (Apple arm64), two threads, simulated target short
writes (32B chunks): baseline reproduces the console byte pattern
(`[Lua] [\x00T\x00o\x00w\x00n…` cut by a full `[l22]` line, tail displaced) —
124 corrupted lines; conversion-fix-alone still interleaves (75);
lock+atomic-only → 0; full fix → 0 with fully readable payloads. **PASS.**
Raw output in `logwriter-proposal/harness-run.txt`.

## 5. Notes / honest limits

- Binary archaeology: the deployed prx's exact TU→header mapping can't be
  reproduced from the l23 packaging tree (no per-TU dep files kept), so F4's
  "which TUs compiled the codecvt header" claim rests on the output bytes +
  the two candidate headers on disk, not on a compile receipt.
- The harness simulates the target's short-write behavior (chunked writes);
  the real PS5 FS partial-write sizes are unknown — the lock makes the fix
  correct for any chunking, which is why F1-F3 (not single-write-only) is the
  core.
- Console validation (rebuild, fake-sign, re-run zzdiag long-line scenario)
  is left for the next authorized build window; nothing was executed on the
  console for this analysis.
