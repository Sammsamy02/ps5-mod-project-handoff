# Agent CJ — Variant-C extended to FName: call the engine's own FName find / ToString in the authenticated eboot

Date: 2026-10-08. READ-ONLY binary/source analysis. The only file written is
this report (scratch scripts under /tmp/l25-cj-fname/). Evidence tags:
**[V]** verified in vendored source / local artifacts, **[B]** verified in the
shipping binary bytes, **[U]** upstream knowledge (flagged, never
verdict-critical).

## 0. Inputs and method

- Binary: `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`,
  194,241,424 B, sha256 **3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc**
  (re-verified before analysis) **[B]**. ELF64 x86-64; exec PT_LOAD vaddr
  [0, 0x7479b1c) at file 0x4000; data/BSS PT_LOAD vaddr [0x913c000, 0x9633418);
  **runtime = image vaddr + 0x400000** (base 0x400000, same slide all prior
  agents use) **[B]**.
- Vendored UE 5.6.1 source: `outputs/2026-10-08-l23-layout-generic/official-source/Engine/`.
  **Scope caveat [V]:** the vendored subset contains
  `Core/Public/UObject/NameTypes.h`, `UnrealNames.h`, `UnrealNames.inl` but **NOT**
  `Core/Public/UObject/NamePool.h` and **NOT** `Core/Private/UObject/UnrealNames.cpp`
  (the task brief listed them; they are absent). All pool-internals statements
  below are therefore sourced from the BINARY (ground truth for this title) and
  marked [B]; upstream-shaped statements are marked [U] only where not
  byte-verified.
- Prior RE cross-checked, then independently re-derived from bytes:
  worker-l3-profile `game-profile.townfall-ps5-ue56-v1.json` FNamePool section,
  worker-re-fname-pool `FNAME-POOL-RE.md` (Tier-1 capstone), seed-table slot 2.
- Method: ELF phdr parse; whole-exec disp32 scan + capstone 5.0.7
  verification for RIP-relative refs (pool-global xref census = **49 refs in 23
  functions**, exactly matching worker-rea-trial's 49/49 Ghidra xrefs [V][B]);
  linear function-boundary recovery (cc-pad + `55 48 89 e5` prologues); full
  disassembly of the ctor chain, shard find/store, hash, and every
  ToString-family function; call-target census via e8-rel32 scan.
- Runtime port state read from
  `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
  and `native-coherent-runtime-1/source/port/unreal/{src,include}` [V].

## 1. Task 1 — FName/FNamePool public API surface (vendored source [V])

All from `Core/Public/UObject/NameTypes.h` (line numbers from the vendored file):

**String→name constructors (all CORE_API = outlined):**
- `FName(const WIDECHAR* Name, EFindName FindType = FNAME_Add)` — :1019
- `FName(const ANSICHAR* Name, EFindName FindType = FNAME_Add)` — :1020
- `FName(const UTF8CHAR* Name, EFindName FindType = FNAME_Add)` — :1021
- Length-known: `FName(int32 Len, const {WIDECHAR|ANSICHAR|UTF8CHAR}* Name, EFindName)` — :1024-1026
- `TStringView` ctors (:1028-1042) are **inline** and delegate to the
  length-known ctor.
- `FName(const TCHAR* Name, int32 InNumber, bool bSplitName)` — :1084.
- `EFindName` (:194-205): `FNAME_Find`=0 (**read-only**; miss → NAME_None),
  `FNAME_Add`=1 (**pool-mutating**; miss → allocate entry, insert into shard
  lookup). NOTE: **there is no `FName::FindExact` in this tree** (0 grep hits)
  — FNAME_Find *is* the exact-find entry point; Find-vs-Add is an argument,
  not a separate function.
- This build is `!WITH_CASE_PRESERVING_NAME && !UE_FNAME_OUTLINE_NUMBER`
  (proven binary-side in §2: 8-byte FName, Number@+4) — so FNAME_Find matches
  on the lowercased string part only, per :1196-1199 comments.

**ToString family (all CORE_API = outlined; :615-755):**
- `FString ToString() const` — :634 (allocating, sret).
- `void ToString(FWideString& Out) const` — :648; `void ToString(FUtf8String&)` — :649.
- `void ToString(FWideStringBuilderBase& Out)` / `(FUtf8StringBuilderBase&)` — :656-657.
- `uint32 ToStringInternal(TCHAR* Out, uint32 OutSize)` — :675 (private;
  **non-allocating**; wraps every public non-allocating form:
  `ToString(TCHAR(&)[N])` :703, `ToStringTruncate` :720-730 — all inline
  wrappers, one outlined core).
- `void AppendString(FWideString& Out) const` — :737; `AppendString(FUtf8String&)` :738;
  `AppendString(FWideStringBuilderBase&)` :745; `AppendString(FUtf8StringBuilderBase&)` :746;
  `TryAppendAnsiString(FAnsiStringBuilderBase&)` :755.
- `FString GetPlainNameString() const` — :615; number-stripping buffer form :618.
- `GetComparisonNameEntry()/GetDisplayNameEntry()` — :626-627.

**FNameEntryId** (:55-134): single `uint32 Value`; `FromUnstableInt` (:103);
EName bridges `FromValidEName` (:123, CORE_API — reads the EName→id table,
which lives at pool+0x14040 in this build, see §2). Internal number is
external+1 (`NAME_INTERNAL_TO_EXTERNAL` :152) — byte-confirmed below.

**FNamePool internals** (NamePool.h/cpp — NOT vendored [V]): the binary
provides the layout instead (§2): EntryCount-ish head, CurrentBlock u32 +0x8,
CurrentByteOffset u32 +0xC, `Blocks[8192]` +0x10, 256 shard descriptors × 0x40
at +0x10040 (each: RW-lock + mask u32 +0xC + entries ptr +0x10 + allocator ptr
+0x18), EName id table at +0x14040. Pool-wide find = per-shard find with
ReadLock; Store = per-shard WriteLock + block allocation. These are the
"Find (read-only)" vs "Add (pool-mutating)" split requested — confirmed at
byte level in §2/§3.

## 2. Task 2 — Binary locations (all [B]; image vaddr first, runtime = +0x400000)

### 2a. The internal pool find used by the string→name ctor

Chain re-derived end-to-end from bytes (every step below disassembled):

| Function | Image | Runtime | Role / identification evidence |
|---|---|---|---|
| `FName::FName(const WIDECHAR*, EFindName)` | **0x23D590** | 0x63D590 | Seed slot 2 ctor [V]. Prologue `554889e553504889f04889fb4885f674` re-verified [B]. OR-reduces wchars (NAME_WIDE bit 32), bextr length, passes `(rdi=this, rsi=name, edx=FindType)` → 0x23D600; `mov [rbx],rax` stores the 8-byte result. |
| ctor internal (view/lowercase/suffix) | 0x23D600 | 0x63D600 | 0x410 stack buffer, SSE narrow+lowercase, trailing `_NNN` parse into r14 (number, `+1` internal), `call 0x242470`, then `shl r14,0x20; mov eax,eax; or rax,r14` — applies the number AFTER the core returns. |
| **core FindOrStore dispatcher** | **0x242470** | 0x642470 | len≥0x400: FNAME_Find→return None; FNAME_Add→self-recursive with literal `"ERROR_NAME_SIZE_EXCEEDED"` (ASCII @0x7ACB255, len 0x18) [B]. Lazy pool init guard `cmp byte [pool-0x20]`. `edx==1` → **Store 0x23C0B0**; else hash+shard find. |
| hash ANSICHAR / WIDECHAR | 0x239B00 / 0x23A2C0 | 0x639B00 / 0x63A2C0 | Pure (stack canary + arithmetic only; len≤0x400 assert branch); return hash in rax, probe/header material in rdx. |
| **shard find (THE internal find)** | **0x23C340** | 0x63C340 | `(rdi=shard, rsi=key)`. **ReadLock PLT 0x7472960 at entry, ReadUnlock PLT 0x74728D0 at exit.** Linear-probe `bucket=key&[shard+0xC]` over `u32` handles at `[shard+0x10]`; `bextract 0xd10` splits block (13 bits)/offset (16 bits); `Blocks[block]` via `[shard+0x18]`; header compare + `memcmp 0x8CDB0` (ANSI)/`0x8CE20` (WIDE); hit → `handle & 0x1FFFFFFF` (29-bit FNameEntryId); empty bucket → 0 (NAME_None). Sole caller: 0x242550 (the core) [B]. |
| Store (Add path) | 0x23C0B0 | 0x63C0B0 | hash → **WriteLock PLT 0x74728C0** → probe → on miss **entry allocation 0x246090** (block reserve, `Malloc(0x20000,2)` per FNAME-POOL-RE [V]) → memmove name in → lookup insert 0x243850 → **Unlock 0x74728D0**. |
| numbered store | 0x242EF0 | 0x642EF0 | number-bits shuffle (`shr 0x27; and 0x3e`), 4 WriteLock/Unlock pairs, 0x246090 ×2 — the CreateNumberedName store side. |
| pool Init (lazy) | 0x23A5D0 | 0x63A5D0 | memset(pool+8,0,0x10008); `Blocks[0]=Malloc(0x20000,2)`; stores flag byte at pool-0x20=1 [V][B refs]. |
| bounds getter | 0x23D570 | 0x63D570 | `CurrentBlock+1 > (id>>16)` — reads pool+8; the engine's own id-bounds check (254 call sites). |
| **FNamePool global** | **0x9365EC0** | 0x9765EC0 | 49 rip-disp refs in 23 functions [B], matching REA's 49/49 [V]; init-flag byte at 0x9365EA0; EName id array at pool+0x14040 = image 0x9379F00 = runtime 0x9779F00 (None@+0, ByteProperty@+4 — matches the l24 `[Seeds][L14]` None-id/ByteProperty-id checks and `REGISTER_NAME(0,None)/(1,ByteProperty)` in UnrealNames.inl [V]). |

The "seed-slot technique" cross-check requested: GUObjectArray (slot 0, image
0x93BA110) is 1,565 code refs away in CoreUObject [V:AA]; the FNamePool global
0x9365EC0 sits in the same LOAD7 BSS and its 49 xrefs cluster in
[0x239090, 0x242EF0] — the Core NamePool/UnrealNames code, and the runtime
seeds already pin the pool via `base + kFNamePoolLabel` [V:
UnrealInitializer.cpp:1138 + live `[Seeds]` L14 checks at 0x9765EC8/0x9765ECC/0x9765ED0
= CurrentBlock/CurrentByteOffset/Blocks[0]].

**Which find does the engine itself use when script/BP resolves
"GetItemIDFromDeprecatedEnum"?** Exactly this chain: `FName(const WIDECHAR*,
EFindName)` → 0x23D600 (lowercase+hash key) → 0x242470 → 0x23C340 shard find
under ReadLock. There is one find, not several.

### 2b. ToString/AppendString family (all [B], identified by structure + call shapes)

| Function | Image | Runtime | Signature (SysV) | Identification |
|---|---|---|---|---|
| **`FName::ToString(FWideString& Out)`** (number-aware) | **0x23F150** | 0x63F150 | `(rdi=this, rsi=&Out)` | Resolves entry via pool Blocks (no lock), `Number=[this+4]`; ANSI-widen or WIDE copy to stack, `Reset/Reserve 0x64820`, assign `0x64B80`, **`mov esi,0x5f; call 0x648A0` (AppendChar '_')**, **`dec esi; call 0x69010` (AppendInt Number-1 = internal→external)**. 33 callers incl. 0x23F0DE. |
| `FName::ToString() → FString` (sret) | 0x23F0C0 | 0x63F0C0 | `(rdi=&ret, rsi=this)` | Number==0 → direct entry→FString assign via 0x642C0/0x64410; else zeroes out-param and delegates to 0x23F150. 5k+ byte-level callers — the hot allocating ToString. |
| **`FName::ToStringInternal(TCHAR*, uint32)`** (non-allocating) | **0x23FA00** | 0x63FA00 | `(rdi=this, rsi=Out, edx=OutSize) → eax=len` | `r13d=OutSize-1`, len=`header>>6`, **`cmovb r15d,r12d` = truncate to buffer**, ANSI in-place widen (overlapping-shift dance) or WIDE memmove; 27 callers. This is the outlined core behind every `ToStringTruncate`/array form. |
| `FName::GetPlainNameString() → FString` (sret) | 0x23EF60 | 0x63EF60 | `(rdi=&ret, rsi=this)` | entry→FString, no number handling. |
| `AppendString`-ANSI-builder variant | 0x23F8D0 | 0x63F8D0 | `(rdi=this, rsi=&builder)` | appends via 0x23CDB0 then `mov byte [rax],0x5f` + AppendInt 0x204310 (builder {pos@+8,end@+0x10}). |
| UTF8 sibling of 0x23F150 | 0x23FC30 | 0x63FC30 | `(rdi=this, rsi=&Out)` | same shape, UTF8 out-helpers. |

FString object layout byte-pinned by 0x64820: `{data ptr@0, Num u32@8, Max
u32@0xC}` = 16 B — **layout-identical to the port's `FString`/`FStringOut`
({AllocatorInstance ptr, ArrayNum, ArrayMax}, int32 SizeType) [V:
port Array.hpp:762-763]**.

Misc identifications: the ubiquitous `[0x9131F08]` load+snapshot+compare in
~139k functions is `__stack_chk_guard` via GOT (file value 0, dynamic
relocation fills it; mismatch → PLT 0x7471E60 + ud2 = `__stack_chk_fail`) —
NOT a name-pool guard; ambient Prospero instrumentation, present in the ctor
wrapper/core exactly as everywhere else [B].

## 3. Task 3 — Call feasibility from the runtime thread

**FName ctor / FNAME_Find path (0x23D590 → … → 0x23C340):**
- **Locks:** one per-shard RW **Read** lock (PLT 0x7472960 in, 0x74728D0 out;
  WriteLock is 0x74728C0, used only by Store). Read-locked, probe, unlocked —
  identical protection to every engine worker thread's concurrent Finds. No
  lock ordering exposure: we hold nothing while calling [B].
- **Allocation on Find-hit: NONE.** The Find branch executes hash (pure),
  ReadLock, table+entry reads, memcmp, ReadUnlock. Allocation exists only in
  (a) the Store branch (0x246090 block reserve) and (b) the lazy-init guard
  (`byte [pool-0x20]==0` → Init). At mod runtime the pool is long-initialized
  (the game is in-menu/in-game; the seeds' own L14 checks read live pool words),
  so neither can fire from a Find call [B].
- **Thread-safety:** this is the engine's own concurrent path (async-loading
  threads, game thread, task-graph threads all call the ctor; 14k byte-level
  call sites). Entries are immutable once published; Blocks append-only; the
  shard lock covers the lookup table only. Calling from our thread is
  indistinguishable from any engine worker thread.
- **FNAME_Find-miss semantics:** returns NAME_None (id 0 / number 0) — clean,
  detectable, no mutation (byte-verified: `xor eax,eax; xor ecx,ecx` fast path).
- **PROVEN IN-GAME:** this exact call is already seed-bound (slot 2, label
  0x23D590) and behaviorally verified via the ByteProperty probe with
  `FNAME_Find` (probe id == pool id, UTF-16 input) —
  `UnrealInitializer.cpp:1177-1224`, live `[Seeds] slot 2 … behaviorally
  verified` [V]. The engine-call option is not theoretical; half of it ships.

**ToString family:**
- 0x23F150 `ToString(FWideString&)`: lock-free pool reads (engine ToString
  never locks — entries immutable); **allocation only through the caller's Out
  object**: growth goes 0x64820 → 0x25E20 (engine TArray growth → engine
  GMalloc). The port's FStringOut is layout-identical, and the port's
  FMemory::* are virtual wrappers on the SAME engine GMalloc (slot 3 validated
  0x970add8→0x97089b8 in-game [V: seeds log; port FMemory.cpp]) — engine-grown
  buffer, port-freed consistently. This is the classic UE4SS scan-method
  binding, transplanted to a seed label.
- 0x23FA00 `ToStringInternal(TCHAR*, uint32)`: **zero allocator involvement** —
  writes into a caller stack buffer (NAME_SIZE+11 = 1035 TCHARs guarantees no
  truncation for legal names), returns length. Strictly safer than 0x23F150.
- 0x23F0C0 `ToString()→FString` sret: feasible but adds the sret convention and
  an engine-owned FString the caller must destruct via engine Free.

## 4. Task 4 — Comparison against the port's current machinery

Port's current state [V]:
- **String→FName:** `FName::construct_with_string` (NameTypes.hpp:239-255)
  already calls `ConstructorInternal` — bound since l24 to `native_fname_thunk`
  → `g_native_fname_ctor` = engine ctor @0x63D590 with explicit FNAME_Find
  guard comments (UnrealInitializer.cpp:1174-1176). **The port does NOT
  reimplement find; it already calls the engine.** Its residual hand-rolled
  machinery is only the pool VALIDATION side (L14 admission checks,
  NativePropertyGuard bounds/suffix tests) — read-only diagnostics.
- **FName→string:** `ToStringInternalWrapper` (NameTypes.cpp:110-142): the
  "Scan" route binds an engine ToString via signature scan (NOT armed under
  seeds — `fname_tostring` is deliberately unbound), so production falls back
  to `UKismetStringLibrary::Conv_NameToString` via `ProcessEvent` on the Kismet
  CDO — a full BP virtual call with a param frame per conversion.

Is the engine Find strictly better than fixing our own scan? **Yes, by
construction**: the id the engine's shard find returns for lowercased
"getitemidfromdeprecatedenum" IS the id the engine stored when the UFunction's
FName was created (same hash, same lowercase transform — byte-identical code
path 0x23D600→0x23C340). Any hand-rolled scan must reproduce: the lowercase
ANSI-ization of wide input, the probe-hash/tag prefilter (`& 0xE0000000` vs
key tag), 29-bit id masking, per-shard mask, linear probing, header Len>>6
with !CASE_PRESERVING bit layout, and the internal+1 number encoding — every
one of which is a drift surface across titles. A mismatch in ANY of them is
precisely the "comparison-index mismatch" hypothesis in
UFUNCTION-NULLPTR-FINDING.md. Calling the ctor eliminates the entire class.

Risks:
- **Signature drift / label pinning:** addresses are pinned to sha 3b6cfad4;
  re-derive per title (same discipline as every other seed slot). The ctor
  slot already carries identity gating (class = CodeCallOnly + 32-byte
  prologue prefix `554889e5...` + behavioral ByteProperty probe) — replicate
  that pattern for any ToString slot bind.
- **Inlining:** not a concern — every function above is outlined and
  multiply-instantiated-called: ctor 9 verified call sites (fc1 three-way
  re-verified [V]) + 14,401 byte-level e8 upper bound [B]; ToStringInternal 27
  callers; ToString(FString) 5k+; shard find is a discrete function (its only
  caller is the core). Nothing in this family is dead-stripped (unlike Z6's
  diagnostics family) — the engine itself is the heaviest user.
- **Edx convention:** FindType rides in edx unchanged through
  0x23D590→0x23D600→0x242470 (byte-verified; engine callers materialize
  `mov edx,1`) — pass 0 for Find, never 1 (Add mutates the pool = GATE-1
  family; stay out).
- **Number encoding:** returned Number is internal (+1); the port thunk already
  handles the (index, number) repack; ToString variants apply `dec` before
  printing — consistent pair.
- **Kismet fallback is not immediately obsolete:** keep it as fallback if the
  ToString slot bind fails its admission checks.

## 5. Task 5 — VERDICT: ENGINE-CALL VIABLE (both halves located; one half already proven in-game)

**Per-function verdicts (found? / address image+runtime / call-feasible):**

| Function | Found | Image / Runtime | Call-feasible |
|---|---|---|---|
| FName(const WIDECHAR*, EFindName) ctor | YES | 0x23D590 / 0x63D590 | **YES — already seed-bound & in-game verified (FNAME_Find, ByteProperty probe)** |
| ctor internal (lowercase/suffix) | YES | 0x23D600 / 0x63D600 | via ctor only (not externally callable — applies number post-hoc) |
| core FindOrStore dispatcher | YES | 0x242470 / 0x642470 | via ctor only |
| shard find (internal FName-pool find) | YES | 0x23C340 / 0x63C340 | NOT directly (key-struct ABI); reached via ctor — ReadLock/ReadUnlock, allocation-free on hit |
| shard store (Add) | YES | 0x23C0B0 / 0x63C0B0 | feasible but DO NOT (WriteLock + allocation = pool mutation) |
| FName::ToString(FWideString&) | YES | 0x23F150 / 0x63F150 | YES (engine-allocator growth of layout-identical FStringOut) |
| FName::ToStringInternal(TCHAR*,uint32) | YES | 0x23FA00 / 0x63FA00 | **YES — zero allocation, recommended** |
| FName::ToString()→FString (sret) | YES | 0x23F0C0 / 0x63F0C0 | YES (sret + engine-owned FString) |
| FName::GetPlainNameString() (sret) | YES | 0x23EF60 / 0x63EF60 | YES (sret) |
| ANSI ctor variant | YES | 0x23D8A0 / 0x63D8A0 | YES (strlen-based; secondary) |

**Recommended fix route (in order):**
1. **Keep/require the ctor call (slot 2) with `FNAME_Find` for every
   runtime-constructed name** — including the Lua UFunction-name resolution
   path. It is the engine's own find; a hit returns the exact stored
   ComparisonIndex; a miss returns NAME_None without mutating the pool. If the
   nullptr reproduces with the ctor correctly invoked, the bug is NOT in name
   construction — look downstream (function-hash resolution / ToString-based
   comparisons, next item).
2. **Bind `FName::ToStringInternal` @0x63FA00** (primary) or
   `ToString(FWideString&)` @0x63F150 (if an FStringOut-shaped binding is
   preferred) as the engine ToString route, displacing the per-call Kismet
   `ProcessEvent` fallback: same admission discipline as slot 2
   (CodeCallOnly class + prologue prefix + behavioral probe — probe e.g.
   ToString(NAME_ByteProperty) must yield "ByteProperty", mirroring the ctor
   probe's pool cross-check). This removes the ProcessEvent dependency from
   the name-comparison hot path that the UFUNCTION-NULLPTR-FINDING analysis
   suspects ("comparison-index mismatch … ToString did not verify").
3. **Never call Store/Add (0x23C0B0/0x242EF0) or use FNAME_Add** — that is
   pool mutation (the GATE-1 family), unnecessary for existing names and
   unsafe from our thread by policy.
4. Fallback if a ToString slot bind is ever refused: keep the Kismet fallback
   (it works; it is only slow) — there is no need to hand-roll a pool scan;
   the engine's semantics (this document §2a) are the spec should a scan ever
   become unavoidable.

Caveats: labels valid for sha 3b6cfad4 only; runtime slide 0x400000;
!CASE_PRESERVING/!OUTLINE_NUMBER configuration (8-byte FName) is baked into
every offset above — re-derive for any title with different config flags.

## 6. Provenance

- Binary sha re-verified at analysis start; all disassembly via capstone 5.0.7
  from /tmp/l25-cj-fname scratch scripts (elf.py/cjdis.py/xref.py/cluster.py/
  calls.py/sweep.py/idfy.py); no writes outside /tmp and this report.
- Vendored-tree line numbers read directly from the l23 tree [V].
- Prior-RE cross-references (profile json, FNAME-POOL-RE.md, FNAME-VERDICT.md,
  agent-AA/AE/Z6 reports, l26 candidate sources) cited inline; every
  load-bearing claim re-derived from bytes unless tagged [V]/[U].
