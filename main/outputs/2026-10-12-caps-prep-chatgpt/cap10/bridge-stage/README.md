# Source-only callback-scoped native bridge

Approved design: bridge-seam-audit/BRIDGE-SEAM.md. No Mono host, compiled C++, public-export-table or PInvoke resolution proof. Nothing applied to the genuine runtime. Full required managed host/GC/TLS/AOT/exception/metadata work remains. B7 does not wait for this prep.

The stager pins genuine candidate UE4SSProgram.cpp SHA121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da and validates both complete anchors before transformation/output. It inserts target-conditional, allocation-free thread-local state/scope before init and wraps only the existing synchronous ready callback inside its original mutex/shutdown conditional. Previous TLS binding is restored on normal exit, exceptions and structural nesting. No asynchronous/nested managed invocation support is claimed.

RuntimeProbe reads actual is_program_started and actual shutdown atomic through the admitted owner scope. It never looks up s_program. Null reply returns-4; absent scope/shutdown/notstarted returns-3; unexpected C++ exception returns-5. Success writes cookie and returns0. All failure returns leave reply unchanged. A non-null reply must already be valid caller-owned writable int32 storage: no arbitrary address validation is claimed. ABI version1 identifies this contract only. Definitions and proposed header agree on noexcept; Probe imports now demand ExactSpelling=true.

Owner evidence is retained in bridge-seam-audit: actual RuntimeOwner stop requests shutdown and joins initialization before program/backend destruction. It refuses self-stop. The ready callback runs synchronously before the event loop; callback exceptions restore admission scope before original init failure handling. Provider module lifetime/whole-engine unload authority remain separate obligations. Candidate UE4SSProgram.cpp:65 includes UE4SSProgram.hpp, whose :3 directly includes atomic and target branch:14 includes Platform/NativeProgramContext.hpp, whose :4 directly includes cstdint. Those inspected includes declare std::atomic_bool/std::int32_t; no new namespace-local include is injected.

Future same-image Mono host may bind exact native addresses through a controlled actual resolver even when public exports expose only module entrypoints. These definitions use default visibility, but final export retention/public lookup is not established. __Internal normally searches the process; fallback registration alone is not an import allowlist. The host must enforce its chosen exact metadata/name/library policy and retain the provider image throughout invocations; no such resolver exists here.

Run from the repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap10/bridge-stage/test_stage.py
PYTHONDONTWRITEBYTECODE=1 python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap10/bridge-stage/stage_bridge.py
```

Default is dry-run. Optional --derived-out accepts only a new .cpp inside bridge-stage, resolves paths and refuses overwrite/outside/live-target output. There is no apply operation. Source hash drift, empty input and reapplication fail closed. Normal local filesystem assumptions apply; this is not adversarial filesystem race hardening. Do not copy a full derived runtime into deliverables.

Retained verification: TDD-RED.log5 source-contract failures before stager; TDD-GREEN.log5pass after; IMPORT-RED.log expected failure accepting missing exact-spelling policy; AFFECTED-PREP-GREEN.log18pass after contract/preflight/regression updates (previous17 plus one exact-name-policy test). Four unchanged lifecycle tests were not rerun. Assembly metadata policy is now dependent on ExactSpelling: ASSEMBLY-EXACT-SPELLING-RED.log records the missing-bit regression and ASSEMBLY-EXACT-SPELLING-GREEN.log records all15 artificial assembly tests passing after enforcing actual ImplMap bit0x1 (fixture default0x201). Test predicates and inverse checks verify staged source bytes, not C++ control-flow/ABI/TLS behavior.

stage-receipt.json records one exact dry-run; WRITE-CHECK.json records one successful isolated temporary derived write for the final comment-corrected version (removed; one earlier-version write also removed), overwrite/outside/live-target refusals3 and original unchanged. bridge.patch contains only the two source changes. The single independent runtime-source review is retained in REVIEW.json and REVIEW.md; its source-only verdict leaves target compilation, resolver, provider lifetime and managed execution unproved. No builds, compiler, dependency install or console contact occurred.
