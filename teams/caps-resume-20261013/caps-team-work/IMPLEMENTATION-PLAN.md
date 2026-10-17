# Remaining capability implementation plan

> For workers: execute the existing capability designs with the debugging, test driven development and verification workflows. User requested implementation now on October 13, 2026.

**Goal:** Deliver usable implementation for #7 step 2, #1/#3 and #10 to Claude, preserving each original acceptance requirement.

**Architecture:** Extend the existing engine delegate mount source, existing public kernel debugging/provider sources and genuine Mono embedding stages. Keep each component separate and stage current-source integration; the integrator owns target builds and console tests.

**Inputs:** Current main worktree `/WORKSPACE/.codex/worktrees/translation-core/what`; Team A #7 step 2 source/review; existing Phase B and #10 designs. Those designs retain their technical requirements. This worktree does not mutate the integrator's candidate.

## Constraints and acceptance

- Work only in this worktree's `caps-team-work/` component subtrees. No console contact, target PRX build, SDK research, game/mod-file edits, protection changes or new kernel-resident code.
- Use real existing public/provider/engine interfaces and exact source identity; do not invent missing ABI, kernel channel, Mono port or GC ownership facts.
- #7 step 2 must use the engine mount route with external unchanged mod files. The step 1 rename/install transaction is not a substitute.
- #1/#3 retain genuine kernel evidence, restoration/recovery and three separate restart passes. Own-process metadata is not kernel memory proof.
- #10 requires actual managed assembly execution in Townfall calling the runtime, including exception/recovery handling; a native callback or fake assembly is not proof. Full Unity proof follows later.
- B7/build/launch remains independent; #10 alone never holds Stellar Blade.
- Run focused meaningful host regressions after observed failures. One independent review per delivered runtime component; no review of reviews.
- No capability is called complete while an original requirement is missing or only host/synthetic evidence exists.

## Task 1 Runtime mount

- [ ] Reproduce the retained runner/count/validation/order defects in `cap07-step2/`.
- [ ] Fix B2/B3 and F1–F7; validate truthful telemetry and exception cleanup.
- [ ] Implement the real engine adapter, request discovery and current-source integration where admitted interfaces support them.
- [ ] Run focused tests, preserve failure/passing evidence and receive one runtime review.
- [ ] Deliver exact integration recipe and pristine-baseline launch card, with remaining target prerequisites explicit.

## Task 2 Managed runtime

- [ ] Locate matching public reference assemblies and compile a genuine `Probe.dll` from the retained C# source.
- [ ] Rebase bridge/observer staging against the current owner source; preserve exact-once refusal on drift.
- [ ] Implement real initialized owner/lifecycle only against established public Mono port, signature, rooting and provider contracts.
- [ ] Run meaningful available compiler/host validation and receive one runtime review.
- [ ] Preserve actual missing target dependencies rather than claiming a mock host is complete.

## Task 3 Kernel provider and probe

- [ ] Inspect pinned public provider source and determine the exact callable contract.
- [ ] Implement a bounded owned-target client/provider integration only where the real existing interface permits it.
- [ ] Exercise failure, partial transfer, restore and retirement behavior with honest host evidence; receive one runtime review if code is delivered.
- [ ] Deliver the real recovery and three-restart acceptance requirements. Missing kernel channel or source/resident identity remains a blocker.

## Task 4 Claude delivery

- [ ] Inspect every component against original requirements, enumerate complete versus incomplete items and current exact integration targets.
- [ ] Package only this worktree's deliverable sources, licenses, integration instructions and verification evidence; exclude private game/Epic/SDK material and transient build caches.
- [ ] Verify archive members and hashes, publish under the chat's `outputs/`, and give Claude an entrypoint and concise status.
