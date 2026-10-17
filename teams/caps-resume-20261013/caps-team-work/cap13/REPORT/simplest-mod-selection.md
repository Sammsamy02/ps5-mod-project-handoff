# cap13 simplest-mod selection — what the #9 system must support, measured

Selection rule (from the task): fewest imports, no game-code patching, no EH, no TLS,
from `BUILD/analysis-summary.json` (23 files, all measured with `TOOL/pe_analyzer.py`).

## Selection

**`SAMPLE/cap13-mod-keybinds-noeh.dll`** — the native port of the upstream UE4SS "Keybinds" mod,
compiled with `-fno-exceptions` to strip the CRT EH floor.

sha256 `0ed2c758094a46bc…` (deterministic build, `-timestamp:0`) (full hash in `BUILD/analysis-summary.json`) · 5120 bytes ·
classified **native-x64** · PE32+ · subsystem WINDOWS_GUI · is_dll=True.

Measured surface (from `REPORT/evidence/cap13-mod-keybinds-noeh.json`):

- **Imports: exactly 1 DLL, 3 functions** — `kernel32.dll: GetProcessHeap, HeapAlloc, HeapFree`
  (the C++ `operator new`/`delete` in the minicrt). Nothing else.
- **Exports: exactly 1** — `mbm_mod_entry_v1` (ordinal 1), the #9 NativeModAbi v1 contract.
- **EH: none** — no `_CxxFrameHandler*`, no `_CxxThrowException` (measured; the default-settings
  build `cap13-mod-keybinds.dll` shows the pair, this build proves they are droppable).
- **TLS: directory present but inert** (CRT `_tls_used` anchor, zero callbacks, no `thread_local`
  use, no `Tls*` imports). A no-anchor minicrt variant removes the directory; API-level TLS need
  measured 0.
- **Game-code patching: none** — no VirtualProtect/WriteProcessMemory/FlushInstructionCache.
- **File/thread/timing/GUI/user32: none.**

Behavior (kept faithful to upstream `assets/Mods/Keybinds/Scripts/main.lua`): on
`on_unreal_init` it calls `host->register_key_down` seven times (Ctrl+J/H/9/8/7/6/5, the upstream
ObjectDumper / CXXHeaderGenerator / UHTCompatibleHeaderGenerator / DumpStaticMeshes /
DumpAllActors / DumpUSMAP / DumpJMAP bindings). Every callback only consults state and returns
`mbm_status`; all real work is delegated to the host. It reads nothing from disk, writes nothing,
spawns nothing, patches nothing.

Runner-up (if a PC-UE4SS.dll-import flavor is wanted instead): `cap13-ue4ss-flavor-console-dump.dll`
— 4 imports (kernel32 ×2 + the real UE4SS.dll C-API `is_ue4ss_initialized`, `execute_lua_in_mod`),
exports `start_mod`/`uninstall_mod`, no EH/TLS/patch. It requires the host to export those two
symbols, which the shipped PS5 runtime does not (measured: 210 dynamic symbols, 2 defined) — that
is why the mbm flavor is the right selection for #9.

## Exactly what it needs to run through the #9 system

1. **Loader (PE side):** map the image; bind 3 `kernel32.dll` imports by name
   (`GetProcessHeap`, `HeapAlloc`, `HeapFree`) — the PS5 runtime must expose or forward these
   (the existing Windows-worker bridge already exercises the same KERNEL32 thread/heap family);
   no TLS callback invocation is required (inert directory); no entry point is required
   (linked `-noentry`; DllMain is absent by design, `mbm_mod_entry_v1` is the only contract).
2. **Admission:** read export `mbm_mod_entry_v1`, verify `MBM_ABI_MAGIC`/`MBM_ABI_VERSION`/
   `struct_size` on the returned `mbm_mod_v1` and on the `mbm_host_v1` the host passes in
   (both checks are implemented inside the mod; the host must refuse on any mismatch, never guess).
3. **Host services the mod actually calls (complete list, from the mod source):**
   `mbm_host_v1.log` (status lines), `mbm_host_v1.register_key_down` ×7, and admission/registration
   bookkeeping via `unregister` at shutdown. Nothing else — no `find_object`, no `property_*`,
   no `call_function`, no `execute_in_game_thread`, no GUI table.
4. **Hook families needed:** none beyond the keybind input path (no ufunction hook, no tick hook).
   This makes it the cheapest possible live test of the #9 input/keybind route end-to-end.
5. **Lifecycle:** `on_program_start` (log), `on_unreal_init` (7 registrations),
   `on_update` (no-op returning MBM_OK), `destroy` (no-op MBM_OK). No allocation leaves the module
   (heap use is internal only), so nothing to reconcile at uninstall.
6. **Failure containment:** every callback returns `mbm_status`; with exceptions compiled out the
   mod structurally cannot throw across the boundary — exactly the NativeModAbi.h guarantee
   ("no C++ type, vtable, RTTI, exception or allocator ever crosses the host <-> mod boundary").

## Why this is the right first mod for #9

- It is the measured minimum: the whole import surface is 3 well-understood heap functions.
- It exercises the three things #9 must prove for ANY native mod: image load + import binding,
  single-export admission, and one host service route (keybinds) — with no confounds.
- It is a real, named, upstream-mod behavior (Keybinds), not a synthetic probe; the deliberate
  probe remains `cap13-feature-probe.dll` (EH/TLS/patch APIs) for negative/robustness testing.

Evidence chain: `BUILD/src/port_keybinds.cpp` (source) → `BUILD/build_all.py` (toolchain:
llvm@18 clang `-target x86_64-pc-windows-msvc` + lld-link; kernel32 import lib built by
llvm-dlltool from the real public export names) → `SAMPLE/cap13-mod-keybinds-noeh.dll` →
`REPORT/evidence/cap13-mod-keybinds-noeh.json` (analyzer output) →
`BUILD/analysis-summary.json` (rollup). Static evidence only; nothing was executed.
