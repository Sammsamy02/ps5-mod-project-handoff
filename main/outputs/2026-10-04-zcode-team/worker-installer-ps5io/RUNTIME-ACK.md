# RUNTIME-ACK.md — runtime acknowledgment contract

Origin: worker-installer-ps5io, 2026-10-04.
Ties worker-7's installer state model (`downloaded -> prepared -> installed`,
with `running` derived, never stored) to what the injected runtime actually
emits. Nothing here requires the installer to trust anything but a file the
runtime itself wrote through the vendored `mfa_status_*` API.

## 1. The status file (existing, owned by the vendored activation component)

Path: `<mod root>/runtime-status.txt` (e.g. `/data/modbridge/PPSA33286/runtime-status.txt`).
Format (UTF-8, TAB-separated, append-only; defined in
`modforge_activation.h` lines "runtime status file"):

```
modforge-runtime-status <TAB> 1
session <TAB> <pid> <TAB> <monotonic ms at open>
state <TAB> <seq> <TAB> <state> <TAB> <code> <TAB> <detail>
note  <TAB> <seq> <TAB> <key> <TAB> <text>      (optional diagnostics)
```

- States the runtime writes: `activated`, `engine-ready`, `mods-started`,
  `failed`, `stopped`.
- The LAST complete `state` line is current. An unterminated trailing
  fragment (crash mid-write) is ignored by readers.
- A new session REPLACES the file (`mfa_status_open` creates/replaces), so a
  stale `mods-started` cannot outlive the next process start of the runtime.

## 2. Who writes what, and when `running` becomes true

Writer: ONLY the injected runtime (game process), during `module_start`, via
`mfa_status_open()` + `mfa_status_write()` from the vendored
`modforge_activation.c` (its default I/O compiles on the console: libc
`open/write` are nm-verified exports of the pinned libs).

| Runtime moment (bootstrap source) | Emitted state | Installer sees |
|---|---|---|
| `mfa_activate()` returned a valid activation (title matched, files preloaded) — before any engine work | `activated` (code 0) | installed, not running |
| `RuntimeState::EngineInitialized` reached via the real `on_engine_initialized` callback (`runtime_owner.hpp`) | `engine-ready` (code 0) | installed, not running |
| Deferred mod startup finished: UE4SS enumerated and started mods from the activation (`activated_mod_source` views) | `mods-started` (code 0) | **`running=1`** |
| Any startup failure / error accessor non-empty / typed `StartupDeadlineExceeded` | `failed` (code != 0, detail = program error text) | running=0, error surfaced |
| Cooperative `stop()` joined the init thread (`Stopping -> Stopped`) | `stopped` (code 0) | running=0 |

`running` is true if and only if the last complete `state` line is
`mods-started` — exactly the rule worker-7's `records` implements today.

## 3. Crash / restart semantics

- Crash before `engine-ready`: last state stays `activated` -> running=0,
  error unknown; the `session` pid is dead.
- Crash after `mods-started`: the file still says `mods-started`. Readers
  must treat it as "last acknowledged state", not liveness: the operator/app
  may additionally check the session pid. A fresh game launch runs
  `mfa_status_open`, which replaces the file, so the stale line cannot
  persist into the new session.
- Crash mid-line: the final unterminated line is ignored (mfa_status_write
  appends whole lines through the retained descriptor).
- `/data` is only readable from the game process during `module_start`; the
  descriptor kept open by `mfa_status_open` is what still works later —
  which is why the runtime opens the status file first and appends through
  it for the whole session.

## 4. Controller integration point: what the UE4SS side must emit

Nothing in the current bootstrap tree calls `mfa_status_*` yet
(`runtime_owner` keeps its own in-memory `RuntimeStatus`; worker-7's
`simulate-runtime` is a host tool, not the runtime). The controller must add,
inside the injected runtime's module_start path (NOT the installer):

1. After `PrepareDeferredUE4SSRuntime(context)` succeeds and the activation
   was consumed: `mfa_status_open(&st, root, (long)pid, now_ms)` then
   `mfa_status_write(&st, "activated", 0, "")`.
2. In the `on_engine_initialized` callback (the port's deferred-startup
   policy): `mfa_status_write(&st, "engine-ready", 0, "")`.
3. Where the port's deferred mod startup completes enumerating/starting mods
   from the `mfa_activation` views: `mfa_status_write(&st, "mods-started", 0, "")`.
4. On any program error/deadline/failure path: `mfa_status_write(&st,
   "failed", <nonzero>, <sanitized error text>)` (mfa sanitizes detail).
5. After `stop()` joins: `mfa_status_write(&st, "stopped", 0, "")`,
   then `mfa_status_close(&st)`; keep the descriptor open for the process
   lifetime otherwise.

State mapping: `Prepared/Starting -> activated`, `EngineInitialized ->
engine-ready` (then `mods-started` from step 3), `Failed -> failed`,
`Stopped -> stopped`. Until steps 1-5 land in the runtime build, `running=1`
can never be observed on the console and the E2E plan's RUN stage fails
honestly rather than faking acknowledgment.
