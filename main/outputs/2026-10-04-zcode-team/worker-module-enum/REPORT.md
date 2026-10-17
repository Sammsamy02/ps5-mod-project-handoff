# worker-module-enum — REPORT

Status: **MODULE_ENUM_HOST_AND_NATIVE_OBJECTS_PASSED** (proposal-only; no target
execution, no console contact, no editor/engine files touched).

## What was built

Windows module enumeration / PSAPI family, per the retained DLL census
(`worker-dll-mod-retention/baseline-dll-census.json`). Exact census-observed
import set implemented (14 KERNEL32 names):

- dwmapi.dll: `FreeLibrary`, `GetModuleFileNameW`, `GetProcAddress`,
  `LoadLibraryW`
- UE4SS.dll: `GetModuleHandleA/W`, `GetModuleHandleExW`, `GetModuleFileNameA/W`,
  `GetProcAddress`, `LoadLibraryA`, `LoadLibraryExA/W`, `FreeLibrary`,
  `K32EnumProcessModules`, `K32GetModuleBaseNameA`, `K32GetModuleInformation`

Files (all under `outputs/2026-10-04-zcode-team/worker-module-enum/`):

- `sources/mb_win_module_enum.{h,c}` — owner: 64-slot module table (ASCII
  basename + full file name + base + image size + entry + refcount + borrowed
  export table), pe_loader key policy (ASCII case-fold, ".dll" gain, trailing
  dot suppression), NULL handle/name = executable-image record, GetProcAddress
  exact-name + 1-based ordinal lookup, LoadLibrary/FreeLibrary reference
  counting over static records and an optional acquire/release ops hook,
  EnumProcessModules-style listing, exact x64 MODULEINFO.
- `sources/mb_win_module_enum_abi.{h,c}` — session registry + 14
  `__attribute__((ms_abi))` thunks with exact census names + kernel32.dll
  binder (ordinal imports and other libraries UNRESOLVED), mapped Win32 errors
  (87/122/126/127/50/170/6/1114) readable via
  `mb_win_module_enum_abi_last_error()`.
- `tests/test_win_module_enum.c` — 60+ assertions: registration (duplicate
  base/folded-name/refused refs), case-insensitive ANSI handle lookup, .dll key
  policy, NULL=exe, proc name/ordinal, no-write-on-fail negative control,
  load/free refcounts (static + hook-acquired + hook release + OWNER_ERROR),
  file/base names + truncation 122, enumeration capacity semantics, MODULEINFO,
  all 14 thunks through the live binder, binder negatives (wrong library,
  ordinal, dead context), end-of-life (post-end thunk 6, re-begin, module-state
  BUSY).
- `INTEGRATION-NOTES.md` — 10-point wiring (promotion, bind order before the
  loader bridge, session wiring, table population from pe_loader records,
  acquire seam, refcount contract, last-error swap point, exact-semantics list,
  non-goals, verification loop).
- `receipts/receipt.json` + owner `.o`, bridge `.ll`, PS5-backend `.o`.

## Evidence

- Host test (`-arch x86_64`, Rosetta): **MODULE_ENUM_SELFTEST_RESULT 0**;
  ASan run also 0. Thunk typedefs carry ms_abi so the host route exercises the
  real Microsoft x64 convention (a SysV/ms_abi mismatch was found and fixed —
  it wedged Rosetta inside the thunks).
- Pinned toolchain (SDK v0.43 `prospero-clang`, LLVM 21.1.8 via `LLVM_CONFIG`,
  same invocation as worker-crt-startup-implement):
  - native owner object: `-Werror -ffreestanding -fno-builtin`, **0 undefined
    symbols** (struct copies replaced with field loops to keep it pure).
  - bridge route: FreeBSD9 frontend → unedited win64cc IR with **exactly 14
    win64cc definitions** → PS5 backend object whose undefined symbols are
    **only** the 12 owner core symbols it calls.
  - `_Static_assert` pins the exact x64 MODULEINFO layout (24 bytes; base@0,
    size@8, entry@16) in both routes.
- No target(PS5) execution and no Windows console contact:
  `target_executed: false` throughout.

## Honest limits

- Only the observed import set is bound (no EnumProcessModulesEx, psapi
  aliases, LoadLibraryEx flag modes beyond hFile=NULL/flags=0).
- GetModuleHandleEx PIN/UNCHANGED_REFCOUNT are accepted but identical because
  the runtime has no unmap model; FreeLibrary on static records keeps them
  findable (documented Windows-visible deviation).
- LoadLibrary without the acquire hook resolves only pre-registered table
  entries (host-test mode); real image acquisition stays with the pe_loader
  via the ops seam until a controller wires point 5 of the notes.
- This is a new family member, not an integrated one: nothing in
  `outputs/ps5-mod-bridge/` was modified (read-only, per bounds).
