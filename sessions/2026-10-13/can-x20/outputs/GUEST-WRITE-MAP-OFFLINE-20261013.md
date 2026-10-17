# Stage 48: validated guest output writes, offline

## Scope

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-write-map-20261013-48`, based exactly on isolated stage 47. The shared candidate, Claude's #2/#4–12 work, kernel source, and original DLLs were untouched.

`GuestWriteMap` grants bounded output writes only to explicitly registered caller-owned writable allocations. It refuses null, zero-size, wrapping, overlapping, duplicate-owner, and cross-boundary ranges. Removing an owner revokes writes. The map is separate from the existing read map, so registering a readable PE image does not authorize writes to it. The `WinApiShim` can accept this map before PE preparation; later `set_hooks` calls preserve the guarded writer. A direct shim method supplies the future output-pointer path for Win32 imports.

**The owner still must validate actual memory permissions and keep the allocation alive through all guest calls.** This stage tests registered stack and heap buffers; it does not automatically register every guest heap, stack, or writable PE section. It does not publish `CreateThread` or any other new DLL import. No unchanged gameplay DLL ran on PS5.

## Verification

- Focused host checks: **41/41** normal and **41/41** AddressSanitizer, including stage 47's real guarded Win64 worker entry on an unchanged synthetic PE.
- All **15** prior offline host regression runners exited 0. The original synthetic fixture passed **11/11**; SHA-256 `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce` stayed unchanged.
- The pinned PS5 SDK compiled all **15** loader units with `-Werror` and linked an offline ELF. No console execution occurred.
- The four-file increment replayed byte-for-byte on an exact stage 47 scratch copy. Host and target evidence ZIP CRC checks passed.

## Deliverables

- [Exact stage 47 increment](guest-write-map-increment-20261013.patch)
- [Source and check manifest](guest-write-map-manifest-20261013.json)
- [Host logs and replay evidence](guest-write-map-host-evidence-20261013.zip)
- [PS5 target build manifest](guest-write-map-target-build-manifest-20261013.json)
- [PS5 target objects and link evidence](guest-write-map-target-build-evidence-20261013.zip)

## Next offline gates

1. Register actual guest-owned writable heap/stack allocations and only validated writable PE sections. Preserve their lifetime across nested loads and worker threads.
2. Connect the stage 47 manager and stage 48 writer to the shim's shared handle namespace and loader serialization, then review Win64 `CreateThread`, `WaitForSingleObject`, `GetExitCodeThread`, and `CloseHandle` imports.
3. Continue Windows CRT/SEH, UE4SS ABI, target matching, #1/#3, and console GS/memory/cache verification in the [reviewer handoff](CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md).

The project is **not** at a console-only wait. “Every DLL in every game” requires functionality and target correspondence that cannot be inferred from arbitrary Windows binaries alone; unsupported cases need a clear refusal.
