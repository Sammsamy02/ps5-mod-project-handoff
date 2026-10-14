# L13 isolated pipeline audit

Initial read-only audit on 2026-10-07 of the L12 runner, driver, composer, packaging script, receipt generators, finalizer and gate-v2 implementation. During that initial audit this report was the only file written; no build or source edit was performed. A later delegated task implemented only the isolated package-script copy and focused checks, recorded in `outputs/2026-10-07-l13-full-build/reports/PACKAGE-PRX-L13-REPORT.md`. No console contact, upload or launch was performed by this worker.

Root: `/WORKSPACE/.codex/worktrees/translation-core/what`.

Candidate: `outputs/2026-10-07-l13-local-admission`.

The worktree AGENTS.md, outage POSTMORTEM.md and PS5-MODDING-MISSION.md were read. Existing lineage sources and receipts can remain read-only; every generated L13 file should live under the candidate's new build subtree. All 13 source hashes in candidate `receipts/LATEST-GATES.json` matched their files when inspected.

## Existing execution chain

1. `launch11-combined/launch12_one_pass.py` checks `EDITIONS-l12.json` units, admits pad/initializer, regenerates receipts and calls the driver twice.
2. `build_runtime_composition6_l12.py` checks composer/package hashes, ODR, receipt coverage, edited sources and entry; invokes the fixed composer; verifies startup symbols; stages the packaging overlay; invokes the package module with its PIN and bootstrap-unit adjustments.
3. `build_runtime_composition6_fixed_l11.py` reads frozen inputs; compiles affected, retained replacement, extra and bootstrap replacement units; partial-links a fresh runtime object; measures raw undefined delta; proves SDK or declared later-provider coverage; rechecks pins; records receipts.
4. `worker-prx-link-gaps/package_prx.py` verifies lineage; compiles bootstrap and typed import shim; links `combined-supplier.o`; exactly matches its unresolved set to the derived contract; builds the copied packager harness; creates bare-original, SDK-stamped bare and fake-SELF artifacts; checks the final artifact.
5. Runner performs LuaMod object comparator, marshal-consumer UND resolution, ODR emitter, manifest finalization, bridge accessor definition/resolution/artifact tying, gate-v2 and twin determinism.

## Minimum copied code and path redirects

Copy the runner, driver, fixed composer and package script to a new candidate build subtree. Copy `freshen-deps.txt` and the change-manifest/edition data there. The receipt generators, finalizer, ODR checker and gate-v2 can be invoked at their existing paths with explicit candidate-owned output arguments; none needs an executable edit solely for isolation.

| Location in original | Required redirect |
| --- | --- |
| Runner `MY`, `DRIVER`, edition filename (lines 28-30) | Candidate build subtree and copied driver/edition |
| Runner `INT` (line 34) | New candidate manifest directory; never original comp6-integration |
| Runner `COMP_RUNS` (line 39) | Candidate-owned composition run parent |
| Runner `regen_receipts` paths/names | New L13 replace/extra receipts, with candidate source pins |
| Runner `the_pass` overlay move-aside (line 151) | Candidate-owned driver staging; must not rename worker-comp6-driver overlay |
| Runner `run-l12-*`, candidate names, compare directory | L13 pass-1/pass-2 paths |
| Runner initializer receipt merge/SeedValidation stage argument | Candidate units, including new headers |
| Driver `MY` (line 87) | Candidate-owned staging root; this controls logs, chain-symbols, overlay, overlay-manifest and ledger mirror |
| Driver `COMPOSE6_FIXED`/SHA (90/94) | Copied, reviewed L13 composer and measured SHA |
| Driver `PKG_FIXED`/SHA (95/96) | Copied, reviewed L13 package script and measured SHA |
| Driver initializer/shadow-copy compose arguments (400/364) | Candidate `units/UnrealInitializer.cpp` and `units/shadow_copy.cpp` |
| Driver preproof `_overlay` (707) | Actual `overlay` argument; it currently hardcodes old worker-comp6-driver staging |
| Driver optional/default `RUN6` | Candidate run, or require explicit `--composition-run` for every full pass |
| Package script | Its `BASE` follows its copy; `_find_root()` walks up at most six levels, so keep copied script close enough to the real worktree root |

Keep `ROOT`, `TEAM`, composer `W/S/ROOT`, frozen baseline paths, LLVM/SDK paths, inherited source paths and real ledger **read locations** unchanged. Changing the composer's W would redirect source discovery and frozen dependency proofs, not merely output. The driver reads the real ledger and writes only its mirror at `MY/transport-ledger.composition6-mirror.json`.

Use one independently retained overlay/overlay-manifest, driver log directory and finalized manifest per pass. Otherwise the old runner's second pass overwrites `luamod-comparator.json`, `consumer-und-resolution.json`, `odr-l11.json`, driver logs and finalized manifest. Moving an old overlay aside also makes candidate-1's recorded overlay path refer to pass-2 contents. Exact per-pass paths avoid that evidence ambiguity.

`post_build` currently picks the latest mtime under `run-l12-*`; use the exact run returned or supplied for the current pass. Run selection by glob can pick an aside directory or unrelated attempt. Build into fresh absent directories, with `reused_composed_runtime_object == false`.

## Source and script pins

Measured historical inputs:

| File | SHA-256 |
| --- | --- |
| launch12_one_pass.py | `f6925267db77df075f93689b312139c038dcc99113e803b78fa17b9fb4ca87fa` |
| build_runtime_composition6_l12.py | `4e5d0acd5b366ca8e13973d7c67d673088bf5183b9e1a1e8ebdd41826be7315f` |
| build_runtime_composition6_fixed_l11.py | `c42dc229b31bc5501cb1b1218cc97b8092f776d17c90facd532cd56c4d86ea08` |
| worker-prx-link-gaps/package_prx.py | `921d8397957df6fe39e19ca010fdcb6ba469ba0c6e80ef458ea41ad49f08be64` |
| EDITIONS-l12.json | `dbea2857326e78a19aa01c31efea931542c8ff6b6e11993878db7e778a9b8706` |
| freshen-deps.txt | `799591111d0bed49982d5aeac92340766cfd98351bf20551af80a505c71a3290` |
| comp6-integration/change-manifest.json | `0a641c6c51e94481772dc751e9a867d86ef49220a2a90a4ea4bdbc957919359b` |
| worker-manifest-finalizer/finalize_manifest.py | `48654a7b0f88d941a37db80cee33ea958c5aebc44ea2a533caceeccc879ec1b0` |

Candidate input pins:

| Candidate file | SHA-256 |
| --- | --- |
| receipts/LATEST-GATES.json | `0609c75cffa7a6f1a7f8eaae17e47dbcaf2c2d98fb129028a8a48422003626f9` |
| receipts/l13-import-extension.json | `7aebba0bec9b7fa60471e49f83a2013157504ecad3ebc653e2afd369d604ccd7` |
| units/UnrealInitializer.cpp | `55eb2e19446e5a329880b2d5c0ab438b050da458acdd02d67655e855623f9a3e` |
| units/runtime_start_bridge_r14_diag_spanpub.cpp | `8fb38c37fa5d9926e3079f93f1c67ac3e84ae72ac62bb70f637324e434f00ee6` |
| units/PoolAdmissionEngine.hpp | `29e20c70a3c2db09cf4707e278ffa99f7a0670a3108dc614855ad6e24dd98526` |
| units/CopyinSink.hpp | `2018c9bdc63b8f2001aeadcc44ab698d0d5c54c08652e5fc0c78f45c0fa65d98` |
| units/CopyinStartup.hpp | `f335316849add3381deefe4b5f1169dc5350b5a39fc4fd983ac6c1fde309eb3c` |

The full unchanged candidate header/shadow pins are in LATEST-GATES.json. The frozen L12 final PRX is `024a4143bb84333e4552814fd68e67f6f72f7b04630dcd2b4a512b44d8bc34b4`. The inherited LuaMod object comparator remains `457ca039e51074c22bfe0519ebb1b555f6aa52666fde6301be5d2daa300a644b`; source remains `8bd12c623bab4e35203e4faa13fb829b141a46a586a0426b47eaa32cb12f75b8`.

EDITIONS-l12 contains historical narrative/hash fields that disagree with its current `units` and actual runtime settings. The executable runner verifies `units`, `bridge_cli`, admissions and the comparator; do not treat stale narrative fields as live pins. New edition data should accurately describe copied files and measured current pins, while recording historical inputs separately.

## Receipt coverage and header staging

Generate fresh replace/extra receipts into the build subtree, preserving the existing inherited `freshen-deps.txt` admissions and include-dir-map behavior. Add all active candidate unit/header files to replace coverage, especially `PoolAdmissionEngine.hpp`; the affected initializer's fresh `.d` gate otherwise rejects it as unpinned. Keep the `.l11-original` archival file out of the compile/stage set.

Add candidate shadow-copy source/header to extra-receipt evidence as appropriate. Caveat: the fixed composer requires the extra-receipt argument and records its hash but does **not** load its `files` map; enforced candidate dependency pins come from replace-receipt pre-seeding and later final pin rechecks. Therefore extra receipt alone does not replace required replace coverage.

The bootstrap bridge is copied to `overlay/lifecycle/runtime_start_bridge.cpp`; its original neighboring headers stop being discoverable there. Explicit hash-pinned `--stage-header` additions are required for `CopyinSink.hpp` and `CopyinStartup.hpp`, and candidate `SeedValidation.hpp` should replace the historical staging argument. `PoolAdmissionEngine.hpp` is directly included only by the initializer and does not need overlay staging for the current bridge, though it still requires replace-receipt coverage. Existing bridge-closure default headers remain in force.

The bridge preproof compiler uses the staged overlay include roots, so these headers must be staged before preproof. Redirecting only the bridge source does not suffice.

## Stage-specific symbol and import admission

### Pre-bootstrap composed runtime object

New initializer references resolve later from the bootstrap bridge:

- `bridge_kernel_write_copyin_probe_until`
- `bridge_copyin_range_faulted`

Add exactly these to copied fixed composer's `linker_provided` set (original line 283). Preserve raw `undefined-delta.added`; record them in `expected_bootstrap_resolutions` and provider evidence. Keep all existing added/removed/binding and pinned SDK checks.

Add the same two names to the copied driver's stage-aware reconciliation whitelist (original line 695). Its expected added set remains the historical runtime checkpoint `{sceKernelGetModuleInfo, sceKernelVirtualQuery}` union the documented later-resolution symbols. Neither `_openat` nor `sceKernelRead` belongs in this pre-bootstrap checkpoint.

Compile the candidate bridge against the actual staged overlay and require both new names, plus the existing requested bridge-provider names, to be DEFINED by that bridge object. Preserve the separate diagnostic.c provider proof. Extend runner accessor verification beyond the old three span accessors to include both new bridge names; checking every expected bridge provider is preferable. Require no such internal bridge names in combined unresolved symbols or decoded final PRX imports.

### Bootstrap / combined object / final PRX

The new bootstrap bridge calls `_openat` and `sceKernelRead`. Candidate `l13-import-extension.json` provides exact libkernel identity rows and pins `target/lib/libkernel.so` to `c6579f2614d8b8f905a673f810a1b562f1432b07397d61c11e876b5648428753`. Its base L12 contract SHA is `649b90ce5200203a61f64234dfaef62f1d291a7a619b5eddfc01d069d7802ccc`.

The copied package script must include these names in its total extension set for typed shim generation, exact combined unresolved-set matching, derived contract, packager count changes and final decoded-NID import census. Clone libkernel identity from `sceKernelOpen` and verify against the explicit candidate rows and pinned provider.

Simply adding them to existing `EXTENDED_IMPORTS` breaks two valid pre-bootstrap gates:

- `undefined_delta_covers_extended_imports` (original 387-389)
- `extended_imports_undefined_in_composed_object` (440-442)

Retain the original runtime extension set for those runtime-object checks; use a separately explicit bootstrap-only set for the two additions. Prove that bootstrap-only set absent from the runtime object, present as strong UND in the freshly compiled bridge, and declared by the pinned SDK provider. Continue exact combined-object and final-PRX equality against the **full** contract. Do not clear those check failures generically or weaken equality to subset checks. Update derived evidence text so it attributes the two new imports to bootstrap, rather than incorrectly calling them runtime-object imports.

The existing final static `imports_resolve_to_sdk_or_linker` already permits declared extension imports outside the runtime baseline; keeping them in the full extension set supports this check.

## Manifest and gate expectations

Copy the original manifest, then declare the candidate initializer, shadow-copy, bridge and new candidate headers using their consumed paths/pins. Keep the inherited program/LuaMod/adapter/context provenance. Use existing read-only finalizer with explicit per-pass `--out`, `--json` and `--overlay-manifest` arguments. No original comp6-integration file should be written.

Declare expected combined import delta before the finalizer measures it:

| Comparison | Added | Removed |
| --- | --- | --- |
| L13 vs frozen gate r11 | `_openat`, `sceKernelRead`, `sceKernelVirtualQuery`, `scePadGetHandle` | none |
| L13 vs frozen L12 candidate-2 | `_openat`, `sceKernelRead` | none |

Frozen r11 has 196 contract names and 211 combined UND entries. L12 has 198 strong contract imports and 213 combined UND. Therefore expected L13 totals are 200 strong contract imports and 215 combined UND, subject to exact fresh measurement. Internal bridge additions should disappear before this comparison.

The finalizer does not make declared/consumed path divergence fatal by itself; it records it. Prefer consumed paths in the L13 manifest and inspect `finalization.divergences` explicitly. Its existing defaults write finalized output beside input, so passing old `INT` would mutate history.

The actual successful L12 gate receipt runs eleven checks: W1, W2, W3, I1, D1, F1, S1, P1, M0, M1, I1+. Its manifest has no `resolution_b` or `d2` section, so B1/B2/B3/D2 do not run. Preserve the actual existing gates and do not report those absent checks as passing. If additional B2 coverage is intentionally activated, the bridge callee delta must be separately declared exactly against its own baseline.

The existing runner permits candidate-1's sole D1 failure until twin-2 exists; after both builds, rerunning gate-v2 against candidate-1 with compare-dir candidate-2 yields a complete durable GO receipt for each pass. D1 compares PRX artifacts; runner separately compares fresh runtime-object SHA. Retain both, and compare combined objects/bridge objects/contract identities as supporting evidence.

`--verify-only` verifies an existing RUN4 chain by default. It provides useful preflight but cannot prove L13 new bridge resolution or fresh composition. Only the new composition, staged bridge preproof, packaging, finalizer, final import census and twin comparison supply that evidence.

## Completion evidence expected from the build owner

- Two fresh composition receipts with candidate paths and no reused composed object.
- Two successful package build receipts/static-check receipts plus explicit artifact SHA/size ties.
- Per-pass LuaMod comparator, marshal consumer UND, ODR, bridge provider/combined/final import evidence.
- Per-pass finalized manifests and receipts, gate-v2 receipts, runtime-object and PRX determinism proof.
- Exact L13-vs-L12 decoded import census with only the two SDK-backed additions.
- Before/after preservation hashes for frozen L12 sources/artifacts/receipts, original pipeline scripts and original comp6-integration manifest files.
- Local-only result; target resolution and console behavior remain untested by this build.
