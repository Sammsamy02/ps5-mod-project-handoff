# Reproduction

Use a new sibling experiment directory; completed receipts refuse overwrite. The frozen `cap10-mono-build` runtime/source/configuration and official `mono-host-research` MDK extraction must exist, with hashes matching `STATUS.md` and the JSON receipts. Copy only this folder's `tools/` into the new directory, and create `tests`, `evidence`, `logs`, `tmp`, `bin`, `owner-objects`, `aot-build`, and `owner-runtime/lib/mono/4.5` there. No global Mono installation is required.

From that new folder:

```sh
python3 tools/build_static_corlib.py
python3 tools/run_production_static_owner.py
```

The scripts infer sibling frozen input paths. The production owner must match the reviewed source hash. The build script copies the exact genuine corlib, uses the default single compilation worker, invokes the genuine compiler with `--aot=full,interp,static,asmonly,outfile=<owned>/aot-build/mscorlib.dll.s`, then assembles with `/usr/bin/clang -arch x86_64 -c`. The generated object is linked directly into the native executable.

The production test compiles the actual owner and boundary with real complete upstream headers/configuration, enables the static registration host macro, and links the actual host observer and generated corlib object. It selects the frozen native runtime dylib by an executable-local install-name change. It runs once with AOT/DLL and dyld-library traces and verifies the production result, explicit static-module selection, absence of dynamic corlib loading, assembly-prefix inventory, and unchanged frozen inputs. Command arrays, environment overrides, working directories, exit statuses, and hashes are in `evidence/PRODUCTION-STATIC-OWNER-CHECK.json` and `evidence/STATIC-CORLIB-BUILD.json`.

`tools/run_static_owner.py` and its generated harness are the preserved first proof using the earlier owner hash and harness-level registration. They are historical evidence, not the final production recipe. `--finalize-existing` on the AOT script was used only to finish receipt validation after correcting the symbol-definition parser; it never reruns the compiler or assembler. Do not use that switch for a fresh build.

Keep `STATUS.md`, this file, `tools`, `tests`, `snapshots`, `evidence`, and `logs` for a compact source/evidence handoff. Exclude generated `aot-build`, `bin`, `owner-runtime`, `owner-objects`, `production-owner-objects`, and `tmp`; their source inputs and identities are recorded. `evidence/HANDOFF-FILES.json` lists the small handoff payload. `evidence/FINAL-INTEGRITY.json` records the final read-only hash audit. No additional runtime execution is needed to review these receipts.
