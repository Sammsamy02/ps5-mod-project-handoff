# cap13/native-loader — staged sources for the native Windows DLL route

Agent JH, 2026-10-13. Companion design: `../DESIGN/native-dll-loader.md`
(read that first — verdict box, ABI wall, feasibility, milestones).
**Everything here is proposal-only, Mac-only, unbuilt.** No console
authorization, no main-tree edits, no builds of the PS5 runtime.

## Staging map

| path | what | grade |
|---|---|---|
| `include/pe_loader.h` | mapper public API (refusal-first contract) | design-grade, syntax-checked |
| `src/pe_types.h` | minimal PE32+ structures + static_asserts on offsets | design-grade, syntax-checked |
| `src/pe_loader.cpp` | section mapping, 16 KiB perm partition, relocations, IAT resolution, TLS/CLR refusal, export lookup | design-grade, syntax-checked; behavior needs M0 tests |
| `src/win_api_shim.h/.cpp` | kernel32/ucrt subset: T1 memory/sync/time/TLS-API/thread, T2 path rules, T3 module registry, T4 fail-closed EH stubs | design-grade; full row set is M1 census-generated |
| `src/win64_bridge.h` | bridge API + trampoline stamper + arity selection | design-grade |
| `src/win64_bridge.S` | host→guest call0..6, guest→host thunks s0..s4 (RSI/RDI/XMM6-15 preserved) | **hand-written asm, UNVERIFIED** until M0 |
| `src/dllmain_invoker.h/.cpp` | `_DllMainCRTStartup` invoke + detach policy | design-grade |
| `src/ue4ss_compat.cpp` | UE4SS surface: service demarshal shape, MSVC-layout boundary objects, allocator-pairing rule | **sketch** — largest open item (M1) |
| `src/pe_adapter.cpp` | `mbm_mod_entry_v1` wrapper (the only host-visible surface) | design-grade; slot dispatch placeholder |
| `tools/pin_msvc_layout.sh` | Mac-side MSVC layout/mangle/vftable pinning via `-target x86_64-pc-windows-msvc` | **mechanism verified 2026-10-13** (see below) |
| `patches/cppmod_pe_route.patch` | optional host routing alternative (manifest naming needs no host edit) | proposal; integrator decides |

## What is actually verified today (honest)

- The **pinning mechanism** works on this Mac: `clang++ -target
  x86_64-pc-windows-msvc -fsyntax-only -Xclang -fdump-record-layouts`
  dumps MSVC record layouts, and `-S` emits authoritative MSVC-mangled
  symbols (`?call@@YAXPEAUCppUserModBase@RC@@@Z`). No Windows required.
- The staged C++ sources pass `clang++ -fsyntax-only -std=c++17` on macOS
  clang 17 (portable subset; PS5 paths behind `__PROSPERO__` guards are
  design-only until integration).
- **Nothing else.** The asm bridge is untested; the mapper is untested
  against a real PE; no PS5 build exists; J1/J2/J3 console unknowns stand.

## M0 test plan (Mac, no console) — next actions

1. **Mapper unit test against the real fixtures** (JG's
   `../FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll`, x64 PE32+, exports
   `start_mod`/`stop_mod`, imports `UE4SS.dll!is_ue4ss_initialized` +
   `KERNEL32.dll!DisableThreadLibraryCalls`): assert export lookup finds
   both, IAT rows resolve against a two-row registry, relocation applies
   under a forced slide, and the negative fixtures refuse (`NOT-A-PE` →
   reason; x86 fixture → `Machine != 0x8664` reason). The synthetic PE
   (headers built in-test) stays in the plan for TLS-dir / CLR-dir /
   RWX-partition refusal coverage, which the fixtures don't carry.
2. **Bridge unit test**: macOS is SysV x86_64. Craft Win64-caller frames
   with small asm blobs (set RCX/RDX/R8/R9 + stack args, call the thunk),
   assert register remap, shadow-space shape, stack-arg staging, and
   XMM6-15/RSI/RDI survival across a SysV callee that clobbers them.
   The .S assembles clean on both macOS and ELF x86_64 targets today
   (verified to /dev/null); execution tests are still owed.
3. **Pin bake**: run `tools/pin_msvc_layout.sh` over the vendored
   `CppUserModBase.hpp` with the port's defines; bake offsets/slot order/
   mangled names into `ue4ss_compat.cpp` (flips `kLayoutPinned`).

## Census facts already consumed from JG (2026-10-13)

`../REPORT/evidence/UE4SS.dll.analysis.json` (real 20.5 MB framework
image): 634 imports / 29 DLLs; EH split across `VCRUNTIME140_1.dll`
(`__CxxFrameHandler4`) and `VCRUNTIME140.dll` (`__CxxFrameHandler3`,
`__C_specific_handler`, `_CxxThrowException`, `__RTDynamicCast`,
`__current_exception*`); framework image itself has a TLS directory
(40 bytes = `sizeof(PeTlsDirectory64)`); USER32(82)/GDI32(11) confirm the
headless-envelope rule. Details in the design's §5a. The staged shim
registry ships the T1 rows + fail-closed rows; the full row set is
census-generated in M1.

## Handoff to JG (pe_analyzer.py / fixtures)

Design section 10 lists the classifier requirements my loader consumes
(TLS dir flag, .pdata handler-flag scan, gs-prefix byte scan, import
census per DLL with C vs `?`-mangled tags, `mbm_mod_entry_v1` vs
`start_mod` export check, 16 KiB RWX-partition fact, secondary-vtable
count). Fixture interface: PE path + expected-census JSON + expected
route verdict. Coordination rule: your census output is the shim's
coverage matrix — the 85-95% mappability estimate in the design dies the
moment your first real census exists.

## Integration notes for Claude (integrator)

- Primary integration = manifest names `dlls/pe_adapter.sprx`; the #9
  host path is untouched. `patches/cppmod_pe_route.patch` is the
  alternative, behind `CAP13_PE_ROUTE`; both keep the MZ refusal intact.
- Console sequence is gated on J1 (exec anonymous memory in the game
  process) — design section 9. One-launch laws apply; 3-restart
  acceptance; no rapid-fire.
- Support matrix + test card are M3 deliverables; do not claim DLL-mod
  support beyond the proven envelope (headless, /MT, no static TLS, no
  executed EH, no game-code patching).
