# Agent AB2 — GMalloc address reconciliation (Claude review 3 open question, variant-A launch gate)

Date: 2026-10-08. READ-ONLY on all inputs; scratch scripts in /tmp/l25-ab2 only;
this report is the sole file written. Binary: `outputs/2026-10-04-zcode-team/
worker-game-version/console-decrypted-eboot.bin`, sha256 `3b6cfad47dd17fea…`
verified before analysis (the seed-table-pinned image). "File-image"/"vaddr" =
module-relative virtual address; runtime = vaddr + 0x400000 (proven §1).

## 0. VERDICT (headline)

**TWO DIFFERENT GLOBALS — and our seed binding is the RIGHT one.**

1. Our seed's `0x970add8` (file-image **0x930add8**) is the **primary,
   platform-installed allocator variable** — written exactly once image-wide by
   the game's own boot code (`FPlatformMemory::Init`-shaped factory 0xb3f0 →
   ctor 0xafd40 → the single store at 0xb660), holding the **static
   FMallocBinned3 instance at 0x93089b8 / runtime 0x97089b8** from birth until
   teardown. It is *not* a bootstrap allocator variable that later gets
   repointed — there is no repoint, static or reloc-based (§3, §4).
2. AE's "GMalloc global at file-image 0x9340950" is a **transcription ghost**:
   the instruction AE anchored on (the GMalloc load inside the engine
   `FMemory::Realloc` at 0xBEF20) actually targets file-image **0x9350950**
   (runtime 0x9750950). 0x9340950 has **zero references of any kind**
   image-wide (§5, §2). The task-premise arithmetic (0x9740950, delta
   0x35B78) inherits the typo; corrected: runtime 0x9750950, delta
   **0x45B78**.
3. 0x9350950 is the **FMemory-inline cluster's own copy**: it is *lazily
   seeded from OUR variable* (`[0x9350950] = *(0x930add8)` via the 12-byte
   accessor 0xc080, under mutex, on first FMemory call; optionally wrapped in
   a thread-safe proxy — a branch that is dead here because Binned3's
   `IsInternallyThreadSafe` (fn 0x120: `mov al,1; ret`) returns TRUE). Both
   variables therefore hold the **identical FMallocBinned3 singleton** in
   steady state (§4).
4. **GO for variant A** as far as this gate is concerned: the engine grows
   `GUObjectArray`'s listener TArray through 0x9350950's instance; our
   corrected-offset Append's Realloc/Free go through 0x930add8's instance —
   the same object. Cross-allocator block rejection is impossible on this
   axis. (Variant A still rests on the offset corrections and locking
   discussed elsewhere; the GMalloc-reconciliation blocker is cleared.)

## 1. Task 1 — the slide is +0x400000, constant across segments

ELF64 SCE_RELEXEC, 5 PT_LOADs (PH0 RX [0,0x7479b1c) @file 0x4000; PH1 R
[0x747c000,0x7eba918); PH3 RW-RELRO-ish [0x7ebc000,0x913a4e8); PH7 RW
[0x913c000,0x9633418), file-backed to 0x93085f2, BSS beyond; PH8 R
[0x9633420,…)). Anchors, each pairing a live-observed runtime address with a
statically-verified structure:

| Segment | Anchor | Runtime (observed) | File-image (static proof) | Slide |
|---|---|---|---|---|
| PH0 exec | klog crash frames (AH re-verified call edges at 0xb2379, 0xb4b75, 0xb175197, 0x1e3ad8, 0xf918) | base 0x400000 | same bytes at vaddr-0x400000 | +0x400000 |
| PH0 exec | Seeds slot 2 FName ctor 0x63d590 | 0x63d590 | 0x23d590 opens `55 48 89 e5…` (function prologue; behaviorally verified by the seed's ByteProperty probe) | +0x400000 |
| PH0 exec | Seeds slot 4 StaticConstructObject 0x890aa0 | 0x890aa0 | 0x490aa0 opens `55 48 89 e5 41 57 41 56…` (prologue) | +0x400000 |
| PH7 data | Seeds slot 0 GUObjectArray 0x97ba110 | 0x97ba110 | 0x93BA110 (AA's 1,565 xref "this" loads) | +0x400000 |
| PH7 data | Seeds slot 1 FName pool 0x9765ec0 | 0x9765ec0 | 0x9365EC0 (pool header validated live, Blocks[0] readback) | +0x400000 |
| PH7 data | Seeds slot 3 gmalloc 0x970add8 → 0x97089b8 | both | variable 0x930add8 + instance 0x93089b8 — **the variable's live VALUE equals the runtime address of the static instance found purely by static xrefs** (§3); this address+value self-consistency pins the PH7 slide independently | +0x400000 |
| PH3 data | vtable dispatch consistency | crash call entered 0x4b49b0 | RELA fills slot 0x7ec2720 with 0xb49b0; the vptr read at runtime pointed into PH3 with the same base offset | +0x400000 |

No counter-example exists in any prior agent's data. **Image vaddr + 0x400000
= runtime, uniformly.** (Claude's "+0x400000 if" conditional is confirmed.)

## 2. Task 2 — the arithmetic, corrected

- AE's cited file-image address for the engine GMalloc global: **0x9340950**
  — WRONG (transcription; §5 shows the real target of the exact instruction
  AE's chain lands on).
- Whole-exec scan for 0x9340950: **0** RIP-relative disp32 candidates, **0**
  imm64/imm32 absolutes, **0** RELA entries. The address names nothing.
- True FMemory-cluster GMalloc global: file-image **0x9350950** → runtime
  **0x9750950**.
- Our seed binding: runtime **0x970add8** = file-image **0x930add8**.
- Corrected delta: 0x9750950 − 0x970add8 = **0x45B78** (the question's
  0x35B78 came from the typo'd address).
- Answer: **two different BSS globals 0x45B78 apart in PH7-BSS** — but not
  rivals: one is seeded from the other (§4). Both are qword pointer
  variables; neither is file-initialized (both past the file-backed end
  0x93085f2; zero RELA entries target either — the loader writes neither).

## 3. Task 3a — binary ground truth at 0x930add8 (our binding) and 0x93089b8

Whole-exec RIP-relative disp32 xref scan (clang-built scanner, every candidate
capstone-verified) + imm32/imm64 absolute scan + full RELA walk:
**exactly 12 code references to 0x930add8 — one store, eleven loads — and
nothing else:**

```
0xb660: mov [rip+0x92ff771], rbx      # 0x930add8   ← THE ONLY STORE (install)
0xc084: mov rax, [rip+0x92fed4d]      # 0x930add8   ← accessor fn 0xc080: return *var
0xddc7/0xdde7/0xde0a/0xde27/0xde4d/0xde67/0xdebd/0xdf07/0xdf2c/0xdf4a:
       mov rdi/rax, [rip+…]           # 0x930add8   ← 10 loads, the shim family (below)
```

- **The install** (function 0xb3f0, a Prospero memory-init factory — it prints
  `"PRT va range %p - %p"` and `"Frame Buffer va range %p - %p"` and leads to
  the `"Used memory before allocating anything was %.2fMB"` log):
  ```
  0xb64f: lea  rbx, [rip+0x92fd362]   # 0x93089b8    ← &static instance (BSS)
  0xb656: xor  esi, esi
  0xb658: mov  rdi, rbx
  0xb65b: call 0xafd40                               ← FMallocBinned3 ctor
  0xb660: mov  [0x930add8], rbx                      ← install (once)
  ```
- **The ctor 0xafd40 (0xafd40–0xb042f, 418 insns) makes the instance a
  FMallocBinned3 from birth.** It performs exactly TWO vptr stores (full-function
  scan):
  ```
  0xafd77: lea rax, [rip+0x7e130aa]   # 0x7ec2e28   (inlined-base view, ANSI-import
  0xafd7e: mov [rdi], rax                           Malloc/Free slots — transient)
  …    init recursive pthread mutex at this+0x38 (attr type 4 = RECURSIVE) …
  0xafdc3: lea rax, [rip+0x7e12916]   # 0x7ec26e0   ← FINAL vtable ap (Binned3)
  0xafddc: mov [r13], rax                            (r13 = this; overwrites)
  …    zero-init + more mutex inits at this+0x74…0xd0+ …
  ```
  RELA ground truth for that vtable (ap **0x7ec26e0**, runtime vptr
  **0xBEC26E0**): +0x30=0xb48e0 Malloc, +0x38=0xb8a20 TryMalloc-def,
  **+0x40=0xb49b0 Realloc**, +0x48=0xb8a30 TryRealloc-def, +0x50=0xb4bf0 Free,
  +0x68=0xb4cd0 QuantizeSize, **+0xd0=0xb2810** (returns L"Binned3").
  This corrects AB's earlier audit, which read only the FIRST vptr store
  (0xafd77) and concluded the instance was a bootstrap "Unspecified allocator"
  later repointed to Binned3 at runtime — **no such repoint exists**: the
  second store wins before any caller can observe the object, 0xb660 installs
  it, and there is no other writer of 0x930add8 anywhere (static, immediate,
  or reloc). The "bootstrap→Binned3 repoint race" in AB §6 is moot.
- **The 11 loads** identify the consumers: the accessor 0xc080
  (`mov rax,[0x930add8]; ret`, 12 bytes) plus a family of tiny allocator
  shims at 0xddc0–0xdf4a that forward C-ABI calls into the CURRENT allocator
  with the TRUE slots — 0xddc0 `Free→[vptr+0x50]`, 0xdde0
  `Malloc(size,0)→[vptr+0x30]`, 0xde00 `Realloc(p,n,0)→[vptr+0x40]`, etc.
  0xddb0 is a one-line `call 0xb3f0; xor eax,eax; ret` (init shim). The
  accessor 0xc080 has **67 direct callers** across mid/high engine code —
  0x930add8 is the widely-consumed, authoritative allocator variable.

## 4. Task 3b/4 — what 0x9350950 is, and the aliasing (the reconciliation)

**0x9350950 (runtime 0x9750950)** is the variable the *FMemory inline cluster*
reads: **417 verified code references, every one an `lea reg,[rip+…]`
address-take** (no direct mov-loads, no direct stores, no immediates, no
RELA) — the cluster compiles as `lea r14,&var; mov rdi,[r14]; mov rax,[rdi];
call [rax+slot]`. Registered stores through those leas (register-tracked over
all 417 sites + full disassembly of the cluster): exactly **three**:

```
0xbebd5: mov [r13], rax     ; r13=&0x9350950 — SEED: rax = call 0xc080 = *(0x930add8)
0xbec5d: mov [r13], rbx     ; PROXY WRAP (only if ![vptr+0xc0]) — dead: 0x120 returns TRUE
0xc136e: mov [rax], 0       ; TEARDOWN null-out (frees tracked blocks first, [vptr+0x50])
```

The seeding lives in the lazy-init function 0xbeba0 (callers 0xb79b0 /
0xb79da / 0xb7a04 — the null-guard slow paths of the FMemory entries):

```
0xbebc2: call 0xc080                     ; rax = *(0x930add8)          [THE HANDOFF]
0xbebc7: lea  r13, [rip+0x9291d82]       # 0x9350950
0xbebd5: mov  [r13], rax                 ; engine-side var := OUR variable's value
0xbebd9: call 0xbbf0 ; … call 0x86ac0    ; stats/logging plumbing
0xbebf2: call [rax+0xc0]                 ; IsInternallyThreadSafe?
0xbebfa: jne  0xbec61                    ; Binned3 → fn 0x120 = {mov al,1; ret} → skip
   (0xbec01: call 0x7471e70 (new 0x28); vptr=0x7ec4030 proxy; 0xbec5d store — NOT TAKEN)
```

So the two-variable design is: **0x930add8 = the platform-primary install
variable (FPlatformMemory::Init at boot); 0x9350950 = the FMemory working
copy, lazily populated from the primary on first engine allocation and
wrapped/proxy-swapped only for non-thread-safe allocators.** For this title
(Binned3, internally thread-safe) both hold the **same instance 0x93089b8**
from the first FMemory call until teardown-null. Steady state:

```
*(0x970add8) = 0x97089b8 = FMallocBinned3 singleton   (installed 0xb660, never repointed)
*(0x9750950) = 0x97089b8 = the same object            (seeded 0xbebd5, proxy branch dead)
```

Runtime corroboration (evidence/l24-ue4ss*.log): `[Seeds] slot 3 gmalloc
bound at 0x970add8 -> 0x97089b8 (data; V1 instance + V2 vptr match)` and
`Post-initialization: GMalloc: 0x970add8 -> 0x97089b8` — stable across all
five l24 sessions; and the l24 crash dispatch *through our binding* provably
entered genuine `FMallocBinned3::Realloc` 0xb49b0 at the correct +0x40 slot
(Z11/AH stack + AH's slot analysis) — i.e. our binding demonstrably reached
the right allocator at the moment of the failure (whose actual cause was the
wrong TArray member offset feeding a garbage Original pointer, per AA §4 —
not the GMalloc binding).

**Seed provenance (task 4):** 0x970add8 was *derived offline* against the
sha-pinned image, not scanned at runtime: `outputs/2026-10-08-l24-package/
lineage/SeedTable.h` slot 3 `kGMalloc{… 0x930add8, "gmalloc", …}` documents
the derivation route "FMallocBinned3 UTF-16 literals → Malloc/Realloc/Free
validation roots → RELA-reconstructed vtable 0x7ec26e0 → ctor 0xafd40 →
factory 0xb3f0 constructs the GLOBAL instance at 0x93089b8 → store 0xb660
into 0x930add8 (ONE store image-wide)" with rip_arithmetic "runtime 0x970add8
= linked 0x930add8 + base". The port aims `GMalloc = bit_cast<FMalloc**>(
results.gmalloc)` at it (UnrealInitializer.cpp:848) and re-reads `*GMalloc` at
every call. I re-derived every link of that route independently above — it is
correct — and the runtime validator's "V1 instance + V2 vptr match" confirms
the live vptr equals RELA ap 0x7ec26e0 + base = 0xBEC26E0 (Binned3).

## 5. Task 5 — DECISIVE: the engine FMemory::Realloc disassembly

AE's chain verified link-by-link: `FUObjectArray::Add` @0x4841A0 (AA) → grow
helper 0x298a0 (`call 0x62fc0` at 0x2992d) → 0x62fc0 (TArray
ResizeAllocation; `call 0xbef20` at 0x62ff9) → **0xBEF20 = engine
`FMemory::Realloc`** (106 direct callers). Bytes (capstone):

```
0xbef20: push rbp; mov rbp,rsp; push r15; push r14; push r12; push rbx
0xbef2b: lea  r14, [rip + 0x9291a1e]        ; 4c 8d 35 1e 1a 29 09
         #   0xbef32 + 0x9291a1e = 0x9350950      ← THE GMalloc LOAD TARGET
0xbef32: mov  rbx, rdi                      ; Original
0xbef35: mov  ecx, edx                      ; Alignment
0xbef37: mov  rdx, rsi                      ; Count
0xbef3a: mov  rdi, qword ptr [r14]          ; rdi = *GMalloc   (this)
0xbef3d: test rdi, rdi
0xbef40: jne  0xbef50
0xbef42: movzx eax, byte [0x932c048]        ; init-state flag
0xbef4b: je   0xbef62                       ; → lock(0x7471eb0); call 0xbeba0 (lazy seed);
0xbef4d: mov  rdi, qword ptr [r14]          ;   unlock(0x7471ec0); re-read
0xbef50: mov  rax, qword ptr [rdi]          ; vptr
0xbef53: mov  rsi, rbx
0xbef56: call qword ptr [rax + 0x40]        ; ff 50 40  ← FMalloc::Realloc slot (RELA-verified)
0xbef61: ret
```

Displacement math, digit-exact: next-instruction address 0x B E F 3 2 +
disp 0x 9 2 9 1 A 1 E = 0x 9 3 5 0 9 5 0. The sibling entry 0xbefa0
(`FMemory::GetAllocationSize`-shaped, `call [rax+0x70]`) leas the same
0x9350950 at 0xbefb4 — the whole FMemory cluster names one variable:
**0x9350950**. AE's 0x9340950 appears nowhere; it was a report-side typo
(0x35B78-vs-0x45B78 and the "two addresses" tension both dissolve with it).

Which one does the ENGINE's Realloc load? — **0x9350950.** Which one does the
engine's boot code install and most subsystems read (accessor 0xc080, 67
callers + shims)? — **0x930add8.** And 0x9350950's sole non-teardown writer
copies 0x930add8's value. **Our binding is the primary; the engine's is a
cached alias of it.**

## 6. Task 6 — consequence for variant A

- Engine-side blocks (the listener TArray's buffer, grown by the engine's own
  Add/Remove via 0x62fc0→0xBEF20→[0x9350950]→Binned3) and port-side
  Realloc/Free of those blocks (via our 0x970add8→Binned3) hit **the same
  allocator instance with the same arena/hash ownership**. The
  "foreign-allocator unrecognized-pointer" failure mode cannot arise from the
  two-address split, because there is no split in instance terms.
- Timing guard already adequate: before 0xb3f0 runs, *(0x930add8) is null and
  the port's own null-check throws cleanly; after it, the value is stable for
  the process lifetime (no writer exists except teardown-null at 0xc136e, at
  engine shutdown — after UE4SS teardown).
- **GO: variant A may launch second with the existing seed binding
  0x970add8.** No re-bind is needed. (If a future title used a non-thread-safe
  allocator, the FMemory copy at runtime-base+0x9350950 would hold a proxy
  wrapper instead — a port would then want the FMemory-side variable; for
  this image that branch is provably dead.)

## 7. Corrections to prior reports (facts, not blame)

1. **AE (agent-AE-variant-c.md §3)**: "engine GMalloc global at file-image
   0x9340950" → actually **0x9350950** (typo; zero refs exist at 0x9340950).
   AE's chain addresses (0x298A0/0x62FC0/0xBEF20) and the [vtable+0x40]
   conclusion are all correct.
2. **AB (agent-AB-gmalloc-audit.md §0.2/§3)**: "0x97089b8 is a bootstrap
   instance (Unspecified allocator), repointed to Binned3 later at runtime" →
   wrong on both halves: the ctor's second vptr store (0xafddc, ap 0x7ec26e0)
   makes the instance Binned3 from birth, and no repoint path exists. §6's
   bootstrap→Binned3 TOCTOU window does not exist. (The audit's variable/store
   census, the 12-ref scan, and the vtable slot layout Malloc+0x30/Realloc+0x40
   were all confirmed correct.)
3. **AH (agent-AH §1.1)**: Binned3 vptr quoted as 0x7ec26e8/runtime 0xBEC26E8
   → RELA ground truth puts 0xb49b0 at 0x7ec2720 = ap+0x40, so ap =
   **0x7ec26e0**, runtime vptr **0xBEC26E0** (8-byte slip; slot assignments
   and all AH conclusions unaffected).
4. AB §0.4's "one-slot vtable shift" mis-dispatch theory was already
   superseded by AH/AU for l24 (staged table Realloc=0x40) — noting only
   because it concerned the same addresses; nothing in this reconcile
   supports it.

## 8. Method & provenance

- Inputs (read-only): the sha-pinned eboot; `outputs/2026-10-08-l25-prep/
  evidence/l24-ue4ss*.log` ([Seeds]/GMalloc lines); `outputs/2026-10-08-l24-
  package/lineage/SeedTable.h`; prior agent reports (AB, AE, AA, AH).
- Tools: hand-rolled ELF phdr/dynamic parser; clang-built whole-exec disp32
  candidate scanner (0x7479b1c bytes) with per-candidate capstone 5.0.7
  verification; capstone linear disassembly of the allocator cluster;
  register-tracking store finder over all 417 lea sites; direct call/jmp
  rel32 scanner; imm32/imm64 absolute scan; full 1,661,599-entry RELA walk
  (standard {r_offset, r_info, r_addend} layout, file 0x932f430). Scratch
  scripts under /tmp/l25-ab2/ (phdr.py, abdis.py, scan.c, verify.py,
  leastore.py, rela.py, callscan.c). No input file modified; this report is
  the only file written.
- Limitations: runtime stores through dynamically-computed pointers cannot
  be enumerated statically; the closed writer sets + zero relocs + live log
  stability + the crash dispatch all corroborate the static model. Exact UE
  source-level symbol names for the two globals are unknowable without
  symbols; the functional roles (platform-primary vs FMemory working copy)
  are established from behavior.
