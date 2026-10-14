# Saved FName construction routine ABI audit

The exact retained game ELF supports this adapter call boundary:

```cpp
struct alignas(8) NativeFName8 {
    uint32_t comparison_index;
    uint32_t number;
};
using NativeCtor = void (*)(NativeFName8* out,
                           const char16_t* name_utf16,
                           uint32_t find_mode);
// RDI = writable output; RSI = NUL-terminated UTF16; EDX = 32-bit mode.
// Bootstrap self-check uses mode 0 (Find).
```

The output pointer is mandatory. The previous `FName(const CharType*, EFindName)`
by-value function declaration places the input in RDI and mode in ESI, so it does
not describe the game entry point. The saved crash evidence shows the resulting
eight-byte write into the ByteProperty literal at runtime RIP 0x63d5f0.

Only reports in this audit directory were written. No runtime/build edits,
network/console contact, payload dispatch, upload or launch occurred.

## Exact artifact and address mapping

- Artifact: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`.
- Size: 194,241,424 bytes.
- SHA-256: `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`.
- ELF64 little-endian x86-64 program headers were parsed with Python `struct`.
  The recorded section-header offset `0x6f48a0a8` exceeds the file; section
  headers were not used.
- PT_LOAD 0: file offset `0x4000`, linked virtual address 0, file/memory size
  `0x7479b1c`, flags `PF_X` only. Thus constructor linked vaddr `0x23d590`
  maps to file offset `0x241590`. Observed load base `0x400000` makes runtime
  address `0x63d590`.
- Complete construction routine: `[0x23d590,0x23d5fa)`; six following bytes
  are INT3 padding before the helper at `0x23d600`.
- Capstone 5.0.7 decoded every byte of the seven reported code ranges,
  including the complete downstream helper and two complete real game callers.
  All 23 local verification assertions passed; see `VERIFICATION.json`.

## Register and data contract

`DISASSEMBLY.txt` contains complete code for the entry point, downstream helper,
mode dispatcher, lookup helper and selected real game callers.

| Contract | Saved instruction evidence |
| --- | --- |
| Output destination first | `0x23d599: mov rbx,rdi`; `0x23d5f0: mov qword ptr [rbx],rax` |
| Input second, UTF16 words | `0x23d596: mov rax,rsi`; `0x23d5a1: movzx edi,word ptr [rax]`; loop loads `[rsi+2]` and adds 2 |
| NUL terminated; no external length | Entry scans until zero word; `0x23d5d8: sub rsi,rax` then `bextr` with start 1/length 32 derives the UTF16 unit count from the byte difference |
| Third argument is 32-bit mode | Entry and normalization helper preserve EDX to `0x242470`; dispatcher compares `edx,1` at `0x2424e9` |
| Exactly eight output bytes | Single qword write through saved output pointer; no display-index write |
| Two 32-bit output fields | `0x23d83f` shifts numeric suffix in R14 by 32, `mov eax,eax` clears upper bits of the returned comparison id, then OR combines them |

The entry computes a packed string view internally, with a width flag in bit
32 when the scanned input has code units above 0x7f. The helper converts ASCII
UTF16 input to a narrow temporary as an internal optimization; this does not
change the external input contract. The helper recognizes numeric suffixes and
places the internal number in the output high 32 bits.

The eight-byte footprint and low-id/high-number packing support the
non-case-preserving layout for this exact entry point. An owned buffer aligned
to eight bytes is conservative and matches all selected temporary-output
callers. The binary does **not** establish the original C++ `alignof(FName)`;
the profile's alignment 8 is an adapter allocation requirement. Retained RC
headers support both alignas(4) and alignas(8), depending on `FNAME_ALIGN8`.

Use explicit `char16_t` at this native boundary and require `sizeof(char16_t)==2`.
The actual pinned Prospero compiler's wchar_t size is a separate parent-owned
compiler verification; host wchar_t size does not determine the game ABI.
The adapter should supply an owned bounded NUL-terminated input. The entry's
initial NUL scan itself is not bounded by an external length argument.

## Real game callers

These are bytes from the saved game, decoded from real function prologues;
they are not host stubs or the failing runtime wrapper.

1. Caller `[0xf7e0,0xf84a)`: at `0xf808` loads UTF16 `LogOutputDevice` into
   RSI, at `0xf80f` loads output `[rbp-0x20]` into RDI, at `0xf813` sets
   EDX=1, and calls at `0xf824`. It then reads `[rbp-0x20]` into RAX at
   `0xf829` and copies those eight bytes into an object.
2. Caller `[0xa4d4f0,0xa4d535)`: incoming input RDI is copied to RSI;
   output `[rbp-0x18]` is passed in RDI; EDX is zeroed at `0xa4d507`;
   call occurs at `0xa4d510`. It then reads `[rbp-0x18]` into RDI at
   `0xa4d515` before calling another function.
3. Caller context `[0x47ac0,0x47cbe)`: passes an empty UTF16 literal in RSI,
   output `[rbp-0x48]` in RDI and EDX=1; call at `0x47ca0`. It reloads
   the qword output from `[rbp-0x48]` at `0x47ca5`.

The raw E8 relative-call byte search yielded 14,401 candidates. That count is
not claimed as the instruction-verified caller count. The selected ranges
were independently decoded and validated against the exact target address.

## Constructor, hidden output and RAX

The observed machine contract is an output-first construction routine and is
consistent with a C++ member constructor (`this` in RDI). The source declaration
and symbol are absent, so `FName::FName(...)` is an inference, not a recovered
name. Calling it a generic hidden return-buffer function does not establish
the original source declaration either.

A conventional hidden-sret return expects the destination pointer back in
RAX. This routine instead stores the packed helper result through RDI and
leaves those packed bits in RAX. That favors the member-constructor
interpretation. More importantly, the real callers consume the writable
output slot. The adapter can safely model this demonstrated contract as
`void(out,input,mode)` without resolving the unavailable original symbol.

Be precise: in **this exact saved artifact**, RAX happens to contain the same
packed bits as the output at RET. The audit does not claim it is always empty
or garbage. Those incidental bits do not establish a by-value FName return
contract, and never justify removing the output argument or interpreting the
existing two-argument wrapper as ABI-correct. Ignore RAX and read the owned
output only.

## Bootstrap Find-only check and retained ordinary Add API

The retained RC enum declares Find=0, Add=1, Replace=2. The saved dispatcher
corroborates the distinction: mode 1 goes to `0x23c0b0`; mode 0 goes through
hash/lookup and `0x23c340`. The latter searches existing buckets and returns
zero on a miss. For invalid long lengths, mode 0 returns zero directly instead
of taking the nonzero-mode fallback that recursively uses mode 1.

The bootstrap self-check must use mode 0 only. Add=1 is prohibited for that
probe because a missing validation name must not be inserted. Keep the live
ByteProperty-id comparison after the call and leave the adapter disabled/reset
on failure. Find avoids the insertion route; it is not a claim that every
downstream operation is side-effect-free: locks and a guarded lazy-pool
initialization still exist in the native code.

Ordinary retained constructor API consumers continue to support both Find=0
and Add=1 through the same output-first UTF16 thunk. The adapter accepts 0/1
and rejects Replace=2 and other values before the native call. The saved real
game caller at `0xf824` corroborates the same ABI with mode 1. This is evidence
for argument/output shape, not a complete audit or console proof of insertion
side effects.

The parent's current `receipts/HOST-TESTS.json` records successful normal and
ASan/UBSan adapter runs, with hashes matching the current adapter and test
sources. `tests/test_adapter.cpp` lines 32–33 explicitly forward Add=1 to a
host stub, assert its mode/output and restore the Find=0 expectation. That
host assertion checks retained Add forwarding; it does not execute the real
game's Add implementation or establish console behavior.

## Live identity requirement

The exact expected first 32 bytes are:

`554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690`

The legacy first 16 are:

`554889e553504889f04889fb4885f674`

Each prefix occurs once in the saved file and once in its executable PT_LOAD.
These are profile facts only. They are not evidence that the live process has
the same bytes. Require the fresh existing kernel-copyin owned capture route
before the first native call, compare only owned bytes, and refuse on missing
or mismatching live prefix. See `LIVE-CAPTURE-AUDIT.md`.

The audit proves a saved-binary calling contract and a source-supported
capture path. It does not prove a working adapter, an available live capture,
successful startup, menu input or an item spawn on the console.
