# Stage 49: writable PE output ranges, offline

## Scope

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-writable-pe-20261013-49`, based exactly on isolated stage 48. The shared candidate, Claude's #2/#4–12 work, kernel source, and original DLLs were untouched.

The PE mapper now retains validated ranges for sections marked writable and not executable. Before `DllMain`, the guarded loader registers those ranges with stage 48's output-write map under the mapped module owner. Multiple sections register atomically; a collision refuses attachment before guest entry. A clean detach revokes every section. Read-only headers and executable code are excluded even though the existing read map can read the image. Stack and heap ranges still require explicit owner registration.

**No new Windows thread or output-pointer import is published.** This stage supplies an internal mapped-PE write path for future reviewed APIs. It does not prove that arbitrary game or CRT allocations are writable, or that the console memory model matches the host.

## Verification

- Focused host checks: **48/48** normal and **48/48** AddressSanitizer. The unchanged synthetic PE's writable section accepted a bounded byte write and immediate restoration. Its headers and executable start refused output writes; detach revoked the section. Multiple disjoint sections and atomic overlap refusal were tested separately.
- All **15** prior offline host regression runners exited 0. The original synthetic end-to-end fixture passed **11/11**; its SHA-256 `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce` stayed unchanged.
- The pinned PS5 SDK compiled all **15** loader units with `-Werror` and linked an offline ELF. No console execution occurred.
- The seven-file patch replayed byte-for-byte on an exact stage 48 scratch copy. Host and target ZIP CRC checks passed.

## Deliverables

- [Exact stage 48 increment](guest-writable-pe-increment-20261013.patch)
- [Source and check manifest](guest-writable-pe-manifest-20261013.json)
- [Host logs and replay evidence](guest-writable-pe-host-evidence-20261013.zip)
- [PS5 target build manifest](guest-writable-pe-target-build-manifest-20261013.json)
- [PS5 target objects and link evidence](guest-writable-pe-target-build-evidence-20261013.zip)

## Next offline gates

1. Register actual guest heap and worker stack allocations with lifetime tracking, so output-pointer APIs can handle those common locations.
2. Connect stage 47's thread manager and the stage 48–49 write path to the shim's handle namespace and loader serialization; then review `CreateThread`, wait, exit-code, and close imports.
3. Continue Windows CRT/SEH, UE4SS ABI, target matching, #1/#3, and the console GS/memory/cache verification in the [reviewer handoff](CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md).

The project is **not** at a console-only wait, and these tests do not show an unchanged gameplay DLL running on PS5.
