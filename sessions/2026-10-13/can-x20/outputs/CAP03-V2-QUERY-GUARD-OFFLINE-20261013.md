# Stage 30: #3 patch boundary checks on the latest resume-v2 base

**Status:** isolated Mac host hardening; PS5 target behavior remains unproved.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-v2-query-guard-20261013-30`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap03/resume-v2`

The #3 patch protocol now rejects a successful protection query whose reported span does not contain the requested address. It checks target-plus-length overflow before page-span calculation or target reads, both when patching and when validating an unpatch backup. The preexisting `resume-v2` execute-only refusal, rollback failure reporting, and backup geometry checks remain intact. These are generic boundary checks and do not alter kernel code or assume per-game addresses.

The Mac host protocol passed **112/112** checks. New tests first reproduced the unrelated query span and wrapping target; after the fix they refused before any page open or target write. The existing patch, rollback, restoration, execute-only, and liveness-gate paths also passed. The host CHECK-subop disassembly passed. At that pass the `resume-v2` base had no PS5 object, so the runner reported `STATIC-NOT-RUN [ps5]`. A later [stage 30 target build](CAP03-STAGE30-TARGET-BUILD-OFFLINE-20261013.md) used the pinned SDK driver with its bundled LLVM path, compiled and linked both stage 30 source files, and passed host and target CHECK disassembly. No console run occurred.

The three-file patch passed `git apply --check`, application to a scratch copy of the exact `resume-v2` base, byte-for-byte replay, and evidence ZIP CRC verification. The shared candidate and Claude's #2/#4–12 work were untouched.

- [Incremental patch](cap03-v2-query-guard-increment-20261013.patch)
- [Base/stage hash manifest](cap03-v2-query-guard-manifest-20261013.json)
- [Host log and replay evidence](cap03-v2-query-guard-evidence-20261013.zip)

Generic detour decoding, trampoline construction, quiescent install/removal, and PS5 mapping, protection, cache, and full restore evidence remain. This is **not** a console-only wait.
