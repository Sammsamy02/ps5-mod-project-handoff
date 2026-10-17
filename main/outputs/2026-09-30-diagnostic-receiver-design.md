# Bounded diagnostic receiver design

This architectural checkpoint builds a development tool, not the finished PS5 runtime. It implements the receiving side researched in `2026-09-30-diagnostic-transport-research.md` without changing the native ABI or executing on the console. The latest console-only product requirements remain binding.

## Authorization and approach

The user instructed unattended progress and agent-assisted development. Routine internal design/plan choices and reversible local implementation proceed under that instruction; this document is not described as human-reviewed approval. The current native candidate has known resolver/termination blockers. Limit this checkpoint's sender to numeric loopback addresses and owned receiver fixtures. No remote sender, HTTP/UI route or automatic device invocation is exposed. After the installed loader is identified and the native profile corrected, remote execution will be a separate concrete checkpoint.

Alternatives: merely document a netcat command leaves output/framing and evidence errors untested; building the full console network stack first couples transport testing to the unresolved entry ABI. A Python standard-library development module gives testable byte framing and failure behavior now, without becoming a product runtime requirement.

## Artifact preflight

`inspect_artifact(path, *, expected_sha256, expected_size)` reads one regular non-symlink file through one descriptor, at most 1 MiB plus an overflow sentinel. Require a lowercase 64-digit SHA-256 and an exact positive integer size within 1 MiB; reject booleans. Read once, compare size/hash, and run the existing trusted `verify_elf.verify_bytes` on those exact immutable bytes. Later file changes cannot alter what is sent.

Compute the pinned elfldr's consumed extent: section-table end versus maximum non-NOBITS section end. Use already checked ELF64 section structures; include every file-backed PT_LOAD byte within that extent or reject. Require extent within the artifact, at least the 64-byte header. Send exactly that prefix, preserving full artifact and prefix sizes/hashes. A trailer is never sent into the payload's inherited stdin. Preflight happens before creating a network socket. Static acceptance is never execution or hardware readiness.

## Bounded report parsing

`parse_report(data)` accepts at most 4096 bytes, strict UTF-8 and a final newline, exactly one JSON object, no duplicate keys, non-finite numbers or excessive nesting. Match schema 1 and the exact fields produced by `report.c`: native_entry_unverified origin; fixed PS5 target, LP64 sizes and little byte order; compiler string of 1–256 printable ASCII characters; candidate ABI sdk-payload-args-v0.43; all five environment values null; the four observations with exact keys and signed-32 resolution codes; signed-64 pid/page values and unsigned-32 uid values; unknown capabilities with null evidence and exactly the ten known id/context pairs. Reject boolean integers. Observed values require successful resolution and positive pid/page; unmeasured values are null with zero resolution code. Unavailable pid/page may preserve nonpositive return values; unavailable uid fields have null values. Output resolution code is signed-32. Do not accept desktop_fixture reports.

Reports remain unauthenticated and have no nonce/artifact binding. A valid received report is report_received_unverified, never proof of execution, freshness, loader, firmware, privileges or translation.

## Loopback fixture transport

`send_loopback_diagnostic(host, port, artifact_path, *, expected_sha256, expected_size, timeout_seconds=5.0)` validates a literal loopback IPv4/IPv6 address, port integer 1–65535 and finite timeout 0.01–60 seconds. Hostnames and non-loopback addresses are rejected before any socket. Preflight errors also reject without a connection. No retries.

One monotonic absolute deadline covers connect, all short sends and all receives. Recompute the remaining timeout before every blocking operation; progressing slow peers cannot restart the budget. After all prefix bytes are sent, half-close SHUT_WR and read at most 4097 bytes. Accept parsing only after peer EOF, so a peer leaving an output pipe open yields response_timeout even if it sent a complete JSON line. EOF with no output is peer_closed_empty. Detect overflow, malformed output, reset, connect/send failure and timeout separately. Always close the socket.

Recheck the deadline immediately after each receive, including EOF: a late EOF must not turn an expired operation into a received report. Account for any bytes already received without accepting their contents as evidence.

Return a frozen DiagnosticResult with safe fields: status, send_complete, bytes_sent, bytes_received, artifact size/hash, sent-prefix size/hash and parsed report or null. Do not expose raw peer/error text. The only success-like status is report_received_unverified. Rejected preflight has no artifact metadata. This is a local fixture result, never console verification.

## Verification and limits

Tests use the saved restricted ELF as opaque data, never execute it, and an owned loopback server that consumes the exact derived prefix. Cover changed inputs, malformed ELF, trailers, duplicate/unknown/schema report fields, desktop origin, capability escalation, invalid numbers, short sends and total deadlines. Real socket integration covers half-close framing, chunked reports, empty/invalid/overflow responses and complete-report-but-no-EOF. A narrow controlled socket double covers short sends and deadline failures that real OS scheduling cannot reliably force. Existing native/provider/core suites must stay green.

No production TLS, game filesystem access, redirect installation, engine conversion or native port is claimed. The static verifier's current restricted profile is unchanged. The module must be re-reviewed when its report schema or remote launch policy changes.
