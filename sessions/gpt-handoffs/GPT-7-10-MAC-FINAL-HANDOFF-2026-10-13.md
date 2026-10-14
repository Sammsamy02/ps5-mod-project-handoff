# Claude handoff: #7 step 2 and minimal #10

**The isolated Mac component work and review are complete. Claude owns the remaining merge, combined mod-loader build/package and PS5 test.** No PS5 contact, main-tree change, frozen-build edit or PRX build/deployment occurred. #1/#3 remain ZCode's work. #10 must not hold ready capabilities or Stellar Blade.

Working source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/`. The accompanying `GPT-7-10-MAC-FINAL-PACKET-2026-10-13.zip` contains the reviewed sources, patches, selected evidence, reproduction tools and the actual target Mono archive, corlib object and matching IL. Its adjacent manifest records every packaged file's full SHA256. Large bootstrap source/build trees and link-only executables remain in the isolated worktree; their exact provenance and reproduction commands are included.

| Item | Delivered and verified | Remaining for Claude |
|---|---|---|
| #7 step 2 | Complete 15-file runtime/source/build/packaging stage; patch forward/reverse exact; 18 focused integration tests; existing runtime review and regressions retained | Compose with current metadata, full rebuild of all changed cache-layout consumers, one console mount proof |
| #10 owner | Genuine 3,584-byte C# Probe; actual production owner runs twice, catches the exact deliberate exception, recovers and cleans up on real Mono | Real game callback/GC/TLS/EH and native-call observation on PS5 |
| #10 loader | Genuine patched Mono built; seven actual loader tests pass, with unpatched failures preserved | Final package/import checks in the combined game build |
| #10 target runtime | Real source-built Mono static archive; all eight native core build stages pass | Integrate it into the mod loader; target behavior remains unproved |
| #10 target corlib | Genuine static x86-64 ELF AOT object; matching IL; all 369 layout constants agree with final runtime config | Link and register it, package matching IL/config under the actual console runtime prefix |
| #10 combined component link | Actual owner and boundary compile for target with warnings as errors and link to genuine Mono + corlib with no unresolved Mono/SGen symbols | Claude's combined PRX build/package and console execution |

## #7: use the complete reviewed stage

Read `cap07-step2/BUILD-INTEGRATION.md`, `REVIEW-CLOSURE.md` and `LAUNCH-CARD.md`. Use **`cap07-step2/derived/review-final/integration.patch`**, SHA256 `9239ed7f9251eb9d71a436a92365aef9e21f15abe0c3dfad34fdc108f8f0897d`. It includes all 15 files: eight changes and seven additions. Older `build-closure-final` patches are superseded; the reviewer fixed their missing additions and malformed EOF handling.

Keep Claude's current metadata. While this review ran, `typescan-r4/CHANGES.json` moved from `f875963b…` to `66b8b180…` for Claude's `NativeModHostCore.hpp` unused-include removal. The exact two-line metadata delta is recorded in `evidence/REVIEW-LIVE-METADATA-DRIFT.diff`; the other seven original inputs still matched at that audit. Merge the #7 metadata changes into the current JSON, then deliberately refresh the reviewed input pin. Do not overwrite the current JSON with the frozen stage or bypass the drift refusal.

The CodeImageCache layout change requires a full consumer rebuild. The reviewed stage registers five NEW_UNITS in total and a 396-input baseline census. These numbers describe that stage; recompute them after adding #10 or later Claude units.

## #10: integrate the minimal Probe

Read **`cap10/MAC-INTEGRATION-HANDOFF.md`** for the concrete source, header, prefix, link and packaging requirements. Supporting reports are `cap10/REVIEW-CLOSURE.md`, `cap10-port-closure/`, `cap10-cross-aot/CROSS-AOT-REPORT.md`, and `cap10-static-corlib-host/STATUS.md`.

Use the current three-anchor `native/current-owner.patch`, not an earlier bridge-only patch. Its UE4SSProgram input remains `44499a0a…`; recheck when merging. Register `MonoOwner.cpp` and `InvocationBoundary.cpp` as two real, pinned NEW_UNITS. Use genuine configured Mono headers and generated config. The production callback patch supplies the observer/provider; **do not include the link-only test fixtures**.

Link these actual artifacts exactly once, after the owner/boundary objects:

| Artifact | Bytes | SHA256 prefix |
|---|---:|---|
| `cap10-cross-aot/artifacts/mscorlib.full-interp.ps5.o` | 35,758,480 | `a9f0a8564ac09739…` |
| Packet: `cap10-port-closure/handoff/lib/libmonosgen-2.0.a` | 30,553,848 | `38d0342104fe5c77…` |
| Required matching `cap10-cross-aot/managed-input/mscorlib.dll` | 4,634,112 | `86364b7803c92d8c…` |

The SDK's Mono `.so` is only an import stub. The supplied archive is the actual runtime. The owner now registers the linked static corlib before initializing Mono; it passes the generated symbol's value, not its address. The module must remain linked throughout the runtime lifetime. Package the matching IL at `lib/mono/4.5/mscorlib.dll` and target-generated configuration at `etc/mono/config`. Compile the owner with the actual console **lib** and **etc** prefixes; the Mac fixture paths are not deployment paths.

The packet includes the 24 required genuine Mono headers/configs under `cap10-port-closure/handoff/runtime-headers/` and configuration under `handoff/runtime-config/mono/config`. Their mapping to local build inputs is hash-bound in the port manifest. The public SDK/libc++ installation remains external.

Extend both build stages' pinned inputs, dependency closure and census for two new units plus the two runtime artifacts. Also extend the raw-import and final provider/NID contracts: the current packaging path still assumes the old 207 final identities. The standalone test has 289 imports from public SDK providers; this is **not** a ready-made allowlist for the combined PRX. Derive and validate that build's real delta. Preserve all existing build/package verification checks.

The target compile/link-only fixture resolves Mono, SGen, registration, invocation and rooting symbols locally. The public compiler uses PIC; the archive and AOT object have no allocated 32-bit absolute relocations, and the linked PIE has no TEXTREL. Absolute 64-bit data pointers still require correct loader relocation. The public default linker produced ET_DYN/PIE with entry zero; the fixture is evidence only and must not be deployed. Shared/PRX packaging has not been tested by GPT.

## Acceptance and scope

Compose the ready changes, complete the existing checks, then make the one planned combined build and test. Claude alone contacts the console. #7's acceptance is in its launch card. #10's Townfall acceptance is real callback admission, interpreter startup, exact Probe identity, both native exports, two distinct nonzero cookies, successful Run → exact exception → recovery and cleanup. Host observers are not evidence for the real game admission path.

The minimal host Probe never requests System.Native; that larger native PAL is not required by its observed path. Broader managed APIs and Unity remain later work. Process/module diagnostics use upstream file-based fallbacks and are unsupported/unproved on PS5; their empty result does not prove support. The target AOT compiled 35,820/35,828 methods, with skipped-method evidence retained. Do not describe this as universal C# compatibility or a console pass.

The review packet has no outstanding demonstrated component compile/link defect. The remaining integration and target tests above are deliberately Claude's work, matching the agreed ownership boundary.
