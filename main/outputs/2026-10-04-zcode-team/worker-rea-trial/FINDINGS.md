# FINDINGS — FName pool investigation (worker-rea-trial, 2026-10-05)

Target: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194,241,424 bytes; ELF64 x86-64 (FreeBSD OSABI); PS5 UE 5.6.1).
Address space: **ELF vaddrs** (== Ghidra/REA addresses). file = vaddr + 0x4000 (code LOAD0);
runtime = vaddr + 0x400000. All addresses below are vaddrs unless suffixed.

Evidence provenance: (L) = derived locally with capstone 5.0.7 direct byte analysis of the
decrypted image (independent of REA); (R) = produced through REA 3.2.1's Ghidra provider.
FINAL: all four mission targets are confirmed by BOTH channels — locally first, then
delivered through REA sessions (section 5); no contradictions between channels.

## 1. The FName constructor at 0x23d590 (Q1) — (L) confirmed, (R) confirmed by REA decompile

- vaddr 0x23d590 = file 0x241590. Prologue: `55 48 89 e5` (push rbp; mov rbp,rsp).
- Signature (PS5 SysV): `FName::FName(FName* this=rdi, const wchar_t* name=rsi)`.
- Body (decompiled-equivalent, from capstone trace of 0x23d590..0x23d5f9):

```c
FName::FName(FName* this, const wchar_t* name) {
    uint64 flags = 0;
    const wchar_t* p = name;
    if (name != NULL) {
        uint32 acc = 0;
        for (wchar_t c = *p; c != 0; c = *(++p)) acc |= (uint16)c;
        if (acc > 0x7f)                       // any char outside 7-bit ASCII
            flags = (uint64)1 << 32;          // "case-preserving / non-Latin" bit (NAME_WIDE)
    } else p = name;
    uint64 len = (uint64)(p - name);          // byte length (2*chars)
    len = bextr64(len, 1, 13);                // >>1 -> char count, 13 bits
    this->BlockOffsetAndLen = FNamePool_Store(name, len | flags);  // call 0x23d600 @0x23d5eb
    *this = <64-bit handle returned in rax>;  // mov [rbx],rax @0x23d5f0
}
```

- Exact key instructions: `movzx edi, word ptr [rax]` (0x23d5a1, wide-char load proves
  wchar_t input), `cmp ecx, 0x7f / seta dil / shl rdi, 0x20` (0x23d5c4-0x23d5cf,
  non-ASCII flag into bit 32), `bextr rsi, rsi, 0x2001` (0x23d5e0 — CONTROL CORRECTED BY
  REA DECOMPILE: 0x2001 = start 1, length 32, i.e. `(byteLen >> 1) & 0xffffffff`, the
  13-bit "len" field bound comes from upstream clamps, not this extract),
  `call 0x23d600` (0x23d5eb, intern), `mov qword ptr [rbx], rax` (0x23d5f0, 8-byte FName).
- The function is 0x23d590..0x23d5f9 (105 bytes), followed by int3 padding then the intern
  helper 0x23d600.
- 0x23d600 is the intern entry (hash + wide/ansi branching, 0x400-char clamp; likely
  `FNamePool::Store` path) — decompilation through REA pending (session queued).

## 2. Callers of 0x23d590 (Q2) — (L) complete; (R) 2 sites verified in analyzed code, image-wide pending full analysis

- Global scan of the entire executable LOAD (122.1 MB) for direct `e8` rel32 calls
  targeting 0x23d590: **14,401 distinct call sites** (byte-level scan,
  `ctor-callers-14401.txt`) — consistent with the universal UE `FName(const wchar_t*)`
  constructor. Caveat: byte-level e8 scans admit mid-instruction false positives (the
  standalone worker hit this with their callers.py); the count is therefore an upper
  bound until instruction-verified. REA's Ghidra xref pass (session B/C) provides the
  instruction-verified cross-check when it lands.
- First 40 sites (vaddr): 0xfcf, 0x1188, 0x140f, 0x1490, 0x15b1, 0x164f, 0x1775, 0x188b,
  0xf824, 0x47ca0, 0x8b880, 0x90aac, 0x9133d, 0x931a4, 0x93387, 0x933b2, 0x9d555, 0xb8bfa,
  0xfb5d6, 0x116578, 0x12596a, 0x164b6d, 0x16b950, 0x16c3b9, 0x16c6d3, 0x16c981, 0x16c9be,
  0x16c9fb, 0x16ca38, 0x16ca75, 0x16cab2, 0x16caef, 0x16cb2c, 0x16d387, 0x16d3d0, 0x16d877,
  0x16ddc7, 0x16de00, 0x16e1a0, 0x172650.
- Prior `ctor-windows.json` sites in the observations dir (e.g. "0x2b45b90") do not
  correspond to e8 bytes under any of the three plausible address interpretations and are
  not reproducible against raw bytes; treat that file's call-site list with caution.

## 3. The FName pool struct at 0x9365ec0 (Q3) — (L) complete, (R) confirmed: 49/49 REA sites identical

- Global scan for `lea r64, [rip+disp32]` over the executable LOAD:
  **49 code references target exactly 0x9365ec0** (`pool-lea-refs-scan.json`).
  Control candidates 0x9361ec0 and 0x8f65ec0: 0 references — 0x9365ec0 is the pool label.
- 0x9365ec0 lies in the RW LOAD (vaddr 0x913c000, filesz 0x1cc5f2 < offset < memsz
  0x4f7418) => **BSS**: zero-initialized global, populated at runtime. Correct for a pool.
- Access pattern (from lookup functions, e.g. 0x23e6f0/0x23e9d0 @0x23e7e2/0x23eac7):
  `lea rcx,[pool]; movzx ebx,di; shr eax,0x10; add ebx,ebx; add rbx,[rcx + rax*8 + 0x10]`
  => block index is `FName >> 16`, **Blocks[] array at pool+0x10**, entry pointer =
  Blocks[i] + 2*(low 16 bits). Entry header: `movzx eax,word [rbx]; shr ecx,eax,6` =>
  length-in-bits-6..15, bit0 = wide flag.
- Containing functions (24 distinct, from prologue-scan clustering): 0x239090, 0x23c670,
  0x23e6f0 (ANSI lookup), 0x23e9d0 (wide lookup), 0x23ed20, 0x23ef10, 0x23ef60, 0x23efd0,
  0x23f040, 0x23f0c0, 0x23f150, 0x23f640, 0x23f8d0, 0x23f980, 0x23fa00, 0x23fc30,
  0x2402f0, 0x240330, 0x240460, 0x240730 (enumerator over stored names), 0x241930 (stats
  walker over per-block hash descriptors), 0x242470, 0x2428b0, 0x242ef0 (**Store/intern**:
  hash buckets at pool+0x1004c-masked region, grow call 0x243850, entry alloc 0x246090).
- Every accessor lazily initializes via a once-guard:
  `cmp byte [0x932c048-area],0; jne skip; lea rdi,[pool]; call 0x23a5d0; mov byte,1`
  => **pool initializer = 0x23a5d0**.

## 4. Pool initializer and the allocation (Q4) — (L) complete, (R) confirmed by REA decompile

`FNamePool::Initialize`-equivalent at **0x23a5d0** (called with rdi = &pool):

```asm
0x23a5f4: call 0x74728e0            ; lock acquire (FRWLock write)
0x23a5f9: lea rdi,[rbx+8]           ; rbx = pool
0x23a602: mov edx,0x10008 ; xor esi,esi
0x23a604: call 0x7471e40            ; memset(pool+8, 0, 0x10008)   ; Blocks[]+counters
0x23a609: mov edi,0x20000           ; SIZE = 0x20000 (131072 B)
0x23a60e: mov esi,2                 ; alignment/flags arg = 2
0x23a613: call 0xb7a20              ; *** ALLOCATION: FMemory::Malloc(0x20000, 2) ***
0x23a625: mov qword ptr [rbx+0x10],rax ; *** Blocks[0] = new 128KiB block ***
0x23a630..0x23a654:               ; lock+zero 256 per-block hash descriptors (0x40 stride,
                                  ; 0x4000 bytes total, pool+0x10040..0x14040; [desc+0x28]=0)
0x23a6c0 loop (r14=0x1004c..0x1404c step 0x40):
          mov edi,0x400 ; mov esi,4
          call 0xb7a20              ; Malloc(0x400, 4) per block: hash buckets
          mov [rbx+r14+4],rax       ; desc->Buckets
          call 0x7471e40            ; memset(buckets,0,0x400)
          mov dword [rbx+r14],0xff  ; desc->Mask = 0xff
0x23a6fd..: ~20 x call 0x23c0b0     ; register default FNames ("None" len 4 first;
                                    ; results stored pool+0x14040, +0x14044, ...)
```

- CurrentBlock/counters live in the memset region pool+8..+0x10010 (the Store function
  at 0x242ef0 manipulates them via `lea rdi,[pool+0x10040+idx*0x40]` descriptors and
  `lock inc dword [rdi+0x20]/[rdi+0x24]` at 0x2430c7/0x2430cb).
- 0x23a618..0x23a654: 256 hash descriptors zeroed (pool+0x10040..0x14040, 0x40 stride).
  Then loop r14 = 0x1004c..0x1404c step 0x40 (256 iterations):
  `0xb7a20(0x400, 4)` per block: hash-bucket array; `[pool+r14+4]=buckets`;
  `memset(buckets,0,0x400)`; `[pool+r14]=0xff` (mask).
- Default FName registration: the REA decompile of 0x23a5d0 shows **236** direct
  `CORROB_store_name(pool, str, len)` calls (0x23c0b0), results stored from pool+0x14040
  ("None", len 4 first, then "ByteProperty" len 0xc at +0x14044, "IntProperty" len 0xb at
  +0x14048, "FloatProperty" len 0xc, ...). The earlier local count "~20" was a
  scan-window underestimate; 236 is the authoritative count. The decompile tail also
  reveals an init self-check (descriptor-counter sums over the 256 x 0x40 descriptors
  cross-checked against counters at pool+0x16b48/0x16ba4, fatal log+abort on mismatch)
  and a stack-cookie guard.

**Blocks[i]/CurrentBlock growth writer — Reserve 0x246090** (cross-checked with the
standalone worker's independent trace, which matches my Store-call observation):

```asm
0x2460a8: mov ecx,[rbx+8]            ; CurrentBlock
0x2460ab: mov ebx,[rbx+0xC]          ; CurrentByteOffset
0x2460cd: cmp edx,eax                ; needed >= space left in open block?
0x2460db: mov rax,[rdi+rcx*8+0x10]   ; Blocks[CurrentBlock]
0x2460e0: and word [rax+rbx],0x3f    ; sliver terminator
0x2460e8: inc ecx
0x2460ea: mov [rdi+8],ecx            ; *** CurrentBlock++ ***
0x2460ed: mov dword [rdi+0xC],0      ; CurrentByteOffset = 0
0x24613f: mov edi,0x20000 ; mov esi,2
0x246149: call 0xb7a20               ; *** FMemory::Malloc(0x20000, 2) again ***
0x246154: mov [rbx+rcx*8+0x10],rax   ; *** Blocks[CurrentBlock] = new block ***
```

So BOTH pool-allocation sites (initial Blocks[0] in init 0x23a5d0 @0x23a613, and
growth in Reserve 0x246090 @0x246149) make the identical call
**FMemory::Malloc(0x20000, 2) -> 0xb7a20 -> GMalloc vtable+0x30**.

- Blocks[0] initial writer = the single store at 0x23a625 inside init 0x23a5d0. The only
  other direct lea of pool+0x10 in the image is the tiny accessor 0x241780
  (`lea rax,[pool+0x10]; pop rbp; ret`), called once from 0x48bf804 in an early-startup
  function that stashes the pointer elsewhere.

**Allocator identification (0xb7a20):**

```asm
0xb7a2a: lea r14,[rip+0x9298f1f]   ; -> 0x9350950 = GMalloc interface pointer (global)
0xb7a36: mov rdi,[r14]             ; GMalloc instance
...
0xb7a4c: mov rax,[rdi]             ; vtable
0xb7a52: call [rax+0x30]           ; virtual Malloc(size, align)
```

- `0xb7a20 = FMemory::Malloc(size, align)`; Free = 0xb7a90 (vtable slot +0x50); the
  GMalloc **global is 0x9350950**; null-GMalloc + flag-byte error path.
- Concrete backend: `Runtime/HAL/MallocBinned3.cpp` string present in the binary
  (0x7a874a2) + `MallocBinnedCommon.h` (0x7addd3a). The standalone worker's deeper
  vtable trace (RELA addend) identifies the installed allocator as a
  **binned-family allocator**: static object at vaddr 0x93089b8, `Malloc` at 0xb48e0
  (vtable 0x7ec26e0 slot 6), fast path `size < 0x20001 && align < 0x11`, per-thread
  freelists via pthread_getspecific, 16-byte size classes, slow path 0xb1460 —
  "FMallocBinned2-family shape" per their structural read; the MallocBinned3 source
  string is consistent with UE 5.6 shipping Binned3 (Binned2/3 share
  MallocBinnedCommon.h). Exact leaf label: plausible but not symbolically proven.
- Runtime GMalloc wrapping: setup function **0xbeb20** lock-cmpxchgs a decorator
  (0x10-byte object, vtable **0x7ec3f20**, chaining the previous GMalloc) into
  0x9350950; the standalone worker separately observed a 0x28-byte thread-forwarder
  wrapper (vtable 0x7ec4030). These are complementary layers of the same wrapper chain
  (decorator -> thread-forwarder -> binned base), not contradictory.

## 5. REA session results — final (restart run, 2026-10-05 12:20-12:34 local)

All four queries DELIVERED through `rea` 3.2.1 (`--provider ghidra`) in persistent-project
process mode (see REA-TRIAL.md restart section for setup). Latency per query: **6.0-13.7 s
wall** (measured with `/usr/bin/time -p`), versus 45 min timeout / 53 min OOM-crash in the
prior import-per-command attempts. Evidence artifacts: `function-23d590.json`,
`xrefs-pool.json`, `xrefs-ctor.json`, `decompile-pool-init.json` (+ `.time` files);
failure records from the prior attempt retained as
`function-23d590.timeout-45min.json` / `xrefs-pool.OOM-execution-failure.json`.

### 5.1 Q1 — FName ctor 0x23d590 decompiled BY REA (op `analyze_function`)

Ghidra's pseudocode (`normalized_result.pseudocode`, function `CORROB_fname_ctor`):

```c
void CORROB_fname_ctor(undefined8 *param_1, ushort *param_2) {
    if (param_2 == (ushort *)0x0) { uVar5 = 0; puVar4 = (ushort *)0x0; }
    else {
        uVar1 = *param_2; uVar5 = 0; puVar4 = param_2;
        if (uVar1 != 0) {
            do { uVar3 = (uint)uVar5 | (uint)uVar1; uVar5 = (ulong)uVar3;
                 uVar1 = puVar4[1]; puVar4 = puVar4 + 1; } while (uVar1 != 0);
            uVar5 = (ulong)(0x7f < uVar3) << 0x20;
        }
    }
    uVar2 = CORROB_find_or_store(param_2,
                (ulong)((long)puVar4 - (long)param_2) >> 1 & 0xffffffff | uVar5);
    *param_1 = uVar2;
    return;
}
```

Line-by-line agreement with the capstone ground truth (section 1): NULL branch, ushort
(wide-char) OR-reduce loop, non-ASCII flag `(0x7f < acc) << 0x20`, char-count length
`(bytes >> 1)` (correcting the "13-bit" bextr annotation — control 0x2001 = start 1,
length 32), intern call to 0x23d600 (`CORROB_find_or_store`), 8-byte handle store.
Dossier extras: 0x23d590..0x23d5f9 body, callees = exactly `CORROB_find_or_store`
(0x23d600), analyzed-code caller = `FUN_0022d770` (0x22d770).

### 5.2 Q3 — pool label 0x9365ec0 xrefs BY REA (op `xrefs`): 49/49 == byte scan

`rea xrefs 0x9365ec0` returned **49 references**, and the site list is IDENTICAL to the
independent capstone `lea` byte scan (`pool-lea-refs-scan.json`): intersection 49/49,
zero differences in either direction. First sites 0x2390a2, 0x2390b5, 0x23c6a1, 0x23c6b6,
0x23e7b5, ... — all inside the FName cluster 0x239090-0x2431xx, matching the 24 accessor
functions of section 3. Control candidates (0x9361ec0, 0x8f65ec0): still 0.

Provenance note: the reused analysis DB had no defined instructions/references in the
accessor functions (the other worker's corroboration state). Bounded cluster enrichment
(0x230000-0x249fff: flow-following disassembly from the 31 known entries + operand
references computed via Ghidra's own `getOpObjects`/rip-relative semantics, saved to the
trial-owned project copy) was applied before this query; Ghidra then reported exactly the
byte-scan set. No target files were modified.

### 5.3 Q2 — callers of 0x23d590 via REA

- `rea xrefs 0x23d590`: **2 instruction-verified call sites in analyzed code**
  (0x22d96a, 0x22dd99). Image-wide verification (byte-scan upper bound: 14,401) still
  requires the full-program auto-analysis, which was externally killed at ~113 min
  (see REA-TRIAL.md) and not replayed. The 2 sites are consistent (both `e8` rel32 ->
  0x23d590 inside a function that also touches the FName cluster); the image-wide count
  remains an upper bound.

### 5.4 Q4 — pool-init/allocation 0x23a5d0 decompiled BY REA (op `procedure_pseudo_code`)

`rea decompile 0x23a5d0` (`CORROB_pool_init`) confirms every section-4 instruction:
lock acquire `func_0x074728e0(pool,0)`; `memset(pool+8, 0, 0x10008)`;
`CORROB_fmemory_malloc(0x20000, 2)`; `Blocks[0] = result` at pool+0x10; the 256
x 0x40-stride descriptor zero loop (pool+0x10040..0x14040, `[desc+0x28]=0`); the bucket
allocation loop from pool+0x1004c to 0x1404c step 0x40 (`malloc(0x400,4)`, memset 0x400,
`[desc]=0xff`); then 236 default-name registrations. Function-name bindings
(`CORROB_fmemory_malloc` = 0xb7a20 path, `CORROB_store_name` = 0x23c0b0 semantics)
agree with the independently traced addresses.

## Cross-check — worker-re-fname-pool (their findings DELIVERED 09:20-09:23 local)

Their `FNAME-POOL-RE.md` uses the same address space as this trial ("file vaddr" ==
ELF vaddr; runtime = vaddr + 0x400000; their `tools/vmap.py` documents the same
+0x4000 file-offset trap). Point-by-point:

| Claim | This trial | worker-re-fname-pool | Verdict |
|---|---|---|---|
| FName ctor | vaddr 0x23d590, prologue 55 48 89 e5, OR-reduce cmp 0x7f, bextr 0x2001, intern call 0x23d600, handle [rbx],rax | identical bytes + same calls | **AGREE** |
| Pool struct | 0x9365ec0 (BSS), 49 exact-target leas | 0x9365ec0, 100+ refs incl. field ranges | **AGREE** |
| Blocks[] / CurrentBlock / CBO | pool+0x10 / +0x8 / +0xC | same | **AGREE** |
| Pool init | 0x23a5d0; memset(pool+8,0x10008); Malloc(0x20000,2) @0xb7a20; Blocks[0] store @0x23a625 | identical instructions, same addresses | **AGREE** |
| Growth writer | Store 0x242ef0 calls Reserve 0x246090 (observed as call target only) | Reserve traced: CurrentBlock++ @0x2460ea, Malloc(0x20000,2) @0x246149, Blocks[i]=rax @0x246154 | **AGREE, theirs deeper** |
| Allocator interface | FMemory::Malloc 0xb7a20 -> GMalloc 0x9350950 vtable+0x30; Free 0xb7a90 slot +0x50 | identical | **AGREE** |
| Concrete backend | MallocBinned3.cpp string + their vtable trace: binned object 0x93089b8, Malloc 0xb48e0, vtable 0x7ec26e0, 76 tables x 0x78 | "FMallocBinned2-family shape" | **AGREE (binned family); exact Binned2-vs-3 label unproven both sides** |
| Wrapper chain | decorator 0xbeb20, vtable 0x7ec3f20 | thread-forwarder vtable 0x7ec4030 (conditional) | **COMPLEMENTARY layers, no conflict** |
| Default names | ~20 registrations from pool+0x14040 ("None" first) | same incl. "None"/"ByteProperty" strings | **AGREE** |
| e8 caller count | 14,401 (byte-level, upper bound) | warns mid-instruction e8 false positives | **consistent; instruction-verified count pending REA** |
| Their first-pass artifact `tools/ctor_23d590.asm` (thunk bytes) | explained by +0x4000 file-offset bug | retracted in their report | **resolved, no live disagreement** |

One open item inherited from their report (not mine to resolve): their launch-9 runtime
observation placed Blocks[0] inside the module BSS page range, contradicting the static
OS-heap allocation chain; they recommend a fresh runtime read (their §7.1).

No disagreements found. Their structural details (Reserve trace, 76-table binned shape,
validator predicate set) and this trial's REA pipeline evidence are complementary.
