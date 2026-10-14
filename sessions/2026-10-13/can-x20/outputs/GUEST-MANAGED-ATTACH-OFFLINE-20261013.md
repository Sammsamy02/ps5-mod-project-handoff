# Stage 54: managed DLL attach and thread activation, offline

The isolated source is `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-managed-attach-20261013-54`, based exactly on stage 53. A new batch entrypoint requires a shim bound to the same pending `GuestThreadManager` and an empty caller-owned result. It attaches DLLs into that stable result, then activates queued workers. On attachment failure it aborts queued thread objects, so they signal without guest entry. A separate translation unit keeps older batch-only test and link paths independent of the thread manager.

The caller must still quiesce every worker before detaching the batch. The existing generic `pe_attach_guest_batch` remains available for callers that do not bind a thread manager. This stage does not publish wait, exit-code, or close imports and does not prove an unchanged gameplay DLL's `DllMain` thread behavior on PS5.

**Verification:** Focused host checks passed **72/72** normally and under AddressSanitizer. All **15** prior host runners exited 0. The unchanged synthetic DLL fixture passed **11/11** and retained SHA-256 `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`. The pinned PS5 SDK compiled all **17** loader units with `-Werror` and linked an offline ELF. The eight-file increment replayed byte-for-byte on exact stage 53, and both evidence ZIP files passed CRC.

Artifacts: [increment](guest-managed-attach-increment-20261013.patch), [manifest](guest-managed-attach-manifest-20261013.json), [host evidence](guest-managed-attach-host-evidence-20261013.zip), [target build manifest](guest-managed-attach-target-build-manifest-20261013.json), [target evidence](guest-managed-attach-target-build-evidence-20261013.zip).

No console execution occurred. Every-DLL/every-game compatibility is not established. Claude's #2/#4–12 work, shared candidate, kernel code, and original DLL bytes were not edited.
