# FNamePool Blocks[0] — allocation trace, townfall decrypted-eboot.bin (PS5 UE 5.6.1 shipping)

Worker: worker-re-fname-pool · 2026-10-04 · Tier 1 (capstone 5.0.7 + lief 1.0.0), Ghidra corroboration pending
Target: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
sha256 `3b6cfad4…22fc` · 194,241,424 B · ELF64 x86-64, type 0xFE10, 5 PT_LOADs, 1,663,596 RELAs, no sections, Sony-scrambled dynamic symbol names.

**Address convention: every address below is a FILE VADDR; runtime = file + 0x400000.**
File-offset mapping: LOAD0/1/3/7 file offset = vaddr + 0x4000; LOAD8 offset = vaddr − 0x324000.

---

## 1. Bottom line

* **Blocks[0] is allocated by the pool's own init function, `FNamePool::Init` @ file 0x23a5d0 (runtime 0x63a5d0),** via one call:

```
0x23a609  bf 00 00 02 00     mov edi, 0x20000
0x23a60e  be 02 00 00 00     mov esi, 2
0x23a613  e8 08 d4 e7 ff     call 0x4b7a20        ; FMemory::Malloc(size, align)
...
0x23a625  48 89 43 10        mov qword [rbx+0x10], rax   ; Blocks[0] = result
```

* **Allocator:** the engine global `GMalloc` (pointer slot file 0x9350950, runtime 0x9750950), through the FMalloc interface **vtable slot 6 (+0x30)**. The concrete allocator is a **static BSS object at file 0x93089b8 (runtime 0x97089b8)** constructed in place by 0x4afd40 and reachable through the allocator-global at file 0x930add8. Its `Malloc` (file 0xb48e0, from vtable 0x7ec26e0 slot 6 relocation addend) is a **binned allocator**: fast path for `size < 0x20001 && align < 0x11`, per-thread freelists via `pthread_getspecific`, 16-byte size classes, slow path file 0x4b1460 — FMallocBinned2-family shape (76 pool tables × 0x78 B, per-table mutex).

* **Block size = 0x20000 bytes (128 KiB), alignment argument = 2.** The binned allocator's granularity is 16 bytes, so the returned pointer is at least 16-aligned in practice.

* **CurrentBlock / CurrentByteOffsets:** init does `memset(pool+8, 0, 0x10008)` → CurrentBlock (u32 @ pool+0x8) = 0, CurrentByteOffset (u32 @ pool+0xC) = 0, all 8192 Blocks[] = null. **CurrentBlock is the index of the block currently being filled (0-based), not a count** — it stays 0 until the first 0x20000 boundary is crossed. A pool with hundreds of names, all still inside block 0, legitimately has CurrentBlock == 0.

* **Image-residency:** the static chain (init → FMemory::Malloc → GMalloc → binned freelists/OS pool pages) never returns module-image memory. The launch-9 runtime observation that placed Blocks[0] inside `[0x970c000,0x9a34000)` **contradicts this** — that range is exactly LOAD7's BSS tail (LOAD7 memsz-only runtime range is [0x97085f2, 0x9a33418]). See §7 Unresolved; do not bake either answer into the validator as a hard region predicate.

* **Port layout verdict:** kPoolLayout {current_block +0x8, current_byte_offset +0xC, blocks +0x10, none_id +0x14040, byte_property_id +0x14044, stride 2, block bits 16, 8192 blocks} — **all confirmed against code**, with one semantic correction (CurrentBlock meaning) and one addition (hash bucket table at +0x10040).

---

## 2. How each piece was pinned (evidence trail)

### 2.1 The pool object is at 0x9365ec0

`FName::ToString`-style getter @ 0x243240 (runtime 0x643240), verified recursively disassembled:

```
0x243261  48 8d 3d 58 68 12 09   lea rdi, [rip+0x9126858]   ; -> 0x9365ec0  (lazy-init flag guard)
0x243277  48 8d 0d 42 68 12 09   lea rcx, [rip+0x9126842]   ; -> 0x9365ec0
0x24327e  c1 e8 10               shr eax, 0x10              ; block = id >> 16
0x243281  48 8b 44 c1 10         mov rax, [rcx + rax*8 + 0x10]  ; Blocks[block]
0x243286  41 0f b7 ce            movzx ecx, r14w            ; offset units = id & 0xFFFF
0x24328a  44 0f b7 3c 48         movzx r15d, word [rax + rcx*2] ; entry header (u16)
0x243297  41 c1 ef 06            shr r15d, 6                ; len = hdr >> 6
... (wide = hdr & 1 at the test dl,1 branch; probe-hash bits 5:1 appear in the store path)
```

A whole-file rip-relative scan (all lea/mov/SSE/movsxd/imm encodings, 103 exact field hits, 203 range hits) shows **100+ code references to exactly 0x9365ec0**, all inside the FName cluster file 0x23d0a2–0x247e8b. No relocations touch pool fields, no absolute 8-byte references exist anywhere in the file.

### 2.2 Creation path

Pinned ctor **0x23d590 is real** (my first pass misread it due to an offset bug — retracted below):

```
0x23d590  55 48 89 e5 ...        push rbp; mov rbp,rsp          ; FName::FName(const TCHAR*, EFindName)
0x23d5c4  83 f9 7f               cmp ecx, 0x7f                  ; OR-reduction of wchar units
0x23d5db  bf 01 20 00 00         mov edi, 0x2001
0x23d5e0  c4 e2 c0 f7 f6         bextr rsi, rsi, rdi            ; wchar length
0x23d5eb  e8 10 00 00 00         call 0x23d600                  ; FNamePool::FindOrStore
0x23d5f0  48 89 03               mov [rbx], rax                 ; FName = id | number<<32
```

FindOrStore 0x23d600 narrows/lowercases into a 0x400-wchar stack buffer (SSE path), parses `_NNN` suffixes, then calls the core @ 0x242470, which lazily inits the pool and probes the hash:

```
0x2424be  80 3d db 39 12 09 00   cmp byte [rip->0x9365ea0], 0   ; bInitialized flag at pool-0x20
0x2424c7  48 8d 05 f2 39 12 09   lea rax, [0x9365ec0]
0x2424d7  e8 f4 80 ff ff         call 0x23a5d0                  ; FNamePool::Init(pool)
0x2424e2  c6 05 b7 39 12 09 01   mov byte [rip->0x9365ea0], 1
...
0x242548  48 8d bc 08 40 00 01 00  lea rdi, [rax + rcx + 0x10040] ; bucket = pool + hash + 0x10040
0x242550  e8 eb 9d ff ff         call 0x23c340                  ; bucket list find/insert
```

### 2.3 Init @ 0x23a5d0 — the answer

```
0x23a5f9  48 8d 7b 08            lea rdi, [rbx+8]
0x23a5fd  ba 08 00 01 00         mov edx, 0x10008
0x23a604  e8 37 78 23 07         call memset                    ; CurrentBlock=0, CBO=0, Blocks[]=null
0x23a609  bf 00 00 02 00         mov edi, 0x20000
0x23a60e  be 02 00 00 00         mov esi, 2
0x23a613  e8 08 d4 e7 ff         call 0x4b7a20                  ; FMemory::Malloc(0x20000, 2)
0x23a625  48 89 43 10            mov [rbx+0x10], rax            ; Blocks[0] = result
0x23a618  4c 8d b3 40 00 01 00   lea r14, [rbx+0x10040]         ; 256 x 0x40 hash buckets
          (per bucket: mutex init call 0x78728e0; [b+8..+0x28]=0; [b+0x28]=0)
0x23a656  4c 8d bb 40 40 01 00   lea r15, [rbx+0x14040]
0x23a65d  ba fc 0a 00 00         mov edx, 0xafc                 ; memset(pool+0x14040, 0, 0xafc)
          (loop r14 = 0x1004c .. 0x1404c step 0x40: tag=0xff; aux = Malloc(0x400,4);
           memset(aux,0,0x400); [pool+r14+4]=aux; [pool+r14+0xC]=pool)
0x23a6fd  48 8d 35 13 c5 95 07   lea rsi, [0x7b96c17] "None"    ; edx=4
0x23a713  e8 98 19 00 00         call 0x23c0b0                  ; store -> id
0x23a727  89 83 40 40 01 00      mov [rbx+0x14040], eax         ; NoneId
0x23a718  ... "ByteProperty" (12) -> [rbx+0x14044] ... "IntProperty" -> +0x14048, "FloatProperty" -> +0x1404C, ...
```

### 2.4 Reserve @ 0x246090 — block growth

```
0x2460a8  8b 4b 08               mov ecx, [rbx+8]        ; CurrentBlock
0x2460ab  8b 5b 0c               mov ebx, [rbx+0xC]      ; CurrentByteOffset
0x2460ae  b8 00 00 02 00         mov eax, 0x20000        ; BLOCK SIZE
          r14d = ((len << wide) + 3) & ~1                       ; needed, 2-aligned
0x2460c3  29 d8                  sub eax, ebx            ; space left
0x2460cd  39 c2                  cmp edx, eax            ; needed-1 >= space?
0x2460cf  72 4e                  jb fit
   new-block:
0x2460d1  8d 43 02               lea eax, [rbx+2]
0x2460d4  3d 00 00 02 00         cmp eax, 0x20000
0x2460d9  77 0d                  ja skip_sliver
0x2460db  48 8b 44 cf 10         mov rax, [rdi+rcx*8+0x10]   ; Blocks[CurrentBlock]
0x2460e0  66 83 24 18 3f         and word [rax+rbx], 0x3f    ; sliver terminator (len=0)
0x2460e8  ff c1                  inc ecx
0x2460ea  89 4f 08               mov [rdi+8], ecx            ; CurrentBlock++
0x2460ed  c7 47 0c 00 00 00 00   mov dword [rdi+0xC], 0      ; CBO = 0
0x2460f4  48 8b 44 cf 10         mov rax, [rdi+rcx*8+0x10]   ; Blocks[CurrentBlock] (new)
0x2460fc  74 3e                  je alloc
   alloc:
0x24613f  bf 00 00 02 00         mov edi, 0x20000
0x246144  be 02 00 00 00         mov esi, 2
0x246149  e8 d2 18 e7 ff         call 0x4b7a20               ; FMemory::Malloc(0x20000, 2)
0x246154  48 89 44 cb 10         mov [rbx+rcx*8+0x10], rax   ; Blocks[CurrentBlock] = rax
   fit:
0x246128  44 89 77 0c            mov [rdi+0xC], r14d         ; CBO += needed  (CurrentBlock UNCHANGED)
```

### 2.5 Allocator identity

```
FMemory::Malloc @ 0xb7a20:
0xb7a2a  4c 8d 35 1f 8f 29 09   lea r14, [rip->0x9350950]   ; GMalloc slot (BSS)
0xb7a36  49 8b 3e               mov rdi, [r14]
0xb7a4c  48 8b 07               mov rax, [rdi]              ; vtable
0xb7a52  ff 50 30               call qword [rax+0x30]       ; slot 6 = Malloc(size, align)
```

Concrete allocator: static object **file 0x93089b8** (runtime 0x97089b8) built by ctor 0x4afd40 (vtable 0x7ec2e28 → 0x7ec26e0; 76 pool-table entries × 0x78 B with per-table mutex), pointer published to file 0x930add8 (`48 89 1d 71 f7 2f 09` @ 0xb660) and wrapped in a 0x28-byte thread-forwarder (vtable 0x7ec4030) only if inner `vtable+0xC0` returns false.

`Malloc` @ 0xb48e0 (vtable 0x7ec26e0 slot 6, via RELA addend):

```
0xb48ea  48 81 fe 01 00 02 00   cmp rsi, 0x20001        ; size limit 0x20000
0xb48f4  83 fa 11               cmp edx, 0x11           ; align limit 16
0xb491c  e8 0f dc 3b 07         call 0x7872530          ; pthread_getspecific(TLS @ file 0x92dc288)
0xb493f  4c 8d 05 ba 26 27 09   lea r8, [rip->0x9327000] ; size-class table (16-byte classes)
0xb4946  48 c1 e8 04            shr rax, 4
0xb499d  e8 be ca ff ff         call 0x4b1460           ; slow path (binned engine)
```

A 0x20000-byte request with align 2 takes the fast freelist or the binned slow path — OS-backed pool pages; no image-resident buffer exists in this chain.

---

## 3. Corrected facts vs inputs I was given

| Input assumption | Verdict |
|---|---|
| Pool label file 0x9365ec0 | **Correct** (I briefly "moved" it to 0x9369ec0 — that was a tool offset bug (file offset ≠ vaddr for LOAD0: delta +0x4000); retracted, re-verified). |
| Blocks at +0x10, CurrentBlock +0x8, CBO +0xC | Confirmed. |
| NoneId +0x14040 / BytePropertyId +0x14044 | Confirmed (init writes verified strings "None"/"ByteProperty" there). |
| "block size 0x20000*2 typically" | Actual immediate is **0x20000 bytes**. |
| "CurrentBlock >= 1" validator check | **Wrong semantics**: CurrentBlock is the 0-based index of the open block; 0 is valid and common. Engine predicate is `id.block <= CurrentBlock`. |
| launch-9: Blocks[0] pointed into heap-class [0x970c000,0x9a34000) | That range is LOAD7's BSS tail. Static evidence says OS heap. Unresolved — see §7. |

## 4. Recommended validator predicate set (replacing/patching SeedValidation.hpp)

1. Gate on the init flag byte `*(uint8*)(pool - 0x20)` (runtime 0x9765ea0): only validate pool shape after it is 1.
2. `Blocks[0] != 0`; readable; alignment ≥ 8 (16 in practice; do not enforce more).
3. Do **not** require "inside heap span X" or "outside image" — evidence is contradictory (§7). If a region check is kept, make it advisory.
4. Replace `current_block >= 1` with: for any entry id read, `id >> 16 <= CurrentBlock` and `Blocks[id>>16] != 0` (this is byte-for-byte the engine's own check @ 0x23d570).
5. `CurrentByteOffset < 0x20000`. Bounds for an entry: if `id>>16 == CurrentBlock`, require `offset*2 + needed <= CurrentByteOffset`; for older blocks the entry must be below the sliver terminator (u16 header with `(h & 0x3f) == h >> 6 == 0`... practically: header `hdr & 0x3f == 0` marks the tail).
6. Strong cheap identity anchor: entry 0 of Blocks[0] is `"None"` (ANSI): `u16 h = *(u16*)Blocks[0]; (h>>6)==4 && (h&1)==0 && memcmp(Blocks[0]+2, "None", 4)==0`.
7. Keep NoneId/BytePropertyId checks at +0x14040/+0x14044 — they are genuine and init-written.

## 5. Method notes (what the next worker should reuse)

* `tools/vmap.py` — authoritative phdr vaddr↔file-offset map (the +0x4000 trap).
* `tools/rdis.py` — recursive-descent capstone disassembler (immune to linear-sweep drift; used for all final claims).
* `tools/scan_correct_base.py`, `tools/scan_refs.py` — rip-relative/abs64/reloc reference scanners (pattern + disp32 arithmetic; ~0 expected false positives, but mid-instruction `e8` bytes DO create false call sites — callers.py hits must be instruction-verified).
* Raw extracts: `scan_refs.json`, `scan_range.json`, `scan_correct_base.json`, `plt_map.json`, `struct_check.json` + disasm listings (`fn_*.asm`, `noneid_site.asm` — note early ones carry the +0x4000 label offset; rdis output is correctly labeled).

## 6. Known tool pitfall found during this task (for the validator team)

`LOAD0` maps vaddr 0 at **file offset 0x4000** (offset = vaddr + 0x4000). Any tool that `seek()`s a file vaddr directly reads bytes 0x4000 into the segment and, worse, computes rip-targets shifted by +0x4000 — this produced the transient wrong-base artifact and the wrong "0x23d590 = thunk farm" first read. The retained launch-10 tooling should be audited for the same assumption.

## 7. Unresolved questions (not guessed, listed honestly)

1. **The launch-9 runtime placement of Blocks[0] inside the module BSS page range.** Static chain is unambiguous (GMalloc → binned, OS-backed). Possible resolutions: region-classifier merging BSS with adjacent heap; a different effective load base at observation time; approximate ranges ("~"). Needs one fresh runtime read of the qword at runtime 0x9765ed0 plus the region map.
2. **PS5 page source under binned slow path 0x4b1460** not traced to individual syscalls (dense code, boundary detection harder; not needed for the validator).
3. **Whether the thread-forwarder wrapper is installed at runtime** (depends on inner `vtable+0xC0`); forwarding-only, no effect on semantics.
4. **Role of the 256 aux buffers** at pool+0x1004C..0x1404C (0x400 B each) — mapped but not chased.
5. **Ghidra corroboration** — headless analysis was still running at delivery; all claims are Tier-1 capstone/lief. If it lands, decompile 0x23a5d0, 0x246090, 0xb48e0 and diff against §2.
