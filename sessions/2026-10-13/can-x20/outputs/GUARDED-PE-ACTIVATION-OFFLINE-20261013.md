# Guarded PE preparation and activation: offline continuation

## Result

Isolated stage 24 adds an opaque PE route that maps real static TLS and possible GS images, registers relocated TLS before page sealing, and keeps DllMain, TLS callbacks, and integer export calls behind `GuestSegmentSwitch`. The public `pe_load` route retains its real TLS/GS refusal. A malformed TLS index aimed at executable code now refuses before a bank slot or mapped index is published. Import names are read through their validated file-backed terminator instead of a 255-byte buffer, so long decorated names are accepted without truncation.

Source stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-pe-activation-20261013-24`. Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-tls-callback-exec-gate-20261013-23`. The shared candidate, Claude's #2/#4–12 work, kernel code, and original DLLs were not edited.

## Evidence

The focused host runner passed **25/25 checks on each of five unchanged DLL cases**, with pinned SHA-256 before and after each run. It exercised guarded mapping, TLS registration, malformed index refusal, segment refusal, synthetic Win64 DllMain and integer export entry, wrong-bank and pre/post-attach refusal, and segment restoration. The five real DLLs were **mapped only** using a test resolver that returns a placeholder host address for every import; their DllMain, callbacks, and gameplay code were **not executed**. These runs prove structural loader preparation, not callable Windows import coverage.

Fresh regressions passed: 27/27 strict loader preflight; 20/20 static TLS; 15/15 guest TEB lifecycle; 13/13 context bridge; 5/5 import registry; 18/18 lazy imports; 18/18 typed imports; 12/12 synchronization/S-list; 15/15 thread notification policy. Read-only TLS preparation passed on all five original DLLs, and the original synthetic PE fixture passed 11/11 with its pinned hash unchanged. The ten-file incremental patch passed `git apply --check`, exact-base scratch application, byte-for-byte replay, and ZIP CRC verification.

## Reviewer handoff

- [Incremental patch](guarded-pe-activation-increment-20261013.patch)
- [Exact-base hash manifest](guarded-pe-activation-manifest-20261013.json)
- [Full evidence ZIP](guarded-pe-activation-evidence-20261013.zip)

Apply the stage 15–23 rollup to its stage 14 base, then apply this stage 24 increment once. Alternatively, use the individual increments in order; do not combine the rollup and its component increments.

## Remaining work

The production segment backend still refuses entry. Stage 24 does not register the mapped module's `DisableThreadLibraryCalls` metadata with `WinApiShim`; it does not track automatic new-thread notifications, prove PS5 executable-memory permissions or cache handling, supply Windows/MSVC CRT and UE4SS C++ ABI coverage, implement mixed floating-point signatures, resolve delayed/dynamic dependencies, or bind PC game targets to PS5 builds. #1 and #3 retain their separate offline and console proof gaps. No unchanged gameplay DLL has run on PS5 or produced an intended effect. “Every DLL in every game” is not established by these finite host tests; unsupported cases must refuse with a specific reason.
