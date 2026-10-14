# REPORT — worker-re-fname-pool, 2026-10-04

## Task
Trace how the game's FName pool allocates and initializes Blocks[0] in the PS5 UE 5.6.1 shipping
binary (`decrypted-eboot.bin`, sha256 3b6cfad4…), so the pool validator (SeedValidation.hpp
kPoolLayout) can be fixed. Feeds a live validator fix.

## Delivered
* `CLAIM.json` — every claim with address + raw bytes + disasm excerpt.
* `FNAME-POOL-RE.md` — full findings + validator predicate recommendations.
* `tools/` — tier-1 scripts and raw extracts (scan_refs.py/json, scan_range.py/json,
  scan_correct_base.py/json/out, plt_map.py/json, struct_check.py/json, callers.py, ptr_refs.py,
  rdis.py (recursive-descent disassembler), vmap.py, disasm listings).
* No project files were modified; everything written under this output dir (+ Ghidra scratch
  project in /tmp/wrfp_ghidra_proj, created because the analyzer rejects paths containing a
  dot-element like `.codex`).

## Answer in one paragraph
Blocks[0] is allocated by the pool's lazy init function `FNamePool::Init` @ file 0x23a5d0
(runtime 0x63a5d0), reached from the first `FName` creation (ctor @ 0x23d590 → FindOrStore
0x23d600 → core 0x242470, gated on a flag byte at pool−0x20 = 0x9365ea0). Init does
`memset(pool+8, 0, 0x10008)` (CurrentBlock=0 @ +0x8, CurrentByteOffset=0 @ +0xC, Blocks[]=null),
then `FMemory::Malloc(0x20000, align 2)` and stores the result at pool+0x10 → **Blocks[0],
block size 0x20000 bytes**. FMemory::Malloc (file 0xb7a20) goes through the GMalloc slot
(file 0x9350950) and calls the FMalloc interface vtable slot 6 (+0x30). The concrete allocator is
a static BSS object (runtime 0x97089b8, ctor 0x4afd40, 76 pool tables × 0x78 B — FMallocBinned2-
family), whose Malloc (file 0xb48e0) is binned: 0x20000 size limit, 16-byte max alignment,
per-thread freelists, slow path 0x4b1460. Subsequent blocks are allocated by the reserve
function 0x246090 with the same `Malloc(0x20000, 2)` call, `CurrentBlock++`,
`CurrentByteOffset=0`, and a sliver-terminator write (`and word [...], 0x3f`) in the previous
block. Core-name ids ("None", "ByteProperty", ...) are written at pool+0x14040... during Init —
the port's NoneId/BytePropertyId offsets are correct.

## Key validator corrections (details in FNAME-POOL-RE.md §4)
1. Pool base 0x9365ec0 stands (the transient "0x9369ec0" was my own tool offset bug, found and
   retracted — see pitfall below).
2. **CurrentBlock is the index of the open block, 0-based — 0 is valid.** Replace
   `current_block >= 1` with the engine's own predicate `id>>16 <= CurrentBlock` (getter at
   0x23d570). This is the most important fix for a live validator.
3. Do not hard-require Blocks[0] "outside image / inside heap-class span": static evidence says
   OS-backed heap, but the launch-9 observation placed it inside the module BSS page range
   (LOAD7 memsz-only = runtime [0x97085f2, 0x9a33418], which contains the observed
   [0x970c000, 0x9a34000)). Unresolved — flagged, not guessed.
4. Strong cheap identity check: entry 0 of Blocks[0] is ANSI "None" (u16 header `h`: `h>>6==4`,
   `h&1==0`, payload "None").

## Honest limits
* The PS5 dynamic symbols are scrambled; all function identity is behavioral (no names).
* The binned slow path (0x4b1460) was not traced to individual sce syscalls — "OS-backed pages"
  is inferred from the allocator's shape (pool tables, TLS caches, size classes), not proven
  line-by-line.
* Whether the 0x28-byte thread-forwarder wrapper is installed at runtime is statically
  undecidable (depends on inner `vtable+0xC0`); it forwards only.
* The 256 aux buffers at pool+0x1004C..0x1404C and the hash-bucket insert machinery (0x23c340,
  0x243310) were mapped but not fully reversed — not needed for the Blocks[0] question.
* Two tool passes during the session used a wrong vaddr↔file-offset assumption (LOAD0 delta is
  +0x4000). All conclusions above come from the corrected, re-run passes; the wrong-base
  intermediate artifacts are retained in `tools/` and marked in CLAIM.json `_meta` so nobody
  re-derives them.
* capstone linear sweeps drift inside large functions; final claims come only from
  recursive-descent disassembly started at int3-boundary prologues.

## Ghidra
Ghidra 12.1.4 headless import + auto-analysis (project /tmp/wrfp_ghidra_proj/fnamepool) was
still running at delivery (194MB target, sharing the machine with a controller-spawned Ghidra
instance over the same binary). All claims are Tier-1 capstone/lief. When it finishes,
decompiling 0x23a5d0, 0x246090 and 0xb48e0 and diffing against FNAME-POOL-RE.md §2 is the
recommended corroboration pass; a ready-made script skeleton would be
`analyzeHeadless /tmp/wrfp_ghidra_proj fnamepool -process decrypted-eboot.bin -noanalysis
-postScript <your>.py` with JAVA_HOME=/opt/homebrew/opt/openjdk@21.

## Runtime-facts requested for the unresolved item
One fresh live sample would close §7.1: read the qword at runtime 0x9765ed0 (Blocks[0]) and the
u32 at 0x9765ec8 (CurrentBlock), plus the launcher region map around the image — that decides
"OS heap vs image BSS" for the validator's region policy.
