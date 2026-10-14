# AI handoff for remaining capabilities

Updated October 13, 2026. The user explicitly stopped work and requested this handoff for another AI. Implementation and research workers have stopped; both independent reviews were interrupted. **Do not resume implementation or console activity until the user resumes it. The requested project is unfinished.**

The latest goal was to fully complete **#7 step 2, #1/#3 and #10**. The user confirms this is their own system. All source stages below are unapplied. No target build, console contact or changes to the integrator's candidate/game/mod files occurred in this continuation. The goal API reported no registered goal when pausing was attempted, so a tool-confirmed paused status is unavailable; work was stopped directly.

## Locations and starting point

- Integrator repository: `/WORKSPACE/.codex/worktrees/translation-core/what`
- New isolated worktree: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`
- Branch: `codex/caps-resume-20261013`, starting commit `53e1e9bdc5a5788621c0b8096a73cc321f0c98a8`.
- New work: `caps-team-work/` beneath that isolated worktree. Changes are uncommitted. `IMPLEMENTATION-PLAN.md` and `PROGRESS.md` record the initial plan and intermediate state; this handoff supersedes their older status wording.
- Previous full capabilities preparation: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-12-caps-prep-chatgpt`
- User deliveries: `/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs`

Read the integrator's current `AGENTS.md`, `CLAUDE.md`, mission and outage rules before any resumed work. Main/team source is changing independently. Preserve B7 and Claude's build/launch path; #10 alone never holds Stellar Blade. The latest user pasted status described other capabilities as progressing in Claude's teams; do not reset them to the older handoff's status.

## Current state

| Item | Delivered in this continuation | Unfinished |
|---|---|---|
| #7 step 2 | Real engine adapter, request producer, synchronized snapshots, call validation, priority checks, source stage and focused host tests | Independent review, complete build/source selection, target compilation and actual launch proof |
| #1/#3 | Exact source/route findings and recovery/acceptance requirements | No kernel client or operational kernel proof delivered; provider route and identity remain unresolved |
| #10 | Actual compiled C# plugin; actual CoreCLR and matching Mono helper runs; production owner/bridge/loader source stages | Independent review; strict production loader execution; target Mono port/dependencies; integration and in-game proof |

## Number 7 step 2

Component: `caps-team-work/cap07-step2/`. Start with `NOTES.md`, `PATCH/APPLY.md`, `LAUNCH-CARD.md`, `SOURCE-RECEIPT.json` and `evidence/stage-recipe.json`. `REVIEW.md` explicitly says independent review pending; `HISTORICAL-REVIEW.md` is the original Team A review, not approval of this new implementation.

New source in `PATCH/` includes `RuntimePakMount.cpp/.hpp`, `NativePakMountAdapter.inl`, `PakRequestProducer.hpp`, `NativePakMountEntry.hpp`, generated pins/profile and `current-source.patch`.

The adapter uses actual admitted bounded reads, code-cache signatures, engine FString allocation/destruction and the already-bound MountPak delegate. The automatic producer discovers up to 16 unchanged regular pak containers under the configured native root's `pakmods` folder; the game-thread pump triggers once. It does not replace this feature with the step 1 file installer.

The mounted count correction is +0x18, not pending-encrypted +0x28. Production reads use the engine's synchronized GetCurrentlyMountedPaks delegate, with captured aggregate-return ABI and engine allocation cleanup. It verifies receiver/member/adjust/vtable/Execute identities, explicit order plus patch bonus overflow, and the returned identity/order in a post-call snapshot. Post-call uncertainty never claims vanilla or safe retry. Later/concurrent mounts can still change precedence.

Saved host evidence: 23 legacy + 11 regression + 10 adapter-fixture C++ assertions; 4 producer + 4 stager + 2 source-contract Python checks. Compiler-failure propagation exits nonzero. Red logs and final passing logs are retained. Saved-image replay binds six prefixes and ten windows/8,235 selected bytes against the full image hash. These are host/source evidence, not target execution.

`tools/stage_current.py` creates a new derived tree, validates four exact original SHA/size/anchor/inverse contracts and adds seven component files. `derived/final/` is the final generated tree; `derived/current/` is older and must not be preferred. The final recipe preserved Claude's new diagnostic changes to LuaMod. Integration targets are:

- `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
- `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`
- `outputs/2026-10-08-l26-runtime/profile/units/CodeSignatureProfile.hpp`
- `outputs/2026-10-08-l26-runtime/profile/units/CodeImageCache.hpp`

The final LuaMod baseline is SHA256 `fd53a4f295390677e47288cbf80df40b623aa32be47a1b6d4546299b04d1599c`; source drift must refuse rather than overwrite the integrator's work.

**Unfinished review findings:** the reviewer was interrupted before a final verdict. It identified a likely important integration hold: kMaxPins grows from 8 to 16, changing shared CodeImageCache metadata layout, but the stager updates only the profile copy. Every actual compiled provider/consumer copy must match; prose alone does not prove source selection. It also noted missing regression coverage for signed boundaries, numbered patch bonuses/overflow, failed post-call snapshot and wrapper descent. These are provisional review observations, not resolved findings or review approval.

The aggregate snapshot declaration is absent locally; its ABI is corroborated by saved instructions, not compilation of the genuine engine type. External-root reachability, allocator/delegate lifetime, real ABI execution, pristine game baseline and the launch remain unproved. Claude's current step 1 install means the step 2 baseline must be restored and independently verified before a meaningful test, under separate console authorization.

## Number 10

Component: `caps-team-work/cap10/`.

`managed/Probe.dll` is a real 3,584-byte Roslyn compilation from `managed/Probe.cs`, SHA256 `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d`. It references only mscorlib and was compiled against the exact Mono-selected .NET Framework 4.7.2 reference, not .NET 10 corelib. `evidence/COMPILE.json` binds source/compiler/reference/artifact and metadata results.

Two distinct actual host results exist:

- `evidence/HOST-CHECK.json`: the actual plugin executed on Mac CoreCLR, called the native test library, threw the deliberate managed exception and completed recovery. This is not Mono or engine-readiness proof.
- `evidence/MONO-HOST-CHECK.json`: matching Mono 6.12.0.206/0cbf0e290c3 under Rosetta compiled and ran the actual `InvocationBoundary.cpp` with a host-only observer and actual plugin. Two real Run invocations, one managed exception, two wrong-signature refusals, nested public GC transitions, pinned boxed Int32 handling and native entry/exit/cookie observations passed. This is helper proof; it did not compile the production owner, prove strict loader binding, prove interpreter-only/no-JIT execution or run in Townfall.

Production source is in `native/MonoOwner.cpp/.hpp`, `InvocationBoundary.cpp/.hpp`, `Cap10InvocationObservation.hpp`, `observer_fragment.inc` and `ProbeImage.hpp`. `tools/stage_current.py` stages three exact anchors in **UE4SSProgram.cpp only**, original SHA256 `fe9fe7bc1f856f2268290336dc65c4f5273aa27f8766426535ccfca0a317e45a`. Earlier stages pinned a different owner and must not be applied blindly. Its target set does not overlap #7's four modified originals; no combined integration driver was completed.

The `.config` files used by host harnesses contain Mac test-library mappings. They are host-test artifacts and must not be deployed as production plugin configuration.

`tools/stage_mono_loader.py` stages three exact public Mono source changes in `mono/utils/mono-dl.c`: reserved owned-name handling avoids ambient OS lookup, missing owned provider refuses, and fallback unregister assigns the returned list head. It is unapplied and uncompiled. Native owner remains excluded unless an actually admitted target port and genuine corlib/config paths are supplied. It uses MONO_AOT_MODE_INTERP; matching AOT/EH/native-wrapper dependencies remain necessary. Do not replace those facts with configuration assertions or host flags.

**Latest failed attempt:** `tools/run_mono_interpreter_check.py` and its CLI harness exist. The saved attempt failed with `System.TypeInitializationException` caused by missing `System.Native` before plugin proof completed. See `evidence/MONO-INTERPRETER-CHECK.log` and `MONO-INTERPRETER-OUTPUT.log`. No passing interpreter receipt exists. Do not treat Mono's advertised interpreter support as a successful interpreter-only run. Diagnose the existing dependency/configuration failure after resumption; preserve it rather than blindly repeating the command.

The independent source review was interrupted. Its preliminary assessment confirmed the helper receipt's narrow scope and consistent signature/exception/GC-handle sequences. No final approval exists. Genuine target GC/thread/TLS/stack roots/context/EH, runtime/corlib/provider lifetime, strict resolver patch and in-game Run → Throw → recovery remain open. Full Unity proof follows the minimal Townfall test.

## Matching Mono dependency

`caps-team-work/mono-host-research/` contains the official Mono MDK package and a selectively extracted runtime without installation. Exact package: 370,007,450 bytes, SHA256 `80b0dbfa59ba9ed76dbf1393998e6a2ed2d1ccc8f5850c7a46fbe31a2aea88d8`. `PACKAGE-PIN.json`, `HOST-READY.json`, `PUBLIC-SOURCES.json`, `BINDINGS.json` and logs retain provenance.

Extracted runtime root: `mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0/`. Executable `bin/mono-sgen64`; library `lib/libmonosgen-2.0.1.dylib`; runtime corlib `lib/mono/4.5/mscorlib.dll`; config `etc/mono`. This is an Intel Mac host requiring Rosetta, not a PS5 port. Stock loader lacks the production source patch. No installer/scripts or global/home installation ran. The large package, extracted dependencies and temporary host build products are excluded from the portable handoff ZIP; their local paths and acquisition receipts remain available.

## Numbers 1 and 3

Component: `caps-team-work/cap01-03/`. Read `HANDOFF.md`, `SOURCE-AUDIT.json` and `ROUTE-ADDENDUM.md/json` with this correction:

The pinned kstuff dispatcher has no exposed generic kernel-copy operation; its private helpers are not automatically callable by another userspace client. That conclusion is scoped to the inspected kstuff route. **Do not expand it into a claim that no public userspace kernel interface exists.** The source archive is present and hash-bound; old missing-source statements are stale.

Immediately before the second filter stop, research found a possible different route in independently licensed public homebrew loader/SDK source: loader-supplied pipe/socket context and kernel_copyin/kernel_copyout APIs. This investigation was incomplete. Preliminary observations were that the loader constructs context rather than merely forwarding untouched descriptors, nominal reads manipulate pipe metadata, and current wrappers do not expose reliable partial-I/O counts. Startup ABI/initialization, exact resident loader↔source identity, privilege/context requirements and compatibility were not established. No client was written and no operation was executed. Validate the actual source before treating this lead as usable; do not add an exploit or protected-hardware bypass to fill the gap.

The eight existing archive-auditor checks are audit-only and do not implement either capability. No mmap/sysctl/process-metadata demonstration or fake callback interface counts as kernel completion. A future real provider/client needs owned bounded targets, independent known answers, exact transfer semantics, compare/write/readback/restore/revoke behavior, recovery proof and three clean passes across three separate restarts. Current Phase B rules separately restrict new kernel-resident code/modified kstuff and console actions; resolve scope from the user's actual authorization and governing rules rather than inventing permission or bypassing them.

## Filter stops and interrupted work

Two worker continuations ended with the tool error: **“This content was flagged for possible cybersecurity risk.”** One was the #10 continuation after the successful matching Mono helper run; the other was the kernel-provider source investigation. The tools did not identify a specific rejected command or detailed reason. Saved work was retained. Do not claim a particular operation was rejected, silently retry through another agent, or attempt to bypass the filter. The user's ownership clarification is recorded above.

The #7 and #10 independent reviews were interrupted by the user's stop request, not by these filter errors. Their final review files/verdicts were not delivered. Implementation workers are completed/errored; reviews are interrupted. No new verification suite was run after the stop request.

## Next work after the user resumes

1. Finish the single interrupted #7 review, resolve the shared-cache source-selection/layout hold and missing behavioral regressions, and verify the exact current integration inputs. Do not overwrite ongoing Claude changes.
2. Finish the #10 review; diagnose the saved System.Native CLI failure if pursuing that host check. Establish the actual target Mono port and strict owner/loader/AOT dependencies before a target build or readiness claim.
3. Complete the public inherited-context provider investigation for #1/#3 only within authorized scope. Establish a real contract and source/resident identity before writing its real client; keep the three-restart/recovery requirements.
4. Produce a coherent cumulative integration/build selection with nonempty gates and one independent runtime review per component. Then the integrator can seek any required concrete deployment/test approval. There is no target executable from this continuation to upload.

## Previous work for all 12

The earlier `caps-1-12-mac-prep.zip`, `VERIFICATION.json`, `STATUS-TABLE.md`, `HANDBACK.md`, `READINESS-NEXT-INPUTS.md` and `CLAUDE-RESUME-HANDOFF.md` remain in the user delivery folder. The old bundle retains 238 scoped utility passes; do not add this continuation's assertions or host executions to that total. Its ZIP SHA256 remains `579b3c87cf34380806e67e98109c57c2c1288f127f40461cfd688efdfbca0060`. It is historical context, not current integration or console proof.

Preserve the earlier disclosed receipt-write incident: the initial old overlay tests refreshed `outputs/2026-10-08-l25-prep/pak-overlay/leon-replay-drift.json` and `leon-substitutions-fixture.json` without pre-run hashes. No guessed restoration was made. Later isolated tests kept originals stable. This exception belongs to the earlier preparation and must not be hidden by a blanket no-writes claim.

The accompanying portable ZIP contains current source stages, tests, receipts, licenses and this handoff, plus the previous full preparation ZIP. It excludes game executables, large Mono runtime/package files and temporary compiler products. `HANDOFF-MANIFEST.json` records the exact archived bytes; archive verification is packaging verification only.
