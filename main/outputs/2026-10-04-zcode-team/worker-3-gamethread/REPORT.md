# REPORT — worker-3-gamethread (slot 7)

Status: COMPLETE for the bounded pass. All deliverables written; host tests
pass. No file outside this output area was created or modified; no network
calls; no console contact; no subagents.

## 1. Exact changed/created files (all mine, under outputs/2026-10-04-zcode-team/worker-3-gamethread/)

- `CLAIM.json` — ownership, inputs, dependencies.
- `INTEGRATION-MAP.md` — the exact current path + the four seams (S1/S2/S3/S4).
- `adapters/gamethread_seams.hpp`, `adapters/gamethread_seams.cpp` — typed,
  refusal-correct seam adapters (host-mirror types of the real seam APIs).
- `adapters/seam_host_test.cpp` — host tests: adapter refusal paths + the
  REAL existing guards (runtime_owner.cpp compiled as-is; the real
  NativeGameThreadIdentity.hpp included as-is).
- `adapters/build_and_test.sh` — one-shot build+run script.
- `adapters/build/seam_host_test` (+ .dSYM) — generated host test binary
  (host-only; not a target artifact).
- `MILESTONE-TEST-DESIGN.md` — the game-thread milestone test definition.
- `REPORT.md` — this file.

Existing files were read, never edited. `runtime_owner.cpp` and
`NativeGameThreadIdentity.hpp` were compiled/included unmodified as read-only
link/include inputs specifically to exercise the REAL guards rather than a
reimplementation.

## 2. Production integration points (where workers plug in — no rewrite)

1. **S1 (worker-1)**: `RC::Platform::MakeNativeProgramContext(NativeLoaderFacts,
   serialized_elf)` — `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp:7`.
   Worker-1 supplies `ModuleInput` (retained lifetime + MainExe readable image),
   `OwnedMappedImageFacts` (ELF64, bases, EH extents) and the bound paths.
   Existing refusals (format, snapshot validation, path validation,
   PreInitialize owner-identity guard at
   `native-coherent-runtime-1/source/port/unreal/src/UnrealInitializer.cpp:946`)
   make anything less than genuine live facts fail closed.
2. **S2 (worker-2)**: `StageNativeLayoutFiles` (wired:
   `UE4SSProgram.cpp:908`), `SetResolvedNativeLayoutAxes` (wired:
   `UnrealInitializer.cpp:832`), `PublishStagedNativeLayoutMaps`
   (`ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp:172`).
   KEY FINDING: `PublishStagedNativeLayoutMaps` has NO production caller today
   (verified by search across all spikes and the coherent source; only tests
   call it). The controller must wire it at/just before
   `InitializeVersionedContainer()` with `(Version::Major, Version::Minor,
   resolved axes)`. Its refusal semantics (unstaged / axes-unknown /
   identity-disagreement; same-facts retry only) are complete and correct —
   wire-up only, no redesign.
3. **S3/S4**: milestone observation point is the first genuine Tick identity
   (`NativeGameThreadIdentity.hpp`) plus the activation-owned
   `on_engine_initialized` chain (`UE4SSProgram.cpp:654-661`).

## 3. Behavior + evidence

Host test `seam_host_test` (Apple clang 15, C++20, -Wall -Wextra, 0 warnings):
**ALL CHECKS PASSED**, covering:

- S1 adapter refusals (10 cases): supplier absent; pretender (paths/base
  without live proof — explicit "not a supplier" error); proof absent;
  lifetime absent; MainExe absent; ranges absent; non-ELF64; serialized-witness
  without EH extents; relative/non-normalized paths. Accept path preserves all
  inputs verbatim.
- S2 adapter refusals (10 cases): profile absent; unstaged pair; version
  absent; axes unknown (both axes); proof absent; required entry missing;
  guessed AND unknown origin; unbounded width; low confidence. Accept path
  resolves to exactly the `PublishStagedNativeLayoutMaps` argument tuple.
- Gate: refuses S1-absent and S2-absent; arms only when both resolved;
  post-arm refusal preserves armed state; no silent rebind.
- REAL RuntimeOwner guards (immutable source compiled as-is): normal
  Prepared→Starting→EngineInitialized→Stopped; double-start refused; honest
  "init returned without a stop request" failure when init returns
  unrequested AND Failed state survives cleanup stop(); exception path
  preserves Failed; SELF-JOIN REFUSAL verified (init-thread `stop()` returns
  false, no deadlock); late ready cannot overwrite Stopped.
- REAL game-thread identity guards: `id()` throws the honest too-early error;
  raw accessors refuse; Published → AlreadyCurrent → DifferentThread with the
  original identity retained.

Honesty statement: these are host behavior checks of seam contracts and
existing guard code only. Nothing here is, or claims to be, a Townfall
game-thread callback; the proof tokens are host-minted under
`MODBRIDGE_SEAM_HOST_TESTS` and that define does not exist in a target build.

## 4. Deviations / issues encountered

- Pinned LLVM 21.1.8 (`native-probe/build/deps/llvm/...`) could not build host
  C++: no bundled host std headers, and with `-isysroot $(xcrun
  --show-sdk-path)` its upstream headers clash with Apple SDK internals
  (`__darwin_wint_t`, `stdarg.h` errors). Since this is a host BEHAVIOR test
  producing no target object/IR, `build_and_test.sh` uses `/usr/bin/clang++`
  (SDK-matched). Any target build still uses the pinned LLVM/SDK route.
  Recorded in the script header too.
- All referenced input paths existed and matched the checkpoint descriptions;
  nothing materially missing. One nuance: multiple copies of
  NativeProgramContext.hpp exist across spikes; I read the canonical
  coherent-runtime source copy (`native-coherent-runtime-1/source/port/...`),
  which is the one the tick-file-owner composition selected.

## 5. Remaining assumptions

- Worker-1/worker-2 deliverable shapes were inferred from the existing seam
  APIs themselves (that is the point of plug-in integration); my mirror types
  must be swapped 1:1 for the real SDK types at integration. The adapters are
  deliberately structured so `ResolveLiveModuleFacts`/`ResolveLayoutProfile`
  outputs map directly onto `MakeNativeProgramContext` inputs and the
  `PublishStagedNativeLayoutMaps` argument tuple.
- The gate object is host tooling for integration testing; on target the same
  ordering is enforced by the existing admission refusals (PreInitialize,
  ModuleSnapshot, layout publication), which I verified by reading, not by
  target execution.
- EH-extent requirement for the witness route mirrors the saved scan-snapshot
  limits ("Explicit bounded complete EH frame required with EH header").

## 6. Next executable test (for the controller)

1. Integrate worker-1's supplier at S1 and worker-2's profile at S2, then wire
   the missing `PublishStagedNativeLayoutMaps` production call (Seam S2 GAP).
2. Run the milestone test exactly as specified in MILESTONE-TEST-DESIGN.md:
   harmless `UObjectArray::GetNumElements()` read delivered on the first
   genuine game-thread Tick, two consecutive ticks, with the §2 evidence set
   (identity + affinity-refusal + stable counts + version/layout provenance +
   clean cooperative stop). Everything in §3 of that document remains
   non-counting.
