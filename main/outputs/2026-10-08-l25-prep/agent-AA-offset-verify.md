# Agent AA — FUObjectArray listener-member offsets verified from ENGINE CODE in the authenticated image

Date: 2026-10-08. READ-ONLY: the only file written is this report (scratch scripts
under /tmp/l25-offset-verify/). Claude review item 1 (Mac-only, pre-launch).

## 0. Inputs and method (reused from Z6)

- Binary: `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`
  — 194,241,424 B, sha256 3b6cfad5… (unchanged). ELF64 x86-64, e_type 0xFE10
  (SCE_RELEXEC), no usable symbol table (shoff bogus); PT_DYNAMIC present with
  Sony-hashed NID import names (unresolvable to names — structure used instead).
- Memory map (from the live `[Seeds]` logs in `evidence/l24-ue4ss-4.log` /
  `/tmp/l24-ue4ss*.log`): runtime base 0x400000; exec segment vaddr
  [0, 0x7479b1c) at file 0x4000; data/BSS PT_LOAD vaddr [0x913c000, 0x9633418).
- **GUObjectArray anchor**: `[Seeds] slot 0 g_uobject_array bound at 0x97ba110
  (data; identity and ABI unverified)` → image vaddr **0x93BA110** (BSS tail of
  PT_LOAD#7, past its file-backed part — a zero-init global, as expected). This
  report removes the "unverified" qualifier: the engine's own code uses this
  address as an `FUObjectArray*` in every function below.
- Method: whole-exec-segment scan for RIP-relative references into
  [GUObjectArray, GUObjectArray+0xC0) (1,654 validated hits, K=0x00 "this"
  loads = 1,565), then capstone disassembly of the referencing functions and of
  the direct-displacement code inside them; PLT call-target grouping;
  UTF-16LE/ASCII string decode for function identification.

## 1. Source-computed layout (vendored UE 5.6.1, THREADSAFE_UOBJECTS=1)

Member order (`official-source/.../UObject/UObjectArray.h:1409-1441`),
`#define THREADSAFE_UOBJECTS 1` hard-set at `ObjectMacros.h:33`:

```
int32  ObjFirstGCIndex            +0x00
int32  ObjLastNonGCIndex          +0x04
int32  MaxObjectsNotConsideredByGC +0x08
bool   OpenForDisregardForGC      +0x0C (+pad)
TUObjectArray ObjObjects          +0x10   (FChunkedFixedUObjectArray, size 0x20:
                                            Objects+0x00, PreAllocatedObjects+0x08,
                                            MaxElements+0x10, NumElements+0x14,
                                            MaxChunks+0x18, NumChunks+0x1C)
FTransactionallySafeCriticalSection ObjObjectsCritical   +0x30, size S
TArray<int32>                    ObjAvailableList         +0x30+S, size 0x10
TArray<FUObjectCreateListener*>  UObjectCreateListeners   +0x40+S, size 0x10
TArray<FUObjectDeleteListener*>  UObjectDeleteListeners   +0x50+S, size 0x10
FTransactionallySafeCriticalSection UObjectDeleteListenersCritical +0x60+S, size S
FThreadSafeCounter PrimarySerialNumber                    +0x60+2S
bool   bShouldRecycleObjectIndices                       +0x64+2S
sizeof(FUObjectArray) = 0x68+2S
```

The ONLY platform-dependent unknown is S = sizeof(FTransactionallySafeCriticalSection)
(Track B's PLATFORM-DEPENDENT flag, `NEW-VALUES-PLATFORM-DEPENDENT.json`
"FUObjectArray::ObjAvailableList offset 56 … storage includes platform locks":
`FThreadSafeCounter`, `UE::FPThreadsRecursiveMutex`, `pthread_mutex_t`):

| S assumption | Create | Delete | DeleteCrit | PrimarySerial | bShouldRecycle | sizeof | who assumed it |
|---|---|---|---|---|---|---|---|
| S = 0x08 | 0x48 | 0x58 | 0x68 | 0x70 | 0x74 | 0x78 | l23 generic-Unix generator (ObjAvailableList=0x38 ⇒ crit=8B pointer-only) |
| S = 0x28 | 0x68 | 0x78 | 0x98 | 0xB0 | 0xB4 | 0xB8 | **l24 profile / townfall-build-facts / RuntimeBuildProfile.hpp pins** (crit=0x28, pthread-by-value) |
| **S = 0x18** | **0x58** | **0x68** | **0x78** | **0x90** | **0x94** | **0x98** | **BINARY (this report)** |

The l24 profile numbers are exactly the S=0x28 column — i.e. the profile was
generated from an assumed critical-section size, not measured from the image.

## 2. Binary evidence — the engine's own listener functions

All at image vaddrs (runtime = +0x400000); all identified by structure + the
source-file string `.\Runtime/CoreUObject/Private/UObject/UObjectArray.cpp`
(0x7ae8209, ASCII) and assert strings (UTF-16LE):

- **0x4841A0 `FUObjectArray::AddUObjectDeleteListener`** (14 &GUObjectArray callers):
  `lea rbx,[rdi+0x78]` → PLT TryLock / slow-Lock (returns mutex ptr stored at
  `[this+0x80]`), recursion counter `inc qword [this+0x88]`; grow check
  `eax=[this+0x70]` (Num) vs `[this+0x74]` (Max); grow helper 0x298A0 with
  `rsi=[this+0x68]` (Data); store `[[this+0x68]+idx*8]=Listener`;
  `[this+0x70]=idx+1`; counter-- → `[this+0x80]=0` + PLT unlock.
  **Add takes the crit at +0x78.**
- **0x484240 `FUObjectArray::RemoveUObjectDeleteListener`** (69 callers): same
  lock at +0x78, same array 0x68/0x70/0x74, linear scan for the listener,
  RemoveSingleSwap (memmove PLT 0x7471E90), `dec [this+0x70]`, shrink check
  `[this+0x74]`, unlock.
- **0x483FF0 delete-notify dispatch** (the loop Claude item 1 asked for; sole
  caller 0x3C92E8 = UObjectBase::Deinitialize chain): locks crit **+0x78**
  (TryLock/slow/counter 0x88), `r13=[this+0x70]` Num, loop: `rax=[this+0x68]`
  Data, `rdi=Data[i]`, `call [vtable+0x10]` = **slot 2
  NotifyUObjectDeleted(Object, InternalIndex)** (rsi=Object, rdx=[Object+0xC]
  read before the loop), reverse iteration, unlock.
- **0x4843A0 `FUObjectArray::Shutdown`**: locks crit **+0x78**; loop over
  **Data [this+0x58] / Num [this+0x60]** — wait, order: first loop is the DELETE
  array **0x68/0x70** calling `call [vtable+0x18]` (slot 3
  OnUObjectArrayShutdown); on non-empty residual logs "All UObject delete
  listeners should be unregistered when shutting down the UObject array"
  (0x772CD54, line 0x221=545) — **string-confirms the 0x68 array IS
  UObjectDeleteListeners and 0x78 IS UObjectDeleteListenersCritical**; unlocks,
  then a second loop over the CREATE array **Data [this+0x58] / Num [this+0x60]**
  calling slot 3, residual error line 0x229=553.
- **0x4840B0 `FUObjectArray::FreeUObjectIndex`** (3 callers: GC purge 0x3736A3/
  0x37373B, Deinitialize 0x491692): `[Object+0xC]` InternalIndex; `cmp
  [rdi+0x24],idx` NumElements; `rdx=[rdi+0x10]` Objects; ×0x18 stride items;
  `[r14+4]` ObjLastNonGCIndex; recycles index into **ObjAvailableList
  Data 0x48 / Num 0x50 / Max 0x54** when `byte [r14+0x94]==1`
  (**bShouldRecycleObjectIndices 0x94**); assert "Removing object (0x%016llx)
  at index %d but the index points to a different object" (0x77D3D0C, line
  0x17d=381). No listener dispatch here (it lives in Deinitialize → 0x483FF0).
- **0x480F10 serial bump**: `lock xadd dword [rdi+0x90],eax`
  (**PrimarySerialNumber 0x90**), wrap check `<= 0x3E7` (=999 =
  START_SERIAL_NUMBER−1, `UObjectArray.h:900-903`), `lock cmpxchg
  [item+0x10],new` — FUObjectItem.SerialNumber at item+0x10; assert line
  0x205=517.
- **0x482280 / 0x373600 / 0x491647** (GC purge, Deinitialize): lock the crit at
  **+0x30** with the same 3-slot shape (lock@0x30, lazy mutex ptr@0x38,
  recursion counter@0x40) = **ObjObjectsCritical 0x30**; Deinitialize
  (0x491647-0x4916A8) saves/restores `byte [GO+0x94]`, locks 0x30, calls
  FreeUObjectIndex, then calls vtable slot 0 (dtor).
- Positive structural ID of GUObjectArray itself: rip-refs `GO+0x10`
  (Objects) and `GO+0x24` (NumElements) with the exact chunked-array access
  (`movzx ecx,idx; lea rcx,[rcx+rcx*2]; shr eax,0x10; add rax,[rdx+rax*8]`,
  ×0x18 FUObjectItem stride, `[obj+0x18]` NamePrivate) at 0x4ACE29/0x4ACE2F
  and dozens more; `cmp byte [GO+0xC],1` ×6 (OpenForDisregardForGC).
- RIP-ref histogram off GUObjectArray: K=0x00 ×1565, 0x10 ×20, 0x24 ×20,
  0x98 ×14, 0xa8 ×25, 0xb4 ×1, 0xb8 ×9 — the 0x98/0xa8/0xb8/0xc0 cluster
  (0x4AD041-0x4AD089) is a name/hash lookup reading the NEXT BSS globals
  (pointer-with-inline-fallback pattern, ×0x20 stride) — i.e. the struct
  ENDS at 0x98 and the profile's 0xB0/0xB4/0xB8 pins point into other globals.

### FTransactionallySafeCriticalSection on this build (PS5/Prospero) = 0x18 bytes

Shape read off every lock site: fast-state word at +0x00 (TryLock PLT takes
&crit and returns bool), lazily-allocated mutex pointer at +0x08 (slow-Lock PLT
returns it; unlock path zeroes it), 64-bit recursion counter at +0x10. Neither
the l23-generic 8-byte (pointer-only) nor the l24-profile 0x28-byte
(pthread-by-value) assumption matches; actual S=0x18.

## 3. Three-way table and per-field verdicts (l24 profile values from
`outputs/2026-10-08-l24-runtime/profile/profiles/townfall-build-facts.json`,
`original/units/CompiledNativeLayoutDeclarations.hpp`, `units/RuntimeBuildProfile.hpp`)

| Member | Profile (l24) | Source-order formula | Binary (engine code) | Verdict |
|---|---|---|---|---|
| ObjFirstGCIndex | 0x00 | 0x00 | 0x00 (quad pinned by 0x10/0x24 anchors; order per source) | CONFIRMED |
| ObjLastNonGCIndex | 0x04 | 0x04 | 0x04 (FreeUObjectIndex `cmp idx,[this+4]`) | CONFIRMED |
| MaxObjectsNotConsideredByGC | 0x08 | 0x08 | 0x08 (by layout coherence; neighbors all binary-pinned) | CONFIRMED |
| OpenForDisregardForGC | 0x0C | 0x0C | 0x0C (`cmp byte [GO+0xC],1` ×6) | CONFIRMED |
| ObjObjects (Objects/NumElements) | 0x10 | 0x10 | 0x10 / +0x24 (ubiquitous) | CONFIRMED |
| ObjObjectsCritical | (absent) | 0x30 | 0x30 (0x482280, 0x491676, GC 0x373611) | NEW: 0x30, size 0x18 |
| ObjAvailableList | (absent, l23-generic 0x38) | 0x30+S | 0x48 (Num 0x50, Max 0x54 — FreeUObjectIndex) | NEW: 0x48 |
| **UObjectCreateListeners** | **0x68** | 0x40+S | **0x58** (Num 0x60, Max 0x64 — Shutdown create loop) | **WRONG — correct 0x58** |
| **UObjectDeleteListeners** | **0x78** | 0x50+S | **0x68** (Num 0x70, Max 0x74 — Add/Remove/dispatch/Shutdown) | **WRONG — correct 0x68** |
| **UObjectDeleteListenersCritical** | (absent) | 0x60+S | **0x78** (lazy ptr 0x80, counter 0x88; Add DOES take it) | **NEW: 0x78, size 0x18** |
| PrimarySerialNumber | 0xB0 | 0x60+2S | 0x90 (`lock xadd` 0x480F51) | WRONG — correct 0x90 |
| bShouldRecycleObjectIndices | 0xB4 | 0x64+2S | 0x94 (FreeUObjectIndex, Deinitialize) | WRONG — correct 0x94 |
| UEP_TotalSize | 0xB8 | 0x68+2S | 0x98 (0x98+ are other globals) | WRONG — correct 0x98 |
| TUObjectArray sub-fields / FUObjectItem | 0x0..0x1C / 0x18 total | same | Objects+0x10abs, NumElements+0x24abs, item {0x0 Object, 0x8 Flags, 0x10 Serial}, stride 0x18 | CONFIRMED |

Root cause of the profile error: the l24 generator assumed
S=sizeof(FTransactionallySafeCriticalSection)=0x28 (pthread-by-value); the
shipping Prospero build uses the 0x18-byte lazy-mutex form. Every field after
ObjObjectsCritical is shifted by 0x10; everything from PrimarySerialNumber on
by 0x20.

## 4. Downstream consequence of the wrong 0x78 (the now-fully-symbolized crash mechanism)

The port's generated accessor (`MemberVariableLayout_SrcWrapper_FUObjectArray.hpp`,
pin 120=0x78) makes `Contains`/`Append`/`RemoveSingleSwap` treat the CRITICAL
SECTION as the TArray:

- "Data" = qword at GO+0x78 = the crit's fast-lock state (0 when unlocked;
  small/garbage otherwise — never a TArray buffer).
- "Num" = dword at GO+0x80 = LOW 32 bits of the lazily-allocated mutex pointer
  (0 until the first slow-path lock; afterwards a huge value, e.g. low32 of a
  0x22C0xxxxxxxx PS5 heap pointer).
- "Max" = dword at GO+0x84 = HIGH 32 bits of that pointer (0, or 0x22C0=8896).

Outcomes at registration (UnrealInitializer.cpp:528 / LuaMod.cpp:6896):
1. Contains dereferences the state-word as a pointer → NULL/near-NULL or wild
   read → immediate fault/stall. This is exactly agent G's observation that all
   five l24 arms stop AT the Add statement with nothing after it printed.
2. If Num(huge) ≥ Max(small) and growth runs: `GMalloc->Realloc(state-word as
   Data, …)` — a non-heap pointer → the engine allocator rejects/aborts.
3. Even a "clean" growth (state==0 ⇒ Realloc(NULL)=malloc succeeds) writes
   Data/Num/Max back into 0x78/0x80/0x84 — destroying the fast state and both
   halves of the mutex pointer of UObjectDeleteListenersCritical. The engine
   locks that crit on EVERY object destruction (dispatch 0x483FF0 via
   Deinitialize; GC purge) and unlocks via the corrupted state — detonating
   under save-load mass destruction (Z1: every crash followed a SaveGame
   mount/unmount), plausibly including the kernel `copyin: has nonsleeping
   lock` signature (engine thread faulting while holding the fast lock).

The profile's create value 0x68 is equally wrong (real 0x58) but is currently
latent: all three port listeners are DELETE listeners; no port code touches
UObjectCreateListeners today (Z8 §1.1).

Offset correction alone removes the crit-corruption/allocator-reject class,
but Z6's mechanism-B caveat stands: a direct unlocked Append onto the engine
TArray at the CORRECT 0x68 with our own allocator still races the engine's
locked dispatch. The engine-native alternatives now have concrete addresses:
`AddUObjectDeleteListener` @ image 0x4841A0 (runtime 0x8841A0 = 0x4841A0 +
0x400000) and `RemoveUObjectDeleteListener` @ image 0x484240 (runtime
0x884240), both taking rdi=&GUObjectArray (runtime 0x97BA110), rsi=Listener;
or lock the crit at
GO+0x78 (TryLock/slow-lock/unlock PLT pattern) around our own append.
(Reported only — no code changed.)

## 5. Provenance

- ELF/phdr/dynamic parsing, RIP-ref scan, call-target grouping, capstone
  disassembly, string decode: scratch scripts /tmp/l25-offset-verify/*.py;
  no writes outside /tmp and this file.
- Profile values read from the l24 tree (paths above); Track B flag from
  `outputs/2026-10-08-l23-layout-generic/NEW-VALUES-PLATFORM-DEPENDENT.json`
  and REPORT.md:53; vendored source from the l23 tree (sha 24d5bead… pinned).
- Every verdict row marked BINARY is backed by the cited function + instruction
  addresses in §2; none rests on [U] upstream knowledge.
