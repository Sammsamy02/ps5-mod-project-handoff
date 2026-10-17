# Native SDK host implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development and TDD. Root serializes exact staging/commits; agents do not touch the index.

**Goal:** Produce a tested ordinary-SDK native status host as the on-console application's first lifecycle checkpoint.

**Architecture:** C main with immutable SDK runtime, fixed bounded HTTP parser/socket lifecycle, loopback default plus temporary explicit-address development profile.

**Tech Stack:** C11/POSIX APIs, Python host tests/build manifest, SDK v0.43, LLVM21.1.8.

**Spec:** `outputs/2026-09-30-native-host-design.md`.

## Global constraints

- No new kernel/privilege/exploit code, no custom syscall resolver or SDK changes.
- Own-process socket-spawn only; explicitly imported _exit after owned cleanup.
- Fixed IPv4 host/port, 4096-byte headers, two-second connection deadline, development lifetime600s.
- Source/build identity is distinct from externally computed artifact hash; native and desktop origins differ.
- Native bootstrap alone grants no install capability and adds no runtime PC dependency.

## Review focus

- Fragmented/malformed headers or abandoned clients must not stall lifetime/shutdown indefinitely.
- Wrong/duplicate Host, body framing and cross-origin shutdown must fail without action.
- Bind failure must preserve the existing listener, and owned shutdown must release this application's resources.
- Default SDK imports/load flags must be reported truthfully and never judged by freestanding candidate assumptions.
- Successful upload/send must not be called native execution; report identity/native observations need separate evidence.

### Task 1: Bounded native host, ordinary cross-build and artifact evidence

**Files:** New native-host directory listed in spec, and `outputs/2026-09-30-native-host-implementation.md`.

- [x] Write real desktop subprocess/parser tests for routes, rejection gates, timeout, shutdown and port reuse; verify missing-feature RED.
- [x] Implement minimal fixed HTTP logic and imported BSD socket lifecycle; no dependency install required.
- [x] Build/test desktop fixture, verify actual process exit and preserve native/desktop identity distinction.
- [x] Add verified local-cache SDK build wrapper with safe extraction, explicit profile and stable source build_id.
- [x] Cross-link both ordinary loopback and distinctly named temporary development ELF; inspect imports/entry/load bounds and record external hashes/loader send extent.
- [x] Self-review and document exact commands/results/lifecycle limits; root obtains independent review before any console test.
- [x] Resolve actual findings with regressions/scoped review, then root performs the narrow authorized console checkpoint if source-compatible upload/launch is available. Record facts rather than unsupported success claims.

## Controller decisions under unattended authorization

Use dependency-free fixed HTTP rather than microhttpd for this checkpoint. Cost if wrong: a missing ordinary import or lifecycle issue may require the already documented MHD route. Permit a separately labeled explicit-address LAN development artifact for console verification. Cost if wrong: it temporarily exposes read-only metadata/owned shutdown to the local network; the permanent target stays loopback.
