# FILEIO_DLL_TEST.elf — console staging info (worker-fileio-apply, 2026-10-04)

Staging-ready payload descriptor only. NO console contact was made (no network per task
rules); FTP/ploader dispatch is the controller's decision, same gates as
worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md (Gate A0 write-probe, Gate A1
FSELF decrypt oracle).

## Payload file

- Local path: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-fileio-apply/out-sdk-composition/cd9b5aec3d344a62bea649b3b78b3ee5/owned-dll-candidate.elf`
- Console payload name: `FILEIO_DLL_TEST.elf` (same dispatch family as INITONCE_DLL_TEST / NEXT_DLL_THREADS probes)
- sha256: `1e91ec1b753ff9b8e729334c2e6722e1f48582efab9315097b82960f206d3d9e`
- bytes: `279504`
- Format (verified on disk): `ELF 64-bit LSB pie executable, x86-64, version 1 (FreeBSD), dynamically linked, not stripped` — genuine SDK payload `crt1 _start`, not a PRX module entry (build receipt `artifact.startup`).
- Build: `cd9b5aec3d344a62bea649b3b78b3ee5` (BUILD_ID embedded; status `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`)

## Profile composed into this ELF (all via real PE imports / win64cc ABI)

entry + clock + workers + sync + init_once + mutex + fileio; `fileio_profile`:
`{"enabled": true, "win64cc_defs": 20, "actual_pe_imports": 10, "namespace_root": "/data/modbridge", "handle_close": "native mb_win_fileio_close_handle delegation", "target_execution": false}`

Compile-time production namespace root: `/data/modbridge` (`--fileio-root`, default).
Host-driver controls used a per-build dedicated directory override
(`MB_OWNED_FILEIO_ROOT_OVERRIDE` → `out-host-validation/<build>/fileio-root/`), per patch 01 design.

## Host validation backing this staging candidate (PASSED)

- Command: `build.py --host-only --entry --clock --workers --sync --init-once --mutex --fileio` → exit 0,
  `OWNED_CANDIDATE_HOST_CONTROLS_PASSED` (receipt: `delivery/receipts/host-validation-receipt.json`, build `47ee2b8391a940f9a1be9b3934064dea`).
- All host result records carry both `"mutex":{"enabled":1}` and `"fileio":{"enabled":1}`;
  worker-call and consumer-imports (61) assertions passed; file-handle retirement through
  `mb_win_fileio_close_handle` exercised in the worker path.
- Known pre-existing link warnings (also on prior candidates): rust-lld `/align specified without /driver`.

## Honest limits

- `target_execution: false` everywhere. This ELF has NOT run on the console. One launch
  per staged build, per master staging package rules.
- No download-and-execute, no PRX, no console contact evidenced by this worker.
