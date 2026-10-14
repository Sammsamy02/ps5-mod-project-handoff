# Loader ABI review — 2026-09-30

The existing native diagnostic is **not ready for execution through the presumed 9021 chain**. The host review passes; the native review identifies a concrete resolver incompatibility, an unresolved segment-permission conflict, and a manager launch path that cannot collect diagnostic output. This review contacted public source repositories only. It did not contact the console, send a payload, or execute the native ELF.

The parent agent reports that `GET /version` on the user's Payload Manager returned `0.5.2`. Firmware `13.60` remains user-reported. Neither observation identifies the running ELF loader binary or its build. The manager version must not be substituted for loader identity.

## Source identities

- Payload Manager v0.5.2: commit `e4e72dc9d831dd09ee8fabc222ad1e7f5139050e`, obtained from its official GitHub tag reference.
- elfldr v0.26: commit `54966675d3884ba9c773f0f52646f8f46b92cbbc`, obtained from its official GitHub tag reference. Public release metadata dates v0.26 to 2026-08-29. This is a source comparison target, not an identification of the user's installed loader.
- SDK source review: the candidate's existing manifest pins post-release commit `f7fd02e6e195902a449b5e664917be8c01888b34`; upstream v0.43 startup sources were separately reviewed. The v0.43 release commit is `d9c9519116944a7f1c22d262012c53b80b3520b7`. See [SDK provenance correction](2026-09-30-native-sdk-host-route.md#release-identity-correction-and-impact).
- Candidate inspected: `outputs/ps5-mod-bridge/native-probe/build/native-probe.elf`, SHA-256 `7aa8bab0d0a436db6d6274d4ac5effa5b1f4a8ed986b6bc10eb3b8cd8ea7e060`.

## Argument layout is compatible; first-slot semantics are not

The candidate's LP64 offsets and result slot match the six-field SDK structure. However, elfldr v0.26 writes the address of the `getpid` stub into offset 0, rather than supplying a genuine name resolver. See [elfldr argument construction, lines 329–342](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/elfldr.c#L329).

Candidate `probe.c:6–10` calls that pointer as a resolver and expects a zero result plus a populated output address. A getter stub returns a positive PID and leaves the address unset. A controlled desktop fixture reproduces `return=3`, `payloadout=3`, ten first-slot calls and no report emission. The fixture does not establish console behavior; it demonstrates the candidate's response to the documented calling convention.

[SDK v0.43 syscall startup](https://github.com/ps5-payload-dev/sdk/blob/v0.43/crt/syscall.c#L35) explicitly handles the overloaded field: it tests whether the pointer is a real `sceKernelDlsym` implementation, otherwise uses its address to establish a syscall trampoline. The candidate omits that machinery and the default CRT. Omitting the default privilege-changing initialization is deliberate; a compatible minimal entry needs its own separately reviewed resolver convention, rather than assuming the misleading field name proves a resolver is supplied.

## Segment permission conflict remains unresolved

The current ELF has these load segments, observed from its program headers and existing LLVM readelf report:

| Segment | Relative range | Flags | 16 KiB page range |
|---|---|---|---|
| First | `[0, 0x1664)` | RWE | `[0, 0x4000)` |
| Second | `[0x1668, 0x40e0)` | RW | `[0, 0x8000)` |

elfldr applies executable protection, then ordinary RW protection in program-header order. Its ordinary wrapper forwards the unaligned address directly to `mprotect`. See [mapping, lines 225–245](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/elfldr.c#L225) and [wrapper](https://github.com/ps5-payload-dev/elfldr/blob/54966675d3884ba9c773f0f52646f8f46b92cbbc/pt.c#L330).

Under [FreeBSD 9 source semantics](https://github.com/freebsd/freebsd-src/blob/releng/9.0/sys/vm/vm_mmap.c#L602), the second call truncates the start and rounds the enlarged length, covering both pages and removing executable permission. That is a source-based inference, not a verified statement about PS5 firmware 13.60. If PS5 differs, the call could instead fail or behave differently. The current alignment-only verifier accepts this overlap and therefore cannot prove compatibility. A candidate intended for this mapping algorithm needs an independently checked layout without conflicting page permissions.

## Entry return and output collection

elfldr passes arguments in RDI and installs a saved instruction pointer on the target stack. The candidate restores its stack and returns; the mechanical shape matches this entry setup. Returning safely in the spawned-process path remains unmeasured. The SDK's [termination logic](https://github.com/ps5-payload-dev/sdk/blob/v0.43/crt/crt.c#L147) distinguishes a hijacked process from a spawned one and uses `exit` for the latter. A simple `ret` does not reproduce that policy.

elfldr redirects the accepted connection to inherited fd 1. Payload Manager v0.5.2 [streams the ELF to its separate local port 9021 service, then closes the socket without reading](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/ps5_launcher.c#L19). The manager launch result records successful transmission, not successful diagnostic execution, report receipt or the payload's result slot. Its [/version route](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/http_server.c#L853) returns only `MENU_VERSION`.

A future experiment needs the actual loader identity, a compatible entry/resolver and segment layout, and a receiving client that preserves the connection and captures bounded output. No execution command or console change is supplied by this review.

## Host re-review

All four prior host findings are resolved in the latest sources: real account status normalization; repeat authorization through the actual `used` record; receipt acceptance limited to fixed safe Nexus/host error messages; and serialized disconnect invalidation with callback revalidation. Receipt-storage failure keeps the worker alive and exposes failure; symlink parents are checked before directory creation. Latest local `tests/host_tests.py`: **14 tests pass**. Known cancellation UI and active-download shutdown limitations remain unchanged.
