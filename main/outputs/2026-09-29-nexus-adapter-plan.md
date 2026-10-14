# Nexus adapter implementation plan

> Required implementation workflow: superpowers:test-driven-development and superpowers:executing-plans or superpowers:subagent-driven-development. Routine review pauses are handled internally under the user's unattended-work instruction.

Goal: implement a provider-neutral-in-game-scope Nexus protocol adapter for every ordinary game domain, with real production HTTP code, official-page authorization configuration and verified local download provenance. Full translation, console UI and hardware operation remain open.

Design: `outputs/2026-09-29-nexus-adapter-design.md`. Dependency: Python3 standard library only. The C++ core and its build remain independent.

1. Write failing tests for typed/redacted errors, HTTPS authenticated-request redirect refusal, byte/time limits and private credential storage. Implement `companion/modbridge_nexus/transport.py`, `credentials.py` and exports. Verify failures before production code; run affected tests after.
2. Write failing PKCE, state/redirect/replay/expiry and token-exchange/refresh tests. Implement `auth.py` against fixed official origins and explicit registered configuration. No live client credentials, password capture or registration.
3. Write failing API tests for arbitrary game domains, user identity/membership, game/mod/file records, bounded responses/rate-limit state, default adult filtering, Premium/free link resolution and malformed schemas. Implement `client.py`; validate provider-origin credentials and supported shape rather than trusting arbitrary records.
4. Write failing NXM/account/expiry and streamed-download tests, including oversized/short reads, partial cleanup, output symlinks and SHA-256 provenance. Implement `downloads.py`. Files stay in quarantine at downloaded stage; no install claim or per-mod special case.
5. Root reviews the module interface, adds a usable companion command adapter/documentation and integrates its offline test target. An independent reviewer examines implementation and tests. Fix material findings with regression evidence and run the fresh combined core/companion suite. Record the exact result and live-service/console limits in the progress artifact.

Ownership: a delegated worker owns `companion/modbridge_nexus/`, `tests/nexus_tests.py` and its report. Root owns companion CLI, Makefile wiring and product documentation. Work only under the managed task worktree; explicit path commits, no history amend during parallel work. Notify root before committing so commits remain serialized.

Acceptance requires functional local protocol code and bounded-download provenance tests, not a static login mockup. Missing project registration keeps the login command unavailable. No fixture result may be reported as a successful live Nexus login or PS5 mod translation.
