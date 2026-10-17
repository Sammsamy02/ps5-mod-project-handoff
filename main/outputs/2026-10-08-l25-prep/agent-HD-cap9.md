# agent-HD-cap9.md — CAP9 C++ mod recompiler, compact report

Design-only, Mac on battery, no builds/console/network. Full design:
`outputs/2026-10-08-l25-prep/CAP9-CPPMODS-DESIGN.md`. Read-only survey of
the vendored UE4SS trees, typescan-r4 candidate, r5 compile/link receipts,
SDK v0.43 toolchain, PHASEB/PHASEC/GA/GG/FC/GH docs.

## Per-path feasibility verdicts

- **PATH (a) recompile-from-source — UE4SS C++ mods: HIGH (primary).**
  The port already compiles UE4SS's C++-mod loader for the PS5 target
  (`cppmod`/`cppmod-base` units, prospero-clang x86_64-sie-ps5); on
  `__PROSPERO__` it discovers `main.prx`/`main.sprx` per mod, dlopens it
  (RTLD_NOW|RTLD_LOCAL, 64-slot bounded retention, never dlclose), and
  calls exported `start_mod`/`uninstall_mod` exactly like PC
  LoadLibrary/GetProcAddress. Lifecycle matches PC (start at program
  start, on_unreal_init after setup_unreal). Two routes designed:
  (a-static) mod TUs relinked into the composition PRX (zero-risk
  bootstrap) and (a-dyn) per-mod PRX + ONE C-ABI gateway export
  (`ue4ss_mod_host_api_v1`) + mod-side adapter — because today's PRX
  exports only module_start/module_stop (2 sce-NID anchors). Gap to
  receipt on console: dlopen-of-/data-prx resolving imports against the
  INJECTED host is unverified (no native C++ mod has ever loaded on
  console; retained-cppmods 10/10 tests were host shims).
  Source-compat gaps: 2-byte-wchar sized-string rules (WIDE-LIBC),
  PC-path C++ twin of GA's shim, thread/sync edits, register_tab throws
  on Prospero, detour callbacks deferred until #3-L3. License check
  documented (UE4SS = MIT; per-mod source license gate).
  SKSE plugins: MEDIUM-LOW (needs the not-yet-existing SKSE-compatible
  host — GH tie-in). ASI/SHV: engine-surface dependent / closed-source
  core.
- **PATH (b) PE shim loading — C-surface mods plausible, C++-surface
  mods break.** Loader design written (manual PE map + relocs, IAT
  resolution against kernel32-basics shims, DllMain entry, export scan).
  UE4SS C++ mods are 100% C++-surface (sampled 2/2 vendored examples:
  CppUserModBase* return for host virtual dispatch, RC_UE4SS_API
  MSVC-mangled imports, STL fields in the crossing object; 1/2
  dynamic_cast; 2/2 register_tab). Verdict: refuse (already implemented
  as the loader's 'MZ' marker refusal with a clear error message).

## The ABI wall one-liner

Same x86-64 does not mean same C++: MSVC passes `this` in RCX with shadow
space vs Itanium RDI/SysV (every member/virtual call misdispatches);
MSVC bare-array vftables vs Itanium offset-to-top+RTTI headers (slots
coincide only for simple dtor-first single inheritance — and still hold
RCX-convention pointers); MSVC SEH __CxxFrameHandler4 vs .eh_frame
libunwind (a throw across the boundary = death); MSVC 32-byte wstring vs
libc++ 24-byte (object fields misread as data); ?-mangling at IAT
resolution; and MSVC TEB/TLS vs -femulated-tls with no TEB — so any C++
object crossing the boundary is broken in layout, calling convention AND
runtime at once, which is why C-flat exports are the only (b)-eligible
class and every real UE4SS/SKSE/SHV mod needs source (a).

## Key artifacts

- Design: `outputs/2026-10-08-l25-prep/CAP9-CPPMODS-DESIGN.md`
- Decisive local sources: `native-coherent-runtime-1/source/port/ue4ss/
  src/Mod/CppMod.cpp:81-127` + `src/Platform/CppModModule.cpp:60-69` (PE
  refusal) + `include/Platform/NativeCppModPath.hpp`; vendored examples
  `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/cppmods/
  {EventViewerMod,KismetDebuggerMod}`; `UE4SS/include/Mod/
  CppUserModBase.hpp`; r5 COMPILE-PLAN/LINK-COMMAND receipts;
  package_prx_l19.py:1104-1110 (2 NID anchors); PHASEC §5 (#4 satisfied);
  agent-FC-wide-libc-audit; agent-GA-path-shim; agent-GG-modmanager.

## First console test (3-restart law)

Rung 0: our minimal hello-mod PRX (vendored dllmain pattern, no GUI/IO/
detours) via (a-static) then (a-dyn) — answers the dlopen-resolution
question. Rung 1: KismetDebuggerMod recompiled with recorded GUI-guard
adaptation. Rung 2: licensed third-party no-GUI mod via GG
install-from-source. PASS per rung = load markers + start_mod + on_update
counter + clean teardown + menu-up + klog clean, 3/3 across separate
boots; any module_start crash = STOP (irreversible side effects).
