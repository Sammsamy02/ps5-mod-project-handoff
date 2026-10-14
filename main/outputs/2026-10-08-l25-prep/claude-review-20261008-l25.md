# Claude review directive receipt — l25 (2026-10-08)

Provenance: faithful paraphrase of the Claude review directive relayed by the
coordinating session in response to `CLAUDE-REPORT-20261008.md` (this
directory). The original review text is not stored on disk here; this receipt
is the durable local record and was cross-checked against the contemporaneous
summary in
`outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md` (section
"STATUS 2026-10-08"). Technical specifics are preserved verbatim where they
are load-bearing. Operational execution lives in `LAUNCH-PLAN.md` (same
directory).

## 1. Verdicts ACCEPTED as-is

1. **A/B v2 verdict accepted.** The valid 1947-byte both-hooks-false arm
   reproduced CE-108255-1 at save load identically to the 13:47 original run:
   `HookLoadMap` + `HookInitGameState` are EXONERATED. Settings-level bisection
   is closed; the next bisect happens in code, not in settings.
2. **Agent G stall location accepted.** Init stalls at
   `UObjectArray::AddUObjectDeleteListener` (`UnrealInitializer.cpp:528`;
   implementation `UObjectArray.cpp:379-389`) — the ONLY statement between the
   last logged line "Adding shutdown listener" (`:527`) and the next expected
   print "Starting callback garbage collector!" (`:542`). It is the first
   write / GMalloc-virtual-call into live game state in startup (append into
   the game-owned delete-listener TArray via `GMalloc->Realloc` on the UE4SS
   init thread). Same failure class as the l21 precedent "Virtual
   FMalloc::QuantizeSize is unavailable". A fault or deadlock there leaves
   `init()` unreturned, so event loop, `on_program_start`, and every `[l22]`
   marker stay unreachable — in every settings arm.
3. **Agent H pump analysis accepted.** The R3 marshal pump arms at
   `LuaMod::on_program_start` (`LuaMod.cpp:7933` carrier-calibration →
   `arm_funcptr_marshal_drain`), which is unreachable while `init()` stalls at
   `:528`. No session ever logged "[R3] FuncPtr-marshal armed on carrier" →
   pump never armed → no `ExecuteInGameThread` drain → L3+R3 dead. One root
   cause explains every downstream symptom. `bHookLoadMap` /
   `bHookInitGameState` do not gate the pump (only `bHookEngineTick=false`
   would; keep it true).
4. **Agent F forensics accepted.** The l24 diag vocabulary is init-only; from
   "Adding shutdown listener" to the crash there are zero events in every
   file — the crash window is unobservable in l24. Best-supported attribution:
   the resident modbridge copyin machinery (async copyin initializer +
   kernel-write probe left running after init) racing the game thread's
   save-load syscalls; kernel "copyin: has nonsleeping lock" requires a
   spinlock-holding copyin, which stock game code does not produce. Unlogged
   LoadMap detour remains a non-excludable secondary.

## 2. Dual hypothesis for the :528 fault (Claude's framing — l25 must
distinguish these)

- **(a) Registration hangs**: the `AddUObjectDeleteListener` call itself
  faults or deadlocks (the append / `GMalloc->Realloc` virtual call into the
  game-owned array executed on the UE4SS init thread).
- **(b) Listener added broken**: the listener IS registered but malformed —
  UE 5.6.1 `FUObjectDeleteListener` Itanium ABI mismatch and/or the unlocked
  TArray write racing the engine's GC — and the engine calls the broken
  listener during save-load mass GC, presenting as the CE-108255-1 crash
  minutes after init.

## 3. Agent K findings — fix ALL THREE

Not only the MEDIUM: apply all three findings from
`agent-K-l25-review.md` before the l25 build —
- F1 (MEDIUM) noexcept Checked wrappers (`NativePropertyPointerGuard.hpp:86-88`)
  call `layout_offset` which throws on a missing layout member →
  `std::terminate` at scan time (regression from l24 collect-and-continue);
  fix = catch-and-record inside the Checked wrappers or assert the exact keys
  in the required preflight set at Install.
- F2 (LOW) `fname_index_within_verified_pool` margin budgets the ANSI suffix
  only (2+8, not 2+16 for wide L"Property") — doctrine-consistent laxness;
  document it.
- F3 (LOW) trailer parser records fabricated SlotRead/field=0/class=0 for
  unparseable `[PropertyPointer]` messages, and `substr` arithmetic could
  throw from inside the catch on out-of-order markers — harden.
Status: applied by `agent-N-review-fixes.md` (LayoutMissing=4 / Unknown=5
appended to the enum without renumbering; whole-body catch in the three
Checked wrappers; marker-order bounds check; ANSI-margin comment), CHANGES.json
hashes updated and re-verified.

## 4. Item-6 re-point BEFORE build

The item-6 host suite must be re-pointed at the l25 headers and re-run BEFORE
any l25 build is treated as certifying the shipped code (it was pinned to the
l24 typescan-r3 include dir). Status: done — `agent-S-item6-repoint.md`
(`2026-10-08-l25-runtime/tests/run_tests.py` → `typescan-r4/include`;
PASS_HOST_ONLY_MAC plain + asan-ubsan, 2/2, tested-header hashes recorded in
`tests/PRE_RUN_HASHES.txt`). Re-run with the single command in that report if
any header changes afterwards.

## 5. l25 scope items 1–6 (as dispatched; see MISSION STATUS 2026-10-08)

1. Listener instrumentation + ABI verify + registration path at
   `AddUObjectDeleteListener` (`:528`) — post-:528 progress markers, guard or
   trace of the first live-state write, surface copyin-oracle involvement if
   the write path needs it (agent O).
2. Runtime-phase diag tags (event loop / `on_program_start` / marshal /
   detour entry / queue depth) so the crash window and the init stall point
   become observable (agent P).
3. Copyin-probe liveness/gating — suspend or serialize the resident probe
   around engine load activity, or prove it idle (agent Q).
4. Twin variants A (normal registration) / B (skip registration,
   diagnostic-only) with twin gates + import checks (agent R).
5. Item-6 host suite re-point + re-run BEFORE build (agent S — done, §4).
6. Klog capture + VANILLA save-load test (ploader NOT armed) to test whether
   the "copyin: has nonsleeping lock" signature is even ours (agent T + root).
Support: ABI ground truth (agent V); launch plan + packet template (agent W —
this document pair); K-fix application/verification (agents N/X).

## 6. Launch plan directive (operative summary; full procedure in
`LAUNCH-PLAN.md`)

- Console REBOOT first — kstuff is paused after the crash run.
- Restore the ORIGINAL `UE4SS-settings.ini` (byte-exact 1947 B from
  `/tmp/l24-settings-backup.ini`, sha256
  `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`) —
  never test on the A/B v2 residue.
- **Variant A first** (normal registration, instrumented); **variant B only
  if A stalls or crashes**; B is diagnostic-only and unsafe long-term (the
  skipped delete listener means stale object pointers are never cleared).
- Per launch: main-menu wait **90 s** (does init pass :528? does the mod
  start?), load the SAME save, wait 30 s, **L3+R3 once**, clean exit or note
  the crash code. One launch per test; each launch needs explicit user OK.
- **Packet to Claude BEFORE ANY upload** (and before any l25 PRX touches the
  console): build receipts, twin variant hashes, static-gate tables, import
  checks; harvest logs and klog result appended as evidence lands.
- Vanilla klog test is independent and may run first.

## 7. Safety invariants carried forward (unchanged)

Fake-sign + decrypt-OFF readback verify for every executable upload; no
rapid-fire launches (clean exits between; reboot after any crash); kstuffSharp
remains quarantined (never autoload); ploader exits per launch cycle — re-arm
via pldmgr between launches in one boot; unmodded play = empty `ploader.ini`,
never touch `autoload.txt`; settings file must stay exactly 1947 bytes
(modforge MFA admission is size-exact from `loadlist.txt`).
