# Captured provider closes a narrow transfer-register case

Actual retained libc.prx is1,911,850 bytes with SHA256 `6951ddd7df5daabfbc9ac495ea0eee73e7de121cddb30a1e979fad005a7c6b04`. Its actual ELF metadata defines NID `+P6FRGH4LfA` as FUNC symbol445, name `+P6FRGH4LfA#D#A`, VA0x3b90, declared size54, symbol-record file offset0x1ab980. Actual SONAME libc.prx, own module libc version0x0101 and library libc version1 match the current saved game import's module/library names and versions. Numeric library/module IDs are image-local: provider3/0 versus game58/56. Full encoded strings therefore differ; matching the NID and provider names/versions is the relevant association. This is a matching captured candidate, not proof of the live game loader's selected bytes or firmware-wide compatibility.

The earlier retained strict metadata contains exactly one matching defined export. The replay verifies its actual symbol/provider bytes rather than trusting the derived JSON alone. A retained public StubCatalog-derived declaration mentions memmove, but the present conclusion does not depend on that label, a guessed C prototype, or an SDK declaration. No Sony SDK/platform-extension source or implementation was used; identified SDK-derived runtime archives/disassembly were excluded.

Entry VA0x3b90/file0x7b90 checks RDI+RDX for unsigned wrap, then whether that address range overlaps its stack frame interval. Failed paths write an FS-relative diagnostic and execute int0x45. On the passing path it reaches common dispatch at0x3c06, beyond this export's declared54-byte range. Dispatch sets RAX to incoming RDI and uses the actual RDX value to select a signed-relative jump table for counts below17. The relevant table base is0x15c1b4/file0x1601b4, independently computed from the RIP-relative LEA. Exact branch/table bytes and file offsets are in receipt.json.

| Incoming RDX | Actual table target | Actual retained path |
|---|---|---|
|0|0x41c7|Restore RBP, return; no data load/store on this selected path.|
|8|0x4215|Load qword from RSI into RDX; store that same qword to RDI; restore RBP, return.|
|16|0x429d|Load unaligned16 bytes from RSI into XMM0; store that XMM0 to RDI; restore RBP, return.|

For these selected count8/16 paths, the entire source value is loaded before its destination store; this supports overlap-safe snapshot transfer for those exact sizes with valid mapped ranges and passing guards (and the required vector instruction support for16). No data outside the destination width is written by the selected body. RAX remains the entry RDI on these paths. These are observed register/instruction semantics, not a general ABI declaration or exhaustive transfer contract. Counts beyond these paths remain unaudited.

This changes the next integration action: the saved array-transfer caller receipt supplies RDX8 for insert-before-one and pop-index0-of-two, and the captured candidate now supplies the corresponding eight-byte transfer semantics. RDX16 similarly supports a conditional two-pointer shift if a caller receipt supplies that exact count. The previously retained two-task append case has oldcount1/index1, so its exact setup at0x1dc90–0x1dcad supplies RSI=P+8, RDI=P+16 and RDX0. The captured count0 body performs no array data write after passing entry guards; this narrows the old opaque-helper prefix-retention assumption, conditional on actual provider binding. Its subsequent pop-index1-of-two computes remainder0 at0x1e9b4–0x1e9be and skips the import entirely. The existing two-task receipt does not execute count16: inserting before two existing entries or popping index0-of-three produces16 by the exact caller formulas, but requires separately established bucket predicates. CALLER-CONNECTION.json binds these existing caller window hashes and source receipts. Whole bucket/scheduler execution, pointer validity, entry-guard satisfaction and actual loader/provider binding remain separate prerequisites. Do not promote all nonzero shifts or shipping/runtime status from this narrow result.

Replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/transfer-contract-audit/audit.py
```

Replay retained5 windows,99 instruction bytes,31 instructions and3 exact table entries; full original SHA/size and metadata/provider association verified, source/context hashes unchanged before/after. The saved-binary slot was released immediately after this replay. No broad disassembly, payload/binary copy, target execution, new console/network/build/compiler/install, or mutation of genuine inputs occurred. Further source-only work can consume this exact-size conditional register contract; a general-size proof would need separately bounded paths and independent live-provider association remains unresolved.
