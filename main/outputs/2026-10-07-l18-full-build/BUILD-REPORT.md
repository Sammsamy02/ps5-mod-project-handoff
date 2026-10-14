**l18 cleanup + XOM code verification: local build complete; upload HOLD**

Two fresh launch11-based compositions and complete packages finished on the Mac.
Both wrapped PRXs and both composed objects are byte-identical.
The final offline gate passed 11/11 checks; each package passed 20/20 static checks.
This receipt establishes local build results. Console execution remains unproved.

The reviewable artifact is
[pass 2 wrapped PRX](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/prx-candidate-l18-2/modbridge_supplier_composition.prx).
It is **30,959,143 bytes**, magic `4f153d1d`, SDK `0x11000000`.
Its SHA256 is:

```
444e3ff5d65a76dec046b856f76b44274bcc5f917865603bd77ec37f652c4c4a
```

Both composed objects are 118,212,864 bytes, SHA256:

```
07da124c200bfe69087fc899ed17ed523d7836956d7b014a12d37839db326e1f
```

[FINAL-RECEIPT.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/FINAL-RECEIPT.json) pins the source, approval, package,
static checks, stable per-pass manifests, final gate and independent artifact proof.
It includes the matching bare-original, bare-SDK, bridge and combined-object hashes.
[Parent artifact inspection](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/parent-review/ARTIFACT-INSPECTION.json) independently
checked both wrapped hashes, size, embedded ELF, magic and SDK.

The parent reviewed and approved the frozen 26-file source/header selection before
execution. The private copies retain the original bytes and include runtime_owner,
initializer, bridge, CopyinStartup and the code-cache/signature helper topology.
Original source-selection SHA256:
`9d6266d35811a6ad9922e4707de99143b705ed4e68d8b4f364d59506ee3301ea`.
Private bound-selection SHA256:
`9ab7946d7dc23572043421ff120496440512dc7b3807c15fcd7e368d03e904c0`.
[Bound pins](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/parent-review/BOUND-PINS.json) record the parent's 26/26 matching copies.
Each full run verified all **369 selected runtime units** and **386 composition
inputs**, including source/object hashes, link order, critical consumer references
and actual helper dependency paths. These are complete selection checks, not only
focused translation-unit checks.

The bridge supplies strong definitions for the three l17 lifecycle providers and:

- `bridge_code_cache_context(uint64_t,const rc::codecache::Metadata**)`
- `bridge_code_cache_signature(uint64_t,uint64_t,const uint8_t*,size_t)`

Preproof, consumer relocations, combined-object resolution and final-import checks
passed. These providers resolve internally; they are not SDK imports.
Detailed evidence remains in the archived
[pass 2 whole-selection receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-2/whole-selected-prx-candidate-l18-2.json),
[provider preproof](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-2/driver/l18-provider-preproof.json),
[accessor check](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-2/accessor-verification-prx-candidate-l18-2.json)
and [ODR receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-2/odr-l11.json).

The final linked import census is **206 exact strong identities**, with no extra,
missing, malformed or unknown-NID imports. The bootstrap addition `sceKernelUnlink`
uses the exact `libkernel` identity, module version 257, library version 1,
`libkernel.prx`, donor `sceKernelOpen`, and NID `AUXVxWeJU-A`.
The installed SDK stub, NID and strict mapping checks passed locally;
target provider resolution is unproved. Six weak requirements are contract entries;
that count is not asserted as a final linked weak-import census.

| Preserved pass | Package checks | Offline gate | Stable final manifest |
| --- | --- | --- | --- |
| 1 | 20/20 | D1-only before a twin existed | [archive 1](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-1/change-manifest.finalized.json) |
| 2 | 20/20 | 11/11 GO | [archive 2](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/prx-candidate-l18-2/change-manifest.finalized.json) |

The first full attempt completed pass 1 composition/package, then stopped in the
private copied finalizer because its inherited self-filename/read roots and
baseline location did not match the isolated layout.
The [path repair](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/private-finalizer-path-repair.json) changed local tooling
paths only. Its [first-failure log](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/logs/full-pipeline.log) remains intact.
The first resume was refused by the finalizer's reserved `PENDING` marker in
manifest prose. The [prose repair](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/receipts/finalizer-marker-prose-repair.json)
changed “evidence pending” to “evidence remains unproved”; its refusal and
[resume log](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/logs/resume-pipeline.log) remain intact.
The final [resume log](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/logs/resume-pipeline-2.log) records successful pass 1
postbuild, a fresh complete pass 2 and the matching composition/package results.
Neither repair changed the frozen 26 runtime files or the first candidate bytes.
Per-pass finalization receipts are archived beside their manifests, so pass 1
proofs remain stable after pass 2 regenerated the private integration root.

The [XOM audit](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/inputs/XOM-AUDIT-REPORT.md) was reported before the full build.
Its receipt, including the settings addendum, is pinned at
`d3054903f2c5842a43c73498bc542b7024c3e40662d56666ee5a56fe9fc125ba`.
The [settings addendum](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/inputs/SETTINGS-PROVENANCE-ADDENDUM.md) records saved package
provenance: 1,448 bytes, SHA256
`d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038`,
with `HookProcessInternal=0` and `HookProcessLocalScriptFunction=0`.
Exact current console configuration content/hash remains unproved; the fresh l16
loadlist declared 1,947 bytes. Before a separately approved launch, verify both
flags are 0 through a read-only check. No settings edit was made or authorized.

Inherited launch11 PLACEHOLDER comments, gate names/dates and old scope descriptions
are historical provenance. Current admission is the frozen l18 selection in
[EDITIONS-l18.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/EDITIONS-l18.json), with the retained pad/LuaMod baseline admitted.
The original prep receipt and file inventory remain historical snapshots;
[PREP-MANIFEST.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/PREP-MANIFEST.json) now records completed local execution.

No console/network/account contact or upload occurred. Historical source/receipts,
shared pipeline/SDK, original mods and configuration were not edited by this build.
Readable live vtables, exact writable UFunction::Func slots, absent inline hooks,
engine ABI/thread/native behavior and original ItemSpawner menu/spawn remain
unproved. PC reference evidence is absent; no PC parity is asserted.
Upload and launch approval remain **HOLD**.
