# worker-combined-dll-allfam — REPORT

Worker: worker-combined-dll-allfam (ZCode team, 2026-10-04-zcode-team). Budget
used ~35 tool calls of 80. No network, no console contact, no target (PS5)
execution, no nested subagents; writes only under this worker directory.

Status: **ALL_FAMILIES_HOST_AND_SDK_ELF_CANDIDATE_PASSED** (build-time status:
`OWNED_CANDIDATE_HOST_CONTROLS_PASSED` then
`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; the ELF artifact was never
executed anywhere).

## What was built

A single console-payload source tree that exercises ALL six completed Windows
service families together — InitOnce + Mutex + FileIO + **CRT startup** +
**Module enumeration** + **Virtual memory** — through actual PE imports,
validated on the host x86_64 route and then composed into the full SDK ELF
candidate.

All inputs are read-only vendored copies; every change lives under
`outputs/2026-10-04-zcode-team/worker-combined-dll-allfam/`:

- `vendor/` — full copy of `worker-fileio-apply/vendor/` (windows-candidate,
  windows-services, windows-execution, windows-image, native-host).
- Family units vendored unchanged into `vendor/windows-services/`:
  `mb_win_crt_startup{,_abi}.{c,h}` (worker-crt-startup-implement),
  `mb_win_module_enum{,_abi}.{c,h}` (worker-module-enum),
  `mb_win_vm.{c,h}` (worker-virtual-memory). Three include-path adaptations
  (`"pe_module.h"` → `"../windows-execution/pe_module.h"`) and one documented
  bind-function adaptation in `mb_win_module_enum_abi.c` (see deviations).
- **New unit**: `vendor/windows-services/mb_win_vm_abi.{c,h}` — the VM family
  shipped native-C only, so this glue adds the session lifecycle, binder, and
  exactly five `ms_abi` thunks (VirtualAlloc/VirtualFree/VirtualProtect/
  VirtualQuery/VirtualQueryEx) following the mutex ABI pattern (dwSize is
  32-bit DWORD per the Microsoft docs; SIZE_T query returns widen).

### build.py extensions (same pattern as --mutex/--fileio)

`--crt-startup` (requires --entry), `--module-enum` and `--vm` (require
--workers): profile defines, native/abi source lists, def-lib links
(crtstartup/modenum/vm, consumer only), fixture compile defines, receipt
`crt_startup_profile`/`module_enum_profile`/`vm_profile`, win64cc IR
expectations `{crt_startup_abi: 8, module_enum_abi: 14, vm_abi: 5}`, and
result-key assertions (`crt_startup`/`module_enum`/`vm` present exactly when
enabled).

### Fixture hooks, def files, census entries

- `tests/worker-crtstartup.h` + `worker-crtstartup-crt.def`: the seven
  census-observed `api-ms-win-crt-runtime-l1-1-0.dll` startup imports
  (_initterm blind walk with interior NULL, _initterm_e errno stop at 99,
  _crt_atexit + _cexit LIFO, the onexit trio incl. the documented
  unbacked-table NULL registration). Codes -90..-97.
- `tests/worker-moduleenum.h` + `worker-moduleenum-kernel.def` (all 14 census
  names; the linker materializes referenced ones): exercises the five
  loader-unique names (GetModuleHandleExW wide probe, GetModuleFileNameA,
  K32EnumProcessModules need==8, K32GetModuleBaseNameA,
  K32GetModuleInformation MODULEINFO 24B) against a static probe record
  ("mbmodenum.dll"/"MbProbe"/image_size 4096) registered natively while the
  consumer session is open. Codes -100..-104.
- `tests/worker-vm.h` + `worker-vm-kernel.def`: full VirtualAlloc reserve|
  commit (write/read-back, query state/protect/type/base), VirtualProtect
  (old-protection return), VirtualFree exact-base release + double-release
  refusal, free-gap query, QueryEx pseudo-handle vs foreign-process refusal.
  Codes -110..-117.
- All three hooks run inside the consumer `LoaderExercise` window (module
  ACTIVE) so worker-call counts and every earlier family's evidence stay
  byte-identical; `results` stays [86,93].

### owned-runtime integration

Report fields + result keys for the three families; owners opened/closed at
unique stages (VM 53/54; CRT has no owner object; module-enum probe registered
at consumer service_begin); sessions end newest-first in `service_end`;
`IMPORT_CAPACITY` headroom documented (full clock stack = 78 ≤ 96 sync cap).

### Bind-order composition rule (important, verified by failure)

The first host run failed at stage 8: the module-enum binder, placed before
the loader-bridge fallback, shadowed GetModuleHandleA/GetProcAddress/
LoadLibraryA and broke the base exercise (77/80 imports bound). Fix: the three
new binders now sit AFTER `mb_win_loader_runtime_bind`, so the retained-image
machinery keeps every pe_loader-backed module/loader resolution and the new
families win only census names no other family claims. With all flags off the
chain is behaviorally identical to the pre-patch tree.

## Evidence

1. Combined host validation (`--host-only --entry --clock --workers --sync
   --init-once --mutex --fileio --crt-startup --module-enum --vm`):
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`
   (`out-host-validation/cec4f8112cab412481e297af06ed85af/receipt.json`).
   Both host driver modes (normal, release) passed; final record:
   entry attach/detach [7,7]/[0,0], contract_errors 0; worker begin
   [_,_,258,259] timeout proof and result 0x81234567/cookie; all six family
   keys `{"enabled":1}`; consumer_imports 78, helper 10 (exact-formula match);
   results [86,93]; cleanup retired {pending:0,retired:1}.
2. Full SDK composition (same flags minus --host-only, plus
   `--fileio-root /data/modbridge --report-path
   /data/modbridge-owned-dll-candidate.jsonl`):
   `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`
   (`out-sdk-composition/133c5e9e59f24c2fb57e7e3422c665a1/receipt.json`).
   - Artifact `owned-dll-candidate.elf`: 300456 bytes,
     sha256 96296c1c92850623…; SDK AMD64 ET_DYN; genuine crt1 `_start`.
   - Native/bridge ABI: unedited win64cc IR counts exactly 8 (crt startup),
     14 (module enum), 5 (vm) definitions, plus every prior family count.
   - 55 undefined imports, all declared by the SDK
     (clock_gettime + sceKernelMprotect: libkernel_web.so; the VM backend's
     new malloc/free: libSceLibcInternal.so).
   - 97 recorded build commands; 121 hashed source files; source digests
     re-verified at the end of the run.
   - The target-side report path `/data/modbridge-owned-dll-candidate.jsonl`
     is compiled into the profile only; the build never opened it.
   - Only warnings: the two pre-existing rust-lld `/align` notices, identical
     to the fileio baseline.

## Honest limits and deviations

- No target execution, no Windows console contact, no PRX created; the ELF is
  a compiled candidate only. No new visual/audio/full-run acceptance.
- `mb_win_module_enum_abi.c` bind adaptation (documented in-file): a record
  whose service never began a module-enum session (the helper fixture, because
  the owner is single-pin) passes imports through UNRESOLVED instead of
  ERROR, so the shared bind chain stays functional for both fixture records.
- Module-enum census names GetModuleHandleA/W, GetProcAddress, LoadLibraryA/W,
  LoadLibraryExA/W, FreeLibrary stay bound to the loader bridge (not to this
  family) in the combined payload — the family's five unique census names are
  the ones exercised here; 14 names remain compiled and IR-verified.
- CRT `_seh_filter_dll` (vcruntime140.dll home, zero census rows) is bound but
  not exercised; the onexit table is exercised in its documented unbacked
  state (registration NULL, execution 0), not with entry-adapter storage.
- VM protection changes are bookkeeping-only: the backend installs no host-
  enforced page protection (documented emulation contract); pages are real
  writable host memory via the SDK libc allocator.
- A stale comment corner: the IMPORT_CAPACITY note cites the non-clock entry
  base (76); with --clock the consumer binds 78 — both fit the 96 cap.

## Key paths

- Build driver: `outputs/2026-10-04-zcode-team/worker-combined-dll-allfam/vendor/windows-candidate/build.py`
- New glue: `vendor/windows-services/mb_win_vm_abi.{c,h}`; hooks/defs under
  `vendor/windows-candidate/tests/worker-{crtstartup,moduleenum,vm}*`
- Raw runs: `out-host-validation/` (+`.log`), `out-sdk-composition/` (+`.log`)
