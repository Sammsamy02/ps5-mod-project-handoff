# REPORT — worker-observer-packaging (2026-10-04)

Deliverables: frozen PRX + build receipts + exact rebuild command (below),
PACKAGING-SOLUTION.md (the dotnet answer), CLAIM.json, this report.
No console contact; no native execution; writes confined to this directory.

## 1. How the working builds ran the SharpProspero packager without dotnet

They never needed a system `dotnet`. All three same-day successful PRX builds
shell out to a **self-contained dotnet SDK vendored in the workspace**:

- Binary: `.superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet`
  (`--version` → 10.0.401; tarball + NuGet cache + CLI home sit beside it).
- Evidence: `native-full-runtime-aligned-entry-1/build-1/package-commands.json`,
  `worker-composition-5-rebuild/prx-candidate-composition5-1/build-receipt.json`,
  `worker-startup-checkpoint/diag/build-min-72abf1a3cd8d/build-receipt.json` —
  every `dotnet` argv uses that absolute path, all rc=0.
- Invocation chain (frozen, reused verbatim here):
  1. `dotnet build harness/Packager.csproj -c Release --ignore-failed-sources`
  2. `dotnet harness/bin/Release/net10.0/Packager.dll <combined.o> <contract.json> <bare-original.prx>`
  3. `python3 finalize_sdk.py <bare> <profile.json> <bare-sdk.prx>` (SDK → 0x11000000)
  4. `python3 make_fself.py <bare-sdk> <final.prx>`

The aligned-entry lineage harness needs no prebuilt SharpProspero CLI and no
patched-harness route: `Packager.csproj` HintPaths the already-built
`SharpProspero.Link.dll`/`SharpProspero.Prx.dll` (verified present). The
patched-harness/"10 recorded patches" machinery in worker-prx-link-gaps is
supplier-composition-lineage-only. Details: PACKAGING-SOLUTION.md.

## 2. What was packaged

`modbridge_runtime_aligned_observer.prx` — 30,152,151 bytes,
sha256 `1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30`
(copy at this directory root; canonical in `build-1/`, reproduced in `build-2/`).

Inputs (STAGE-PLAN frozen recipe, only source-list/module-name deltas):
- observer sources (9 units; `probe/observer.c` replaces `startup_input.c` +
  `memory_lua.c`; Lua include dropped), frozen compile flags, `ld.lld -r`
  against pinned full-runtime object `3cbde160…`
  → combined object `225f6d6895f0b745ff176ff0d7e3acf08585f006e0391fa9de3b3bb02f608887`
- frozen 191-row `import-contract.json` (`58d94bc6…`, sha-pinned by the harness)
- observer harness `Program.cs` (re-diffed vs frozen: the ONLY change is
  `moduleName/exportLibraryName = modbridge_runtime_aligned_observer`)
- observer `profile.json` (module/log paths + marker per STAGE-PLAN;
  `finalize_sdk.py` consumes only `sdk_version` = 0x11000000)

## 3. Provenance finding: delivered combined object is NOT packageable

The task-named input `host-test/target-smoke/combined-observer.o`
(`7fa9f3f1…`) deviates from the frozen recipe and fails the packager:

- Its `constructor_boundary.o` (`2337e1bf…`) was compiled WITHOUT the frozen
  flag set — flag-less `prospero-clang++` reproduces it byte-for-byte;
  unoptimized -O0-style codegen vs the frozen -O1 output. The frozen-recipe
  `constructor_boundary.o` is byte-identical to the frozen lineage's own
  build-1 object (`ff4f3e33…`). 8 of 9 target-smoke units match the frozen
  recipe exactly; only the C++ unit deviates (despite the finish-pass receipt
  claiming "exact frozen recipe" for all 9).
- In the delivered combined object `sceKernelOpen` is `UND NOTYPE` (typed
  `FUNC` declaration lost in the `ld.lld -r` merge); the SharpProspero harness
  fails closed: `Missing exact typed owned import reference: sceKernelOpen`
  (rc=2). Evidence: `build-1-pinned-input/refusal-receipt.json`,
  `refusal-stderr.txt`.

Nothing was replayed blindly: the deviation is recorded and the shipped
candidate uses the frozen-recipe object — the only input the harness accepts.

## 4. Verification (checkpoint-probe pattern, both builds)

`check_observer_artifact.py` (adapted from `check_diag_artifact.py`):
`LOCAL_PACKAGE_CHECKS_PASSED` on build-1 and build-2:

- fself container law: wrapper magic 4f153d1d, embedded ELF header equality
  (section count zeroed as make_fself does), payload sha256, all segments
  byte-equal to the bare PRX.
- ELF module: PT_SCE_MODULE unique, SDK stamp 0x11000000, explicit
  DT_INIT/DT_FINI, loader owns no init/fini arrays.
- init/fini slots **232/2 — exactly the frozen lineage values** (the observer
  adds no constructors, as STAGE-PLAN predicted).
- Import surface vs the 191-row contract: exactly 191 GLOBAL UND dynsyms,
  decoded from the on-device SceNid long form (`<nid>#<lib>#<mod>`), set
  equality with the contract, zero malformed/unknown NIDs — same representation
  as the console-proven frozen PRX (whose UND dynsym count is also exactly 191).
- `module_start`/`module_stop` exported FUNC; no boundary symbol imported;
  typed import declarations verified in the packaged combined object.
- Determinism: build-2 (fresh compile + link + dotnet packaging) is
  **byte-identical** (same sha256); combined-object compile determinism also
  reproduced across fresh output trees.

## 5. Exact rebuild command

```
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-observer-packaging
python3 build_observer.py build-3      # frozen-recipe compile + link; pins 225f6d68…
python3 package_observer.py   build-3  # self-contained-dotnet packaging chain
python3 check_observer_artifact.py build-3
```
Expected: 30,152,151 bytes, sha256 `1afa0997…`. Fresh output dir required.

## 6. Bounds

Local packaging + static verification only; no console contact, no staging, no
promotion claim. The STAGE-PLAN's console steps remain gated on explicit user
authorization. Files: `CLAIM.json`, `PACKAGING-SOLUTION.md`, this report,
`build_observer.py`, `package_observer.py`, `check_observer_artifact.py`,
`build-1/`, `build-2/` (full receipts + compile-commands + artifact-checks),
`build-1-pinned-input/` (delivered-object refusal evidence),
`modbridge_runtime_aligned_observer.prx` (frozen candidate copy),
`observer-src/` (read-only copy of the observer sources).
