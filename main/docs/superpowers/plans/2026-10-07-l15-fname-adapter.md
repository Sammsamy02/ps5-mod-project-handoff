# l15 FName ABI correction implementation plan

> Inline execution authorized by the user's detailed seven-point request. Use test-driven development and an independent final review. No console contact.

Goal: replace the raw return-value FName binding with a verified native output-pointer adapter, preserve admission, audit every direct engine-call boundary, and produce deterministic locally checked PRX receipts.
Architecture: keep the retained public Function/FName API unchanged; bind it to an internal thunk. A freestanding adapter supplies an aligned zeroed 8-byte output plus explicit UTF-16 input. Gate the actual callee on owned live prologue bytes; validate the first result by safe admitted-pool string resolution before later calls.
Spec: user request approved 2026-10-07; l14 POST-LAUNCH-REPORT.md and exact crash artifacts.
Constraints: no network or console; preserve l14/original files; no permission changes, kernel features or pad changes; UTF16 interface is char16_t even though actual pinned compiler wchar_t=2; game ABI facts stay in profile; 60-second shared budget and existing cancellation remain.
Review focus: wrong argument order; signature unavailable/mismatch; bad index or unreadable pool result; non-BMP conversion and 4-byte host wchar; retained API/object drift.

## Task 1 — constructor boundary
- [x] Write host tests for aligned zeroed output, wide-no-length/mode args, legacy call refusal, UTF16/non-BMP/invalid conversion.
- [x] Observe failure before NativeFNameAdapter implementation.
- [x] Implement native void-output adapter and bind thunk without changing retained API.
- [x] Run host tests, sanitizer and real Prospero target compile.

## Task 2 — verification and audit
- [x] Verify live target prefix using existing safe owned copyin capture only; no direct exec-only reads. Host-test missing/mismatch/fault/deadline (zero callee calls).
- [x] Resolve returned ID through safely admitted pool bytes and compare UTF16 text; reject number/index/layout mismatch before trusting binding.
- [x] Integrate checks before slot 2 trusted binding; list all other direct call families with expected registers and unproved facts.
- [x] Add checklist row O for post-admission engine-call ABI mismatch.

## Task 3 — full pipeline
- [x] Fork l14 pipeline into independent l15 outputs, attest sources and helper headers; retain frozen inputs.
- [x] Run two fresh local builds, import census/delta, packaging and all unchanged gates, plus adapter/width integration gate.
- [x] Independent review, focused correction if needed, receipt/hash manifests and local-only report. Upload remains HOLD pending explicit approval.

Completed local evidence: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/BUILD-REPORT.md
Console status: untested; upload HOLD.
