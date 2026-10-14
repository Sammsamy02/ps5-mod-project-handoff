# Agent CT — UObjectBase / FUObjectItem member offsets binary-verified from ENGINE CODE in the shipping eboot

Date: 2026-10-08. READ-ONLY analysis; this report is the only file written
(scratch scripts under /tmp/l26-ct/). Second pincer on the walk-finds-nothing
failure: names resolve, walk runs, zero results for every class.

## 0. Inputs and method

- Binary: `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`
  — 194,241,424 B, sha256 re-verified this session:
  `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`.
  ELF64 x86-64 (SCE_RELEXEC). Phdr map re-parsed independently, matches AA:
  exec PT_LOAD vaddr [0, 0x7479b1c) at file 0x4000; data/BSS PT_LOAD vaddr
  [0x913c000, 0x9633418). **Image vaddr + 0x400000 = runtime address.**
- GUObjectArray anchor: image vaddr **0x93BA110** (runtime 0x97BA110), from
  AA; my own RIP-lea rescan reproduces AA's histogram exactly (1565 refs to
  GO+0x00; next globals at GO+0x98/GO+0xa8 = hash tables, not members).
- Vendored UE 5.6.1 headers: `outputs/2026-10-08-l23-layout-generic/official-source/Engine/Source/Runtime/CoreUObject/Public/UObject/{UObjectBase.h,ObjectMacros.h,UObjectArray.h}` [V].
- l26 profile rows: `outputs/2026-10-08-l26-runtime/profile/profiles/townfall-build-facts.json`
  (`required_members`) [V].
- Method = AA technique: locate engine functions that demonstrably access the
  members, capstone-disassemble, read the displacements. No symbols needed;
  functions identified by structure + behavior, anchored on AA's already
  string/fingerprint-identified functions (FreeUObjectIndex 0x4840B0, serial
  bump 0x480F10, GC purge 0x373600, Deinitialize-family 0x3C92E8+, find-fast
  0x4ACExx).

## 1. Headline

**Every l26 profile row for the target members matches the binary. No offset
mismatch found.** UObjectBase {0x8, 0xC, 0x10, 0x18, 0x20}, FUObjectItem
{Object 0x0, Flags 0x8, stride 0x18}, UClass::ClassCastFlags 0xD8 — all
CONFIRMED from engine code in the authenticated image. Two semantic
(non-offset) findings matter for the walk-failure analysis (§5):
FUObjectItem::SerialNumber is at **+0x10** (not the pre-5.6 +0xC), and
**RF_Unreachable does not exist in 5.6.1 EObjectFlags** — Unreachable is
EInternalObjectFlags 1<<28 stored in FUObjectItem::Flags (which is exactly
where CO's walk reads it — that part of the port is correct).

## 2. UObjectBase — binary evidence [B]

Best witness: the StaticFindObjectFast-class hash lookup (image 0x4ACE00
region; uses GO+0x10 Objects table, GO+0x24 NumElements, ×0x18 item stride,
then on each candidate object):

```
0x4ace7a: cmp qword ptr [r13 + 0x18], r12   ; NamePrivate  (full 8-byte FName)
0x4ace84: mov r10d, dword ptr [r13 + 8]     ; ObjectFlags  (32-bit)
0x4ace88: test dword ptr [rbp + 0x10], r10d ; flag-mask test
0x4ace92: cmp qword ptr [r13 + 0x20], rbx   ; OuterPrivate
0x4acea9: mov r10, qword ptr [r13 + 0x10]   ; ClassPrivate
0x4aceaf: cmp r10, qword ptr [rbp - 0x98]   ; (ExactClass compare)
```

One function, four members. InternalIndex (+0xC) witnesses:

- `0x4840bb: mov ebx, dword ptr [rsi + 0xc]` in FUObjectArray::FreeUObjectIndex
  (AA anchor re-disassembled this session, assert line 0x17d=381 intact).
- `0x484145: mov dword ptr [r15 + 0xc], 0xffffffff` — the WRITE of INDEX_NONE
  to InternalIndex at object+0xC on free.
- Weak-ptr resolvers 0x4b6b91/0x480e46/0x4817… all `mov e??, dword ptr [obj + 0xc]`.

Object teardown family 0x3C9310 (UObjectBase::EndDestroy-equivalent; the
function whose tail calls the AA delete-notify dispatch at 0x3C92E8) reads in
one body: `[rbx+8]` ObjectFlags dword (tests 0x1000 RF_NeedPostLoad,
0x2000 RF_NeedPostLoadSubobjects, 0x3400 combo, 0x40000000 RF_MirroredGarbage,
`test al,0x10` RF_ClassDefaultObject), `[rbx+0xC]` InternalIndex → GO chunked
item, item flags, and on CDO `mov rdi,[rbx+0x10]` (ClassPrivate) → virtual
`[vtable+0x3C8]`. Serialization walk 0x3C9A99 region reads `[r14+0x10]`
ClassPrivate, `[r14+0x18]` NamePrivate, `[r14+0x20]` OuterPrivate together.

UE_STORE_OBJECT_LIST_INTERNAL_INDEX (UObjectBase.h:37 defaults 0) is 0 in this
build: OuterPrivate is compared at +0x20 (above); had it been 1, OuterPrivate
would sit at +0x28. The "Class" ZConstructor (§4) initializes UField::Next at
**[rbx+0x28]**, i.e. UObjectBase ends at exactly 0x28.

## 3. FUObjectItem — binary evidence [B]

- Stride **0x18**: `movzx ecx,bx; lea r12,[rcx+rcx*2]; shl r12d,3; add r12,[rdx+rax*8]`
  (idx*24, high16=chunk, 65536-item chunks) at 0x4840cd and dozens of sites;
  FreeUObjectIndex zeroes the whole item with `vmovups [r12],xmm0` (16B) +
  `mov qword [r12+0x10],0` (8B) = 24 bytes.
- **Object +0x0**, full 8-byte pointer (UE_ENABLE_FUOBJECT_ITEM_PACKING off):
  `mov r9, qword ptr [r12]` + identity check vs the freed object (0x4840e1).
- **Flags +0x8**, dword, EInternalObjectFlags (NOT EObjectFlags):
  - GC reachability 0x3740d0: `mov eax,[rbx+8]; test eax,0x10000000; …
    lock cmpxchg dword ptr [rbx+8], ecx` — atomically SETS
    EInternalObjectFlags::Unreachable (1<<28) on item.Flags.
  - Teardown 0x3C93cf: same dword load and `test eax,0x10000000`.
  - FreeUObjectIndex 0x484118: `movzx eax,word ptr [r12+0xa]; test eax,0x6e50`
    — the high word of Flags (item+0xA) & 0x6E50 = dword mask 0x6E500000 =
    LoaderImport|AsyncLoadingPhase1|Native|Async|AsyncLoadingPhase2|RefCounted|RootSet
    — bit-for-bit EInternalObjectFlags_RootFlags of the vendored enum.
- **SerialNumber +0x10** (dword, atomically CASed):
  `mov eax,dword ptr [r14+0x10]; … lock xadd dword ptr [rdi+0x90],eax` (PrimarySerialNumber
  GO+0x90) `; cmp eax,0x3e7` (=999 = START_SERIAL_NUMBER−1) `; lock cmpxchg
  dword ptr [r14+0x10],ebx` — serial-bump helper 0x480F10 + two inlined copies
  (0x480E86, 0x48176F). Vendored 5.6.1 member order confirms: Object(+0),
  Flags(+8), ClusterRootIndex(+0xC), SerialNumber(+0x10), RefCount(+0x14).
  **This is NOT the pre-5.6 layout (SerialNumber was +0xC with ClusterRootIndex
  +0x10); +0xC in this build is ClusterRootIndex.**

## 4. UClass::ClassCastFlags / IsA chain — binary evidence [B]

- **ClassCastFlags +0xD8 (qword)** — two independent witnesses:
  1. Super-inheritance function 0x322800 (the "Assemble/CopyCastFlags-from-super"):
     `mov rax,[rbx+0x40]` (**UStruct::SuperStruct +0x40**) `; test rax,rax; je;
     mov ecx,0x4aa3875e; and ecx,dword ptr [rax+0xd4]; or dword ptr [rbx+0xd4],ecx;
     mov rax,qword ptr [rax+0xd8]; or qword ptr [rbx+0xd8],rax`
     — ORs super's ClassCastFlags (+0xD8, qword) into self, and the EClassFlags
     dword (+0xD4) with the inherited-flags mask.
  2. The **UClass self-class ZConstructor** at 0x3264C0 (constructs the object
     named **"Class"** — UTF-16 string at 0x76ae74e verified):
     `mov esi,0x200` (**sizeof(UClass)=512**, = profile UEP_TotalSize), then
     `mov dword ptr [rbx+0xd4],0x10000080` (EClassFlags) and
     **`mov qword ptr [rbx+0xd8],0x20`** — CASTCLASS_UClass = 0x20 in THIS
     build. Also initializes UStruct members: SuperStruct [rbx+0x40],
     Children [rbx+0x48], ChildProperties [rbx+0x50], PropertiesSize
     dword [rbx+0x58]=0x200, MinAlignment word [rbx+0x5c]=8 — all matching the
     l26 profile UStruct rows (64/72/80/88/92). UField::Next zeroed at
     [rbx+0x28] (= profile 40).
- **Canonical 'UClass' comparison (what IsA<UClass> needs)**:
  - The UClass self-object is a singleton cached in the BSS/data global at
    image **0x939e550** (runtime 0xD39E550): `mov rax,[0x939e550]; test rax,rax;
    jne done` at 0x3264df, write-back at 0x326788 — only these two refs (the
    ZConstruct itself), i.e. other engine call sites do not reload it via
    direct RIP refs (they go through the constructor chain or compare
    differently).
  - Its vtable constant is the qword at image **0x7ed7a28** (runtime 0xBED7A28)
    (lea'd at 0x326531, stored as [obj+0] of the self class).
  - No SuperStruct-walk IsA loops (`mov rA,[rA+0x40]` self-walk: 12 sites,
    none UObject) and no `[class+0xD8]&imm` direct tests survive in the
    shipping image — consistent with UE 5.6 compiling IsA<T> to the
    ClassCastFlags fast path / inlined pointer compares that were
    dead-stripped or folded. The full-fidelity fallback chain in this binary
    is: obj+0x10 (ClassPrivate) → class+0x40 (SuperStruct) walk, terminating
    compare against the 0x939e550 singleton; the fast path is
    qword [class+0xD8] & 0x20 (CASTCLASS_UClass=0x20 — note: not the stock-UE
    public enum ordering; the game's generated enum assigns UClass bit 5).
    The vendored tree does not contain the generated EClassCastFlags numerics
    [U]; the binary value 0x20 is authoritative and matches l26's
    UClass.ClassCastFlags=0xd8 evidence window ("CASTCLASS_UClass20").

## 5. EObjectFlags / EInternalObjectFlags bit positions [V+B]

Vendored ObjectMacros.h:540-601 (complete enum read this session):
RF_Public=1, RF_Standalone=2, RF_MarkAsNative=4, RF_Transactional=8,
**RF_ClassDefaultObject=0x10**, **RF_ArchetypeObject=0x20**, RF_Transient=0x40,
RF_MarkAsRootSet=0x80, RF_TagGarbageTemp=0x100, RF_NeedInitialization=0x200,
RF_NeedLoad=0x400, RF_NeedPostLoad=0x1000, RF_NeedPostLoadSubobjects=0x2000,
… RF_MirroredGarbage=0x40000000, RF_AllocatedInSharedPage=0x80000000.
**There is no RF_Unreachable in 5.6.1 EObjectFlags.** Unreachable=1<<28 lives
in EInternalObjectFlags (ObjectMacros.h:616-644: Garbage=1<<21, Native=1<<25,
Async=1<<26, AsyncLoadingPhase2=1<<27, **Unreachable=1<<28**, RefCounted=1<<29,
RootSet=1<<30, PendingConstruction=1<<31).

Binary corroboration on ObjectFlags at obj+0x8 (32-bit): RF_NeedPostLoad
0x1000 (0x3C9329 etc.), RF_NeedPostLoadSubobjects 0x2000 (0x3C9369),
0x3400 combo, RF_MirroredGarbage 0x40000000 (0x3C939E),
**RF_ClassDefaultObject 0x10** (`test al,0x10` at 0x3C93E8 → CDO path takes
`[rbx+0x10]` ClassPrivate → virtual 0x3C8; also 0x3C9AD7, 0x3C9C67, 0x3C9D29),
**RF_ArchetypeObject 0x20 as bit 5** (`and edx,0x20; shr edx,5` at 0x3C9C7E in
the same object-counting function that reads ClassPrivate+0x10; `test eax,5`
RF_Public|RF_MarkAsNative beside it). Internal flags on item+0x8: Unreachable
0x10000000 (set + test, §3), RootFlags 0x6E500000 (§3). Bit-for-bit agreement
with the vendored enum; no console drift found.

## 6. Three-way table (profile values from l26 townfall-build-facts.json required_members)

| Member | Profile (l26) | Binary (engine code) | Verdict |
|---|---|---|---|
| UObjectBase::ObjectFlags | 8 (w4) | +0x8, dword (0x4ace84; 0x3C9326 …) | CONFIRMED |
| UObjectBase::InternalIndex_Private | 12 (w4) | +0xC read (0x4840bb) + write −1 (0x484145) | CONFIRMED |
| UObjectBase::ClassPrivate | 16 (w8) | +0x10 (0x4acea9; 0x3C9407; 0x3C9C73) | CONFIRMED |
| UObjectBase::NamePrivate | 24 (w8) | +0x18, 8-byte FName compare (0x4ace7a) | CONFIRMED |
| UObjectBase::OuterPrivate | 32 (w8) | +0x20 (0x4ace92; 0x3C9AA9); cond. member absent | CONFIRMED |
| UObjectBase::UEP_TotalSize | 40 | ends at 0x28 (UField::Next init [rbx+0x28], 0x326551) | CONFIRMED |
| FUObjectItem::Object | 0 (w8) | +0x0, full qword, packing OFF (0x4840e1) | CONFIRMED |
| FUObjectItem::Flags | 8 (w4) | +0x8, dword EInternalObjectFlags (0x3740d0; 0x484118) | CONFIRMED |
| FUObjectItem::SerialNumber | (no profile row) | **+0x10** (lock cmpxchg 0x480F65; +0xC is ClusterRootIndex) | NEW FACT (no mismatch — row absent) |
| FUObjectItem::UEP_TotalSize / stride | 24 | 0x18 stride everywhere; item zeroed 16+8 B | CONFIRMED |
| RF_ClassDefaultObject | (CO port 0x10) | 0x10 at ObjectFlags+8 (0x3C93E8 …) | CONFIRMED |
| RF_ArchetypeObject | (CO port 0x20) | 0x20 bit 5 at ObjectFlags+8 (0x3C9C7E) | CONFIRMED |
| RF_Unreachable | (CO uses item-Flags 1<<28) | NOT an EObjectFlags bit in 5.6.1; Unreachable=1<<28 is EInternalObjectFlags, stored at item+0x8 (0x3740d0/0x3C93D2) | CONFIRMED (CO's placement correct) |
| UClass::ClassCastFlags | 216 (0xD8, w8) | +0xD8 qword (0x322828-0x32282F; init 0x3265D9) | CONFIRMED |
| UClass::UEP_TotalSize | 512 | 0x200 passed to allocator (0x3264F6) | CONFIRMED |
| UStruct::SuperStruct (IsA chain) | 64 (0x40) | +0x40 (0x32280E; ctor 0x32655D) | CONFIRMED |
| UClass canonical self pointer | (port resolves at runtime) | singleton global image 0x939e550 (runtime 0xD39E550); vtable const 0x7ed7a28; name "Class"; ClassCastFlags=0x20 (CASTCLASS_UClass) | NEW FACT |

TUObjectArray sub-rows re-confirmed in passing: ObjObjects at GO+0x10 with
NumElements GO+0x24 (0x4ACE2F/0x4ACE29), 65536-item chunks, high16/low16 index
split — the walk's container plumbing offsets are right.

## 7. Implications for walk-finds-nothing (reported only, no code changed)

1. The walk's member offsets are NOT the root cause: every offset and bit the
   CO walk uses (item+0x8 Flags, 1<<28 Unreachable, obj+0x8 bits 0x30,
   obj+0x10 ClassPrivate, obj+0x18 NamePrivate, ×0x18 stride, GO+0x10/+0x24)
   is binary-correct. The AA precedent (profile wrong because of a
   platform-dependent lock size) does NOT repeat here — the l26 rows for these
   members are right.
2. If any port component assumes the PRE-5.6 FUObjectItem layout
   (SerialNumber at +0xC), it reads ClusterRootIndex (usually 0/INDEX_NONE)
   as the serial and +0x10 serial as whatever it expects at +0x10 — harmless
   for FindFirstOf-style walks (they don't use serials) but corrupting for
   weak-pointer caching.
3. If any port code tests "RF_Unreachable" (0x10000000) against
   ObjectFlags at obj+0x8, it is actually testing
   **RF_HasExternalPackage** in 5.6.1 — objects with external packages
   (packages/inmost-outer machinery) would be wrongly excluded. CO's audit
   says the walk reads the bit from item.Flags — correct — but any other
   consumer should be checked (CO also flags port PendingKill=1<<29 vs
   binary RefCounted=1<<29 in FUObjectItem::Flags).
4. IsA<UClass> should compare obj->ClassPrivate against the engine singleton
   (image 0x939e550 → runtime 0xD39E550 holds/points-to the "Class" object)
   or test qword [obj->ClassPrivate+0xD8] & 0x20 — NOT a stock-UE cast-flag
   constant (this game's CASTCLASS_UClass is 0x20, not the public-enum-order
   value), and there is no RF_…-style drift: ClassCastFlags at +0xD8 is
   binary-proven.

## 8. Provenance

- ELF/phdr parse, RIP-reference scans, byte-pattern scans, and capstone
  disassembly performed this session from /tmp/l26-ct/ against the
  sha-verified image; AA anchors re-verified before use (FreeUObjectIndex,
  serial bump, GC purge, GO histogram all reproduce).
- Profile values read from the l26 units/profile files cited in §0; vendored
  headers read at the cited lines; no other files read or written beyond
  this report.
- All verdicts marked [B] rest on the cited instruction addresses (image
  vaddr; add 0x400000 for runtime).
