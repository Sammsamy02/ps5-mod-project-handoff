# Stage 31: dependency-ordered guarded DLL batch

**Status:** isolated Mac host work; not a console-only wait or universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordered-dll-batch-20261013-31`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-forwarded-module-exports-20261013-28`

`pe_attach_guest_batch` accepts a caller-supplied set of PE byte buffers, source paths, and segment backends in any order. It retries a DLL only when every unresolved static import names another still-pending batch member. A ready provider attaches first; its existing guarded export registry then resolves the consumer's IAT. The companion detach function walks reverse attachment order. Duplicate normalized basenames, unsupported external imports, missing exports, and no-progress intra-batch cycles produce named refusals. If a later DllMain refuses, prior attached modules are detached; a module whose detach itself fails stays in the result for explicit caller cleanup. The batch is bounded to 256 inputs.

The focused Mac suite passed **12/12** checks. Copied synthetic PE fixtures demonstrated consumer-first loading, a three-DLL dependency chain, exact missing-import diagnostics, duplicate refusal before guest entry, cycle refusal, rollback after DllMain failure, and retryable cleanup after segment-backend refusal. The original fixture SHA-256 remained `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`. No original DLL or shared candidate was edited.

All **14** offline regression runners exited 0, including five unchanged DLL read-only TLS checks. The original synthetic end-to-end fixture passed **11/11**. The four-file incremental patch passed `git apply --check`, application to a scratch copy of the exact stage-28 base, and byte-for-byte replay. The evidence ZIP passed CRC verification.

- [Incremental patch](ordered-dll-batch-increment-20261013.patch)
- [Base/stage hash manifest](ordered-dll-batch-manifest-20261013.json)
- [Full regression and replay evidence](ordered-dll-batch-evidence-20261013.zip)

This requires the caller to supply all DLL bytes and paths; it does not scan a mod folder, load absent dependencies, resolve mutually dependent DLLs, implement guest `LoadLibrary`/`GetProcAddress`, or run on PS5. Broad Windows/CRT/UE4SS imports, guest-thread lifecycle, target correspondence, and #1/#3 gaps remain.
