# TLS callback executable gate: Mac continuation

## Result

An isolated TLS preparation stage now requires the caller to identify file-backed executable ranges for every non-null relocated TLS callback. A callback target is checked before the static TLS bank receives a slot or the mapped image's `AddressOfIndex` is written. A callback inside the mapped image but outside executable code therefore refuses without publishing any TLS state. The read-only real-DLL runner supplies validated PE section ranges; its independent post-registration check remains.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-tls-callback-exec-gate-20261013-23`, copied from isolated stage 22. Shared candidate, Claude's work, and original DLL binaries were untouched.

## Verification

The new synthetic case first failed to compile against the old four-argument registration API. With the executable predicate implemented, the static TLS suite passed **20/20**, including the new in-image non-executable refusal with no index write or bank slot. Read-only preparation passed on all five unchanged DLLs; two have two callbacks each and three have none. Source hashes were unchanged. Later import/lifecycle and loader preflight regressions passed, and the synthetic PE fixture remained **11/11** with its pinned hash unchanged. The four-file patch passed exact-base scratch replay; the evidence ZIP passed `unzip -t`.

## Handoff and limits

- [Incremental patch](tls-callback-exec-gate-increment-20261013.patch)
- [Exact-base manifest](tls-callback-exec-gate-manifest-20261013.json)
- [Evidence ZIP](tls-callback-exec-gate-evidence-20261013.zip)

This hardens TLS preparation but does not lift the PE loader's real TLS/GS refusal or validate the console segment backend. The unchanged DLLs still do not execute; other imports and target correspondence remain unresolved. This is **not** a console-only wait.
