# REPORT — worker-error-path

**Verdict: ANALYSIS COMPLETE.** The composed composition-4 runtime (run-3) has four
early-return paths in `module_start` that produce **exactly zero output**, one of them
structurally incapable of logging (`-22`, the diagnostic-channel-open gate). The ack
status file (`runtime-status.txt`) has three distinct silent-zero-output conditions,
and as composed today its start entry (`StartActivatedRuntimeWithAck`) has **no
in-PRX caller**, so the status channel is dormant until the next game-bound caller
unit lands. One path crashes the game by design (`std::terminate` in
`RuntimeOwner::~RuntimeOwner`). Full ranked analysis:
`ERROR-PATH-ANALYSIS.md` (same directory).

## Task answers (essentials)

1. **module_start bootstrap** (`outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`):
   - FIRST statement: `__atomic_test_and_set(&entry_guard)` (line 51). First external
     call: `diagnostic_ready()` (line 57). First substantive call:
     `probe_lifecycle_start` (line 69, `lifecycle/lifecycle.c`).
   - First error paths (guard busy `-20`, already-active `0`, bad state `-21`,
     diagnostic gate `-22`) **return silently — nothing is written anywhere**. The
     `-22` path cannot write by construction: the diagnostic channel IS the only
     logger (`diagnostic.c` uses only sceKernelOpen/Write/Close against the
     build-pinned `PROBE_LOG_PATH`).
   - First observable output: `diagnostic_event("module_start_entered", 0)` (line 59).
2. **ue4ss_backend constructor** (composition-4's attested replacement,
   `worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp`):
   `PrepareActivatedUE4SSRuntime` has try/catch that writes `ack->failed(1, what)` and
   rethrows — but only when the ack sink had already opened. Failures **before**
   `RuntimeAckSink::Open` (mod_source pre-set; `MakeActivatedUE4SSModSource` title/file
   validation) leave `ack == nullptr`: **no status file, no failed line**, and the
   exception text is discarded by `probe_call_constructor_catching` (returns code only).
   Backend-ctor failures after the sink opens ARE observable.
3. **runtime_ack_supplier / RuntimeAckSink** (`runtime_owner.cpp:130-153`): open
   failure does **not** fail startup — it returns an inert sink (`fd=-1`) whose
   `last_error_` is never persisted; every later emit is a silent recorded no-op.
   Empty `status_root` behaves identically. Allocation failure returns `nullptr`.
4. **Zero-output paths** (documented with condition/mechanism/loaded-module status in
   ERROR-PATH-ANALYSIS.md sections 2, 5, 7): `-20/-0/-21/-22` gates; inert ack sink;
   dormant ack caller; activation rejection pre-open; mid-session `mfa_status_write`
   failure (no complete final line → e2e reader sees "not running"); silent
   truncation/drop inside `diagnostic_event`.
5. **Crash path**: `RuntimeOwner::~RuntimeOwner` → `if (!stop()) std::terminate()`;
   `stop()` returns false on the initializer thread itself → game terminates. All other
   failure modes leave the module loaded and the game running.

## Method / provenance

- Sources: module_entry.c, lifecycle.c, constructor_boundary.cpp, diagnostic.c (the
  four newest trace generations are byte-identical; the diagnostic implementation is
  not under outputs/ps5-mod-bridge — it ships prebuilt in the composition baseline),
  composition-4's pinned ue4ss_backend.cpp + runtime_owner.cpp (composition-3
  bootstrap-stage, re-replaced as `bootstrap-*.o` in run-3), runtime_ack_supplier.cpp,
  WIRING-SPEC.md, run-3 receipt/runtime.map/exports.stdout.
- Caller wiring claim verified against run-3 `runtime.map` / `exports.stdout`
  (symbol defined + exported) and a workspace-wide source grep (no caller);
  WIRING-SPEC §S5.5 states the caller ships with the next game-bound module unit.
- Evidence boundary: `probe_runtime_validate_and_preload` / `probe_worker_start`
  bodies are prebuilt baseline objects, sources not in this workspace; their behavior
  is inferred from the lifecycle.c ops contract only. Nothing was executed; no
  console, no network, no nested subagents.
- Calls used: ~34 of 40.

## Artifacts (write scope honored)

- `CLAIM.json` (written first)
- `ERROR-PATH-ANALYSIS.md` — full analysis, ranked silent-failure table
- `REPORT.md` — this file (last)
