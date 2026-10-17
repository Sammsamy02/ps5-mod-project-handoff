# Shared DDS module implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development with TDD and one independent task review. Root serializes staging/commits; agents do not modify the index.

**Goal:** Add reusable DDS/BC layout inspection and a lossless neutral block object to the portable mod preparation core.

**Architecture:** Checked little-endian reader with explicit format/topology and completeness facts; immutable owning neutral blocks; inventory passes true total size and planner gates remain unresolved.

**Tech Stack:** C++20, existing nlohmann JSON and SHA-256, dependency-free parser.

**Spec:** `outputs/2026-09-30-dds-module-design.md`.

## Global constraints

- Finished product PS5-only; build computer is development only.
- No per-mod patches or claimed PS5 writer/game acceptance.
- 65,536 subresources and 256 MiB neutral input limits, checked arithmetic before allocation.
- Preserve exact typed format/header/trailer bytes; incomplete spans never manufacture hashes.
- Ordinary non-DDS behavior and whole-stream inventory hashes remain unchanged.

## Review focus

- A full-sized file represented by a capped prefix must not be labeled truly truncated or fully inspected.
- DX10 cube count must expand to six ordered face items; volume slices stay inside each mip.
- Unknown encoding and reserved alpha metadata must not invent interpretation.
- Host round trip must preserve opaque trailer and every header byte.
- Parsing must never bypass target writer/game acceptance or yield real deployment permission.

### Task 1: Parser, neutral form and inventory seam

**Files:** Create dds.hpp, dds.cpp, dds_tests.cpp. Modify core.hpp, inspection.cpp, inventory.cpp, Makefile and relevant inventory/planner tests under `outputs/ps5-mod-bridge`.

**Interfaces:** Exact public API and fact fields are in the spec. Existing two-argument inspect_binary remains source-compatible through optional third total_size.

- [x] Write independent synthetic fixtures/tests for each review focus and representative BC/topology/error boundaries.
- [x] Run focused compilation/tests and record the expected RED result before implementation.
- [x] Implement checked reader, stable outcomes, bounded layout/hash work and immutable neutral slices/round trip.
- [x] Integrate magic dispatch and true streamed size without changing non-DDS behavior or whole-file digest provenance.
- [x] Verify focused parser/inventory/planner tests, then complete core and sanitizer tests. Fix actual failures through RED/GREEN regressions.
- [x] Self-review against spec, record exact commands/results and remaining limits in `outputs/2026-09-30-dds-module-implementation.md`.
- [x] Root reviews diff and stages/commits exact product files; independent reviewer audits spec/security/completeness using written diff.
- [x] Address actionable review findings with scoped regression tests and scoped re-review. Root runs appropriate full regression checkpoint and syncs reviewed commits to the original branch.

## Decisions under unattended authorization

Controller chose a strict BC subset instead of permissive repairs. Cost if wrong: some otherwise usable DDS variants remain Unsupported until a separately verified extension. Controller chose complete-input owning neutral storage for this first module. Cost if wrong: large inputs stay incomplete under the analyzer cap; a future streaming normalizer will be needed for efficient console preparation.
