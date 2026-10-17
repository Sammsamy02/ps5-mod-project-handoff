# REPORT — worker-runtime-ack

Deliverable: `runtime-ack.diff` (git-style unified diff, sha256
`4862ffa989aafd2040ba44353d5c954538c1e1210603efeee38f082e42b21fad`, ASCII-only)
adding the five RUNTIME-ACK.md emission points to the UE4SS bootstrap.
Everything here is host evidence; nothing has run on a console.

Files changed by the diff (apply from the worktree root with
`git apply runtime-ack.diff` or `patch -p1 < runtime-ack.diff`):

- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.hpp`
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp`
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.hpp`
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp`
- `activated_mod_source.{hpp,cpp}`: **unchanged** — the activation views are
  already validated there; the ack root is caller-supplied like `mod_source`.

Line anchors below refer to the CURRENT (pre-diff) files.

## Design: sink injection (no global)

`RuntimeAckConfig {status_root, pid, now_ms}` is a new defaulted parameter of
`PrepareActivatedUE4SSRuntime` (ue4ss_backend.hpp:22-24), threaded to
`UE4SSBackend` (ue4ss_backend.cpp:18) and `RuntimeOwner` (runtime_owner.hpp:31,
runtime_owner.cpp:12-13). This mirrors the existing caller-supplied
`NativeProgramContext::mod_source` / `activation_root` pattern: the
module_start composition owns the facts, the bootstrap owns the emission. An
empty `status_root` yields a null sink; every emission site is `if (ack)`
guarded, so with no sink configured there is no file and no behavior change
(proven by T1 and by the unchanged original test suite passing).

`RuntimeAckSink` (runtime_owner.hpp, new) wraps one vendored `mfa_status`
session: `Open` = `mfa_status_open` (replaces the file; fresh session), every
emit = one whole-line `mfa_status_write` through the retained descriptor,
`close` = `mfa_status_close`. All methods are `noexcept` and record failures
in `last_error()`; an open failure produces an inert sink (fd < 0, writes
become recorded no-ops) so startup continues unacknowledged rather than
failing. A mutex serializes emissions (ctor thread, init thread, control
thread are already happens-before ordered in practice; the mutex makes it
strict).

## Per-point rationale

**Point 1 — `activated` (open + write).**
`PrepareActivatedUE4SSRuntime` ue4ss_backend.cpp:66-77. `MakeActivatedUE4SSModSource`
(:75) is where the contract's "mfa_activate() returned a valid activation
(title matched, files preloaded)" is actually enforced (activated_mod_source.cpp:24-29
title/engine/ue4ss-ps5, :56-57 preload check). Only after that does
`RuntimeAckSink::Open` run — during module_start, while /data is still
readable (contract §3) — and `activated` is written once the buffered
inventory (including `start_native_mods()`, ue4ss_backend.cpp:36) also
succeeded. If activation consumption itself fails, no file is created at all:
nothing was activated, the installer correctly sees no session.

**Point 2 — `engine-ready`.**
The port fires `on_engine_initialized` exactly once, after
`fire_unreal_init_for_cpp_mods`/`setup_unreal_properties`/`share_lua_functions`
and only when no shutdown was requested (canonical coherent
UE4SSProgram.cpp:654-661; checkpoint `native_ue4ss_activation_takeover`). The
bootstrap already chains its wrapper there (ue4ss_backend.cpp:26-29). The
emission is placed in that chain immediately after `signal->ready()` — the
call that performs the owner's `Starting -> EngineInitialized` transition
(runtime_owner.cpp:79-86) — so `engine-ready` is written from inside the real
callback, after the owner has recorded readiness.

**Point 3 — `mods-started` (decision, see below).**
Emitted in the same callback chain, after `engine-ready`, gated on
`mods_startup_verified_`, which is set only after `start_native_mods()`
succeeded (ue4ss_backend.cpp:34-39) — i.e. UE4SS enumerated the Lua list and
settings from the `mfa_activation` views (`setup_mods`) and installed/started
the native C++ mods. Ordering is load-bearing: the port REQUIRES
`start_native_mods()` before init (`NativeModStartupBlock::InitializationStarted`,
UE4SSProgram.cpp:558-570), so the actual call site precedes engine-ready.
Writing `mods-started` at that call site would leave `engine-ready` as the
last state line and `running=1` could never be observed. Instead the line is
emitted at the first honest bootstrap-visible moment after engine-ready:
inside the engine callback, which the program itself follows only by
launching the update thread whose `on_program_start` runs
`install_lua_mods()/start_lua_mods()` gated by `m_native_mods_started`
(checkpoint `native_mod_load_order`: Lua entry only after engine init).
Residual window: between the callback and Lua main.lua on the update thread.
It self-corrects on the only non-crash path (a concurrent stop makes
`stopped` the last line), and a crash inside it is exactly contract §3's
"last acknowledged state, not liveness" case (session pid check backstops).
A strictly-exact alternative needs a one-line hook in the port's
`on_program_start` (canonical UE4SSProgram.cpp — outside this diff's
bootstrap scope); flagged for the controller in "Decisions needed".

**Point 4 — `failed` (code != 0, sanitized detail).**
All bootstrap failure convergences emit: (a) `RuntimeOwner::start()` thread
creation failure (runtime_owner.cpp:31-37); (b) init returned without a stop
request (runtime_owner.cpp:91); (c) the exception catches in
`RuntimeOwner::initialize` (runtime_owner.cpp:93-102) — these carry the
backend's rethrow of `native_startup_error_message()` (ue4ss_backend.cpp:49-52),
which is where typed `StartupDeadlineExceeded` and genuine engine errors
surface, concurrent with stop or not; (d) preparation-time failures: the new
try/catch in `PrepareActivatedUE4SSRuntime` wraps backend construction
(construction/inventory rejection, ue4ss_backend.cpp:32-38), emits `failed`,
then rethrows the original error unchanged — the ack never masks it. All
sites use code 1; the detail text (mfa-sanitized) identifies the failure. A
failed session stays terminal: later `stop()` does not append `stopped`.

**Point 5 — `stopped` then `close`.**
`RuntimeOwner::stop()` after `thread_.join()` (runtime_owner.cpp:57-62): when
the final state is `Stopped`, write `stopped` then `close()`. Emit-once lives
in the sink (`stopped_written_`) because the init thread itself can complete
the `Stopping -> Stopped` transition first (runtime_owner.cpp:87-89) — an
"already stopped" guard in the owner would suppress the emission on the
normal cooperative-stop path (this exact bug was caught while building the
host test). Repeated `stop()` (including the destructor's) adds no line. On
`Failed` sessions nothing is written or closed; otherwise the descriptor
stays open for the process lifetime, per contract §4.5.

## Host-test evidence (`host-test/`, reproduce with `host-test/build.sh`)

Built with `/usr/bin/clang++ -std=c++20 -Wall -Wextra` on darwin arm64:
the PATCHED `ue4ss_backend.cpp` + `runtime_owner.cpp`, the REAL
`activated_mod_source.cpp` and REAL vendored `modforge_activation.c`
(worker-installer-ps5io/vendored, compiled as C99), against stub port headers
(`stubs/`) that mirror the canonical `ue4ss-unreal-native-build-1` surfaces
(`NativeProgramContext` validation semantics included). Activations are real:
each test stages a modforge mod root (loadlist.txt + preloaded lua_mod) and
calls `mfa_activate`.

| Test | Scenario | Observed |
|---|---|---|
| T1 | empty ack config | full lifecycle; no `runtime-status.txt` created |
| T2 | success + ack | `session\t4242\t1700000000123`; states exactly `activated,engine-ready,mods-started,stopped`; running=1 observed mid-run; repeated stop adds no line |
| T3 | program construction rejected | prepare throws original error; file has header+session+`failed` with `\t1\t` and "stub construction error" |
| T4 | deferred mod inventory rejected | `failed` only (no `activated`); running=0 |
| T5 | engine error after callback | `activated,engine-ready,mods-started,failed`; owner Failed terminal; running=0 |
| T6 | open failure (root is a file, ENOTDIR) | inert sink; engine starts and stops normally |
| T7 | write failure mid-session (RLIMIT_FSIZE clamp + SIGXFSZ ignored) | emissions record `last_error="mfa_status_write(failed) failed: 27"` (EFBIG); no crash; no partial line landed |
| T8 | open-time write failure (RLIMIT_FSIZE 0) | sink inert; full lifecycle completes with zero ack lines |

Additional evidence: the UNCHANGED original
`bootstrap/tests/runtime_owner_test.cpp` passes against the patched
`runtime_owner` (source/binary compatibility of the defaulted ctor; no
behavior change without a sink). Both binaries also pass under
`-fsanitize=address,undefined`. The diff was applied with `patch -p1` to a
pristine copy and reproduced the patched files byte-for-byte (APPLY-MATCH
for all four files). Raw outputs are echoed in the build transcript
(`host-test/build.sh` run).

## Rollback

Revert = remove the diffs: `git apply -R runtime-ack.diff` (or
`patch -R -p1 < runtime-ack.diff`) restores all four files exactly; no other
file is touched, and the API changes are additive default parameters, so no
caller changes are needed either way. One build-system note after applying:
`runtime_owner.{hpp,cpp}` now include `modforge_activation.h` (the vendored
activation component the bootstrap already links via
`activated_mod_source.cpp`), so that include directory must be visible to
every TU including `runtime_owner.hpp`.

## Decisions needed beyond RUNTIME-ACK.md

1. **Point 3 exactness vs additivity.** The chosen `mods-started` emission is
   the honest bootstrap-visible boundary (engine callback + verified deferred
   mod startup), but Lua `main.lua` begins a few instructions later on the
   update thread. If the controller wants the strictly exact moment, a one-
   line hook after `start_lua_mods()` in the port's `on_program_start`
   (canonical UE4SSProgram.cpp, outside the bootstrap files) or a new
   optional `NativeProgramContext` callback field is required — both outside
   this diff's authorized file set, so not forced here.
2. **Failure codes.** Contract fixes only "code != 0"; all bootstrap-origin
   failures currently use `1`. If the installer wants distinct codes per
   site (prepare/init/deadline), that is a one-line change per emission.
3. **`PrepareDeferredUE4SSRuntime` takes no ack** — the deferred path never
   consumes an activation and the status file is tied to the activation's
   mod root (contract §1), so no session can exist there. Confirm this
   reading.
4. **Who supplies `pid`/`now_ms`.** The module_start composition must pass
   the real game pid and a monotonic millisecond clock (the vendored default
   I/O uses `clock_gettime`; the sink takes the value as a parameter and
   stays testable). Not decidable inside the bootstrap.
