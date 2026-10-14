# NEXT-LAUNCH-RUNBOOK — single-run, all-reachable-evidence collection (one game session)

Worker: worker-launch-readiness · 2026-10-05 · read-only synthesis of completed evidence
plans into ONE launch procedure. No console dispatch, no launch authorization, no runtime
claims. Sources merged: `worker-pad-defer/results/EVIDENCE-PLAN-INPUT-ARRIVES.md`,
`worker-engine-pump-chain/{REPORT,CHAIN}.md`, `worker-menu-spawn/results/02-MENU-OPEN-INPUT.md`,
`worker-reload-support/SURVEY.md`, `worker-post-seeds-chain/CHAIN.md`,
`worker-durable-events/PROCEDURE.md`, `worker-real-timing/TIMING.md`,
`worker-ploader-70s/GAP-ANALYSIS.md`, `worker-construction-error/ANALYSIS.md`,
pool v5 units (`launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp`,
`launch10-build/final-units/SeedValidation.hpp`, `launch11-combined/units/UnrealInitializer.cpp`).

## 0. Honesty banner (read before anything else)

- EVERY rung below is **PENDING-live-proof**. Every in-game line in the merged timeline is
  the SHAPE of evidence to expect, not a captured result. Nothing in this runbook closes a
  rung; only same-run evidence does.
- The evidence ladder is: **runtime starts → mod loads → game-thread work → input ARRIVES →
  menu opens.** Each rung claims only what the SAME run (same pid) shows. **This runbook
  never promises the menu.** Menu is the last rung and is claimed only if its lines appear.
- Expected pool outcome TODAY is an honest REFUSE (mapped-but-unreadable class) pending
  on-target mincore semantics evidence from the standalone payload. That refusal is a
  SUCCESS of the evidence policy, not a failure of the launch — and it blocks init, so it
  legitimately caps how far the ladder can climb this boot.
- The game must SURVIVE every acceptable outcome; a crash is FAIL regardless of progress,
  and a silent no-op (no `module_start_entered`, no failure line, no status change) is also
  FAIL (LAUNCH-DAY-10 guard).

---

## 1. THE ONE QUESTION this launch answers (asked FIRST)

> **Does the TownfallItemSpawner mod LOAD now?**
>
> PASS = this exact line appears in UE4SS.log, in the launch's own pid window:
> `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
> AND the line `Mod is absent from native Lua source` appears ZERO times for that pid
> AND the mod path anywhere printed for that pid is CLEAN (no NUL-interleaved garble).
>
> The load banner is the joint proof that (a) the ctor path lookup matched after the
> double-garble fix (widen loop + qualified `modbridge::normalize_path_for_lua` calls),
> (b) `start_mod` ran, (c) main.lua's top level executed through `lua_pcall`, and (d) all
> six `RegisterKeyBind` calls succeeded against the deferred-attached pad source (wpd1
> boots logged `Exception during mod startup: Input registration unavailable…` instead).
> `runtime-status` `mods-started` does NOT prove this — that ack fires at engine-ready on
> ctor inventory OK. The banner is the ONLY mod-executed proof.

Everything else in this runbook is the ordered conditional cascade that decides whether
this question is even reachable this boot, and what else to bank while we are in the
session anyway.

---

## 2. The conditional cascade (ordered; stop claiming past the first unmet rung)

### Cascade step C0 — boot frame (always collected)
Expect, in order, in the launch pid: bridge spanpub publication + activation consumed →
backend ctor → program ctor (`Setting up mods...` — see §5 ordering quirk) →
`[Seeds] Required-slot seed table armed (slots 0/2/3/4); ...` → seeds consumption.
Pad deferral trio: `pad_input_deferred`=6, `pad_input_deferred_error`=0x80920004,
`pad_input_deferred_input_gate`=1, and NO `runtime_start_failed`.

### Cascade step C1 — pool v5 verdict (THE fork)
The v5 policy publishes **readable non-image spans only** (image-extent overlaps are
excluded at publication; image readability is proven by anchors), classifies every shape
failure (`failure_class`: Structural / SpanTableUnavailable / CaptureTruncated / NotCovered),
and on the first listing-gap miss (CaptureTruncated or NotCovered) fires the fresh
`mincore`-class RANGE probe **once per boot** over Blocks[0]'s full capacity, emitting the
durable tag:

```
pool_range_verdict_fresh  =  0 not-mapped (ENOMEM) / 1 mapped / 2 unavailable (other errno) / 3 invalid request
```

- **EXPECTED TODAY: the pool still REFUSES** (`[Seeds] REFUTED: ...` in UE4SS.log; the same
  text rides the runtime-status `failed` line via `ack->failed`). A seed refusal propagates
  and aborts init — **it cannot coexist with a healthy startup, so the load banner is
  unreachable and rung claims stop here.** Collect: the `pool_range_verdict_fresh` value +
  the NEW refusal note text (the `range-verdict resolution: ...` suffix in the refusal
  line, or `not consulted (failure class not rescuable)`), then go to §7 rows 1-4 and STOP
  claiming further rungs. Bank everything C0 captured; nothing else is reachable.
- **IF THE POOL VALIDATES UNEXPECTEDLY** (no refusal; `[Seeds] FName pool validated at
  0x9765EC0 ...` appears): capture the FULL seeds chain (all five `[Seeds]` lines, §5) and
  the marshal lines, then continue to C2.

### Cascade step C2 — post-seeds chain and the marshal (only after C1 pass)
Capture in order: `Using engine version: ...` → object waits (`Waiting for object
construction...`, `Constructed [n/m]: ...`) → `Finding GameEngine::LoadMap` (the first
exec-text walk — under a refusing walkguard this line is the LAST progress line before a
loud refusal + fail-closed shutdown; if that happens, everything below is unreachable) →
`[R3] refused: FName constructor verification detour (...)` → `[R3] refused: EngineTick
pre-callback registrations ...` → `[R3] EngineTick detour unavailable; deferring game-thread
pump to R3 marshal` → remaining hook resolves → engine-ready ack (runtime-status
`engine-ready`) → event-loop jthread starts.

### Cascade step C3 — the LOAD question (THE ONE QUESTION, only reachable here)
`Starting mods ...` / `Mod 'TownfallItemSpawner' ... starting mod.` (exact text is
lineage-dependent; the banner is the invariant) → **`[Lua] [TownfallItemSpawner] Loaded
v1.0. ...`** → `mods-started` ack. Record PASS/FAIL per §1. If the pump armed (C2), also
capture: `[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick
(callback id 0)` — arm proof; `[R3] marshal is the game-thread pump (initializer deferred
its EngineTick registrations)` — pump-live proof (first carrier invocation). **Armed
without pump-live proves nothing about drain latency (F-1 open).** Failure variants to
capture verbatim: `marshal carrier candidate not found` / `marshal arm refused: ...` /
`marshal arm failed: no ranked candidate passed ...`.

### Cascade step C4 — input ARRIVES rung (only after C3 PASS)
Per the pad evidence plan: first attempt `pad_open_class`=0 (exactly once), then
`pad_retry`=1 / `pad_retry_failed`=0x80920004, further attempts 2,3,... with growing gaps.
ARRIVAL = `pad_available` = <handle>, value > 0, exactly once, t_ms strictly after the last
`pad_retry_failed`, and NO `pad_retry`/`pad_retry_failed` afterwards. Any `pad_retry_failed`
value ≠ 2157051908 is the AUDIT FINDING-1 discriminator (new failure mode) — capture the
exact value and reconcile before any re-launch.

### Cascade step C5 — delivery and menu rungs (only after C4 arrival)
Leave ≥2 s after `pad_available` (the first post-open poll swallows the baseline), press
OPTIONS, repeat up to 3 presses ~2 s apart. Delivery evidence (UE4SS.log, same pid, t_ms ≥
the `pad_available` t_ms): EITHER the menu chain (`[Lua] [TownfallItemSpawner] <N> spawnable
pickups found. Pages=<M>` then `[Lua] [TownfallItemSpawner] Menu created.`) OR the
ExecuteInGameThread failure lines (`Tried to install hook but function is unavailable.` /
`Failed to add hook, detour installation likely failed!` / `EngineTick callback registration
failed`) OR the R3-discriminator `ExecuteInGameThread: EngineTick method requested but
EngineTick hook is not available (AOB scan failed)`. ANY of these is INPUT-rung delivery —
**the menu opening is the separate rung and its absence does not fail delivery.** Menu
pass lines are exactly the two quoted above; `Scan failed: Item database not ready. Load a
save first.` means drain alive but no save loaded; ZERO new lines on first press = input
delivery or drain failure (discriminate with §7 rows 12-13).

The mod listens for exactly SIX keybinds (main.lua:694-729), edge-only keydown, no
autorepeat, no modifier chords; the staged PS5Pad.ini maps the pad onto them:

| pad (PS5Pad.ini) | UE4SS Key | role |
|---|---|---|
| OPTIONS | F7 (118/0x76) | toggle menu (the press this rung uses) |
| CROSS | RETURN | activate row = spawn / option cycle |
| UP / DOWN | UP_ARROW / DOWN_ARROW | selection move |
| LEFT / RIGHT | LEFT_ARROW / RIGHT_ARROW | page prev / next |

Press #2+ on F7 toggles closed by design; console open (`allow_input`) silently swallows
all keys — a silent gate, not a failure.

---

## 3. Build-identity precondition (before launch)

The full tag set only appears on a build containing ALL of: pad_defer `7b5d4636...` program
+ the pad_coexist four files (wpd2 retry), pool v5 (mincore probe + non-image-only span
publication + `failure_class` + `resolve_listing_gap`), and the double-garble fix (widen
loop + qualified `modbridge::` calls). Verify from the controller's staging receipt BEFORE
launch. Symptom of a stale fold: deferral pair present but NO `pad_retry` at all, or a
garbled mod path — see §7 rows 15-16. Those outcomes mean rung NOT EXERCISED (build
identity), never a route failure.

## 4. Pre-launch baseline pin (Step 0 of the durable procedure)

The accumulated sdiag of the last completed run is the append-only baseline. Record
`N=$(wc -c < <latest baseline jsonl>)` (reference precedent:
`outputs/2026-10-04-zcode-controller/launch10-prep/sdiag-baseline-run5.jsonl`, 318910 B,
sha256 40504e28..., for RUN-5). Also re-verify the other baselines (ue4ss log, status,
entry, xwp) are unchanged before staging; any drift → STOP and reconcile before attributing
anything. Launch is manual (user starts Townfall); this runbook authorizes nothing.

## 5. Merged observable timeline for ONE session (expected order; all PENDING)

Order within one boot (one pid). Sources: post-seeds-chain CHAIN §0-§3 (receipt-pinned
units), engine-pump-chain CHAIN, pad evidence plan.

| # | stage | observable line(s) | sink | once-per-epoch? |
|---|---|---|---|---|
| 1 | module_start | spanpub publication; `module_start_entered` class stamps; PATH_TEST stamps later | sdiag + entry log | yes |
| 2 | backend ctor | `Setting up mods...` **(see ordering quirk below)** | UE4SS.log | yes |
| 3 | pad defer | `pad_input_deferred`=6, `pad_input_deferred_error`=0x80920004, `pad_input_deferred_input_gate`=1 | sdiag | yes |
| 4 | seeds arm | `[Seeds] Required-slot seed table armed (slots 0/2/3/4); ...` | UE4SS.log | yes |
| 5 | seeds bind | `[Seeds] slot 0 g_uobject_array bound ...` → `[Seeds] slot 3 gmalloc bound ... V1 instance + V2 vptr match` → `[Seeds] FName pool validated at 0x9765EC0 ...` → `[Seeds] slot 2 FName ctor bound ...` → `[Seeds] slot 4 static_construct_object_internal bound` | UE4SS.log | yes (refusal instead = terminal) |
| 6 | pool v5 probe | `pool_range_verdict_fresh` ∈ {0,1,2,3} — ONCE, first listing-gap miss only | sdiag | yes |
| 7 | pool refusal (expected) | `[Seeds] REFUTED: FName pool at 0x...: <failure> (observed CurrentBlock .., Blocks[0] 0x..; published readable spans ..; range-verdict resolution: <note>)`; runtime-status `failed` line carries the same text | UE4SS.log + status | yes — terminal |
| 8 | post-seeds | `Using engine version: ...`; `Waiting for object construction...`; `Constructed [n/m]: ...` | UE4SS.log | yes |
| 9 | first exec walk | `Finding GameEngine::LoadMap` (last progress line under a walk refusal) | UE4SS.log | yes |
| 10 | R3 fold | `[R3] refused: FName constructor verification detour (...)`; `[R3] refused: EngineTick pre-callback registrations ...`; `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` | UE4SS.log | yes |
| 11 | engine-ready | runtime-status `engine-ready` (durable mirror line, same pid) | status + sdiag | yes (status is LATEST-STATE-ONLY) |
| 12 | event loop | event-loop jthread started (implicit in what follows; UpdateThread ~5 ms cadence) | — | yes |
| 13 | marshal arm | `[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick (callback id 0)`; later `[R3] marshal is the game-thread pump (...)` | UE4SS.log | arm once; pump-live on first carrier invocation |
| 14 | mods start | `Starting mods ...`; `Mod 'TownfallItemSpawner' ... starting mod.` | UE4SS.log | yes |
| 15 | **LOAD** | **`[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`**; ZERO `Mod is absent from native Lua source` lines | UE4SS.log | yes |
| 16 | pad retry | `pad_open_class`=0 (once), `pad_retry`=1.., `pad_retry_failed`=0x80920004 | sdiag | once per session, then per-attempt |
| 17 | pad ARRIVAL | `pad_available`=handle >0, once, after the last `pad_retry_failed`; no retries follow | sdiag | yes |
| 18 | delivery | menu chain OR ExecuteInGameThread failure/discriminator lines (§2 C5) | UE4SS.log | per press (repeatable in-session) |
| 19 | menu (NOT promised) | `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>` then `[Lua] [TownfallItemSpawner] Menu created.` | UE4SS.log | first open (rescan after map load) |

**"Setting up mods" ordering quirk (document so nobody misreads the log):**
`Setting up mods...` runs at **ctor-time `start_native_mods` (backend ctor) and therefore
precedes every `[Seeds]` line**, because seeds run only later inside
`init() → setup_unreal → Initialize → ScanGame`. If a fetched log seems to list seeds lines
before `Setting up mods...`, re-check raw timestamps/pid prefixes before believing it —
never reorder the narrative by eye. Same rule for the mods-START lines at #14: they come
from the event loop, i.e. AFTER all seeds and engine-ready work, never before.

**Timing expectations (all measured, all scoped — do not extrapolate):**
- composition `module_start` ≈ **0.8 s** — measured on the EARLY-REFUSAL path only; a full
  successful startup is unmeasured and must not be projected from it.
- inter-plugin gap ≈ **70.7 s** (70.7-71.8 s, 4/4 boots) between composition module_start
  return and path_test termination stamps — ploader-side readiness/attach overhead. This is
  NORMAL; it is not a hang and is not a reason to touch the console.
- pool wait budget: if the shape-retry loop spins it burns up to **120 s**
  (`SecondsToScanBeforeGivingUp` coerced 30→120). A terminal refusal landing in ~11 s is
  consistent ONLY with an immediate refusal class (span-table unavailable, anchor/entry
  mismatch, class refusal, another init throw, or owner stop) — use the gap as a
  discriminator, never as a verdict by itself.
- pad retry cadence: first attempt ≈ 1 s after boot (poll 200 at the 5 ms UpdateThread
  cadence), doubling toward the ~16 s cap. Gaps are observations, not a pass requirement.
- menu latency in an armed-marshal world is carrier-cadence-bound and UNMEASURED (F-1) —
  allow generous seconds and record press timestamps before declaring silence.

## 6. Pull procedure (per stage; PULL-ONLY, same-pid correlation)

1. **One fetch, ~60 s after the boot settles** (earlier only if a terminal refusal is
   already visible and the controller chooses to close the session). Fresh curl per file,
   LAUNCH-DAY-10 §3 order, into a new run dir `outputs/2026-10-04-zcode-controller/runN/`:
   `UE4SS.log` → `runtime-status.txt` → `modbridge-supplier-diag.jsonl` (absence is normal)
   → `modbridge-exec-write-probe.jsonl` (drift guard) → `townfall_runtime_aligned_entry.log`
   (history guard). Do NOT re-fetch to "catch the status before it truncates" — the status
   is LATEST-STATE-ONLY (terminal O_TRUNC write); the durable evidence is the sdiag jsonl
   (accumulates across epochs) + the pid-prefixed UE4SS mirror lines.
2. **Read the durable evidence FIRST** (authoritative):
   ```
   EV=outputs/2026-10-04-zcode-team/worker-durable-events/tools/epoch_view.py
   python3 $EV runN/modbridge-supplier-diag.jsonl \
       --ue4ss runN/UE4SS.log --status runN/runtime-status.txt \
       --baseline-bytes $N [--build-id <edition from the staging receipt>] \
       [--json > runN/epochs.json]
   ```
   ONE row per epoch = one game pid; new epochs are the `[NEW]` rows. The status file
   applies ONLY to the epoch whose pid equals its session row. t_ms goes backwards between
   epochs (BOOT-GAP flagged) — never subtract t_ms across epochs. Cross-sink t_ms agreement
   is exact (≤1 ms skew observed).
3. **UE4SS.log greps for this launch** (always filtered to the launch pid; lines carry
   `[t=<ms> pid=<pid>]` prefixes):
   ```
   grep -n "Loaded v1.0"                    runN/UE4SS.log   # THE ONE QUESTION
   grep -n "Mod is absent"                  runN/UE4SS.log   # must be ZERO for this pid
   grep -n "Seeds\] REFUTED\|Seeds\] slot\|FName pool validated" runN/UE4SS.log
   grep -n "range-verdict resolution\|Setting up mods\|Finding GameEngine::LoadMap" runN/UE4SS.log
   grep -n "\[R3\]"                          runN/UE4SS.log
   grep -n "spawnable pickups found\|Menu created\|Scan failed\|EngineTick" runN/UE4SS.log
   ```
   sdiag tag readout (pool verdict, pad tags) comes from the epoch_view table; one-off
   filter: `python3 -c "import json,sys; [print(json.loads(l).get('pid'),json.loads(l).get('tag'),json.loads(l).get('value')) for l in open(sys.argv[1]) if 'pool_range_verdict_fresh' in l or 'pad_' in l]" runN/modbridge-supplier-diag.jsonl`
4. **NUL-interleave check on ANY remaining garbled print** — the codecvt-era bug wrote every
   character followed by a 0x0000 unit, and the sink truncates at the first interleave NUL,
   so suffix content is UNRECOVERABLE; a line that ends early or shows a mangled mod path is
   the signature. Durable check over the whole log:
   ```
   python3 -c "import sys; d=open(sys.argv[1],'rb').read(); ls=d.split(b'\n'); bad=[(i+1,ls[i][:60]) for i in range(len(ls)) if b'\x00' in ls[i]]; print('NUL-lines:',len(bad)); [print(n,b[:60]) for n,b in bad[:20]]" runN/UE4SS.log
   ```
   Any NUL-bearing or truncated-looking line around the mod path → capture the byte context
   and treat the garble fix as NOT in the linked build (build identity, §7 row 16).
5. **One-pager + drift guard** (unmodified evidence.sh):
   ```
   sh outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh \
      --from-dir runN --baseline-dir outputs/2026-10-04-zcode-controller/launch10-prep \
      --sdiag runN/modbridge-supplier-diag.jsonl --out runN/summary.txt --json-tail
   ```
   Any CONFIG-DRIFT warning → STOP; verify the ini (1216aab6...) before attributing
   anything. Save `runN/epochs.json` next to the raw fetches; pin THIS run's sdiag byte
   size + sha as the NEXT baseline cut. Never replay a failed request; a new run gets a new
   pid/epoch.
6. **Same-pid correlation rule (all stages):** use ONLY records with
   `pid = max(pid in file)`; never mix pids; use seq order within the pid; UE4SS delivery
   lines count only with t_ms ≥ the `pad_available` t_ms in the SAME pid;
   per-writer sequences only — NEVER subtract across writers (pathtest ns stamps, r4 log ms,
   sdiag t_ms are different clocks).

**In-session repeatability (what ONE session can and cannot redo; reload-survey verdict):**
- REPEATABLE in-session: Lua-driven diagnostics — repeated OPTIONS presses, repeated Lua
  prints; `UE4SS.log` appends in-session and is covered by the one fetch.
- NOT repeatable in-session: everything at `module_start` stage (pool verdict, seeds,
  `[R3]` arm, deferral/retry start — sdiag emits at startup only) and `runtime-status`
  (one terminal write). New boot-stage evidence needs a NEW epoch. There is NO host→game
  command channel; do not plan in-session re-probes of boot rungs.
- A supported per-mod Lua reload is code-present but evidence-absent (both hot-reload
  settings refuse PROSPERO startup; no trigger exists in the composed runtime). Do not
  attempt reloads this launch.

## 7. Failure-decision table (observable → next controller action)

| # | observation | verdict / next action |
|---|---|---|
| 1 | Pool REFUSES; `pool_range_verdict_fresh`=**2** | syscall class UNSUPPORTED on this kernel → **STOP the mincore route**; the range-verdict semantics proof must come from the on-target standalone payload (separate process) first; no re-launch on this route. |
| 2 | Pool REFUSES; `pool_range_verdict_fresh`=**1** + note `... MAPPED over the full required read range, but read permission is UNPROVEN ...` | EXPECTED today — honest mapped-but-unreadable refusal. Bank the fresh durable evidence; **await standalone semantics evidence** (if mincore proves protection-aware, `RangeVerdictImpliesReadable` becomes an evidence-backed source). Do NOT re-launch unchanged; a distinct-ID revision requires NEW derivation evidence. |
| 3 | Pool REFUSES; `pool_range_verdict_fresh`=**0** | genuine NOT-mapped proof over Blocks[0]'s full range → wrong-base/wrong-pool suspicion; Blocks[0] candidate label dead → **seed-table derivation audit** before any re-derivation. |
| 4 | Pool REFUSES; `pool_range_verdict_fresh`=**3** | invalid probe request (len 0 or >16 MiB) — instrumentation bug; capture; the pool verdict itself is NOT RATED this boot. |
| 5 | Pool REFUSES with `range-verdict resolution: not consulted (failure class not rescuable)` | failure class was Structural/SpanTableUnavailable — the probe correctly never fired; fix the structural cause; probe path untested. |
| 6 | Pool VALIDATES (unexpected) | capture all five `[Seeds]` lines + arming line as the full seeds chain; continue C2; ALSO record that the v5 refusal expectation was falsified — with the exact span/envelope numbers. |
| 7 | `[R3] FuncPtr-marshal armed` present, `marshal is the game-thread pump` absent | carrier armed but NEVER invoked (F-1 open) → carrier-rate investigation; do not claim any drain capability. |
| 8 | `marshal carrier candidate not found` / `marshal arm refused: ...` / `marshal arm failed: no ranked candidate passed ...` | capture verbatim; marshal route owns the failure; game-thread-pump rung NOT EXERCISED. |
| 9 | LOAD banner present, ZERO `Mod is absent` | **LOAD RUNG CONFIRMED** (scope: main.lua top level + six binds registered; menu/spawn NOT claimed). Continue to C4/C5. |
| 10 | `Mod is absent from native Lua source` with a CLEAN path printed | the garble fix worked but the lookup STILL missed → **different lookup issue: escalate to the source registration audit** (NativeModSource `lua_mods()` row bytes vs the normalized path; loadlist key `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`). |
| 11 | LOAD banner absent + `Failed to execute main script: ...` / `Exception during mod startup: ...` | capture exact text + the NUL check result; reconcile against the read-path chain (key miss vs validate_path throw vs pcall error) before any revision. |
| 12 | `pad_available`>0 AND ≥1 delivery line | **INPUT RUNG CONFIRMED** (scope: edge delivered to a registered Lua callback; menu/spawn rungs separate). |
| 13 | `pad_available`>0, no delivery line | acquisition proven, delivery UNRESOLVED → verify presses were after the arrival t_ms AND the load banner exists; if both hold, G-AP1 (no per-press tag) blocks attribution; do NOT re-launch unchanged. |
| 14 | Only `pad_retry`/`pad_retry_failed`=0x80920004 | handle never freed; outcome B not exercised; outcome A intact (startup continued, mods started). No pad-route conclusion. |
| 15 | `pad_retry_failed` value ≠ 2157051908 | **AUDIT FINDING-1 discriminator** — new failure mode (prime suspect: repeated `scePadInit` already-initialized class at phase 3). Capture the exact value; reconcile before ANY re-launch. |
| 16 | Deferral pair present, NO `pad_retry` at all | linked binary predates the pad_coexist fold → build identity; input rung NOT EXERCISED. |
| 17 | `Mod is absent` / any path print garbled or NUL-interleave hits >0 | double-garble fix NOT in the linked build → build identity; compare staged unit shas (String.hpp 7c983db4..., luamod lineage) before touching the route. |
| 18 | First OPTIONS press produces ZERO new lines | input delivery (R4) or drain (R3) failure → discriminate: the `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` line means input ALIVE + drain dead; total silence points at input. Menu rung untouched either way. |
| 19 | `[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.` | drain ALIVE, no save loaded → menu mechanics intact; load a save in-session and re-press (repeatable in-session). |
| 20 | `Finding GameEngine::LoadMap` is the last progress line + `[WalkRxGuard] REFUSED`/`guard_off_no_rx_verdict` | GATE-1 exec-read refusal, fail-closed; everything downstream unreachable THIS boot; runtime-start rung still proven (game survives). |
| 21 | `runtime_start_failed` present at boot | startup refused — capture code + the pid-mirror text via epoch_view; nothing downstream is rateable. |
| 22 | CONFIG-DRIFT warning (entry/xwp growth, DIAG lines) | STOP; verify ini sha before attributing anything to the candidate. |
| 23 | Crash, or silent no-op (no `module_start_entered`, no failure line, no status change) | FAIL regardless of prior progress; capture what exists; do not rate any rung. |
| 24 | `pad_available`=0 or out-of-order seq | impossible by construction → capture corrupt; do NOT rate. |

## 8. Anti-noise rules (carry over, unchanged)

`IsKeyBindRegistered` is NOT evidence (returns false while deferred). Value 0 for
`pad_available` is not a handle. `IsKeyBindRegistered`-style host guesses never substitute
for log lines. All claims cite ONE pid window. The ~200-poll cadence and ~0.8 s
early-refusal figures stay scoped exactly as previously reported. Critic/observer scores
never replace these regression greps. No failed/reserved-job replay; a re-derivation gets
distinct IDs and new evidence only.

## 9. What this run may honestly conclude (max claims)

- MAX claim if everything passes C0→C5: "runtime started; pool validated (fresh
  range-verdict evidence captured); seeds all bound; marshal armed AND pump-live line
  captured; **mod LOADED** (banner, no absence line, clean path); input ARRIVED
  (`pad_available`>0); OPTIONS delivered to a registered Lua callback (first-press lines);
  menu opened (`<N> spawnable pickups found` + `Menu created.`)". Item SPAWNING (M6) is
  NOT part of this runbook's claims even if observed — record it verbatim, claim nothing.
- EXPECTED honest outcome today: C1 refuses (pool) → runbook records the fresh
  `pool_range_verdict_fresh` value + refusal note, every downstream rung is recorded as
  NOT-REACHED (blocked by terminal seed refusal), and the launch still banks: boot-frame
  tags, the v5 probe evidence, and game survival. That is a complete, successful evidence
  run at the current rung.
- The menu is never promised, predicted as reached, or counted ahead of its own two lines
  in the same pid.
