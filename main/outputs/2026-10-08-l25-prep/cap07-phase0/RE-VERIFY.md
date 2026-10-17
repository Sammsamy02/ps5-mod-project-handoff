# CAP-07 PHASE-0 — independent static re-verification of the pak-launch facts

Static analysis only (Mac-only, read-only). No console, no network, no builds,
no edits to any other file. Independent re-derivation with our own script
`cap07-phase0/re_verify.py` (capstone 5.0.7; own phdr map + RELA reconstruction
+ byte-scan xref/callers + annotated disasm) — the old `pak-pin-pass-pins.json`
was NOT trusted; every fact below was recomputed from the image bytes.

## 0. Image basis (verified)

- Image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
  194,241,424 B — hash re-verified by the script (fail-closed `REFUSAL` if
  mismatch). Matches the PAK-PIN-PASS canonical image.
- `file offset = vaddr + 0x4000` verified for ph0/ph1/ph3. DT_RELA at
  `0x9653430` size `0x2607ee8`; ph8 fileoff = vaddr − 0x324000. Reconstructed
  **1,649,556** `R_X86_64_RELATIVE` entries (PAK-PIN-PASS reports the same
  count — AGREE). All vtable pointer words are zero on disk; values are the
  RELA addends.
- All vaddrs are IMAGE vaddrs. UE 5.6.1. RTTI off (offset-to-top/typeinfo
  words are plain zero with no reloc — AGREE with prior pass).

Verdict labels: every line is **console-proven = NO** (static/Mac-only).
Agreement with `PAK-PIN-PASS.md` is marked AGREE / DISAGREE / EXTENDS.

---

## 1. Chain head: FPlatformFileManager::Get() and who builds the chain

### Accessors (all re-disassembled)

**`Get()` @0xb8ed0** — bytes `0f b6 05 61 15 27 09 84 c0 74 …`
```
0xb8ed0: movzx eax, byte [rip+0x9271561]   ; guard byte 0x932a438
0xb8ed9: je   0xb8ee3
0xb8edb: lea  rax, [0x932a428]             ; returns &singleton
0xb8ee2: ret
   lazy: lea rdi,[0x932a438]; call 0x7471eb0 (__cxa_guard_acquire);
         mov qword [0x932a428], 0  (@0xb8efe); call 0x7471ec0 (guard_release);
         lea rax,[0x932a428]; ret
```
AGREE with PAK-PIN-PASS §1 exactly (guard 0x932a438, singleton 0x932a428,
returns &0x932a428, guard PLTs 0x7471eb0/0x7471ec0).

**`GetPlatformFile()` @0xb8a70** — bytes `48 8b 07 48 85 c0 74 01 c3 55`
```
mov rax,[rdi]; test rax,rax; jne ret; (else) call 0x4ca0; mov [rbx],rax; ret
```
Reads `[mgr+0]`; on null lazily builds via **0x4ca0** and stores back to
`[mgr+0]`. AGREE.

**Second accessor @0xb8aa0** — bytes `48 8b 47 08 48 85 c0 74 01 c3`
```
mov rax,[rdi+8]; test; jne ret; (else) call 0x4ca0; mov [rbx+8],rax (@0xb8ab8); ret
```
Reads `[mgr+8]`; on null builds via **0x4ca0** and stores to `[mgr+8]`.
Only **2** callers (0x8ac8b, 0x177d9a3). AGREE on address/shape.

**Lazy default builder `0x4ca0`** — bytes `0f b6 05 69 3b 30 09 84 c0 74`
Guard byte `0x9308810`; builds the **default physical instance `0x9308808`**
(ctor 0x5b80 @0x4cd3; `__cxa_atexit` 0x7471ed0 with dtor 0x6cd0); returns
`&0x9308808`. Physical class vtable is **0x7ebc6a0** (see §2). AGREE
(0x9308808, guard 0x9308810, ctor 0x5b80).

### Guard byte / singleton segment
`0x932a428` and guard `0x932a438` are in ph7 (BSS) — zero on disk, no reloc.
AGREE.

### Who STORES `[0x932a428+0]` (SetPlatformFile) and `[+8]`

Byte-scan for every rip-relative reference to `0x932a428` found **only 6**
(4 `lea`, 2 `movimm`), **all inside `Get()` itself** — i.e. the only direct
address-taking of the singleton is `Get()`'s own return LEAs and its lazy
`mov qword [0x932a428],0`. **Therefore SetPlatformFile does NOT touch the
singleton by rip-relative; it receives the manager pointer in RDI from a prior
`Get()` and stores through the register.** Tracing all **157** `Get()` call
sites and classifying what each does with the returned RAX pointer isolates the
real chain-mutators:

| Writer | vaddr | shape | role |
|---|---|---|---|
| **SetPlatformFile** | **0xb8ad0** | `mov [rdi],rsi; mov rdi,rsi; mov rax,[rsi]; call [rax+0x30]; ret` (bytes `55 48 89 e5 48 89 37 …`) | **writes `[mgr+0]`** (new chain top), then calls the new file's vtable **+0x30** (InitializeAfterSetActive-class) |
| **RemovePlatformFile** | **0xb8d60** | walks chain via +0x68, splices out rsi; top-removal branch does `mov [r14(=mgr)],rax` from rsi→GetLowerLevel(+0x68) then new-top `[+0x30]` | **writes `[mgr+0]`** only when removing the current top |

**Callers of SetPlatformFile 0xb8ad0: exactly 4**, all in the function whose
body carries the string `.\Runtime/Launch/Private/LaunchEngineLoop` (=
**FEngineLoop::PreInit / LaunchCheckForFileOverride**): sites 0x3a142c7,
0x3a1431d, 0x3a14373, 0x3a143c9. **Callers of RemovePlatformFile 0xb8d60:
exactly 1** — the pak module teardown fn 0x178ab32 (see below).

**Writer of `[mgr+8]`: ONLY the accessor 0xb8aa0's own lazy init** (stores the
**default physical file 0x9308808** via 0x4ca0). No other code writes `[mgr+8]`.

> **DISAGREE / correction vs PAK-PIN-PASS §1 and §7.2.** The prior pass left
> open the hypothesis that `[GPlatformFileManager+8]` is "the engine-wide
> FPakPlatformFile holder." Statically that is **wrong**: `[mgr+8]` is lazily
> initialized to the **physical/default** platform file (0x4ca0→0x9308808,
> physical vtable 0x7ebc6a0), i.e. it behaves as a cached
> GetLowestPlatformFile/physical pointer, not a pak holder. Also, the "pak
> factory installs its FPakPlatformFile into an `[arg+8]` slot" observation
> refers to the **pak module object's** own `+8` member (`[rbx+8]` at
> 0x178ab40/0x178ab56), NOT `[mgr+8]`. The A1 recipe (walk from `[mgr+0]`
> following `+0x68` GetLowerLevel and match vptrs) is unaffected and remains
> the correct discovery route.

### Pak module teardown fn 0x178ab32 (clarifies the `[obj+8]` seam)
```
cmp [rdi+8],0; je done          ; rdi=pak module, [rdi+8]=its FPakPlatformFile
Get()->r14; rdi=[rbx+8]; [pak]->GetTypeName(+0x78); rdi=mgr; call 0xb8af0(FindByType)
if present: Get()->rax; rsi=[rbx+8]; call 0xb8d60 (RemovePlatformFile)
```
So this is the shutdown path (unlink the pak layer if it is in the chain). The
install counterpart is the FEngineLoop wrapper loop (§4).

---

## 2. FPakPlatformFile vtable 0x7fee8c0 — all 74 slots reconstructed

Vtable base (vptr value) = **0x7fee8c0**. Confirmed by the complete dtor's
self-restore: dtor 0x17652f0 does `lea rcx,[0x7fee8c0]` @0x1765308
(bytes `48 8d 0d b1 95 88 06 …`) and `rela_at(0x7fee8c0)=0x17652f0`. Header
words (−0x10/−0x8) are zero (RTTI off). 72 non-null slots +0x00..+0x238, then
NULL at +0x240/+0x248 (ends before FPakFile vtable 0x7feeb10). AGREE with the
prior pass's layout.

Full RELA-reconstructed impl list (offset : impl) — all AGREE with
PAK-PIN-PASS §2 where it listed values:

```
+0x00 0x17652f0 (complete dtor)   +0x08 0x17656a0 (deleting dtor)
+0x10 0xb290  (stub: ret)         +0x18 0xe0 (stub: xor eax; ret -> false)
+0x20 0x1769df0 (ShouldBeUsed)    +0x28 0x1769ee0 (Initialize, assert str)
+0x30 0xb290  (stub)              +0x38 0xb290 (stub)
+0x40 0x1771240 (real, struct arg) +0x48 0xb290 (stub)
+0x50 0x25340  +0x58 0x25380  (ICF-folded inner dispatch, ->[inner+0x68])
+0x60 0xb290 (stub)
+0x68 0xb3500 (GetLowerLevel: mov rax,[rdi+8]; ret)
+0x70 0xb3510 (SetLowerLevel: mov [rdi+8],rsi; ret)
+0x78 0x17643d0 (GetTypeName -> L"PakFile")
+0x80 0x177c2c0 (FileExists)      +0x88 0x177c360 (FileSize)
+0x90 0x177c460  +0x98 0x253d0
+0xa0 0x177c500  +0xa8 0x177c5a0  +0xb0 0x177c650
+0xb8 0x177c6f0 (OpenRead)
+0xc0 0x177c7d0  +0xc8 0x177c870  +0xd0 0x177c950
+0xd8 0xe3240 (fwd ->inner+0xd8)  +0xe0 0xe0  +0xe8 0xe0
+0xf0 0x84cb0 (fwd [r8]->+0xf8)   +0xf8 0x1770bb0  +0x100 0x25450 (fwd->+0xf8)
+0x108 0x1770e40  +0x110 0x1769800  +0x118 0xe3ab0 (fwd ->inner+0x118)
+0x120 0x1769830  +0x128 0x1769870  +0x130 0x1768fc0  +0x138 0x1766a70
+0x140 0x86600  +0x148 0x86600  +0x150 0x86630  +0x158 0x86670 (fwd->+0x1b8)
+0x160 0x866d0  +0x168 0x86760  +0x170 0x86790  +0x178 0x84d50 (fwd->+0x180)
+0x180 0x1765e50  +0x188 0x1765fa0  +0x190 0x84e30 (fwd->+0x198)  +0x198 0x17658d0
+0x1a0 0x177cec0  +0x1a8 0x84fd0 (fwd->+0xb8)  +0x1b0 0x85140 (fwd->+0x130)
+0x1b8 0x851b0 (fwd->+0x138)
+0x1c0 0x1769150 (IterateDirectory)  +0x1c8 0x1767e60
+0x1d0 0x85e20 (fwd->+0x1c0)  +0x1d8 0x85e80 (fwd->+0x1c8)
+0x1e0 0x1767e70 (fwd->+0x110)  +0x1e8 0x1768da0 (fwd->+0x110)
+0x1f0 0x1768e10  +0x1f8 0xe3c10 (fwd ->inner+0x1f8)
+0x200 0x1771040  +0x208 0x86270  +0x210 0x177d040  +0x218 0x177d170
+0x220 0xe0  +0x228 0xe0  +0x230 0xb290  +0x238 0x25460 (ret INT64_MAX)
```

### Shapes of the requested slots

The real file-op slots share one shape: **RSI = TCHAR\* path**, RDI = this;
they call the **pak path/lookup helper 0x1765d30** (or 0x1768e50) with the
path and a stack-local out-struct, then on a pak miss **forward to the inner
file `[this+8]` at the SAME vtable slot**. RSI is the TCHAR* passed straight
into the lookup helper (0x1765d30 is the path normalizer / FindFileInPakFiles).

- **+0x80 FileExists 0x177c2c0**: `mov rbx,rsi(path); mov r14,rdi; xor edx;
  xor ecx; call 0x1765d30` — path→lookup, then pak/inner. AGREE (+0x80).
- **+0x88 FileSize 0x177c360**: `lea rcx,[rbp-0x80](out); mov rbx,rsi(path);
  mov r14,rdi; call …` — int64 return. AGREE (+0x88).
- **+0xb8 OpenRead 0x177c6f0**: `lea rdx,[rbp-0x30](out handle); mov rbx,rsi
  (path); call 0x1765d30` — returns IFileHandle; AGREE (+0xb8).
- **+0xc8 0x177c870 / +0xd0 0x177c950**: OpenWrite/handle-release family,
  self-forward at own offset. AGREE.
- **+0x10 0xb290 / +0x18 0xe0 / +0x30 0xb290 / +0x38 0xb290**: stubs
  (ret / return-false). AGREE (unlabeled stubs).
- **+0x40 0x1771240**: does NOT take a bare path — reads `[rsi+8]` as a count
  (`cmp dword [rsi+8],0; jle …`) and uses `[rdi+0x38]`; takes a **structured
  argument (TArray/batch at RSI)**, not a TCHAR* to the normalizer. Still
  unlabeled. EXTENDS PAK-PIN-PASS [GAP] (now: structured/batch arg, not a path
  slot).
- **+0x108 0x1770e40**: args `ecx,edx` (flags) + `rsi`=path; calls 0x1765d30
  (path lookup) — OpenRead/OpenAsyncRead-with-flags class. RSI path → normalizer.
- **+0x110 0x1769800**: `call 0x1768e50` (pak-map existence probe), on miss
  `mov rdi,[r14+8]; call [rax+0x110]` — forwards to inner at **same +0x110**.
  RSI = path.
- **+0xd8..+0x100**: +0xd8 0xe3240, +0xf8 0x1770bb0, +0x100 0x25450 — a mix of
  pure same-offset inner-forwarders (`mov rdi,[rdi+8]; mov rax,[rdi]; jmp
  [rax+SAME]`) and ICF helpers; +0xe0/+0xe8 return false.
- **+0x118..+0x1b8**: dominated by same-offset inner-forwarders (e.g. +0x118
  0xe3ab0, +0x1a8 fwd→+0xb8, +0x1b0 fwd→+0x130, +0x1b8 fwd→+0x138) plus a few
  real impls (+0x120/+0x128/+0x180/+0x198/+0x1a0). AGREE with the prior pass's
  "17 same-offset forwarders" cross-validation (we count the same family;
  the interface offsets are self-consistent — a mis-based vtable would break
  the forwarder self-offset agreement, and it holds).

### Physical (lowest) platform-file vtable 0x7ebc6a0 — same slots

```
+0x10 0xb290 (ret)   +0x18 0xe0 (false)   +0x30/+0x38/+0x40 0xb290 (stub)
+0x68 0xe0  => GetLowerLevel returns NULL  (this is the BOTTOM of the chain)
+0x78 0x253c0 (GetTypeName, folded)
+0x80 0x7690 (FileExists)  +0x88 0x7810 (FileSize)  +0xb8 0x7df0 (OpenRead)
+0xc8 0x7990  +0xd0 0x7dd0  +0x108 0x8b50  +0x110 0x74f0  +0x1c0 0x85210
```
All real slots funnel through the OS-path helper **0x71c0** (TCHAR→OS path)
and then into **PLT import stubs** (see §3). `+0x68 = 0xe0` (returns 0) proves
it is the lowest level (no inner) — this is why it is excluded from the
wrapper enumeration below. AGREE with the prior pass's physical-impl pins
(0x7690/0x7810, "physical impl loads stat size").

### All IPlatformFile wrapper classes (GetLowerLevel 0xb3500 at +0x68)

A raw "+0x68 == 0xb3500" scan yields **343** vtables — but `0xb3500`
(`mov rax,[rdi+8]; ret`) is ICF-shared by many unrelated classes, so that is
over-matched. Requiring the **paired SetLowerLevel `0xb3510` at +0x70**
(the real IPlatformFile discriminator) yields exactly **4** wrapper classes,
each with a literal GetTypeName (+0x78):

| vtable | dtor (+0x00) | GetTypeName impl (+0x78) | type literal |
|---|---|---|---|
| **0x7ec2930** | 0x6cd0 | 0xb3520 | **"LogFile"** |
| **0x7ec39f8** | 0x6cd0 | 0xe3920 | **"CachedReadFile"** |
| **0x7fee8c0** | 0x17652f0 | 0x17643d0 | **"PakFile"** |
| **0x8616af8** | 0x38e84e0 | 0x38ea180 | **"SandboxFile"** |

These are all the chain-capable IPlatformFile wrappers linked into the image
(the physical file 0x7ebc6a0 is the base, GetLowerLevel→NULL). EXTENDS the
prior pass (it pinned only PakFile); the full wrapper set is Pak / Cached /
Sandbox / Log. (Caveat: a wrapper with a *custom* SetLowerLevel would be missed
by the strict filter; none such appeared — the 4 are the ICF-standard set.)

---

## 3. IoStore read path — the key question: VTABLE, not direct OS calls

**Answer: IoStore opens/sizes/reads `.utoc`/`.ucas` containers through the
IPlatformFile VTABLE on the chain-top object it gets from
`FPlatformFileManager::Get().GetPlatformFile()`, NOT via direct OS syscalls.
Direct OS calls happen only at the PHYSICAL layer at the bottom of the chain.**

Evidence:
- Container format strings (UTF-16): `.utoc` @0x7974c24 (refs in fns 0x175454f,
  0x175dd46, **0x176f09c** = pak mount-helper utoc tail), `.ucas` @0x78d1148
  (refs in fns 0x16fb19a, 0x1754d56, **0x175de76**, 0x176af36), partition
  suffix `_s%d` @0x753ebbc. `.utoc`-extension diagnostic @0x78d15a8. (The bare
  `.ucas`/`.utoc` ascii forms are absent — only UTF-16, as expected for FString.)
- The IoStore functions pair **`Get()` (0xb8ed0) then `GetPlatformFile()`
  (0xb8a70)** at **15 sites** in the 0x16f0000–0x1762000 range (e.g. 0x16faf72/
  0x16faf7a, 0x175de93/0x175de9b). The returned chain-top pointer is captured
  into the reader object (e.g. @0x16faf81 `mov r15,rax`, then stored into the
  reader struct). **Zero** direct PLT file-open calls appear in these functions.
- **Proven vtable dispatch** (partition walk fn 0x171b9d0):
  bytes `48 8b 7b 10 4c 89 f6 48 8b 07` @0x171ba68 →
  ```
  0x171ba68: mov rdi,[rbx+0x10]   ; the IPlatformFile* held at reader+0x10
  0x171ba6c: mov rsi,r14          ; RSI = container partition path (TCHAR*)
  0x171ba6f: mov rax,[rdi]        ; its vtable
  0x171ba72: call [rax+0x88]      ; FileSize  (slot +0x88)
  ```
  So the container is sized through `[reader+0x10]->vtable[+0x88]` with the
  path in RSI. The same `[reader+0x10]` object serves FileExists(+0x80)/
  OpenRead(+0xb8) for the container. AGREE/EXTENDS PAK-PIN-PASS §4 (it noted
  `[reader+0x10]` vt+0x88; here it is byte-proven with the this-pointer).
- **Where the OS actually gets hit**: only at the physical layer. Physical
  OpenRead 0x7df0 calls the OS-path helper 0x71c0 then an **import PLT stub**
  0x7472090: bytes `ff 25 7a 03 cc 01 68 29 00 00` =
  `jmp [rip+0x1cc037a] (GOT 0x9132410); push 0x29; jmp 0x7471df0` — a classic
  lazy-bind PLT thunk to OS import #0x29 (file open; symbol Sony-obfuscated).

**Consequence for B2 (observe-only arm):** because container I/O dispatches
through the chain-top IPlatformFile vtable, a data-only vtable observe/hook on
the layer the reader captures (chain top from Get().GetPlatformFile(), i.e.
the pak layer once installed) WILL see the utoc/ucas opens; IoStore does not
bypass the chain with raw syscalls on this build. The exact OpenRead slot the
readers call for the container handle is +0xb8 on the same `[reader+0x10]`
object (sync) with +0x108-class used for async; FileExists+0x80 / FileSize+0x88
are byte-proven, OpenRead+0xb8 is the same-object sync path. (Phase-0 live
logging should still confirm the slot + this-pointer per the prior pass.)

---

## 4. Boot order (static estimate)

**Initialize 0x1769ee0** is virtual (pak slot **+0x28**), so there are **0
direct E8 callers** — it is invoked via `call [vt+0x28]`. Identity confirmed by
its own assert string `"FPakPlatformFile::Initialize"` @0x1769f01 (it also
references `"RetirePakReaders"` @0x1769f2d). AGREE with the prior pass.

The chain is assembled in **FEngineLoop::PreInit** (fn carrying
`.\Runtime/Launch/Private/LaunchEngineLoop`). The wrapper-stacking loop runs
the identical block **4 times** (4 conditional layers), e.g. @0x3a142a0–0x3a143e0:
```
call [rax+0x20]   ; ShouldBeUsed  (cmdline gate, e.g. -NoPak)
call [rax+0x28]   ; Initialize(inner, cmdline)   <- pak layer => 0x1769ee0
call 0xb8ed0      ; Get()
call 0xb8ad0      ; SetPlatformFile(wrapper)  -> installs new [mgr+0], calls new[+0x30]
call 0xb8ed0/0xb8b90/0xb8a70  ; re-fetch chain top
```
So per layer: **ShouldBeUsed(+0x20) → Initialize(+0x28) → SetPlatformFile**.
The FPakPlatformFile is one of these 4 layers; its Initialize wires the inner
(lower) file and parses the command line.

**Container mounting happens AFTER the chain is built:** the pak mount helper
0x176ec30 and its utoc companion tail (fn 0x176f09c, refs `.utoc` @0x176f279)
reach the chain top via `Get().GetPlatformFile()` and register the associated
`.utoc`/`.ucas` container (container ctor 0x177fc90, registry call 0x175a4d0 —
addresses carried from the prior pass, not independently re-derived here). The
IoStore readers (0x171b9d0, 0x175de76, 0x16faf…) capture
`Get().GetPlatformFile()` lazily at first container access and store it at
`[reader+0x10]`.

**Estimated order:** physical base (default 0x9308808) → FEngineLoop PreInit
stacks wrappers (ShouldBeUsed/Initialize/SetPlatformFile ×4, pak among them) →
paks mount + IoStore containers register/open through the now-built chain. A
Phase-0 observe hook armed after PreInit, on the chain-top vtable, will see the
container opens.

---

## 5. Agreement summary vs PAK-PIN-PASS.md

- AGREE: image basis & RELA count; Get()/GetPlatformFile()/0xb8aa0/0x4ca0
  addresses and shapes; singleton 0x932a428 + guard 0x932a438; default physical
  0x9308808 + vtable 0x7ebc6a0; pak vtable base 0x7fee8c0 and all listed slot
  impls; dtor pair 0x17652f0/0x17656a0 with self-LEA; GetLowerLevel 0xb3500 at
  +0x68 / SetLowerLevel 0xb3510 at +0x70; GetTypeName→"PakFile"; FileExists+0x80,
  FileSize+0x88, OpenRead+0xb8, IterateDirectory+0x1c0; Initialize 0x1769ee0 at
  +0x28 with assert string; IoStore sizes partitions via `[reader+0x10]` vt+0x88;
  0x9131f08 is the abort-canary import (loaded at function entry, not the file
  manager).
- EXTENDS (new this pass): SetPlatformFile = **0xb8ad0** (writes `[mgr+0]`,
  calls new[+0x30]), 4 callers all in FEngineLoop PreInit; RemovePlatformFile =
  **0xb8d60** (1 caller, pak teardown); full IPlatformFile wrapper set =
  **PakFile / CachedReadFile / SandboxFile / LogFile** (strict +0x68/+0x70 pair,
  with vtable addresses & type literals); the per-layer boot pattern
  ShouldBeUsed(+0x20)→Initialize(+0x28)→SetPlatformFile; **byte-proof that
  IoStore container I/O is vtable-dispatched, with OS syscalls only at the
  physical layer via PLT stub 0x7472090 (GOT 0x9132410, import #0x29)**; slot
  +0x40 (0x1771240) takes a structured/batch arg (count at RSI+8), not a path.
- DISAGREE (correction): `[mgr+8]` is **NOT** an FPakPlatformFile holder — it
  is the lazily-cached **physical/default** platform file (0x4ca0→0x9308808),
  written only by the 0xb8aa0 accessor's own lazy init. The prior pass's
  "`[GPlatformFileManager+8]` = engine-wide FPakPlatformFile holder" hypothesis
  is not supported statically; the `[obj+8]` pak install it cited is the pak
  **module** object's member, not the manager's.

## 6. Honest limits
- Everything here is static / Mac-only; nothing is console-proven. Live
  admission still needs prologue-signature + live-base + vptr census per the
  pin law.
- Container-registry internals (0x177fc90 ctor, 0x175a4d0 registry) are cited
  from the prior pass, not independently re-derived this session.
- The IoStore OpenRead slot on `[reader+0x10]` is inferred as +0xb8 (sync) /
  +0x108 (async) from the shared-object argument; only FileSize+0x88 is
  byte-proven with its this-pointer. Phase-0 live logging remains the decider
  for exact slot + layer + this per the prior pass.
- Script: `cap07-phase0/re_verify.py` (commands: verify, chain, stores,
  getcallers, callers, pakvt, physvt, vtslots, wrappers, iostore, bootorder,
  disasm, xref). Run with the system python3 (capstone in user site-packages).

— CAP-07 Phase-0 static re-verify, 2026-10-12. Read-only on the pinned eboot;
only files created are this report and `re_verify.py`.
