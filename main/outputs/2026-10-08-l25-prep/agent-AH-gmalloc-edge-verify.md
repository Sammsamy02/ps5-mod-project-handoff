# Agent AH — independent binary verification of the GMalloc-edge crash chain (Claude review 2)

Date: 2026-10-08. READ-ONLY on all inputs; scratch work in /tmp/l25-ah/; this report is
the only file written. Every claim below was re-derived from bytes, not inherited from
Z11/Claude; where I agree with Z11 I say so explicitly.

## 0. Artifacts and mappings established

| Artifact | Role |
|---|---|
| `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin` | game eboot, 194,241,424 B, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (verified; matches the crash-session image fp `04bb4330…` build id in the lineage staged table) |
| `outputs/2026-10-08-l24-package/pass-1/modbridge_supplier_composition.prx` | l24 FSELF actually loaded in every abort block (fp `ffad2cd5…`, sha `2e712a8b…`) |
| `outputs/2026-10-08-l24-package/pass-1/build/combined-supplier.o` | pre-link object, 207,709 sections / 295,090 syms; source of all modbridge symbol names + relocations |
| `outputs/2026-10-08-l24-package/lineage/CompiledNativeLayoutDeclarations.hpp`, `SeedTable.h`, `RuntimeBuildProfile.hpp` | the l24 staged native-layout table (provenance cross-check only; verdicts rest on the binaries) |
| Vendored UE 5.6.1 headers: `official-source/Engine/Source/Runtime/Core/Public/HAL/MemoryBase.h`, `Misc/Exec.h`, `Misc/OutputDevice.h` | slot-order citations |

Mappings (both re-derived and frame-verified, not assumed):

- **eboot**: PT_LOAD0 (exec) file `0x4000` ↔ vaddr 0; all analysis at module-relative
  vaddr (klog base 0x400000). Note: `/tmp/eboot-backup.bin` (sha `afe0a2a5…`) differs
  from the canonical file only past file offset `0xB93C37F` (trailing metadata, past the
  last PT_LOAD's file content) — **code bytes identical**, so Z11's frame decoding
  transfers. I re-verified every eboot frame edge myself anyway: return addresses
  `0xf918`, `0x1e3ad8`, `0x175197`, `0xb2379`, `0xb4b75` each directly follow a real
  call instruction (0xf913 → abort@PLT 0x7472200; 0x1e3ad5 → `call [rax+0x10]`;
  0x175192 → 0x1e3900; 0xb2374 → 0x1750b0; 0xb4b70 → 0xb2160). 5/5 verify.
- **PRX (final image)**: exec vaddr V ↔ **file 0x9bb0 + V** (not the intermediate ELF's
  phdr offsets). Proof: with this single bias, **all 12** modbridge stack frames are
  call-aligned (a unique bias in ±0x40000 does this; Z11's "FSELF data block at file
  0x9bb0" note agrees). modbridge `RC::Unreal::FMalloc::Realloc` = fn@0xa1240, crash
  call site `call rax` at 0xa1369 (return 0xa136b).

## 1. Task 1 — the GMalloc vtable layout and what +0x40 is

### 1.1 The game's FMallocBinned3 vtable, reconstructed from eboot bytes

The eboot has no symtab; vtables are filled at load by a table of
`{value, r_offset, r_info=8}` triples (R_X86_64_RELATIVE-shaped) in the file
(file ≈ 0x933e4xx). `fn@0xb49b0` (the crash-entered function) is stored at exactly one
place: **vaddr 0x7ec2728**. The live vptr is pinned by two un-fakeable internal
dispatch distances inside base-class thunks that the same vtable contains:

```
0xb8a20 (thunk): mov rax,[rdi]; call [rax+0x30]     → must be Malloc
0xb8a30 (thunk): mov rax,[rdi]; call [rax+0x40]     → must be Realloc (TryRealloc fwd)
0xb8a40 (thunk): mov rax,[rdi]; call [rax+0x30]; memset  → MallocZeroed default
```

`0xb48e0` (at 0x7ec2718) is Malloc-shaped (`cmp rsi,0x20001` size bound, `cmp edx,0x11`
alignment bound at entry ⇒ args (this, Count, Alignment)); therefore vptr+0x30 =
0x7ec2718 ⇒ **vptr = 0x7ec26e8** (module-relative; runtime 0xBEC26E8). (The FMallocBinned3
dtor at 0xb1070 stores `lea rax,[0x7ec26e0]` — the base-in-destruction view, 8 bytes off;
the thunk distances are the operative anchor.) Full reconstructed vtable:

| slot off | fn | identity (shape-verified) |
|---|---|---|
| +0x00 / +0x08 | 0xb1070 / 0xb1440 | ~FMallocBinned3 complete / deleting dtor (dtor destroys a dozen mutexes at this+0x2210…0x23f0) |
| +0x10 … +0x28 | 0xe0 ×4 | `xor eax,eax; ret` — FExec::Exec-family returning false (see §1.2) |
| **+0x30** | 0xb48e0 | **Malloc** (fast path: TLS arena ctx, size-class tables, free-list pop) |
| +0x38 | 0xb8a20 | TryMalloc (fwd → [+0x30]) |
| **+0x40** | **0xb49b0** | **Realloc** — the crash-entered function |
| +0x48 | 0xb8a30 | TryRealloc (fwd → [+0x40]) |
| +0x50 | 0xb4bf0 | Free |
| +0x58 / +0x60 | 0xb8a40 ×2 | MallocZeroed / TryMallocZeroed |
| +0x68 | 0xb4cd0 | QuantizeSize (size-class table lookup) |
| +0x70 / +0x78 / +0x80…+0x98 | 0xb4db0 / 0xb2820 / 0xb4df0… | GetAllocationSize / Trim / TLS-cache family |
| +0xa0 … +0xd0 | … | InitializeStatsMetadata … GetDescriptiveName |

This matches, slot for slot, the l24 lineage staged table
(`[FMalloc] Malloc=0x30 TryMalloc=0x38 Realloc=0x40 TryRealloc=0x48 Free=0x50
QuantizeSize=0x68 GetAllocationSize=0x70 Trim=0x78 … ClearAndDisable…=0x98
InitializeStatsMetadata=0xa0 … GetDescriptiveName=0xd0`) — independent confirmation.

### 1.2 Vendored-header slot math (UE 5.6.1)

`MemoryBase.h:96` — `class FMalloc : public FUseSystemMallocForNew, public FExec`.
`FUseSystemMallocForNew` has only static operator new/delete (no vtable slots).
`Misc/Exec.h`: `CORE_API virtual ~FExec();` (line 31 → 2 Itanium slots), then in a
shipping build (`UE_ALLOW_EXEC_COMMANDS` = 0 branch, lines 50+55 `final`) plus
`Exec_Runtime`/`Exec_Editor` (lines 61/67; `Exec_Dev` compiled out) = **4** Exec-family
slots — exactly the four `xor eax,eax; ret` stubs at +0x10…+0x28. Then FMalloc's own
declaration order (MemoryBase.h:104-284): Malloc(104) → TryMalloc(110) → **Realloc(115)**
→ TryRealloc(122) → Free(127) → MallocZeroed → TryMallocZeroed → QuantizeSize → …

| counting model | Malloc | Realloc | +0x40 = |
|---|---|---|---|
| no base slots at all (naive) | 0x00 | 0x10 | QuantizeSize |
| FExec = 1 Exec slot only | 0x08 | 0x18 | TryMallocZeroed |
| **actual game/vendored shipping combo: dtor pair + 4 Exec + order above** | **0x30** | **0x40** | **Realloc** |

**Answer to the brief's "(0x40−8)/8 = 7?"**: that formula (subtracting one slot as if
the vptr pointed at offset-to-top) is the wrong model. Under the x86-64 Itanium ABI the
vptr points at slot 0, so **+0x40 is slot index 0x40/8 = 8**; with the game's six
pre-Malloc slots, slot 8 = Realloc. Yes — **+0x40 is consistent with (and exactly is)
the Realloc slot in this engine's FMalloc vtable.**

### 1.3 The modbridge side: which offset the crash call actually used

`RC::Unreal::FMalloc::Realloc` (PRX fn@0xa1240) does NOT hard-code an offset. It
computes `offset` once (function-local static, `.bss…6offset`, guard-checked at entry)
from `RC::Unreal::FMalloc::VTableLayoutMap` — an `unordered_map<wstring, uint32>` keyed
by member name (lambda at PRX 0xa1380; key `.L.str.23` = **L"Realloc"**, throw message
"Virtual FMalloc::Realloc is unavailable, possibly unsupported in engine version"),
then: `vptr = *(void**)this; fn = *(void**)(vptr + offset); call fn(this, Original,
Count, Alignment)` (bit_cast chain per the section's relocations; crash `call rax` at
0xa1369).

Where does the map value come from? Three layers, all in the l24 binary:

1. Per-engine-version defaults `RC::Unreal::UnrealVirtual{407…508}::set_virtual_offsets`
   insert-if-absent. Extracted from the final PRX (31 variant sites referencing
   L"Realloc"): UE4.07-4.11 → 0x20, 4.12-4.24 → 0x18, 4.25-5.01 → 0x20, 5.02 → 0x30,
   **5.03-5.08 → 0x38 — none ever produces 0x40.**
2. `RC::Unreal::NativeProvidedVirtualVC::set_virtual_offsets()` calls
   `RC::Platform::NativeLayouts::PublishStagedNativeLayoutMaps(5, 6, 0, 0)` which
   publishes the **staged native layout table** — lineage
   `CompiledNativeLayoutDeclarations.hpp`, pinned to this exact image
   (`BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`, EngineRevision 5.6.1, image sha
   3b6cfad5…): **`[FMalloc] Realloc=0x40`**. `RuntimeBuildProfile.hpp` lists
   `FMalloc::Realloc` as a *Required* layout pin with value 64 (=0x40).
3. The crash stack itself is the final arbiter: the entered function (containing return
   0xb4b75) is 0xb49b0 = the +0x40 slot. **The runtime offset used was 0x40.**

So the executed instruction sequence at the identified frames is precisely:

```
mb 0xa12bd: mov ecx,[offset-static]        ; = VTableLayoutMap[L"Realloc"] = 0x40 (staged native)
mb 0xa12c3: rax = vptr + rcx               ; &vtable[0x40]
mb 0xa12ce: call read-qword                ; load slot
mb 0xa1369: call rax                       ; → eboot 0x400000+0xb49b0 = FMallocBinned3::Realloc
eb 0xb49be: cmp rdx,0x20001 / cmp ecx,0x11 ; fast-path size/alignment bounds
eb 0xb4a15…: arena/TLS/free-list checks; fast path declined
eb 0xb4b70: call 0xb2160                   ; general Realloc path
```

Claude's "GMalloc+0x40 as the Realloc slot in the chain" is therefore **correct as a
binary fact** (it is the vtable slot, not a field of the allocator object — for
completeness, GMalloc-instance +0x40/+0x48 are the large-block mutex owner/recursion
fields written in the general path).

## 2. Task 2 — the `call [rax+0x10]` at eboot 0x1e3ad5

`fn@0x1e3900` is a varargs member formatter (FOutputDevice::Logf-shaped):

- prologue spills xmm0-7 + rdx/rcx/r8/r9 (va_start), saves a global device holder
  (`[0x9131f08]`, deref'd twice — GLog-side, saved for a post-call vtable-tamper check),
- grows a stack buffer via free@PLT/malloc@PLT and `call 0x7ad40` (vsnprintf-family) in a
  doubling loop,
- then: `movzx edx, byte [r12]` (verbosity byte from the 4-byte UE log header embedded in
  the message), `r12 += 4` (skip header → the TCHAR*), **`rax = [r14]` where r14 = this
  (the function's rdi arg)**, `rcx = r12`, **`call [rax+0x10]`** — i.e.
  `this->Serialize(V, Verbosity, Category)`.

What object r14 holds: `fn@0x1750b0` (its caller) is the engine's LowLevelFatalError
formatter: rdi points at the literal "LowLevelFatalError" string, it calls the
file/line printer, sets a global critical-error flag byte to 1, then
`rdi = *(void**)0x9359d28` (a GError-style output-device holder), stores the caller's
return address at `[rdi+0x10]`, and tail-calls `fn@0x1e3900` with fmt `L"%s"` (the
pre-formatted message as the single vararg). So **rax = vptr of the engine's error
output device (GError), and slot +0x10 is dispatched on that FOutputDevice-derived
object**.

Slot math vs vendored `Misc/OutputDevice.h` (declaration order, lines 142-213):

| slot | declaration |
|---|---|
| +0x00, +0x08 | `CORE_API virtual ~FOutputDevice();` (line 142 — Itanium dtor pair) |
| **+0x10** | **`virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) = 0;` (line 145)** |
| +0x18 | Serialize overload w/ double Time (line 146) |
| +0x20 / +0x28 / +0x30 | SerializeRecord (151) / Flush (153) / TearDown (162) |
| +0x38…+0x58 | Dump / IsMemoryOnly / CanBeUsedOnAnyThread / …OnMultipleThreads / …OnPanicThread (181-213) |

The dispatch arguments match Serialize's ABI exactly (rdi=this, rsi=V, rdx=Verbosity,
rcx=&Category). **+0x10 = FOutputDevice::Serialize — CONFIRMED; this is the fatal
logging path.** The callee (entered, proven by return 0xf918) is fn@0xf850, which does
FOutputDeviceError::Serialize's job: formats via the GLog-side holder 0x9131f08,
frees the message buffer, honors a "quiet" byte flag, performs a virtual cleanup call on
the device, dumps a stack trace, and on the flag path executes
`call abort@PLT (0x7472200); ud2` (at 0xf913). (Aside: a ctor at 0xf7e0 that stores
vptr 0x7ebc8f0 places fn@0xf850 at +0x18 — a sibling/different device class; the
dispatched object's vtable demonstrably has it at +0x10 since that call landed there.
Either way it is a Serialize-slot dispatch, not an allocator vtable.)

## 3. Task 3 — what the intervening allocator code checks; why it goes fatal

**Allocator family: FMallocBinned3** — proven by the shipped literals at the two fatal
sites (both UTF-16): file `'.\Runtime/Core/Private/HAL/MallocBinned3.cpp'`, messages
`'FMallocBinned3 Attempt to realloc an unrecognized pointer %p'` and
`'MallocBinned3 Corruption Canary was 0x%x, should be 0x%x'`. Code shape agrees
(region granularity `>> 0x1e` with `>>0x20 ≤ 0x12` bound, per-region max-block word
tables ×16, 32-byte page descriptors, free-list pop + steal-from-larger-class, TLS
arena context via 0x7472530 with flag at ctx+0x990, per-bundle mutexes).

The two eboot allocator frames:

- **fn@0xb49b0 = FMallocBinned3::Realloc fast path.** Handles only small in-arena
  blocks: `NewSize < 0x20001 && Alignment < 0x11`, arena/TLS context valid, free block
  available for the size class (else steal from one class up). On any miss →
  `call 0xb2160`. The crash took exactly this decline (return 0xb4b75).
- **fn@0xb2160 = general Realloc.** In order: `Original == NULL` → plain Malloc;
  pointer's arena-relative offset outside the managed window (`offset>>0x20 > 0x12`) →
  large/OS-allocation path: take mutex at this+0x38 (owner cached at this+0x40,
  recursion count this+0x48), compute the hash bucket from heap geometry
  (shifts/masks from this+0x10/0x18/0x20/0x28, table at this+0x58), and walk the chain:
  `while ([node] != key) node = [node+0x18]`.
  - **Walk exhausts → `LowLevelFatalError("MallocBinned3.cpp", 0x370 = line 880,
    "FMallocBinned3 Attempt to realloc an unrecognized pointer %p", Original)`** — call
    at 0xb2374. **The observed crash frame (return 0xb2379) is THIS branch.**
  - Node found → header validation `cmp dword [hdr], 0x17ea5678` (Binned3 canary);
    mismatch → `LowLevelFatalError(…, 0xed = line 237, "MallocBinned3 Corruption Canary
    was 0x%x, should be 0x%x")` — call at 0xb241a. **NOT the taken branch** (that would
    have left return 0xb241f on the stack).
  - Canaries OK → in-place resize when capacity suffices, else Malloc-copy-Free.

**The condition that routed execution to the fatal path: the `Original` pointer passed
by modbridge's `TSizedHeapAllocator::ResizeAllocation → FMemory::Realloc →
FMalloc::Realloc` was not found in the current GMalloc (FMallocBinned3 singleton)'s
managed arena ranges or its large-allocation hash.** I.e., the allocator rejected the
block as *unrecognized* — exactly Claude's reading. It is not a wrong-vtable-slot
condition: the dispatch provably reached the true Realloc (+0x40).

## 4. Task 4 — verdict

**CONFIRM** Claude's interpretation, with one precision added:

- The `+0x10` dispatch is the engine's **fatal logging path**: an
  `FOutputDevice::Serialize`-slot virtual on the error output device (GError), inside
  `LowLevelFatalError → FOutputDevice::Logf("%s") → Serialize → FOutputDeviceError::Serialize
  → abort()`. Slot math matches vendored OutputDevice.h (dtor pair then Serialize at
  +0x10).
- The allocator **rejected the Realloc** — literally
  `FMallocBinned3 … Attempt to realloc an unrecognized pointer %p` (MallocBinned3.cpp:880)
  — and it is **not** a wrong-vtable-slot-inside-the-allocator problem: the modbridge's
  dispatch through GMalloc vtable **+0x40** entered the genuine FMallocBinned3::Realloc
  (0xb49b0), whose slot identity is triple-pinned (RELA patch table, thunk dispatch
  distances, lineage staged table). Claude's "+0x40 = Realloc" is correct; the brief's
  alternative slot formula "(0x40−8)/8 = 7" is not how the vtable is addressed.
- One refinement: the *rejection cause* is only narrowed statically to "pointer not in
  the current allocator's structures". The two live sub-hypotheses —
  (a) `Data` was allocated by a **different allocator** (the GMalloc instance was
  swapped during init — the l24 log records `GMalloc: 0x970add8 -> 0x97089b8`, i.e.
  linked 0x930add8 (variable) now holding the singleton at linked 0x93089b8; anything
  allocated under the previous instance, or by modbridge's own libc heap pre-repoint,
  is invisible to the current Binned3 hash), vs (b) the block/pointer was **corrupted or
  stale** (init-thread write racing save-data teardown, per Z1/Z11) — cannot be
  separated from the static binary.

Runtime evidence that would discriminate (ties to the item-4 logging values):

1. **GMalloc variable value** (read `*(void**)0x970add8`): expect `0x97089b8`
   (linked 0x93089b8 + base 0x400000). Any other value → a different allocator
   instance is live and the whole slot table must be re-derived for it.
2. **Instance vptr** (read `*(void**)GMalloc`): expect `0xBEC26E8` (= 0x400000 +
   0x7EC26E8). This pins FMallocBinned3 and makes +0x40 resolve to module+0xb49b0
   (Realloc) by the patch table — confirming a correct dispatch at the moment of crash.
3. **`Data` pointer** (the `Original` handed to Realloc — the delete-listener TArray's
   allocation): map it against the current instance's arena window
   (`Data − arenaBase`, high-32 bits ≤ 0x12) and against the previous instance's /
   libc-heap ranges. In-window-but-unhashed → corruption/race (b). Out-of-window and
   inside a *previous* allocator's or libc heap → foreign-allocation (a). Additionally,
   logging `*(uint32*)(header)` where findable would show canary 0x17EA5678 vs garbage,
   but the taken branch was the not-found one, so the canary never got consulted.

## Appendix — key disassembly excerpts (module-relative vaddrs)

```
;; modbridge FMalloc::Realloc wrapper (PRX 0xa1240) — slot resolution + crash call
0xa125f: cmp byte [guard],0 / 0xa126f: __cxa_guard_acquire      ; one-shot init
0xa129c: rax=[rbp-0x58](this); …; call 0xa0fe0 / 0xa107e0-family ; bit_cast<byte*, FMalloc*>
0xa12bd: mov ecx, dword [rip+0x427dbd5]                          ; = static 'offset' = map[L"Realloc"]
0xa12c3: add rax, rcx                                           ; vptr + offset  (offset = 0x40)
0xa12ce…0xa12d6: read slot; bit_cast to void*(FMalloc::*)(void*,ulong,uint)
0xa1356: mov rdi,[rbp-0x58]  ; this
0xa135e: mov rsi,[rbp-0x10]  ; Original
0xa1362: mov rdx,[rbp-0x18]  ; NewSize
0xa1366: mov ecx,[rbp-0x1c]  ; Alignment
0xa1369: call rax            ; ← crash return 0xa136b → FMallocBinned3::Realloc

;; initializer lambda (PRX 0xa1380): VTableLayoutMap.find(L"Realloc") … it->second
;; throw: "Virtual FMalloc::Realloc is unavailable, possibly unsupported in engine version"

;; eboot FMallocBinned3::Realloc fast path (0xb49b0) — decline to general path
0xb49be: cmp rdx, 0x20001 ; 0xb49c8: cmp ecx, 0x11 ; jne 0xb4b70
0xb4a32/0xb4a3e: TLS/arena checks; 0xb4b39: test rbx,rbx; je 0xb4b70
0xb4b70: call 0xb2160     ; ← crash return 0xb4b75

;; eboot general path (0xb2160) — the rejection branch actually taken
0xb2194: shr rcx,0x20 ; 0xb2198: cmp ecx,0x12 ; ja 0xb2228      ; outside managed window
0xb2241/0xb224a: mutex lock (0x7471ee0) / owner cache [this+0x40]
0xb22a0: cmp [r8],rcx ; 0xb22a5: r8=[r8+0x18] ; loop             ; hash-chain walk
0xb2358: (not found) lea rdi,[.\\Runtime/Core/Private/HAL/MallocBinned3.cpp]
0xb2363: lea rdx,[FMallocBinned3 Attempt to realloc an unrecognized pointer %p]
0xb236a: mov esi, 0x370          ; line 880
0xb2374: call 0x1750b0           ; ← crash return 0xb2379  ★ the fatal entry
;; (not taken) 0xb2310: cmp ecx,0x17ea5678 ; jne 0xb23ff → "Corruption Canary …" line 0xed

;; eboot LowLevelFatalError varargs (0x1750b0)
0xb1751 7: lea rdi,["LowLevelFatalError"]  0xb1751e: call printer(file="",line=…,fmt,path)
0xb175182: mov byte [GIsCriticalError-style flag], 1
0xb175189: mov rdi,[0x9359d28]   ; GError-style device
0xb17518e: mov [rdi+0x10], rbx   ; caller return address
0xb175192: call 0x1e3900         ; fmt L"%s"  ← crash return 0xb175197

;; eboot FOutputDevice varargs formatter (0x1e3900) — the +0x10 dispatch
0x1e3a8d: call 0x7ad40 (vsnprintf-family, buffer-doubling loop above)
0x1e3ac6: movzx edx, byte [r12]  ; verbosity from 4-byte log header
0x1e3acb: mov rax, [r14]         ; r14 = this (the error device)
0x1e3ace: add r12, 4             ; skip header → TCHAR* V
0x1e3ad2: mov rcx, r12           ; &Category per Serialize ABI
0x1e3ad5: call [rax + 0x10]      ; ← FOutputDevice::Serialize  ★ crash return 0x1e3ad8

;; eboot FOutputDeviceError::Serialize (0xf850) → abort
0xf890: call 0x1e4300 (format via GLog-side holder 0x9131f08)
0xf8c9: cmp byte [quiet-flag], 0 ; … virtual cleanup call [rax+0x60]; stack dump
0xf90e: cmp byte [..], 1 ; jne exit@PLT(0x74722c0)
0xf913: call 0x7472200           ; abort@PLT  ← crash return 0xf918
0xf918: ud2
```

Provenance: all disassembly from the two sha-pinned binaries above via capstone 5.0.7
(scratch scripts under /tmp/l25-ah/); symbol names from combined-supplier.o symtab +
relocations; no input file modified.
