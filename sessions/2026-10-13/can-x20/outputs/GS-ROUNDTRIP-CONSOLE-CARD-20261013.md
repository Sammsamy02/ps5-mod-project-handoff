# First console gate: no-game GS round trip

**Prepared offline; not uploaded or run.** The standalone source and target ELF are in the [build evidence bundle](gs-roundtrip-probe-build-evidence-20261013.zip), with hashes in the [manifest](gs-roundtrip-probe-manifest-20261013.json). The ELF SHA-256 is `5b4e48420130c2a69834cae0b1133945af93c0fc41596edb2e1ac7accbf1d979`.

The probe uses the [stage 42 backend candidate](PS5-GS-BACKEND-CANDIDATE-OFFLINE-20261013.md) on its own payload thread, without loading a PE or touching a game. It records the original host GS base, creates a local TEB with self and TLS-array pointers, enters the guest GS view, reads `GS:[0x30]` and `GS:[0x58]`, restores host GS, and reads the host base again. It reports `PASS` only when both guest fields and the restored host base match. An unrecoverable restoration aborts the payload.

The pinned PS5 SDK compiled and linked the source with `-Werror`. Target disassembly contains both required GS reads, and the ELF imports `sysarch`. These are build checks only. The generic LLVM reader also warned about this SDK ELF's `PT_DYNAMIC` size, as it did for the earlier #3 target diagnostic; the SDK linker exited 0.

When console access and the integrator's test window are available:

1. Check the ELF hash against the manifest and capture firmware, resident runtime identity, payload loader, and boot ID.
2. Run once without a game. Retain stdout, exit code, and any crash/recovery log.
3. Require `self=PASS tls=PASS host-restored=PASS` and `verdict=PASS`; any missing line, nonzero exit, or crash is a failed gate.
4. Only after that gate, test the stage 42 backend with a synthetic PE lifecycle. Keep unchanged gameplay DLLs behind the existing guarded refusal until imports, CRT/SEH, target matching, and observable behavior have their own evidence.

No console action was taken while preparing this card.
