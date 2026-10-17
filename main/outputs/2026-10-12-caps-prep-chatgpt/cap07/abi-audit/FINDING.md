# ABI finding: +0x108/+0x110 are not established async-read seams

Mac-only, 2026-10-12. Canonical decrypted image freshly hashed to `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (194,241,424 bytes), Capstone 5.0.7. Addresses below are image virtual addresses, mapped through actual ELF PT_LOAD records. No RELA parsing or hardcoded relocation coordinates were used in this analysis. Existing cap07/evidence/vtable-audit.json supplies target references: table address point0x7fee8c0, +0x108→0x1770e40, +0x110→0x1769800. Target-to-slot relation is referenced, not independently reconstructed here.

## Concrete argument and return facts

| Slot | Fresh binary fact | What it rules out |
|---|---|---|
| +0x108, target0x1770e40 | Saves RSI as RBX; saves EDX/ECX in R15D/R14D at0x1770e4e–0x1770e54. On forwarding, loads inner from `[this+8]`, restores RSI, zero-extends the two saved low-byte flags into EDX/ECX, calls inner +0x108 at0x1770e7d, and returns the full RAX. On lookup hit, returns zero. | No evidence of an async callback/request parameter. It is a pointer-like object-returning path with two byte-valued flags, not a directory visitor callback. Exact function/type names remain inferred until independent source agreement. |
| +0x110, target0x1769800 | Calls0x1768e50 with original RDI/RSI; saves original RSI for forwarding. Moves helper EAX into ECX, sets AL=1, tests CL; if false, calls inner +0x110 with original RSI at0x1769824. Returns AL from the predicate/forwarder. Upper RAX bits are not established pointer payload. | Cannot safely treat this as an async-open handle return or cast it to an object pointer. It is a low-byte predicate on the observed path. |

## Independent caller data flow for +0x108

Function beginning0x1771040 (included in window-1770e40.txt) calls the actual **inner-chain** vtable at0x17710d7: RDI=`[this+8]`; RSI is its first supplied path; EDX=0; ECX is another caller flag's low bit. The returned RAX is saved as R12 at0x17710eb, null-tested at0x17710f6, then saved at `[rbp-0x90]`.

A separate object, returned from0x176ff00, supplies a remaining-byte count through its vtable+0x58 at0x1771124. The function allocates a 0x10000-byte buffer, asks that separate object to fill it using +0x28 at0x1771155 (RSI=buffer, RDX=chunk length), tests AL, then passes **the same buffer and length** to the +0x108-returned object through its vtable+0x38 at0x177116c and tests AL again. It decrements remaining bytes and repeats. The returned object is released through +0x8 at0x17711ed.

Inference: +0x108 is a synchronous destination/write-handle-opening shape in this path. This is strong positive counterevidence to PAK-PIN-PASS.md's `OpenAsyncRead-class` shorthand. It does **not** independently establish the C++ name, complete handle interface, all flag meanings, all calling conventions, or full class identity. Do not ship an inferred signature from this receipt alone.

## Directory predicate versus directory iteration

Function starting0x1768fd0 (in window-1768e50.txt) calls inner-chain +0x110 at0x1769062 and tests **AL** at0x176906a. When true it builds/passes a visitor-shaped stack object, selects **+0x130 versus +0x1c0** using the caller's low-byte control flag (0x176907d–0x17690ce), and performs the selected virtual call. A second independently entered function0x1767e70 calls inner +0x110 at0x1767e91, tests AL, then optionally calls inner +0x1e0 at0x1767eac.

Inference: +0x110 is directory-existence/predicate-shaped; the iteration itself is a subsequent, separate visitor dispatch. Confirm the exact source name against an independent interface definition before assigning it. Existing +0x1c0 directory-enumeration evidence at0x1769ac4 was re-decoded from function start0x1769a20 with RDX pointing to a stack visitor; it is not the +0x110 predicate.

## Why IoStore-neighborhood displacement hits misled the earlier audit

The bounded raw-pattern scan found +0x108 call instructions in0x1710000–0x178b000, but equal displacement does not mean equal class. A decisive counterexample is0x171c9d6, decoded continuously from0x171c950: constructor code at0x171c987 loads vtable address **0x7ec5cd0**, stores it to `[R12]` at0x171c9ac, and calls that same object's +0x108 after a helper call. This is a different address point from FPakPlatformFile's0x7fee8c0, not a platform-file chain handle-open proof. Similarly0x17459d7's +0x108 result is tested through AL; its caller's class is unresolved here. Do not aggregate these unrelated slot offsets as evidence of an async-read seam.

## Result and remaining blocker

Remove the assumption that +0x108/+0x110 identify the actual utoc/ucas async-read handles. These receipts establish concrete register widths, return usage and separate copy/iteration flows. Root's independent source inspection can confirm or refute exact names. The actual read seam still needs a caller with independently established IPlatformFile receiver provenance and path/handle-flow evidence, plus live observation in a later authorized launch. No runtime patch, new pin, executable change, compiler/build, console action or network access was performed.

`receipt.json` records every bounded window's address, ELF file offset, byte length, SHA256 and instruction count, canonical image hash, Capstone version, reference-file hashes and search limitations. `window-*.txt` contains bounded decoded instructions only; no Epic source was copied.
