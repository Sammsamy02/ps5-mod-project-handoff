# Capabilities 1 3 and DLL implementation preparation plan

> Required workflow: executing-plans, inline execution and one final independent review.

Goal: deliver source-pinned, Mac-tested staged corrections and an exact Claude integration/acceptance handoff. No claim of console completion.
Architecture: keep owned staging separate from the shared candidate and frozen payloads; reuse existing resident/process/engine interfaces. Spec: ../cg-resume-20261013-01/C1-RETURN-CONTRACT.md through C5-RETARGETING-PROPOSAL.md; user approved continuing the handoff on 2026-10-13.

Constraints: Mac-only; no console contact, kernel edits or new kernel code; preserve mod DLL bytes; no Epic/SDK source in delivery; generic runtime; separate loading, resolution and behavior evidence. Existing return-contract/source-binary gaps cannot be declared solved by a host test.

Review focus: unreadable mappings; failed permission verification; sticky dirty recovery; runner child exits/timeouts; ABI table compatibility and construction parameter semantics.

- [ ] Task 1 cap03: tests first against copied production source. Refuse unreadable originals; never write on failed OPEN verification; latch failed recovery and refuse subsequent new patches. Validate backup bounds/flags. Preserve and run legacy behavioral checks with process isolation for dirty cases.
- [ ] Task 2 cap01: shared raw-RAX/CF classifier with explicit per-probe expectations; correct test count/evaluation and logging va_list reuse in a distinct source revision; no target build.
- [ ] Task 3 DLL evidence runner: fail on build/child/missing-input/timeout/empty checks; no automatic failed retries; run controlled local child regressions, do not replay old wedged E2E.
- [ ] Task 4 construct service: inspect full current host integration and stage append-only service/typed engine adapter where contracts are established; real host-unit tests, explicit unsupported parameter refusal; no reconstructed-mod success claim.
- [ ] Task 5 reconcile C1/C3 and upstream artifact target; package current-source pins, precise diff/apply guards, acceptance requirements and remaining external blockers; one independent review, fix important findings, final verification.

Integration is Claude-owned. No shared branch commit/push or deployment follows this plan.
