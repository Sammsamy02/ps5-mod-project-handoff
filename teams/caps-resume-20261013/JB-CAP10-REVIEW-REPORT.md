# Agent JB report — CAP10 (#10) independent review + System.Native diagnosis (2026-10-13)

Mac-only: review, diagnosis, one host rerun. No PS5 builds, no console, no
integrator source edits. Full detail: `caps-team-work/cap10/REVIEW-VERDICT.md`.

## Delivered

1. **REVIEW-VERDICT.md** (`caps-team-work/cap10/REVIEW-VERDICT.md`) — the first
   complete independent review of the #10 stage: compilation, host evidence,
   production source, host→PS5 gap, verdict, loader assessment, corlib chain,
   and a new live drift finding.

2. **System.Native diagnosis: root-caused and FIXED.** Mono 6.12 corlib
   P/Invokes `System.Native` (implemented by `lib/libmono-native-compat.0.dylib`,
   272 `SystemNative_*` exports, mapped via `etc/mono/config`). The harness set
   `MONO_CFG_DIR=…/etc/mono`, but Mono appends `mono/config`, so it parsed
   `…/etc/mono/mono/config` (nonexistent) → dllmap never loaded →
   `DllNotFoundException`. Correct semantics per Mono's own `assembly.c`:
   config dir = `PREFIX/etc`. One-line fix in
   `tools/run_mono_interpreter_check.py`; rerun PASSES:
   `evidence/MONO-INTERPRETER-CHECK.json` =
   REAL_MONO_INTERPRETER_HOST_PASS (2 runs, 1 exception, honest
   not-AOT-only/not-strict-resolver flags). Failed receipts preserved as
   `*.FAILED-20261013.log`. Debug reproduction saved in
   `cap10/tmp/interp-debug-full.log`. Advisory: same off-by-one exists in
   `run_mono_host_check.py`/`MonoHarness.cpp` argv (masked; green receipt left
   untouched).

3. **Loader patch verified correct** against pinned stock `mono-dl.c`
   (sha `b7ccf30b…`): all three changes sound; the third
   (`fallback_handlers = g_slist_remove(…)`) fixes a genuine stock Mono
   use-after-free bug that the production owner's own unregister would
   trigger. PS5 compile of patched Mono needs the full 0cbf0e290c3 tree +
   Prospero toolchain + a real port (threads/GC/EH/interpreter, static embed,
   target `libmono-native` + dllmap) — NOT feasible this session; patch is
   ready for that build.

4. **Corlib compatibility confirmed at runtime:** Probe.dll references exactly
   `mscorlib,4.0.0.0,b77a5c561934e089`; the runtime corlib presents the same
   identity (v4.0.30319); same bytes ran on CoreCLR, Mono embedding, and Mono
   interpreter. Full PS5 dependency chain documented in the verdict (§4).

5. **New live finding:** integrator `UE4SSProgram.cpp` drifted at 10:19 (now
   `ad3ae621…` ≠ pinned `fe9fe7bc…`); the stager correctly refuses
   (fail-closed verified live); anchors still exact-once, no Cap10 content
   applied; `CURRENT-STAGE.json`/`current-owner.patch` now STALE — re-pin +
   re-review required before any apply. (Independently corroborated by agent
   JD's report.)

## Verdict summary

Host evidence and source stage: approved as host-only proof. Target execution:
unproven — port, patched-loader build, corlib/native/config deployment,
re-pinned UE4SS integration, and in-game Run→Throw→recovery all remain open.

Report ends.
