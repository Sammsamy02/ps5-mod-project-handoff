# CAP9 — C++ MOD RECOMPILER (run Windows DLL mods on PS5) — design only

Agent HD (ZCode), 2026-10-08 l25-prep packet. User-added capability #9
(priority after #11): run Windows DLL mods — UE4SS C++ mods, SKSE/F4SE
plugins, ASI and ScriptHookV plugins — on PS5. **DESIGN ONLY** — Mac on
battery: no builds, no console contact, no network; training-knowledge
labeled. Nothing in this document authorizes any console action.

Evidence labels:
- **[LOCAL-SOURCE]** — from files in this worktree (paths + line numbers).
- **[TRAINING-KNOWLEDGE]** — from my training data; version-sensitive facts
  must be re-verified against a pinned primary source before code depends
  on them (staleness register §8).
- **[INFERENCE]** — my reasoning on the above.

Laws inherited (binding) [LOCAL-SOURCE: PHASEB-PLAN.md §0; CAP10-CSHARP-
DESIGN.md "Laws inherited"; ORIGINAL-MOD-IDENTITY.md]:
1. **Mod files byte-identical** — the original PC mod files (source or DLL)
   are never modified; any converted output is a DERIVED artifact generated
   at install and recorded in provenance (GG mod-manager manifest shape).
2. **No Sony SDK material** — only the pinned open
   `ps5-payload-sdk` (sdk-v0.43) + prospero-clang toolchain already used by
   every l26 build.
3. Standing console laws: 3-restart acceptance (DONE = 3/3 across separate
   boots), one user-launched test per boot, no rapid-fire launches, klog to
   NEW files, decrypt-off executable fetches, never upload a downloaded or
   modified executable (fake-sign laws are for GAME executables only; our
   own PRX uploads are plain [LOCAL-SOURCE: PS5-MODDING-MISSION.md "THE
   DELIVERY STACK"]).

---

## 0. THE ONE FACT THAT DEFINES THE PROBLEM

**The port already ships UE4SS's C++-mod infrastructure, compiled for the
PS5 target by the pinned toolchain — and its loader already refuses Windows
PE by explicit design.** Evidence:

- The l26/r5 builds compile the C++ mod loader units with prospero-clang++
  for `x86_64-sie-ps5`: `cppmod` = `native-coherent-runtime-1/source/port/
  ue4ss/src/Mod/CppMod.cpp` and `cppmod-base` = `.../Mod/CppUserModBase.cpp`
  [LOCAL-SOURCE: outputs/2026-10-08-r5-source-integration/COMPILE-PLAN.json
  units "cppmod"/"cppmod-base"; same flags as every runtime TU:
  `-std=c++23 -fms-extensions -fPIC -DRC_UE4SS_EXPORTS
  -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero -DUE_BUILD_SHIPPING=1`].
- On `__PROSPERO__` the loader looks for a NATIVE module:
  `Platform::FindNativeCppModModule(m_dlls_path, ...)` resolves
  `main.prx`, `main.sprx`, `<name>.prx`, `<name>.sprx` (legacy
  `main.dll`/`<name>.dll` names retained last), then
  `Platform::CppModModule::Open` `dlopen`s it (`RTLD_NOW | RTLD_LOCAL`),
  and `FindExport("start_mod")` / `FindExport("uninstall_mod")` are
  resolved and invoked exactly like PC's `LoadLibraryExW`/`GetProcAddress`
  pair [LOCAL-SOURCE: port/ue4ss/src/Mod/CppMod.cpp:81-127;
  include/Platform/NativeCppModPath.hpp:21-23].
- PE is refused at a 2-byte 'MZ' marker check with the message
  *"Windows PE mod requires an unimplemented binary/ABI translation;
  native loader cannot run it"* [LOCAL-SOURCE: port/ue4ss/src/Platform/
  CppModModule.cpp:60-69]. The comment: "A binary adapter must supply a
  native artifact here; the native module owner explicitly refuses Windows
  PE" [CppMod.cpp:85-86].
- The lifecycle on the port matches PC: `start_native_mods()` →
  `setup_mods(); install_cpp_mods(); start_cpp_mods()` (one-shot, gated by
  `CheckNativeModStartup`) at program start, `fire_unreal_init_for_cpp_mods()`
  after Unreal setup, update-loop `fire_update()`, and bounded ownership
  (64-slot code retention, never `dlclose`) [LOCAL-SOURCE: typescan-r4/
  candidate/src/UE4SSProgram.cpp:606-646, :705; CppModModule.cpp:15-49;
  native-retained-cppmods-1/README.md "host-final-1" 10/10 focused tests].

So CAP9 is NOT "build a C++ mod system" — it exists. CAP9 is exactly two
questions: **(a)** how the *published source* of a PC mod becomes that
native `.prx` artifact at install, and **(b)** whether the *original PE
bytes* could ever be loaded instead — and where the MSVC-vs-PS5 C++ ABI
wall makes that impossible. The port's designers already answered (b) with
a refuse; this document maps the wall precisely and makes the refusal
principled (and machine-checkable at install).

What a UE4SS C++ mod IS, concretely (sampled from the vendored examples —
this is the ABI surface both paths must satisfy):
- Exports: `extern "C" __declspec(dllexport) RC::CppUserModBase*
  start_mod()` and `void uninstall_mod(RC::CppUserModBase*)` — flat C
  names, but the RETURN VALUE is a C++ object [LOCAL-SOURCE: vendored
  cppmods/EventViewerMod/src/dllmain.cpp:8-18; KismetDebuggerMod/src/
  dllmain.cpp:31-43 — identical pattern in both].
- The mod derives from `CppUserModBase` (~14 virtuals: `on_update`,
  `on_unreal_init`, `on_ui_init`, `on_program_start`, `on_lua_start/stop`
  overloads, `on_dll_load`, `render_tab`, `on_cpp_mods_loaded`, virtual
  dtor) and the HOST dispatches virtually through the returned pointer
  [LOCAL-SOURCE: vendored UE4SS/include/Mod/CppUserModBase.hpp:31-218;
  port CppMod.cpp fire_* :177-302].
- The mod calls back INTO UE4SS: every `RC_UE4SS_API` member
  (`CppUserModBase()` ctor/dtor, `register_tab`,
  `register_keydown_event`) plus the whole `Unreal::` namespace
  (`FindFirstOf`, `Hook::RegisterEngineTickPreCallback`, ...) — on PC
  these are `__declspec(dllimport)` symbols resolved from UE4SS.dll
  [LOCAL-SOURCE: Common.hpp:4-12; EventViewerMod/src/EventViewer.cpp:38;
  EventViewerMod/CMakeLists.txt:28 `target_link_libraries(... UE4SS)`].
- Object STATE crosses as data: five `StringType` (= `std::wstring`)
  metadata fields + `std::vector<std::shared_ptr<GUI::GUITab>> GUITabs`
  live in the object the host inspects [LOCAL-SOURCE: CppUserModBase.hpp:
  33-41; deps/first/String/include/String/StringType.hpp:23-27].

---

## 1. PATH (a) — RECOMPILE FROM PUBLISHED SOURCE

### 1a. The pipeline (mod source → native PRX → loaded mod)

```
published source (repo URL + pinned commit)         [Mac-side only]
  → license check (§1d)
  → build recipe: per-TU prospero-clang++ compile    (the r5 COMPILE-PLAN
    argv as template: same includes/defines the runtime itself uses)
  → optional source-adaptation patch layer           (§1c gaps; recorded)
  → link: -shared object → package as PRX            (package_prx_l19.py
    machinery; exports start_mod / uninstall_mod)
  → derived artifact <name>.prx + sha256 + recipe hash  (never modifies
    the original source; GG manifest records provenance)
  → install: ue4ss/Mods/<name>/dlls/main.prx         (the exact filename
    precedence FindNativeCppModModule already expects)
  → MFA loadlist admission row (size-exact) + GG deploy gates
  → runtime: install_cpp_mods() → CppMod ctor → CppModModule::Open →
    start_mod() at program start; on_unreal_init after setup_unreal
```

Two routes for the mod→host symbol direction (the ONE structural gap in
path (a)):

**(a-dyn) per-mod PRX + the existing dlopen loader** (matches the in-tree
design). The mod's TU set imports the `RC_UE4SS_API` surface — but today's
composition PRX exports ONLY `module_start`/`module_stop` under their
sce-NIDs (`BaOKcng8g88#F#A` / `KpDMrPHvt3Q#F#A`) — two dynsym anchors
total [LOCAL-SOURCE: outputs/2026-10-08-l26-package-B/package_prx_l19.py
:1104-1110; CLAUDE-CAPABILITY-STATUS row #5 "PRX carries only 2 dynsym
anchors"]. Nothing a `dlopen`'d mod could resolve against. Design fix —
the classic C-ABI gateway:

- Host adds ONE dynsym export: `ue4ss_mod_host_api_v1()` returning a
  `const UE4SS_ModHostV1*` — a versioned struct of C function pointers
  covering exactly the RC_UE4SS_API surface mods touch (base ctor/dtor
  completion hooks, register_keydown_event, output/send, and the bounded
  `Unreal::` entry points the port already exposes internally). Bounded,
  name-filtered, one dynsym delta regardless of mod count.
- A small **mod-side adapter library** (OUR source, MIT, compiled at
  install into every mod PRX) implements `CppUserModBase`'s out-of-line
  members on top of the gateway and satisfies the mod's imports. Because
  BOTH sides compile with the same prospero-clang against the same
  headers, the Itanium vtable/layout agreement is automatic — the adapter
  runs the base ctor mod-side (field layout identical), and the host's
  virtual dispatch through the returned vtable is same-ABI.
- OPEN (must be receipted on console, §5 rung 0): whether the PS5 dynamic
  loader resolves a dlopen'd module's imports against an INJECTED module
  (our composition PRX is ploader-injected, not dynlib-registered)
  [INFERENCE; the SDK proves payload-context dlopen of a system sprx —
  samples/hello_dlfcn/main.c `dlopen("libSceRandom.sprx", RTLD_LAZY)` +
  dlsym by plain name [LOCAL-SOURCE] — but game-process + injected-host
  resolution is the untested composition].

**(a-static) per-mod composition relink** — mod TUs are appended to the
launch11 one-pass unit list; the mod is linked INTO the runtime PRX
(no dlopen, no gateway, no export growth). Zero new console capability;
every symbol resolves at static link. Cost: full runtime rebuild +
30.9MB-class upload per mod change, and mod install couples to runtime
builds (minutes). Correct for a curated mod set; the fallback if (a-dyn)'s
resolution question fails on console.

Recommendation: (a-dyn) is the target shape (the in-tree loader was built
for it); (a-static) is the guaranteed-correct bootstrap that can ship
first and de-risk the first console rung while (a-dyn) is verified.

### 1b. What API a recompiled mod gets today (verified)

Compiled against our platform layer (the r5/typescan-r4 header set —
"the same headers a recompiled mod would include"), a mod gets:

| PC API | On our target | Evidence |
|---|---|---|
| `start_mod`/`uninstall_mod` lifecycle | WORKS (loader in-tree) | CppMod.cpp:100-104 |
| `on_update`/`on_unreal_init`/`on_program_start`/`on_cpp_mods_loaded` virtuals | WORKS (host fires them; on_unreal_init after setup_unreal) | UE4SSProgram.cpp:705; CppMod.cpp:256-302 |
| `register_keydown_event` | WORKS (`HAS_INPUT`; PS5Pad L3+R3 route console-proven) | CppUserModBase.cpp:68-79; capability row #6 |
| `on_lua_start/stop` | WORKS (LuaMadeSimple same-type linked) | CppMod.cpp:177-254 |
| `register_tab` / `UE4SS_ENABLE_IMGUI` | **THROWS on Prospero** ("Native debugging GUI service is unavailable") | CppUserModBase.cpp:58-65 |
| `Unreal::FindFirstOf`/UObject API | WORKS on pinned profiles (rides the console-proven seed/pin surface) | capability rows #2; Lua-side proofs |
| UFunction hooks (`RegisterHook`-class) | WORKS (pointer-swap method, console-proven R3 carriers) | capability row #3, GV inventory items 1-2 |
| Engine-tick detours (`RegisterEngineTickPreCallback` etc.) | COMPILED OUT/DEFERRED on console under R3 marshal; needs Phase B L3 instruction rung | capability row #3 "instruction-level blocked" |
| `on_dll_load` | fires for real module loads (`fire_dll_load_for_cpp_mods`) | UE4SSProgram.cpp:185-209 |

This table IS the per-family feasibility truth: a mod whose logic sits on
virtuals + UFunction hooks + keydown events recompiles to a fully-working
PS5 mod TODAY; a mod built on GUI tabs or engine detours loads but loses
those arms until #3's L3 rung and the (separately designed) UI route.

### 1c. Source-compat gaps (what "recompile" actually has to fix)

The WIDE-LIBC lessons apply verbatim [LOCAL-SOURCE: agent-FC-wide-libc-audit.md]:

1. **wchar stride.** Our target's compiler `wchar_t` is 2-byte (SDK
   libc++.a's wstring members are 2-byte — `cmpl $0x14` short-cap,
   `__min_cap 10`) while the CONSOLE libSceLibcInternal wide imports are
   4-byte stride (30 wide/locale imports; `wcslen` yields len/2 → garble).
   A recompiled mod must use the port's sized-string helpers
   (`Helpers/String.hpp` DecodeUtf8/(ptr,count) forms) and never
   bare-pointer `std::wstring{ptr}` / traits::length construction. Violation
   class = garbled strings/diagnostics, not crashes — enforce with a
   compile-warning gate in the recipe (deny bare-pointer wstring
   construction patterns).
2. **File paths (the GA seam, C++ side).** PC mods hardcode `C:\...`,
   backslashes, `Binaries/Win64/...` relative roots. GA's LuaPathShim
   rules (drive-letter drop, `\`→`/`, Binaries/Win64 + `../` virtual-cwd
   collapse, `ue4ss/`-rooted keys, write confinement inside the calling
   mod's folder) are the spec; a C++-level twin belongs in the port's
   File/IO layer (or the recipe patches the mod's path constants) — the
   mod root is `/data/modbridge/roots/<title-id>/` [LOCAL-SOURCE:
   agent-GA-path-shim.md resolver rules; MODBRIDGE_MOD_ROOT]. Rung-1 mods
   should do no file IO to keep this out of the first test.
3. **Threads/sync.** `CreateThread`/`_beginthreadex`/Win events →
   `std::thread`/pthread (the port's input/update threads already do this);
   critical sections → `std::mutex`. Pure source edits, recipe-recorded.
4. **CRT-mismatch warning is void.** PC UE4SS warns mods must match the
   host's CRT/config [LOCAL-SOURCE: CppUserModBase.hpp:29-30 comment] — on
   path (a) WE compile both sides with one pinned toolchain; that entire
   failure class disappears.
5. **GUI guard.** `register_tab` throws on Prospero — recipes for GUI mods
   must `#ifdef`-gate those calls (recorded adaptation) or the mod ctor
   throws at `start_mod` and the mod is refused (loader catches, marks
   not-installable, logs) [LOCAL-SOURCE: CppMod.cpp:107-112].

### 1d. Licensing check (documented, per mod)

- UE4SS itself: **MIT** (Narknon) [LOCAL-SOURCE: vendored ue4ss/LICENSE
  head — "MIT License, Copyright (c) 2022 Narknon"]. Our headers/adapter
  may be composed freely; derived mod PRXs carry the mod's own license.
- The mod's SOURCE license must permit: compile + distribute a derived
  binary for private console use. Accept: MIT/Apache-2/BSD/Zlib (clean);
  GPL-2/3 (derived PRX must offer corresponding source = the recipe +
   patch layer; record the offer); MPL (file-level, fine). REFUSE:
  closed-source distributions, "no modification/no derivative works"
  CC-NC/ND-style terms, and Nexus "permissions" rows that forbid
  redistribution outside Nexus [TRAINING-KNOWLEDGE on license texts;
  the check is procedural and recorded per mod in the GG manifest].
- SKSE plugin sources are commonly MIT/Apache (CommonLibSSE ecosystem)
  [TRAINING-KNOWLEDGE, version-sensitive] — but see §1f: the host layer
  they compile against does not exist yet.

### 1e. Automation level — mod-manager "install from source"

GG's mod-manager gains one install mode [LOCAL-SOURCE:
agent-GG-modmanager.md for the existing gates it plugs into]:

```
modmanager.py install-from-source <manifest>   (Mac-side; console NEVER compiles)
  manifest = { repo, pinned commit, mod name, license verdict + text hash,
               recipe id, adaptation patch (optional), expected exports }
  steps: fetch (network on the Mac, at author time) → license gate →
         compile (r5-argv template, bounded TU/error limits) → link+package
         PRX (start_mod/uninstall_mod exported) → record provenance
         (source sha, recipe sha, PRX sha) → reuse EXISTING install/deploy
         pipeline: loadlist row appended LAST, size-exact MFA admission,
         preflight/keymap/conflict gates, 3-pass acceptance plan
```

Originals untouched; revert = row + artifact removal. Same never-edit law
as #7/#8 overlays. A failed compile is a normal installer error (no
console interaction), so iteration is cheap.

### 1f. Feasibility verdict per mod family (path a)

| Family | Verdict | Why |
|---|---|---|
| **UE4SS C++ mods** | **HIGH — the port's own infra** | Same headers, same toolchain, loader in-tree + host-tested 10/10; gaps are §1b's feature table + §1c's mechanical source edits; console rungs §5. |
| **SKSE/F4SE plugins** | **MEDIUM-LOW — blocked on the missing host layer** | Recompiling a CommonLibSSE plugin needs a SKSE-compatible host (plugin API, Papyrus VM binding, per-build offset profiles). GH's verdict stands: "PC SKSE plugins are native Windows PE DLLs — they do not run, period; everything is a reimplementation" [LOCAL-SOURCE: OTHER-ENGINES-RESEARCH.md §3c]. Path (a) applies to their published SOURCE only after that layer exists (GH tie-in); the header-inspection tooling already in-tree is the data-layer start [§3a LOCAL-SOURCE cites]. |
| **ASI plugins** | **LOW — engine-surface dependent** | .asi is a load format, not an API; each plugin binds a game/engine surface (or ScriptHookV). With source + an existing host surface → same pipeline; otherwise refuse. |
| **ScriptHookV plugins** | **LOW — closed-source core** | SHV itself is closed source (clean-room rebuild required); its C++ plugin SDK presumes the SHV runtime [LOCAL-SOURCE: OTHER-ENGINES-RESEARCH §5a/§5c]. Path (a) reaches only open-source plugins after an SHV-class host exists. |

---

## 2. PATH (b) — LOAD THE ORIGINAL PE DLL THROUGH A WINDOWS SHIM LAYER

Same x86-64 CPU — but "same ISA" is the ONLY thing that transfers. Design
first, wall second (the wall decides).

### 2a. The loader (mechanical design — all [INFERENCE]/[TRAINING-KNOWLEDGE] unless noted)

1. **Map**: parse PE headers; allocate one anonymous RWX region (or RW for
   relocation then mprotect RX+RW as needed); copy sections filesz→memsz,
   zero-fill to virtualsize (PE 4K section alignment vs PS5 16K pages —
   manual copy, no file mapping); apply `BASE_RELOCATION` records for the
   load slide.
2. **Imports**: walk the import (and delay-load) directories; resolve each
   `dllname!symbol` against the shim export tables; write the IAT.
   `LoadLibraryA/W` inside the mod recursively re-enters the loader (or
   refuses); `GetProcAddress` returns shim/host aliases.
3. **TLS**: if the PE has a TLS directory (statically-linked MSVC CRT with
   thread-local state — common), a real TEB with `__tls_array` at the
   segment base the code expects is required — see wall item 7: REFUSE-class.
4. **Entry**: call `DllMain(DLL_PROCESS_ATTACH)` (MSVC CRT init:
   static-duration object ctors + atexit registration live here; skipping
   it breaks any CRT-bearing mod).
5. **Exports**: parse the export table; `start_mod`/`uninstall_mod` are
   undecorated C names on x64 — mechanically findable.
6. **Runtime services**: SEH unwind data (.pdata/.xdata) must at least be
   parsed enough to NOT misinterpret mod frames during host-side stack
   walks (our #5 symbolizer parses PS5 unwind tables, not SEH).

### 2b. The Win-API shim surface (what imports matter per family)

- **kernel32 (every family)**: memory (`VirtualAlloc/Protect/Free`,
  `HeapAlloc/HeapFree/GetProcessHeap` → mmap/malloc), threads
  (`CreateThread` → pthread wrapper, `WaitForSingleObject`,
  `Sleep`, `TlsAlloc/TlsGetValue/TlsFree`), files (`CreateFileW/ReadFile/
  WriteFile/SetFilePointer/CloseHandle` → GA path-shim rules + fopen),
  module queries (`GetModuleHandleW("UE4SS.dll")` → fake handle;
  `GetProcAddress` → alias table), misc (`GetLastError/SetLastError`,
  `QueryPerformanceCounter`, `GetTickCount`, `InitializeCriticalSection`
  family → pthread_mutex, `Interlocked*` → atomics). Bounded and
  buildable — this subset is genuinely small.
- **user32/gdi32**: message hooks, windowing — mods almost never need them
  headlessly; REFUSE-list.
- **The framework's own exported API** — the load-bearing import set:
  - UE4SS C++ mods import **MSVC-mangled C++ symbols from UE4SS.dll**
    (every `RC_UE4SS_API`: `??0CppUserModBase@RC@@QEAA@XZ` ctor,
    `register_tab`, the `Unreal::` surface) [LOCAL-SOURCE: Common.hpp
    dllimport; EventViewerMod CMake links UE4SS]. A shim UE4SS.dll must
    satisfy them with ABI-COMPATIBLE bodies — impossible, per §2c items
    1/3/5.
  - SKSE plugins import SKSE's C++ virtual interfaces (MessagingInterface
    etc.) and CommonLibSSE's REL/offset services [TRAINING-KNOWLEDGE];
    SHV .asi import ScriptHookV.dll's C++ surface — same class of wall,
    plus the host layers don't exist (§1f).

### 2c. THE ABI WALL, mapped precisely (the task's core question)

The premise to correct: "x86-64 MSVC-vs-Itanium vtable layout is
compatible" — **partially true and completely irrelevant**: even where
slot order coincides, five other deltas kill every crossing. The real
list:

1. **Calling convention (the universal killer).** MSVC x64 passes `this`
   in RCX (RCX/RDX/R8/R9 + 32-byte shadow space); the PS5 target uses the
   SysV/Itanium convention (`this` in RDI; RDI/RSI/RDX/RCX/R8/R9; no
   shadow space; different aggregate/pass-by-value and return rules)
   [TRAINING-KNOWLEDGE — stable ABI facts; our side pinned by the
   prospero-clang `x86_64-sie-ps5` target, LOCAL-SOURCE wrapper script].
   Every C++ member/virtual call across the boundary misdelivers
   arguments: an RCS-convention callee invoked by an RDI-convention
   caller reads `this` from the wrong register → garbage or crash.
   Layout tricks cannot fix register assignment; the only fix is a
   generated register-repacking thunk per crossing symbol (a full C++ ABI
   bridge — even Wine does not attempt cross-ABI C++ loading; Wine runs
   PE-on-PE with a Windows ABI toolchain) [INFERENCE].
2. **Vtable layout deltas (the honest detail).** MSVC vftable = bare
   function array, no header; Itanium = [offset-to-top, typeinfo] header
   with the vptr pointing past it; MSVC emits the complete+deleting dtor
   pair as two slots at the dtor's declaration position — Itanium also
   two slots (D1/D0) at declaration position. So for a SINGLE-inheritance
   class whose dtor is declared first (CppUserModBase IS such a class,
   LOCAL-SOURCE header order), the positive slot indices from the vptr
   happen to coincide [INFERENCE — verify per class before relying on
   it]. Divergence arrives with: multiple inheritance secondary vtables,
   virtual bases (MSVC vtordisp thunks), covariant-return thunks, and any
   class declaring virtuals before its dtor. And per item 1, even a
   coincident slot holds an RCX-convention pointer.
3. **RTTI.** MSVC (TypeDescriptor / CompleteObjectLocator / hierarchy
   descriptors) vs Itanium (typeinfo / __class_type_info chain). Any
   `dynamic_cast`/`typeid` executed by one side on the other's object
   parses foreign metadata → garbage/crash. Sampled: KismetDebuggerMod's
   dllmain uses `dynamic_cast` on the mod object [LOCAL-SOURCE:
   KismetDebuggerMod/src/dllmain.cpp:22-24]; EventViewerMod carefully
   avoids it (comment!) but still crosses via items 1/5.
4. **Exceptions/unwind.** MSVC x64 C++ EH is SEH-based
   (`__CxxFrameHandler4`, funclets, scope tables, RtlUnwind); our target
   is Itanium zero-cost `.eh_frame` + libunwrap (`-fexceptions -frtti`,
   links `-lunwind`) [LOCAL-SOURCE: prospero-clang++ wrapper argv]. A
   throw ACROSS the boundary = unwind walks frames lacking the expected
   metadata = terminate/crash — confirmed as a design fact, not a tuning
   issue. Mod-INTERNAL try/catch additionally needs the MSVC EH runtime
   + a working SEH dispatcher on PS5 (none exists).
5. **STL/data layout.** `StringType` = `std::wstring`: MSVC layout 32 B
   (SSO 8 wchars) vs our libc++ 2-byte layout 24 B (SSO 10 wchars)
   [TRAINING-KNOWLEDGE on MSVC; LOCAL-SOURCE on our side via FC's
   `__min_cap 10` / 2-byte arithmetic]. `CppUserModBase`'s five StringType
   fields + `std::vector<std::shared_ptr<GUITab>>` cross as DATA (host
   reads ModName; dtor walks GUITabs) → offset mismatch → garbage reads
   before any call is even made. `std::function` (MSVC always-heap vs
   libc++ SBO) and every other STL type in a crossing signature likewise.
6. **Name mangling.** MSVC `?`-decoration vs Itanium `_ZN...`: a
   link-time-only issue for static resolution, but DLL imports resolve by
   exact exported string at load time — the shim must alias-mangle; and
   the aliased bodies still hit items 1/3/4/5.
7. **TLS.** MSVC thread-locals + `__tls_array` expect a TEB at the FS/GS
   base with the Windows TLS-slot layout; our target builds with
   `-femulated-tls` and has no TEB [LOCAL-SOURCE: wrapper argv].
   Fabricating a TEB at the expected segment base needs userspace
   descriptor control we do not have → REFUSE-class for TLS-bearing PEs.

### 2d. Where (b) survives vs breaks (quantified)

- **Survives (mechanically plausible)**: strictly C-ABI mods — flat
  `extern "C"` exports, POD/flat structs by value/pointer, static CRT
  (/MT), no exceptions/RTTI/STL/TLS crossing, no framework C++ imports.
  Then §2a's loader + §2b's kernel32 shim is real engineering, not
  fantasy.
- **Breaks (structurally)**: ANY C++ object crossing — which for UE4SS
  C++ mods is 100% of the surface, by construction (the mod API IS a
  C++ object). Sampled quantification from the vendored examples
  [LOCAL-SOURCE]: **2/2 mods** return a `CppUserModBase*` for host
  virtual dispatch (wall 1+2), import `RC_UE4SS_API` C++ symbols (walls
  1+5+6), embed STL fields in the crossing object (wall 5); **1/2** uses
  `dynamic_cast` (wall 3); **2/2** call `register_tab` (unavailable on
  PS5 even on path (a) without adaptation). Typical SKSE/SHV plugins are
  likewise C++-virtual-interface consumers [TRAINING-KNOWLEDGE].
  For UE4SS C++ mods the verdict is not "hard" — it is **category-error
  hard**: the boundary object's layout, calling convention, and runtime
  (RTTI/EH/TLS) are all three foreign at once.
- Note the port already encodes this refusal: the MZ check at
  CppModModule.cpp:67-68 with the message naming "unimplemented
  binary/ABI translation" [LOCAL-SOURCE]. That message is the correct
  UX: refuse loudly, name the reason, point to path (a).

---

## 3. COMPARISON + RECOMMENDATION + INSTALL-TIME DETECTION

### 3a. Recommendation

- **PATH (a) IS PRIMARY** for UE4SS C++ mods (HIGH feasibility; infra
  exists; first console rungs §5). Bootstrap with (a-static) relink;
  promote to (a-dyn) + the C gateway after rung-0 verifies dlopen
  resolution against the injected host.
- **PATH (b) = REFUSE-WITH-CLEAR-ERROR for every C++-surface mod** (all
  UE4SS C++ mods, SKSE/F4SE plugins, SHV plugins as a class). The refusal
  is principled (§2c), already implemented in the loader, and must stay.
- (b)'s C-flat residue (hypothetical headless C-export plugins) is gated
  on engine-surface host layers that do not exist (GH tie-in) — design
  retained in §2 for the day one exists; build nothing now.

### 3b. Install-time detection (PE ABI-risk classifier — a Mac-side analyzer, GG-integrated)

Read the PE's export + import directories + TLS directory (pure parse,
no console):

```
CLASSIFY(pe):
  cpp_imports   = any import symbol matching /^(\?|_ZN)/ against
                  framework DLLs (UE4SS.dll, SKSE*.dll, ScriptHookV.dll)
                  OR any msvcrt/msvcp140/msvcp dynamic import
  cpp_exports   = any export matching /^\?/
  tls_dir       = present
  seh_cxx       = any .pdata entry whose companion indicates C++ EH
                  (heuristic; optional discriminator)

  if cpp_imports or cpp_exports or tls_dir:
        -> "(a)-required: C++ ABI surface (mangled=<n>, tls=<bool>).
             Source with recompile rights needed; original DLL cannot
             run on PS5 (MSVC C++ ABI wall: calling convention, RTTI,
             exceptions, STL layout)."
        if no source available -> refuse install, record reason.
  else:
        -> "(b)-eligible (C-ABI only) — mechanical loader class; gated
             on engine-surface host (capability: none today)."
```

Same shape as GM's analyzer-emits-directives law: the classifier only
LABELS; the builder/installer acts. The original DLL bytes are never
modified either way; the derived PRX from (a) is a provenance-recorded
install artifact.

---

## 4. DEPENDENCIES ON CAPABILITIES 1–8 (explicit)

| Cap | Dependency | Evidence |
|---|---|---|
| **#3 code patching** | HARD for detour-class mod callbacks: engine-tick/ProcessEvent detour arms are compiled out/deferred under R3 marshal on console; instruction rung needs Phase B L3 (`SYS_mdbg_call 573` write channel). Mods on UFunction pointer-swap hooks + listeners work today. | CLAUDE-CAPABILITY-STATUS #3 + GV hook-kind inventory; PHASEB-PLAN L3 row |
| **#4 load timing** | ALREADY SATISFIED for C++ mods: they load at program start via `start_native_mods()` (one-shot) with `on_unreal_init` after setup_unreal — same timing class as PC; ploader injection receipts stand. | PHASEC-EARLY-LOADING-DESIGN §5 "#4 SATISFIED-ALREADY"; UE4SSProgram.cpp:434-443, :606-646, :705 |
| **#2 scanner/profiles** | Mod calls into `Unreal::` ride the per-game seed/pin profiles (FindFirstOf, FName pool, GUObjectArray); a mod on an unpinned UE4 game runs unpinned — GF #2 anchor-pack blocker applies per title. | capability row #2 |
| **GA path shim** | C++-level twin of the Lua resolver rules needed for any file-IO mod (§1c.2); rung-1 mods avoid file IO. | agent-GA-path-shim.md rules 1-8 |
| **GG mod manager** | install-from-source mode, provenance/manifest, deploy gates, loadlist-LAST, MFA size-exact admission for the PRX artifact, 3-pass acceptance. | agent-GG-modmanager.md; retained-cppmods README (native_plugin entries + file-size checks) |
| **#5 debugger** | Per-mod final-link symbol sidecars for crash symbolization of mod PRXs (same GF #5 "needs-X" gap). | capability row #5 |
| **#7/#8** | Not required for logic-only mods; asset-touching mods ride the overlay/material-fallback paths unchanged. | capability rows #7/#8 |
| **#1** | NOT needed — C++ mods are in-process by design (PC parity: same). | OTHER-ENGINES §capability tables |

---

## 5. FIRST CONSOLE TEST + PASS/FAIL (3-restart law)

Honest constraint: **no native C++ mod PRX has ever loaded on console** —
the retained-cppmods receipts are host tests with controlled OS shims
[LOCAL-SOURCE: native-retained-cppmods-1/README.md "Native OS calls are
controlled stand-ins"]. The rungs:

- **Rung 0 (load-bearing new fact): hello-mod PRX.** A minimal mod WE
  author, following the exact vendored dllmain pattern (start_mod/
  uninstall_mod + ctor setting ModName/ModVersion + on_unreal_init/
  on_update logging; NO GUI, NO file IO, NO detours). Route (a-static)
  first (zero dynamic-resolution risk), then the identical PRX via
  (a-dyn)'s dlopen to answer the resolution question. Installed as
  `ue4ss/Mods/<name>/dlls/main.prx` + MFA row, deployed by GG.
- **Rung 1: simplest REAL example recompiled.** Of the two vendored
  examples, KismetDebuggerMod is the smaller (2 TUs) — both need the
  recorded GUI-guard adaptation (register_tab throws on Prospero), and
  Kismet's `dynamic_cast` is harmless on path (a) (same-ABI). Recipe =
  pinned source + adaptation patch + r5-argv compile → PRX.
- **Rung 2: a real third-party no-GUI community mod** (selection passes
  the §1d license check) — first proof of install-from-source.

PASS (each rung, per the mission law): fresh boot → residency verified →
user launches ONCE → UE4SS.log shows the native-mod load markers
(CppMod ctor success path / `fire_dll_load_for_cpp_mods`), `start_mod`
returned, `on_update` counter advancing (or on_unreal_init marker for
rung 1+), clean uninstall at teardown, no "Stopping mod" Terminal
refusal, game reaches menu (menu-up precedent), klog clean. **DONE = 3/3
identical passes across 3 separate boots.** FAIL = any module_start-class
crash (module side effects are irreversible — retained-cppmods README),
or dlopen failure for (a-dyn) (expected classes: resolution-against-
injected-host unsupported → document, stay on (a-static)) → STOP, report,
no same-boot retry (no rapid-fire launches).

---

## 6. UNLOCKS (what CAP9 buys, honestly scoped)

- **Path (a), UE family**: the entire UE4SS C++ mod ecosystem WITH
  available+licensed source, on any pinned UE title — mods stronger than
  Lua (native speed, full C++ surface, direct Unreal namespace use); our
  own runtime components (e.g. migrating the Item Spawner logic from Lua
  to C++) ride the same pipeline; the GUI-less subset is unlocked at
  rung-0/1 already, detour-armed mods unlock with #3-L3.
- **Path (a), other engines**: SKSE/F4SE/ASI/SHV sources become
  compilable targets the moment GH's per-engine host layers exist — CAP9
  supplies the generic recompile+package+install machinery once there is
  something to compile against.
- **Path (b)**: unlocks nothing near-term; its value is the REFUSAL being
  precise, machine-checkable (§3b), and final for C++-surface mods —
  which protects users from silent-corruption class failures.

## 7. HONEST GAPS (all must close before any rung)

1. dlopen-in-game-process resolving imports against the ploader-injected
   composition PRX: UNKNOWN — rung 0 exists to answer it (hello_dlfcn
   proves payload-context dlopen of system sprx only [LOCAL-SOURCE]).
2. PRX export form for `FindExport("start_mod")`: the packaging path
   exports entry points under sce-NIDs; whether dlsym-by-plain-name finds
   `start_mod` in a dlopen'd homebrew PRX must be receipted (the port's
   FindExport uses dlsym with the plain name) [LOCAL-SOURCE:
   CppModModule.cpp:84-90; package_prx_l19.py:1104-1110].
3. The C-gateway + mod-side adapter (a-dyn) is design-only: exact
   bounded export set, version handshake, and per-surface coverage need
   a spec pass against the RC_UE4SS_API surface mods actually use.
4. C++-level path-shim twin (§1c.2) unwritten.
5. (a-static) needs the launch11 one-pass build to accept external mod
   unit lists cleanly (EDITIONS.json pinning discipline for mod TUs).
6. No third-party mod has passed the license check yet (§1d is process,
   not evidence).

## 8. STALNESS REGISTER ([TRAINING-KNOWLEDGE] items, most-stale first)

1. MSVC vs Itanium ABI micro-details (vtable header/slot coincidences,
   MSVC std::string/wstring sizes, std::function SBO): stable for
   decades, low risk — but pin the exact MSVC layout facts against a
   real dump before writing the §3b classifier's SEH/layout heuristics.
2. Wine's non-attempt at cross-ABI C++ loading: architectural position,
   low risk; re-verify current Wine docs if ever cited as precedent.
3. SKSE/CommonLibSSE + SHV plugin-API shapes and their typical licenses:
   version-sensitive; GH's re-verify-before-depend rule applies.
4. NexusMods per-mod permission rows as license source: policy, not law;
   per-mod verification required.
