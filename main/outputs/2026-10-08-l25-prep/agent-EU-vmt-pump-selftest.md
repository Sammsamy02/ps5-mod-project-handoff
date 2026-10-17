# agent-EU — GameEngine Tick VMT pump (FIX 4) + end-of-init SELF-TEST battery (FIX 5)

Date: 2026-10-09. Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (consumption law
honored — existing rows re-keyed by hash sync, new files registered in `added_headers`
/`added_header_hashes`; scope key `scope_eu_vmt_pump_selftest` added to CHANGES.json).
Technique source read in full: `outputs/2026-10-08-l23-runtime/profile/VMT-FALLBACK.md`.

## FIX 4 — VMT Tick pump (PRIMARY game-thread pump)

### Arming point
`candidate/src/UnrealInitializer.cpp`, inside the `bHookEngineTick` block, in the
`#if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE` fold, immediately after the
existing R3 deferral (`R3MarshalGate::pump_pending.store(true, ...)` + refusal logs) and
after the vtable-fallback Tick bind that assigns `UEngine::TickInternal`. The arm call is
`(void)VmtPump::vmt_pump_arm(GameEngine)` — the SAME startup-lookup instance
(`GetInstanceOfClass("GameEngine", "/Script/Engine.Default__GameEngine")`, the one logged
`GameEngine CDO/Instance: 0x190003A340` on this console); no hardcoded pointer. Arming
NEVER throws into init; every refusal is one loud log line and leaves the R3 carriers as
the only pump.

### Mechanics (host-testable core: `candidate/src/VmtTickPumpCore.inc`)
Shared VERBATIM between the target glue and the host fixture (`namespace RC::Unreal::VmtTickPump`,
struct `PumpCore` + `drive_tick` + `narrow_ascii_copy`):

1. Tick slot from `UEngine::VTableLayoutMap[L"Tick"]` (byte offset / 8; loud warning if it
   differs from the profiled 0x310 receipt; never a hard fail — the live profile binds).
2. Bounded scan (512 entries) for the LAST plausible code pointer in the original vtable,
   reading through byte-wise volatile loads (SeedArming read_u64 discipline). Classifier =
   `rc_seed::classify_seed_address(word, SeedArming::module_view_base()) ==
   SeedAddressClass::CodeCallOnly` — the pinned image's exec LOAD0 text segment, data-only.
3. Copy `(vptr - 0x10)` — offset-to-top + RTTI words, bit patterns preserved verbatim
   (RTTI may be null) — through the last code entry, into a private `operator new[]`
   allocation (16-aligned, RW, never freed — process-lifetime retention per the doc's
   pre-entry-reader grace-period argument).
4. Every copied entry re-validated: null (reserved slot) or text; a non-text word inside
   the extent is a loud `NonTextEntryInTable` arm refusal. Tick slot must itself be text.
   Slot poke verified against the hook pointer; full copy verified bit-identical except
   the poked slot BEFORE any publication.
5. Publish = ONE aligned 8-byte volatile vptr store (+ observational readback) swapping
   GEngine's vptr to `copy_allocation + 2`.
6. Hook entry `vmt_engine_tick_entry(void*, float, bool)` — plain SysV function matching
   the profiled entry (receiver RDI, float XMM0, bool ESI). `drive_tick` ordering:
   (a) forward to the captured ORIGINAL slot value exactly once, direct call, same
   receiver/delta/idle (calling the object's virtual Tick again is impossible by
   construction — the saved pointer is called, never a virtual dispatch);
   (b) first call: publish the game-thread identity through the existing rev-B bridge
   `R3MarshalPublishGameThreadIdentity()` (StartupWait-cancel aware, NativeNameThread
   ticket, `publish_current` — the immutable-first-wins machinery); a refusal aborts the
   first-tick extras, never the already-completed original call;
   (c) first call after publication: the FIX 5 UFunction self-test;
   (d) every tick: the time-boxed drain. Everything after the original call is
   exception-isolated per stage — no stage can omit/duplicate the original or throw
   across the engine boundary.
7. Drain bridge `RC::Unreal::VmtEngineTickDrain()` defined in `candidate/src/LuaMod.cpp`
   right after `funcptr_marshal_hook`: the R3 marshal's drain shape (raw game-thread gate
   -> `NativeLuaActionDispatchScope` ownership -> deferred LoadMap arm ->
   `m_game_thread_actions` -> marshal-tagged delayed actions -> pending
   NotifyOnNewObject callbacks -> pending requeue) plus the FIX 4 time-box (4 ms
   steady_clock deadline gates the later stages; unrun work stays queued) and a per-tick
   cap (128 simple actions; overflow returns to the front of the pending queue; safe —
   `SimpleLuaAction` is a trivial value struct with no destructor logic). The R3 carriers
   REMAIN the FALLBACK pump, untouched: `pump_pending` stays true (the marshal's own
   pairing law), both drains share queue-move semantics and the dispatch-scope guard, so
   sequential double drains are idempotent.
8. Teardown: `UnrealInitializer::VmtTickPumpRetire()` (noexcept) called FIRST in
   `UE4SSProgram::shutdown_native_program` (before the event-loop join / mod uninstall /
   device close). Ownership-checked restore — only writes if the object still owns our
   replacement address point; a foreign writer is reported (`ownership lost`) and never
   clobbered. The disarmed Prospero fold provides a no-op definition so the link holds in
   both folds.

### Accepted-risk declarations vs VMT-FALLBACK.md (controller decision, in-code comments)
Arming at startup init is not a proven quiescence point — the single aligned store is the
accepted publication primitive (an in-flight dispatch saw either the fully-valid original
or the fully-prepared copy). The table extent is the bounded validated scan, not a full
construction-table proof. The copy is retained for the process lifetime (no free — the
grace-period proof is unavailable on this target).

### XOM-safety
Data-only: no code byte is written or read anywhere on the arm/publish/drain path; the
shared original vtable is never written; no BP-class or name lookups run inside the hook;
the only engine calls are the original Tick (already the engine's own dispatch) and the
self-test's ProcessEvent (below).

## FIX 5 — end-of-init SELF-TEST battery

Placement: `UnrealInitializer.cpp`, `SelfTest::run_end_of_init_battery()`, called after
the post-arm try/catch closes (a post-arm failure rethrows past it — honest failure keeps
precedence) and immediately BEFORE `PostInitialize(UnrealConfig)` — i.e., after the scan,
names/types stores and ProcessInternal derivation, before mod start. Per line:
`[ST] <test> PASS/FAIL <detail>`, narrow-first (`st_log` — the `send_narrow_diag`
doctrine: fmt narrow -> one `Unicode::DecodeUtf8` widen -> literal fallback), every probe
independently try/catch-isolated, NEVER stops init. Decoded/verbatim text enters logs
through `narrow_ascii_copy` — the u16string-copy discipline (per-unit down-copy, NUL
terminated, non-ASCII downgraded to '?', never the broken SDK widen); the battery only
consumes NUL-terminated ctor copies (the splitter fix is agent ET's parallel work).

Line list (in order):
1-10. `[ST] fname <name> PASS/FAIL id=<hex> blk=<pool block number> decoded='<narrow>'`
   for: `None`, `ByteProperty`, `/Script/Engine`, `/Script/CoreUObject`, `Actor`,
   `PlayerController`, `ReceiveTick`, `Tick`, `UserWidget`, `ItemDatabaseSubsystem`
   (FNAME_Find ctor -> `FName::ToString` in-port pool decoder round trip; block = id>>16;
   PASS requires exact decode AND id!=0 except for `None` itself).
11-15. `[ST] sfo <path> PASS/FAIL <addr>` for `/Script/CoreUObject.Object`,
   `/Script/Engine.Actor`, `/Script/Engine.PlayerController`, `/Script/UMG.UserWidget`,
   `/Script/UMG.Default__WidgetBlueprintLibrary` (`StaticFindObject_InternalSlow`).
16. `[ST] ffo Engine PASS/FAIL <addr>` (`FindFirstOf`).
17. `[ST] ffo GameInstance PASS/FAIL <addr>` (`FindFirstOf`).
18. `[ST] fao PlayerController PASS count=<n>` (`FindAllOf`).
19. `[ST] pump vmt ARMED|REFUSED (ufunc line pending first tick)` — FIX 4 state evidence.
20. `[ST] ufunc Conv_NameToString PASS result='ByteProperty' (<n> units)` — emitted by
   the pump's FIRST genuine tick (`VmtPump::first_tick_ufunc_selftest`, on the just-
   published game thread): one harmless `UKismetStringLibrary_Conv_NameToString` call on
   the bound KismetStringLibrary CDO with the known name `ByteProperty`, result narrowed
   and logged; unbound/exception shapes log FAIL lines.

## Verification

- Host fixture `tests/actual_eu_vmt_pump.cpp` (includes the SAME `VmtTickPumpCore.inc`),
  plain + ASan/UBSan: ALL CHECKS PASS (receipts `results/EU-HOST-FIXTURE.json`). Covers:
  bounded scan (extent=last text entry, null-inside-extent legal, tick-slot-past-extent
  refusal, empty-table refusal), arm/copy (prefix verbatim, bit-identical entries, poked
  slot, 16-byte alignment, address point = alloc+2, re-arm refusal, non-text-inside-
  extent refusal with nothing retained), publish + genuine dispatch through the swapped
  table (hook fires; original forwarded EXACTLY ONCE with exact receiver/delta/idle;
  identity published + self-test on first tick only; drain every tick), publication
  refusal shape (original still forwarded once, extras skipped, retry succeeds later),
  restore (original vptr back, subsequent dispatch hits the original directly, hook never
  fires, second retire no-op), foreign writer never clobbered (ownership loss reported),
  and `narrow_ascii_copy` (exact copy, early NUL, truncation-at-cap, '?' downgrade,
  degenerate caps).
- Target compile (pinned prospero-clang++, l24-build-r2 composition argv + ES vfs
  overlay): `unreal-unrealinitializer`, `program`, `luamod` all exit 0 — receipts
  `results/EU-TARGET-COMPILE.json` (+ per-unit logs). No new warnings from this change
  (remaining warnings are pre-existing port-header ones). Touched TUs cannot host-compile
  by design (PS5 SDK); the host-testable surface is the shared `.inc`, which is exactly
  what the fixture exercises.
- CHANGES rows synced: `candidate_sha256` refreshed for the three touched TUs;
  `VmtTickPumpCore.inc` and the fixture registered in `added_headers`/
  `added_header_hashes`; scope key `scope_eu_vmt_pump_selftest` records arming point,
  fix summaries and verify receipts.

## Files

- NEW `candidate/src/VmtTickPumpCore.inc` — host-testable pump core.
- NEW `tests/actual_eu_vmt_pump.cpp` — host fixture (plain + asan-ubsan binaries built).
- `candidate/src/UnrealInitializer.cpp` — include + bridge decls, VmtPump glue (classifier,
  st_log, ufunc self-test, hook entry, arm), `VmtTickPumpRetire` (+ disarmed no-op fold),
  SelfTest battery + end-of-init call site, arming call in the R3 fold.
- `candidate/src/LuaMod.cpp` — `RC::Unreal::VmtEngineTickDrain` (time-boxed marshal-shape
  drain).
- `candidate/src/UE4SSProgram.cpp` — TU-local retire declaration + first call in
  `shutdown_native_program`.
- `CHANGES.json` — hashes synced, new files registered, scope key.

No console contact, uploads, or launches. The gameplay prototype status and every other
scope are untouched.
