# Agent AU — FMalloc vtable-map fix (AB §4/§5/§7) + AD item-4 log spec alignment

Date: 2026-10-08. Inputs read-only: agent-AB-gmalloc-audit.md, agent-AH-gmalloc-edge-verify.md,
the port tree, the l24-build-r2 receipts, the staged VTableLayout.ini, vendored UE 5.6.1
MemoryBase.h. Written: the staged candidates + CHANGES.json rows below, verification logs
(/tmp/l25-au-verify/, copies under `2026-10-08-l25-prep/evidence/`), and this report.
No console contact, no link, no port-root mutation.

## 0. What was fixed and where it reaches the build

**FIX 1 — the compiled-in 5_06 FMalloc vtable DEFAULT map.**

- Authoritative defect site (verified): port root
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp`
  (sha256 `95488869f96ef1429f9fcc201f46d7effc3c9330811f4338ea961f79a182e44d`), textually
  included by `port/unreal/src/VersionedContainer/UnrealVirtualImpl/UnrealVirtual506.cpp:37`
  (`#include <FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp>` inside
  `UnrealVirtual506::set_virtual_offsets()`), compiled into l24 as unit
  `unreal-src-versionedcontainer-unrealvirtualimpl-unrealvirtual506` (sole `.d` reference to
  the file in the whole 374-unit dep universe; the two other `generated_include` dirs in that
  unit's argv — l24-r3/candidate and l23-r2/candidate — carry only MemberVariableLayout
  headers, so the port root was the unique resolution point).
- The `__cxa_guard`-protected per-name statics AB cites are the wrapper bodies in
  `UnrealVirtualBaseVC.hpp` (IMPLEMENT_UNREAL_VIRTUAL_WRAPPER) that LOOK UP this map at first
  call; the map itself lives in the generated file above — that is the compiled-in default
  corrected here.
- **VTableLayout.ini check (task item 1)**: the staged 216 B INI
  (`outputs/2026-10-04-zcode-team/worker-gap-l2a/staged/VTableLayout.ini`, sha pinned in
  townfall-build-facts.json) carries only `[NativeLayout]` header fields + `[UObject]
  ProcessEvent=0x268` — **no FMalloc rows**. The console-consumed truth for FMalloc is the
  staged NATIVE layout table (l24 lineage `CompiledNativeLayoutDeclarations.hpp` via
  `NativeProvidedVirtualVC::set_virtual_offsets` → `PublishStagedNativeLayoutMaps`), which AH
  proved supplied Realloc=0x40 and won in l24 (insert-if-absent ordering). So the compiled-in
  default is exactly what AH called it — a latent hazard that mis-dispatches only if staging
  fails — and fixing it does not touch the staged consumption path. (The l25
  UE4SSProgram.cpp ini consumer does have an `[FMalloc]` section branch; the INI simply
  doesn't ship one.)
- **Fix applied** (staged candidate; port root untouched, per the Z9/O law): every non-dtor
  FMalloc entry shifted **+8** to the binary-verified true slots (AB §4/§7; AH §1.1
  reconstructed vtable + lineage staged table, independently derived). Dtor-pair keys
  (`__vecDelDtor`, `__vecDelDtor__uint32` = +0x00/+0x08 slots) unchanged. Tail entries beyond
  AB's binary-verified range (> GetDescriptiveName@+0xd0) carry the same uniform +8 per AB
  §0.4 "one slot above … for every entry"; their relative order is corroborated by vendored UE
  5.6.1 `MemoryBase.h:248-281` (OnMallocInitialized → OnPreFork → OnPostFork →
  GetImmediatelyFreeableCachedMemorySize → GetTotalFreeCachedMemorySize).

### Slot table (before → after; game truth = after, per AB §4/§7 + AH)

| key | old 5_06 default | corrected (= game truth) |
|---|---|---|
| Malloc (+ overload key) | 0x28 | **0x30** |
| TryMalloc (+ key) | 0x30 | 0x38 |
| Realloc (+ key) | 0x38 | **0x40** |
| TryRealloc (+ key) | 0x40 | 0x48 |
| Free (+ key) | 0x48 | **0x50** |
| MallocZeroed (+ key) | 0x50 | 0x58 |
| TryMallocZeroed (+ key) | 0x58 | 0x60 |
| QuantizeSize (+ key) | 0x60 | 0x68 |
| GetAllocationSize (+ key) | 0x68 | 0x70 |
| Trim (+ key) | 0x70 | 0x78 |
| SetupTLSCachesOnCurrentThread | 0x78 | 0x80 |
| MarkTLSCachesAsUsedOnCurrentThread | 0x80 | 0x88 |
| MarkTLSCachesAsUnusedOnCurrentThread | 0x88 | 0x90 |
| ClearAndDisableTLSCachesOnCurrentThread | 0x90 | 0x98 |
| InitializeStatsMetadata | 0x98 | 0xA0 |
| UpdateStats | 0xA0 | 0xA8 |
| GetAllocatorStats (+ key) | 0xA8 | 0xB0 |
| DumpAllocatorStats (+ key) | 0xB0 | 0xB8 |
| IsInternallyThreadSafe (+ _C) | 0xB8 | 0xC0 |
| ValidateHeap | 0xC0 | 0xC8 |
| GetDescriptiveName | 0xC8 | **0xD0** |
| OnMallocInitialized | 0xD0 | 0xD8 |
| OnPreFork | 0xD8 | 0xE0 |
| OnPostFork | 0xE0 | 0xE8 |
| GetImmediatelyFreeableCachedMemorySize (+ _C) | 0xE8 | 0xF0 |
| GetTotalFreeCachedMemorySize (+ _C) | 0xF0 | 0xF8 |
| __vecDelDtor / __vecDelDtor__uint32 | 0x0 | 0x0 (unchanged) |

Mechanical validation: 43 keys parsed from the original; 41 changed, each verified
`new == old + 8`; 2 unchanged (dtors). Note AB §3/§4 said "GetDescriptiveName is absent from
the map" — the on-disk 5_06 file actually carries it at 0xC8; corrected to 0xD0 either way.

**How the corrected default reaches the l25 build** (build_runtime.py mechanics, verified
against the driver source):

- The generated include is a `.cpp`-suffixed textual include, so it cannot ride the header
  VFS alias (build_runtime.py:47 admits only `.hpp/.h/.inc/.inl` into `header_specs`), and it
  is not a unit source, so a bare replacements row on it would be a **silent no-op** (Z9
  risk 1). Two coordinated pieces instead:
  1. Corrected file staged at
     `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp`.
     That directory is **already** a `include_directories` entry in typescan-r4/CHANGES.json,
     and build_runtime.py:109 inserts those `-I` flags at argv[1:1] — ahead of the inherited
     argv — so `<FunctionBodies/5_06_…FMalloc…>` resolves to the candidate while the other
     100 FunctionBodies includes fall through to the port dir (verified by the emitted .d,
     see §2).
  2. A **byte-identical** candidate row on the unit source
     `port/unreal/src/VersionedContainer/UnrealVirtualImpl/UnrealVirtual506.cpp` — required
     because build_runtime.py:94 only recompiles units whose source is in `replacements`
     (or whose deps hit an alias); this row pulls
     `unreal-src-versionedcontainer-unrealvirtualimpl-unrealvirtual506` into
     `affected_units`. No source delta to the TU itself.
- Provenance rows (original = the exact l24-consumed port paths, pin-identical snapshots
  under `typescan-r4/original/…`), both in `files` and legacy `changes` (kept in sync).
  Duplicate-original scan across profile/typescan-r4/thread after the additions: CLEAN
  (24 originals).

**FIX 2 — AD's PENDING-ALIGNMENT fallback replaced by AB's §7 spec** in
`typescan-r4/candidate/src/UObjectArray.cpp` (`AddUObjectDeleteListener`):

- `L25GMallocSnapshot` extended with `slot_28/slot_30/slot_38/slot_40/slot_d0`; read inside
  `l25_snapshot_gmalloc()` only when the vptr passes `l25_pointer_word_plausible`, each slot
  address re-screened, contents read via the existing byte-wise `l25_read_word` guarded-read
  idiom. Read-only; never called through (AB §3/§7's warning about wrapper-thrown
  GetDescriptiveName respected — these are raw word reads).
- Both GMalloc log lines EXTENDED in place (marker prefixes and all existing fields stable,
  nothing renamed):
  - pre: `[L25][DeleteListener] header pre: Data=… GMallocVar=… GMalloc=… GMallocInst=…
    vptr=… slots +0x28={} +0x30={} +0x38={} +0x40={} +0xd0={}`
  - post: `[L25][DeleteListener] GMalloc post: GMallocVar=… GMalloc=… GMallocInst=… vptr=…
    slots +0x28={} +0x30={} +0x38={} +0x40={} +0xd0={}`
  (the `slots +0x28=… +0xd0={}` fragment is AB §7's exact format.)
- The `PENDING ALIGNMENT` comment block is replaced by an ALIGNED block citing agent-AB §7
  (eboot sha 3b6cfad4…, AH corroboration) plus the interpretation table: vptr 0x82c2e28 =
  bootstrap ("Unspecified allocator"), 0x82c26e0 = FMallocBinned3 ("Binned3"); +0x38 holding
  0x4b8a20 (TryMalloc default wrapper) is the signature of dispatch through the pre-fix
  one-slot-low default map. AD's historical CHANGES entry (which mentions the then-pending
  state) left as append-only history.
- Diagnostic tags unchanged (`l25_dl_gmalloc_pre/post` still carry the instance low-32).

**Rebase note (task item 4 anticipated this)**: on arrival the CHANGES.json row still pinned
candidate `4f4afddf…` while disk held `69463958…` (AT's code-6 pre-read guard had landed;
a concurrent agent recorded `69463958` in CHANGES.json at 15:46, mid-flight). AU rebased on
the CURRENT file: AT's refusal-5/6 ordering guard, sane gate, RISK/fresh-alloc logging all
preserved untouched; the row now pins AU's final hash. Single-owner drift window closed.

## 1. Diffs (summary)

- Generated include candidate vs original: 41 single-token value changes (all +0x8), 2 dtor
  keys untouched, plus a 12-line provenance comment header. Full key table in §0.
- UObjectArray.cpp: comment block swap (16 lines → 24), struct +5 fields, snapshot function
  +14 lines (guarded slot reads), pre/post log lines extended with 5 fields each, +2 comment
  lines at the post site. Brace/paren balance 0/0; marker sweep: all pre-existing marker
  strings present and unrenamed; `PENDING` absent from the file.

## 2. Compile verification (touched TUs)

| TU | compiler | mode | exit | notes |
|---|---|---|---|---|
| unreal-uobjectarray (AU candidate) | host /usr/bin/clang++ (Z13 recipe) | -fsyntax-only | 0 | `logs/syntax-unreal-uobjectarray.log` |
| unreal-uobjectarray | host | -c | 0 | `logs/obj-unreal-uobjectarray.log` |
| unreal-uobjectarray | **prospero-clang++** (real l24 argv, source→candidate) | -c | 0 | only known inherited-header warnings |
| unrealvirtual506 (candidate TU + shadowing -I, build_runtime-style argv) | host | -fsyntax-only | 0 | 4 warnings, all inherited headers (Atomic.hpp -Wswitch ×2, UnrealType -Wundefined-inline, enum-deprecation) |
| unrealvirtual506 | **prospero-clang++** | -c | 0 | object 2,746,976 B |

Shadow proof (the mechanism actually engages): the emitted `.d` resolves
`5_06_VTableOffsets_FMalloc_FunctionBody.cpp` to the **candidate** path while all other
FunctionBodies includes (100 references) still resolve from the port root.
A/B object proof (control object compiled with the port include vs the candidate object):
`llvm-objdump --disassemble-symbols=_ZN2RC6Unreal16UnrealVirtual50619set_virtual_offsetsEv`
diffs on exactly 84 lines — only the FMalloc emplace immediate sites; distinctive pair: `$0x28`
sites exist only in the control, `$0xf8` sites only in the candidate. Diff saved as
`evidence/setoffsets.diff`. Prospero runs used the SDK wrapper with
`LLVM_CONFIG=…/LLVM-21.1.8-macOS-ARM64/bin/llvm-config` (same env build_runtime.py sets).
Host-compile log copies under `2026-10-08-l25-prep/evidence/` (`au-*`,
`prospero-unrealvirtual506.log`, `host-syntax-unrealvirtual506.log`,
`prospero-uobjectarray.log`, `unrealvirtual506.d`, `setoffsets.diff`).

## 3. Hashes (all pinned in typescan-r4/CHANGES.json; shasum -a 256)

| file | sha256 |
|---|---|
| original `port/…generated_include/FunctionBodies/5_06_…FMalloc….cpp` (= copied_original) | `95488869f96ef1429f9fcc201f46d7effc3c9330811f4338ea961f79a182e44d` |
| candidate `typescan-r4/candidate/generated_include/FunctionBodies/5_06_…FMalloc….cpp` | `2f20d64d280dcd32c36beea9748cb0541cb93cdaea5ddac139a526c2637c0a58` |
| original/copied/candidate `UnrealVirtual506.cpp` (byte-identical, invalidation-only row) | `8e407b89aef15222eb39c321a65c461b3f5b05b0c56e9dbbc26e9eb8d30634db` |
| candidate `typescan-r4/candidate/src/UObjectArray.cpp` (AT guard + AU extension; original port pin `171560a9…`) | `ea7221e8d7537e9b0dd36ddc0ac26a2c76050f343fdf7e8e2b8b03f297281f29` |
| `typescan-r4/CHANGES.json` (20 rows, files==changes, 8 dated entries) | `55b38893ed8346bda45116b2a817f459dc5d5ca47e1580a51402016823b54fc1` |

Final sweep: all 20 rows match disk on original/copied_original/candidate hashes; every
candidate under `outputs/2026-10-08-l25-runtime/`; no duplicate-original conflicts across the
three parts; port root verified pristine (its pins still match the l24 SOURCE-SELECTION pins).

## 4. Not done / honest limits

- No full l25 rebuild (parent owns `build_runtime.py`/`build_variants.py` execution); the
  unrealvirtual506 unit should appear in the produced CHANGE-SELECTION.json `affected_units`
  (Z9 risk-8 check for agent R). No link, so no import-delta statement; none expected (values
  and logging only).
- The +8 tail corrections beyond GetDescriptiveName@+0xd0 are order-corroborated but not
  binary-verified (AB's enumeration stops at 0xd0); they are unreachable by the port's
  FMemory wrappers today and are strictly better than the one-slot-low status quo.
- The runtime slot log lines are pre/post-Append only in Add (AB's "recommended" second site
  at UnrealInitializer PostInitialize:369 remains as-is; task scoped the change to
  UObjectArray.cpp).
- Fix 1 does not change behavior on a healthy staged build (staged table still wins
  insert-if-absent); it removes the silent one-slot-low fallback if staging ever fails.
