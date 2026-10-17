# build_runtime.py bootstrap-replacement mechanism — PROPOSAL (controller decision)

Status: PROPOSAL-ONLY. `ue4ss-native-tick-file-owner-1/build_runtime.py` is a
pinned script this worker must not edit; the composition-prep precedent
(controller-apply bundle + hash-pinned attestation) is the pattern to follow.

## Problem

`build_runtime.py` line 43 reuses the composition bootstrap units verbatim:

```python
for u in base['bootstrap_units'].values():check(u['source'],u['source_sha256']);check(u['object'],u['object_sha256'])
```

Replacing the three bootstrap sources with the ack-enabled set (REPLACE-MANIFEST.json)
therefore fails with `Changed input:` before anything compiles. The affected-units
mechanism (lines 55-64) only covers the six hardcoded native units.

## Minimal mechanism (mirrors the existing affected-units recompile, bounded to bootstrap)

New arguments:

```
--replace-bootstrap-unit NAME=SOURCE_PATH   (repeatable; NAME in activated_mod_source|ue4ss_backend|runtime_owner)
--replace-receipt PATH   attestation JSON: {"files": {abs_path: sha256, ...}, "receipt": ..., "note": ...}
                          covering EVERY spike project file the replacement touches
                          (replaced .cpp/.hpp AND any header they include from pinned project dirs, e.g.
                           native-coherent-runtime-1/source/bootstrap/runtime_owner.hpp,
                           native-coherent-pad-runtime-1/build-1/source/NativePadInput.hpp unchanged->keep pin)
```

Semantics (all inside `main()`, after the base checks):

1. `for spec in a.replace_bootstrap_unit:` parse `name=src`; require
   `name in base['bootstrap_units']` and `Path(src).is_file()`.
2. For the unit's recorded `argv` (NOT the loader-context template — bootstrap
   units keep their own 66/69-include argvs): swap the `.cpp` path, retarget
   `-o`/`-MF` into the output dir as `bootstrap-<name>.o/.d`, `run('bootstrap-'+name, argv, env with the unit's LLVM_CONFIG)`.
3. Dependency gate: for every file in `deps(new .d)` that is under the spike
   project tree, require it in `pins` OR in `replace_receipt['files']` with a
   matching sha256; pin attested hashes into `pins`. Anything else unpinned
   stays a hard `ValueError` (same policy as extras).
4. Mutate the composition inputs: `selected[name] = {**old, 'source': str(src),
   'source_sha256': sha(src), 'object': str(target), 'object_sha256': sha(target),
   'dependency_file': str(dep), 'argv': argv, 'exit': 0, 'bootstrap_replaced': True}`;
   `replacements[old['object']] = str(target)`; append name to `fresh` so the
   receipt's `freshly_compiled_units` grows (364 -> 366/367 for the ack set).
5. Require `--replace-receipt` whenever `--replace-bootstrap-unit` is given
   (same shape as the extras' mandatory attestation).
6. No relaxation of the undefined-delta gate: with the ack set the expected
   delta is `added == []` (every new reference resolves in-closure; measured).
   Any non-empty `added` keeps the current refuse/`--extend-undefined-baseline`
   behavior unchanged.

## Expected composition-3 invocation (for the record)

```
python3 .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py \
  --output .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-3/run-1 \
  --rust-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json \
  --host-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json \
  --extra-unit lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp \
  --extra-unit lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp \
  --extra-unit lmf-loader-adapter=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp \
  --extra-unit lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp \
  --extra-define LMF_DISABLE_DL_RETENTION \
  --extra-receipt outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/build-receipts/run-1/receipt.json \
  --extend-undefined-baseline \
  --replace-bootstrap-unit runtime_owner=<prep>/bundle/.../runtime_owner.cpp \
  --replace-bootstrap-unit ue4ss_backend=<prep>/bundle/.../ue4ss_backend.cpp \
  --replace-receipt <prep>/replace-receipt.json
```

(activated_mod_source is NOT replaced; the hpp files ship inside the
replace-receipt because the replacement .cpps include them. The exact
destination-relative files and their hashes are in REPLACE-MANIFEST.json.)
