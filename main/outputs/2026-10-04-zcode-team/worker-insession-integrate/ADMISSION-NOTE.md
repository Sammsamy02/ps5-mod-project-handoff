# ADMISSION NOTE — worker-insession-integrate (2026-10-05/06)

STAGED PAIR that integrates the refusal-surviving in-session diagnostic
service (worker-insession-refusal, host 94/94) into the PS5 UE4SS runtime
composition. Everything here is STAGED / PROPOSAL-ONLY: nothing was
dispatched, built for console, or run on console. Composition, console work
and game claims are explicitly OUT OF SCOPE. Host logic proof only
(host-test/receipts/host-test-pass.txt, 49/49 checks, 3/3 stable runs,
2026-10-06T05:18:09Z).

## 1. The pair (complete staged files, this worker's staging/)

| file | baseline (sha256) | staged (sha256) |
|---|---|---|
| `staging/runtime_start_bridge_r14_diag_spanpub_insession.cpp` | controller launch10 final-units `runtime_start_bridge_r14_diag_spanpub.cpp` `497348b8d247d531912594ddaea85482c19131eff3be4ff155932c3d954091a6` | `8b5d546d91d45798e0d8d9bafc033944b793e773e16af2283d02ad674a381f2b` |
| `staging/runtime_module_entry_insession.c` | worker-d1-determinism-diagnosis `staging/lineage-overlay-2-d1-c4/lifecycle/runtime_module_entry.c` `c97055f40f5a6dd94dd6360c4459538b8130e8b63a806a84d975d7c7604db470` | `40b4b2a4f74d0976912d5af776304cb3636ecc0a06a631840fc6b5ae4ae48995` |

Diffs (PURE-ADDITIVE, zero removed lines, verified):
- `staging/diffs/runtime_start_bridge_r14_diag_spanpub-insession-integrate.diff`
  sha `825eb8faca3d84eecde9dd204e18542b32565e31291a4d30d4825fc5a8c635f2` (8 hunks)
- `staging/diffs/runtime_module_entry-insession2-hook-APPLIED.diff`
  sha `ea9264ff54083b35e93e582872a2ef6bed4bcb5471e637e676998ff65bfadcc4` (5 hunks)

The second file applies worker-insession-refusal's staged diff
`staging/diffs/runtime_module_entry-insession2-hook.diff` VERBATIM (all five
one-line calls + the include; service_stop in the diff's stated SPAN-CORRECT
form at the top of module_stop) plus a dated provenance comment. No other
unit is modified.

Referenced UNCHANGED units (never duplicated, provenance preserved):
- worker-insession-refusal/staging/: `insession_cmd2_core.{h,c}`,
  `insession_cmd2_service.{h,cpp}` — the four ADDED composition units.
- worker-insession-refusal/staging/diffs/runtime_owner-insession2-refused.diff
  — NOT part of this pair; see §5 (companion).

## 2. Integration point chosen (and the alternatives refuted)

BRIDGE + MODULE ENTRY. The bridge is the spawn/registration host exactly as
directed: `bridge_insession_hook()` runs inside the scan wrapper immediately
AFTER `bridge_io_controls()` — i.e. during `supplier->capture()` in Stage 1
of `modbridge_runtime_start_with_activation`, BEFORE conversion, BEFORE
`StartActivatedRuntimeWithAck`, and before ANY seeds work
(`consume_required_slot_seeds` runs later, on the async init thread via
`setup_unreal -> ScanGame`). The service thread therefore exists across
every post-capture recoverable refusal (lease gap, conversion, context,
start, async seeds refusal).

- `ue4ss_backend.cpp` REFUTED as the host (task alternative 3): the backend
  ctor runs INSIDE Stage 3 wiring (after the scan wrapper) and
  `initialize()` runs on the RuntimeOwner init thread; its shutdown path
  never executes on refused boots, so a backend-hosted `service_stop` would
  leak a live thread past `probe_lifecycle_stop`/DSO finalize. It owns no
  module_start/module_stop.
- `UE4SSProgram.cpp` (wpd1, `cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af`)
  deliberately UNMODIFIED: (a) module_start/module_stop live in the module
  entry, not the program; (b) the service's design law is independence from
  engine/program state (it must work exactly when `shutdown_native_program`
  has torn the output devices down); (c) any program-side poll hook would
  recreate worker-insession-diag's SUPERSEDED v1 event-loop consumer.
- Entry-hosted `service_start` (entry diff, before `start_composed_runtime`)
  and bridge-hosted `bridge_insession_hook()` coexist by IDEMPOTENCE: with
  both applied the entry's earlier spawn wins and the bridge call is a
  no-op re-registration of the same values (host-proven, T7). With the
  bridge alone the thread still spawns at the scan wrapper, but dispatch
  stays HELD (no settle caller) — the pair is designed to ship TOGETHER.

## 3. Command wiring (task items 4)

- io_probe ops a-d -> ONE adapter `bridge_insess_io_batch_probe` registered
  on all four slots, calling the EXISTING frozen `bridge_io_controls()`
  verbatim. RECONCILIATION: worker-insession-refusal's note asked for
  per-control functions `bridge_io_control_a..d`; the frozen io-controls
  region defines NONE (the controls are inline blocks) and restructuring it
  would break its own HOST-TEST-EXTRACT verbatim contract — so the task's
  first-listed option ("call bridge_io_controls()") is implemented. The op
  letter survives in the service's reply line ("io_probe op=c"); per-control
  verdicts stay in the io_ctrl_* diagnostic tags. The adapter rc (0) is a
  DISPATCH ACK, never a control verdict. When worker-io-controls publishes
  per-control functions, re-wire slots 0..3 one-to-one — a one-line change
  per slot inside the INSESSION-INTEGRATION-HOST-TEST region.
- pool_probe -> `insession_cmd2_register_pool_probe_target` with the pinned
  SeedTable hypothesis target: `kBaseObserved + kFNamePoolLabel`
  (0x400000 + 0x9365ec0 = 0x9765ec0) and
  `(uint32_t)rc_seed::kFNameBlockCapacityBytes` (0x20000). The service calls
  `bridge_kernel_write_copyin_probe` FRESH on every command (the
  once-per-boot gate lives in the initializer TU only, never the bridge).

## 4. Bridge additions (all inside/below the frozen regions, none inside)

1. Header provenance comment block (8 hunks total, additive only).
2. `static void bridge_insession_hook(void) noexcept;` forward declaration
   beside the r14 L4 forward declaration.
3. The INSESSION-INTEGRATION-HOST-TEST region (between markers, after the
   io-controls HOST-TEST-EXTRACT-END): service header + SeedTable.h +
   SeedValidation.hpp includes, the adapter, the hook (registrations +
   idempotent spawn). The region is SELF-CONTAINED and is compiled VERBATIM
   by the host test (sed-extracted to hook.inc) — the shipped TU contains
   the same bytes the test compiled (worker-io-controls marker law).
4. Call site: `bridge_insession_hook();` directly after `bridge_io_controls();`
   in the scan wrapper.
5. `insession_cmd2_note_initializer_refused()` at every POST-capture refusal
   return of `modbridge_runtime_start_with_activation`: the lease-gap return,
   the `!converted` return, the null-owner return, both Stage-3 catches
   (MRS_ERR_START), both Stage-2 catches (MRS_ERR_CONTEXT), both outer
   catches (MRS_ERR_CONVERSION). All are provably at/after the spawn point;
   the note is noexcept, once-per-boot, and idempotent with the entry's
   calls (compare_exchange law).

## 5. Entry additions (refusal-worker diff verbatim)

- include `insession_cmd2_service.h`;
- `insession_cmd2_service_start()` in module_start AFTER a clean constructor
  run, BEFORE `start_composed_runtime()`;
- `insession_cmd2_settle_startup()` AFTER `start_composed_runtime()` returns
  (outcome known, refusal or ok) — dispatch unheld exactly at the service
  law's sanctioned point;
- `insession_cmd2_note_initializer_refused()` at the `mfa_activate_failed`
  branch and BOTH start-failure arms;
- `insession_cmd2_service_stop()` at the TOP of module_stop (span-correct
  form): joins on every boot that started the thread, before the runtime
  stop and long before `diagnostic_close_channel`.

COMPANION (recommended, outside this pair): worker-insession-refusal's
`runtime_owner-insession2-refused.diff` covers the ASYNC refusal note
(seeds refusal surfacing on the RuntimeOwner init thread after module_start
returned 0). Without it the service still WORKS in-session (settle fired;
io_probe/pool_probe dispatch normally — host-proven with the refusal flag
set, T3-T5), but the one-line refused-mode triage line is not emitted for
the async case. Do NOT apply worker-insession-diag's v1 event-loop hook:
v2 supersedes it (at most ONE in-session diagnostic_event consumer).

## 6. Host test (integration layer)

`host-test/run_host_test.sh` compiles: the REAL service TU + REAL grammar
core (worker-insession-refusal/staging, `INSESSION_CMD2_HOST_TEST`), the
REAL extracted hook region (verbatim), and the mock surface (kernel trio +
read + rename hook forward to a mkdtemp FS; `diagnostic_event` and
`bridge_kernel_write_copyin_probe` recorded; `bridge_io_controls` counted).
`host-test/test_insess_integration.cpp` sha
`47e06d19a2b88e3bfb0e07a964412262424875e06c1330bbff4de7f9493a751e`;
script sha `aef6390e33d3f3ebdfc1865c01f2404c200542d07ce09409da42a8e304b0fed8`;
receipt sha `3c359e1de8116c6cab2edcef96d5a2f340b1fc4971f028608fd0ac270f0ab747`.

49/49 PASS, re-run 3/3 stable. Asserted: pre-registration "not wired"/"not
registered" replies with zero bridge calls (T0); hook runs no IO by itself
(T1); dispatch HELD pre-settle for >= 450 ms (T2); `note_initializer_refused`
triage line then the held command dispatches exactly once after settle —
i.e. the service works WITH THE INITIALIZER-REFUSED FLAG SET (T3); io_probe
op=c -> exactly one `bridge_io_controls()` call, reply "op=c ...
tags=batch-A-D" (T4); pool_probe -> exactly one copyin probe with the
registered (0x9765ec0, 0x20000) verbatim, reply addr/len/class/fresh (T5);
diag/log/reject superset sanity (T6); double-start idempotence — one thread,
one dispatch (T7); stop join < 500 ms and a dead thread consumes nothing
(T8). Two test-sequence bugs were fixed during bring-up (a stale-.consumed
assertion; an illegal start-after-stop shape that no composition can reach —
stop is module_stop-only); the integration code needed no changes.

SCOPE: this proves the INTEGRATION layer (lifecycle, wiring, dispatch
gating). The service layer's 94-check suite (worker-insession-refusal)
remains the parser/dispatch authority; neither makes console claims.

## 7. Admission shape (composition/pin-cascade checklist)

1. Add the four worker-insession-refusal units to the TU set; their
   directory must be on the include path (the entry and the bridge hook
   include `insession_cmd2_service.h` unqualified; the bridge hook also
   includes `SeedTable.h`/`SeedValidation.hpp` from the final-units dir —
   the same dir the baseline bridge already includes `SeedValidation.hpp`
   from).
2. Replace the bridge TU with `runtime_start_bridge_r14_diag_spanpub_insession.cpp`
   (baseline 497348b8 -> 8b5d546d), the entry with
   `runtime_module_entry_insession.c` (c97055f4 -> 40b4b2a4). No other
   baseline byte changes; the io-controls HOST-TEST-EXTRACT region and the
   spanpub/L1/L3 logic are untouched (diff hunks 125/175/1300/1513/1546/
   1599/1654/1663 only; the 1300 hunk starts BELOW the extract-END marker).
3. Recommended companion: apply runtime_owner-insession2-refused.diff
   (async refused note). Never apply the v1 event-loop hook.
4. `bridge_kernel_write_copyin_probe` must resolve INTRA-PRX (present from
   composition6-4 onward); a missing definition is a LOUD LINK STOP.
5. Per-build import audit stays mandatory (PLOADER-IMPORT-ANALYSIS.md §4):
   the new TUs reference EXISTING contract members only
   (sceKernelOpen/Write/Close; read/rename/pthread_create/pthread_join/
   nanosleep/getpid/clock_gettime; strcmp/strlen) — the derived contract
   must come out at exactly 196 strong; any drift is a stop. The bridge's
   new includes are header-only constants (no new UNDs).
6. First composition compile is the console-branch proof; there is no
   console-only branch in the added code (deliberately, per the service law).

## 8. Honest bounds

- Host logic proof only. CONSOLE behavior is NOT measured and NOT claimed:
  FTTP STOR reachability mid-session, libkernel read/rename/pthread binding
  under the game sandbox, real copyin verdicts in-game, UE4SS.log
  interleaving under frame load, behavior at game quit.
- io_probe runs the WHOLE batched controls for any op (documented above);
  Control D spends the window-E candidate copy only when Control B verifies
  the write route — the frozen batch law, unchanged.
- The pool target is the SeedTable HYPOTHESIS address; a pool_probe verdict
  class 0/-1 there is address-class evidence about THAT address, never a
  disproof of the live pool (Blocks[0] storage class remains unresolved
  upstream). The registered target is not updated from initializer
  candidates in this pair — a deliberate one-target, pinned-constant
  integration; extending it is a one-call change at the hook.
- Line numbers in the paired files' comments refer to the pinned baselines
  above (shas); drift there is documentation drift only.
