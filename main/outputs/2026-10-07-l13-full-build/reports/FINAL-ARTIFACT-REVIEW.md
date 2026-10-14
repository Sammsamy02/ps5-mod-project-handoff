# Final L13 artifact review

**PASS — local artifact review. No blocking concern found.**

Reviewed the completed fresh twins on 2026-10-07 by reading receipts, hashing artifacts and sources, and inspecting the final ELF dynamic symbol tables. No builds, gate reruns, console contact, uploads or launches were performed during this review.

## Twin identity

Every artifact below was re-hashed from disk, matched its own receipt and had the same hash/size in candidates 1 and 2. Both composition receipts declare `reused_composed_runtime_object: false`.

| Artifact | Bytes | SHA-256, identical in both twins |
| --- | ---: | --- |
| Final wrapped PRX | 30,907,159 | `cd58fa12c1864eb11f3f1eb72a66361c8f7468f9b349629e98261a3aac6a705f` |
| Bare original PRX | 30,898,520 | `486f03aee08d168ce9ca58b4c2358de6226d5a685aa70f69a2e886c48fe39726` |
| Bare SDK-stamped PRX | 30,898,520 | `b05fe42345789d5e71debce6857ffc2cb60eb12c451f51b57252e484f1e1fb4e` |
| Runtime object | 118,142,040 | `76f6472292b595fcf20a3cc6d1881223e8153718090f71634e8c07a6697d8bb5` |
| Combined supplier object | 118,267,784 | `06699a9ef5bc910b6ef1cdfe6aea230066457b812b4d38c1c3340e252b502035` |
| Bootstrap bridge object | 66,184 | `8a552b650117655921cce309c616657168389e220bb7b3048877a23471e49f53` |

All artifact-receipt paths resolve inside their respective candidate directory. The two package build receipts have no gaps. Accessor verification is PASS for both, with no inspection errors.

## Gate receipts

Each candidate's `verification-evidence/static-checks.json` records **20/20 PASS**, including exact final contract/import equality, SELF payload/bare identity, SDK stamp, lifecycle ownership and entry alignment.

Both final gate receipts record **11/11 PASS**, with IDs `W1 W2 W3 I1 D1 F1 S1 P1 M0 M1 I1+`:

- Candidate 1: `gate-receipt-l13-1-final.json`, GO, comparing candidate 1 against candidate 2 and using the archived pass-1 manifest.
- Candidate 2: `gate-receipt-l11-prx-candidate-l13-2.json`, GO, comparing candidate 2 against candidate 1.

The original `gate-receipt-l11-prx-candidate-l13-1.json` remains the historical provisional D1-only refusal from before the twin existed. Use the separate final candidate-1 receipt above. This retained provisional record is not an unresolved failure. B1/B2/B3/D2 are not among this lineage's executed gate IDs and are not claimed as passing.

## Exact import census

Inspected `.dynsym` of each final bare SDK ELF and decoded its import NIDs against its contract; each contains exactly **200** declared imports, with no unknown or missing names. Also inspected the frozen L12 final bare SDK ELF: it contains exactly **198** imports.

Both L13 twins differ from that actual L12 import set by exactly:

- Added `_openat`, encoded `bGVEgWXy6dg#B#C`.
- Added `sceKernelRead`, encoded `Cg4srZ6TKbU#B#C`.
- Removed: none.

All 198 prior contract identity rows retain the same module, library, versions, Soname and published name. Both new rows identify `libkernel.prx`. The manifest's separate delta against frozen r11 is exactly `_openat`, `sceKernelRead`, `sceKernelVirtualQuery`, `scePadGetHandle`, with no removals and 215 combined UND entries versus r11's 211.

## Manifest and source correspondence

Each archived finalized manifest matches the SHA in its finalization receipt:

- Pass 1: `130a37b21c85847938514320e1fe1cd9d27e29ad4907f0e7e1ff024692a20dcd`.
- Pass 2: `46fe6529b5b599a6a76a20466c0e175f24867086c2900261e0b84f6062d0c767`.

Their composition-receipt SHA, package-receipt SHA and combined-object SHA all match the corresponding current files. Both record zero finalization divergences.

For every declared changed unit (`program`, `lmf-loader-adapter`, `native-loadercontext`, `luamod`, `unreal-unrealinitializer`), the declared source path resolves to the run's consumed source and its actual source hash matches both the manifest and composition receipt. The staged bridge and module-entry source paths/hashes also match the corresponding archived overlay manifests.

For both passes, `PoolAdmissionEngine.hpp`, `CopyinSink.hpp` and `CopyinStartup.hpp` match their archived overlay source SHA, staged attested SHA, replacement-receipt SHA and current approved source bytes. These headers are fully evidenced in those records even though the manifest's inherited `changed_headers_staged` list still contains only its five historical entries.

## Preserved inputs

Current hashes match all **14 preserved-input pins** from the approved local gates, including the frozen L12 PRX and three pad source files; all **13 approved candidate source/header pins** also match. Additionally, the four historical pipeline pins in `receipts/pipeline-lineage.json`, the original package script and the original comp6-integration input manifest match their pre-build hashes.

The frozen L12 PRX remains `024a4143bb84333e4552814fd68e67f6f72f7b04630dcd2b4a512b44d8bc34b4`. Approved initializer and bridge source hashes remain `55eb2e19446e5a329880b2d5c0ab438b050da458acdd02d67655e855623f9a3e` and `8fb38c37fa5d9926e3079f93f1c67ac3e84ae72ac62bb70f637324e434f00ee6`.

This establishes deterministic local packaging and the declared static import boundary. Console loader resolution and in-game behavior remain outside this review.
