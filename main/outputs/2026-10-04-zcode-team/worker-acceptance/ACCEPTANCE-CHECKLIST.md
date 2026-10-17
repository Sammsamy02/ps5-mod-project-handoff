# ACCEPTANCE-CHECKLIST — the definitive per-rung tick-list for "goal reached"

Worker: worker-acceptance · 2026-10-05 · SYNTHESIS ONLY. This document assembles the
final acceptance checklist from already-landed worker evidence plans. No code, no
console contact, no game claims: **every tick box below is PENDING live evidence;
nothing here certifies that any rung ever passed.**

**End goal being adjudicated:** the Item Spawner menu VISIBLE and an item ACTUALLY
SPAWNED, in-game, evidenced. Goal = ALL rungs below ticked from ONE pull of ONE
session's console evidence, same pid throughout (§ Verdict logic).

---

## 0. How to use this checklist

1. **ONE pull.** One fetch per file, ~60 s after the boot settles, in the
   NEXT-LAUNCH-RUNBOOK §6 order, into a fresh run dir:
   `UE4SS.log` → `runtime-status.txt` → `modbridge-supplier-diag.jsonl` →
   `modbridge-exec-write-probe.jsonl` (drift guard) →
   `townfall_runtime_aligned_entry.log` (history guard). Every tick cites these
   pulled files — never a live console, never a re-fetch to "catch" a state
   (runtime-status is LATEST-STATE-ONLY, terminal O_TRUNC).
2. **ONE pid.** Read the durable evidence FIRST with `worker-durable-events`
   `epoch_view.py` (one row per epoch = one game pid; new epochs are `[NEW]`).
   ALL ticks use only records with `pid = max(pid in file)`, in seq order; UE4SS.log
   lines are filtered to the same pid (`[t=<ms> pid=<pid>]` prefixes); UE4SS delivery
   lines count only with t_ms ≥ the `pad_available` t_ms of the SAME pid. Never mix
   pids; never subtract t_ms across epochs or across writers.
3. **Ordered ladder.** Stop claiming past the first unmet rung (NEXT-LAUNCH-RUNBOOK
   §2). A rung left unticked means every downstream rung is recorded NOT-REACHED —
   that is an honest complete evidence run, but NOT the goal.
4. **Sink abbreviations.** `S` = `/data/modbridge/roots/PPSA33286/runtime-status.txt`
   (runtime-status; five-emission ack sink, states exactly `activated` →
   `engine-ready` → `mods-started`, terminal `stopped`/`failed`). `L` =
   `/data/plugins/UE4SS.log` (mod/Lua lines carry `[Lua] [TownfallItemSpawner] `).
   `D` = `/data/modbridge-supplier-diag.jsonl` (sdiag records
   `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":…,"value":V,"t_ms":T}`;
   cross-sink t_ms agreement exact, ≤1 ms skew observed).

### Build-identity precondition (before ANY tick can mean what it says)

The full tag set only exists on a build containing ALL admissions: pad_defer
program `7b5d4636…` + the pad_coexist four files (wpd2 retry) + pool v5 (mincore
probe, non-image-only span publication, `failure_class`, `resolve_listing_gap`) +
the double-garble fix (widen loop + qualified `modbridge::normalize_path_for_lua`).
Verify from the controller's staging receipt BEFORE launch. Symptoms of a stale
fold (deferral pair with no `pad_retry`; garbled/NUL-interleaved mod path) are
"NOT EXERCISED — build identity", never a route failure. (Defined by
worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §3.)

---

## G. GAME-IDENTITY preamble (must tick before any rung row)

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| G1 | S line 1: `session\t<pid>\t<epoch_ms>` at the Townfall title root `/data/modbridge/roots/PPSA33286/runtime-status.txt` | Present; the pid is THE checklist pid used by every row below; session is fresh per the O2 oracle (session-ms sane vs monotonic base) | worker-runtime-ack C3 shape + freshness oracles via worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` §1 and worker-menu-input-map `RUNG-TABLE.md` sink table |
| G2 | L: `Using engine version: 5.6` (PATTERN — `<maj>.<min>` plausible-not-pinned; UnrealInitializer.cpp:1678, profile `townfall-ps5-ue56`) | Present once in the launch pid after seeds; engine identity of the pulled log confirmed | worker-launch10-acceptance-criteria `ORACLE.md` F24 + worker-post-seeds-chain `CHAIN.md` S2 |
| G3 | L line prefixes `[t=<ms> pid=<pid>]`; D records `pid` field; `epoch_view.py` `[NEW]` row | G1's pid = the max pid in D = the pid filter for all L greps; one epoch only; no t_ms arithmetic across epochs | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §6 (pull procedure, same-pid rule) |

**Naming honesty:** the task brief calls G1/G2 the "gamedata line". No landed worker
output defines an artifact by that exact name. The nearest DEFINED identity lines are
G1 (session/pid at the game's data root) and G2 (engine-version line). `worker-runbook-v2`
has NOT landed; when it does, it must ratify (or rename) this preamble row.

---

## Rung 0 — LOAD (the one question: does the mod load?)

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 0.1 | L, exactly ONE fresh line: `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (main.lua:746) | Byte-exact: 87 bytes incl. trailing `\n`, 0 NUL bytes, pure ASCII, contains `Loaded v1.0` and `F7` (reference capture `worker-banner-smoke/results/banner_capture.bin`, sha256 `9fd8410a664d27e0fd21d4139af7827499b002e17c5cb4aec27c01648a517369`). Exactly once for the pid. This banner is the ONLY mod-executed proof (`mods-started` does NOT prove it) | worker-banner-smoke `BANNER-SMOKE.md` (expected bytes) + worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §1 |
| 0.2 | L, absence check: `Mod is absent from native Lua source` (LuaMod.cpp:941 emission) | ZERO occurrences for the launch pid AND every mod-path print for that pid is CLEAN (no NUL-interleaved garble, no early-truncated line) | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §1 + worker-post-seeds-chain `CHAIN.md` §0.1 |
| 0.3 | Six `RegisterKeyBind` calls (main.lua:694-729) — NO per-bind runtime tag exists; the boot evidence is the banner itself: load completing past :694-729 proves all six succeeded (the Handler throws `Input registration unavailable: no active input source` (HANDLER:109) into the script-error path otherwise, and then NO banner would exist) | Banner (0.1) present ⇒ 6/6 binds registered against an active input source. Host-fixture expectation for the record: exactly 6 `RegisterKeyBind(<function>)` with KeyDef.hpp codes F7=0x76, UP=0x26, DOWN=0x28, RETURN=0x0D, LEFT=0x25, RIGHT=0x27 + 1 `RegisterLoadMapPreHook` | worker-banner-smoke `BANNER-SMOKE.md` (fixture case table) + worker-menu-input-map `RUNG-TABLE.md` R2 "hidden rung proof" + worker-menu-spawn `02-MENU-OPEN-INPUT.md` §integration req 1 |
| 0.4 | L whole-file NUL-interleave check (byte scan over the pulled log) | 0 NUL-bearing lines for the launch pid; any NUL/truncated line around the mod path = garble fix NOT in the linked build (build identity; rung NOT EXERCISED) | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` §6 step 4 |

Documented non-tick shapes (record verbatim, from NEXT-LAUNCH-RUNBOOK §7 rows 10-11):
`Mod is absent` with a CLEAN path → source-registration audit (escalate);
`Failed to execute main script: …` / `Exception during mod startup: …` → capture + the
0.4 NUL result, reconcile before any revision. Never re-press or re-launch unchanged.

---

## Rung 1 — POOL + SEEDS (cascade step C1)

> Attribution: `worker-runbook-v2` has NOT landed (no such directory). Per the task
> brief's fallback, this rung is defined from the **pool-v5 tags**:
> worker-post-seeds-chain `CHAIN.md` (receipt-pinned lines) + worker-launch-readiness
> `NEXT-LAUNCH-RUNBOOK.md` cascade step C1.

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 1.1 | L: `[Seeds] Required-slot seed table armed (slots 0/2/3/4); …` (UnrealInitializer.cpp:1153). Ordering guard: `Setting up mods...` (ctor-time, :1811) precedes ALL `[Seeds]` lines — if a fetched log shows seeds first, re-check raw timestamps before believing it | Present once, in the correct order, for the pid | worker-post-seeds-chain `CHAIN.md` §1 + `NEXT-LAUNCH-RUNBOOK.md` §5 rows 2/4 ordering quirk |
| 1.2 | L, the five seeds-bind lines in EXACT order: 1) `[Seeds] slot 0 g_uobject_array bound …` (:858) → 2) `[Seeds] slot 3 gmalloc bound … V1 instance + V2 vptr match` (:909) → 3) `[Seeds] FName pool validated at 0x9765EC0 …` (:1038) → 4) `[Seeds] slot 2 FName ctor bound …` (:1063) → 5) `[Seeds] slot 4 static_construct_object_internal bound` (:1080) | All five present, in that order, no refusal between them | worker-post-seeds-chain `CHAIN.md` §1 table (receipt-pinned) |
| 1.3 | D tag: `pool_range_verdict_fresh` ∈ {0,1,2,3} — emitted ONCE per boot, on the first listing-gap miss only | Present (or proven not-required because no listing-gap miss occurred). 0=not-mapped, 1=mapped, 2=unavailable, 3=invalid request | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` C1 (pool v5 policy) |
| 1.4 | L: NO `[Seeds] REFUTED: …` line; S does NOT end `failed` carrying seed-refusal text | Pool validated + all five binds ⇒ rung 1 TICKED. A refusal aborts init (throw propagates; `ack->failed(1, "[Seeds] REFUTED: …")`) — it CANNOT coexist with a healthy startup, so the 0.1 banner and every downstream rung are NOT-REACHED | worker-post-seeds-chain `CHAIN.md` §1 Q1 + §2 (seeds-refusal→failed path) |
| 1.5 | Honest-refusal branch (non-tick documentation row): L `[Seeds] REFUTED: FName pool at 0x…: <failure> (…; range-verdict resolution: <note>)` + D `pool_range_verdict_fresh` value | If refusal: record the verdict value + note verbatim. 2 → syscall unsupported, STOP the mincore route; 1 → mapped-but-unreadable, EXPECTED today, bank evidence; 0 → not-mapped, seed-table derivation audit; 3 → instrumentation bug, pool verdict NOT RATED this boot. Stop claiming further rungs | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` C1 + §7 rows 1-5 |

---

## Rung 2 — ENGINE / PUMP (cascade step C2)

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 2.1 | L: `Finding GameEngine::LoadMap` (UnrealInitializer.cpp:1934/1877) — the first exec-text walk; the walkguard gate outcome line. Under a refusing walkguard this is the LAST progress line before fail-closed shutdown | Walk PERMITTED: the line is traversed (later progress lines exist) and there is NO `[WalkRxGuard] REFUSED` in L and NO `guard_off_no_rx_verdict` in the S failure detail. A refusal (GATE-1) kills the whole serialized startup — everything downstream NOT-REACHED this boot | worker-engine-pump-chain `CHAIN.md` M1/GATE-1 + worker-menu-input-map `RUNG-TABLE.md` R3 + `NEXT-LAUNCH-RUNBOOK.md` §7 row 20 |
| 2.2 | L, the R3-refusal pair: `[R3] refused: FName constructor verification detour (…)` (:2039) and `[R3] refused: EngineTick pre-callback registrations …` (:2039-2040), followed by `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` (:2040) | All three present in order (the designed DIFF-2 compile-out shape). These refusals are EXPECTED and good — they prove the R3 marshal world is armed | worker-post-seeds-chain `CHAIN.md` S5 + worker-engine-pump-chain `CHAIN.md` M3 |
| 2.3 | L, marshal ARM proof: `[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick (callback id 0)` | Present once. Failure variants recorded verbatim as non-ticks: `marshal carrier candidate not found` / `marshal arm refused: …` / `marshal arm failed: no ranked candidate passed …` (marshal route owns the failure; pump rung NOT EXERCISED) | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` C3/§7 row 8 + worker-engine-pump-chain `CHAIN.md` M3 |
| 2.4 | L, pump-LIVE proof: `[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)` — fires on FIRST carrier invocation | Present at least once in the pid. Armed-without-pump-live proves nothing about drain latency (F-1 open) — do not claim drain capability | worker-launch-readiness `NEXT-LAUNCH-RUNBOOK.md` C3 + §7 row 7 |
| 2.5 | S: `engine-ready` ack (state 2; on_engine_initialized chain, ue4ss_backend.cpp:33-45). S is LATEST-STATE-ONLY — interpret only against G1's epoch pid | Present (as far as a terminal `failed`/`stopped` line allows reading the ladder). engine-ready is an init-thread fact, NOT game-thread proof by itself | worker-runtime-ack C3 shape via worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` CP2 + worker-acceptance-evidence `EVIDENCE-SPEC.md` §0.1 |
| 2.6 | Event loop — NO direct tag exists. Proxy: L `Starting mods …` / `Mod 'TownfallItemSpawner' … starting mod.` (event-loop jthread PROG:673 → on_program_start PROG:1514 → start_lua_mods PROG:1488; ~5 ms UpdateThread cadence) | Those mods-start lines exist for the pid ⇒ the loop that alone emits them is alive. They must appear AFTER all seeds/engine-ready work, never before | worker-engine-pump-chain `CHAIN.md` M2 + `NEXT-LAUNCH-RUNBOOK.md` §5 rows 12/14 |

---

## Rung 3 — INPUT ARRIVES (cascade step C4)

> Attribution: the task's `worker-controller-input` directory has NOT landed. The
> evidence plan it names exists and is landed under **worker-pad-defer**
> (`results/EVIDENCE-PLAN-INPUT-ARRIVES.md` + `RETRY-PATH-AUDIT.md`); this rung uses
> that plan plus `NEXT-LAUNCH-RUNBOOK.md` C4. No content gap — attribution gap only.

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 3.1 | D boot-deferral trio, in order: `pad_input_deferred`=6, `pad_input_deferred_error`=2157051908 (`0x80920004`), `pad_input_deferred_input_gate`=1; and NO `runtime_start_failed` | All three present in the pid; startup continued | worker-pad-defer `EVIDENCE-PLAN-INPUT-ARRIVES.md` §durable tags + reading step 2 |
| 3.2 | D retry liveness: `pad_open_class`=0 EXACTLY ONCE (first attempt), then `pad_retry`=1,2,3,… (monotonic) with `pad_retry_failed`=0x80920004 per failed attempt; t_ms gaps growing ~1 s doubling toward the ~16 s cap (cadence is an observation, not a pass requirement) | Retry machinery alive per plan step 3. Deferral pair present with NO `pad_retry` at all = linked binary predates the coexist fold → rung NOT EXERCISED (build identity) | worker-pad-defer `EVIDENCE-PLAN-INPUT-ARRIVES.md` §retry + verdict row "deferral pair present, NO pad_retry" |
| 3.3 | D ARRIVAL tag (THE tick): `pad_available` = <handle>, **value > 0**, exactly once per acquisition, t_ms strictly AFTER the last `pad_retry_failed`, and NO `pad_retry`/`pad_retry_failed` afterwards in the same pid | Exactly that. `pad_available`=0 or out-of-order seq = impossible by construction → capture corrupt, do NOT rate | worker-pad-defer `EVIDENCE-PLAN-INPUT-ARRIVES.md` §ARRIVAL + §anti-noise rules |
| 3.4 | FINDING-1 discriminator: any `pad_retry_failed` value ≠ 2157051908 | MUST BE ABSENT for a tick. If present: NEW failure mode (prime suspect: repeated `scePadInit` already-initialized class at phase 3) — capture the exact value and reconcile before ANY re-launch (checklist halts) | worker-pad-defer `RETRY-PATH-AUDIT.md` FINDING-1 + `EVIDENCE-PLAN-INPUT-ARRIVES.md` verdict row 4 |
| 3.5 | Delivery corroboration, L same pid with t_ms ≥ the `pad_available` t_ms (press protocol: wait ≥2 s post-arrival — the first post-open poll swallows the baseline — then OPTIONS, up to 3 presses ~2 s apart): EITHER the menu chain (`… spawnable pickups found. Pages=<M>` / `Menu created.`) OR the ExecuteInGameThread failure set (`Tried to install hook but function is unavailable.` / `Failed to add hook, detour installation likely failed!` / `EngineTick callback registration failed`) OR the R3 discriminator `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` | ANY ONE of those lines = INPUT RUNG CONFIRMED (scope: edge delivered to a registered Lua callback; menu/spawn rungs separate). `pad_available`>0 with NO delivery line: acquisition proven, delivery UNRESOLVED — G-AP1 (no per-press tag) blocks attribution; do not re-launch unchanged. Anti-noise: `IsKeyBindRegistered` is NOT evidence; value 0 is not a handle | worker-pad-defer `EVIDENCE-PLAN-INPUT-ARRIVES.md` §delivery + verdict table + `NEXT-LAUNCH-RUNBOOK.md` C5/§7 rows 12-13 |

---

## Rung 4 — MENU OPENS (cascade step C5, the F7 press chain)

> Attribution: the task's `worker-menu-chain` directory has NOT landed. The boundary
> list it names is assembled here from the landed plans: worker-menu-input-map
> `RUNG-TABLE.md` R5 + first-press discrimination ladder, worker-menu-spawn
> `02-MENU-OPEN-INPUT.md`, worker-engine-pump-chain `CHAIN.md` M5, and
> `NEXT-LAUNCH-RUNBOOK.md` C5. No content gap — attribution gap only.

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 4.1 | Menu-toggle preconditions, all provable from the already-pulled evidence: (a) rung 0 banner (6 binds live); (b) rung 2.4 pump-live line (else the first press can only emit the drain-dead discriminator — see 3.5); (c) a save LOADED and user in-game (user fact; main.lua:62-67 gates the DB find on it) | All three hold before the press is counted. Without (c) the correct outcome is the T3 guard shape below — which is a valid negative test, NOT a rung-4 tick | worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` §4.0 P1-P5 + worker-menu-input-map `RUNG-TABLE.md` R5 |
| 4.2 | L, first OPTIONS press of the session (or after a map load), FIRST success line: `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>` (main.lua:189; N>0 requires the ItemDatabaseSubsystem find to have resolved) | Present once, for the pid, after the 4.1 preconditions; N ≥ 1 | worker-menu-spawn `02-MENU-OPEN-INPUT.md` §exact success observables + `NEXT-LAUNCH-RUNBOOK.md` C5 |
| 4.3 | L, SECOND success line, in order after 4.2: `[Lua] [TownfallItemSpawner] Menu created.` (main.lua:643) | Present once, immediately following 4.2's line in the same press window. 4.2+4.3 together = menu-OPEN log rung ticked | worker-menu-spawn `02-MENU-OPEN-INPUT.md` + worker-engine-pump-chain `CHAIN.md` M5 |
| 4.4 | Press #2+: silence in L is SUCCESS (toggle-close is mute by design, main.lua:489-491; rescan only when `#items==0`, :651; after a map load the first press always logs again, RegisterLoadMapPreHook reset :731-744). Nav binds menu-closed are silent no-ops by design (:699-723) — never read as "pad dead" | No new lines on second press; menu visibly closes; input unlocked | worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` §4.1/§4.4 T4 + worker-menu-input-map `RUNG-TABLE.md` R5 |
| 4.5 | THE GOAL'S VISIBILITY HALF: the menu overlay on screen — dark-red `TOWNFALL : ITEM SPAWNER` panel, top-right (570x500), 12 row texts + controls line; user's eyes. Recorded as a receipt in the run's `game-evidence-receipt.json` (C4 capture or eye receipt + timestamps); a screenshot/impression without the underlying file artifacts is not evidence | User (or continuous C4 capture) confirms the panel, correlated in time with lines 4.2/4.3 in the same pid window. **No log line substitutes for visibility** | worker-menu-spawn `02-MENU-OPEN-INPUT.md` (panel spec) + worker-post-spawn-closure-prep `ACCEPTANCE-RECORD.md` §1.5 (eyes-as-receipts law) + worker-acceptance-evidence `EVIDENCE-SPEC.md` §0.2.4 |

Documented non-tick shapes for rung 4 (each names its precondition; from
`02-MENU-OPEN-INPUT.md` + RUNG-TABLE R5 + NEXT-LAUNCH-RUNBOOK §7 rows 18-19):
`Scan failed: Item database not ready. Load a save first.` = drain+query alive, no save
(load one in-session, re-press — repeatable); `Scan failed: <other>` = DB getters threw;
`WidgetBlueprintLibrary not found.` / `UI creation failed: <err>` = UI-build failure, name
the call; PlayerController miss is SILENT (:494-497) — no panel, no log; ZERO new lines on
first press = rung 3 delivery or rung 2 drain failure (use the first-press discrimination
ladder; the 3.5 R3-discriminator line means input ALIVE + drain DEAD).

---

## Rung 5 — ITEM SPAWNED (the CROSS path)

> Attribution: worker-menu-spawn (`results/03-SPAWN-PRECONDITIONS.md`,
> `01-API-AUDIT.md`) + worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` §5.

| # | Evidence artifact (file : line/tag) | Pass condition | Defined by |
|---|---|---|---|
| 5.1 | In the open menu: navigate UP/DOWN (rows), LEFT/RIGHT (pages; no per-move log lines exist — expect none) to **Antibiotics** and press CROSS (= RETURN, main.lua:708 → activateSelection → spawnSelected inside ExecuteInGameThread). Antibiotics is the recommended FIRST attempt: static extra entry `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics.BP_Item_Antibiotics_C` (main.lua:72-79 normalized by :122-136), least untested surface, host-smoke-proven chain. Second attempt: any scan-surviving legacy entry; third: Axe | Selection happened (visible); CROSS pressed once (retry-safe: guards + pcall) | worker-menu-spawn `03-SPAWN-PRECONDITIONS.md` §pre-verifiable + §checkable-once-alive checklist item 3 + `MASTER-VERIFICATION-RUNBOOK.md` §5.1 |
| 5.2 | THE SPAWN LINE: `[Lua] [TownfallItemSpawner] Spawned Antibiotics x<N> ID=<id>` (format `Spawned <ItemName> x<quantity> ID=<inventoryID>`, main.lua:333-334). The `ID=` value is read back LIVE from the spawned actor's `PickupInteractable` component AFTER `FinishSpawningActor` — a real-actor readback, not a constant (main.lua:308-335) | Present once per successful spawn, same pid; non-empty `ID=` (a non-empty ItemId is itself a spawn gate at :316). The in-menu status line also switches to `Spawned <Item> x<N>` | worker-menu-spawn `03-SPAWN-PRECONDITIONS.md` + worker-engine-pump-chain `CHAIN.md` M6 + `MASTER-VERIFICATION-RUNBOOK.md` §5.3 |
| 5.3 | THE GOAL'S SPAWN HALF: a NEW pickup actor visible in-world at the aim point (ground-trace hit), absent from the immediately-prior frame, within ~1 s of CROSS; user walks to it. Operator count table: press # → item id → frame timestamp (C4 capture / eye receipt into `game-evidence-receipt.json`) | Log line (5.2) AND visible actor corroborate each other. A `Spawned` line WITHOUT a visible actor = investigate (spawned into unloaded/hidden space), NOT a pass; a visible actor without the line should not be possible (log is written from the post-readback path) | worker-menu-visibility-runbook `MASTER-VERIFICATION-RUNBOOK.md` §5.3 + worker-post-spawn-closure-prep `ACCEPTANCE-RECORD.md` §1.5 |
| 5.4 | Failure discriminator (non-tick): `[Lua] [TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` (main.lua:341-342), stage ∈ {player (:224), ground trace (:240), load pickup class (:279), spawn actor (:293), configure pickup (:311)} — the stage string names exactly which live precondition failed (ground trace = FHitResult OUT marshalling, rank-3 risk; load pickup class = path absent in this build; spawn actor = engine-side refusal; configure pickup = actor spawned but NOT a live configured pickup — explicitly NOT a successful spawn) | MUST BE ABSENT for the tick. Any occurrence: record verbatim, distinct-revision fix, never re-rate unchanged evidence | worker-menu-spawn `03-SPAWN-PRECONDITIONS.md` + `MASTER-VERIFICATION-RUNBOOK.md` §5.4 stage table |
| 5.5 | HONEST SOFT-FAIL FALLBACK (ItemDatabaseSubsystem gate): `FindFirstOf("ItemDatabaseSubsystem")` (main.lua:64) is the HARD gate for the ENTIRE menu list — including the 6 static extras, because the items assignment sits after the throwing call (:147/:185). Documented soft-fail line: `[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.` (main.lua:66,193-194); if the DB is found but getters throw: `Scan failed: <other>`. Recovery is in-session: load a save, re-press OPTIONS (repeatable in-session; no re-launch, no re-stage) | If this line appears instead of 4.2: rung 4 is NOT ticked this press; the outcome is recorded as the T3 guard passing (drain + query machinery alive). The checklist does NOT advance to rung 5 until a save is loaded and 4.2/4.3 appear | worker-menu-spawn `03-SPAWN-PRECONDITIONS.md` (hard-gate + live-game list items 1-2) + `NEXT-LAUNCH-RUNBOOK.md` §7 row 19 + `MASTER-VERIFICATION-RUNBOOK.md` §4.3(c) |

---

## Verdict logic — what "goal reached" means

1. **Goal = ALL of G, rungs 0-5 ticked, from ONE pull, ONE session, ONE pid.** Any tick
   from a different pid, a different epoch, a different build identity, or a host fixture
   is void. Rungs tick out of order do not count (ordered ladder; stop claiming past the
   first unmet rung).
2. **The goal statement is stronger than the log rungs.** Rungs 4.2+4.3 and 5.2 are log
   ticks; the GOAL additionally requires the two eye/capture receipts: menu VISIBLE (4.5)
   and pickup ACTUALLY spawned and visible (5.3), correlated by timestamps in the same
   session. Eyes facts are recorded as receipts, never as memories; no log line
   certifies visibility by itself.
3. **An honest partial run is a complete evidence run, not the goal.** E.g. the EXPECTED
   outcome at time of writing — pool refusal at rung 1 (mapped-but-unreadable,
   `pool_range_verdict_fresh`=1) — banks fresh durable evidence and records every
   downstream rung NOT-REACHED. That is a success of the evidence policy and a
   non-goal verdict in the same breath.
4. **Global non-evidence (applies to every row):** host fixtures/suites/compiles; frozen
   historical artifacts re-presented as live; screenshots or impressions without file
   artifacts; re-rating unchanged evidence; replaying failed/reserved jobs; critic scores
   substituting for regression greps; `IsKeyBindRegistered`; `pad_available`=0; edited
   main.lua (any edit voids rung 0's unchanged-bytes claim).
5. **A crash, or a silent no-op (no `module_start_entered`, no failure line, no status
   change), is FAIL regardless of progress — no rung is rated from that pull.**

---

## Open definition items (owner must supply / ratify)

| Item | Status | Owner |
|---|---|---|
| `worker-runbook-v2` rung-1 cascade | NOT LANDED (no directory). Rung 1 above uses the task-brief fallback (pool-v5 tags). When v2 lands, reconcile it against rows 1.1-1.5 — especially the expected-refusal framing | worker-runbook-v2 (unlanded; dispatch owner must create it) |
| "gamedata line" preamble naming | No landed artifact by that exact name; G1+G2 are the defined identity lines | worker-runbook-v2 must ratify/rename |
| `worker-controller-input` evidence plan | Directory NOT landed; the named plan exists under worker-pad-defer (`EVIDENCE-PLAN-INPUT-ARRIVES.md`). Attribution gap only, no content gap | dispatch owner (relabel or reconcile) |
| `worker-menu-chain` boundary list | Directory NOT landed; boundary list assembled from worker-menu-input-map R5 + worker-menu-spawn 02 + worker-engine-pump-chain M5. Attribution gap only, no content gap | dispatch owner (relabel or reconcile) |
| G-AP1 per-press tag (the one genuine evidence gap) | NO tag fires when a pad event reaches the handler, so "zero new lines on first press" cannot discriminate input-dead vs drain-dead from sinks alone (rung 3.5/4 non-tick paths). Proposed bounded fill: one-shot emitters at PADSRC:88 / HANDLER:91 or one `PAD:` MIRROR line in L — distinct-ID revision, NOT staged | pad/input workers (per worker-menu-input-map `REPORT.md` gaps table) |

**Do-not-claim guard:** this checklist defines what WOULD count. It never certifies that
any of it happened. Host tests, drafted logs, and baseline lines never advance a rung.
