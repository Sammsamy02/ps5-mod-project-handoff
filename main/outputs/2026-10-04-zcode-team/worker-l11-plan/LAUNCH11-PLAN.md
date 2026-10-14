# LAUNCH11-PLAN — the single ordered compose-and-launch plan (decision tree COLLAPSED)

Worker: worker-l11-plan · 2026-10-05 · r2. Supersedes this dir's prior same-day
attempt (which kept a three-branch W_WORKS/W_FAILS/window tree). LOCAL ONLY:
this is a plan, not a build — every command is a controller step; nothing here
was executed on the console or composed. Abbreviations: `TEAM=` 
`outputs/2026-10-04-zcode-team/`, `LM=TEAM:worker-luamod-rebase`,
`MI=TEAM:worker-l11-marshal-integration`, `EL=TEAM:worker-l11-enqueue-ladder`,
`LG=TEAM:worker-l11-loadmap-guard`.

## 0. Why the tree collapsed (the five fresh facts, each verified)

1. **MARSHAL IS THE ONLY PUMP PATH** (LG REPORT §4 + CLAIM E2/E4): live game
   text is never registry-backed, so the PLH detour layer cannot arm against it
   in ANY kernel world — the "W-works initializer owns the pump" branch
   (RUNBOOK §3.1's `marshal not armed: initializer owns the game-thread pump`)
   is unreachable as composed. **The plan has NO W_WORKS/W_FAILS pump fork.**
2. **Enqueue-ladder fix DELIVERED** (EL): throw site `LuaMod.cpp:4807`; staged
   `f50976ae1e2626b0…` via diff `3b19068a194ec82d…`; fixed object
   `8593884ea825d523…`; host 23/23 (armed = enqueued+drained; W_FAILS =
   enqueued+survives with loud stall log), control 14/14.
3. **LoadMap r2 defer DELIVERED** (LG): the arms fail SOFTLY as composed (no
   crash — the feared class does not exist there); r2 gates the eager arms on
   `R3MarshalPumpPending`; r2 = log hygiene (ONE Warning instead of ~19
   per-arm failure pairs) + explicit process-lifetime loss declaration; the
   mod's `RegisterLoadMapPreHook` reset never fires without the detour (stale
   cross-map state; menu-rebuild-on-reopen mitigation, §5.7 below).
4. **The compose chain is proven to the consumer object** (MI RUNBOOK +
   dryrun VERDICT): variant-B admission GO; comparator CHAIN
   `854a476bdd4f9985` (consumer base control, byte-matched 4x) →
   `8593884ea825d523` (with enqueue fix) / `3eeba02fd92efd99` (with loadmap
   r2). Both fix diffs apply to the SAME consumer base `04c9d91664dd82c7`
   (disjoint hunks: 5004–5143 vs 7321–8157).
5. **The launch-10 pad-defer fix (worker-pad-defer, IN FLIGHT) folds into the
   SAME lineage**: it is prerequisite for ANY runtime start surviving to mods
   (eager pad activation sits in the backend ctor BEFORE program init —
   `ue4ss_backend.cpp:21-26`, `Handler::init` throw `UE4SSProgram.cpp:731-734`).
   NO artifacts exist on disk yet (verified 2026-10-05): it enters the
   BACKEND/BOOTSTRAP unit lineage with its own diff+receipts+comparator —
   never the luamod stack. **Compose is BLOCKED on its delivery or an explicit
   recorded waiver.**

## 1. The ordered compose command chain (controller-owned, host-side, serialized)

One luamod replace-unit source, three stacked diffs, one initializer staging,
one driver. All shas sha256 (first 16 shown; verify full at open).

### A0 — Input sha gates (STOP on any drift; never edit pins)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
TEAM=outputs/2026-10-04-zcode-team; LM=$TEAM/worker-luamod-rebase

# INTEGRATION.md §1 set (unchanged):
shasum -a 256 $LM/build_runtime_composition6_fixed_l10lm.py   # 9de2c4c8ccfbbdf8  (fixed edition, UNMODIFIED)
shasum -a 256 $LM/build_runtime_composition6_l10lm.py         # 9b332fc56ededa74  (driver, UNMODIFIED — copy, never edit)
shasum -a 256 $LM/staging/initializer-l10-r3/UnrealInitializer.cpp  # 8b0c7e9905ce4a85  (armed fold, R3_MARSHAL_MODE=1 at :179)
shasum -a 256 $LM/staging/initializer-l10-r3/SeedTable.h      # 59effe6e4cd34eb0
shasum -a 256 $LM/receipts/composition6-replace-receipt.json  # 27c1b3b0dc8d646a
shasum -a 256 $TEAM/worker-prx-link-gaps/package_prx.py                          # e6fbe6c5011b498e (FROZEN)
shasum -a 256 $TEAM/worker-comp6-driver/build_runtime_composition6_fixed.py      # 8dd92a6fe4b71cf1 (FROZEN)
shasum -a 256 $TEAM/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp   # 75abf2ab77791f4e (REQUIRED --program-source)

# consumer lineage + the two L11 fixes:
shasum -a 256 $TEAM/worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp  # 04c9d91664dd82c7
shasum -a 256 $TEAM/worker-l11-marshal-integration/diffs/l11-marshal-consumer.diff                 # f595c57fb8f3ca10
shasum -a 256 $TEAM/worker-l11-enqueue-ladder/diffs/l11-enqueue-ladder.diff                        # 3b19068a194ec82d
shasum -a 256 $TEAM/worker-l11-loadmap-guard/diffs/l11-loadmap-defer-r2.diff                       # d2ad2818db4b7338

# single-fix reference objects (control law; hash-verify in each worker's work/):
#   EL work/control-el-luamod.o == 854a476bdd4f9985 | EL work/el-ladder-luamod.o == 8593884ea825d523
#   LG work/luamod-control-r2.o == 854a476bdd4f9985 | LG work/luamod-defer-r2.o   == 3eeba02fd92efd99
```

### A1 — Build the COMBINED luamod staging (NEW ids; where each diff enters)

All three LuaMod diffs enter the SAME unit (`luamod`), stacked on the consumer
base in ONE canonical order (proven order-independent — this plan applied both
orders in /tmp and got the identical bytes — but the order is fixed anyway):

```bash
MYD=$TEAM/worker-l11-plan   # or a NEW controller driver home; NEW ids throughout
mkdir -p $MYD/staging/luamod-l11-combined
cp $MI/staging/luamod-l11-marshal-consumer/LuaMod.cpp $MYD/staging/luamod-l11-combined/LuaMod.cpp
cd $MYD/staging/luamod-l11-combined
patch -p1 -i $EL/diffs/l11-enqueue-ladder.diff      # enters: ExecuteInGameThread ladder, hunks @5004-5143
patch -p1 -i $LG/diffs/l11-loadmap-defer-r2.diff    # enters: on_program_start eager arms, hunks @7321-7377 + @8100-8157
shasum -a 256 LuaMod.cpp   # expect dc68760253ec141f2eec4696050b8a2dd9f01b7f2894829d5f543516fb44f7f4
```

`dc68760253ec141f…` is this plan's host derivation (round-trip-checked; both
stack orders verified byte-identical); the controller re-derives and must match
it byte-for-byte before proceeding.

### A2 — Frozen-argv compile gate on the combined staging (the REAL comparator)

```bash
# the EL/LG frozen-argv pattern (EL work/argv-control-el.json / argv-el-ladder.json;
# prospero-clang++ -c, LLVM_CONFIG=TEAM/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config;
# only source/-o/-MF redirected from the 172-token argv)
#   control (fresh 04c9d916 copy)  -> MUST reproduce 854a476bdd4f9985 (5th independent reproduction)
#   combined staging               -> NEW object sha = THE launch-11 luamod.o comparator (record it)
#   both: exit 0, 0 errors, 23 baseline warnings kind-parity; llvm-nm undefined sets IDENTICAL
#   (zero new imports expected: both fixes are __PROSPERO__-guarded source-only edits;
#    R3MarshalPumpPending is already UND in the control, resolved by the initializer at link)
```

NEITHER `8593884e…` NOR `3eeba02f…` is valid for the combined unit — they are
single-fix references and remain the rollback comparators (§4).

### A3 — pad-defer admission gate (BLOCKING)

On worker-pad-defer's delivery: verify its staged diff + compile receipts +
its own frozen-argv object comparator, admit its unit through the replace/extra
receipt (`--file` admissions per its own DIFF), in the BACKEND/BOOTSTRAP
lineage. Absent at compose time = **STOP** (or an explicit, recorded controller
waiver that launch-11 accepts the eager-pad start hazard). Never a silent
omission; never patched into the luamod stack.

### B — Tagged driver copy (ONE edit, NEW id — the L10LM edit discipline)

Do NOT edit the delivered driver/fixed script. Make one tagged driver copy of
`build_runtime_composition6_l10lm.py` (`9b332fc5…`) with exactly the
`--replace-runtime-unit luamod=…` line re-pointed to
`$MYD/staging/luamod-l11-combined/LuaMod.cpp`; diff-verify the copy contains
nothing else. The fixed edition (`build_runtime_composition6_fixed_l10lm.py`,
`9de2c4c8…`, L10LM E0) is used UNMODIFIED — its admission set already admits
the retained `luamod` replace-unit (+ the affected six + retained `program`).

### C — Receipts, verify-only, two full passes (serialized; packaging included)

```bash
# replace-receipt revision: re-point the add-luamod-replacement-source entry at the
# combined staging via the documented regen mechanism (LM/regen_receipt_l10lm.py pattern;
# RUNBOOK §2 step 1). Admission set: combined luamod source (+ pad-defer unit files on
# delivery) via --file; every other pin unchanged (initializer 8b0c7e99, program 75abf2ab;
# provenance records the lineage diffs f595c57f + 3b19068a + d2ad2818).
python3 outputs/2026-10-04-zcode-team/worker-comp6-driver/regen_replace_receipt.py --program-source <staging> ...
python3 $MYD/<driver-copy>.py --verify-only
python3 $MYD/<driver-copy>.py \
  --composition-run <fresh-run-dir>/run-a --out <fresh-out-dir>/prx-candidate-l11p-a \
  --program-source outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp
# then run-b / prx-candidate-l11p-b (determinism twin). Fresh dirs per pass; the driver
# refuses pre-existing dirs. ONE heavy build at a time; exclusive of any launch-10
# packaging; one package_prx.py at a time. These passes INCLUDE packaging
# (package_prx.py e6fbe6c5 via the L10LM E4 in-memory injection).
```

### D — Compose acceptance (structural, path-independent)

- exit 0; `retained_units_replaced == ["luamod","program"]`; 368 units;
- run-a == run-b byte-identical (composed object, bare, wrapped fself);
- **`luamod.o == <the A2 combined comparator>`** (if the compose's staging
  basename differs and bytes drift, STOP and re-run the structural checks on
  the produced object — never silently widen the comparator);
- composed object: NEW sha (the dry-run's `633a8c82…` comparator is INVALID on
  this lineage — do not reuse); record size + the delta vs the dry-run world;
- chain-symbols 14/14; the final object's undefined set vs the r3-armed
  baseline: added exactly `["sceKernelGetModuleInfo"]`, removed 0, changed 0;
- **the three consumer UND symbols RESOLVE**:
  `RC::Unreal::R3MarshalPumpPending`,
  `RC::Unreal::R3MarshalPublishGameThreadIdentity`,
  `RC::Unreal::UFunction::GetFuncPtr` — they resolve against the armed-fold
  initializer (`8b0c7e99…`, mode 1) and the unreal-ufunction unit. Unresolved
  bridge symbol = wrong fold = STOP (the rev-B pairing law refusing loudly).

### E — Manifest + gate v2

Derive the composition-change manifest from the r3-armed/l10lm pattern
(INTEGRATION.md §5 / RUNBOOK §2 step 4): new `manifest_id`; initializer entry
re-pointed to `LM/staging/initializer-l10-r3/`; second entry
`{"unit":"luamod","role":"replace","source_paths":[$MYD/staging/luamod-l11-combined/LuaMod.cpp],
"sha256_after":{"…":dc687602…}}`; provenance records base `4540593a…` + diffs
`f595c57f…` (consumer), `3b19068a…` (enqueue), `d2ad2818…` (loadmap r2),
pad-defer diff `<TBD>`; `import_delta` empty both directions (backed by the A2
undefined-set identity + the D measurement); `resolution_b`/`d2_text_base`
verbatim. Then `worker-gate-evolution/gate_check_composition5_v2.py` in
manifest mode vs the frozen r11 baseline: expect the same 15/15 families.

## 2. The ini/window decision

- **ploader.ini: UNCHANGED.** The candidate replaces the composition slot's
  bytes (`/data/modbridge_supplier_composition.prx`) under the 2-plugin ini
  `1216aab6…` (composition=2 + path_test=2). The staging flow NEVER writes the
  ini: pre-image re-pin before, byte-verify `1216aab6…` after (LAUNCH-DAY-10
  §3 discipline). The 9-entry ini (`fd9c7393…`) stays SUPERSEDED history;
  Option P (`d6770f44…`, 149 B) still requires the layout owner's NEW
  pinned-bytes decision and is NOT part of this launch.
- **UE4SS-settings.ini: UNCHANGED.** Shipped v3 `d8faa4ec…` keeps
  `DefaultExecuteInGameThreadMethod=EngineTick`: the fixed ladder consumes that
  default and marshal-first overrides it when armed; flipping it would be an
  untested combination and a new revision.
- **Probe window: PARALLEL EVIDENCE, NOT A GATE.** The paired probe window
  (xwp row-0 rebuild `igd1-e030a09eb4d1` + fc1pool + path_test) still matters
  — it yields the D2/IGW verdict (needed for the walkguard fold-1
  admissibility per WST-5: `RC_ASM_WALK_RX_GUARD=1` only when seq5
  `rx_result.mprotect_rc==0`) and the D-1 verdicts — but with the pump fork
  gone (§0.1) NOTHING in this plan waits on it. Riding it in the same launch
  requires the probe ini pin (a separate owner decision); otherwise it stays
  host-side. cc1 adoption (`d83410aa…`) likewise stays a separate matrix §4-i
  decision — F-1 evidence comes from the oracle lines +
  carrier-invocation-relabeled counting, not from cc1.
- **Window discipline:** ONE fresh window per attempt, game fully CLOSED
  throughout; reachability G0 (`http://ps5-host-not-configured.invalid:8084/list_payloads`) +
  ini pre-image check before any staging; readback content law after; timing
  refusals (GRACE) = new window, never re-rated evidence; staging serialized
  against any other packaging.

## 3. The tiered in-game oracle (acceptance = these lines in UE4SS.log, nothing else)

Tier 0–1 are KEYPRESS-INDEPENDENT (engine-driven carrier; any ticking actor
drives `ReceiveTick`). Only Tier 2 needs exactly ONE OPTIONS press. Menu
navigation beyond the toggle is NOT a verdict input.

**Tier 0 — boot/ARM (no input).** Expected, in order (RUNBOOK §3.1):

```
[R3] refused: FName constructor verification detour (…)          <- DIFF-2, at its phase
[R3] refused: EngineTick pre-callback registrations (…)
[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal
Finding GameEngine::LoadMap                                      <- resolve walks still run (reads)
Starting Lua mod 'TownfallItemSpawner'
[R3] FuncPtr-marshal armed on carrier /Script/Engine.Actor:ReceiveTick (callback id 0)   <- THE ARM PROOF
[R3] LoadMap detour registrations deferred: …                    <- EXACTLY ONE (the r2 gate took)
```

Honest readings: the diagnostic variants `marshal carrier candidate not
found` / `marshal arm refused: carrier …` (precheck) / `marshal arm failed: no
ranked candidate passed` are evidence, not silence. A missing r2 Warning plus
per-arm LoadMap failure pairs = the gate did not take = compose drift → STOP.
The line `[R3] marshal not armed: initializer owns the game-thread pump` is
UNREACHABLE per §0.1 — if it EVER appears, that is an anomaly → reconcile, no
row. If the resolve walks die, the log ENDS at `Finding GameEngine::LoadMap` /
`Finding GameEngine::Tick` and NO `[R3]` marshal lines appear (the consumer
never runs) — the walks-survival precondition, honestly negative (§5.6).

**Tier 1 — pump live (no input).** First carrier invocation:

```
[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)
```

Absence WITH the armed line present = the carrier never actually fires — that
is F-1 rate evidence (OPEN), not a plan failure.

**Tier 2 — closure drain (ONE OPTIONS press; via the FIXED ladder).**
F7 → default-method `ExecuteInGameThread` → marshal-first (armed) → enqueued →
drained by the marshal pump: ≥1 NEW `[TownfallItemSpawner]` line (menu open),
and **ZERO** `EngineTick callback registration failed` throws (the old
Tier-2b blocker is fixed by the `8593884e…` lineage). In a marshal-armed world
a `[R3] ExecuteInGameThread: closure enqueued but the … pump is not registered`
stall line means the armed line was absent (world mismatch) — evidence, never
silence. The stall line is BY DESIGN loud in a refused world (volume trade
declared by EL).

**Tier 3 — explicit-method probe (optional, parallel, distinct-ID probe mod).**
`ExecuteInGameThread(closure, EGameThreadMethod.FuncPtrMarshal)` enqueues
straight onto the marshal (works today per RUNBOOK §3.3 2a); drain proof = the
closure's `[Lua]` print after the enqueue.

**Instrument semantics (declare in acceptance notes):** query-harness "two
consecutive accepted reads" counts CARRIER INVOCATIONS, not ticks; menu
latency is carrier-cadence-bound; `ReceiveTick`'s aggregate rate is UNMEASURED
(F-1 OPEN).

## 4. Rollback (ordered; every step is files-only)

1. **Per-fix, pre-deploy (host):** drop the diff from the stack and re-derive:
   combined → `04c9d916…`+enqueue (`8593884e…` world, comparator valid) →
   `04c9d916…` (consumer world, `854a476b…`) → rebase staging `4540593a…`
   (dry-run world, `5595feb7…` unit). The reverse pair (enqueue dropped,
   loadmap r2 kept) has NO measured object — if ever needed it is a NEW gate,
   never an assumption.
2. **pad-defer:** revert its own unit staging per its own DIFF — independent
   of the luamod stack.
3. **Deployed module:** replace the staged prx with the previous staged build
   (launch-10's last good), files only, readback content law, old-PRX `.bak`
   secured first. The FuncPtr store is process-lifetime: a restarted game
   loads the rolled-back build clean; no unload-restore promise is made.
4. **In-session:** the marshal is not re-armable/disarmable at runtime by
   design — a runtime misbehavior is a rebuild, not a toggle.

## 5. Honest unknowns (read before any claim)

| # | unknown | status |
|---|---|---|
| 1 | **F-1 carrier calibration** | OPEN. `ReceiveTick` aggregate rate unmeasured; the 2-entry ranked table is the bounded stand-in; armed-but-never-pumped is a live outcome (Tier 1 absence = the evidence). |
| 2 | **Five WithDelay bindings deferred** | `ExecuteInGameThreadWithDelay` (base 5108), `Retriggerable…` (5302), `ExecuteInGameThreadAfterFrames` (5457), `LoopInGameThreadWithDelay` (5517), `LoopInGameThreadAfterFrames` (5645) still arm-before-enqueue; closures via them from default-method paths stay losable in write-refused worlds; needs its own distinct-ID revision. The spawner's F7 path (single-arg `ExecuteInGameThread`, main.lua:694-696) IS covered. |
| 3 | **In-game behavior UNPROVEN** | every delivered piece is host-only (PS5 surface shimmed; `__PROSPERO__` host-defined in host tests); the console is untouched since launch-9; the oracle lines are the only acceptance evidence and none exist. |
| 4 | **Combined object comparator** | does not exist until A2 runs (`8593884e`/`3eeba02f` are single-fix references only); host-provisional SOURCE sha `dc68760253ec141f…`. |
| 5 | **pad-defer** | IN FLIGHT, zero artifacts on disk (no sha, no diff, no receipts); compose BLOCKED until delivered or explicitly waived (recorded). |
| 6 | **Resolve-walk survival** | the R3-armed world still runs the initializer resolve walks (DIFF-2 compiles out registrations, not the resolve); this chain links the shared prebuilt asmhelper (no walkguard step in it); walkguard fold absorption remains the probe-window-gated matrix decision (parallel, WST-5), and a walk refusal/death aborts boot BEFORE any `[R3]` marshal line. |
| 7 | **Stale cross-map state** | without LoadMap hooks the mod's per-map reset (`main.lua:731-747`) never fires: menu stays open across map loads with stale refs/caches until manually rebuilt (close/reopen); r2 declares the loss in ONE boot Warning — mitigation is behavioral, not fixed. |
| 8 | **Packaging determinism at canonical paths** | the dry run stopped before `build_overlay`/`package_phase` by mission bound; Stage C's passes are the first serialized packaging of this lineage. |
| 9 | **Stall-log volume** | in a permanently refused world the fixed ladder logs every refused-arm call (declared EL trade: "never silent"). |
| 10 | **Carrier rank 3 (`Tick`)** | joins only after its owner is resolved (RUNBOOK §5.3). |

## 6. What this plan does NOT claim

No in-game acceptance, no >8 quality, no completion, no kernel-behavior claim.
The matrix (LAUNCH11-MATRIX.md) remains the router for POST-evidence rows
(D-1, walkguard folds, backend arming); this plan owns only the ONE compose
lineage and its launch, with the probe window riding as parallel evidence.
Every cited worker's non-claims carry over unchanged.
