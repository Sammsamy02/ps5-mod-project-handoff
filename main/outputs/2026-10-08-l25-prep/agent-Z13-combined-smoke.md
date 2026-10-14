# agent-Z13: combined-tree host compile-smoke (l25 typescan-r4)

Date: 2026-10-08. Tree: `outputs/2026-10-08-l25-runtime/typescan-r4/` (candidate). Work dir:
`/tmp/l25-combined-smoke/` (adapt.py, run.py, run_prospero.py, stubs/, shimmed/, obj/, logs/,
probe_main.cpp, odr_*.cpp, stub_syms_final.txt, odr_new_a.txt / odr_new_b.txt / odr_old.txt).
**The tree was not modified.** Every deviation from the real tree lives only under /tmp and is
disclosed below.

## 1. Method

Six changed l25 TUs compiled together (the full cross-agent surface): `TypeChecker.cpp`,
`UE4SSProgram.cpp`, `LuaMod.cpp`, `UnrealInitializer.cpp`, `UObjectArray.cpp`, `ClassListener.cpp`.
Host compiler: Apple clang 17.0.0 (arm64). Per-TU argv = the unit's own argv from
`2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json` (same -I list, same vfs overlay, same
flags incl. `-std=c++23 -fms-extensions -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero`), with the
source swapped to the l25 candidate and these host adaptations:

- `-I` order: `/tmp/l25-combined-smoke/stubs` + `typescan-r4/candidate/include` +
  `typescan-r4/include` + `typescan-r4/candidate/generated_include` + `typescan-r4/candidate/src`
  first, then the receipt's full -I list (so `<Unreal/UObjectArray.hpp>` resolves to the l25
  candidate header everywhere).
- `-D__PROSPERO__ -D__SCE__ -fshort-wchar` ( Prospero branches active; wchar_t 16-bit like the
  real target).
- Stub headers (only target gates relaxed, content otherwise verbatim copies):
  `stubs/Unreal/Core/Prospero/ProsperoPlatform.hpp` (drops the x86_64-SDK `#error` gate and the
  `sizeof(wchar_t)==2` assert; kept `sizeof(void*)==8 && sizeof(long)==8`) and
  `stubs/polyhook2/PolyHookOs.hpp` (drops the `__x86_64__` gate).

A **fidelity pass** additionally ran the REAL target compiler (`prospero-clang++`, receipt argv
verbatim, no stubs, no shims) over the same six real l25 files, exactly the way
`typescan-r4/target_compile.py` would drive them (it inserts only `-I include`,
`-I candidate/generated_include`, `-I <src parent>`).

## 2. Compile matrix

| TU | host -fsyntax-only | host -c | prospero -fsyntax-only (real compiler, real file) | prospero -c |
|---|---|---|---|---|
| TypeChecker.cpp | PASS | PASS | PASS | PASS |
| UE4SSProgram.cpp | PASS | PASS | PASS | PASS |
| UnrealInitializer.cpp | PASS | PASS | PASS | PASS |
| ClassListener.cpp | PASS | PASS | PASS | PASS |
| UObjectArray.cpp | **FAIL** (E1) | PASS w/ shim S1 | **FAIL** (E1) | **FAIL** |
| LuaMod.cpp | **FAIL** (E2) | PASS w/ shim S2 | PASS | PASS |

Shims (tree untouched; /tmp copies with exactly one changed line each, disclosed headers):
- S1 `shimmed/UObjectArray.cpp`: line ~610 `const_cast<void*>(...)` -> `static_cast<void*>(...)`.
- S2 `shimmed/LuaMod.cpp`: line ~1396 `std::ifstream file(wide_path, ...)` ->
  `std::ifstream file(std::filesystem::path(wide_path), ...)`.

Link stage: `clang++ -r` partial link of all six objects: **exit 0, no duplicate strong symbols**.
Smoke binary: `probe_main.o + combined-partial.o + inert extern stubs`, linked with
`-undefined dynamic_lookup`, **ran to completion, all checks passed**. The probe defines the
`extern "C" void diagnostic_event(const char*, unsigned)` sink (same signature all four phase-tag
TUs declare) and proves the slot-4 vtable layout at runtime. 238 inert storage stubs
(`stubs_data.s`, full list `stub_syms_final.txt`) cover extern symbols owned by the other ~368
TUs of the real 374-object composition (StaticClassStorage/MemberOffsets statics, LuaLibrary
fns, bridge extern-Cs) — never read or executed by the probe.

## 3. ODR / vtable verdict for FUObjectDeleteListener (the slot-4 repair)

1. **Record layout**: `-Xclang -fdump-record-layouts` blocks for `FUObjectDeleteListener` are
   **byte-identical** via angle-include and absolute-path routes (`odr_new_a.txt` ==
   `odr_new_b.txt`), and identical to the old port-tree header too: `sizeof=8, align=8` (vptr
   only). The l25 edit adds no data members; the ABI delta is purely vtable slot count, which
   record layouts cannot show — covered next.
2. **Static vtables, host arm64 (new header)**: `__const` relocation maps in
   `unreal-flat-classlistener.o` show `FClassDeleteListener` and `FShutdownDeleteListener`
   vtables with exactly 5 virtual slots; slot 4 (+0x30 from slot base) =
   `__ZNK2RC6Unreal22FUObjectDeleteListener16GetAllocatedSizeEv` (inlined base impl, returns 0);
   slots 0/1 = D1/D0 dtors, slot 2 = NotifyUObjectDeleted, slot 3 = OnUObjectArrayShutdown.
3. **Static vtable, REAL PS5 x86-64 object** (prospero-clang, new header on -I):
   `.data.rel.ro._ZTVN2RC6Unreal20FClassDeleteListenerE` relocs:
   `+0x08 ZTI, +0x10 D2, +0x18 D0, +0x20 NotifyUObjectDeleted, +0x28 OnUObjectArrayShutdown,
   +0x30 GetAllocatedSize` — exactly the vendored UE 5.6.1 engine layout.
4. **Runtime probe (smoke binary)**: pmf slot deltas Notify->Shutdown->GetAllocatedSize are
   consecutive (1,1); `sizeof(FUObjectDeleteListener)==8`; raw `vptr[4]` dispatch (the engine's
   exact call shape from FUObjectArray::GetDeleteListenersAllocatedSize) returns the override
   value 42; normal virtual dispatch reaches all three overrides. `ALL SLOT CHECKS PASSED`.
   (Cosmetic note: the pmf "virtual" low-bit flag reads 0 on arm64-apple; the ptr deltas are
   the evidence used.)
5. **Old-header control**: a TU compiled against the port-tree `Unreal/UObjectArray.hpp`
   (unchanged there: base ends at slot 3, no GetAllocatedSize) emits a 4-slot vtable
   (`odr_old_vtable.o`: last entry OnUObjectArrayShutdown). On the real PS5 compiler the
   no-`candidate/include` variant of ClassListener.o has vtable section size 0x30 (4 slots);
   with `candidate/include` it grows to 0x38 (5 slots) with the +0x30 GetAllocatedSize reloc.
   Mixed-header builds are a **silent latent ODR hazard** (no link error): any listener compiled
   against the old header leaves the engine's slot-4 call dispatching past its vtable.

## 4. Classified findings

### (a) Real cross-agent / change-set bugs — R's target compile is NOT clear to proceed

- **E1 — ill-formed const_cast, blocks the target compile.**
  `typescan-r4/candidate/src/UObjectArray.cpp:610` (in new L25
  `RemoveUObjectDeleteListener` diagnostics):
  `const_cast<void*>(DeleteListeners.GetData())` where `GetData()` returns
  `RC::Unreal::FUObjectDeleteListener**` (non-const) — `const_cast` from `T**` to `void*` is
  ill-formed. Verbatim (identical from host clang AND prospero-clang):
  `error: const_cast from 'ElementType *' (aka 'RC::Unreal::FUObjectDeleteListener **') to 'void *' is not allowed`
  Note the sibling lines in the same function are legal because they const_cast a `const void*`
  variable; only this direct-call site is wrong. Minimal fix for the fix-agent:
  `static_cast<void*>` (or drop the cast and pass through a `const void*` local like `pre_data`).
- **E3 — the l25 header is not routed to the compile (silent ABI-repair drop).**
  Neither `target_compile.py` (inserts only `-I typescan-r4/include`,
  `-I candidate/generated_include`, `-I <src parent>`) nor `2026-10-08-l24-build-r2/headers.vfs.json`
  (zero entries for `Unreal/UObjectArray.hpp`) puts `typescan-r4/candidate/include` on any
  include path. Verified consequence on the real compiler: ClassListener.cpp compiled the
  `target_compile.py` way resolves `<Unreal/UObjectArray.hpp>` to the OLD port-tree header and
  emits a **4-slot** vtable (section 0x30); only with `candidate/include` prepended does it emit
  the intended 5-slot vtable (0x38, +0x30 GetAllocatedSize). So as currently routed, the entire
  rebuild would compile the l25 sources against the old base — the slot-4 repair silently
  disappears, and out-of-six consumers like `LuaType/FLuaObjectDeleteListener`
  (l22 diagnostics `LuaUObject.hpp:32`, compiled via the vfs overlay in `LuaUObject.cpp`)
  keep vtables that end at slot 3. R must add a vfs overlay entry mapping
  `port/unreal/include/Unreal/UObjectArray.hpp` ->
  `typescan-r4/candidate/include/Unreal/UObjectArray.hpp` (the overlay reaches ALL 374 units;
  the -I prefix only reaches units it is prepended to), or otherwise put the candidate header
  first for every TU that can instantiate a `FUObjectDeleteListener`-derived vtable.

### (b) Stub gaps (host-only, resolved; no target impact)

- **E2 — LuaMod.cpp:1396** `std::ifstream file(wide_path, std::ios::binary)` with
  `std::wstring`: Apple libc++ lacks the wstring-constructible ifstream overload that the Sony
  target libc++ provides. Pre-existing l23 code (NOT an l25 edit; the l25 LuaMod diff is purely
  the phase-tag blocks), and the real prospero compile PASSES it — host-only divergence, shimmed
  for the object stage (S2).
- wchar_t==2 static asserts in `HAL/Platform.hpp:1185,1189` and `Helpers/Utf16Utf8.hpp:13`:
  resolved with `-fshort-wchar` (matches the real Prospero target where wchar_t is 2 bytes).
- `ProsperoPlatform.hpp` `#error This platform profile requires the actual x86_64 Prospero SDK
  target.` and `PolyHookOs.hpp` `#error PolyHook Prospero header support requires the x86_64
  SDK target.`: stub copies with only the target gates relaxed (see section 1).
- 238 link-time inert stubs for externs owned by TUs outside the six-unit set (expected for a
  6-of-374 link; the partial-link already proved our six define no conflicting symbols).

### (c) Pre-existing tree issues (unchanged by l25, inherited)

- `-Wswitch` warnings in port `Unreal/Core/Templates/Atomic.hpp:241,262` (enumeration values
  SequentiallyConsistent/Count not handled) — the "inherited Unreal-header warnings" already
  disclosed in the l25 REPORT.md.
- The wide-path ifstream block in LuaMod (l23 legacy "for Windows filesystem operations" code)
  itself — flagged above as E2 only because of the host stdlib; on-target it is fine.

## 5. Cross-agent integration checks that PASSED

- `extern "C" void diagnostic_event(const char*, unsigned int)` is declared consistently in
  UE4SSProgram.cpp, LuaMod.cpp, UnrealInitializer.cpp ( Prospero-guarded there) and used by
  ClassListener.cpp's `L25CallMarker::emit_event`; all leave it undefined (`U _diagnostic_event`)
  for the in-link diagnostic.c definition — exactly one signature, no drift.
- Registration wiring is symbol-consistent: luamod.o + unrealinitializer.o reference
  `FUObjectArray::AddUObjectDeleteListener`, classlistener.o references
  `RemoveUObjectDeleteListener`; both defined (T) in unreal-uobjectarray.o; no duplicates.
- Phase-tag blocks (LuaMod first-invocation / throttled pump / queue-depth) compile clean on
  both compilers and reference only the shared sink + existing locals.
- Guard/TypeChecker edits compile clean on both compilers; TypeChecker.o built with zero errors.

## 6. Is R's target compile clear to proceed?

**No — two blockers, both cheap to fix:**
1. E1 (one-line const_cast fix in `candidate/src/UObjectArray.cpp:610`).
2. E3 (route the new header: vfs overlay entry for `Unreal/UObjectArray.hpp` — or prepend
   `-I typescan-r4/candidate/include` for every unit — otherwise the rebuild silently compiles
   the slot-4 repair away).

After those two, the combined six-TU surface is green on both the host smoke and the real
prospero compiler; the slot-4 vtable is verified byte-exact against the vendored UE 5.6.1
layout on the actual target ABI.

## 7. Reproduction

```
cd /tmp/l25-combined-smoke
python3 adapt.py && python3 run.py <unit> syntax|obj     # host pass (per unit)
python3 run_prospero.py syntax|obj                        # real target compiler
./smoke                                                   # slot probe (needs obj/ built)
```
Logs: `logs/{syntax,obj,prospero-*}-<unit>.log`, `logs/odr_tu_*.layout.log`. Objects:
`obj/*.o`, `obj/combined-partial.o`, `prospero-obj/*.o` (real PS5 ELF),
`prospero-obj/classlistener-NEWHEADER.o` (with-candidate/include control).
