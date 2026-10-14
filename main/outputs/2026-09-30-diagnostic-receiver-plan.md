# Diagnostic Receiver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this short local checkpoint. Steps use checkbox (`- [ ]`) syntax for tracking. Root implements; an independent reviewer checks the result. Research agents continue in parallel.

**Goal:** Verify a bounded development receiving client against owned loopback fixtures without invoking the console.

**Architecture:** Reuse the restricted ELF verifier on immutable checked bytes. Add strict report validation and one-deadline half-close/read transport in a separate standard-library module. Keep all non-loopback invocation disabled.

**Tech Stack:** Python 3 standard library, unittest, existing native ELF verifier and artifact.

**Spec:** `outputs/2026-09-30-diagnostic-receiver-design.md`

## Global Constraints

- No product runtime dependency on a PC, phone or AI; this module is a development diagnostic.
- No remote sender, device contact, native ABI change, privilege initialization or game installation.
- Artifact bound 1 MiB; report bound 4096 bytes plus one overflow detector; timeout 0.01–60 seconds; no retries.
- Existing strict ELF verifier and report schema remain authoritative; status report_received_unverified never means executed.
- Root stages exact files and commits serially; retain unrelated research and dependency cache.

## Review Focus

- A changed file after preflight must not alter sent bytes; task 1 tests immutable snapshots.
- ELF file trailers must not become payload stdin; task 1 pins extent and task 2 asserts exact received bytes.
- Slow progressing peers must not reset the deadline; task 2 tests chunked timeout and controlled short sends.
- A complete JSON line without EOF must time out; task 2 includes a holding-open peer.
- Spoofed desktop reports, boolean integers, duplicate fields and capability claims must fail closed; task 1 tests each input class.

### Task 1: Artifact and report gates

**Files:** create `outputs/ps5-mod-bridge/native-probe/diagnostic.py`; create `outputs/ps5-mod-bridge/native-probe/tests/test_diagnostic.py`.

**Interfaces:**
- Consumes: `verify_elf.verify_bytes(data: bytes) -> dict`.
- Produces: frozen `DiagnosticArtifact(data: bytes, size: int, sha256: str, extent: int, prefix_sha256: str)`; `inspect_artifact(path, *, expected_sha256, expected_size) -> DiagnosticArtifact`; `parse_report(data: bytes) -> dict`. Invalid input raises ValueError/OSError, with no network side effect.

- [x] Step 1: Write tests asserting known artifact size 50848/hash 33fba54b4e18d9467a3b295aca080a411729758e12ca91f6015c9dd74dd3be3f, exact hand-checked section extent, snapshot immutability, trailer exclusion, size/hash/symlink/oversize/invalid-ELF rejection. Add a complete hand-authored schema-1 native report and mutations for every parsing gate in the spec.
- [x] Step 2: Run `python3 -m unittest discover -s outputs/ps5-mod-bridge/native-probe/tests -p test_diagnostic.py -v`. Expected: failing assertion that diagnostic gates are not implemented.
- [x] Step 3: Implement immutable artifact inspection and strict report parsing in diagnostic.py; keep verifier unchanged.
- [x] Step 4: Run the same command. Expected: all gate tests pass. Run existing native tests too; expected all pass.
- [x] Step 5: Root commit exact new module/tests and design/plan/requirements files with message `Add diagnostic artifact and report gates`.

### Task 2: Bounded loopback transport and evidence

**Files:** modify diagnostic.py/test_diagnostic.py; modify `outputs/ps5-mod-bridge/native-probe/README.md`; create `outputs/2026-09-30-diagnostic-receiver-report.md`.

**Interfaces:**
- Consumes Task 1's inspected immutable artifact and parse_report.
- Produces frozen DiagnosticResult as specified; `send_loopback_diagnostic(host, port, artifact_path, *, expected_sha256, expected_size, timeout_seconds=5.0) -> DiagnosticResult`.

- [x] Step 1: Write owned IPv4 loopback server tests for exact prefix/half-close/chunked valid report; empty, malformed, overflow and held-open reports; progressing response timeout. Assert safe status, byte counts, send_complete and unmeasured evidence. Reject non-loopback/hostname/port/timeouts and failed preflight before socket creation. A narrow clock/socket double pins short sends, shrinking timeout, failure and no retry.
- [x] Step 2: Run the diagnostic tests. Expected: failure because send_loopback_diagnostic is absent.
- [x] Step 3: Implement one absolute deadline, short-send loop, SHUT_WR, bounded receive-to-EOF and fixed safe results; close every socket.
- [x] Step 4: Run diagnostic and all native tests, then `make test-all` with the bundled Node/Python environment. Expected: all suites pass; no Nexus or console contact.
- [x] Step 5: Record exact evidence/limits and root commit exact files with message `Verify bounded loopback diagnostic transport`. Run a fresh independent review; fix material findings with failing regression tests before local integration.

Self-review: all design contracts have task/test ownership. No report schema or native entry changes are implied. Unattended execution is authorized by the user's instruction; this is an internal design/plan ruling, not a claim that the human approved these artifacts. Whole-goal coverage and console execution remain open.
