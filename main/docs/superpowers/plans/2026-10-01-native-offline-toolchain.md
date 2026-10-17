# Verified extracted toolchain implementation plan

**Goal:** Compile the current native UI without restoring the removed LLVM archive, using an explicit exact-byte selected extraction input.

**Architecture:** The ordinary native builder retains its default archive-only path. An explicit selected-root option checks a repository-pinned manifest before copying the same verified selected bytes into a fresh build dependency tree. All compiler/tool, SDK, source, ABI, include-closure and archive checks continue. This changes the Python verifier recipe hash and rebuilds the crypto archive; its source, configuration and compilation profile stay unchanged.

**Design:** `.superpowers/native-observations/native-offline-toolchain-design.md`. Root independently checked the approved descriptor digest and all 299 selected member hashes before promoting only that map to `native-host/llvm_selected_manifest.json`. This is retained build provenance, not a digital signature or runtime proof.

## Constraints

- Explicit `--llvm-selected-root` only. No implicit cache search, archive reconstruction, tool downloads, PATH fallback or curl work. Default missing-archive behavior remains unchanged.
- Manifest bytes must match a fixed expected SHA256 before parsing. Validate exact schema, pinned package/archive/descriptor identity, member count and normalized regular member map. Reject missing/extra/changed/link/special members, unsafe path components, root/ancestor symlinks and a non-executable archiver before accepting identity.
- Copy captured verified bytes only into the selected fresh output dependency tree using the existing staged writer. Do not mutate the selected input, old caches or frozen artifacts. Verify existing destination trees exactly when reusing them.
- Compiler/tool binaries and their existing aliases remain independently checked; SDK ZIP and complete extraction verification remain mandatory. Keep existing ABI macros, environment scrubbing and per-TU include-closure checks.
- Record explicit input mode, selected-root path and manifest pin in private build evidence. Semantic dependency identity retains the same original LLVM archive and selected-byte pins; the changed recipe receives its own cache key.
- Host mode rejects native-only selected input. No console, payload, FTP, account, game or system writes or calls. Native ELF is compile/link only, never executed.

## Ownership — Luna

Own only `native-host/crypto_build.py`, `native-host/build.py`, a focused `native-host/tests/test_offline_toolchain.py`, and `.superpowers/native-observations/native-offline-toolchain-implementation.md`. Root owns the promoted manifest, Makefiles, README, plans, Git and synchronization. Title and paging owners are independent.

- [x] Run focused behavioral RED for the explicit selected input and rejected mutated roots, against the current missing option/helper.
- [x] Add the exact pinned-map verifier and explicit option plumbing. Keep default archive path and crypto profile unchanged; describe the recipe identity change accurately.
- [x] Run focused GREEN for a verified root, missing/extra/changed members, links/specials, wrong map pin/profile, non-executable archiver, invalid input mode, and absent-option failure. Synthetic temporary trees may exercise rejection; current retained map must independently pass the real check.
- [x] Build current loopback native app once into a new owned output root from pinned local SDK/compiler/selected/Mbed/framework inputs. Retain fresh source/build/ELF/import evidence, without target execution or a broad review/sanitizer matrix. Stop and report any ordinary new failure rather than weakening verification.
- [x] Record proof and freeze owned files for root handoff.

## Root follow-through

- [x] Inspect focused changes/results and verify the current source identity, then commit/sync the narrow builder fix separately.
- [x] Keep changed native UI unexecuted and live services unavailable; compilation is not console browser/account/game acceptance.

Verified checkpoint: 15 focused offline/builder tests and 4 relevant crypto regressions passed; root passed the 7 offline tests and matched all app source/artifact identities. The final loopback ELF linked with 14 successful app commands, verified source/config/header provenance and no host include origin. It was not executed or deployed. Prior native archive-default failure remains an expected negative path.
