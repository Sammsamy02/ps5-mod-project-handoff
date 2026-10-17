# PACKAGING-SOLUTION — how SharpProspero packaging runs without a system `dotnet`

## The answer

`dotnet` is not absent from this machine's toolbox — it is absent from `PATH`.
Every successful PRX build today (frozen lineage `native-full-runtime-aligned-entry-1`,
`worker-composition-5-rebuild/prx-candidate-composition5-1`, `worker-startup-checkpoint/diag`)
used a **self-contained dotnet SDK stored inside the workspace**:

```
/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet
```

Version `10.0.401` (osx-arm64, `dotnet-sdk-osx-arm64.tar.gz` unpacked next to it,
acquisition recorded in `dotnet-acquisition.json`, NuGet cache + CLI home isolated
in the same directory). The observer worker's gap ("dotnet absent on this Mac")
was a PATH assumption, not a missing tool.

## The exact frozen invocation chain (verified in receipts)

From `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/package-commands.json`
and the two worker receipts (all rc=0):

1. `<dotnet> build <dir>/harness/Packager.csproj -c Release --ignore-failed-sources`
2. `<dotnet> <dir>/harness/bin/Release/net10.0/Packager.dll <combined.o> <import-contract.json> <out>/bare-original/<module>.prx`
3. `python3 finalize_sdk.py <bare> <profile.json> <out>/bare-sdk/<module>.prx`   (SDK stamp → 0x11000000)
4. `python3 make_fself.py <bare-sdk> <out>/<module>.prx`                          (fself wrapper)

No prebuilt SharpProspero CLI and no "10 recorded patches" route is needed for
this module family: the aligned-entry lineage harness references the already
built `SharpProspero.Link.dll` / `SharpProspero.Prx.dll` (absolute HintPaths in
`Packager.csproj`, DLLs present under
`.superpowers/native-dependency-spike/native-runtime-import-probe-1/packager/source/tools/.../bin/Release/net10.0/`).
(The patched-harness route in `worker-prx-link-gaps/package_prx.py` is only for
the supplier-composition lineage, which needs the `_DYNAMIC` DynamicWriter fix
and a 196-import derived contract.)

Environment isolation (mirrors `sharpprospero-host-tool-1/run-tool.py`; this
worker points the writable caches inside its own directory):

```
DOTNET_ROOT=<spike>/dotnet  DOTNET_CLI_HOME=<own>/dotnet-cli-home
DOTNET_MULTILEVEL_LOOKUP=0  DOTNET_CLI_TELEMETRY_OPTOUT=1  DOTNET_NOLOGO=1
NUGET_PACKAGES=<own>/nuget-packages
```

## Applied to the observer

Inputs (STAGE-PLAN frozen recipe; only source-list/module-name deltas):
- `worker-aligned-entry-observer/source/` + `build_local.py` recipe
  (`probe/observer.c` replaces `startup_input.c` + `memory_lua.c`, Lua include dropped)
- harness `Program.cs` — diff vs frozen harness = ONLY
  `moduleName/exportLibraryName "modbridge_runtime_aligned_entry"` →
  `"modbridge_runtime_aligned_observer"` (re-diffed this pass, 1 line)
- pinned full-runtime object `native-stage27-local-deps.o`
  sha256 `3cbde16051daf30294dfe23c8aff04980757beb742f8b859ea0eef3f69e67fb9`
- frozen 191-row `import-contract.json`
  sha256 `58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c`

## Provenance finding (blocks using the delivered combined object)

The observer worker's `host-test/target-smoke/combined-observer.o`
(sha256 `7fa9f3f12ca19d88dddf766d639e7922f1c29890d9279b72a72682ad473f0dfd`)
is **not** a frozen-recipe product and is **not packageable**:

- its `constructor_boundary.o` (2337e1bf…) was compiled WITHOUT the frozen flag
  set — byte-reproduced with a flag-less `prospero-clang++` invocation; the
  frozen-recipe `constructor_boundary.o` is byte-identical to the frozen
  lineage's own build-1 object (ff4f3e33…), while the target-smoke copy is
  unoptimized -O0-style codegen;
- in the delivered combined object `sceKernelOpen` appears as `UND NOTYPE`;
  the typed `FUNC` import declaration is lost in the `ld.lld -r` merge, and the
  SharpProspero harness **fails closed**:
  `diagnostic packaging refused: Missing exact typed owned import reference: sceKernelOpen`
  (rc=2; evidence: `build-1-pinned-input/refusal-receipt.json` + stderr capture).

The shipped candidate therefore uses the **frozen-recipe recombined object**
(sha256 `225f6d6895f0b745ff176ff0d7e3acf08585f006e0391fa9de3b3bb02f608887`),
which packages cleanly and whose import surface verifies exactly.

## Exact rebuild command (from scratch)

```
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-observer-packaging
python3 build_observer.py build-3      # frozen-recipe compile + ld.lld -r, pins 225f6d68…
python3 package_observer.py   build-3  # dotnet build + Packager + finalize_sdk + make_fself
python3 check_observer_artifact.py build-3
```

Expected result: `build-3/modbridge_runtime_aligned_observer.prx`
30,152,151 bytes, sha256 `1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30`.
(Fresh output directory required; the scripts refuse to overwrite existing trees.)
