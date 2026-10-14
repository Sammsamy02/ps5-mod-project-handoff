# Zcodes continuation findings

Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.

The Mac continuation produced five analysis deliverables. C1 remains partially open: the archive does not contain the target kernel syscall-exit body needed to prove CF. C2/C3/C4 have actionable source-backed findings; C5 is a design proposal, not an approved implementation.

| Task | Result |
|---|---|
| C1 Return contract | Producer-side CF guarantee unproved; fallback is not a general MSR classifier. Fresh suite has 13 assertions, all pass, despite printing 14 |
| C2 Construction flow | One row removes a service-exposure gap, not C++ DLL ABI/callback/layout gaps. The prescribed aggregate-8 repack is contradicted by fresh compiler evidence |
| C3 Existing route | Existing resident mprotect skip is the candidate; new arbitrary kernel reads are not inherently required. Fresh 98/98 mock checks pass, but pre-open reads, unsafe rollback and unenforced dirty state remain acceptance risks |
| C4 KF evidence | Local fixtures/samples executed; genuine framework/proxy refused. Log has 98 passing checks, one wedged case and one missing input hidden by runner success. Positive upstream gameplay target remains blocked |
| C5 Retargeting | Proposed independent name, correspondence and compatibility gates; generated profiles and engine adapters retain their separate roles |

## Loading

Partial Mac fixture/sample execution was recorded by KF. This continuation audited saved logs and ran only local protocol/classifier tests and assembly-only ABI probes. It did not rerun KF’s wedge-prone suite. No console payload was built or loaded.

## Target resolution

Source-backed process permission interception and native engine binding exist. Automatic PC→PS5 semantic correspondence and complete foreign C++ ABI mediation are not established.

## Working patch behavior

Host protocol model passes 98 checks. Unchanged upstream gameplay DLL behavior, console patch/restoration and restart proof remain unverified. Treat source risks in C3 as blockers before console use of the current payload.

## Next work

First correct the Mac evidence runner in a new revision with nonzero build/child exits, missing-input failures and bounded timeout verdicts. Correct the protocol’s access/recovery/dirty-state gaps with targeted negative tests in a separate staged revision; preserve the frozen package. Add the construction service only with compatible table growth and typed parameter/admission decisions. Acquire and classify a genuine upstream gameplay DLL before claiming upstream compatibility. No console action follows automatically from any of these steps.

Files C1 through C5 contain the matrices and evidence limits. SOURCE-PINS.json holds full input paths and hashes; evidence/AUDIT.json reconciles counts. CHECKSUMS.json pins this package. Original shared status reports were deliberately left intact; use this as an additive handoff.
