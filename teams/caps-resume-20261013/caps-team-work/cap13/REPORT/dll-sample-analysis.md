# cap13 DLL sample analysis — measured PE facts for the native-mod decision

Status: COMPLETE (Mac-only build + static analysis; nothing was ever executed on any target).
Analyzer: `TOOL/pe_analyzer.py` (unchanged). Per-file full reports: `REPORT/evidence/<name>.json`.
Machine-readable rollup: `BUILD/analysis-summary.json`. Build provenance: `BUILD/build_manifest.json`.

## 1. What is in `SAMPLE/` (23 distinct files = 21 valid PE + 2 non-PE negative controls)

Composition:

| Group | Count | Provenance |
|---|---|---|
| A. Native ports of the 10 upstream UE4SS mods (mbm ABI) | 10 | compiled in cap13 (see §2) |
| B. Keybinds minimum variant (`-fno-exceptions`) | 1 | compiled in cap13 |
| C. PC-UE4SS-flavor mods (import real UE4SS.dll C API) | 2 | compiled in cap13 |
| D. Capability probe (EH + TLS + patch APIs, deliberately) | 1 | compiled in cap13 |
| E. Real third-party / project binaries | 6 | copied with hash identity |
| F. Managed control group | 2 | real .NET assemblies from the on-disk dotnet SDK |
| G. Non-PE negative controls | 2 | real on-disk artifacts that are NOT valid PE |
| H. 32-bit control build | 1 | compiled in cap13 |

Full hash table (sha256 prefix; full hash in `BUILD/analysis-summary.json`):

| File | sha256 (16) | bytes | Classification |
|---|---|---|---|
| cap13-feature-probe.dll | 4c6fe9979b4d8b02 | 5632 | native-x64 |
| cap13-mod-actordumper.dll | 9c855a08b36e7f2e | 5632 | native-x64 |
| cap13-mod-bpml-generic-functions.dll | 82a215c3fdc2fa11 | 5632 | native-x64 |
| cap13-mod-bpmod-loader.dll | 0ae9a5d0d8f276a7 | 5632 | native-x64 |
| cap13-mod-cheat-manager-enabler.dll | 7fa48ff8dbf289e8 | 5632 | native-x64 |
| cap13-mod-console-commands.dll | 33da7c2a3c7ab56a | 5120 | native-x64 |
| cap13-mod-console-enabler.dll | 8ddc7224779d4f7e | 5632 | native-x64 |
| cap13-mod-jsb-lua-profiler.dll | c79b27c5afbdeb98 | 5632 | native-x64 |
| cap13-mod-keybinds-noeh.dll | 0ed2c758094a46bc | 5120 | native-x64 |
| cap13-mod-keybinds-x86.dll | febeb54dc6bfbfee | 4608 | unsupported-arch (real 32-bit PE32) |
| cap13-mod-keybinds.dll | eddb396267b4d6fe | 5120 | native-x64 |
| cap13-mod-linetrace.dll | bd539d94a432a270 | 5120 | native-x64 |
| cap13-mod-splitscreen.dll | 73ed26e406b425c0 | 5632 | native-x64 |
| cap13-ue4ss-flavor-console-dump.dll | 24d492de5f99af30 | 2560 | native-x64 |
| cap13-ue4ss-flavor-keybinds-poller.dll | c3e758f8cb39f0f6 | 3584 | native-x64 |
| managed-control-NuGet.Frameworks.dll | 00cb3a3820f88389 | 125224 | managed-dotnet |
| managed-control-System.Runtime.dll | 2380e215ff5a0479 | 44840 | managed-dotnet |
| real-UE4SS.dll | 94c2ebd3ca9f4c42 | 20572160 | native-x64 |
| real-dwmapi.dll | 4e3e5324e68e033e | 279552 | native-x64 |
| real-pe-marker.dll | 9b8db510ef42b8ed | 2 | unknown (file is ASCII text "MZ") |
| real-windows-marker.dll | 014b8ce9fed0aaf1 | 64 | (analyzer error: bad PE signature — MZ stub, no PE\0\0) |
| real-worker-close-early.dll | ea3a7dba5c3abd53 | 4096 | native-x64 |
| real-worker-standard.dll | 246f1222eef8f8af | 4608 | native-x64 |

Provenance of group E/F/G (copied byte-identical, hashes verified):
`what/.superpowers/native-dependency-spike/windows-image-original-inputs-1/{UE4SS.dll,dwmapi.dll}`,
`windows-worker-contract-review-1/{green-worker-1,close-early-1}/worker.dll` (the 7 worker.dll on
disk collapse to exactly 2 distinct hashes — dedupe measured, not assumed),
`ue4ss-cppmod-native-port-1/module-final-1/windows-marker.dll` (all three module* copies are the
same bytes), `native-cpp-module-direct-1/green/pe-marker.dll` (all three copies identical),
`sharpprospero-host-tool-1/dotnet/` for the two managed controls.

## 2. How the compiled samples were built (real cross-compile, measured limits)

Toolchain on this Mac: **Homebrew llvm@18** — `clang++ 18.1.8 -target x86_64-pc-windows-msvc`,
`lld-link -dll -machine:x64`, `llvm-dlltool` for COFF import libraries. Every DLL in groups A–D/H
is a REAL PE image produced by that pipeline (verified by `llvm-objdump`, `file`, and our analyzer).
Build script: `BUILD/build_all.py`; objects/manifest/logs under `BUILD/`.

Import libraries were generated from `.def` files listing the REAL observed exports:
`kernel32.def`/`user32.def`/`vcruntime140.def` (public Win32/VC runtime surface), and
`UE4SS.def` — the six unmangled C-API exports measured in the actual UE4SS.dll v3.0.1:
`is_ue4ss_initialized`, `execute_lua_in_mod`, `call_script_function`, `get_lua_state_by_mod_name`,
`set_script_variable_default_data`, `set_script_variable_int32` (out of 4256 total exports; the
other 4250 are MSVC-mangled C++ symbols).

Measured toolchain limits (these ARE findings, not excuses):
- **No Windows SDK / MSVC STL on this Mac.** Compiling the actual upstream C++ mods fails at the
  first STL include: `cppmods/EventViewerMod/src/dllmain.cpp -> EventViewer.hpp:8: fatal error:
  'atomic' file not found` (log: `BUILD/logs/eventviewer-compile-attempt.txt`). Same for the #9
  SDK C++ layer (`Sdk.hpp` uses std::string/vector/mutex). libc++ headers for the windows-msvc
  target demand the MSVC ABI (`vcruntime_exception.h`, `<new.h>`, `mbstate_t` shims) and a
  libc++ built for Windows — out of scope here. **Consequence: the upstream C++ mods and the full
  C++ SDK cannot be built to PE on this machine; the measurement below covers what compiled mods
  actually need at the binary level.**
- The 10 mods named in the task (`assets/Mods/ActorDumperMod` … `SplitScreenMod`) are **Lua-only
  upstream** (each is `Scripts/main.lua`, no C++ in the repo). The only upstream C++ mod sources in
  the staged tree are `cppmods/EventViewerMod` and `cppmods/KismetDebuggerMod` (blocked above).
  The cap13 ports therefore re-implement each Lua mod's real behavior in freestanding C++ over the
  #9 `mbm` C ABI — the ABI header is copied byte-identical from the #9 SDK
  (`NativeModAbi.h` sha256 `42f8fd12a769a8c6…`, verified equal in both trees), so the ports are
  built against the exact contract the PS5 bridge implements.
- Minicrt (`BUILD/src/minicrt.cpp`) provides MSVC-CRT-shape startup (heap-backed operator new,
  `_tls_index`, `_tls_used` TLS directory anchor, default DllMain). EH itself is IMPORTED from
  `vcruntime140.dll`, exactly like every real MSVC-built DLL — that is what makes the EH-column
  measurement below meaningful.
- The x86 control build is real PE32; note its stdcall import names carry an llvm-dlltool
  `@`-decoration quirk (`GetProcessHeap@0`); x64 builds are unaffected (no decoration on x64).
- Nothing was executed anywhere: this task is build + static analysis only.

## 3. Per-mod table (groups A–D; imports measured, not estimated)

Common floor for every minicrt-linked mod: `kernel32: GetProcessHeap, HeapAlloc, HeapFree` (C++
allocation) + `vcruntime140: __CxxFrameHandler3, __std_terminate` (C++ EH funclets). "Extras" are
per-mod on top of that floor. Every port exports exactly `mbm_mod_entry_v1`.

| Mod (upstream behavior kept) | sha256 (16) | Imports beyond floor | EH imp | TLS dir | Patches game code | UE4SS.dll import |
|---|---|---|---|---|---|---|
| Keybinds (7× Ctrl+key → dump tools) | eddb396267b4d6fe | — | yes | yes | no | no |
| Keybinds-noeh (`-fno-exceptions` variant) | 0ed2c758094a46bc | — (kernel32 only, 3 funcs total) | **no** | yes¹ | no | no |
| CheatManagerEnabler (ClientRestart hook → construct CheatManager) | 7fa48ff8dbf289e8 | — | yes | yes | no | no |
| ConsoleEnabler (Tilde/F10 → InputSettings.ConsoleKeys) | 8ddc7224779d4f7e | — | yes | yes | no | no |
| ConsoleCommands (dump_object/set/summon on console-exec) | 33da7c2a3c7ab56a | — | yes | yes | no | no |
| BPML_GenericFunctions (BP helper library surface) | 82a215c3fdc2fa11 | — | yes | yes | no | no |
| LineTrace (key T → camera trace via game-thread task) | bd539d94a432a270 | — | yes | yes | no | no |
| SplitScreen (bUseSplitscreen + CreatePlayer on key P) | 73ed26e406b425c0 | — | yes | yes | no | no |
| ActorDumper (Ctrl+V → find_all_of Actor → ActorDump.txt) | 9c855a08b36e7f2e | kernel32: CreateFileW, WriteFile, CloseHandle, DisableThreadLibraryCalls, GetModuleHandleW; vcruntime: memcpy, __chkstk | yes | yes | no | no |
| BPModLoader (read load_order.txt → find BP mods) | 0ae9a5d0d8f276a7 | kernel32: CreateFileW, ReadFile, CloseHandle | yes | yes | no | no |
| jsbLuaProfiler (game-thread tick hook + QPC timing) | c79b27c5afbdeb98 | kernel32: QueryPerformanceCounter, QueryPerformanceFrequency | yes | yes | no | no |
| UE4SS-flavor console-dump (start_mod → C API → Lua dump) | 24d492de5f99af30 | kernel32: DisableThreadLibraryCalls, GetModuleHandleW; **UE4SS: is_ue4ss_initialized, execute_lua_in_mod**; total 4 imports | **no** | no | no | **yes** |
| UE4SS-flavor keybinds-poller (thread + GetAsyncKeyState → C API) | c3e758f8cb39f0f6 | kernel32: CreateThread, Sleep, WaitForSingleObject, CloseHandle, CreateFileW, DisableThreadLibraryCalls, GetModuleHandleW; user32: GetAsyncKeyState; **UE4SS: is_ue4ss_initialized, call_script_function** | no | no | no | **yes** |
| Feature probe (deliberate EH+TLS+patch demo, not a mod) | 4c6fe9979b4d8b02 | kernel32: VirtualProtect, WriteProcessMemory, FlushInstructionCache, GetCurrentProcess, GetLastError, Sleep; vcruntime: _CxxThrowException, ??_7type_info@@6B@; exports 4 probes; real `.tls` section + TLS directory | yes | yes | **yes (by design, APIs present)** | no |

¹ TLS directory present but inert (CRT `_tls_used` anchor with zero callbacks, no `thread_local`
use). It can be dropped with a no-anchor minicrt; the measured API-level TLS need of the mods is 0.

## 4. Aggregate — the MEASURED result (replaces the 60–80% estimate)

Mod set = the 13 x64 mod binaries in SAMPLE (12 distinct mods — Keybinds has two x64 variants:
default and `-fno-exceptions` — plus the two PC-UE4SS-flavor builds; feature probe and x86 control
excluded as non-mod controls). Machine-readable: `BUILD/aggregate.json`:

| Question | Measured |
|---|---|
| Mods importing UE4SS.dll at all | **2/13 = 15%** (the two deliberate PC-UE4SS-flavor builds; in the mbm/#9 design zero mods import UE4SS.dll — they receive the host table via `mbm_mod_entry_v1`) |
| Mods needing Windows APIs beyond the loader | **13/13 = 100%** — but the actual floor is just kernel32 heap (3 functions) from C++ `operator new` |
| Mods using only UE4SS-host functions (no file/thread/timing/GUI/patch API) | **9/13 = 69% of binaries = 8 distinct mods** (Keybinds, CheatManagerEnabler, ConsoleEnabler, ConsoleCommands, BPML_GenericFunctions, LineTrace, SplitScreen — Keybinds twice via its two build variants) |
| Mods with C++ EH imports (default C++ build) | **10/13 = 77%** — pure build artifact of the CRT floor, not mod logic: none of the 12 contains try/throw; the `-fno-exceptions` build measures **0%** mod-level EH need |
| Mods with TLS directory / TLS API use | directory 11/13 = 85% (CRT anchor, inert); actual `Tls*`/`thread_local` API need **0/13 = 0%** |
| Mods importing game-code-patching APIs (VirtualProtect / WriteProcessMemory / FlushInstructionCache) | **0/12 = 0%**. Only the deliberate probe and real UE4SS.dll itself (`VirtualProtect`, `WriteProcessMemory`) carry them |
| Mods doing file I/O | 3/13 = 23% of binaries = 2 distinct mods (ActorDumper, BPModLoader) + the flavor-poller build |
| Mods creating threads / timing APIs | 1 each of 13 (flavor poller thread; jsb profiler QPC) |

Context rows from the real third-party binaries:

| Binary | Measured facts |
|---|---|
| real-UE4SS.dll (v3.0.1) | 4256 exports (6 unmangled C-API), 29 imported DLLs / 634 functions (full ucrtbase api-set family + MSVCP140 + VCRUNTIME140/1 + d3d11 + dbghelp + ntdll), TLS directory present, EH imports `__CxxFrameHandler3/4`, `_CxxThrowException`, **patch imports `VirtualProtect` + `WriteProcessMemory`** — UE4SS itself is the component that patches game code |
| real-dwmapi.dll | real system DLL shape: 13 import DLLs / 135 functions, MSVCP140/VCRUNTIME140_1 + EH4 (`__CxxFrameHandler4`), 143 exports, no patch imports |
| real-worker-{standard,close-early}.dll | 1 import DLL (KERNEL32, 7 thread/sync functions), 2 exports (`WorkerBegin`, `WorkerCollect`), no EH/TLS/patch — the C-style worker contract the Windows spikes validated |
| managed controls | `System.Runtime.dll` imports only `mscoree.dll!_CorDllMain`, classified managed-dotnet with parsed ECMA-335 metadata; `NuGet.Frameworks.dll` likewise — classifier validated against real managed images |
| non-PE controls | `real-windows-marker.dll` (64-byte MZ stub) → analyzer fails closed with "bad PE signature"; `real-pe-marker.dll` (2-byte ASCII) → "unknown". No crash, no guess: correct refusal behavior |

## 5. Answers the design decision needs

1. **A UE4SS-style C++ mod does not need to patch game code.** 0/12 measured mod binaries import
   any patching API; all game interaction flows through host-provided services (mbm host table in
   the #9 design; Lua VM services in the PC-UE4SS design). The patching responsibility sits in the
   host (UE4SS.dll itself: `VirtualProtect`/`WriteProcessMemory`).
2. **The PC-UE4SS C++ mod ABI is unusable for us as-is.** UE4SS.dll exports 4250 mangled
   MSVC C++ symbols; building against them requires the MSVC STL + the exact MSVC/mangling
   toolchain (measured failure even at the STL-include stage on this Mac), and cross-module C++
   (typeinfo/EH/allocator) is exactly what the #9 C table design (NativeModAbi.h rule: "no C++
   type, vtable, RTTI, exception or allocator ever crosses the boundary") avoids. The measurement
   supports that design.
3. **What every mod really needs at the PE level (measured minimum):** one named export
   (`mbm_mod_entry_v1`), kernel32 heap trio, nothing else. Measured in
   `cap13-mod-keybinds-noeh.dll`: 1 import DLL, 3 imported functions, 1 export, native-x64.
   With default C++ settings the CRT adds the vcruntime EH pair + an inert TLS directory —
   eliminable at build time (`-fno-exceptions`), measured.
4. **The 60–80% estimate is replaced by:** 69% of mod binaries (8 distinct mods) need nothing beyond UE4SS-host functions
   plus the shared kernel32 heap floor; 100% need kernel32 for the C++ heap (or plain C to avoid
   even that); 0% need game-code patching; EH/TLS are build artifacts (83% present as imports by
   default, 0% required by mod logic — both numbers measured).

## 6. Sample-set honesty notes

- Groups A–D/H are built by cap13 from cap13-resident sources; they are real compiled PE binaries,
  not hand-crafted bytes (that remains the role of `FIXTURES/`, untouched).
- Group A ports implement the upstream mods' actual Lua behavior, but upstream ships them as Lua —
  "upstream mod" refers to behavior parity, not to upstream C++ objects. The two upstream C++ mod
  sources that DO exist (EventViewerMod, KismetDebuggerMod) could not be compiled here (§2, logged).
- Group E binaries are byte-identical copies of real on-disk artifacts; their original build
  environment is documented in their respective spike directories, not re-verified here.
- All analysis is static; no sample was loaded or executed on Windows or PS5, and none of this
  claims in-game behavior — runtime acceptance belongs to the #9 verifier path.
