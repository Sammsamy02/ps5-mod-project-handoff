# CAP13 DESIGN — Native Windows DLL loader for PS5 (PE-on-PS5 compatibility route)

Agent JH (ZCode), 2026-10-13, cap13 packet. Owner of the loader design;
JG owns `pe_analyzer.py` and fixtures. **DESIGN + staged source only —
Mac-only. No console contact, no builds of the PS5 tree, no main-tree
edits.** Nothing here authorizes console action; every console rung in §9
needs Claude/integration first.

Evidence labels (inherited from CAP9-CPPMODS-DESIGN.md):
- **[LOCAL-SOURCE]** — from files in this worktree (path + line).
- **[LOCAL-VERIFY]** — a fact I verified on this Mac today with the local
  toolchain (command + output quoted in the text).
- **[TRAINING-KNOWLEDGE]** — version-sensitive external facts; re-verify
  against a pinned primary source before code depends on them.
- **[INFERENCE]** — my reasoning.

---

## 0. Verdict box (read this first)

1. **The route is buildable in layers, and the mechanical layers are
   genuinely easy**: PE section mapping, base relocations, import-table
   resolution against a shim registry, and calling `DllMain` are classic
   loader engineering, all of which can be **unit-tested on the Mac with
   zero console risk** (the mapper is ordinary userspace code; macOS is
   also x86_64 SysV, so the ABI bridge can be host-tested too).
2. **The wall is exactly where CAP9 said it is** — not the loader, but the
   MSVC C++ ABI (calling convention remap is solvable with thunks; RTTI,
   SEH/EH, static TLS, and STL-in-boundary-layouts are the killers).
   Original PE UE4SS C++ mods are C++-surface mods by construction
   [LOCAL-SOURCE: CAP9-CPPMODS-DESIGN.md §0], so cap13's route lives or
   dies on how much of that wall we cross **per mod, census-driven**.
3. **One load-bearing unknown decides feasibility and has never been
   probed**: whether the game process can map/execute anonymous memory
   (`mmap` + `mprotect PROT_EXEC`) at all (J1, §9). If it cannot, this
   entire route is dead on arrival — there is no other way to run PE
   machine code in-process. This probe is one console rung and should run
   before any further investment.
4. **Recommendation: build the census-driven MVP** (§8). CAP13's own
   acceptance requires the *original DLL bytes* to load and act
   [LOCAL-SOURCE: ZCODE-ASSIGNMENT.md "Acceptance"], so no amount of
   #9-style recompiling substitutes for this route. But its coverage is a
   bounded envelope (headless, no static TLS, no executed EH, no
   game-code patching), published honestly as a support matrix. #9
   (rebuild from source) remains the high-coverage route; this is the
   original-bytes proof route. They are complementary, not rivals.

---

## 1. Scope, laws, integration point

- Byte-identity law: the original DLL is read, hashed, **never written**;
  relocations/imports are patched only in the in-memory copy [LOCAL-SOURCE:
  ZCODE-ASSIGNMENT.md "Input DLL bytes/hash remain unchanged"].
- No Sony SDK material; PS5-side code targets the pinned
  `ps5-payload-sdk` + prospero-clang `x86_64-sie-ps5` profile only.
- No instruction rewriting of the game: any mod whose imports show it
  patches game code (`VirtualProtect` on game RX pages, `WriteProcessMemory`
  on the self process) is **refused** — cap13 states DLLs that rewrite game
  instructions stay unsupported until a separately established capability
  exists [LOCAL-SOURCE: ZCODE-ASSIGNMENT.md "Dependencies and limits"].
- **Integration point**: the existing #9 host admits only the C-table ABI —
  `mbm_mod_entry_v1` with `mbm_host_v1` services and `mbm_mod_v1` lifecycle
  [LOCAL-SOURCE: typescan-r4/include/Mod/NativeModAbi.h:45-47, :203-270].
  The current loader refuses PE at a 2-byte `MZ` marker and refuses
  `start_mod`-only (upstream UE4SS C++ ABI) modules with a rebuild
  instruction [LOCAL-SOURCE: typescan-r4/candidate/src/Mod/CppMod.cpp:127-159;
  include/Platform/NativeModLoaderPolicy.hpp — EntryKind::LegacyCppAbi
  refusal reason]. **We do not touch that refusal.** Instead the PE route
  ships as a separate adapter module that *itself* exports
  `mbm_mod_entry_v1` and internally drives the PE loader. The host's
  selection logic stays untouched; discovery/manifest decides which file is
  a PRX (native route) and which is a PE (adapter route). This keeps the
  #9 host's fail-closed posture and makes the whole feature removable.

```
CppMod (host, unchanged)
  └─ CppModModule::Open(<module file>)
       ├─ main.prx  (native route, today)
       └─ main.dll  (PE route, NEW: file is the ORIGINAL mod DLL, bytes intact)
            └─ pe_adapter.sprx  (our shim PRX, exposes mbm_mod_entry_v1;
                 hosts the PE loader inside itself and is started with the
                 original DLL's path + hash as arguments)
```

Dispatch detail: the existing `FindNativeCppModModule` prefers
`main.prx/main.sprx` and only falls back to `main.dll` names last
[LOCAL-SOURCE: candidate/include/Platform/NativeCppModPath.hpp — candidates
array]. So the PE route is expressed as: `dlls/main.dll` = original
untouched DLL; `dlls/pe_adapter.sprx` = our adapter; the manifest row names
the adapter. The host loads the adapter as a normal ABI-v1 module; the
adapter locates the adjacent `main.dll`, verifies its hash against the
manifest, and maps it. The host never sees PE bytes. One staged patch to
`CppMod.cpp` is provided in case the integrator prefers routing inside the
host instead of via manifest naming (native-loader/patches/ — proposal
only).

---

## 2. Architecture

```
                 ┌────────────────────────────────────────────────┐
                 │ pe_adapter.sprx (SysV, prospero-clang, our code)│
                 │                                                │
  original.dll ──▶ pe_loader: map sections, relocate, resolve    │
  (bytes intact) │ imports via win_api_shim, refuse TLS/SEH,      │
                 │ call DllMain via win64 bridge                  │
                 │        │                                       │
                 │        ▼                                       │
                 │ win64 bridge (asm): SysV ↔ Win64 reg remap,    │
                 │ shadow space, Win64-nonvolatile preservation   │
                 │        │                                       │
                 │        ▼                                       │
                 │ win_api_shim: kernel32/ucrt/vcruntime subset   │
                 │ (T1 memory/sync/time/thread, T2 files, T3      │
                 │ module registry, T4 fail-closed EH stubs)      │
                 │        │                                       │
                 │        ▼                                       │
                 │ ue4ss_compat: UE4SS.dll export surface →       │
                 │ mbm_host_v1 services + MSVC-layout boundary    │
                 │ objects (CppUserModBase base subobject)        │
                 └────────────────────────────────────────────────┘
                        │ mbm_mod_entry_v1 (plain C)
                        ▼
                 #9 host runtime (unchanged, fail-closed)
```

Load order inside the adapter:
1. verify manifest hash of `main.dll` (bytes untouched on disk);
2. `pe_loader::load()` → mapped image or a precise refusal (named reason:
   TLS present / RWX-requiring page partition / import `X!Y` missing / …);
3. resolve exports (`start_mod`, `uninstall_mod`, optional `mbm_mod_entry_v1`);
4. call entry point (`_DllMainCRTStartup`) with `DLL_PROCESS_ATTACH`;
5. publish `mbm_mod_v1` whose callbacks call the PE's `start_mod` /
   virtual slots through the bridge; every callback converts guest failure
   into `MBM_E_MOD_FAULT` (NativeModAbi.h law 3) and never unloads the
   image (matches the host's retained-module law
   [LOCAL-SOURCE: CppMod.cpp:441-455 — ABI-v1 modules stay resident]).

---

## 3. THE PE LOADER (task §1)

### 3a. Format parsing — feasible, fully Mac-testable

Parse: DOS header `e_magic='MZ'`, `e_lfanew` → `PE\0\0` → COFF header
(`Machine == 0x8664` required — refuse anything else, including ARM64
PEs) → `IMAGE_OPTIONAL_HEADER64` (`Magic == 0x20b`, `ImageBase`,
`SectionAlignment`, `SizeOfImage`, `AddressOfEntryPoint`, `DataDirectory`).
Directories used: `Export(0)`, `Import(1)`, `Exception/.pdata(3)` — parsed
only for the classifier, never used to dispatch exceptions (§4c),
`BaseRelocation(5)`, `TLS(9)` — presence is a refusal (§3e),
`DelayImport(13)`. Everything else ignored by design (resources, debug,
COM/.NET — a COM descriptor / CLR header is a refusal: managed PEs are
cap10's domain, not this loader's).

### 3b. Mapping — feasible; one real constraint

- Allocate one anonymous region of `SizeOfImage` rounded to 16 KiB (PS5
  page size; the PE's 4 KiB section alignment does not match, so file
  mapping is impossible — sections are copied manually
  [INFERENCE, consistent with CAP9 §2a]).
- Copy each section `PointerToRawData → VirtualAddress`, zero-fill to
  `VirtualSize`; headers at offset 0 (read-only).
- Permissions per section (`Characteristics`): `CNT_CODE|MEM_EXECUTE` →
  `R+X`; `CNT_INITIALIZED_DATA` → `R(+W` if `MEM_WRITE`)`; `CNT_UNINITIALIZED_DATA`
  → `RW`; discardable → keep `R` (cheap, avoids page-fault surprises).
- **The 16 KiB partition constraint**: PS5 `mprotect` is 16 KiB-granular;
  PE sections are 4 KiB-granular. When two adjacent sections that need
  different permissions share a 16 KiB page, the page's permission is the
  *union*. If a page mixes EXEC and WRITE, the union is `RWX`. Policy:
  compute the permission partition at 16 KiB granularity; if any partition
  requires `W+X`, **refuse with a named diagnostic** unless the J1 probe
  (§9) has positively established that `PROT_WRITE|PROT_EXEC` anonymous
  memory is permitted in the game process. This converts an unknown into a
  load-time diagnostic. (Most mods' layouts separate `.text` from `.data`
  by ≥16 KiB, so this rarely fires [INFERENCE].)
- The image is **never unmapped** (retained-module law, §2).

### 3c. Base relocations — feasible, mechanical

`ImageBase` is a hint; we cannot honor it under ASLR, so compute
`slide = load - ImageBase` and walk `DataDirectory[BaseReloc]`: block
header (`VirtualAddress`, `SizeOfBlock`), entries `(type<<12 | offset)`;
apply `DIR64(10)` (add slide to the 8-byte target — covers virtually all
x64 fixups), `HIGHLOW(3)` (4-byte), skip `ABSOLUTE(0)`. Unknown type →
refusal. The IAT we overwrite in §3d needs no reloc because we write
final addresses after reloc processing (order: copy → relocate → resolve
imports → init TLS-refuse → entry).

### 3d. Imports — feasible, bounded by the census

Walk `DataDirectory[Import]`: per descriptor, `Name` → DLL string; walk
`OriginalFirstThunk` (fall back to `FirstThunk` when
`OriginalFirstThunk == 0`); per thunk: ordinal bit `0x8000000000000000` →
ordinal lookup, else RVA of `IMAGE_IMPORT_BY_NAME` → `Hint` + ASCII name.
Resolve `dll!symbol` against the shim registry (§5) **and** a per-image
export registry (loaded PE DLLs can import each other — the adapter keeps
a list and resolves mod→mod imports first). Write the resolved pointer
into the matching `FirstThunk` (IAT) slot.

**Failure policy is load-time and fail-closed**: an unresolvable import
refuses the load with a diagnostic listing every missing `dll!symbol` in
one pass. No null-filled IATs, no silent stubs — cap13 acceptance demands
"unavailable imports/features produce a precise diagnostic instead of a
fake successful no-op" [LOCAL-SOURCE: ZCODE-ASSIGNMENT.md "Acceptance"].

Delay-load imports need no directory processing at load: the mod's own
`__delayLoadHelper2` (statically linked inside the mod for MSVC builds)
re-enters through its *kernel32 imports* — which are our shims — so
`LoadLibraryA/W/ExW`, `GetProcAddress`, `FreeLibrary` in the shim make
delay-loading work with zero extra loader code [INFERENCE; standard
delay-helper structure, [TRAINING-KNOWLEDGE] for the exact helper
internals].

`LoadLibrary` of arbitrary new DLLs re-enters `pe_loader::load()`
recursively (bounded depth 4, cycle-checked by normalized path).

### 3e. TLS — split verdict (important)

- **Static TLS (PE TLS directory): REFUSE.** x64 Windows PE code reaches
  thread-locals via `gs:[0x58]` (TEB `ThreadLocalStoragePointer`) and the
  TEB itself via `gs:[0x30]`. Fabricating a TEB at the GS base requires
  segment-register control userspace does not have on this target; our own
  toolchain builds with `-femulated-tls` precisely because of this
  [LOCAL-SOURCE: CAP9-CPPMODS-DESIGN.md §2c item 7]. A mod executing
  `gs:[…]` reads kernel per-CPU data → garbage/crash. Presence of a TLS
  directory → install-time refusal, named reason. TLS callbacks are
  therefore also refused with it (they exist only inside a TLS directory;
  task item 1e is answered honestly: **not feasible without kernel
  support; refused, not approximated**).
- **Dynamic TLS (`TlsAlloc`/`TlsGetValue`/`TlsSet`/`TlsFree`): EMULATE.**
  These are plain kernel32 calls, not PE TLS: a slot table
  (pthread_key or a per-thread array indexed by handle) is trivial and
  included in the T1 shim (§5). This distinction matters because mods that
  *call* TLS APIs are common; mods with a *static TLS directory* are the
  refuse class.

### 3f. Entry point (`DllMain`) — feasible

MSVC-linked DLLs expose `_DllMainCRTStartup` at `AddressOfEntryPoint`;
calling it with `(image_base, DLL_PROCESS_ATTACH=1, NULL)` runs CRT
initialization, C++ static-duration constructors and `atexit` registration,
then calls the author's `DllMain` [TRAINING-KNOWLEDGE — stable CRT
behavior]. So one bridged call (SysV→Win64, §4a) gets static-init for
free. Exit path: on mod stop we call the entry with
`DLL_PROCESS_DETACH=0` only if the PE also exports a working
`uninstall_mod`; otherwise detach is skipped and logged (the image stays
resident anyway). `/MD` DLLs additionally import CRT bootstrap symbols
(`__security_init_cookie`, `_CRT_INIT` …) — census-driven tier in §5.

### 3g. Exports — feasible

Parse `DataDirectory[Export]`: name table + ordinal base. Route selection
reuses the host's own shape: `mbm_mod_entry_v1` present → already-SDK PE
(no C++ bridge needed at all — a Windows-built SDK mod goes straight to
the host); else `start_mod`/`uninstall_mod` → upstream C++ ABI, which is
§6's problem. x64 exports of the lifecycle pair are undecorated C names
[LOCAL-SOURCE: CAP9 §0, vendored dllmain.cpp pattern], so plain string
match works.

---

## 4. THE ABI BRIDGE (task §2)

### 4a. C functions — feasible, small asm, signature-bounded

Register/stack deltas (both conventions fully documented; stable facts):

| | Win64 | SysV x86_64 |
|---|---|---|
| int arg regs 1-4 | RCX, RDX, R8, R9 | RDI, RSI, RDX, RCX |
| int arg regs 5+ | stack at `[rsp+0x28+8k]` | stack at `[rsp+8+8k]` |
| float args | XMM0-3, positional count shares the 4-slot budget | XMM0-7 interleaved by position |
| stack args base | `[rsp+0x28]` (32 B shadow space) | `[rsp+8]` |
| return | RAX (≤8 B), else hidden RCX sret | RAX:RDX (≤16 B trivial), else hidden RDI sret |
| non-volatile | RBX, RBP, RSI, RDI, R12-15, **XMM6-15** | RBX, RBP, R12-15 |

Two directions, two different problems:

- **host → guest** (we call the mod: `DllMain`, `start_mod`, virtual
  slots, thread start routines): the guest preserves *more* non-volatiles
  than SysV requires, so the only work is the register remap, building the
  32 B shadow space, and stack-arg shifting. A single asm stub family
  `win64_call0..call6(+float variants)` covers every host→guest call.
- **guest → host** (mod calls our shim imports): the thunk is a Win64
  callee at entry but must call a SysV body that may legally clobber
  RSI/RDI/XMM6-15 — which the Win64 *caller* expects preserved. Every
  shim export therefore goes through a thunk that saves RSI, RDI and
  XMM6-15, remaps RCX→RDI, RDX→RSI, R8→RDX, R9→RCX, copies stack args
  down by 0x20, calls the SysV body, restores, returns in RAX.
  Frame cost ≈ 32 B GP + 160 B XMM — negligible per call.

**Why no libffi-class machinery is needed**: every crossing has a
*documented signature by construction*. Guest→host crossings are shim
functions we author (kernel32/ucrt/UE4SS surface — we know the
prototypes). Host→guest crossings are the lifecycle exports, virtual
slots of classes whose headers we hold (vendored UE4SS headers
[LOCAL-SOURCE]), and documented Win32 callback shapes
(`CreateThread` start routine, enum callbacks). Arbitrary unknown
signatures never cross. The rare 9-16-byte aggregate returns (Win64:
hidden sret; SysV: RAX:RDX) are a descriptor flag; the MVP refuses
crossings that need unhandled aggregate modes with a named diagnostic.

MSVC `this`-calls on x64 are plain Win64 (this in RCX) — covered by arg0
remap; no separate thiscall exists on x64 [TRAINING-KNOWLEDGE, stable].

### 4b. C++ virtual calls — feasible for the classes we hold, in one direction

- Guest vtable (mod object, host calls in): MSVC vtables are a bare array
  of function pointers, vptr at offset 0, no `-16` Itanium header; slot
  order = declaration order with the ctor/dtor pairs at declaration
  position. For single-inheritance classes whose dtor is declared first —
  exactly `CppUserModBase`'s shape [LOCAL-SOURCE: CAP9 §0,
  CppUserModBase.hpp ~14 virtuals] — host-side slot indices derived from
  the vendored header are correct [INFERENCE, CAP9 §2c item 2]. Each slot
  holds guest (Win64) code: host dispatch is just `win64_callN(slot,
  object_ptr, …)` — trivial.
- Host-provided vtables (shim objects the mod calls into, e.g. the
  `CppUserModBase` base subobject's vftable the base-ctor installs): we
  must emit a *Win64-viewable* vtable image — an array of pointers, each
  pointing to a guest→host thunk that reaches our SysV implementation.
  MSVC vtable images and mangled vftable symbol names can be **pinned on
  the Mac**: compile the real vendored header with
  `clang++ -target x86_64-pc-windows-msvc -S` and read the emitted
  `.rdata "?vftable@…" qword list`. Verified today that this toolchain
  dumps MSVC record layouts and emits MSVC-mangled symbols:
  [LOCAL-VERIFY] `clang++ -target x86_64-pc-windows-msvc -fsyntax-only
  -Xclang -fdump-record-layouts` on this Mac prints
  `*** Dumping AST Record Layout` with the vftable pointer at offset 0;
  and `-S` emits `"?call@@YAXPEAUCppUserModBase@RC@@@Z"` — authoritative
  MSVC mangling, no Windows required. The pinning tool is staged at
  `native-loader/tools/pin_msvc_layout.sh`.
- Multiple inheritance / virtual bases / covariant returns: MSVC uses
  adjustor thunks and vtordisp frames; the Itanium side differs
  structurally. Policy: **refuse at install** any mod whose crossing
  classes have these shapes (the classifier detects them via
  `?_7…@@6B@` secondary-vtable symbols and RTTI hierarchy descriptors in
  the census). `CppUserModBase` and the UE4SS boundary types are
  single-inheritance [LOCAL-SOURCE: vendored headers], so the MVP
  envelope loses little.

### 4c. Exceptions / SEH — NOT feasible for the MVP; fail-closed instead

Honest assessment (task asks for exactly this):

- MSVC x64 C++ EH = table-based SEH: `.pdata` `RUNTIME_FUNCTION` binary
  search → `.xdata` unwind-code interpreter → `__CxxFrameHandler4`
  language handler over `FuncInfo`/scope tables, plus funclet frames and
  `RtlUnwindEx`-style two-pass unwind. Mod-internal `try/catch` needs the
  whole dispatcher; `__try/__except` needs `__C_specific_handler`; both
  additionally assume a TEB for the exception chain (gs-relative).
- It is *proven possible* as an engineering project — Wine implements
  x64 SEH on non-Windows hosts, and LLVM's libunwind has an SEH backend —
  but it is a multi-month subsystem with a TEB dependency at its core.
  **Out of scope for the MVP; do not design around it.**
- What the MVP does instead:
  1. Normal control flow never consults EH tables — only throws and
     `__try` do. An `/EHsc`-built mod that *never actually throws* runs
     correctly with EH machinery absent [INFERENCE; the tables are dead
     data].
  2. EH runtime imports (`__CxxFrameHandler4`, `__C_specific_handler`,
     `RaiseException`, `_CxxThrowException`, `RtlUnwind`,
     `_except_handler4`) get **fail-closed stubs**: log a precise
     diagnostic ("mod executed a C++ throw / SEH raise; EH is not
     supported; mod stopped") and terminate the *mod* per NativeModAbi.h
     law 3 — never the game, never a silent no-op.
  3. `__try/__except` reliance is detectable statically: on x64,
     scope-table frames show up as `.pdata` entries with handler flags
     (`UNW_FLAG_EHANDLER/UHANDLER`) [TRAINING-KNOWLEDGE, PE format spec] —
     a classifier requirement handed to JG (§10).
- dynamic_cast: within the mod's *own* classes it is a pure data walk in
  the mod's image (works without OS support). Across to shim objects it
  needs MSVC `CompleteObjectLocator`s — out of MVP; crossing classes are
  RTTI-free by construction, and the shim vtables will carry no COLs, so
  the install-time envelope states "no dynamic_cast across the boundary".

---

## 5. THE WINDOWS API SHIM (task §3)

Tiers; each function is one registry row (`dll`, `symbol-or-ordinal`,
`thunk`, `impl`, `mode`). Modes: `MAP` (real implementation), `NOOP-LOG`
(harmless, returns benign value), `FAIL-CLOSED` (precise diagnostic +
mod stop), `REFUSE` (load-time refusal).

### 5a. What real UE4SS DLL mods import — now measured, not guessed

**[LOCAL-SOURCE: JG's census of a real UE4SS.dll —
`../REPORT/evidence/UE4SS.dll.analysis.json` (20.5 MB image, sha256
94c2ebd3…)]**: 634 imports across 29 DLLs — MSVCP140(207), KERNEL32(123),
USER32(82), the api-ms-win-crt set (~130), VCRUNTIME140(24),
MSVCP140_ATOMIC_WAIT(13), GDI32(11), SHELL32(5), IMM32(4), plus singles
(d3d11, D3DCOMPILER_47, dbghelp, bcryptprimitives, PSAPI,
VCRUNTIME140_1). Exact real names that matter to the tiers:

- **EH runtime lives in TWO DLLs**: `VCRUNTIME140_1.dll!__CxxFrameHandler4`
  (the x64 C++ handler) AND `VCRUNTIME140.dll` carries `__CxxFrameHandler3`,
  `__C_specific_handler`, `_CxxThrowException`, `__std_terminate`,
  `__RTtypeid`, **`__RTDynamicCast`** (dynamic_cast crosses as a real
  import, §4b/4c), and **`__current_exception` / `__current_exception_context`**
  — TEB-coupled state, real-data confirmation of the §3e/4c wall.
- `longjmp`/`__intrinsic_setjmp` are imported (VCRUNTIME140): setjmp/longjmp
  is implementable without SEH (register-stack restore) — a small T1-class
  shim, not part of the refused EH set; decide in M1 against the census.
- KERNEL32 confirms the T1/T2/T3 tier split and adds: file-mapping class
  (`CreateFileMappingW`/`MapViewOfFile`/`UnmapViewOfFile` → mmap, mappable),
  tool-help/thread introspection (`CreateToolhelp32Snapshot`, `Thread32*`,
  `OpenThread`, `QueueUserAPC` → registry-backed stubs or refuse), console
  (`AllocConsole`, `GetStdHandle`, `GetConsoleMode` → log-backed NOOP),
  `AddDllDirectory`/`RemoveDllDirectory` (registry), `VirtualQuery`
  (bounded), `GlobalMemoryStatusEx` (mappable).
- `WaitOnAddress`/`WakeByAddress*` (api-ms-win-core-synch-l1-2-0): futex
  class, condvar-emulatable (T1).
- **The framework image itself carries a TLS directory** (40 bytes =
  exactly `sizeof(PeTlsDirectory64)`, §3e) — the refusal rule applies to
  TLS-bearing images regardless of who built them.
- USER32(82)/GDI32(11)/IMM32(4)/d3d11: the GUI/console-render surface —
  phase 2+ via the cap11 ImGui route, out of the headless MVP envelope.

A MOD's census is bounded by what it actually references (UE4SS.dll
surface + its own CRT subset), typically far below the framework's 634;
JG's per-mod census replaces the mappability estimate in §7a with
numbers. What real mods import [TRAINING-KNOWLEDGE, pending per-mod
census]: the kernel32/sync/thread/time/text/file classes in the tier
table below (authored before the census; the census is now the truth).

### 5b. Mappable to PS5 equivalents — the large majority

- T1 `MAP` (mechanical): memory → `malloc/mmap`; critical sections →
  `pthread_mutex`; events → condvar wrapper; `CreateThread` →
  `pthread_create` with a host→guest bridged start routine; `TlsAlloc`
  family → emulated slot table (§3e); atomics → `__atomic` builtins;
  time → `clock_gettime(CLOCK_MONOTONIC)`; text conversion → local
  UTF-8/UTF-16 codec; `OutputDebugString`/`GetLastError/SetLastError` →
  log / thread-local error slot; `IsDebuggerPresent` → `return 0`;
  `EncodePointer` → per-process cookie xor (self-consistent pair).
- T2 `MAP` with the **GA path-shim rules** (drive-letter drop, `\`→`/`,
  `Binaries/Win64` + `../` virtual-cwd collapse, write confinement to the
  mod's folder under `/data/modbridge/roots/<title-id>/`)
  [LOCAL-SOURCE: CAP9 §1c.2, agent-GA-path-shim.md rules]: the whole
  `CreateFileW` family onto `fopen`/`open` + rules.
- T3 `MAP` (module registry): `GetModuleHandleW(L"UE4SS.dll")` → handle
  of the ue4ss_compat surface (§6); game exe → sentinel handle;
  `GetProcAddress` → shim/host alias table lookup only.

### 5c. Stub implementations (bounded, honest)

- `NOOP-LOG`: `SetUnhandledExceptionFilter` (accept+ignore, log — the
  host owns crash policy), `SwitchToThread` → `sched_yield`, `Sleep` →
  `usleep`.
- `/MD` CRT bootstrap tier: `__security_init_cookie`,
  `EncodePointer/DecodePointer`, `_CRT_INIT`, `_except_handler4`
  (fail-closed body — installed only because presence ≠ use, §4c).

### 5d. Fundamentally incompatible — refuse or fail-closed

- `FAIL-CLOSED`: the EH/SEH runtime set (§4c).
- `REFUSE` (load-time, named): any import that rewrites game code —
  `VirtualProtect`/`VirtualAlloc` with EXEC intent on game pages,
  `WriteProcessMemory`, `FlushInstructionCache` (cap13 limit law, §1);
  `user32`/`gdi32` windowing/input surfaces (headless MVP — key input is
  cap6's controller route; a mod hard-requiring user32 is out of
  envelope); TLS-directory-bearing images (§3e); CLR/COM descriptors
  (cap10 domain).
- Estimated coverage: of a typical census, roughly **85-95% of distinct
  symbols are MAP/NOOP**, the residue is exactly EH + exec-memory +
  TEB-adjacent [INFERENCE + TRAINING-KNOWLEDGE; the census replaces this
  estimate with numbers per mod].

---

## 6. THE UE4SS INTERFACE (task §4)

### 6a. What upstream UE4SS.dll exports

[LOCAL-SOURCE for shape; TRAINING-KNOWLEDGE for the exact PC export
list]: the `RC_UE4SS_API` surface — `CppUserModBase` ctor/dtor,
`register_hook`-class members, `register_keydown_event`; the
`Unreal::` namespace (`FindFirstOf`, `FindAllOf`, hook registration
classes, `CustomClass`/`BP` helpers); `RC::Helpers`; LuaMadeSimple
accessors; GUI tab registration (`UE4SS_ENABLE_IMGUI` builds export the
Dear ImGui C++ surface). All of it is **MSVC-mangled C++**
(`??0CppUserModBase@RC@@QEAA@XZ` …) [LOCAL-SOURCE: CAP9 §0/§2b — mods
link `UE4SS` via CMake and the port deliberately exports no such
symbols].

### 6b. How the PS5 port provides it — two halves, census-driven

**Half 1 — services (mod calls UE4SS → host does work):** every
lifecycle/lookup/hook call behind the exported surface already exists on
the other side of `mbm_host_v1` — `find_object`, `find_first_of`,
`find_all_of`, `object_*`, `property_get/set/address`,
`call_function`, `hook_admission`, `register_*`, `execute_in_game_thread`,
plus the GUI table `mbm_gui_v1` [LOCAL-SOURCE:
NativeModAbi.h:203-240]. `ue4ss_compat` implements each MSVC-mangled
export as: guest→host thunk → argument demarshal (MSVC boundary types →
plain C) → `mbm_host_v1` call → marshal back. **Coverage is the selected
mod's import census + margin** — no complete UE4SS.dll reimplementation
(cap13 law: no unused Windows/framework surface before the first mod
works).

**Half 2 — boundary objects (the C++ ABI crossing):**

- Game-memory types (`UObject`, `FName`, `FString`, `FOutputDevice`…)
  are *the game's own MSVC layouts* — the same bytes the PS5 game
  binary uses. UE4SS-style field access matches by construction, and the
  port's cap2 seed/pin profiles already locate the pool/array anchors
  [LOCAL-SOURCE: CAP9 §4 "#2 scanner/profiles"]. These types cross as
  opaque pointers plus documented layout knowledge — **compatible**.
- Host-C++ types in the boundary (`std::wstring StringType` fields,
  `std::vector<std::shared_ptr<GUITab>>`) are where MSVC(32 B SSO-8)
  vs libc++(24 B SSO-10) diverge [LOCAL-SOURCE: CAP9 §2c item 5]. The
  shim therefore constructs the `CppUserModBase` **base subobject at
  MSVC layout** (offsets/sizes pinned Mac-side via
  `tools/pin_msvc_layout.sh`):
  - base ctor shim body: install the shim vftable image (§4b), value-init
    the five `StringType` fields as *empty MSVC strings* and the vector
    header, set metadata via guest-side string construction where the mod
    will overwrite it anyway;
  - **allocator-pairing rule** (the subtle one): any shim code that must
    destroy/free memory the *guest* allocated resolves the guest's own
    `free`/`operator delete` import address at load time and calls it
    through the bridge — shim allocator never frees guest heap and vice
    versa. The base-dtor shim (runs at `uninstall_mod`) uses exactly
    this.
  - mod display name/version: read from the manifest row, **not** by
    parsing MSVC `std::wstring` internals (defers the last STL-layout
    dependency; noted honestly).

**Versioning risk [TRAINING-KNOWLEDGE]**: upstream UE4SS headers evolve;
the mod was built against *its* vendored header set. The shim pins one
header set (the vendored one in this tree) and the classifier records
which UE4SS version tag the mod manifest claims; mismatch = refuse with
diagnostic. Slot-count fingerprints (vftable symbol sizes in the mod's
rdata) make drift detectable at install.

### 6c. Compatibility verdict

- UObject/FName/FString crossing: **compatible** (same game bytes).
- CppUserModBase object boundary: **feasible** at MSVC layout for the
  known header set, census-driven service coverage; this is real
  engineering, bounded by the selected mod's imports, not open-ended.
- Anything built on GUI tabs: **phase 2+** — rides `mbm_gui_v1`'s shared
  ImGui context [LOCAL-SOURCE: NativeModAbi.h:177-200], but the ImGui
  C++ surface must then also exist under MSVC mangling — large,
  deferred, honestly stated.
- Lua interop surface, `on_lua_start`: phase 2+ (host table has no Lua
  arm today).

---

## 7. FEASIBILITY ASSESSMENT (task §5)

### 7a. Percentages (estimates; labeled as such)

- **Envelope mods** (headless; no static TLS; no `__try`; no executed
  throw path; no GUI; no game-code patching; C++ surface within the
  pinned header set; file IO only under T2 rules): if J1 passes, I
  expect **~60-80% of mods inside that envelope to reach "intended
  visible action"** — the residue being census gaps and CRT tiers.
- **All UE4SS DLL mods**: well under **~30%** until GUI (§6c) and
  deeper CRT tiers land; mods whose logic is 90% ImGui tabs would run
  headless-but-useless, which the support matrix states as such.
- Non-UE4SS families: ASI/C-flat plugins become reachable with the *same*
  mapper+bridge+T1-T3 (they are the §3-§5 machinery without §6) — a real
  but later unlock; SKSE/SHV remain blocked on missing host layers
  [LOCAL-SOURCE: CAP9 §1f] regardless of this loader.

### 7b. Fundamental blockers (no design around them)

1. J1 exec-memory unknown (§9) — binary gate on the whole route.
2. Static TLS / TEB (§3e) — refused, permanent within userspace rules.
3. SEH/EH execution (§4c) — refused-then-fail-closed; implementing it is
   a separate capability with a TEB dependency at its core.
4. Game-code rewriting mods (§1) — refused by law until a separately
   established capability exists.
5. Closed-source host frameworks (ScriptHookV-class) — no interface to
   provide [LOCAL-SOURCE: CAP9 §1f].

### 7c. Worth building? — yes, scope-capped

CAP13's acceptance **requires** original-bytes loading; #9's recompile
route can never satisfy it ("A rebuilt native module is a #9 result, not
this proof" [LOCAL-SOURCE: ZCODE-ASSIGNMENT.md]). The MVP is also cheap
where it counts: the loader+bridge+T1-T3 layers are ordinary code,
Mac-testable end-to-end except the J1/J2 console unknowns, and reusable
beyond UE4SS (C-flat ASI plugins). What we must NOT build: full EH, full
UE4SS export parity, user32 surfaces "for later". The cap13 law is
explicit: no unused surface before the first mod works.

### 7d. Minimum viable first mod (the envelope, concretely)

UE4SS C++ mod, `/MT`, whose `DllMain` does nothing beyond CRT init,
whose `start_mod` returns a `CppUserModBase`-derived object, and whose
entire behavior rides: ctor metadata + `on_update`/`on_unreal_init`
virtuals + `register_keydown_event` + a handful of `Unreal::FindFirstOf`
/ `property_get/set` / `call_function` calls. No GUI, no file IO, no
threads, no exceptions on live paths, no static TLS directory. The
adapter maps it, bridges the ~5 virtuals the host fires, routes its
census of UE4SS imports to `mbm_host_v1`, and its intended action
(e.g., toggling a property on a found object on keydown) is visible.
That is the first console proof; everything else widens the matrix.

---

## 8. IMPLEMENTATION PLAN (task §6 — what is staged today)

Staged under `native-loader/` (all proposal-only, integrator reviews):

| file | content | state |
|---|---|---|
| `include/pe_loader.h`, `src/pe_types.h`, `src/pe_loader.cpp` | header parsing, section mapping with 16 KiB perm partition, relocation pass, import resolution against a registry, export lookup, TLS refusal, entry-point address | design-grade C++; macOS/PS5 dual-guarded; `-fsyntax-only` clean on this Mac |
| `src/win_api_shim.h/.cpp` | registry + T1/T2/T3 implementations (mutex/event/thread/time/text/files+path rules), fail-closed EH stubs, module registry | design-grade; kernel32 subset |
| `src/win64_bridge.h`, `src/win64_bridge.S` | `win64_call0..6` (host→guest), guest→host thunk with XMM6-15/RSI/RDI preservation, DllMain invoker | asm by hand — **unverified**, needs the M0 host unit test before anything trusts it |
| `src/dllmain_invoker.cpp` | entry invocation + detach policy | design-grade |
| `src/ue4ss_compat.cpp` | boundary-object shims (base ctor/dtor at MSVC layout, allocator-pairing), export-row table shape for the census-driven surface | sketch-grade — the largest open work item |
| `src/pe_adapter.cpp` | `mbm_mod_entry_v1` wrapper around the whole pipeline | design-grade |
| `tools/pin_msvc_layout.sh` | Mac-side MSVC layout/mangle/vtable-image pinning via `-target x86_64-pc-windows-msvc` | **verified working today** [LOCAL-VERIFY] |
| `patches/cppmod_pe_route.patch` | optional host-side routing (alternative to manifest naming) | proposal only |

Milestones:

- **M0 (Mac, no console):** mapper unit tests against a synthetic PE
  generated in-test (bytes constructed in code — JG owns real fixture
  DLLs); thunk round-trip tests (macOS is SysV, guest side simulated with
  hand-assembled Win64 register states); `pin_msvc_layout.sh` output
  captured into the shim's layout table. Exit: mapper+bridge tested green
  on Mac.
- **M1 (Mac):** win_api_shim T1-T3 complete against the census of a real
  mod (JG's census replaces the estimate); ue4ss_compat for the chosen
  envelope; synthetic end-to-end on Mac (map → imports → DllMain →
  start_mod through the bridge → virtual fires).
- **M2 (console — needs Claude/integration, one-launch laws):** J1 probe
  first (tiny payload PRX: anon `mmap`+`mprotect RX`+call); then the
  hello-envelope PE via the MFA install route; then the real selected
  mod; 3-restart acceptance per law. Teardown is a **no-unload** quiesce
  (retained image) + `DLL_PROCESS_DETACH` where safe.
- **M3:** support matrix + test card + cost numbers (cap13 acceptance
  items), second game via the same generic route.

---

## 9. Console unknowns (load-bearing, honest)

- **J1 — executable anonymous memory in the game process.** Can a
  ploader-injected PRX `mmap` anonymous memory and `mprotect` it
  `PROT_EXEC` (and `RWX`)? Unverified; no payload-sdk sample proves it in
  *game-process* context [INFERENCE from CAP9 §7.1 — same
  unknown-shape]. If NO → route dead; the census/classifier work still
  pays off for refusal diagnostics. Run this before any other console
  spend.
- **J2 — fault containment.** Can the process install a SIGSEGV/SIGILL
  handler that covers mod threads for crash isolation? If NO, containment
  is only the fail-closed stubs + host fault isolation that exists
  (NativeMods host isolates attach faults [LOCAL-SOURCE:
  CppMod.cpp:210-220]); a wild guest access may still take the process
  down. The test card must state this risk per rung.
- **J3 — `/MD` CRT bootstrap completeness** (which `_CRT_INIT`-tier
  symbols a real /MD mod demands): census decides; /MT envelope avoids
  it for the first mod.

## 10. Handoff to JG (`pe_analyzer.py` / fixtures) — classifier requirements

Status after JG's delivery: the analyzer already emits per-DLL import
lists (`imports[].dll/.functions`), data directories with RVA/size (the
TLS/CLR checks in §3e are a no-op on its current output), the export
table, and a `ue4ss` block (`imports_ue4ss_dll`,
`exports_ue4ss_mod_entrypoints`). Real fixtures exist
(`../FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll` — x64 PE32+ exporting
`start_mod`/`stop_mod`, importing
`UE4SS.dll!is_ue4ss_initialized` + `KERNEL32.dll!DisableThreadLibraryCalls`;
plus x86 and not-a-PE negatives). Remaining asks (my loader consumes):
1. **Per-symbol tags** in `imports[].functions`: `C` vs `?`-mangled, and
   bucket per DLL — the shim coverage matrix input (§5a/6b).
2. **`.pdata` handler-flag scan**: count unwind infos with
   `UNW_FLAG_EHANDLER|UHANDLER|CHAININFO` → `__try`/SEH reliance class
   (§4c) [TRAINING-KNOWLEDGE on flag bits — verify vs the PE spec].
3. **`.text` gs-prefix byte-scan** (`65 48 8B … 25 <disp32>` + REX
   variants `65 4C`/`65 44`) → TEB-dependent class (§3e); heuristic,
   refuse is the safe direction.
4. **16 KiB permission-partition fact** (§3b): emit
   `rwx_partition_required: bool` from the section table at 16 KiB
   granularity (the page size is mine, the fact is the analyzer's).
5. **Secondary-vtable symbol count** (`?_7…@@6B@` beyond the primary) and
   RTTI descriptor presence → MI/virtual-base refusal class (§4b).
6. Keep the analyzer read-only (byte-identity law).

Fixture interface the loader's self-test consumes: PE path +
expected-census JSON + expected route verdict. JG's x64 fixture is the
first cross-tool agreement target (my mapper's export lookup must find
its `start_mod`/`stop_mod`; its two imports must resolve through a
registry seeded with those exact rows; relocs must apply under a slide).

## 11. Staleness register (most-stale first)

1. MSVC ABI micro-facts quoted from training (shadow space layout, XMM
   non-volatile range, EH flag bit values, MSVC `std::wstring` 32 B
   shape): stable-but-unpinned → the design **derives layout/vtable/mangle
   facts via `tools/pin_msvc_layout.sh` instead of trusting memory**
   [LOCAL-VERIFY that the mechanism works].
2. Upstream UE4SS.dll export list + current mod-built header drift:
   version-sensitive; census per mod is the truth. The framework census
   (§5a) is [LOCAL-SOURCE] as of 2026-10-13 for ONE UE4SS.dll build
   (sha256 94c2ebd3…) — other UE4SS builds will differ.
3. Delay-helper internals (§3d) and CRT bootstrap symbol tiers (§3f):
   verify against a real /MT and /MD census before M1 exit.
4. Wine/LLVM SEH precedent (§4c): architectural position only; re-verify
   before ever citing as a plan.
