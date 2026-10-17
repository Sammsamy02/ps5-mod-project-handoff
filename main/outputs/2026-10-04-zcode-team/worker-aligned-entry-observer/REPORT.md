# REPORT — worker-aligned-entry-observer (FINISH pass)

Original goal: an observer extension of the console-proven aligned-entry
module lineage that reports whether UE4SS came up — stat
`/data/plugins/UE4SS.log` every ~2 s for ~60 s, on growth a bounded 512-byte
tail read with the last line logged, plus existence stat of
`/data/modbridge/roots/PPSA33286/Mods/TownfallItemSpawner`; one worker
thread; imports only from the frozen 191-row contract; NEW revision with
distinct module name `modbridge_runtime_aligned_observer` and filename
`modbridge_runtime_aligned_observer.prx`; frozen module untouched. A prior
attempt died mid-work; this pass verified what was intact, did not redo
passing work, and completed the rest.

## Found intact (verified, not redone)

- All 15 source files under `source/` (lifecycle/import/diagnostic machinery
  byte-identical to the frozen lineage per `receipts/frozen-copy-identity.txt`;
  observer-specific `probe/observer.{c,h}`, `probe/probe.c`,
  `generated/probe_config.h`, harness `Packager.csproj`/`Program.cs`).
- Host test (`host-test/host_test.c`, `shim_drain.c`) with
  `receipts/host-test-result.txt`: all four recorded sha256 values re-hashed
  and matched byte-for-byte before any rework.
- Contract location identified:
  `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json`
  (191 strong rows, 6 weak, 0 missing, 0 conflicts; 187 FUNC + 4 OBJECT).

## Repairs completed this pass

1. `source/probe/probe.c`: restored `#include "../import_table.h"` and
   `extern void __cxa_finalize(void *);` — the observer revision had dropped
   both lines that the frozen probe carried; under the frozen recipe's
   `-Wall -Wextra -Werror` the file could not have compiled on target.
2. `sentinels.c` host syntax errors diagnosed as Mach-O-only noise (ELF
   `.init_array` sections) — no change needed; compiles clean under
   prospero-clang.
3. Packaging gap made explicit and reproducible: `dotnet` is absent on this
   Mac, so the final PRX packaging could not run. Added worker-dir
   `build_local.py` (frozen recipe adapted only in source list and output
   tree); verified by diff that the harness `Program.cs` differs from the
   frozen one ONLY in moduleName/exportLibraryName.

## New verification this pass

- Host test rebuilt from scratch and rerun: `HOST_TEST_PASSED all observer
  logic cases`, exit=0 (growth detection, log format/last-line edges incl.
  no-trailing-newline, 512-byte tail bounds, pread-fail drain fallback).
- Local target-compile smoke with the exact frozen toolchain and flags
  (LLVM-21.1.8 dist + ps5-payload-sdk, `LLVM_CONFIG` env as in the frozen
  recipe): all 9 units `-Werror`-clean; `ld.lld -r` composition with the
  pinned full-runtime object (sha256 3cbde160… verified before use)
  succeeded → `host-test/target-smoke/combined-observer.o` (sha256
  7fa9f3f12ca19d88dddf766d639e7922f1c29890d9279b72a72682ad473f0dfd).
  Local compile only; no console contact, no target execution.
- Import compliance proven against the contract JSON: every observer-called
  row present (stat/open/pread/read/close/nanosleep/pthread_create/join/self,
  `__error` via errno, plus direct-bound sceKernelOpen/Write/Close and
  `__cxa_finalize`); `lseek` absent from contract AND source; zero new
  import names; 187-row `import_names.h` = 191 contract rows minus the 4
  direct-bound rows, order preserved.

## Deliverables

- `OBSERVER-DESIGN.md` — behavior spec, read-only guarantee, import-compliance
  table vs the 191-row contract, host-test and build documentation.
- `STAGE-PLAN.md` — ploader.ini entry `= 2` alongside the existing entries
  (byte-identical otherwise), expected log lines with formats, result
  interpretation, rollback, non-goals.
- `build_local.py` — reproducible frozen-recipe build for the new revision
  (packaging command included; blocked only on dotnet).
- `receipts/finish-pass.txt` — repair record, host-test rerun, target smoke,
  contract facts. `CLAIM.json` updated with honest claim states;
  `prx_built` and `console_proven` are explicitly NOT claimed.

## Honest limits

No PRX artifact exists yet (packaging needs dotnet or the 5090 PC), no
console contact was made, and no console behavior is claimed. The observer
log, marker, build id, module name, and filename are all distinct from the
frozen aligned-entry module, which remains untouched.
