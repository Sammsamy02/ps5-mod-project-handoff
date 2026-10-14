# Native load-layout correction

The loader source review exposed a concrete static integration problem: the candidate's executable and writable load segments share 16 KiB pages. Upstream elfldr applies segment permissions in order, so the later writable segment can remove execute permission under its FreeBSD-like mapping behavior. Actual console semantics remain unmeasured.

Correct the locally generated ELF layout before attempting runtime work. Preserve the verified upstream linker script unchanged as vendor evidence. Add a local GPL-derived script with every nonempty load segment starting on a 16 KiB boundary, without page overlap. Keep the existing freestanding entry, resolver-only behavior and no default CRT or direct syscall instruction. This task does not resolve the first-slot getpid/resolver convention, output collection or spawned-process termination.

Implementation plan:

- [x] Add verifier rejection fixtures for unaligned load starts, overlapping pages and overflowing address ranges; observe failure before correction.
- [x] Add a local linker script and use it for the candidate build; retain pinned vendor bytes and licenses.
- [x] Require bounded, aligned and disjoint page ranges in the verifier, reporting the checked property explicitly.
- [x] Run native tests, clean/rebuild/inspect, and independently compare program headers and unchanged resolver behavior.
- [x] Update the native report/hash and retain an explicit not-ready-for-hardware status until remaining loader issues are corrected.

Review scope: static ELF integrity and local build evidence only. No console writes, payload transfer or execution. The user's unattended development instruction authorizes this reversible correction. Root serializes commits; implementer owns native-probe files and a separate layout report, while root owns overall progress documents.

Independent final review passed all 16 native tests, forced a fresh cross-build/static inspection and checked eight further zero-size/high-address/page-rounding cases. Program headers and SHA-256 matched the implementer's report; vendor, entry/collector/report and argument sources remain unchanged. No additional consequential finding was identified in this scope.
