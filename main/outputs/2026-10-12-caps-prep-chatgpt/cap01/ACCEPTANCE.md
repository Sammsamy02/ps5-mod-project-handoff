# Read acceptance ladder (design-only)
Sources: outputs/2026-10-08-l25-prep/PHASEB-PLAN.md §§2,3.1,4.6 and GP CORRECTIONS; CLAUDE-CAPABILITY-STATUS.md “R0 first-rung spec”; capability-prep-20261012/PHASEB-DOC-STATUS.md. Paths are repo-relative; no source code is copied here.

1. Resolve identity/contract first: distinguish binary-to-catalog hash from source-to-binary identity. The old status says exact source absent while the correction appendix cites source; this contradiction needs a dated source provenance receipt, not another assumed pin.
2. R0 known-value baseline: ordinary own-process firmware query, independently known expected firmware, dashboard/no game, bounded lifetime and clean exit. This is NOT a kernel-memory primitive. The older R0 label “in-process reads” and the later first-rung sysctl spec are different checks; name both in the ledger.
3. R1 resident-service check, only after the exact service ABI is pinned. Corrected resident result is zero. Unknown operations can return a normal parent pid; do not discover service support from return values.
4. R2 own-process module/process metadata, using only already specified read interfaces. Capture bounds, error handling and known-answer comparisons; no remote allocations or protection changes.
5. R3 cross-process read is conditional on pinned copy semantics and target identity. A remote syscall is not intrinsically read-only; restrict its operation list and require explicit review before advancing.
6. R4 is REFUTED in the GP correction: no assumed extended syscall/kernel-VA read route. Never count a metadata query or cross-process read as arbitrary kernel-content read proof.

Acceptance: three clean passes across three separate restarts for each new kernel rung, one console operator, one test at a time. Each receipt records boot/session, exact artifact identity, residency observation, known expected result, actual result, bounded exit, clean-log verdict and rollback. No promotion on absent logs, variance, crash, unknown firmware or incomplete identity. Historical userspace runtime proof does not satisfy this rule.

Limits: this document authorizes no console action, payload, firmware values, new resident code or write path. Exact actions need the user's approval under CLAUDE.md; kernel input stays declined. The latest human sequence controls scheduling; nothing here delays B7 or initiates another-game prep.
