# One independent review — cg-ready-20261013-02

Review date: 2026-10-13. Assessment: Important findings need correction before the package is declared ready for integration. No Critical findings found. This is a source/Mac review, not console acceptance.

## Scope and method

Read PLAN.md, PROGRESS.md, staged production files and tests, and existing evidence. Compared cap03 to ../cap03/src, cap01 to ../cap01-03/probe/KernelReadProbe-v1.c, and construction changes to the current typescan-r4 include and candidate/src/Mod sources under the what worktree. Also inspected the pinned native construction parameter defaults and conversion path. Reviewed the final current append-only ABI helper and 26-check construction test added during review.

Ran only bounded controlled Mac checks: runner unittest suite (10 tests, all pass); fresh build/run of test_construct.cpp (26 checks, zero failures); and two temporary standalone reproductions described below. Reproductions used the production CodePatch.cpp with existing host OS-boundary doubles and flags=0, or a local Python child. No raw host kekcalls, console contact, kernel changes, target link, old wedge-prone E2E cases, or foreign DLL execution. No production source was edited.

## Important findings

### I1. cap03 liveness still accepts a carry-flag failure when raw RAX is zero

Location: cap03/src/CodePatch.cpp:241–245, reached by patch_code_ex at the require-liveness gate.

`is_error ? -ret : ret` collapses CF=1/RAX=0 to zero; the subsequent predicate reports ALIVE and admits OPEN. This is precisely the return pair now refused by the staged cap01 v2 CHECK control flow and its check-carry-zero regression. A carry result from an ambiguous or failed provider path can therefore pass cap03's mandatory safety gate, even though cap01 refuses the same evidence. This behavior is inherited from the original cap03, but remains a concrete defect in the staged safety package. Use the explicit predicate raw RAX == 0 AND CF clear, preserve both raw observations in the log, and exercise that classifier without invoking a host syscall. This does not resolve the external production CF-contract uncertainty; it removes an internal fail-open inconsistency.

### I2. failed scope recovery can leave the sticky dirty latch clear

Location: cap03/src/CodePatch.cpp:596–601 and 780–783 / 809–816.

The latch is set for byte-write or permission restoration failures, but an armed scope's unpatch refusal before WRITE (for example kNotPatchedState, failed query, unreadable mapping, or failed OPEN) returns without setting it. The scope then logs STATE DIRTY and abandons automatic recovery, yet new patches remain admitted. This conflicts with the plan's failed-recovery latch and creates an unsafe continuation after the scope cannot prove its patch was restored.

Controlled reproduction, using the actual staged production translation unit and existing host doubles: apply a PatchScope; change one patched target byte; call revert(); then attempt another patch at target+32. Output: `REVIEW refusal=105 dirty=0` followed by `REVIEW next=0 dirty=0`. The unrecovered scope remains armed and its destructor emits the scope-leak path. Mark failed recovery of an armed scope dirty consistently, and add a regression covering this pre-write refusal path rather than only final mprotect failures. Ordinary argument rejection before any patch exists need not latch the module.

### I3. runner can PASS while its output includes an unparsed failed CHECK

Location: cap13/runner/runner.py:48–52.

Only FAIL followed by whitespace or end-of-line is counted, and unknown CHECK lines are silently ignored. A realistic failure message that uses a colon after FAIL is excluded from the failed count. If another check passes and a stale/incorrect final summary counts only that passing check, the runner accepts the result despite explicit failed-check evidence. The new summary-count reconciliation does not catch failures that the parser never recognizes.

Controlled child output:

```
CHECK: failure FAIL: unresolved import
CHECK: harmless PASS
RESULT: PASS (1 checks, 0 failed)
```

Actual run_case result: status=PASS, checks=1, failed_checks=0, exit=0, timeout=false. Reject malformed/unrecognized CHECK and RESULT protocol lines, or parse a documented strict grammar that cannot silently discard a failed check. Add this adversarial parser case. This finding concerns the runner's gate, not a claim that any captured upstream refusal log is currently wrong.

## Minor findings

### M1. explicit repeated revert() calls bypass the scope's one-cycle behavior

Location: cap03/src/CodePatch.cpp:809–816.

After a failed revert, armed_ stays true and revert_attempted_ is never checked by revert(). A second explicit call reruns the entire unpatch protocol, including another restoration retry budget. The destructor correctly avoids this retry, but the public method is documented as idempotent. Return the already-recorded failure on repeated calls, or explicitly change and justify the public retry contract. This behavior is inherited from original cap03.

### M2. probe summary still understates its failure gates

Location: cap01/KernelReadProbe-v2.c:253–257 and 347.

The new summary says planned_gates=4, while CHECK, LSTAR, EFER read/classification, TSC, and READ_DR can each increment failures and fail the run. EFER's value/shape is evidence-only, but transport/classifier failure is still a fifth gate. Count that gate or distinguish four primary checks from the additional EFER transport requirement. The control-flow and exit-code correction itself appears sound.

## Construction and validation assessment

The construction service is appended after the old 184-byte x64 host-table prefix, preserves ABI version 1, and supplies a short-circuit size guard before reading the optional field. Existing adapters default to MBM_E_UNAVAILABLE. The new host thunk clears out, checks service lifecycle and game-thread admission, holds CallScope across synchronous construction, and withholds partial backend output on errors/exceptions. The fresh 26-check test covers these host behaviors and retention during a racing stop. No concrete new defect found in these changes.

The engine adapter constructs the native FStaticConstructObjectParameters rather than reinterpreting an MSVC object. The inspected native constructor sets Class/Outer and existing member defaults supply no flags/template/instance graph/external package; Name follows native FName default construction. The narrow ABI cannot represent nondefault name, flags, template, graph or callback parameters, so it cannot establish compatibility with general foreign construction calls. Tests use a recording backend and do not execute this engine adapter; the recorded Prospero syntax-only check (zero errors, nine existing header warnings) establishes compilation syntax only.

cap03 now refuses unreadable queried target pages before logging/copying originals, validates known protection bits/QMP address bracketing, and validates backup page/count/span shape before unpatch OS access. Failed VERIFY-OPEN uses permission-only recovery and does not blindly rewrite target bytes. Existing cap03 evidence records 21 passing isolated cases. These checks still rely on truthful query metadata and caller-owned readable input/backup pointers, quiescent target execution, and externally stable mappings; this review does not claim fault containment for arbitrary dangling pointers or concurrent unmapping.

## Honest external limitations, not package defects

Production provider raw-RAX/CF behavior, exact QMP/VQ signatures/layout/protection semantics, cross-core execution/quiescence, target link and deployment, engine function resolution and native construction ABI on the target, object/outer lifetime and engine class-specific construction preconditions remain unverified by these Mac checks. Inherited runtime valid() reads the UObject internal index before checking its array slot, so arbitrary/stale object pointers are not memory-fault-contained; the existing native raw-pointer contract already documents that risk. No foreign DLL behavior success follows from host refusal evidence, unchanged hashes, loading, resolution, or a recording construction backend. The staged package should preserve these limits instead of treating them as solved by this review.

## Reviewed source pins

- `cap03/src/CodePatch.cpp`: `68a981d4d41c2624d77ce3a022fa06a66ea0199af28afba541a9efc3ab8a8dfb`
- `cap03/src/CodePatch.hpp`: `793bc7158cc91d870172f69a3c59e3202a69fe9ae5ff6953f1d1031a0d7a6815`
- `cap01/KernelReadProbe-v2.c`: `0f30cd4f576b0c7551b530a5d29256b6314f1b124c9d16e5fdd654485dc2ce7e`
- `cap13/runner/runner.py`: `b03136e88702e69852fa4f3ba48825a844bc58b347e4d50c2727316fbb225e5c`
- `cap13/construct/include/Mod/NativeModAbi.h`: `53e4c59a9a987649060265ed193aafab777722253c4700d9b34125379cad9ff4`
- `cap13/construct/include/Platform/NativeModHostCore.hpp`: `046c3c6dc96ff8efaba2586c82ee1e941498de28db76a329526cc8df8e8cc62d`
- `cap13/construct/src/Mod/NativeModEngineServices.inc`: `806f491a13a9bac667d81c534d14d20c9358c18e10da83e86df2ad00a58f0b35`
