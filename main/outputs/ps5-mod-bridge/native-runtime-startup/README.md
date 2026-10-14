# Native runtime startup source bundle

This directory preserves the aligned native module-entry and lifecycle sources used by the verified startup candidate. It is a source-only reusable bundle; it does not contain a full PRX, object files, import table, generated profile, diagnostic sink implementation, or third-party code. The separate `bootstrap/` directory now contains a locally compiled UE4SS ownership adapter; it has not run on the console.

`module_start` and `module_stop` retain `force_align_arg_pointer`. The tested candidate recorded incoming stack alignment and call-site alignment as zero, completed all 232 init-array callbacks, then ran a delayed in-memory Lua check and closed its state. The exact tested PRX is the frozen candidate in `native-full-runtime-aligned-entry-1`; this bundle itself has not been placed on a console.

## Composition requirements

The composition must supply all of the following from its own pinned build/profile:

- `probe_config.h` values for the target title, log path, input path, size limits, and Lua expectation. Do not reuse another title's profile.
- The generated import table and validation implementation corresponding to that profile.
- An implementation of the `diagnostic.h` sink interface with imports proven for the target runtime.
- Implementations of the `probe_api.h` lifecycle adapter functions. They own input preload/release, constructor validation, worker start/join/current-thread checks, and status logging.
- The full-runtime relocatable object and SDK linker-owned init/fini/EH-frame bounds, `__dso_handle`, and `__cxa_finalize` provider binding.
- The pinned native Lua headers/library required by `memory_lua.c`.

`startup_input.c` loads a bounded exact byte count into owned memory, checks EOF, and closes the source descriptor before delivery. `memory_lua.c` uses a fresh text-only Lua state without standard libraries or game APIs. These components do not start UE4SS or establish engine readiness.

## Scope boundary

The verified console startup is below the UE4SS runtime boundary. The new `bootstrap/` adapter calls the actual `UE4SSProgram` constructor and `init()` in source, but those calls have not been executed. No trusted loader-produced live `ModuleSnapshot` or game-thread callback has been supplied. Read `bootstrap/README.md` for ownership, cancellation, deferred mod loading and full-rebuild requirements. Neither compilation nor the earlier Lua diagnostic establishes UE4SS or game-mod support.

Origin of the ten preserved startup code files: `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1`. Their per-file hashes remain recorded in `SOURCES.sha256`; the separate new bootstrap sources have their own verification receipt.
