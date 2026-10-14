# GPT #7 step 2 and #10 — current handoff, 2026-10-13

Worktree: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`.
All results are isolated and unmerged. No PS5 contact, target PRX build, live loader/build/frozen edit or mod edit occurred in this continuation. Claude owns integration, gates, build and console testing; ZCode owns #1/#3. Preserve the current single-console-owner arrangement.

| Component | Delivered now | Still required |
|---|---|---|
| #7 step 2 | Complete current source/build/packaging stage, exact input bindings, focused green tests and actual filesystem/no-detour gates | Arranged independent review of the new additions, cumulative integration, one full build for its next launch, approved console proof |
| #10 | Actual production owner compiles and executes Run → managed throw → recovery in matching Mono full-AOT/interpreter mode on Mac; source re-pinned; relocated config and anonymous-image defects fixed; genuine TLS configuration patch staged | Complete usable PS5 Mono port/dependencies, compiled owned loader, integration/build and in-game proof. This is not finished console support and must not hold ready builds or Stellar Blade |

## #7: use the current complete stage

Start with `caps-team-work/cap07-step2/BUILD-INTEGRATION.md` and `BUILD-INTEGRATION-RECEIPT.json`. The authoritative generated stage is `derived/build-closure-final/INTEGRATION.json` plus `integration.patch`: eight original inputs, fifteen staged files. Earlier derived directories and source receipts are historical.

New continuation changes close real integration gaps: pakmount NEW_UNITS registration with source hash/template; two refreshed C++ metadata rows; five runtime header pins; required PakMountCodeProfile lineage overlay; and a hard refusal of incremental compilation because CodeImageCache Metadata changed from eight to sixteen pins. Five cache consumers were identified in all374 retained dependency metadata files. The integrator must run the actual full-build dependency/header-consumer gates; this census is not new compilation proof.

The real filesystem gate initially caught15 calls. Producer/adapter now use error-code status/size/iterator operations. Focused checks passed: five integration tests, six producer tests, thirteen adapter assertions; filesystem gate seven files/zero violations, no-detour gate seven files/zero attempts. JA's ninety-assertion core review is preserved and unchanged core suites were not repeated. Newly incoming cap09 LuaMod frame dispatch was inspected and retained in the exact rebind.

JA's conditional verdict is at `outputs/cap07-step2/REVIEW-VERDICT.md` in the worktree and `reviews/JA-CAP07-REVIEW-VERDICT.md` in the ZIP. JA's approval predates the new filesystem/build/packaging changes; do not treat it as approval of those additions. Root's final read-only binding check matched all eight current originals and fifteen staged files.

For integration, compose every approved change needed for the next launch, run gates and build once. The default driver mode is full; omit --incremental. Do not follow the old JD separate-build-per-patch sequence. Real engine ABI, allocator/delegate lifetime, external mod-root reachability and visible replacement content remain launch checks. Keep all original game files unchanged.

## #10: actual owner execution is now proved on host

Start with `caps-team-work/cap10/CURRENT-STATUS.md` and `CURRENT-SOURCE-MANIFEST.json`. The unchanged genuine Probe.dll is3584 bytes, SHA256 `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d`.

`evidence/OWNER-HOST-CHECK.json` now records REAL_MONO_OWNER_HOST_PASS: actual MonoOwner.cpp + InvocationBoundary.cpp, actual MONO_AOT_MODE_INTERP, two managed Runs, one exact managed exception, both native imports, recovery and clean diagnostics. This uses the matching genuine Mono6.12.0.206 runtime, a newly generated --aot=full,interp corlib copy and a host-only observer. It uses stock Mono's loader: strict ambient-resolution refusal and PS5 behavior are still unproved.

The production owner now loads global dllmap configuration, uses PREFIX/lib and PREFIX/etc semantics, and names the embedded image so Mono's AOT loader has non-null filename metadata. The real relocated File.ReadAllBytes regression passes without MONO_PATH masking mistakes. Both production translation units compile against genuine matching headers with warnings treated as errors. Host validation is explicitly rejected for target builds; no target admission macro was enabled. JB's independent verdict is preserved as the earlier review; these later changes need the arranged reviewer.

Current UE4SSProgram source pin: `44499a0aaeb617c80f9cee3ad62222cdffe9f45b4b71cc663fb25cce616150f1`; derived `4fab1e61549bdb8ff6bcc10cdfa35c47b350086e80ec937d9b8c0b4996ba2eba`. Three anchors and inverse verified, not applied. If the main source moves, inspect and rebase the narrow patch; never overwrite Claude's changes or weaken drift refusal.

Port preparation at `caps-team-work/cap10-port-research/port-stage/` fixes one actual source problem: the public compiler uses emulated TLS, while Mono's default native TLS-offset assumptions are incompatible. The patch selects Mono's existing pthread TLS route. Actual SDK probes show both __PROSPERO__ and __FreeBSD__=9 already defined, so no duplicated or invented FreeBSD backend was added. Seven source checks, four policy cases and five bounded compiler checks passed. Full configure regeneration/runtime compilation is still outstanding.

The SDK libmonosgen file is an import stub, not a runtime. A complete public-source target build must still supply genuine GC/thread/stack/TLS/context/signal behavior, target AOT transitions, System.Native/configuration and the patched owned loader. The Mac AOT dylib is not deployable to PS5. Unity is the later full integration proof.

## Paste to the arranged reviewer

Review the current #7 complete stage and #10 source/host evidence, focusing on changes since JA/JB: #7 error-code filesystem operations, source/metadata selection, CodeImageCache layout propagation and packaging; #10 lib/etc/config loading, named embedded image, host/target mode separation, actual owner invocation and the narrow pthread TLS patch. Use the exact receipts and existing test evidence; repeat tests only for a concrete uncertainty. Report actionable defects with file/line, consequence and minimal fix, plus the precise approval scope. Do not add unrelated guards, frameworks or extra build/launch steps. Stay off PS5 and do not modify main, build/frozen outputs or mods. Never promote host success or partial port preparation to console completion.

The ZIP contains reviewable patches, new source, test source, receipts and public dependency source/licenses. It omits generated full runtime-source snapshots, compiler objects, large MDK/AOT dependencies, game binaries and Epic source. Existing local generated trees remain available to Claude.
