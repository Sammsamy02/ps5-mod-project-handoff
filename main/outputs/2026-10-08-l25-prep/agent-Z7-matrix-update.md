# Agent Z7 — LAUNCH-PLAN interpretation matrix v2 update

Date: 2026-10-08 evening. READ-ONLY research + two additions to
`outputs/2026-10-08-l25-prep/LAUNCH-PLAN.md` (this directory). No other file
modified; no builds, no console/network contact. No existing LAUNCH-PLAN
content was rewritten — both changes are pure appends.

## Sources read first (as tasked)

1. `agent-V-listener-abi.md` — UE 5.6.1 confirmed; engine vtable slot 4
   (`SIZE_T GetAllocatedSize() const`, dispatched at `UObjectArray.h:1471`
   via `GetDeleteListenersAllocatedSize` ← `LogHashMemoryOverheadStatistics`
   `UObjectHash.cpp:2416`) was missing from our base → OOB read one word past
   our vtable → uncontrolled call. CONFIRMED OOB mechanism. Registration also
   writes the engine's live `UObjectDeleteListeners` TArray UNLOCKED with our
   own allocator (`port/unreal/src/UObjectArray.cpp:379-387`), vs the engine's
   `UObjectDeleteListenersCritical` (THREADSAFE_UOBJECTS=1).
2. `agent-Q-copyin-probe.md` — sink bank RETIRED at init (fds closed at
   `copyin_cleanup.complete()`, before the "slot 2 FName ctor bound" print);
   NO post-admission probe caller in any of the 374 composed units;
   "resident copyin probe races save-load" attribution WEAKENED; residual
   post-admission writers are modbridge-owned-buffer copyins only (ack sink,
   UE4SS log fd, diag channel — transition-only).
3. `evidence/VANILLA-VERDICT.md` — same-save vanilla load: 0 hits for
   `fatal signal`/`copyin`/`nonsleeping`/`libkernel`; CE-108255-1 signature
   requires our runtime. Includes the Z2 post-hoc note on the timed-out
   boot-armed ploader.
4. `agent-Z2-ploader-timing.md` — ini read EXACTLY ONCE at arm
   (`main.c:839`, single `read_ini` call site); resident window ≤120 s
   (`INJECTOR_TIMEOUT_S`); a `loadpayload` dispatch while resident is a
   lock-guarded NO-OP that cannot refresh the resident's list.
5. `agent-P-phase-tags.md` — exact `[l25]` grep strings (below).
6. Verified build facts: variant A carries the ABI repair
   (`2026-10-08-l25-runtime/typescan-r4/candidate/include/Unreal/UObjectArray.hpp`
   — `virtual size_t GetAllocatedSize() const { return 0; }` declared LAST =
   slot 4); `build_variants.py` defines l25-A normal registration / l25-B
   registration skipped (diagnostic-only). `agent-O-listener-instrumentation.md`
   STILL ABSENT → placeholder retained.

## What was appended to LAUNCH-PLAN.md

1. **§1.5 Ploader arming law (agent Z2)** — inserted at the end of §1
   PRE-LAUNCH (after 1.4; nothing rewritten). For every MODDED launch, after
   ANY `ploader.ini` change: confirm prior instance exit, dispatch
   `loadpayload` ONCE, confirm a FRESH `payload.elf` pid in `processes_list`
   before launching; an ini edit only takes effect at the NEXT arm; includes
   the verified vanilla-test corollary (boot-armed instance read the ini at
   boot, pre-flip, and timed out ≤120 s pre-launch → injection-free run).
2. **`## Interpretation matrix v2 — 2026-10-08 evening`** — appended at the
   end of the file (§3.3 left untouched, superseded by addition). Contains an
   input digest, a 5-row outcome table, and per-outcome blocks with exact
   grep markers + next actions (A-FULL / A-STALL / A-CRASH / B-SURVIVES /
   B-CRASH), plus the agent-O placeholder note.

## Matrix v2 (as appended, condensed)

- **A-FULL** (A survives save load + `[l25] on_program_start entered` +
  `[l22] Lua mod started` + L3+R3 works): listener family was BOTH the crash
  and the stall. Keep A resident; never run B; l26 folds V's lock/allocator
  registration fixes; mission ladder advances.
- **A-STALL** (A survives save load, mod doesn't start): crash = listener
  family; stall = something else past :528. Four-rung `[l25]` ladder
  localizes: (1) no `[l25] event loop started` → still :528-region
  (registration-adjacent; agent-O tiebreaker; runtime-status ack absent);
  (2) loop started, no `[l25] on_program_start entered` → gating upstream;
  (3) on_program_start entered, no `[l22] Lua mod started` → check
  `[TypeScan] scan-complete … core-ready=true`, `[R3] event carrier scan`,
  `[R3] marshal arm failed`; (4) mod started, L3+R3 dead →
  `[R3] FuncPtr-marshal armed`, `[l25] marshal hook first invocation
  (carrier fired)`, `r3_marshal_pumped`, `[l22] ExecuteInGameThread queued
  depth=`, `pad_borrow_begin`/`pad_key_edge`. Run B only if rung 1 reproduces.
- **A-CRASH** (A still crashes, CE-108255-1 / `copyin: has nonsleeping
  lock`): listener family insufficient — suspects ordered: residual
  modbridge-buffer copyin writers, remaining unlocked live-state writes,
  LoadMap detour/hook (agent F secondary); resident copyin PROBE now
  low-prior (retired at init, no post-admission caller). Check: 
  `copyin_probe_fire_post_admission`, `[Seeds][L25] copyin probe state after
  admission: kernel-write dispatch STOPPED` + `copyin_probe_liveness_state=3`
  / `copyin_probe_liveness_oracle_calls`, bridge `copyin_probe_state` /
  `copyin_probe_sinks_open` / `copyin_probe_nonce_used` /
  `copyin_probe_attempts_total` / `copyin_probe_attempts_late_total`,
  non-overlay `slot 2 FName ctor bound` retirement proof, aligned-entry log
  build id, klog `nonsleeping`+RIP. Overlay caveat: Q's `[Seeds][L25]`
  lines exist only if the copyin-probe overlay is wired per WIRING.md —
  verify adoption before treating absence as evidence. Then run B to
  bracket; escalate to Claude with A harvest.
- **B-SURVIVES** (B survives where A crashed): registration/write race
  (unlocked write, our allocator) confirmed as the crasher; slot-4 OOB
  secondary (real but not this crash). l26 = engine-lock + engine-allocator
  registration (or engine's exported `AddUObjectDeleteListener`), keep
  slot-4 virtual; restore A resident (B never stays).
- **B-CRASH** (B also crashes): neither registration nor listener — resident
  machinery beyond the listener; rare given the vanilla verdict but still
  ours. Same A-CRASH checklist + last-reached `[l25]` phase tag brackets the
  fault window vs klog timestamp. STOP launching (safety box 5), reboot,
  escalate to Claude with both harvests + phase-tag evidence; no replays.

## Exact grep vocabulary used (sources)

- agent P (in-build): `[l25] event loop started` / `event_loop_started=1`;
  `[l25] on_program_start entered` / `on_program_start_entered=1`;
  `[l25] marshal hook first invocation (carrier fired)` /
  `r3_marshal_first_invoke`; `r3_marshal_pumped`;
  `[l22] ExecuteInGameThread queued depth=` / `eigt_queue_depth`.
- agent Q (overlay-gated): `[Seeds][L25] copyin probe state after admission:`
  (…`kernel-write dispatch STOPPED`); `copyin_probe_liveness_state`;
  `copyin_probe_liveness_oracle_calls`; `copyin_probe_fire_post_admission`;
  bridge `copyin_probe_state`, `copyin_probe_sinks_open`,
  `copyin_probe_nonce_used`, `copyin_probe_attempts_total`,
  `copyin_probe_gate_released`, `copyin_probe_attempts_late_total`.
- Pre-existing (§3.2 vocabulary): `Adding shutdown listener`,
  `Starting callback garbage collector!`, `[l22] Lua mod started`,
  `[R3] FuncPtr-marshal armed on carrier`, `[R3] event carrier scan`,
  `[R3] marshal arm failed`, `[R3] marshal is the game-thread pump`,
  `[l22] keybind callback entry`, `[TypeScan] scan-complete`,
  `pad_l3r3_edge` → `pad_l3r3_f7_dispatched`, `pad_key_edge`,
  `pad_borrow_begin`, `pad_first_state_read`,
  `modbridge-runtime-aligned-entry/v1 build=`, `slot 2 FName ctor bound`,
  klog `copyin`/`nonsleeping`/`fatal signal`/`libkernel`.

## Not done / caveats

- No launch, no build, no console contact (read-only task).
- agent-O listener markers remain a placeholder; A-STALL rung 1 and
  B-SURVIVES post-skip receipts need its exact strings before first launch.
- Agent Q's overlay adoption into the twin build is unverified; matrix rows
  that cite `[Seeds][L25]` carry an explicit absence-is-not-evidence caveat.
