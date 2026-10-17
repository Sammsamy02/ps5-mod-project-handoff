# SPAWN-PLAYBOOK — the one-window operator card (first menu open + first spawn)

worker-spawn-playbook · 2026-10-05 · OPERATOR-CARD SYNTHESIS ONLY. Assembles already-landed
worker plans into one minute-by-minute card for the user's single comfortable window in a
FUTURE boot. **No code, no console contact, no game claims: every "expected" below is the
SHAPE of evidence to check, PENDING live proof. Nothing here certifies that any rung ever
passed.** One session, one pid; ordered ladder; stop claiming past the first unmet rung.

## Sources (each numbered step cites its owner)

- **pad timing**: worker-pad-defer `results/EVIDENCE-PLAN-INPUT-ARRIVES.md` (press protocol,
  verdict table, anti-noise rules) + `results/RETRY-PATH-AUDIT.md` §1 (post-open poll facts).
- **menu/spawn contract**: worker-menu-spawn `results/02-MENU-OPEN-INPUT.md` (six binds,
  success observables, integration contract) + `results/03-SPAWN-PRECONDITIONS.md` (DB hard
  gate, Antibiotics-first rationale, Failed-at-stage list).
- **bind map**: worker-real-input-pad `PS5Pad.ini` (OPTIONS=F7, CROSS=RETURN, UP/DOWN/LEFT/
  RIGHT = arrows) via worker-menu-input-map `RUNG-TABLE.md` R2/R4/R5 + discrimination ladder.
- **mod bytes**: canonical main.lua (package-v3, 21,415 B, identical in
  worker-settings-bundle-merge / worker-itemspawner-bundle / worker-menu-chain fixture);
  every `main.lua:<n>` pin below is against those bytes.
- **rungs/acceptance**: worker-acceptance `ACCEPTANCE-CHECKLIST.md` rows 3.1-5.5 + verdict
  logic; worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §2 C0-C5, §5, §6, §7.

**Input-tag honesty (read first):** there is NO per-press input tag in the composed runtime
today — gap **G-AP1** (worker-menu-input-map REPORT §4; checklist open items). A
`pad_edge_dispatch` one-shot or `PAD:` MIRROR line is PROPOSED, NOT STAGED (EVIDENCE-PLAN
last section). The only per-press observables are UE4SS.log mod lines and the on-screen UI.
The sdiag pad tags (`pad_available` etc.) are per-ACQUISITION, not per-press. "Zero new lines
on a press" therefore cannot by itself separate input-dead from drain-dead — §3 gives the
discrimination ladder that works around this.

---

## 1. Preconditions — what the boot evidence must show BEFORE any press is meaningful

All of these are read from the already-pulled evidence of the SAME pid (pid = max(pid in
sdiag); never mix pids). A press made while any of these is missing is unattributable noise.

| # | What must hold | Exact evidence (sink) | Source |
|---|---|---|---|
| P1 | Build identity: linked build contains pad_defer program `7b5d4636…` + pad_coexist four files + pool v5 + double-garble fix. Verify from the controller's staging receipt BEFORE launch | receipt (pre-launch) | NEXT-LAUNCH-RUNBOOK §3; checklist §0 |
| P2 | Boot deferral trio, in order, and NO `runtime_start_failed` | sdiag `pad_input_deferred`=6 → `pad_input_deferred_error`=2157051908 (`0x80920004`) → `pad_input_deferred_input_gate`=1 | EVIDENCE-PLAN step 2; checklist 3.1 |
| P3 | Retry liveness | sdiag `pad_open_class`=0 exactly once (first attempt), then `pad_retry`=1,2,3,… each with `pad_retry_failed`=0x80920004; gaps ~1 s doubling toward ~16 s cap (observation, not requirement). Deferral pair with NO `pad_retry` at all = stale fold → rung NOT EXERCISED (build identity), do not press-and-hope | EVIDENCE-PLAN step 3; checklist 3.2; runbook §7 row 16 |
| P4 | FINDING-1 absent | NO `pad_retry_failed` value ≠ 2157051908 anywhere in the pid. If present: NEW failure mode — capture exact value, reconcile, checklist HALTS (no presses count) | RETRY-PATH-AUDIT FINDING-1; checklist 3.4 |
| P5 | ARRIVAL (the gate for everything below) | sdiag `pad_available` = <handle>, **value > 0**, exactly once, t_ms strictly AFTER the last `pad_retry_failed`, and NO `pad_retry`/`pad_retry_failed` afterwards in the pid. `pad_available`=0 or out-of-order seq = corrupt capture, do NOT rate | EVIDENCE-PLAN step 4; checklist 3.3; runbook §7 row 24 |
| P6 | Mod loaded — the banner | UE4SS.log, exactly one fresh `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (main.lua:746); ZERO `Mod is absent from native Lua source` lines; no NUL-interleaved/garbled path lines. Banner present ⇒ all six RegisterKeyBind calls succeeded against the deferred-attached source | checklist 0.1-0.4; RUNG-TABLE R2; runbook §1 |
| P7 | Game-thread pump armed AND live | UE4SS.log `[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick (callback id 0)` AND `[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)`. Armed without pump-live proves nothing about drain latency (F-1). sdiag/status `engine-ready`/`mods-started` are init-thread acks only — NOT pump proof | checklist 2.3-2.5; runbook C3 |
| P8 | A save is LOADED, user in-game | User fact at the game screen (spawned pawn exists). Without it the FIRST press correctly yields the DB-gate soft-fail (§3b) — a valid negative test, not a tick | checklist 4.1(c); main.lua:64-67, :147 |
| P9 | Press hygiene | One controller paired; game console CLOSED (console open sets `allow_input`=false and silently swallows every key — G-AP3, silent); clean brief presses (edge-only dispatch: held buttons do not re-fire; duplicate-sample guard drops them by design) | RUNG-TABLE R4 + ladder step 3; EVIDENCE-PLAN anti-noise |

**Anti-noise that is NEVER evidence:** `IsKeyBindRegistered` output (returns false while
deferred); `pad_available`=0; host fixtures; re-presented historical artifacts; critic
scores. All claims cite one pid window.

---

## 2. The press sequence — minute by minute

Bind map for the operator (PS5Pad.ini, staged; main.lua:694-729 registered at load):

| Pad | Key delivered | Role |
|---|---|---|
| OPTIONS | F7 | toggle menu open/close |
| UP / DOWN | UP_ARROW / DOWN_ARROW | move the red `>> ` selection marker |
| CROSS | RETURN | activate highlighted row = SPAWN (or option cycle) |
| LEFT / RIGHT | LEFT_ARROW / RIGHT_ARROW | page prev / next |

Menu geometry once open (main.lua:541, :563-591, :375-422): dark-red `TOWNFALL : ITEM
SPAWNER` panel, top-right, 570x500; 12 rows — 1 `Quantity: 1`, 2 `Page: 1/N`, 3 `Refresh
pickup list`, 4-11 eight pickup slots, 12 status line; plus the controls line `UP/DOWN
SELECT | ENTER SPAWN | LEFT/RIGHT PAGE | F7 CLOSE`. Selection starts on row 1 (Quantity).
Non-exhaustive input warning: while the menu is open the same presses still reach the game
(mod compensates with SetIgnoreLook/MoveInput) — expect the character to possibly react; do
not treat that as failure.

### The sequence (t0 = the `pad_available` t_ms of this pid)

| Step | When | Press | Expected observable AFTER the press | Log line (UE4SS.log, same pid, t_ms ≥ t0) | Input-tag (sdiag) |
|---|---|---|---|---|---|
| W0 | t0 → t0+≥2 s | **wait** | game runs normally | nothing new | first post-open poll returns empty; NEXT poll swallows the baseline; first rising edge dispatches from the poll after (~5-10 ms post-open). The ≥2 s wait exists so the baseline swallow cannot eat press 1 (RETRY-PATH-AUDIT §1.5; EVIDENCE-PLAN step 5) |
| 1 | after W0 | **OPTIONS** | menu panel appears top-right (allow generous seconds — menu latency is carrier-cadence-bound and UNMEASURED, F-1; record press timestamps before declaring silence) | BOTH, in order: `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>` (main.lua:189) then `[Lua] [TownfallItemSpawner] Menu created.` (main.lua:643). N ≥ 1 required for a rung-4 tick | none (G-AP1). `pad_available` already fired at t0 and must NOT repeat |
| W1 | ~2 s | **wait + look** | read the panel: marker `>> ` on row 1 (Quantity); scan row texts for `Antibiotics` | press #2+ OPTIONS silence is SUCCESS (toggle mute by design, main.lua:648) — do NOT press OPTIONS again now; it would CLOSE the menu. The ≤3-press OPTIONS protocol below applies ONLY if press 1 produced silence (§3a) | none |
| 2..k | ~1 s apart | **DOWN** (repeat) | marker moves down one row per press; wraps bottom→top (main.lua:674-682). NO log lines, NO sdiag — silence here is BY DESIGN (nav binds are silent; menu-closed nav is a no-op, never read as pad-dead) | nothing (correct) | none |
| — | as needed | **LEFT/RIGHT** | page flips; row 2 label changes `Page: n/N`. Silent by design | nothing (correct) | none |
| S | marker on `Antibiotics` | **CROSS** | in-world pickup appears at the aim point (~150 units ahead on the ground) within ~1 s; panel row 12 becomes `Spawned Antibiotics x1` | `[Lua] [TownfallItemSpawner] Spawned Antibiotics x1 ID=<id>` (main.lua:334-335); `<id>` is a LIVE readback from the spawned actor's PickupInteractable after FinishSpawningActor and must be non-empty (main.lua:311-318) | none |
| C (optional) | after checking | **OPTIONS** | panel hides, look/move input unlocked | SILENCE = success (mute by design). After any map load the first press always logs again (RegisterLoadMapPreHook reset, main.lua:731-744) | none |

Navigation math for Antibiotics (derived from main.lua:72-79, :181-183 — items sort
alphabetically; only legacy entries `AA Batteries` and `Alcohol` can sort before it):
Antibiotics is ALWAYS on page 1 — pickup slot 1 if neither legacy entry resolved (3 DOWN
presses from row 1), slot 2 with one (4 DOWN), slot 3 with both (5 DOWN). Confirm visually
against the marker; never count presses blind. Antibiotics is the mandated FIRST spawn
attempt: static extra `/Game/Townfall/AlwaysCook/Items/Collectable_Items/
BP_Item_Antibiotics.BP_Item_Antibiotics_C`, least untested surface, host-smoke-proven chain.
Second attempt: any scan-surviving legacy entry; third: Axe (checklist 5.1).

**OPTIONS-press protocol reconciliation (the one trap):** the EVIDENCE-PLAN delivery
protocol is "up to 3 presses ~2 s apart" — that is for the case where press 1 produced
NOTHING. If press 1 opened the menu (the two lines + panel), delivery is already CONFIRMED;
further OPTIONS presses are toggles, not retries. Sequence: silence → OPTIONS again (~2 s) →
silence → OPTIONS a third time → still nothing = delivery UNRESOLVED, go to §3a. Do not
keep pressing past 3 total OPTIONS presses for delivery attribution.

---

## 3. Failure branches — exact log evidence and the next action

### (a) Menu didn't open on OPTIONS — zero new mod lines, no panel

Three suspects, discriminated in this order (worker-menu-input-map ladder; runbook §7 rows
18, 12-13; EVIDENCE-PLAN verdict table):

1. **Input-dead vs unresolved (R4):** `pad_available`>0 exists, banner exists, presses were
   after t0, and there are NO lines at all (no menu chain, no discriminator) after up to 3
   OPTIONS presses → delivery UNRESOLVED: **G-AP1 (no per-press tag) blocks attribution.**
   Do NOT re-launch unchanged; the fix is the proposed per-press tag (distinct-ID revision).
   Before concluding, exclude the silent gates: game console open (allow_input=false
   swallows all, G-AP3); mid-session pad disconnect; wrong user/pad (User=-1 takes the FIRST
   logged-in user); held-button re-presses dropped by the edge guard (clean brief presses
   only); nav binds pressed menu-closed are silent no-ops BY DESIGN.
2. **Drain-dead, input ALIVE (R3):** the press produced the discriminator line
   `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available
   (AOB scan failed)` (LuaMod.cpp:4719, fires at first press via main.lua:695) and/or the
   ExecuteInGameThread failure set (`Tried to install hook but function is unavailable.` /
   `Failed to add hook, detour installation likely failed!` / `EngineTick callback
   registration failed`). ANY of these is INPUT-RUNG DELIVERY (checklist 3.5) — input
   works, the game-thread drain does not. Capture verbatim; menu/spawn stay NOT-REACHED.
   If a `[WalkRxGuard] REFUSED` line exists anywhere, the candidate is a fold-0 build: the
   menu can NEVER open on it (record honestly).
3. **Menu-code-failed (R5):** a scan line appeared but no `Menu created.`/no panel:
   `Scan failed: <other>` = DB getters threw; `WidgetBlueprintLibrary not found.`
   (main.lua:501); `UI creation failed: <err>` (main.lua:636); PlayerController miss is
   SILENT (main.lua:494-497) — no panel AND no log. Also re-check ladder step 1: status must
   end `mods-started` and the banner must exist; missing → R1/R2 failure lists instead.

### (b) Item list empty / panel shows `---` in all pickup slots — the DB gate

`FindFirstOf("ItemDatabaseSubsystem")` (main.lua:64) is the HARD gate for the ENTIRE list —
including the six static extras, because `items` is assigned only after the throwing call
(main.lua:147/:185). Observable: menu still OPENS (panel + `Menu created.` may still log —
buildUI does not depend on items), every pickup row reads `---`, row 12 reads `Could not
build pickup list. Load a save first.`, and UE4SS.log has
`[Lua] [TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.`
(main.lua:66,193-194).

**What the user does next:** this soft-fail means drain + query machinery are ALIVE and the
save simply is not loaded. In-session recovery, repeatable, no re-launch, no re-stage:
(1) load a save at the game's own menu — note this triggers a map load, and the mod's
RegisterLoadMapPreHook resets items/UI state, so the FIRST OPTIONS press after the load logs
the scan line again (main.lua:731-744); (2) re-press OPTIONS; (3) re-check for the two
menu-open lines with N ≥ 1; (4) continue §2 from W1. The same "Load a save first" condition
also guards the spawn's `player` stage (main.lua:227-228) — without a pawn nothing can
spawn. Checklist effect: rung 4 is NOT ticked this press; the outcome is recorded as the T3
guard passing; rung 5 stays NOT-REACHED until a save is loaded and 4.2/4.3 appear (checklist
5.5; runbook §7 row 19).

### (c) Spawn attempted but failed — the Failed-at-stage lines

`[Lua] [TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` (main.lua:341-342).
The stage string names exactly which live precondition failed — record it VERBATIM together
with the `<err>` text; the failed actor is auto-destroyed (main.lua:344-346), so nothing is
left behind:

| stage (main.lua) | err shapes | meaning for the next iteration |
|---|---|---|
| `player` (:224) | `Load a save first.` / `Player controller unavailable.` | no pawn/controller — recover in-session (§3b), else controller-lookup issue |
| `ground trace` (:240) | `No ground ahead. Move to an open, flat area.` / `Surface too steep.` / `Ground trace returned no position.` | position problem (move the character) OR FHitResult OUT-param marshalling (rank-3 risk) — distinguish by trying open flat ground first |
| `load pickup class` (:279) | `Pickup class could not be loaded: <path>` | path absent/not resident in this build; LoadAsset silent-nil (asset-registry gap) — capture the exact path |
| `spawn actor` (:293) | `Actor creation returned invalid.` | engine-side refusal of BeginDeferredActorSpawnFromClass/FinishSpawningActor |
| `configure pickup` (:311) | `Spawned actor has no PickupInteractable.` / `Spawned pickup has no valid inventory ID.` / `Pickup mismatch: expected <id>, got <id>` | actor spawned but is NOT a live configured pickup — explicitly NOT a successful spawn |

Also capture: press timestamp, item, quantity, whether anything appeared on screen (eye
receipt into `game-evidence-receipt.json` with timestamps). A `Wait for the previous spawn.`
status (main.lua:216-218, busy guard, status-row only) means a spawn is still in flight —
wait, do not hammer CROSS. Next action rule: distinct-ID revision with the captured stage
evidence; never re-rate unchanged evidence, never re-press hoping for a different result.

---

## 4. End-of-session evidence capture — what gets pulled, in what order

ONE pull, one fresh curl per file, ~60 s after the boot settles (or earlier only if a
terminal refusal is already visible), into a fresh run dir. Never re-fetch to "catch" a
state: runtime-status is LATEST-STATE-ONLY (terminal O_TRUNC).

Order (NEXT-LAUNCH-RUNBOOK §6 step 1, LAUNCH-DAY-10 §3 order — the "five files"):

1. `UE4SS.log` (from `/data/plugins/`)
2. `runtime-status.txt` (from `/data/modbridge/roots/PPSA33286/`)
3. `modbridge-supplier-diag.jsonl` — **sink-absence check: absence of this file is NORMAL,
   record it and move on; it is not a failure**
4. `modbridge-exec-write-probe.jsonl` (drift guard)
5. `townfall_runtime_aligned_entry.log` (history guard)

Then, in order (runbook §6 steps 2-5):

6. Read durable evidence FIRST: `worker-durable-events/tools/epoch_view.py` over the sdiag
   (+ UE4SS.log + status + baseline bytes + build id) → one row per epoch; identify this
   boot's pid; save `epochs.json`.
7. The §6.3 grep set over UE4SS.log, filtered to the pid: `Loaded v1.0`, `Mod is absent`,
   `Seeds] REFUTED|Seeds] slot|FName pool validated`, `range-verdict resolution|Setting up
   mods|Finding GameEngine::LoadMap`, `\[R3\]`, `spawnable pickups found|Menu created|Scan
   failed|EngineTick`; sdiag one-off filter for `pool_range_verdict_fresh` + `pad_` tags.
8. NUL-interleave byte scan over the whole log (runbook §6.4 one-liner): any NUL-bearing or
   early-truncated line around the mod path = garble fix NOT in the build (build identity,
   rung NOT EXERCISED).
9. `evidence.sh` one-pager + drift guard (runbook §6.5): any CONFIG-DRIFT warning → STOP,
   verify the ini sha before attributing anything.
10. Pin THIS run's sdiag byte size + sha256 as the NEXT baseline cut (§6.5/§4).
11. Append the two eye/capture receipts to the run's `game-evidence-receipt.json` (menu
    VISIBLE 4.5; pickup actor VISIBLE 5.3), timestamp-correlated to the log lines — a
    screenshot/impression without the underlying file artifacts is not evidence.

In-session repeatability reminder (runbook §6): repeated OPTIONS/Lua-driven presses are
repeatable in-session and covered by the one pull; everything at `module_start` stage
(pool verdict, seeds, R3 arm, deferral/retry start) and runtime-status are NOT repeatable —
new boot-stage evidence needs a NEW epoch. There is no host→game command channel; no
in-session re-probes of boot rungs; no reloads this launch.

---

## 5. The honest GO/NO-GO ladder — what ticks each rung (acceptance rows 3.1-5.5)

Ladder is ordered; tick out of order does not count; stop claiming past the first unmet
rung; ALL ticks from ONE pull, ONE session, ONE pid; every downstream rung of an unmet rung
is recorded NOT-REACHED (an honest complete evidence run — still not the goal).

| Rung | TICKED (all required) | Combinations that are explicitly NOT a tick |
|---|---|---|
| 3 INPUT ARRIVES | 3.1 trio in order + no `runtime_start_failed` (§1 P2) AND 3.2 retry liveness with `pad_open_class`=0 exactly once (P3) AND 3.3 `pad_available`>0 once, after last `pad_retry_failed`, none after (P5) AND 3.4 satisfied: every `pad_retry_failed` = 2157051908 (P4) AND 3.5 ≥1 delivery line in L, same pid, t_ms ≥ `pad_available` t_ms (menu chain OR ExecuteInGameThread failure set OR the `ExecuteInGameThread: EngineTick…` discriminator). = **INPUT RUNG CONFIRMED** (scope: edge delivered to a registered Lua callback; menu/spawn separate) | `pad_available`>0 with no delivery line after verified presses = acquisition proven, delivery UNRESOLVED (G-AP1) — partial, not a tick; only `pad_retry*` = outcome B not exercised; any `pad_retry_failed` ≠ 2157051908 = checklist HALTS; `pad_available`=0 = corrupt capture, do NOT rate; deferral pair with no `pad_retry` = build identity, NOT EXERCISED |
| 4 MENU OPENS | 4.1 preconditions: banner (rung 0) + pump-live line (2.4) + save loaded/in-game (§1 P6-P8) AND 4.2 first-press `<N> spawnable pickups found. Pages=<M>` with N ≥ 1 AND 4.3 `Menu created.` in the same press window = menu-open LOG rung ticked. GOAL additionally requires 4.5 the on-screen panel as a receipt (dark-red `TOWNFALL : ITEM SPAWNER`, top-right, 12 rows + controls line), timestamp-correlated — **no log line substitutes for visibility** | The DB soft-fail `Scan failed: Item database not ready. Load a save first.` = T3 guard passing, NOT a tick (recover per §3b, then re-press); `Scan failed: <other>` / `WidgetBlueprintLibrary not found.` / `UI creation failed:` = R5 failures named; PlayerController miss = silent, no tick; ZERO lines on first press = rung 3/2 problem, use §3a ladder. 4.4 behavior (press #2+ silence = success) is corroboration, never a substitute for 4.2+4.3 |
| 5 ITEM SPAWNED | 5.1 navigation done and CROSS pressed once on Antibiotics (visible marker on the row; retry-safe) AND 5.2 `Spawned Antibiotics x<N> ID=<id>` with NON-EMPTY ID (live actor readback) AND 5.3 a NEW pickup actor VISIBLE in-world at the aim point within ~1 s, absent from the prior frame, eye/C4 receipt into `game-evidence-receipt.json` — 5.2 and 5.3 corroborate each other AND 5.4 satisfied: NO `Failed at <stage>.` line anywhere in the window | `Spawned` line WITHOUT a visible actor = investigate (spawned into unloaded/hidden space), NOT a pass; visible actor without the line should not be possible; any `Failed at <stage>` = non-tick, capture per §3c; `Wait for the previous spawn.` = not a spawn outcome at all |

Verdict logic (checklist): the GOAL = rungs 0-5 ALL ticked from the one pull — including
the two eye receipts (4.5, 5.3). A crash or a silent no-op (no `module_start_entered`, no
failure line, no status change) is FAIL regardless of progress; no rung is rated from that
pull. An honest partial run banks durable evidence and records the rest NOT-REACHED — that
is a success of the evidence policy and a non-goal verdict in the same breath. Global
non-evidence: host fixtures; re-presented history; `IsKeyBindRegistered`; `pad_available`=0;
edited main.lua (any edit voids rung 0's unchanged-bytes claim).

---

*Do-not-claim guard: this card defines what WOULD count and what the operator does at each
branch. It never certifies that any of it happened. Menu and spawn are never promised —
each is claimed only from its own lines in the same pid, plus its eye receipt.*
