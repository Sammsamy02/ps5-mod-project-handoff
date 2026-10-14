# Logwriter atomicity fix proposal — agent CE (2026-10-08)

READ-ONLY analysis + fix design. No tree edits made. Harness proof lives in this
directory (`race_repro.cpp`, `harness-run.txt`). All citations are against:

`/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
(PREFIX below) plus the deployed l22/l23 overlays under
`outputs/2026-10-08-l22-runtime/diagnostics/` and
`outputs/2026-10-08-l23-package/review-l23-20261008T170248Z/implementation/`.

Evidence: `outputs/2026-10-08-l25-prep/evidence/zzdiag-v2-session.log` (UE4SS.log
fetched from console; confirmed by `evidence/UFUNCTION-NULLPTR-FINDING.md`).

## 1. The two writers and the shared sink

| Role | Site |
|---|---|
| WRITER 1 — Lua print (`[Lua]`, wide) | PREFIX`ue4ss/src/LuaLibrary.cpp:35-110` `global_print`; single send at `:102` `Output::send(formatted_string)` |
| WRITER 2 — `[l22]` normal-char | l22-diagnostics overlay `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp:4300,4305` (drain loop), same code at l23 `implementation/thread/src/LuaMod.cpp:4362,4367`; logged via `L22Diagnostics::observe` → `Output::send(STR("[l22] ..."), args)` |
| Shared sink | `Output::send(StringViewType)` PREFIX`first/src/DynamicOutput/Output.cpp:31-46` → the ONE default device `Output::NewFileDevice` registered in PREFIX`ue4ss/src/UE4SSProgram.cpp:256-257` (UE4SS.log, single fd opened `O_WRONLY|O_APPEND` by `PosixFile::open_file` PREFIX`first/src/File/FileType/PosixFile.cpp:244-273`) → `FileDevice::receive` PREFIX`first/include/DynamicOutput/FileDevice.hpp:79-90` → formatter `OutputDevice::default_format_string` PREFIX`first/src/DynamicOutput/OutputDevice.cpp:35-38` → `PosixFile::write_string_to_file` `PosixFile.cpp:293-299` → `encode_utf8` `:138-156` → `write_all` `:70-80` → looped `::write` |

The sink is a single `FileDevice` with a single shared fd. There is no buffer
shared between messages — `fmt::format` and `encode_utf8` build complete
per-call strings/vectors — but there is also **no lock anywhere**: base
`OutputDevice::lock()/unlock()` are default no-ops (vendored
`deps/first/DynamicOutput/include/DynamicOutput/OutputDevice.hpp`), `FileDevice`
does not override them, the RAII `Output::Lock` (vendored
`DynamicOutput/include/DynamicOutput/Output.hpp:409-448`) is never used by
`Output::send`, and `PosixFile::write_all` loops `::write` without any mutex.
The only device that locks is `WriterDevice` (PREFIX`first/include/DynamicOutput/WriterDevice.hpp:28`,
recursive_mutex) — and it is not the UE4SS.log device.

## 2. Exact race mechanism (from code + evidence bytes)

1. **It is true two-thread concurrency, not a same-stream reorder.** The drain
   loop logs `[l22] drained … (callback entry)` → `call_function` → `[l22]
   callback returned` on ONE thread (l23 `LuaMod.cpp:4359-4370`); on that thread
   the Lua `print` inside the callback completes its write before the
   `callback returned` send begins, so a single-threaded interleave is
   impossible. The file nevertheless shows the `callback returned` line landing
   INSIDE that same callback's `[Lua]` message (evidence offset ~131601), and
   two distinct Lua states (`ref=12` state `0x8801db3e8`, `ref=6` state
   `0x8800be8a8`) interleaving at 19:09:53. The second writer is the mod's
   async worker thread (`LuaMod::start_mod` PREFIX`ue4ss/src/Mod/LuaMod.cpp:6440-6460`
   starts `start_async_thread()` BEFORE `load_and_execute_script`, so async
   prints race the loader/drain thread's `[l22]` lines; at 19:09:37 the mod's
   own `[Lua] [TownfallItemSpawner] Loaded v1.0` print from the async side is
   cut by the loader thread's `[l22] Lua mod started … (main script returned)`
   line — same-mod, two threads).
2. **A message is not write-atomic.** `write_all` (`PosixFile.cpp:70-80`) only
   guarantees completion, not atomicity: a short `::write` return splits the
   message across multiple syscalls, and the other thread's complete lines land
   between the segments. The evidence contains the displaced tails as orphan
   fragments (e.g. `\x00s\x00t\x00a` and `\x00\t\x00u` fragments preceding
   later `[Lua]` lines) — tails are displaced, or fully lost when `io_error`
   throws mid-message (the throw escapes `global_print`'s try — which only
   wraps conversion — and aborts the rest of the Lua callback as an error).
   Length-dependent loss matches the zzdiag finding that 30-char chunked
   prints "survive" while long lines lose their tails.
3. **The wide/NUL-interleaved payload bytes are a second, deterministic bug.**
   The `[Lua]` payloads (and one `[l22]` line whose argument was a converted
   string) appear in the file as `T\x00o\x00w\x00n…` — every other byte NUL.
   That is the unit-doubling stride corruption of the PS5 SDK libc++
   `std::wstring_convert` codecvt on the short-wchar target ("4-byte-unit
   writes into a 2-byte-wchar buffer"), exactly as documented in the staged-fix
   comment in PREFIX`first/include/Helpers/String.hpp:15-22` (fixed there, but
   the deployed binary still emits the corrupted form — some deployed TUs
   resolve the vendored codecvt header
   `deps/first/Helpers/include/Helpers/String.hpp:200-205/267-272` with its
   `static std::wstring_convert` instead). It doubles message size (worse
   partial-write exposure) and destroys forensic readability, but it is NOT the
   interleaving mechanism: the conversion is a pure per-call function that does
   not call the writer mid-message.

Reproduced byte-for-byte on host by `race_repro.cpp` (see `harness-run.txt`):
the baseline scenario emits the exact console pattern
`[Lua] [\x00T\x00o\x00w\x00n…\x00m\x00a\x00i[l22] ExecuteInGameThread callback returned …\n`
with the tail displaced.

## 3. Proposed fix (minimal, allocation-light, R3-compatible — no engine calls)

Four independent parts; F1+F3 are the atomicity core, F4 the readability core.

### F1 — give FileDevice a real lock ( PREFIX`first/include/DynamicOutput/FileDevice.hpp` )

```cpp
 #include <mutex>
  private:
    mutable File::Handle m_file;
+   mutable std::recursive_mutex m_write_mutex;
 ...
  public:
+   auto lock() const -> void override { m_write_mutex.lock(); }
+   auto unlock() const -> void override { m_write_mutex.unlock(); }
```
Recursive to match the WriterDevice precedent and to stay safe if a formatter
ever re-enters send while the lock is held. No allocation, no engine calls.

### F2 — make Output::send hold Output::Lock ( PREFIX`first/src/DynamicOutput/Output.cpp:31` )

```cpp
  auto send(File::StringViewType content) -> void
  {
+     const Output::Lock lock{};   // locks all default devices (single device here)
      for (const auto& device : DefaultTargets::get_default_devices_ref()) { ... }
  }
```
The template `send` overloads in the vendored `DynamicOutput/Output.hpp:307-397`
need the same one-line guard (or a tiny non-template helper they all call).
`Output::Lock` already exists and is documented for exactly this
("multiple things with multiple calls to send without interruptions").

### F3 — stage-then-flush the whole encoded message ( PREFIX`first/src/File/FileType/PosixFile.cpp:293` )

Keep `encode_utf8` whole-message (already is), and rely on the device lock for
message atomicity: `write_all` stays as the partial-write recovery loop, but now
runs under F1/F2 so no other `Output::send` user can interleave. Optional
zero-allocation hardening: replace the per-call `std::vector<uint8_t>` in
`write_string_to_file` with a static staging buffer (bounded by the same
65536-unit / 196608-byte limits WriterDevice documents) — safe because the
device lock now serializes the entire encode+write; steady-state logging then
performs zero heap allocations on the write side.

### F4 — route the Lua print payload through the explicit codec ( PREFIX`ue4ss/src/LuaLibrary.cpp:58` )

In `global_print`, replace `to_generic_string(raw_string)` with the explicit
locale-free codec already shipped in the tree:
```cpp
 auto lua_str = Unicode::DecodeUtf8(std::string_view{raw_string}); // StringType
```
(handles `nil`/error branches the same way). This removes the codecvt
NUL-interleave from the hot path regardless of which `Helpers/String.hpp` a
given TU resolves; separately, the build's include order should put
`port/first/include` ahead of `deps/first/.../include` in every TU so the fixed
header wins globally.

### F5 (optional, defense in depth) — don't let an output failure abort the mod

Wrap the `Output::send(formatted_string)` at `LuaLibrary.cpp:102` in the same
observe/try-catch shape `L22Diagnostics::observe` uses, so an `io_error`
mid-write degrades to a swallowed/flagged log failure instead of a Lua error
that kills the callback (this is what makes tails "unrecoverable" today).

### Compatibility notes

- R3 (funcptr-marshal world): every change is libc++ + `::write` only; no
  engine, FName, UObject, or allocator-touching calls; usable from the game
  thread, drain contexts, and mod async workers alike.
- Cost: one uncontended recursive_mutex lock/unlock per log line (tens of ns),
  zero added allocations (F3 optionally removes one per line). Contention only
  arises while a write is actually in flight — log lines are short.
- The `[l22]` diagnostics and every other `Output::send` caller are fixed with
  no changes of their own — the sink is the single point of repair.

## 4. Host compile/run proof (this directory)

- `race_repro.cpp` — extracts the two writer regions + sink; two threads;
  simulates target short `::write` returns (32-byte chunks).
- `harness-run.txt` — results:
  - `1-baseline-deployed` (buggy conversion + unlocked split writes): **124 corrupted lines**, byte pattern identical to console evidence;
  - `2-conversion-only`: **75** — readable payloads but still interleaved (proves conversion fix alone is insufficient);
  - `3-lock-atomic-only`: **0** corrupted;
  - `4-full-fix`: **0** corrupted, payloads fully readable.
- Verdict line: PASS (baseline reproduces; lock+atomic and full fix eliminate).

## 5. Suggested validation on target (for the next build window, not this one)

1. Rebuild the runtime with F1-F4; fake-sign per PS5 rules; verify decrypt-off.
2. Re-run the zzdiag v2 scenario (long single-line prints + keybind-triggered
   ExecuteInGameThread callbacks + `[l22]` markers): expect every `[Lua]` line
   contiguous and ASCII-clean in UE4SS.log, no orphan fragments, no lost tails.
3. Regression: confirm startup `[l22]`/`[L25]` lines still appear in order and
   the log file still opens once (NewFileDevice truncate-on-start preserved).
