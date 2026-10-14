# Private Nexus Companion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. Steps use checkbox syntax.

**Goal:** Connect a controller interface to actual local account/catalog/download/analysis operations while preserving the full PS5 translation objective.

**Architecture:** A loopback Python host serves static UI and safe JSON. The existing provider and C++ core perform operations; account and jobs are serialized, with a background queue and private receipts. Native console hosting remains unvalidated.

**Tech Stack:** Python 3.9+ standard library, existing C++20 core, HTML/CSS/JavaScript, bundled Playwright for local browser tests.

**Spec:** outputs/2026-09-30-companion-ui-design.md

## Global Constraints

- Private personal use; no public release or shared credentials.
- Shared game-domain/format/runtime behavior; no per-mod production branches.
- No live provider/device action during fixture verification.
- Downloaded/analyzed is not prepared/installed/game-working.
- Workers own disjoint files; root serializes commits, no amend/rewrite.

## Review Focus

- Slow request/response headers must respect elapsed deadlines.
- Stale selection responses must not cause a different file to be submitted.
- A cancelled/failed login must preserve the previous account.
- Duplicate job intents and restart must not duplicate or invent completed work.
- Browser records/details must not expose provider secrets or internal paths.

### Task 1: Core graph and persistent account fixes

**Files:** src/planner.cpp, tests/planner_tests.cpp, tests/cli_tests.py; companion/modbridge_nexus/*.py, companion/nexus.py, tests/nexus_tests.py, tests/companion_commands_test.py, tests/account_tests.py.

**Interfaces:** Existing build_plan; CredentialStore.save_account/load_account; AccountManager.client/status/complete_sign_in/disconnect.

- [x] Add large-chain/ring and expiry/rotation/legacy/cancel/deadline regressions; observe RED.
- [x] Implement iterative graph traversal and provider account lifecycle; preserve statuses and safe interfaces.
- [x] Run default/O0/O1 core checks and all provider/command/account tests; observe GREEN.

### Task 2: Local host and jobs

**Files:** companion/host.py, companion/modbridge_host.py, tests/host_tests.py.

**Interfaces:** BridgeApplication(store, quarantine, oauth_config=None, *, account=None, download=None, analyze=None); create_server(application, host='127.0.0.1', port=0). Endpoints capabilities/account/games/mods/files/detected-titles/jobs/installed and auth start/callback/disconnect. Normalize metadata at the UI boundary, preserve official provider field names.

- [x] Test missing configuration, session/Origin/Host/CSRF, duplicate intents, safe receipts, interrupted restart and owned download-to-analysis.
- [x] Run the new tests against absent host to observe RED.
- [x] Implement bounded loopback host, safe job queue/receipts, configured OAuth return and explicit lifecycle cleanup.
- [x] Run owned HTTP tests and actual C++ archive analysis integration to observe GREEN.

### Task 3: Controller UI

**Files:** web/index.html, web/app.js, web/style.css, tests/ui_tests.cjs.

**Interfaces:** Same-origin JSON defined in the design; public navigation-only sessionStorage; session CSRF header for mutations.

- [x] Test arrows/Enter/pad actions, stale response selection, safe account state, public-only restoration, modal focus and idempotent jobs.
- [x] Run tests before UI implementation; observe RED.
- [x] Implement TV interface, accessible focus and honest stage/account labels.
- [x] Run browser tests, render at TV size and inspect screenshot; observe GREEN.

### Task 4: Integration and review

**Files:** Makefile, README.md, outputs reports and SDD ledger.

- [x] Wire Python/browser checks into named Make targets and integrated verification.
- [x] Run a real local host with owned provider fixtures and inspect the complete UI workflow.
- [x] Obtain independent code review; fix consequential findings and rerun affected tests.
- [x] Commit exact reviewed files and update evidence/remaining native prerequisites. Preserve original-workspace deliverables before eventual worktree archive.

Self-review: all scoped account/catalog/download/UI requirements have tasks. Full Nexus search, native console host, translation backends and gameplay validation remain outside this implementation checkpoint and remain open in the overall goal. Unattended user authorization permits internal routine design/plan review and reversible execution.

Integration outcome: reviewed source is checked out in the original workspace on local branch `codex/ps5-mod-bridge`; main remains at its prior documentation checkpoint. Fresh original-workspace verification passed82C++/96Python and both browser suites, plus16native fixture tests and static acceptance of the preserved ELF. The managed worktree stays available with its verified toolchain cache for the active goal. Native/runtime translation work remains open.
