# REPORT — worker-l11-loadmap-guard (r2)

2026-10-05 · RESTART completion: the prior attempt was lost after staging +
compile gates + a drafted (never-built) host test. This round verified the prior
artifacts in place, CORRECTED their central claim, staged r2, built and ran the
host test, and wrote the deliverables. LOCAL ONLY throughout: no console contact,
no composition run, no packaging, no canonical-receipt writes, no other worker's
staging touched. Deliverable order honored (CLAIM.json → ANALYSIS.md → DIFF.md →
host-test/receipt.json → this REPORT.md last).

## 1. Verdict in five lines

1. The LuaMod-side LoadMap arm does **NOT** route through the folded WalkRxGuard
   site — and it is **NOT** an unguarded raw copy either. As composed (x64Detour
   `5bb9cea4` + ZydisDisassembler `facfea22` + read-lease OwnedMemory `d824528d`
   — the exact units in every launch-10/11 receipt), every PLH touch of live
   game text is mediated by the owned-memory registry, which soft-refuses
   non-owned extents with **zero dereference and zero syscalls**.
2. The feared igf6 crash class **does not exist on this path as composed**. Each
   eager arm in the R3 world costs one PLH SEV line + one Output Error line and
   an ERROR_ID that LuaMod ignores; the boot continues; Lua loads.
3. The real [WalkRxGuard] fold-0 refusal lives EARLIER — in the initializer's
   resolve walks — aborts UE4SS init loudly before any LuaMod arm, and is the
   actual cause of RUNBOOK §5.2's "no Lua ever loads" (not a LuaMod-arm throw;
   that RUNBOOK sentence is inaccurate for the composed layer).
4. The r2 staged fix (defer the eager arms while the initializer's bridge says
   pump_pending) is therefore NOT crash-safety; it closes R3D §5.3 item 3,
   replaces ~19 per-arm failure pairs per boot with ONE loud Warning, and makes
   the process-lifetime loss explicit. Compile-gated: control byte-identical to
   the marshal-integration lineage (`854a476b…`), r2 object NEW (`3eeba02f…`),
   undefined sets identical, round-trip proven.
5. What the unchanged mod loses without LoadMap hooks is real and now declared
   at boot: its `RegisterLoadMapPreHook` per-map reset (menu close, UI refs,
   item/ownedActor caches, pagination/selection/busy) is accepted Lua-side but
   never invoked.

## 2. Corrections this worker owns (raw prior reports unchanged)

- The first attempt's staged comment (r1, preserved) and drafted host-test
  header claimed PLH's 100-byte disassembly is a naked exec read ("kills the
  process (igf6 class) AFTER the marshal armed"). False for the composed units:
  reads go through `accessor.safe_mem_read` (ZydisDisassembler.cpp:65) →
  `OwnedMemory::copy` (read-lease :76-83), which refuses non-owned extents
  without dereferencing. r2's comment and Warning literal state the measured
  mechanism.
- DRAIN-TRACE's XD pin (`0619d2b4`, polyhook-sources-1 lineage; hook() at :322,
  no isGood gate) does not describe the build; every launch-10/11 composition
  receipt pins `5bb9cea4` (owned-default-detour lineage; hook() at :351, isGood
  gates at :491-492, owned rollback at :500-522). DRAIN-TRACE C1/C2's exec-
  read/exec-write characterization is superseded for the launch-11 base by
  CLAIM.json E4. Raw DRAIN-TRACE is preserved unchanged per contract.
- RUNBOOK §5.2's "arms a detour and throws under BOTH_FAIL (the TRY catches;
  start_lua_mods() never runs)" — as composed, the arm does not throw
  (AddCallback logs + ERROR_ID; LuaMod.cpp:7357 ignores it). "No Lua ever loads"
  remains true at fold-0, but from the earlier walk refusal (guard throw →
  initializer rethrow → module-entry failed ack), not from the LuaMod arm.

## 3. What was done this round (all receipts in place)

- Verified the walkguard fold-0 ship state and the mission premise (REPORT.md +
  CLAIM.json of worker-launch10-walkguard-prep: fold-0 object `81dd832b`,
  composed runtime `49fd49e7` byte-identical to launch-11 dry-compose).
- Traced the arm path end-to-end with pinned file:lines (CLAIM.json E1-E3).
- Re-verified the lost round's artifacts: r1 staging/diff shas, compile gates
  (0 errors), control object == `854a476b…` (byte-identical to the
  marshal-integration consumer object), undefined sets r1 == control.
- Staged r2 (corrected rationale + corrected Warning literal; gate code
  identical to r1), generated the deliverable diff, proved the round-trip, and
  re-ran the frozen-argv compile gate (control + r2).
- Built and ran the r2 host test against the real composed detour layer —
  5/5 phases, ASan clean (host-test/receipt.json): exec-only-denied → soft
  refusal + survival; readable-unregistered → registry-based refusal, zero
  mutation, still running; owned readable+writeable → REAL end-to-end arm
  (patch lands, trampoline runs original, unhook restores); write-refused R3 →
  isGood false, zero writes, survival; copy-refused → rollback, restored.
- Documented the unchanged mod's LoadMap loss from its actual main.lua and the
  API inventory.

## 4. Honest bounds / open items for the brief

- HOST-ONLY evidence: nothing here proves or disproves console behavior; the
  in-game oracle lines remain the only launch-11 acceptance evidence.
- Structural finding for the DIFF-1/carrier owner (recorded, not resolved
  here): live console game text is never registry-backed, so the PLH detour
  layer structurally cannot arm against it in any kernel world — the "W-works
  initializer owns the pump" oracle (RUNBOOK §3.1) is unreachable in the
  composed units as pinned; the FuncPtr-marshal is the only armed pump path.
  This sharpens, but does not change, the launch-11 R3-world plan.
- The r2 gate is reachable only in worlds where the walks were admitted (fold-1)
  or seeded; at fold-0 the boot aborts earlier, loudly. No runtime re-arm path:
  deferral is process-lifetime by design.
- F-1 (carrier rate) and the D2/IGW verdicts stay OPEN elsewhere; no claim here
  touches them.

## 5. Deliverable index (sha256 first 16)

| artifact | sha |
|---|---|
| `CLAIM.json` | `d1a66d9c69ed557e` |
| `ANALYSIS.md` | `67b627bc507f674c` |
| `DIFF.md` | `299dda787c019066` |
| `diffs/l11-loadmap-defer-r2.diff` (deliverable fix) | `d2ad2818db4b7338` |
| `staging/luamod-l11-loadmap-defer-r2/LuaMod.cpp` | `9a7a37d6c8377c02` |
| `work/luamod-control-r2.o` (== lineage `854a476b…`) | `854a476bdd4f9985` |
| `work/luamod-defer-r2.o` (NEW) | `3eeba02fd92efd99` |
| `host-test/test_detour_refusal_r2.cpp` | `b761074987aff6e5` |
| `host-test/test_detour_refusal_r2` (binary) | `39ff2a0cf52be88b` |
| `host-test/build_and_run_r2.sh` | `9f20c00a70e60534` |
| `host-test/receipt.json` | `acb473e318556fe6` |
| r1 preserved: `diffs/l11-loadmap-defer.diff` | `da8923ba345ba791` |
| r1 preserved: `staging/luamod-l11-loadmap-defer/LuaMod.cpp` | `d661e68fd32c1acd` |
| r1 objects preserved: `work/luamod-consumer-control.o` / `work/luamod-loadmap-defer.o` | `854a476bdd4f9985` / `131c8bafaf0de044` |
