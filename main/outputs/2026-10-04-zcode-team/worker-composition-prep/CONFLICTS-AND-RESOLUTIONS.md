# CONFLICTS-AND-RESOLUTIONS.md — patch series vs current spike state

worker-composition-prep, 2026-10-04. Scope: the 8 patches in
`outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/patches/` against the CURRENT
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/{include,src}` (the
composition-1 integrated baseline) and the existing dl-retention gate. All resolutions were
APPLIED AND VERIFIED in the vendored tree (`staging/`), never in the real spike tree.

## C1. LiveLoaderObservedFacts.hpp — NO textual conflict (verified, resolved trivially)

- Concern: composition-1 promoted a `LiveLoaderObservedFacts.hpp` data header into the
  spike; patch 0001 might target a different base.
- Fact: patch 0001 applies CLEANLY (all hunks) to the current spike copy, and the patched
  result is byte-identical (sha256 `8b81038262af671b…`) to the worker's verified vendored
  copy. The worker's patch base IS the composition-1-promoted header; the patch is purely
  additive (in-game observed values, the two distinct refusal codes -4001/-4002, VMMAP
  sizing constants 1408-entry TU buffer / 1024-snapshot).
- Resolution: none required beyond applying 0001 (bundled file, COPY-MANIFEST row 1).
- Post-condition recorded: the header contains the refusal constants consumed by the
  patched SCE TU (grep-verified: `kRefusalDtDebugUnfilledIngame` at its -4002 return,
  `kRefusalExecHandleRawZeroIngame` at its -4001 paths).

## C2. Existing dl-retention gate — SEMANTIC conflict risk, resolved by keeping the exact flag

- Concern: composition-1 compiled the SCE TU with `-DLMF_DISABLE_DL_RETENTION` (verified in
  composition-1 `commands.json`: it is the ONLY `--extra-define`, applied to all three
  extra units). A recomposition that drops or alters this flag would silently change
  lifetime semantics or reintroduce `dlopen`/`dlclose`/`_DYNAMIC` imports.
- Facts:
  - The patched SCE TU still honors the macro (3 references in
    `src/live_module_os_ops_sce.cpp`); degraded dl retention stays EXACTLY as composition-1
    left it (g3): a supplier that cannot retain yields zero facts; nothing dereferenced.
  - Under the define, the patched TU's undefined set drops `dlopen`, `dlclose` AND
    `_DYNAMIC` vs the old TU (nm-verified on the template-reproduced ELF object:
    `getpid, memcpy, memset, sceKernelGetModuleInfo, sysctl` + the scan ref only).
  - The VmScan identity route records `retention.kind == None` honestly instead of faking
    or aborting (worker REPORT, N-checks).
- Resolution: the recomposition command keeps `--extra-define LMF_DISABLE_DL_RETENTION`
  (RECOMPOSE-COMMANDS.md Step 2). My template reproduction compiled ALL FOUR extra units
  with that exact define, matching composition-1 — so the prediction covers the true flags.
- Post-condition to assert after composition: zero `dlopen|dlclose|_DYNAMIC` symbols in
  the composed object (Step 4.2 command).

## C3. `_DYNAMIC` removal vs the undefined-baseline gate — resolved analytically (no action)

- Concern: the build gate REFUSES removed/changed undefined symbols even with
  `--extend-undefined-baseline` (build_runtime.py source: extension only tolerates
  additions). Dropping `_DYNAMIC` vs the composition-1 object could trip it.
- Fact: the gate baseline is the script's HARD-CODED notify-cancel receipt
  (`ue4ss-native-notify-cancel-1/build-1/receipt.json`, sha-pinned
  `bcd23a2a…`; file re-hashed today and still matching). That baseline never contained
  `_DYNAMIC` (composition-1's delta ADDED it). Therefore dropping the reference is
  invisible to the gate: predicted removed set is EMPTY (see
  `build-receipts/template-run-1/predicted-undefined-delta.json`).
- Resolution: none required. Do NOT try to keep `_DYNAMIC` referenced "for compatibility".

## C4. Adapter duplication — REAL state conflict, resolved in the vendored tree (must apply)

- Concern: the spike tree contains composition-1-promoted adapter copies
  (`include/lmf_to_native_loader_facts.hpp`, `src/lmf_to_native_loader_facts.cpp`), but
  patch 0008's diff base is the worker-s1-adapter OUTPUT tree, not the spike copies. A
  controller who applied only patches 0001-0007 to the spike would compose an adapter
  WITHOUT the VmScan opt-in — silently.
- Facts: applying 0008 to a mirror of the s1-adapter tree succeeds cleanly (5+3 hunks);
  the patched adapter .hpp/.cpp are byte-identical to the worker's vendored
  `src/lmf_to_native_loader_facts.{hpp,cpp}` (diff-checked, shas `a5bc3b70fee0…` /
  `b6d13bf9da24…`).
- Resolution (in the vendored tree, and in the bundle): REPLACE both spike adapter copies
  with the patched versions. These are COPY-MANIFEST rows marked
  `replace-composition1-promoted`. Post-apply, the spike tree's adapter == worker-verified
  adapter. The 169-check suite was run against exactly this resolved configuration
  (adapter TU from the patched s1 mirror + patched spike headers first on the include
  path) and passes: `VENDORED_TREE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED`.
- Opt-in wiring note: with the patched adapter in place, scan-fed conversions remain
  refused unless the external caller passes
  `LmfConversionOptions{.allow_vm_scan_base_as_module_image_base = true}`; grep verified
  no composed runtime unit references it. No factory/supplier source change is needed or
  made.

## C5. Everything else — clean

Patches 0002-0005 (ops slot + core capture + SCE TU) and 0006/0007 (new base-scan TU)
apply cleanly to the current spike state; the patched results are byte-identical to the
worker's verified vendored copies (per-file equality in COPY-MANIFEST.json,
`worker_vendored_equal: true` for all 9 files).
