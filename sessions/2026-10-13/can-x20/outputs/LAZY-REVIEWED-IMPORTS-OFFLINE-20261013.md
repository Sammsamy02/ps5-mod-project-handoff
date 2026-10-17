# On-demand reviewed imports: Mac continuation

## Result

An isolated stage lets the `WinApiShim` install a reviewed built-in import when the PE loader asks its `ImportResolver` for that name. The caller supplies one platform executable-memory backend: allocate writable memory, seal it read/execute with any required cache work, and release unused memory. Without that backend, the resolver still refuses every unpublished built-in. Unknown and unreviewed names never allocate memory. Registered module exports keep priority.

This is generic across the DLL name/symbol rows in the internal reviewed catalog. No game-specific addresses or DLL modifications were added. The PE loader already calls `resolve` per import; the new path can therefore supply a typed trampoline without a separate manual installation pass. It does **not** make unsupported imports callable or configure a PS5 backend by itself.

The source is at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-lazy-reviewed-imports-20261013-17`, copied from isolated stage 16. Shared candidate, Claude's work, and original DLL binaries were untouched.

## Verification

The new test first failed to compile because the backend API did not exist. With the implementation, focused checks passed **12/12**: no-backend refusal, unknown/unreviewed refusal without allocation, lazy RW-to-RX sealing, invisibility and recursive refusal during sealing, Win64 call, repeat resolution, failed-seal cleanup and retry, registered-module priority, and a forced two-thread race that published one trampoline and released the loser.

The packaged regressions passed: typed imports **18/18**, context bridge **13/13**, import registry **5/5**, TEB lifecycle **12/12**, static TLS **19/19**, loader preflight **27/27**, read-only TLS preparation on five unchanged DLLs, and synthetic fixture **11/11**. The pinned fixture hash was unchanged. The four-file patch passed `git apply --check`, scratch apply, and byte comparison; the evidence ZIP passed `unzip -t`.

## Handoff

- [Incremental patch](lazy-reviewed-imports-increment-20261013.patch)
- [Exact-base manifest](lazy-reviewed-imports-manifest-20261013.json)
- [Evidence ZIP](lazy-reviewed-imports-evidence-20261013.zip)

The memory backend is a trusted platform seam. Mac RX behavior does not establish PS5 executable permission or instruction-cache handling. The loader still refuses unchanged DLLs with required unimplemented imports and TLS/GS needs. CRT/UE4SS ABI, Win32 synchronization, and target correspondence remain offline work. The project is **not** at a console-only wait.
