# RUNBOOK — launch-11 marshal-consumer integration (extending worker-luamod-l11-dryrun VERDICT)

Worker: worker-l11-marshal-integration, 2026-10-05. LOCAL ONLY: never touches
`launch10-build/`, any canonical receipt, or another worker's staging. All
artifacts live under `worker-l11-marshal-integration/`. No composition run, no
packaging, no console contact was performed by this worker; everything below is
either host-proven here (receipts cited) or is a controller/console step spelled
out exactly.

Provenance chain (all sha256, verifiable):

| link | artifact | sha256 (first 16) |
|---|---|---|
| base | `worker-luamod-rebase/staging/luamod-l10rebase/LuaMod.cpp` (variant-B, dry-run GO) | `4540593ac9e66cdc` |
| rev | `diffs/l11-marshal-consumer.diff` (this worker) | `f595c57fb8f3ca10` |
| staged | `staging/luamod-l11-marshal-consumer/LuaMod.cpp` (base + rev, round-trip-proven) | `04c9d91664dd82c7` |
| control obj | `work/control-luamod.o` (base under frozen argv) == dry-run gate object | `5595feb785206c00` |
| consumer obj | `work/consumer-luamod.o` (staged under frozen argv) | `854a476bdd4f9985` |

---

## 1. What changed vs the dry-run GO state

Exactly ONE replace-unit source revision: the `luamod` staging copy gains the
consumer (2 hunks, one file — see DIFF.md). Nothing else: same 172-token argv,
same driver machinery, same staged initializer (the ARMED fold,
`worker-luamod-rebase/staging/initializer-l10-r3/UnrealInitializer.cpp`
`8b0c7e9905ce4a85…`, in-file `UE4SS_R3_MARSHAL_MODE=1` at line 179), same
program source. The consumer is what turns the dry-run's §6 decision into a
buildable candidate; it does NOT by itself make the R3 world fully bootable
(§5 below — say this out loud in any launch brief).

## 2. Controller steps for the consumer-integrated compose (in order)

1. **Re-pin the one revision**: produce the controller's replace-receipt
   revision where the `add-luamod-replacement-source` entry points at
   `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp`
   (`04c9d916…`) instead of the rebase staging (`4540593a…`), via the
   documented regen mechanism (`regen_receipt_l10lm.py` pattern; the dry-run
   VERDICT step 2/4 already names it). Do NOT edit the delivered driver or
   fixed script in place: make a tagged driver copy (one edit, distinct ID —
   the L10LM-E3 `--replace-runtime-unit luamod=…` line re-pointed), matching
   the L10LM edit discipline. Every other pin unchanged, including the two
   lineage diffs and the initializer staging `8b0c7e99…`.
2. **Fresh-unit sha gate** (VERDICT step 1, revised): the 8 pinned shas with
   the single luamod substitution above; both lineage diffs unchanged.
3. **Two full driver passes** (VERDICT step 2 discipline, unchanged): UNMODIFIED
   driver copy, fresh run dirs, `--program-source
   worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp` REQUIRED,
   serialized (one heavy build at a time; exclusive of any launch-10
   packaging; one `package_prx.py` at a time).
4. **Compose acceptance** (structural, path-independent — the dry-run's
   byte-sha comparators carry over with ONE substitution):
   - exit 0; `retained_units_replaced == ["luamod","program"]`; 368 units;
   - run-a == run-b byte-identical;
   - **`luamod.o == 854a476bdd4f998581f206af9faaa10b64eba8814fc590350c9565dce47f9565`**
     (this session's consumer object; the frozen-argv route reproduced the
     dry-run's control byte-identically, so the same path-independence is
     expected — if the compose's staging basename differs and the bytes drift,
     STOP and re-run the structural checks below on the produced object; do
     not silently widen the comparator);
   - composed object: NEW sha (the `633a8c82…` comparator was valid only for
     the as-authored variant-B; do NOT reuse it);
   - chain-symbols 14/14; composed final object's undefined set vs the
     r3-armed baseline: the three new luamod UND symbols
     (`RC::Unreal::R3MarshalPumpPending`,
     `RC::Unreal::R3MarshalPublishGameThreadIdentity`,
     `RC::Unreal::UFunction::GetFuncPtr`) must be RESOLVED (absent from the
     final object's dynamic-undefined set) — they resolve against the armed-fold
     initializer and the untouched unreal-ufunction unit respectively. If
     either bridge symbol is UNRESOLVED at final link, the initializer fold is
     wrong (mode 0) — stop; that is the rev-B pairing law refusing loudly;
   - manifest (VERDICT step 4): same derivation, second entry's
     `sha256_after` now `04c9d916…`, provenance records this worker's diff
     `f595c57f…` and the base `4540593a…`.
5. **Gate v2** (VERDICT step 5, unchanged): expect the same 15/15 families.
6. **Determinism**: the two passes must remain byte-identical (the consumer
   adds a `.data.rel.ro` candidate table and no new randomness).

## 3. In-game acceptance oracle — the marshal path

### 3.1 Arming (no keypress needed; the carrier is engine-driven, C-7)

Order-bearing log lines (R3-armed fold world, BOTH_FAIL write-refusal):

```
[R3] refused: FName constructor verification detour (...)   <- DIFF-2, at its phase
[R3] refused: EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch)
[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal
Finding GameEngine::LoadMap                                  <- resolve walks still run (reads OK)
...
Starting Lua mod 'TownfallItemSpawner'                        <- PROG:1506 via start_lua_mods
[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick (callback id 0)
```

The `armed on carrier` line is THE arm proof (DIFF-1's own emission, now
reachable because the consumer calls it). Variants, each diagnostic:

```
[R3] marshal carrier candidate not found: Actor:ReceiveTick (not interned, or class not constructed yet)
[R3] marshal arm refused: carrier <full name> ...            <- precheck: null FuncPtr / script-only / no FUNC_Native
[R3] marshal arm failed: no ranked candidate passed the carrier precheck; closures will queue until a carrier arms
```

W-works world (initializer armed its detours normally) must show exactly:

```
[R3] marshal not armed: initializer owns the game-thread pump (pump_pending=false)
```

and NONE of the `[R3] refused:` lines from the initializer.

### 3.2 Pump live (first carrier invocation, still no keypress)

```
[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)
```

Emitted inside `funcptr_marshal_hook` after the first successful identity
publication. Any ticking actor drives `ReceiveTick` → post-hook → publish →
this line. Absence with the armed line present = carrier never invoked (F-1
rate evidence: the candidate does not actually fire).

### 3.3 Closure drain — TIERED HONESTY (read before any F7 claim)

- **Tier 2a — explicit-method probe (works today):** a probe Lua mod calling
  `ExecuteInGameThread(closure, EGameThreadMethod.FuncPtrMarshal)` enqueues
  straight onto the marshal. Drain proof =
  `[Lua] [<mod>] <closure's print>` after the enqueue (the DRAIN-TRACE §1
  chain re-routed: enqueue → carrier invocation → publish gate →
  `process_simple_actions` → `lua_pcall`).
- **Tier 2b — the TownfallItemSpawner as-is is BLOCKED by a DIFF-1 ladder
  gap (§5.1):** F7 → `ExecuteInGameThread` with the default EngineTick method
  still attempts the lazy EngineTick detour and throws
  `EngineTick callback registration failed` in the write-refused world —
  before anything is enqueued. Do NOT score "marshal path" on F7 until §5.1's
  revision lands; a throw line there is EXPECTED behavior of the current
  revision, not a marshal failure.

### 3.4 Instrument semantics change (declare in acceptance notes)

Query-harness "two consecutive accepted reads" now counts CARRIER invocations,
not engine ticks (DIFF-2 rev-B relabel; R3D §4.4b). Prior "ticks"-phrased rung
evidence is not comparable. Menu latency becomes carrier-cadence-bound
(R3D §5.5); `ReceiveTick`'s aggregate rate is UNMEASURED (F-1 OPEN).

## 4. Log-lines NOT to expect (honest negatives)

- No `GameEngine::Tick address (vtable: ...)`-gated pump claim: the resolve
  walk still runs in this world (DIFF-2 compiles out the registrations, not
  the resolve) and its exec-page READ risk (DRAIN-TRACE C1) is UNCHANGED by
  the marshal path. If the walks die, the log ends at `Finding GameEngine::LoadMap`
  / `Finding GameEngine::Tick` — the marshal consumer never runs.
- No F7-driven `Menu created.` through the DEFAULT method (§3.3 Tier 2b).
- No `[R3] marshal not armed:` line in the R3 world (that line belongs to the
  W-works world).

## 5. Known follow-ups (distinct-ID revisions, none silently patchable here)

1. **The DIFF-1 ladder trap (BLOCKING for the spawner's default path).**
   DIFF-1's ladders keep `is_engine_tick_hook_available()` (address-availability)
   as the primary predicate; only the address-UNAVAILABLE branch falls to the
   marshal. In W_FAILS_R_WORKS the address IS assigned (vtable fallback), so
   `ensure_engine_tick_hooked()` runs and throws `EngineTick callback
   registration failed` at the first default-method ExecuteInGameThread — the
   marshal sits armed and unused. R3D §4.5 specified the opposite ordering
   ("availability predicate = carrier armed (module state), NOT an AOB
   is_ready() — the §3 trap"). Fix owner: DIFF-1 author/controller; a revision
   must re-order the PROSPERO ladders (marshal-first when armed) in
   ExecuteInGameThread + all three WithDelay-family ladders, with distinct IDs.
2. **The R3 world's remaining eager registrations.** After the consumer runs,
   `LuaMod::on_program_start`'s own `RegisterLoadMapPreCallback` still arms a
   detour and throws under BOTH_FAIL (the `TRY` catches; `start_lua_mods()`
   never runs; no Lua ever loads). DIFF-2 compiled out the initializer's
   EngineTick + FName registrations only; R3D §5.3 item 3's obligation for the
   LuaMod-side LoadMap registration is unowned. Until this revision lands, the
   R3-world boot still dies AFTER arming, loudly, at the LoadMap hook —
   meaning §3.1/3.2's oracle lines are reachable only in a world where the
   LoadMap detour happens to be installable, or after a scoped guard lands.
   This bounds what launch-11 can claim in-game; state it in the brief.
3. **In-game calibration (F-1).** cc1 census + R3D §4.2 rate/thread windows
   decide the carrier; the consumer's 2-entry ranked table is the bounded
   stand-in. Rank 3 (`Tick`) joins only after its owner is resolved.

## 6. Rollback

- **Pre-deploy (host):** the consumer revision is one staging-source swap.
  Rollback = point the replace receipt/driver back at the rebase staging
  (`4540593a…`); the compose comparator reverts to the dry-run's verified
  world (`633a8c82…`, `5595feb7…`). No other artifact changes.
- **Deployed module:** replace the staged prx with the previous staged build
  (files only; no engine-state revert exists). The FuncPtr store is
  process-lifetime: `UnregisterAllHooks` restores the original pointer
  (UFunction.cpp:121-132) but no unload-restore promise is made (VSD §4.5
  teardown honesty) — a restarted game loads the rolled-back build clean.
- **In-session behavior fallback:** if the carrier misbehaves at runtime, the
  marshal is not re-armable/disarmable at runtime by design ("no runtime
  re-arm path", DIFF-2 header); rollback is a rebuild, not a toggle.

## 7. Bounds remembered

One heavy build at a time; packaging serialized against launch-10; no failed/
reserved-job replay; controller owns serialized builds and canonical receipts;
every claim above carries its receipt under `receipts/`; nothing in this
runbook is satisfied by draft or context counts — the in-game oracle lines are
the only acceptance evidence, and F-1 stays OPEN until they exist.
