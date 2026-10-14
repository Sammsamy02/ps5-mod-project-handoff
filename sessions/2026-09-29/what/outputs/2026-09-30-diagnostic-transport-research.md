# Diagnostic transport research — 2026-09-30

Research only. No console connection, ELF send, implementation, or execution was performed. The sources below are pinned comparisons; the reported Payload Manager `0.5.2` and `/version` response do not identify which ELF loader binary is running. Firmware `13.60` is user-reported. Do not automatically invoke the remote endpoint.

## What the pinned loader does

At elfldr commit [`54966675d3884ba9c773f0f52646f8f46b92cbbc`](https://github.com/ps5-payload-dev/elfldr/tree/54966675d3884ba9c773f0f52646f8f46b92cbbc), `socksrv.c` accepts TCP on port 9021 and peeks four bytes to dispatch the ELF path ([`socksrv.c`, lines 214–249, 287–329](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/socksrv.c#L214)). For ELF, `elfldr_read` receives an `Elf64_Ehdr`, computes an initial extent as `e_shoff + e_shnum * sizeof(Elf64_Ehdr)` (64 bytes, also the ELF64 section-header size), then scans non-`SHT_NOBITS` section headers and extends the extent to the greatest `sh_offset + sh_size`. It reads exactly that many bytes with `MSG_WAITALL`; it does not wait for TCP EOF ([`elfldr.c`, lines 716–782](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/elfldr.c#L716)). Thus half-closing the client write side after sending a correctly bounded ELF is compatible with the read sequence; EOF is not its framing mechanism.

After reading, `on_connection` calls `payload_spawn(..., fd, ...)`; the default `pipe` setting is `1`. In `elfldr_exec`, a positive `stdio` fd is duplicated into the new process, then installed as stdin, stdout, and stderr. The accepted connection handler returns from spawn setup and closes its own descriptor, but payload-side duplicates can keep the TCP connection alive. `elfldr_spawn` returns after setup; it does not wait for payload completion ([`socksrv.c`, lines 259–282, 327–329](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/socksrv.c#L259); [`elfldr.c`, lines 443–455, 663–670](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/elfldr.c#L443)). The pinned source therefore supports a client strategy of write-half-close followed by bounded reads until peer EOF or a deadline. It does not establish on-device timing or prove this is the user’s loader.

Manager commit [`e4e72dc9d831dd09ee8fabc222ad1e7f5139050e`](https://github.com/itsPLK/ps5-payload-manager/tree/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e) streams the file to its *local* `127.0.0.1:9021`, closes the socket, and returns based on send/read errors; it never reads response bytes ([`ps5_launcher.c`, lines 17–81](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/ps5_launcher.c#L17)). Its successful result means transmission completed, not that a payload ran or reported.

## Suggested receiving-client contract

Keep this as a small Python standard-library transport module. A reviewable API could be:

```python
send_diagnostic(
    host, port, elf_path, *, expected_sha256, expected_size,
    connect_timeout, send_timeout, response_timeout,
    max_report_bytes=4096,
) -> DiagnosticResult
```

Before opening a socket: run the existing local ELF verifier; read the file once with a strict configured size bound; require exact manifest size and SHA-256; parse the ELF headers with checked arithmetic; and derive the same loader-consumed extent (section-table end versus largest non-`SHT_NOBITS` section end). Require that extent to be in-file and send exactly that prefix, so any file trailer cannot leak into inherited stdin. Preserve both full-artifact size/hash and sent-prefix size/hash in the local result. Refuse on any verifier, hash, size, arithmetic, or extent mismatch. The verifier and manifest are local gate evidence only; they do not establish the remote loader identity.

Use a fresh TCP socket for one transfer. Apply explicit connect/send/response deadlines based on a monotonic clock. Send all bytes handling short writes, then call `shutdown(SHUT_WR)`; leave the read side open. Receive at most `max_report_bytes + 1` total bytes, stopping at EOF or the single response deadline. The extra byte is an overflow detector; exceeding 4096 bytes is a hard protocol failure. Timeout, reset, or early close returns a distinct transport result and retains only bounded received bytes. Do not retry automatically: a retry could cause a second payload invocation after an ambiguous timeout.

The current native probe caps output at 4096 bytes in at most eight `write` attempts (`native-probe/README.md`); `report.c` emits one newline-terminated JSON object. Parse only after EOF or after the deadline policy explicitly chooses to accept a complete bounded frame. Require strict UTF-8, exactly one JSON object, no duplicate keys or non-finite numbers, a final newline, and no extra non-whitespace data. Enforce `schema_version == 1`, `record_kind == "native_probe_report"`, an allowed `evidence_origin` (for the contemplated real entry, `native_entry_unverified`), exact known top-level fields, required nested keys, and field types/ranges. Unknown schema versions or keys should fail closed. Capabilities must remain `status: "unknown"` with `evidence: null`; environment identity fields remain null; report observations must be limited to their declared `observed`, `unavailable`, or `unmeasured` states and integer/null values. Reject desktop-fixture origin for any target-run report.

Keep transport/evidence statuses explicit, for example:

| Status | Supported claim |
|---|---|
| `preflight_rejected` | Local artifact/verifier gate failed; no connection attempted. |
| `connect_failed` / `send_failed` | A bounded transport step failed; no execution inference. |
| `send_complete` | The checked byte prefix was handed to the local TCP stack; not loader acceptance. |
| `response_timeout` / `peer_closed_empty` / `response_overflow` | No valid bounded report was obtained. |
| `report_received_unverified` | A bounded response parsed against the schema. It is not cryptographically tied to this artifact, loader, firmware, or invocation. |

Never produce `loaded`, `executed`, `console_verified`, or capability-validation status from send success, TCP closure, a JSON report alone, or a desktop fixture. TCP output has no authenticity or freshness in the existing protocol: the report lacks an invocation nonce and artifact digest. If later work needs invocation correlation, that requires a separately reviewed report-schema change (e.g. nonce and build digest) and still does not identify the loader unless the endpoint is independently attested.

The loader’s ELF extent calculation trusts remote header arithmetic and section counts before its later sanity check. Keep the host-side verifier as a mandatory preflight and apply tight bounds before deriving or slicing lengths. The proposed client is a receiving-client design only; it does not authorize device contact or automatic invocation.

## Native report contract reviewed

`outputs/ps5-mod-bridge/native-probe/report.c` writes schema version 1 with top-level `schema_version`, `record_kind`, `evidence_origin`, `build`, `candidate_entry_abi`, `environment`, `observations`, `output_resolve_code`, and `capabilities`. The environment’s firmware/loader/version/game/update fields are null. Each of ten capabilities is unknown with null evidence. Four observations are `getpid`, `getuid`, `geteuid`, and `getpagesize`; each has a status, integer-or-null `value`, and integer `resolve_code`. `report.h` defines those statuses and fields. Probe output is not a receipt of its own execution unless independently tied to a real invocation; the current schema contains no such correlation field.

This contract is intentionally subordinate to `outputs/2026-09-30-loader-abi-review.md`: the review identifies the first-argument resolver mismatch and a manager path that cannot receive output. The native candidate is not ready for hardware execution, and the loader running on the reported console remains unknown.
