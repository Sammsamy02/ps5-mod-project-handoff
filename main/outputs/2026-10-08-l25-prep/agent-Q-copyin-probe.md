# Agent Q — l25 item 7: resident copyin probe liveness

Date: 2026-10-08. READ-ONLY analysis + overlay files. No builds, no console, no network.
Deliverables: `outputs/2026-10-08-l25-runtime/copyin-probe/` (two pure-addition candidates,
two patches, MANIFEST.json, WIRING.md). No file outside `2026-10-08-l25-runtime/` was
modified; typescan-r4 and the l24 lineage were only read.

Sources analyzed: l24 packaging lineage
(`outputs/2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp`, sha
89e4b643…), the l25 parent composition
(`outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json` + `undefined.stdout`),
unit sources it pins (runtime_owner.cpp, ue4ss_backend.cpp, UnrealInitializer.cpp at
`2026-10-08-l24-runtime/typescan-r3/candidate/src/`, SeedValidation.hpp,
PoolAdmissionEngine.hpp, pool_admission.h, CopyinSink.hpp/CopyinStartup.hpp, probe.c,
lifecycle/runtime_module_entry.c, diagnostic.c/h, NativeFNameAdapter.hpp), the l25
build model (`2026-10-08-l25-runtime/build_runtime.py`, parts profile/typescan-r4/thread),
and `agent-F-crash-forensics.md`.

## 1. What actually stays resident after startup admission

The copyin machinery has four pieces; their post-admission state differs:

| Piece | Post-admission state | Evidence |
|---|---|---|
| Sink bank (parent dir fd + 64 single-use O_EXCL sink fds) | **RETIRED (closed)** at admission completion | opened only in `bridge_copyin_directory_prepare` (bridge:980-1043) during module_start's LMF capture (bridge:1718); closed by `bridge_copyin_directory_release` (bridge:1044-1061) from `bridge_copyin_initializer_complete` (bridge:1072-1077), which `consume_required_slot_seeds` invokes via `CompletionCleanup copyin_cleanup` (UnrealInitializer r4:973) and releases at `copyin_cleanup.complete()` (r4:1155) — success, refusal-unwind, and the earlier preflight catch (r4:1355) all retire it |
| Initializer owner gate (`g_copyin_initializer_owner`) | **STILL ARMED on the runtime init thread for the whole session** | `RuntimeOwner::initialize` calls `bridge_copyin_initializer_enter` (runtime_owner.cpp:101) and its `CopyinOwnerScope` destructor calls `leave` only when `initialize` returns (runtime_owner.cpp:91-103); `backend_->initialize` = `program_->init()` which blocks until shutdown (ue4ss_backend.cpp:59-71; runtime_owner.hpp:67-69 "initialize blocks until shutdown") |
| Kernel-write probe fn (`bridge_kernel_write_copyin_probe_until`) + write loop | Present in PRX, callable, but **structurally stopped**: with the bank retired it returns Unavailable at the parent-fd check BEFORE any kernel write (bridge:1256; gate check at 1248; per-sink single-use cap at 1259) | the l12-lineage short-write loop survives as the page-chunk `while (landed < len)` loop (bridge:1285-1311), bounded by the deadline and the 64-sink budget — but it cannot run without fds |
| Residual data: fault ledger + 3×4 KiB capture slots (bridge:1124-1130), `g_code_cache`, RuntimeAckSink retained fd, diagnostic channel fd | Retained, **read-only or state-transition-only**; no post-admission kernel writes copy from game memory | complete() comment "Keep both owned captures and fault evidence" (bridge:1076); ack writes only at state transitions (runtime_owner.cpp:76-80,183-204) |

Also resident but orthogonal: the probe worker thread is one-shot (probe.c:57-83: sleeps
`PROBE_DELAY_SECONDS`=5 (probe_config.h:10), delivers the 88-byte Lua self-test, EXITS —
crash-session diag shows `worker_sleep_complete`→`buffered_lua_result` then nothing);
the deferred pad input gate (agent F's `pad_input_deferred_input_gate=1`) is a separate
system; the L23 game-thread probe is the listener work, out of scope here.

**Post-admission callers: none exist.** Across all 374 units of the composed runtime,
only `UnrealInitializer.cpp` references the admission engine / copyin bridge symbols
(`fnamechecks::`, `fnameabi::`, `bridge_write_copyin_capture`,
`bridge_kernel_write_copyin_probe*`) — verified by scanning every pinned unit source —
and every call site is inside `consume_required_slot_seeds` (init). `bridge_io_controls`
(Control D) has no caller ("No module-start caller remains", bridge:1347-1349). Runtime
FName construction goes through the native ctor call only (NativeFNameAdapter.hpp:39-49;
no bridge dependency in NameTypes.cpp).

## 2. Is it supposed to be idle after Seeds admission? Can it still fire?

Supposed to be: **idle-by-resource-exhaustion**, deliberately. The design intent is
"armed gate, retired bank": the gate stays armed until shutdown to preserve the
single-owner telemetry discipline (CopyinStartup.hpp:5-10), while the retired bank makes
any dispatch refuse pre-kernel-write. It is NOT armed-for-use: nothing can legitimately
fire it after admission.

It CAN still fire only in these windows, all init-phase:
- During admission: the `admission::retry` loop (r4:1080) — 5 s interval
  (PoolAdmissionEngine.hpp:12 `kRetryIntervalMs`), bounded by the one 60 s shared budget
  (kAdmissionBudgetMs, PoolAdmissionEngine.hpp:11) — including VQ-inconclusive →
  probe fallback (PoolAdmissionEngine.hpp:127-140) and the ctor signature/roundtrip
  checks between pool admission and bank retirement (r4:1100-1144).
- No lazy re-probe on faults exists: the fault ledger only EXCLUDES ranges
  (SeedValidation.hpp / engine `session.exclude`), and seed refusals are terminal for
  the launch (r4:736-742 "Terminal for this launch; derive afresh with new evidence,
  never replay").
- Post-admission, a hypothetical future caller on the init thread would pass the gate
  (1248) but hit the retired-bank refusal (1256) — no kernel write. My logging makes
  that event loud instead of silent.

**Refinement to agent F's attribution (important):** in the crash session the bank is
provably retired BEFORE UE4SS init finished — UE4SS.log:538 "slot 2 FName ctor bound …
detour verification skipped" is emitted after `copyin_cleanup.complete()` (r4:1155
precedes that print at r4:1156-1157). With oracle-probes=0 and zero fds, the resident
kernel-write probe could not have performed kernel copyin at crash time. The residual
modbridge writers at crash time (ack sink fd, UE4SS log fd, diag channel fd) all copyin
from modbridge-owned buffers and write only on transitions — not the game-lock race
shape. So either the crash driver was agent F's secondary candidate (uninstrumented
LoadMap detour / hook path), or something not yet instrumented at all. The runtime-phase
instrumentation (listener work) remains the decisive evidence; my liveness logging
closes the copyin-probe side of the blind spot and can falsify the primary attribution
on the next launch.

## 3. What I implemented (l25 placement and why)

`build_runtime.py`'s overlay model is one-hop (`replacements[original]=candidate`,
build_runtime.py:41-47, resolved once at :106), so two parts cannot chain on one unit
source — and the typescan-r4 part already owns the only correct unit
(`typescan-r4/CHANGES.json` `files` maps typescan-r3 UnrealInitializer.cpp → its
candidate c4364dd7…). The bridge is not a unit of the runtime build at all (it is
packaging-side; `bridge_copyin_initializer_*` are undefined in
`outputs/2026-10-08-l24-build-r2/pass1/undefined.stdout:23-30`). Per the task's fallback
clause I delivered overlay files + exact wiring (details in
`copyin-probe/WIRING.md`, hashes in `copyin-probe/MANIFEST.json`):

1. **UE4SS side** — `copyin-probe/candidate/src/UnrealInitializer.cpp` (base = typescan-r4
   candidate) + `patches/unrealinitializer-copyin-probe-liveness.patch` (pure additions):
   - `CopyinProbeLiveness::note_admission_complete(...)` called immediately after
     `copyin_cleanup.complete()` (r4:1155 region): ONE-TIME `copyin_probe_liveness_state=3`
     + `copyin_probe_liveness_oracle_calls` diagnostic events AND the UE4SS.log line
     `[Seeds][L25] copyin probe state after admission: kernel-write dispatch STOPPED …`
     (stating bank retired, gate armed-until-shutdown, oracle-probes count, throttle
     promise).
   - `CopyinProbeLiveness::note_probe_call(...)` in `L13AdmissionOps::probe()` (r4:911-916):
     THROTTLED (first + ≥10 s apart) `copyin_probe_fire_post_admission` events + WARNING
     UE4SS.log line if any probe dispatch is attempted after the state note. Init-phase
     fires stay on the existing `pool_copyin_probe_*` telemetry.
   - Import-safe: only `diagnostic_event` (already an undefined symbol of the baseline
     object, undefined.stdout:38; declared exactly as UE4SSProgram.cpp:13 does) — the
     build's raw-import-delta assertion (build_runtime.py:143-144) is untouched.
2. **Bridge side** — `copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp` (base =
   l24 lineage, sha 89e4b643…) + `patches/runtime_start_bridge-copyin-probe-liveness.patch`
   (72 added lines): one-time OBSERVED post-retirement state in
   `bridge_copyin_initializer_complete` (`copyin_probe_state` bits: gate_armed /
   bank_retired / parent-fd-still-open(abnormal) / sink-still-open(abnormal), plus
   `copyin_probe_sinks_open`, `copyin_probe_nonce_used`, `copyin_probe_attempts_total`),
   shutdown stop-proof in `leave` (`copyin_probe_gate_released`,
   `copyin_probe_attempts_late_total`), and a throttled 10 s late-fire census inside
   `probe_until` (after the owner-gate check, before any existing behavior). All
   observation-only; no probe behavior changed; statics only; no new link symbols.
3. **Wiring**: exact steps in `copyin-probe/WIRING.md` — the typescan-r4 owner (or
   coordinator) adopts the candidate/patch and updates their row's `candidate_sha256`
   to 125a0471…; the packaging step swaps the lineage bridge (verifying base sha
   89e4b643…) or applies its patch. NOT wiring it into build_runtime.py's parts list was
   deliberate: a chained row would be silently never compiled (false belief of coverage).
   Static checks done: both diffs are pure additions; brace/paren balance equals base.

## 4. Can the probe be safely suspended after admission? (recommendation, not gated)

**Recommendation: YES — safe to gate today, gate it in l26 (not unilaterally).**

- What suspension means concretely: call `g_copyin_initializer_owner.leave()` at the end
  of `bridge_copyin_initializer_complete()` (one line, bridge-side). After that
  `allows()` is false for every thread including the init thread, so `probe_until`
  returns -1 at the first check (bridge:1248) — fail-closed for the whole session.
- Why it is safe today:
  - No post-admission caller exists (section 1); nothing observes the difference between
    the retired-bank refusal (Unavailable) and the gate refusal (-1) — both refuse.
  - In-session re-admission is contractually absent: seed refusals are "terminal for
    this launch" (r4:736-742); pool growth never re-enters admission (runtime FName
    goes through the native ctor, NativeFNameAdapter.hpp:39-49; no bridge symbols in
    NameTypes.cpp); captures/fault-ledger consumers are all admission-time.
  - Next-launch re-admission is unaffected: a new process re-runs module_start →
    `prepare()` re-opens the bank (bridge:980).
- Why not unilateral: (a) it removes the (already inert) machinery from the runtime
  phase — as an A/B experiment for the CE-108255-1 attribution it should be a
  controller decision landing WITH the runtime-phase instrumentation so the experiment
  is observable; (b) a future feature that wants in-session re-admission (e.g.,
  re-validating grown pool blocks) must then re-arm explicitly by a new controller
  decision rather than silently relying on the resident gate; (c) my finding (section 2)
  weakens the copyin-probe as the crash primary, so gating it is defense-in-depth, not
  a fix. With my overlay adopted, the shutdown events already report
  `copyin_probe_attempts_late_total`, so a gated build and an ungated build produce
  distinguishable evidence either way.

## Provenance / caveats

- No build was run (rules): syntax was checked statically (pure-addition diffs, balance
  check); the l25 import-delta assertion is enforced by build_runtime.py at build time.
- The UE4SS-side patch is based on typescan-r4's candidate c4364dd7…; if that owner
  revises their file, rebase the three hunks (anchors: `probe()` r4:911-916, helper
  after `native_fname_thunk` r4:956-967, call after r4:1155).
- Bridge-side patch is based on l24 lineage 89e4b643…; verify at packaging time.
- Files written: `2026-10-08-l25-runtime/copyin-probe/{MANIFEST.json,WIRING.md,
  candidate/src/UnrealInitializer.cpp, candidate/lifecycle/runtime_start_bridge.cpp,
  patches/unrealinitializer-copyin-probe-liveness.patch,
  patches/runtime_start_bridge-copyin-probe-liveness.patch}` and this report.
