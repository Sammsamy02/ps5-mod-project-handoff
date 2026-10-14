# Native plugin activation continuation packaging

This recipe combines the reviewed native FName constructor ABI correction, native C++ child lifecycle/packaging work, canonical Container source reconciliation, and retained native C++ activation. Final assembly requires the explicit activation allowlist and accepts pinned composition/review files. This directory does not publish anything or update the project checkpoint.

The dependency is the prior native C++ loader handoff on `codex/runtime-handoff-2026-10-03`, commit `6b3cce37807adc9c75af19f138de764a814d13c6`, directory `handoff/native-cpp-loader-2026-10-03`, archive SHA-256 `267cbacf438a9c4a896c7801e1a6babe43e1846574f99d652befac1fcffa8061`. Its small publication receipt is retained in this continuation so the recipe can recheck that identity after extraction. Restore the prior archive and its earlier source handoffs too. This is a continuation, not a standalone SDK, source tree, or runtime binary.

## Included completed work

- The FName allowlist contains the four-file native explicit-output constructor correction, original bytes, focused native and legacy controls, and the consumer rebuild recipe. All 165 affected header consumers compiled: 164 runtime units and one bootstrap unit. This corrects the retained target ABI; it does not prove a game address or engine layout.
- The child allowlist contains child runtime source, packaging changes, lifecycle controls and compact local package verification receipts. It does not prove target child loading or gameplay.
- The Container sync receipt pins the two canonical files restored to the bytes already selected by the coherent runtime. That synchronization did not change the existing runtime.
- The root's `native-coherent-fname-runtime-1/compose.py`, its README, and the FName allowlist's `composition-review.json` preserve the composition recipe and compact audit. The 359-runtime-unit, three-bootstrap-unit relocatable object had SHA-256 `c69e6a9245d2a16ed4d4578662f26b342ca95979a981ffdead4f5630829782e4` (116,571,544 bytes). All 165 replacements occupied the original input positions; other units, fixed dependencies and all 197 undefined symbols were preserved. The object is excluded.
- The retained C++ activation allowlist connects Claude's activation reader to native mod selection, prepared module ownership, startup and retirement. Its 26 native compiles feed the root's activation composition: 360 runtime units and three bootstrap units, with all 197 undefined symbols unchanged. The resulting relocatable object was 117,135,608 bytes, SHA-256 `5428e255158bf9bcfdce97bb528b7ea721d5f3993751ad32e278c7b11235ebbf`; it is excluded. The root's compact report and the independent preservation review qualify this local result.

## Assembly

Run from the repository root with Python 3.10 or later:

```sh
python3 outputs/2026-10-03-native-plugin-activation/build_archive.py
```

This checks the pinned completed allowlists, each file's bytes and SHA-256, the exact Container receipt, the composition script, and the prior handoff identity. It writes only `build/preview/MANIFEST.json`, explicitly listing the missing activation input. It does not create an archive. Each run requires a new output directory; use `--output build/preview-2` for another preview.

Pass the root-supplied activation allowlist and final composition/review files by repository-relative path and exact hash. The frozen activation allowlist is `.superpowers/native-dependency-spike/native-retained-cppmods-1/source-backup-files.json`, SHA-256 `961becff2fc3e45d7b770689c3f387c7942ccaf10efe2b8d6ff1bf50a2f82058`. The final command has this form:

```sh
python3 outputs/2026-10-03-native-plugin-activation/build_archive.py \
  --activation-allowlist 'PATH/TO/export-files.json=SHA256' \
  --extra-file 'PATH/TO/compose.py=SHA256' \
  --extra-file 'PATH/TO/REPORT.md=SHA256' \
  --extra-file 'PATH/TO/composition-review.json=SHA256' \
  --final
```

`--extra-file` is repeatable. The final manifest retains the exact activation and extra-file arguments, and the activation allowlist itself is included. That allowlist must use a `files` array of `{path, bytes, sha256}` records. All paths must be normalized and repository-relative. A changed input, conflicting pin, symlink, binary extension, non-UTF-8 content, NUL byte, oversized expanded receipt, or capture/secret path is refused. Source bytes are retained exactly; no paths inside evidence files are rewritten. The recipe rechecks source hashes before assembly and verifies the zip's member list, CRCs and bytes afterward.

Final output is `build/final/{MANIFEST.json,ARCHIVE.json,native-plugin-activation-source.zip}`. The archive includes repository-relative source paths plus `NATIVE-PLUGIN-ACTIVATION-MANIFEST.json`. No `CHECKPOINT.json`, publication action, compiled object, PRX, library, executable, saved game capture, SDK binary, or expanded per-unit composition receipt is generated or bundled.

## Rebuild order and limits

Restore the dependent handoffs and reconstruct the coherent snapshot and fixed dependencies using their retained recipes. Rebuild the pad and native C++ loader bases before overlaying this continuation's changed canonical files; original bytes are also retained in the component baseline directories. Obtain fresh per-unit objects, dependency files and receipts. Then apply this continuation and run the included FName `rebuild_consumers.py --base-receipt ...` and the root's `native-coherent-fname-runtime-1/compose.py` against those fresh receipts. Follow with the retained native C++ `compile_consumers.py` and `native-coherent-cpp-activation-1/compose.py` in their documented order. Old composed objects cannot substitute for the per-unit inputs. The retained SDK, LLVM and .NET prerequisites remain external; historical receipt paths are evidence, not bundled artifacts.

This packaging step establishes a checked source continuation only. Production loader facts, live memory/hook capability, engine startup, target child lifecycle and a representative UE4SS mod remain separate runtime requirements. No game or console action is part of this recipe.
