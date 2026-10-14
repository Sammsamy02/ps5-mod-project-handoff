# Stage 47: guest thread-object core, offline

## Scope

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-thread-handles-20261013-47`, copied byte-for-byte from the isolated stage 46 source before this increment. The shared candidate, Claude's #2/#4–12 work, kernel source, and original DLLs were untouched.

Stage 47 adds `GuestThreadManager` around the stage 46 guarded Win64 worker entry. Creation validates that a start address lies in exactly one attached executable PE region before spawning. It reserves an opaque non-reused handle and a process-wide 32-bit guest thread ID; the worker's `GetCurrentThreadId` uses that same ID. Supported creation is an immediate start with default attributes and stack. Suspended starts, custom stacks, and other flags refuse explicitly.

The manager has completion wait, timeout, outcome query, and close operations. Closing a handle invalidates it without stopping the worker. `quiesce_all` joins every worker, including workers whose handles were closed, before batch unload. A failed guest worker remains detectable after its handle is closed. Finished closed records are reaped so a long-lived manager does not retain each old thread object. The manager still relies on the caller to keep the batch, TLS bank, shim, and segment backends alive until quiescence.

**This is an internal core, not a published `CreateThread` import.** Guest-output-pointer writes, loader serialization during `DllMain`, complete handle namespace integration, and Win32 error conventions remain gates. The tests do not show any unchanged gameplay DLL running on PS5. The stage 42 GS backend remains a candidate until console verification.

## Verification

- Focused host checks: **33/33** normal and **33/33** AddressSanitizer. Tests cover invalid start and unsupported flags before spawn, ID agreement, zero-timeout wait, close while running, non-reused handle/ID, completion, failure retention after close, and actual Win64 guest worker execution on the unchanged synthetic PE fixture.
- All **15** prior offline host regression runners exited 0. The original synthetic end-to-end fixture passed **11/11**, with SHA-256 `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce` unchanged.
- The pinned PS5 SDK compiled all **15** loader units with `-Werror` and linked the offline ELF. This is target build evidence only; no console execution occurred.
- The six-file patch applied cleanly to a scratch copy of exact stage 46 and reproduced all six files byte-for-byte. Host and target evidence ZIP CRC checks passed.

## Deliverables

- [Exact stage 46 increment](guest-thread-handles-increment-20261013.patch)
- [Source and check manifest](guest-thread-handles-manifest-20261013.json)
- [Host logs and replay evidence](guest-thread-handles-host-evidence-20261013.zip)
- [PS5 target build manifest](guest-thread-handles-target-build-manifest-20261013.json)
- [PS5 target objects and link evidence](guest-thread-handles-target-build-evidence-20261013.zip)

## Next offline gates

1. Add a validated writable guest-memory route, including heap and stack allocations, for Windows API output pointers and error codes.
2. Integrate the thread manager with the shim's opaque handle namespace and loader lock, then review `CreateThread`, `WaitForSingleObject`, `GetExitCodeThread`, and `CloseHandle` as Win64 callable imports. Their published behavior must preserve batch lifetime through nested DLL loads and unload.
3. Continue the CRT/SEH, UE4SS ABI, dependency, target-correspondence, and #1/#3 engineering listed in the [main reviewer handoff](CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md).

The project is **not** at a console-only wait. No implementation can promise every arbitrary Windows DLL in every game without translating each required API and finding a valid target on each PS5 build; unsupported cases need an explicit, diagnosable refusal.
