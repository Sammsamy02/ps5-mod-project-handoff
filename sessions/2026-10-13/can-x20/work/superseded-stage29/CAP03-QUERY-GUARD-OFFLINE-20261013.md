# Stage 29: #3 patch boundary validation

**Status:** isolated Mac host hardening; PS5 target behavior remains unproved.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-query-guard-20261013-29`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap03`

The #3 patch protocol now rejects a protection-query result whose reported span does not contain the requested address, even when the query returns success. It checks target-plus-length overflow before page-span calculation or target reads. Unpatch validates the backup's target range, page count, page base, and span length before reading its arrays or target bytes. These are generic boundary checks and do not alter kernel code or assume per-game addresses.

The Mac host protocol passed **105/105** checks. New tests first reproduced an unrelated query span, wrapping address, and forged backup geometry; after the fix they refused before any page open or target write. The existing patch, rollback, restoration, and liveness-gate paths also passed. The runner's static CHECK-subop check passed, but its PS5 object is copied from the base and **does not compile this new source**. No PS5 compiler was available on PATH, and no console run occurred.

The two-file patch passed `git apply --check`, application to a scratch copy of the exact base, byte-for-byte replay, and evidence ZIP CRC verification. The shared candidate and Claude's #2/#4–12 work were untouched.

- [Incremental patch](cap03-query-guard-increment-20261013.patch)
- [Base/stage hash manifest](cap03-query-guard-manifest-20261013.json)
- [Host log and replay evidence](cap03-query-guard-evidence-20261013.zip)

Generic detour decoding, trampoline construction, quiescent install/removal, and PS5 mapping, protection, cache, and full restore evidence remain. This is **not** a console-only wait.
