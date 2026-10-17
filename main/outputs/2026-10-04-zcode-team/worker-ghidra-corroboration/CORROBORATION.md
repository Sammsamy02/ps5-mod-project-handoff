# Ghidra corroboration of worker-re-fname-pool — townfall decrypted-eboot.bin

Worker: worker-ghidra-corroboration (RESTART completion) · 2026-10-05
Target: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (verified before use)
Ghidra: 12.1.4 headless, project `/tmp/wrfp_ghidra_proj/fnamepool` (program `decrypted-eboot.bin`, 330 MB db, import+analysis COMPLETE — prior attempt finished it; no re-import needed).

**Address convention: every address below is a FILE VADDR, identical to the RE worker's convention.**
Ghidra imported the ELF with imageBase=0 in the vaddr space (`mappingDelta=0x0`, signature-verified by the postScript), so **Ghidra addresses == RE file vaddrs** — the two toolchains are directly comparable with no offset translation.

Evidence set:
- `FnamepoolDecomp.java` — the postScript (this deliverable). Creates `CORROB_*` functions at the pinned vaddrs, dumps per-function asm + decompiled C into `decomp/`, plus `SUMMARY.txt` and `memory_blocks.txt`.
- `decomp/*.c`, `decomp/*.asm` — Ghidra decompiler/disassembler output for 10 functions (all completed; none truncated).
- `byte_checks.receipt.txt` — this worker's independent raw-byte verification of every RE-quoted instruction sequence, string constant, and call-target arithmetic (struct-parsed phdrs, no lief/capstone reuse).
- `tools_phdr_check.py` + `phdr_check.receipt.txt` + `phdr_load0_check.json` — the +0x4000 LOAD0 trap audit.

---

## 1. Bottom line

All three mission claims are **CONFIRMED** by Ghidra decompilation and independently re-confirmed at the byte level:

1. **FName ctor 0x23d590 → FindOrStore 0x23d600 → core 0x242470** — CONFIRMED, byte-for-byte.
2. **FNamePool::Init 0x23a5d0** (memset pool+8 size 0x10008; Malloc(0x20000,2) → pool+0x10; buckets +0x10040; None/ByteProperty interning +0x14040/+0x14044) — CONFIRMED, with one minor correction: the third/fourth interned slots are +0x14048 IntProperty, **+0x1404C BoolProperty, +0x14050 FloatProperty** (the RE doc's "+0x1404C FloatProperty" is off by one slot; both decomp and raw strings agree).
3. **CurrentBlock getter 0x23d570** (`id>>16 <= CurrentBlock`) — CONFIRMED, byte-for-byte.

No substantive REFUTED verdicts. Two **address-label artifacts** were found inside the RE worker's own quoted evidence (semantics unaffected, details in §5). The LOAD0 +0x4000 trap audit (§6) reproduces the RE doc's phdr claims exactly and classifies every pinned address; the two artifacts above are the only places where a stale shifted label survived into the RE outputs.

---

## 2. Claim 1 — FName ctor 0x23d590 → FindOrStore 0x23d600 → core 0x242470: CONFIRMED

### 2.1 Ctor 0x23d590 (`decomp/fname_ctor.c`, `fname_ctor.asm`)

Ghidra disassembly matches the RE doc's bytes exactly (independently re-read from the file):

```
0023d590  55 4889e5 5350      PUSH RBP; MOV RBP,RSP; PUSH RBX; PUSH RAX
0023d5a1  0fb738              MOVZX EDI,word [RAX]          ; wchar walk
0023d5b3  09f9                OR ECX,EDI                    ; OR-reduction of wchar units
0023d5c4  83f97f              CMP ECX,0x7f
0023d5c7  400f97c7            SETA DIL                      ; wide flag = any unit > 0x7f
0023d5cb  48c1e720            SHL RDI,0x20                  ; flag -> bit 32
0023d5db  bf01200000          MOV EDI,0x2001
0023d5e0  c4e2c0f7f6          BEXTR RSI,RSI,RDI             ; (byte_len >> 1) = wchar length
0023d5e8  4809ce              OR RSI,RCX                    ; len | wide<<32
0023d5eb  e810000000          CALL 0x0023d600               ; FindOrStore  (target verified)
0023d5f0  488903              MOV qword [RBX],RAX           ; *this = id | number<<32
```

Ghidra decompile: OR-reduction loop, `uVar5 = (0x7f < or) << 0x20`, `CORROB_find_or_store(param_2, len | wide<<32)`, `*param_1 = result`. CONFIRMED.

### 2.2 FindOrStore 0x23d600 (`decomp/find_or_store.c`)

Decompiled semantics, all matching the RE description:

- empty string (len 0) returns 0;
- non-wide path: SSE/AVX narrowing of wchars into a ~0x400-byte stack buffer, hard cap `0x400` (`uVar13 = 0x400; if (uVar11 < 0x400) …`) — the "0x400-wchar stack buffer (SSE path)" claim;
- wide path passes through with bit 32 set;
- `_NNN` suffix parsing: backward digit scan (`9 < *p-0x30` breaks), `_` (0x5f) required before digits, leading `0` (0x30) rejected, max 10 digits, `num = num*10 + digit`, requires `num < 0x7fffffff`; then `len -= digit_count`, `number = num + 1`;
- calls `CORROB_pool_core(narrowed, len [| wide<<32])`;
- returns `id | number << 32` (`uVar9 = uVar7 | uVar17 << 0x20`).

### 2.3 Core 0x242470 (`decomp/pool_core.c`, `find_or_store.asm` tail)

```
002424be  803ddb39120900      CMP byte [0x09365ea0],0x0     ; bInitialized flag (pool-0x20)
002424c7  488d05f2391209      LEA RAX,[0x9365ec0]           ; pool base (rip-target verified)
002424d7  e8f480ffff          CALL 0x0023a5d0               ; FNamePool::Init (target verified)
002424e2  c605b739120901      MOV byte [0x09365ea0],0x1     ; set flag
002424e9  83fa01              CMP EDX,0x1                   ; FNAME_Add?
002424fe  e8ad9bffff          CALL 0x0023c0b0               ; store_name (FNAME_Add path)
0024250d  480fbae320          BT RBX,0x20                   ; wide?
0024251c  e8df75ffff          CALL 0x00239b00               ; hash ANSI   (else 0x0023a2c0 wide)
00242548  488dbc0840000100    LEA RDI,[RAX + RCX*1 + 0x10040]  ; bucket = pool + hash*0x40 + 0x10040
00242550  e8eb9dffff          CALL 0x0023c340               ; bucket_find_insert (target verified)
```

Ghidra decompile renders the same bucket computation as `(hash & 0xffffffff) * 0x40 + 0x9375f00` — i.e. pool 0x9365ec0 + 0x10040, identical arithmetic. Length guard `(param_2 & 0xfffffc00) == 0` (len < 0x400) confirmed. All rip-relative targets independently recomputed from raw bytes and hit exactly 0x9365ec0 / 0x23a5d0 / 0x23c0b0 / 0x23c340. CONFIRMED.

---

## 3. Claim 2 — FNamePool::Init 0x23a5d0: CONFIRMED

(`decomp/pool_init.c` — 586-line decompilation, complete; `pool_init.asm`.)

### 3.1 memset(pool+8, 0, 0x10008)

```
0023a5f9  488d7b08            LEA RDI,[RBX + 0x8]
0023a5fd  ba08000100          MOV EDX,0x10008
0023a602  31f6                XOR ESI,ESI
0023a604  e837782307          CALL 0x07471e40               ; memset PLT (file vaddr; RE label 0x7871e40 = runtime form)
```

Decomp: `func_0x07471e40(param_1 + 8, 0, &UNK_00010008)`. Covers CurrentBlock(+8)=0, CurrentByteOffset(+0xC)=0, Blocks[0..8191](+0x10..+0x10040)=null. CONFIRMED.

### 3.2 Malloc(0x20000, 2) → Blocks[0] = pool+0x10

```
0023a609  bf00000200          MOV EDI,0x20000
0023a60e  be02000000          MOV ESI,0x2
0023a613  e808d4e7ff          CALL 0x000b7a20               ; FMemory::Malloc (file vaddr; RE label 0x4b7a20 = runtime form)
0023a625  48894310            MOV qword [RBX + 0x10],RAX    ; Blocks[0] = result
```

Decomp: `uVar6 = CORROB_fmemory_malloc(0x20000,2); *(undefined8 *)(param_1 + 0x10) = uVar6;`. CONFIRMED. (The e8 displacement arithmetic was independently recomputed: 0x23a618 − 0x182bf8 = 0xb7a20.)

### 3.3 Hash buckets at +0x10040

```
0023a618  4c8db340000100      LEA R14,[RBX + 0x10040]
0023a61f  41bf00400000        MOV R15D,0x4000               ; 0x4000 bytes total
0023a630    MOV RDI,R14; XOR ESI,ESI; CALL 0x074728e0       ; mutex init per bucket
0023a63e  c4c17c114608        VMOVUPS ymmword [R14+0x8],YMM0  ; [b+8..+0x28] = 0
0023a644  41c7462800000000    MOV dword [R14+0x28],0x0
0023a64c  4983c640            ADD R14,0x40                  ; stride 0x40 -> 256 buckets
```

Decomp loop: `puVar9 = &UNK_00010040 + param_1; … puVar9 += 0x40 … while (puVar9 != &UNK_0001404c)`. 0x4000/0x40 = 256 buckets. CONFIRMED (the RE's "256 x 0x40 hash buckets at +0x10040, per-bucket mutex, [b+8..+0x28]=0, [b+0x28]=0" is exact).

Aux entries: second loop from 0x1004c to 0x1404c step 0x40 — decomp:

```c
*(long *)(puVar9 + param_1 + 0xc) = param_1;            // bucket+0x18 = pool
uVar6 = CORROB_fmemory_malloc(0x400,4);
*(undefined8 *)(puVar9 + param_1 + 4) = uVar6;          // bucket+0x10 = buffer
func_0x07471e40(uVar6, 0, 0x400);                       // memset(buffer,0,0x400)
*(undefined4 *)(puVar9 + param_1) = 0xff;               // bucket+0x0c tag/mask = 0xff
```

matches the RE's mapped-but-unchased aux table. (Side finding: bucket+0xc doubles as the 0xff probe mask and bucket+0x10 is the Malloc(0x400,4) slot array — store_name reads them exactly that way.)

### 3.4 memset(pool+0x14040, 0, 0xafc) and name interning

```
0023a656  4c8dbb40400100      LEA R15,[RBX + 0x14040]
0023a65d  bafc0a0000          MOV EDX,0xafc
0023a667  e8d4772307          CALL 0x07471e40               ; memset
0023a6fd  488d3513c59507      LEA RSI,[0x07b96c17]          ; -> "None"
0023a713  e898190000          CALL 0x0023c0b0               ; store_name
0023a727  898340400100        MOV dword [RBX + 0x14040],EAX ; NoneId
```

Decomp interning sequence with string lengths (all string vaddrs re-read from the binary at corrected LOAD1 offsets):

| slot | string va | string (raw bytes) | len |
|---|---|---|---|
| +0x14040 | 0x7b96c17 | `None` | 4 |
| +0x14044 | 0x7abcd83 | `ByteProperty` | 0xc |
| +0x14048 | 0x7bfb4f8 | `IntProperty` | 0xb |
| +0x1404c | 0x7b12fe2 | `BoolProperty` | 0xc |
| +0x14050 | 0x7c184db | `FloatProperty` | 0xd |
| +0x14054 | 0x7a572f6 | `ObjectProperty` | 0xe |

None→+0x14040 and ByteProperty→+0x14044 (the offsets the validator uses) are CONFIRMED. **Correction:** the RE doc line `…"FloatProperty" -> +0x1404C` is wrong by one slot — +0x1404C receives the 12-char `BoolProperty`; FloatProperty (13 chars) lands at +0x14050. Validator impact: none (it only checks +0x14040/+0x14044).

Also confirmed (beyond the RE notes): init ends with bucket-chain consistency assertions (sums over the 256 buckets vs counters at pool+0x16b40..+0x16ba4) and a fatal-log path — irrelevant to the validator, but part of the function.

---

## 4. Claim 3 — CurrentBlock getter 0x23d570: CONFIRMED

(`decomp/current_block_getter.c`, `.asm`; every byte re-read from the file.)

```
0023d570  55 4889e5           PUSH RBP; MOV RBP,RSP
0023d574  8b054e891209        MOV EAX,dword [0x09365ec8]    ; CurrentBlock (pool+0x8); rip-target verified = 0x9365ec8
0023d57a  c1ef10              SHR EDI,0x10                  ; block = id >> 16
0023d57d  ffc0                INC EAX
0023d57f  39f8                CMP EAX,EDI                   ; CurrentBlock+1 vs block
0023d581  0f97c0              SETA AL                       ; al = block < CurrentBlock+1
0023d584  5d c3               POP RBP; RET
```

`SETA` after `CMP EAX,EDI` is true iff `(CurrentBlock+1) > (id>>16)` unsigned, i.e. **`id>>16 <= CurrentBlock`** — byte-for-byte the RE's claim and the exact predicate it recommends for the validator (`decomp` renders it `param_1 >> 0x10 < _DAT_09365ec8 + 1U`). CONFIRMED.

Context corroborations for this claim:
- **reserve 0x246090** (`decomp/reserve.c`): reads CurrentBlock `[pool+8]` / CBO `[pool+0xC]`; `needed = ((len << wide) + 3) & ~1`; new-block path `and word [rax+rbx],0x3f` sliver terminator, `inc ecx; mov [rdi+8],ecx; mov dword [rdi+0xC],0`, `Malloc(0x20000,2)` when `Blocks[CurrentBlock]==0`, store `[pool + cb*8 + 0x10]`; fit path stores only `[rdi+0xC] += needed` (CurrentBlock untouched) — confirming "CurrentBlock is a 0-based index of the open block, 0 is valid". Returns `block | (old_CBO>>1)<<32`. All RE-quoted bytes verified at the exact addresses.
- **fname_tostring 0x23f640** (`decomp/fname_tostring.c`): second lazy-init site (`DAT_09365ea0` → `CORROB_pool_init(0x9365ec0)`), `Blocks[id>>16]` at pool+0x10 stride 8, entry at `offset*2`, `len = hdr>>6`, `wide = hdr&1`, ` _%d` number suffix — independently confirming the layout the RE attributes to its "0x243240" getter (see §5a).

---

## 5. Address-label artifacts found in the RE worker's outputs (semantics unaffected)

Every RE-quoted **byte sequence** in scope was verified exact; the following quoted **addresses** are mislabeled. None changes a verdict, but downstream workers should not copy these labels.

**(a) FNAME-POOL-RE.md §2.1 quote block is +0x3C00 too high.** The seven quoted addresses 0x243261/0x243277/0x24327e/0x243281/0x243286/0x24328a/0x243297 do NOT contain the quoted bytes (checked). The identical byte sequences live at **0x23f661, 0x23f677, 0x23f67e, 0x23f681, 0x23f686, 0x23f68a, 0x23f697** — inside the function at **0x23f640** (which the postScript had flagged: "RE doc labels it +0x3C00"). Every rip-target from the corrected sites computes to exactly 0x9365ec0. The true function at 0x23f640 was decompiled by Ghidra and shows precisely the quoted semantics (lazy-init guard, `Blocks[id>>16]` at +0x10, entry `offset*2`, `hdr>>6` len, `hdr&1` wide). So the RE's layout evidence is real; its "getter @ 0x243240" label is not — 0x243240 contains unrelated code (`8b6f18 488b5dc0…`).

**(b) CLAIM.json pool-base sample site is +0x4000-shifted.** `"site TRUE 0x23d0a2 bytes 488d3d17ce1209"` — those bytes are actually at **0x2390a2** (0x23d0a2 − 0x4000); at 0x23d0a2 the file contains `743c4c8b…`. This is the documented early-tool +0x4000 artifact surviving into one sample line. The lea itself is genuine: `lea rdi,[rip+0x912ce17]` at 0x2390a2 → **0x9365ec0**, so the pool-base conclusion stands.

**(c) Runtime-form labels inside a file-vaddr document.** The RE doc declares "every address is a FILE VADDR" but labels the allocator/PLT targets in runtime form: `call 0x4b7a20` (file 0xb7a20), `memset PLT 0x7871e40` (file 0x7471e40), `pthread_getspecific 0x7872530` (file 0x7472530), `mutex_init 0x78728e0` (file 0x74728e0), `operator_new 0x7871e70` (file 0x7471e70). All call-target displacements were recomputed from raw bytes and match the file-vaddr forms; runtime = file + 0x400000 as stated. Labels only.

**(d) Minor tool-receipt notes.** `tools_phdr_check.py` (a) lists `fmemory_malloc: 0x4b7a20` (runtime) in its PINNED dict while the JSON output correctly used 0xb7a20; (b) its `current_block_getter` byte check compares the fn-start address against the +4 `mov eax` bytes — the receipt's "MISMATCH" line actually shows the correct prologue `554889e5 8b05…` (cosmetic). Its LOAD labels are list positions (LOAD0..LOAD4 = phdr idx 0,1,3,7,8) whereas the RE doc uses phdr indices (LOAD0/1/3/7/8) — same facts, different label convention.

---

## 6. +0x4000 LOAD0 trap audit

Recomputed independently with `struct` (no lief/capstone reuse): `tools_phdr_check.py` → `phdr_check.receipt.txt`, machine-readable `phdr_load0_check.json` (saved by the prior attempt, reproduced byte-identically on re-run).

- ELF type 0xFE10, 14 phdrs, 5 PT_LOADs at phdr indices **0, 1, 3, 7, 8**:

| phdr idx | flags | vaddr range | file offset | delta (off−va) |
|---|---|---|---|---|
| 0 | R+X | 0x0 – 0x7479b1c | 0x4000 | **+0x4000** |
| 1 | R | 0x747c000 – 0x7eba918 | 0x7480000 | **+0x4000** |
| 3 | RW | 0x7ebc000 – 0x913c000 | 0x7ec0000 | **+0x4000** |
| 7 | RW | 0x913c000 – 0x9633418 (BSS tail) | 0x9140000 | **+0x4000** |
| 8 | — | 0x9633420 – 0xbc602c8 | 0x930f420 | **−0x324000** |

So the RE header claim — "LOAD0/1/3/7 file offset = vaddr + 0x4000; LOAD8 offset = vaddr − 0x324000" — is CONFIRMED exactly. Four of five loads carry the +0x4000 trap; the fifth carries a different negative delta.

**Which loads carry the trap: all that contain any pinned address.** Every pinned address in the RE outputs lives in the +0x4000 loads (none in LOAD8). `phdr_load0_check.json` → `pinned_address_classification` maps all 34 pinned addresses to their load and correct file offset, e.g. pool_init 0x23a5d0 → file 0x23e5d0; pool_base 0x9365ec0 → file 0x9369ec0; memset_plt 0x7871e40 → file 0x7875e40. BSS classification also confirmed: pool 0x9365ec0, init flag 0x9365ea0, GMalloc slot 0x9350950, allocator object 0x93089b8, size-class table 0x9327000 are **memsz-only (no file bytes)** in phdr idx 7 — any tool reading file bytes "at" those vaddrs reads unrelated LOAD8 data (this is exactly the mechanism that produced the RE's transient 0x9369ec0 wrong-base artifact, since 0x9369ec0 is merely the *file offset* of the pool vaddr).

**Ghidra-side exposure: none.** The postScript signature check (`mov eax,[rip…]` bytes at 0x23d574) matched at delta 0x0 (`MAPPING: Ghidra addr = imageBase + file_vaddr + 0x0`; `memory_blocks.txt` shows segments at the vaddr boundaries incl. LOAD8 starting 0x9633420). All Ghidra/decomp outputs in this folder are in the same vaddr space as the RE claims.

**Affected pinned addresses in RE outputs:** only the two label artifacts in §5(a)/(b) — the §2.1 quote block (+0x3C00 labels) and the CLAIM.json 0x23d0a2 sample (+0x4000 label). All other quoted addresses were re-verified byte-exact at their stated vaddrs (see `byte_checks.receipt.txt`: 40+ OK lines covering the getter, ctor, core, init, reserve, malloc entry, malloc-impl fast path, and string constants).

---

## 7. Verdict summary table

| # | Claim (RE source) | Verdict |
|---|---|---|
| 1a | FName ctor 0x23d590: OR-reduce wchars, wide flag bit32, bextr len, call FindOrStore, `*this = id\|number<<32` | CONFIRMED |
| 1b | FindOrStore 0x23d600: 0x400-cap narrow/lowercase SSE path, `_NNN` suffix parse, calls core, returns id\|number<<32 | CONFIRMED |
| 1c | Core 0x242470: lazy init via flag byte pool−0x20 (0x9365ea0), init(0x9365ec0), FNAME_Add→0x23c0b0 else hash→bucket pool+hash*0x40+0x10040→0x23c340 | CONFIRMED |
| 2a | Init 0x23a5d0: memset(pool+8, 0, 0x10008) | CONFIRMED |
| 2b | Init: Malloc(0x20000, align 2) → Blocks[0] @ pool+0x10 | CONFIRMED |
| 2c | Init: 256 hash buckets @ +0x10040, stride 0x40, per-bucket mutex | CONFIRMED |
| 2d | Init: None → +0x14040 (len 4), ByteProperty → +0x14044 (len 12) | CONFIRMED |
| 2e | Init: memset(+0x14040, 0, 0xafc); aux Malloc(0x400,4) entries 0x1004c..0x1404c | CONFIRMED |
| 2f | "+0x1404C FloatProperty" | **REFUTED (minor):** +0x1404C = BoolProperty; FloatProperty = +0x14050. Validator-relevant offsets (+0x14040/+0x14044) unaffected |
| 3a | Getter 0x23d570 returns `id>>16 <= CurrentBlock` | CONFIRMED (byte-for-byte) |
| 3b | CurrentBlock semantics: 0-based open-block index; 0 valid after init (reserve fit path never touches it) | CONFIRMED |
| 3c | Reserve 0x246090: 0x20000 blocks, sliver `&0x3f`, CurrentBlock++/CBO=0, Malloc(0x20000,2) when next block null, CBO+=needed | CONFIRMED |
| 4a | FMemory::Malloc 0xb7a20: GMalloc slot 0x9350950, lazy creator, `call [vt+0x30]` | CONFIRMED |
| 4b | Malloc impl 0xb48e0 fast path: `cmp rsi,0x20001`, `cmp edx,0x11`, pthread_getspecific, size classes @0x9327000 (shr 4), slow 0x4b1460 | CONFIRMED (entry sequence; full binned internals out of scope, not contradicted) |
| 5 | Pool base 0x9365ec0; flag 0x9365ea0; CurrentBlock 0x9365ec8; pool in LOAD7-BSS (no file bytes) | CONFIRMED |
| 6 | phdr map: LOAD0/1/3/7 = vaddr+0x4000, LOAD8 = vaddr−0x324000 | CONFIRMED |
| 7 | RE §2.1 getter quote addresses (0x243261ff @ "0x243240") | **REFUTED as labels / CONFIRMED at −0x3C00:** true sites 0x23f661ff in fn 0x23f640; bytes+targets exact |
| 8 | CLAIM.json sample "site 0x23d0a2" | **REFUTED as label / CONFIRMED at 0x2390a2** (+0x4000 artifact); lea → 0x9365ec0 genuine |

## 8. Still unresolved (unchanged from the RE doc, not decidable statically here)

- The launch-9 runtime observation placing Blocks[0] inside the module-BSS page range vs the static GMalloc/binned chain. Static side now has Ghidra-grade corroboration; the runtime contradiction needs a fresh runtime sample of the qword at runtime 0x9765ed0.
- PS5 page source under binned slow path 0x4b1460 (not traced to syscalls; out of scope).
- Whether the thread-forwarder wrapper is installed at runtime.
- Deep binned-allocator internals (76×0x78 tables, vtable walks beyond the entry slot) were corroborated only at their entry points; not contradicted anywhere.
