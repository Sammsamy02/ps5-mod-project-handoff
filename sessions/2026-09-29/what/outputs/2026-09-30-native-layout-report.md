# Native load-layout correction report — 2026-09-30

The native diagnostic now builds with disjoint 16 KiB load pages. The corrected ELF remains **not ready for hardware execution**: the first-slot getpid/resolver mismatch, output collection and spawned-process return behavior remain unresolved. No console contact, payload transfer or native ELF execution occurred.

Plan: `outputs/2026-09-30-native-layout-correction.md`. Baseline preserved by root at commit `2519883`. Implementation scope: `native-probe/**` and this report; no staging or commits by the implementer.

## Changes

- Added a local GPL-3.0-or-later linker script, derived from the SDK v0.43 script. Executable, read-only and writable loads begin on separate 16 KiB pages. The build uses this script; pinned vendor script and license bytes remain unchanged.
- The verifier now rejects unaligned nonempty load starts, intersecting page ranges, uint64 address-end overflow and page-rounded-end overflow. Its JSON explicitly reports these checks and accepted page ranges.
- Added independent ELF-header fixtures for each rejection and a valid aligned, disjoint layout.
- Added the Makefile `PYTHON` override and documented it. Native entry, collection, report formatting and payload argument sources are byte-identical to the baseline.

## RED → GREEN evidence

Before implementation, the new rejection tests demonstrated that the old verifier accepted unaligned loads, shared pages and both overflow cases: four assertion failures. The positive fixture was accepted but the new explicit property fields were absent. After implementation, all 11 verifier tests passed. The complete native suite passed **16 tests**, including its desktop ASan/UBSan fixture and dependency/preparation rejection tests.

Commands completed successfully using the bundled Python runtime:

```sh
make test PYTHON=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3
make clean
make inspect PYTHON=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3
```

The clean rebuild checked 13 pinned local tool/vendor files before invocation, compiled freestanding x86-64 objects with LLVM 21.1.8, linked the candidate, ran the acceptance verifier, and saved independent LLVM readelf/disassembly outputs. An additional direct Python ELF-header read agreed with LLVM's program headers. Baseline source comparisons and vendor SHA-256 comparisons also passed.

## Corrected artifact

File: `outputs/ps5-mod-bridge/native-probe/build/native-probe.elf`.

- SHA-256: `33fba54b4e18d9467a3b295aca080a411729758e12ca91f6015c9dd74dd3be3f`
- Size: **50,848 bytes**
- Format: ELF64 little-endian x86-64 PIE; entry relative address `0`
- Static checks: no needed libraries, undefined imports, relocations, constructors or direct syscall encodings

| PT_LOAD | File offset | Relative start | File/memory size | Flags | Accepted page range |
|---|---|---|---|---|---|
| 0 | `0x4000` | `0` | `0x1664` | RX | `[0, 0x4000)` |
| 1 | `0x8000` | `0x4000` | `0x3a5` | R | `[0x4000, 0x8000)` |
| 2 | `0xc000` | `0x8000` | `0xe0` | RW | `[0x8000, 0xc000)` |

Each load has `p_align = 0x4000`. PT_DYNAMIC occupies the writable load at `0x8000`. This removes the baseline's conflicting RX/RW page coverage, without asserting actual loader mapping or runtime compatibility.

Unchanged pinned vendor linker-script SHA-256: `3bacf56a21602a1298752023a4c5b0c305954ddf6cdbce1464476fc9da9d9093`. Both licenses and the preserved public SDK header also matched their manifest hashes.

## Completion ledger

- [x] Independent rejection fixtures written and observed RED.
- [x] Local derived linker script added; pinned vendor evidence preserved.
- [x] Bounded, aligned, disjoint load-page checks implemented and reported.
- [x] Full native tests, clean rebuild, static acceptance and independent inspection completed GREEN.
- [x] Hash/program headers recorded; remaining loader blockers retained.

Ruling: verification covers the whole native subsystem; root owns the overall product checks and final review. This keeps implementation ownership within the assigned native scope while retaining independent review by root. No runtime/translation capability is promoted by this correction.
