# L21 allocator evidence — build blocked

All **17 linked FMalloc wrappers** are inventoried; **0/17 have both authoritative UE5.6.1 Shipping declaration proof and image corroboration**. Six image-semantic candidates are supported below; the remaining eleven semantic assignments stay unresolved. No profile or runtime source was changed and no runtime was built.

Scope is the saved Townfall PS5 Shipping image, SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, **194,241,424 bytes**. The selected profile records target `prospero-x86_64-sysv`, pointer size 8, revision `5.6.1-0+++UE5`, and build-id `04bb4330a7d71dcefe1cc2c4d19769d5`; that revision is explicitly a semantic reconstruction, not proof of the actual preprocessed engine declarations. This image does not prove an unconditional other-game ABI.

The inspected runtime package is SHA256 `885f43c69b5c23fd4db422905297268902d2183770ca8d735bf068d3a1d9084c`, **31,149,719 bytes**; its bare ELF is SHA256 `3d83d03a681ce8ce2f498f4879b6e99a4c3d8bf62ea45cfb9e3ec7068b23db5e`. The 17 distinct FMalloc failure strings in that ELF match the 17 wrappers in selected `FMemory.cpp`. Direct `GMalloc` callers were found for Malloc, Realloc, Free, QuantizeSize, GetAllocationSize, Trim, SetupTLSCachesOnCurrentThread, and ClearAndDisableTLSCachesOnCurrentThread; linked presence alone does not establish reachability of the other nine.

## Declaration/ABI result

The user explicitly confirmed there is no UE5.6.1 source checkout on this Mac. No exact `MemoryBase.h`, `Exec.h`, or `MallocBinned3.h` was found in the inspected local source/output trees. The licensed cache receipt pins Epic `5.6.1-release` commit `6978b63c8951e57d97048d8424a0bebd637dde1d` for other headers, but contains no authoritative FMalloc/FExec declaration. Runtime `FMemory.hpp` is a nonvirtual offset-map wrapper; generated `5_06_VTableOffsets_FMalloc_FunctionBody.cpp` has MSVC-shaped `__vecDelDtor` entries and is not native source evidence. Exact Shipping/Prospero `#if` effects remain unresolved. See [declaration availability and searches](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/DECLARATION-AVAILABILITY.json).

[Itanium ABI §2.5.2](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable-components) specifies a complete/deleting pair **for a virtual destructor** and preserves primary-base layout. That conditional rule does not prove these engine classes declare one or establish platform-specific additions. In this image, slot 0 targets `0xb1070` (vptr reset plus member-cleanup shape); slot 1 targets `0xb1440`, which calls `0xb1070` then `0xaf390`. These support a destructor-pair interpretation for this concrete image, but do not substitute for FExec/FMAlloc source order. No universal “add eight bytes” rule was applied.

## Saved-image result

All addresses below are **linked ELF virtual addresses**. The retained runtime base is `0x400000`; add it only to obtain the historical expected runtime address. File offsets are calculated from PT_LOAD.

Factory instructions at `0xb64f..0xb660` construct the object at `0x93089b8` and store it in global `0x930add8`. Constructor instructions at `0xafdc3..0xafddc` store address point `0x7ec26e0`. Both globals occupy BSS; their live values cannot be read from this file. The table is reconstructed from `DT_RELA` at file `0x932f430`, size `0x2607ee8`, with type-8/symbol-0 addends. Its raw pointer words are zero before relocation. All 32 inspected consecutive slot targets lie in executable PT_LOAD 0 (`p_flags=1`, X-only file metadata).

| Semantic candidate | Measured byte offset | Linked target | Independent image behavior |
|---|---:|---:|---|
| Malloc | `0x30` | `0xb48e0` | Count/alignment call shape; removes a free-list entry and returns pointer |
| Realloc | `0x40` | `0xb49b0` | Original/Count/alignment; allocation/copy/release paths |
| Free | `0x50` | `0xb4bf0` | One pointer argument; returns block to free list |
| QuantizeSize | `0x68` | `0xb4cd0` | Size/alignment rounding through bin tables; returns rounded size |
| GetAllocationSize | `0x70` | `0xb4db0` | Derives size from pointer, writes `[rdx]`, returns boolean |
| GetDescriptiveName | `0xd0` | `0xb2810` | Returns UTF-16 `Binned3` at `0x78cb9da` |

These are **image-semantic candidates, not approved profile values**. Direct RIP-relative GMalloc census found 11 loads and 10 first virtual dispatches: six at `+0x30`, two at `+0x40`, two at `+0x50`. This bounded encoding/dispatch census is not a whole-program control-flow or alias proof. QuantizeSize/GetAllocationSize candidates are supported by their complete target bodies, not by a GMalloc-direct callsite found in that census.

TryMalloc/TryRealloc forwarders, Trim/TLS-shaped functions, shared void-return stats slots, and shared true-return slots cannot independently establish all remaining method identities. Their candidate offset fields remain null; no guessed native offsets were emitted. Every member has status, source calls, generated-PC comparison value, and exact evidence references in the [machine-readable candidate table](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/measured/candidate-table.json).

## Live/file distinction and corrected historical labels

The saved launch log at line 360 reports `0x970add8 -> 0x97089b8` with V1 instance/V2 vptr match and “identity and ABI unverified.” `SeedValidation.hpp:709–728` explicitly says the table is compared as a constant and **not dereferenced** at that gate. This audit reads no live slots and proves no successful allocator virtual call. ELF flags are not current VM-permission measurements. Historical labels were not imported blindly: target `0xb290` is a void-return stub (not a string-return stub), and the free thunk starts at `0xddc0` (not `0xddb0`).

## Reproduction and remaining gate

Run `python3 outputs/2026-10-08-l21-audit/allocator/measure_allocator.py --out reproduced-new` from the worktree. The script requires Python 3 and Capstone, verifies the image hash and selected source pins, parses ELF headers/relocations, saves byte-addressed disassembly and references, and performs no network operations. Two executions produced byte-identical evidence: [verification](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/measured/verification.json), [reproduction receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/REPRODUCIBILITY.json). Full hashes and saved-log excerpt are in [input pins](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/measured/source-pins.json); [disassembly](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/measured/image-disassembly.txt) and [structured ELF evidence](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/measured/image-measurements.json) preserve the raw support.

**Build remains blocked:** obtain/reconcile authoritative exact FExec/FMAlloc declarations and Shipping/Prospero macro provenance; independently bind the eleven unresolved identities; then reconcile all 17 methods before any profile promotion. Saved instance/vptr equality must remain distinct from any later live slot admission. This report authorizes no package, upload, payload, console contact, or production account action. Only anonymous public primary ABI/API documentation was additionally read; Epic API pages returned no usable content.
