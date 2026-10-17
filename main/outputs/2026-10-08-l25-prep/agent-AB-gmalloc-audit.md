# Agent AB — GMalloc usage audit (Claude review item 2, Mac-only)

Date: 2026-10-08. READ-ONLY on all inputs (runtime sources, eboot, l24 object/PRX,
vendored UE tree, session logs). Scratch work in /tmp only; this report is the sole
file written. All eboot "runtime" addresses assume the klog-observed module base
0x400000; "vaddr" = file-virtual address (runtime − 0x400000). Binary analyzed:
`outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`
(sha256 `3b6cfad4…`, same image Z6/Z11 calibrated against).

## 0. Headline verdicts

1. **Variable-vs-cached: the runtime reads the game's GMalloc VARIABLE at every
   call, and the instance VPTR at every call.** Nothing about the instance is
   cached. The only cached datum is the vtable SLOT OFFSET (per-name, resolved
   once from `FMalloc::VTableLayoutMap`). The `+0x12b call rax` in Z11's chain
   loads rax from `vptr + cached_offset` of the then-current `*GMalloc`.
2. **`0x970add8` is the game's global `FMalloc* GMalloc` variable; `0x97089b8`
   is a STATIC (BSS) allocator instance it points at** — installed by a single
   store in the game's early startup (eboot vaddr 0xb660). The log line's arrow
   means "variable address → current value", not old→new.
3. **The instance class at post-init is NOT Binned3.** Its constructor installs
   vtable ap 0x7ec2e28 (runtime 0x82c2e28); its `GetDescriptiveName` slot holds
   the base `FMalloc::GetDescriptiveName` returning L"Unspecified allocator"
   (fn 0xb51f0), and three of its Malloc-family slots bind one undefined
   scrambled-NID import (`zr094EQ39Ww#6#4`, dynsym[150]) — a bootstrap/libc-era
   allocator. `FMallocBinned3` IS fully linked (vtable ap 0x7ec26e0, runtime
   0x82c26e0; `GetDescriptiveName` at 0xb2810 returns L"Binned3"; Malloc 0xb48e0;
   Realloc 0xb49b0) but nothing in the binary statically installs its vtable —
   it is installed later at runtime (post-init), i.e. the real repoint target.
4. **MAJOR SECONDARY FINDING (new, static proof): the game's FMalloc vtable
   layout is exactly one slot (+8) ABOVE the port's generated 5_06 map for every
   entry.** Game truth: Malloc@+0x30, TryMalloc@+0x38, Realloc@+0x40,
   TryRealloc@+0x48, Free@+0x50, …, QuantizeSize@+0x68, …, GetDescriptiveName@+0xd0.
   Port map (5_06 file): Malloc 0x28, Realloc 0x38, Free 0x48, QuantizeSize 0x60.
   **Every FMalloc virtual the port dispatches lands one slot below the intended
   function.** In particular the port's "Realloc"@0x38 lands on the game's
   `TryMalloc` default wrapper (0xb8a20: `mov (%rdi),rax; call [rax+0x30]`),
   which forwards Realloc's argument registers unshifted to `Malloc` — i.e.
   `Malloc(Count = (SIZE_T)Original /*a pointer value*/, Alignment = (uint32)NewSize)`.
   This is a deterministic mis-dispatch, and it mechanically explains the :528
   crash family (§5).

## 1. Variable-vs-cached — source proof (port tree)

Tree: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`
(SOURCE-SELECTION.json of the l24 build pins UnrealVirtual506.cpp and the
generated_include FunctionBodies to exactly this tree; the FMalloc generated file
mtime Oct 2 09:03 predates the Oct 8 l24 build — the build shipped these values).

- `unreal/src/FMemory.cpp:8` — `FMalloc** GMalloc{};` The port's `GMalloc` is a
  **pointer to the game's global variable** (double pointer), not an instance.
- `unreal/src/UnrealInitializer.cpp:848` — `GMalloc = std::bit_cast<FMalloc**>(results.gmalloc);`
  — aimed once, at the *address of the game's variable* (signature-scan result).
- `unreal/src/Core/HAL/UnrealMemory.cpp:146-154` — `FMemory::Realloc`:
  ```cpp
  if (!GMalloc || !*GMalloc || !…bVersionedContainerIsInitialized) { throw …; }
  return (*GMalloc)->Realloc(Original, Count, Alignment);
  ```
  `(*GMalloc)` is a **call-time read of the game variable**; the result is `this`
  for the wrapper. (Same pattern in every FMemory entry, :16-169.)
- `unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp:43-58`
  — `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(FMalloc, Realloc, …)`, the body behind
  `FMalloc::Realloc` (`FMemory.cpp:32-35`):
  ```cpp
  static const auto offset = []{ … VTableLayoutMap.find(STR("Realloc")) … }();  // cached SLOT OFFSET only
  std::byte* vtable = std::bit_cast<std::byte*>(*std::bit_cast<std::byte**>(this)); // CALL-TIME vptr read
  auto func = …(vtable + offset); … RC_UE_VIRTUAL_CALL_PARAMS(func, args);          // the `call rax`
  ```
- Verified in the shipped l24 object (`2026-10-08-l24-package/pass-1/build/
  combined-supplier.o`, section `.text._ZN2RC6Unreal7FMalloc7ReallocEPvyj`):
  `mov offset(%rip),%ecx; add %rcx,%rax` — offset loaded from a `__cxa_guard`-protected
  static populated by `VTableLayoutMap.find`; and in the shipped PRX itself
  (mb24.elf, calibrated mapping — `module_start` 0x15a0 lands on its prologue):
  at mb vaddr 0xa1351..0xa136b:
  `mov rdi,[rbp-0x58]; mov rax,[rbp-0x48]; mov rsi,[rbp-0x10]; mov rdx,[rbp-0x18];
  mov ecx,[rbp-0x1c]; call rax` — return address 0xa136b = Z11's `mb+0xa1240+0x12b`.
  **So rax came from `*(vptr + static_offset)` where vptr was read from
  `this = *GMalloc` moments earlier at call time.** No instance caching anywhere.

## 2. Repoint mechanics — binary proof

ELF phdrs (parsed): PT_LOAD RW (PH7) file 0x9140000/size 0x1cc5f2, vaddr
0x913c000, memsz 0x4f7418 → file-backed to vaddr 0x93085f2, BSS beyond.

- Runtime 0x970add8 → vaddr **0x930add8**: PH7, **BSS** (past file-backed end),
  0x2420 bytes above the instance. This is the game's `FMalloc* GMalloc` global.
- Runtime 0x97089b8 → vaddr **0x93089b8**: PH7, BSS (0x3c6 bytes past file end).
  A **static instance**, not heap.

Whole-exec RIP-relative disp32 xref scan for 0x930add8 (custom scanner over the
exec segment; every candidate confirmed by disassembly): **exactly 13 references,
exactly ONE store**:

```
eboot vaddr 0xb64f: lea  rbx,[rip+…]        # 0x93089b8   ← &static instance
         0xb656: xor  esi,esi
         0xb658: mov  rdi,rbx
         0xb65b: call 0xafd40                              ← its constructor
         0xb660: mov  [rip+…],rbx           # 0x930add8   ← GMalloc = &instance  (THE INSTALL)
```

The other 12 references are loads (uses): a 12-byte accessor `return GMalloc` at
0xc084 and a cluster of loads at 0xddca–0xdf4d (allocator plumbing reading the
current GMalloc). `movabs` imm64 scan for the vtable addresses: zero hits — no
other install path exists statically for the variable.

So the "repoint" the UE4SS line shows is simply the game's own early-startup
install of its bootstrap allocator into the global variable; the port's log line
(`UnrealInitializer.cpp:369`) prints `(void*)GMalloc` (the variable address) and
`(void*)*GMalloc` (its current value) — both stable across all five l24 sessions
(`/tmp/l24-ue4ss*.log:607`, always `0x970add8 -> 0x97089b8`). Because the port
re-reads the variable at every call (§1), a later game repoint (bootstrap →
Binned3 at engine init) is picked up automatically — including mid-PostInitialize,
which is exactly the observed window (§4-§5).

## 3. Instance class from the vptr — static identification

Method: constructor → vtable address point → relocation addends per slot →
function identification by body/string anchors. RTTI is disabled in this game
(Itanium vtables carry [offset-to-top=0][typeinfo=NULL] prefixes — raw zeros,
no relocs), so class identity comes from the GetDescriptiveName slot.

**Instance installed at post-init (0x97089b8, ctor 0xafd40):**
- 0xafd77: `lea rax,[0x7ec2e28]; mov [rdi],rax` — vtable ap 0x7ec2e28
  (runtime vptr **0x82c2e28**). Slot addends (from the 1.66M-entry RELA table):
  +0x30/+0x40/+0x50 = dynsym[150] `zr094EQ39Ww#6#4` (undefined scrambled NID
  import — same symbol in Malloc/Realloc/Free positions: a trap/bootstrap
  binding), +0x38 = 0xb8a20 (TryMalloc default), +0x48 = 0xb8a30 (TryRealloc
  default), +0x68 = 0x96220 (`return Count`), +0xb8 = 0x96230 (DumpAllocatorStats
  default — Logf's "Allocator Stats for %s" with a virtual GetDescriptiveName
  self-call at slot +0xd0), **+0xd0 = 0xb51f0 = base `FMalloc::GetDescriptiveName`
  → L"Unspecified allocator"** (string at vaddr 0x79ec78e, single UTF-16LE hit).
  ⇒ a bootstrap-era allocator, **not** any named engine allocator.

**The linked-in real allocator (crash-side):**
- `FMallocBinned3` vtable ap **0x7ec26e0** (runtime vptr **0x82c26e0**);
  `GetDescriptiveName` slot +0xd0 → **0xb2810**: `lea rax,[0x78cb9da]; ret` —
  returns **L"Binned3"** (exact null-terminated UTF-16LE literal, 1 hit).
  Corroboration the class is really Binned3: assertion strings
  "…MallocBinned3 Corruption C…", "FMallocBinned3 Attempt to f/r…" (UTF-16LE ×8)
  and the source-path literal `\Runtime/Core/Private/HAL/MallocBinned3.cpp`
  are linked; "Binned2", "Mimalloc", "FMallocProxy" strings: **0 hits** file-wide.
- No static code/data reloc references ap 0x7ec26e0 (or ±8) — its vtable is
  installed by runtime-only means (see §5 caveat); the engine's Binned3 instance
  is therefore the *runtime* repoint target, invisible to file-level analysis.

**How the log line should capture the class at runtime** (the task's requested
recipe): read `instance = *variable`; read `vptr = *(void**)instance`; match
`vptr` against the statically known address points (rebase by the live module
base): `0x82c26e0` ⇒ FMallocBinned3 (GetDescriptiveName would return L"Binned3"),
`0x82c2e28` ⇒ bootstrap ("Unspecified allocator"). Do **not** call
`GetDescriptiveName` through the port wrapper — the port's 5_06 map has **no
`GetDescriptiveName` entry**, so `FMalloc::GetDescriptiveName`
(FMemory.cpp:102-105) would throw `Virtual FMalloc::GetDescriptiveName is
unavailable` at the static-initializer lambda. Reading the slot contents as raw
pointers (§7) is safe.

## 4. The one-slot vtable shift (decisive new evidence)

Two independent games' worth of anchors fix the game's true layout:

(a) **Shared default-impl wrappers** (bodies compiled from MemoryBase.h inlines,
    referenced by 65 allocator vtables):
  - 0xb8a20 `mov (%rdi),rax; call [rax+0x30]` — TryMalloc default → forwards to
    **Malloc@+0x30**;
  - 0xb8a30 `mov (%rdi),rax; call [rax+0x40]` — TryRealloc default → forwards to
    **Realloc@+0x40**;
  - 0xb8a40 `call [rax+0x30]; … memzero` — MallocZeroed/TryMallocZeroed defaults
    → Malloc@+0x30 + zero.
  Under the port's map (Malloc@0x28) 0xb8a20 would forward to itself — impossible.
(b) **Function signatures** at the slots: 0xb48e0 consumes (this, rsi=Count,
    edx=Alignment) = 3-arg Malloc at +0x30; **0xb49b0 opens with
    `cmp rdx,0x20001` (size threshold) / `cmp ecx,0x11` (alignment threshold) and
    spills rsi/rdx/ecx = 4-arg Realloc at +0x40**, and its taken branch
    `jne 0xb4b70` lands exactly on Z11's `call 0xb2160` (ends 0xb4b75 — I
    verified the `e8 eb d5 ff ff` bytes and the `c3 cc cc cc` function boundary
    before 0xb49b0 myself).
(c) **Slot-value semantics fit only one enumeration**: [dtor][dtor][4 prefix
    virtuals (ret-0 defaults)][Malloc 0x30][TryMalloc 0x38][Realloc 0x40]
    [TryRealloc 0x48][Free 0x50][MallocZeroed 0x58][TryMallocZeroed 0x60]
    [QuantizeSize 0x68][GetAllocationSize 0x70][Trim 0x78][SetupTLSCaches 0x80]
    [MarkUsed 0x88][MarkUnused 0x90][ClearDisable 0x98][InitStatsMeta 0xa0]
    [UpdateStats 0xa8][GetAllocatorStats 0xb0][DumpAllocatorStats 0xb8]
    [IsInternallyThreadSafe 0xc0 → returns TRUE][ValidateHeap 0xc8 → returns
    TRUE][GetDescriptiveName 0xd0]. This fits BOTH vtables perfectly, including
    the base-class "Unspecified allocator" at 0xd0 on the bootstrap vtable and
    L"Binned3" at 0xd0 on the Binned3 vtable, and the no-Exec-slot count (the
    Konami/Sony build omits FExec::Exec from FMalloc's vtable and carries four
    unnamed prefix virtuals instead of the stock three-slot prefix the port's
    generator assumed).

The port's `5_06_VTableOffsets_FMalloc_FunctionBody.cpp` (generated_include;
mtime Oct 2, included by `UnrealVirtual506.cpp:37`, compiled into l24 per
SOURCE-SELECTION) maps every name exactly one slot lower: Malloc 0x28,
TryMalloc 0x30, **Realloc 0x38**, TryRealloc 0x40, Free 0x48, MallocZeroed 0x50,
TryMallocZeroed 0x58, QuantizeSize 0x60, GetAllocationSize 0x68, Trim 0x70,
SetupTLSCaches 0x78, MarkUsed 0x80, MarkUnused 0x88, ClearDisable 0x90,
InitializeStatsMetadata 0x98. (GetDescriptiveName is absent from the map.)

## 5. Crash-mechanism synthesis (what this means for :528)

With the shift, `AddUObjectDeleteListener` (:528) → TArray Append → ResizeTo →
`FMemory::Realloc` → port `FMalloc::Realloc` dispatches **+0x38 = game
TryMalloc-default (0xb8a20)** with Realloc's register state
(this, rsi=Original, rdx=Count, rcx=Alignment). The wrapper forwards registers
unshifted to Malloc@+0x30, producing
`Malloc(Count = (SIZE_T)Original /*the old array pointer — a huge number*/,
Alignment = (uint32)NewSize)`. On a Binned3 instance that is an
enormous-size/misaligned request into 0xb48e0; on the still-installed bootstrap
instance the same slot chain reaches the scrambled-import trap. Either branch
ends in the game's fatal-error path — matching Z11's chain into the 7-arg
formatter (the double argument is the bogus size) and `abort()` → CE-108255-1.
This makes the :528 failure **deterministic**, not a race: the first GMalloc-
mediated TArray growth cannot dispatch correctly with the current map. It also
subsumes Claude's hypothesis (a): the registration does not "hang" — it
mis-dispatches and detonates (or traps) inside the game allocator.

Honest caveat on Z11's outermost frame: Z11's inner call edges are real (I
re-verified the bytes), but the klog user-stack copy was impaired at the
backtrace boundary ("copyin: has nonsleeping lock"), and a direct entry into
0xb49b0 (true Realloc@+0x40) would require the port to have used offset 0x40,
which the compiled source excludes. The wrapper-mediated path above is the only
mechanism consistent with ALL statically verified facts if the stack walk simply
dropped the tiny wrapper/allocator-head frames; alternatively the walk is
complete and the live instance's vptr state at that instant differed from every
file vtable (runtime install, §3). **The item-4 logging below is designed to
decide exactly this at the next run** — it prints the variable, instance, vptr,
and the slot contents at +0x28/+0x30/+0x38/+0x40 so the executed path is
identifiable without trusting any stack walk.

## 6. TOCTOU

- **Yes, a real but narrow window exists, and the observed timeline exercises
  it.** `FMemory::Realloc` (UnrealMemory.cpp:146-153) null-checks `*GMalloc`
  then re-reads `*GMalloc` for the call: if the game's single install store
  (0xb660) lands between the two reads, the call uses the NEW instance (benign —
  the new value is valid; the guard's purpose is only pre-init protection).
  The sharper window is inside the wrapper: `this` is loaded from the variable
  once, then the vptr is read from `this` (UnrealVirtualBaseVC.hpp:52) — if the
  engine re-constructs/re-points the allocator between those loads, the slot is
  fetched through the OLD vtable of a NEW object (or vice versa). With the UE4SS
  init thread racing engine init (bootstrap→Binned3 install) — precisely the
  post-init→:528 window — this mid-init repoint race is live. Note the game's
  only observed store is an unsynchronized plain `mov`; both instances are
  static BSS objects (never freed), so use-after-free of the instance itself is
  not in scope.
- The static slot offset is `__cxa_guard`-protected (verified in the object
  disasm) — first-call thread race on the map lookup is safe.
- With the §4 shift, TOCTOU is presently secondary: the mis-dispatch is
  deterministic and will fault on every first GMalloc-mediated growth,
  raced or not.

## 7. EXACT spec for the item-4 logging (drop-in, port idiom)

Site: `port/unreal/src/UObjectArray.cpp`, inside
`UObjectArray::AddUObjectDeleteListener` (currently :379-389), immediately
BEFORE `DeleteListeners.Append(TArray{Listener});` — plus (recommended) the same
block at the top of `UnrealInitializer::PostInitialize` next to the existing
`:369` line so the two captures bracket the mid-init repoint. Read-only pointer
reads; NO virtual calls through unvalidated offsets (do not call
`GetDescriptiveName` via the wrapper — the key is absent from the 5_06 map and
it would throw):

```cpp
// [gmalloc-audit] item-4: capture the GMalloc variable, its current value, the
// instance vptr, and the raw vtable slot contents that FMemory::Realloc's
// dispatch would use. Read-only diagnostics; never call through these slots.
{
    FMalloc** gmalloc_var = GMalloc;                       // address of the game's global (logged 0x970add8)
    void* instance = (gmalloc_var && *gmalloc_var) ? static_cast<void*>(*gmalloc_var) : nullptr; // *variable
    void* vptr     = instance ? *static_cast<void**>(instance) : nullptr;                          // *(void**)instance
    auto slot = [&](uint32_t Offset) -> void*              // raw slot content, no call
    {
        return vptr ? *reinterpret_cast<void**>(reinterpret_cast<std::byte*>(vptr) + Offset) : nullptr;
    };
    Output::send(STR("[gmalloc-audit] var={} instance={} vptr={} slots +0x28={} +0x30={} +0x38={} +0x40={} +0xd0={}\n"),
                 (void*)gmalloc_var, instance, vptr,
                 slot(0x28), slot(0x30), slot(0x38), slot(0x40), slot(0xd0));
}
```

(The formatting follows `UnrealInitializer.cpp:369`'s existing `Output::send(STR("… {} …"), (void*)…)` idiom;
`<Unreal/FMemory.hpp>` is already in scope at both sites via `UnrealInitializer.hpp`.)

Interpretation table (module base 0x400000) — what the implementing agent should
expect and record:

| Field | Expected (post-init, as of l24) | Meaning |
|---|---|---|
| var | 0x970add8 | game GMalloc variable (BSS, vaddr 0x930add8) |
| instance | 0x97089b8 | static bootstrap instance (BSS, vaddr 0x93089b8) |
| vptr | 0x82c2e28 | bootstrap vtable ap 0x7ec2e28 → GetDescriptiveName ⇒ L"Unspecified allocator" |
| vptr (if repointed) | 0x82c26e0 | FMallocBinned3 vtable ap 0x7ec26e0 ⇒ L"Binned3" |
| +0x28 | bootstrap 0xe0-stub / Binned3 0xe0-stub | filler prefix slot (map's "Malloc" — WRONG slot) |
| +0x30 | bootstrap: import `zr094EQ39Ww#6#4` / Binned3: 0x4b48e0 | true Malloc |
| +0x38 | 0x4b8a20 both | TryMalloc default wrapper — **what the port's "Realloc" dispatch actually calls today** |
| +0x40 | bootstrap: import / Binned3: 0x4b49b0 | true Realloc (0xb49b0 = the fn whose large-path calls 0xb2160 → fatal) |
| +0xd0 | 0x4b51f0 (base, "Unspecified allocator") / 0x4b2810 (Binned3) | GetDescriptiveName implementations |

GetDescriptiveName is *statically* resolvable only as this offline vptr match
(the runtime string is a game-side return value; calling it is out of scope for
the log). If a future build wants the string, the safe route is to add
`GetDescriptiveName` to a corrected, probe-validated map entry at **+0xd0** —
not to call it through the current map.

## 8. Provenance & method

- Source citations: Read/grep of the port tree (§0 paths) on 2026-10-08.
- eboot: manual ELF64 phdr/section parsing (constant −0x4000 file↔vaddr bias on
  all PT_LOADs; PH8 strtab/symtab/rela bias 0x324000); PT_DYNAMIC parse →
  DT_RELA (1,661,599 entries) used as the ground truth for vtable contents;
  RIP-relative disp32 xref scan (clang-built scanner) + imm64 scan + thunk scan;
  Apple objdump disassembly of a section-header-zeroed copy (/tmp/eboot-noshdr.bin).
- l24 artifacts: combined-supplier.o section/symtab/rela walk; PRX FSELF ELF
  extraction with Z11's coordinate calibration re-verified byte-exact
  (module_start 0x15a0 prologue; dispatch `call rax` ends at mb 0xa136b).
- Session logs: /tmp/l24-ue4ss*.log, /tmp/l24-ab2-ue4ss.log (GMalloc line :607 in all).
- Limitations: the engine's Binned3 install site is not statically visible
  (§3); Z11's outermost-frame attribution could not be independently confirmed
  (§5); the four prefix virtuals' names are unknown (RTTI stripped); the
  scrambled NID import's resolution is runtime-only. None of these affect the
  §0 verdicts; the §7 logging is designed to close them at the next run.
