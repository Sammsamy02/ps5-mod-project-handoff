# EVIDENCE-SPEC — acceptance-ladder rungs: exact artifacts, PASS/FAIL, negative evidence, non-evidence

worker-acceptance-evidence, 2026-10-04. This document defines, per rung of the
acceptance ladder (`genuine game-thread callback` → `valid object query` →
`harmless reflected access` → `unchanged Item Spawner activation` → `visible
spawn through real input`), the EXACT evidence artifacts: what the console
shows, what each file must contain, what constitutes PASS vs FAIL, the
NEGATIVE evidence (what you would see if the rung did NOT work), and the
NON-EVIDENCE list (what does not count even if it looks impressive).

**Status honesty: this is a contract for future console runs. Nothing here
claims any rung is currently PASSED.** Every quoted byte-string, field name,
and path below is taken from the cited sibling artifacts; none was executed
by this worker.

---

## 0. Shared evidence channels and global rules

### 0.1 The four console-side evidence channels

| # | Channel | Path / form | Producer | Freshness rule |
|---|---|---|---|---|
| C1 | Runtime log | `/data/townfall_runtime_aligned_entry.log` — the CURRENT in-game runtime lineage (mode-2 `modbridge_runtime_aligned_entry.prx`) | composed runtime records | Appears ~1 minute after launch, at injection completion (ploader shows "injecting 2 plugins" → OK/FAIL ~1 min in). Freshness oracle = file mtime per the launch ritual; stale mtime = absent evidence. |
| C2 | In-game facts JSONL | `/data/modbridge-ingame-facts.jsonl` | probe PRX (`modbridge_ingame_facts`) and any `ig_run_*` add-on emitting through `IgSink` | One record envelope per line: required non-empty `build_id` + integer `seq`; duplicate seq flagged; the staged build REPLACES the file on the same path, so the file only ever contains the current build's records. |
| C3 | Runtime ack | `runtime-status.txt` at the activation root | worker-runtime-ack's `RuntimeAckSink` (five emission points) | Line 1 is the session header `session\t<pid>\t<epoch_ms>`; subsequent whole lines from exactly: `activated`, `engine-ready`, `mods-started`, then terminally `stopped` or `failed`. `running=1` is derived mid-session; a missing `end`/terminal state means the process died — never fabricated. |
| C4 | Console capture | video or burst-frame capture covering screen + timestamps, plus collected log tails | operator/controller during the run | One continuous capture per rung-5 attempt; artifacts enumerated in the step checklist (VISIBILITY-TEST §3 step 9). |

Plus a fifth, mod-side channel that is *routed by* the runtime rather than
owned by it: the Lua mod's own `print()`/`log()` lines (tagged
`[TownfallItemSpawner]`). Which retained log path the composed runtime routes
these into is a property of the runtime build, not of the mod — the rung-4
oracle is therefore deliberately **path-agnostic**: grep the ACTIVE retained
Lua log (current lineage: C1) for the mod tag. `ConsoleEnabled=1` in
`ue4ss/UE4SS-settings.ini` keeps the console-side channel enabled.

### 0.2 Global collection rules (apply to every rung)

1. Launch ritual: game fully closed → ploader armed → USER launches Townfall
   (never automated) → records ~1 min later. One launch per staged build. On
   FAIL: collect ploader surface text + log/JSONL tails via FTP; do NOT
   relaunch repeatedly without a changed implementation or a specific open
   question; never replay a failed attempt blindly.
2. Evidence comes only from the REAL Townfall process through C1–C4. Host
   fixtures, host test suites, compile probes, and static analyses are
   delivery evidence for the workers that produced them — they never advance
   a rung (per MILESTONE-TEST-DESIGN §3).
3. Never re-rate unchanged evidence; a refusal/inconclusive record is honest
   evidence, not noise to retry away.
4. Screenshots or user impressions without the underlying file artifacts are
   not evidence.

---

## RUNG 1 — genuine game-thread callback

**Claim under test:** the composed runtime's serialized Tick/ProcessEvent
route actually fires inside the real game's `UGameEngine::Tick`, on the real
game thread, and that thread's identity is published by the first genuine
Tick (`NativeGameThreadIdentity.hpp`; `publish_current()`).

### 1.1 Artifacts produced when Tick fires

- **C2 (JSONL):** the controller driver's record(s) for the query-harness
  dispatch channel — refusal-counter snapshots (per-kind counts, last refusal
  + sequence) and, once a query pair completes, a terminal record (rung 2's
  artifact; at rung 1 only the counter channel and identity facts are
  required).
- **C1 (log):** the runtime's own structured lines, including
  `Using engine version: 5.6` from the real version scan, and the refusal
  log line for any mis-ordered or wrong-thread dispatch.
- **C3 (ack):** `activated`, `engine-ready` lines prove the engine-init
  callback chain ran — necessary context, NOT sufficient for rung 1
  (`engine-ready` fires in the `on_engine_initialized` chain, which precedes
  the first genuine Tick; identity publication is a Tick-driven fact).
- **Pre-publication guard evidence:** a `GetGameThreadId()` attempt before
  the first Tick throws the honest "not yet initialized by
  UGameEngine::Tick" error; the harness refuses a dispatch with
  `RefusedIdentityUnknown` and counts it (design §2.1's pre-publication
  guard). Its presence in C1/C2 is positive evidence the guard is live.

### 1.2 PASS

All of the following, from one real launch:

1. Tick count increments: the dispatch counters show accepted tick dispatches
   advancing (`ticks_accepted` ≥ 2 for the query pair; counter snapshots
   taken at two distinct times differ), proving repeated genuine callbacks —
   not a single fired shot.
2. The game-thread identity publishes: `game_thread_id_key` non-zero,
   originating from the first genuine `UGameEngine::Tick`, and stable across
   subsequent dispatches (a mid-pair key flip is itself refused).
3. `IsInGameThreadRaw() == true` appears in the record/log for the accepted
   dispatch.
4. Guard evidence, not just the happy path: ≥ 1 counted refusal of
   `refused_wrong_thread` (or `refused_identity_unknown`) from a real
   non-game-thread / pre-publication dispatch attempt.
5. Lifetime preserved: `RuntimeOwner::stop()` from a loader/control thread
   returns true, joins, and C3 reaches `stopped` (or honestly `failed`)
   without hang or crash in the bounded observation window.
6. Provenance: the Tick address traces to the live-image scan or the
   serialized-ELF witness route (`TickSerializedElfWitness`, original
   no-match recorded); no defaults.

### 1.3 FAIL

- No Tick: no accepted-tick counter movement in any C2 snapshot; C3 stops at
  `engine-ready`/`mods-started` forever; `game_thread_id_key` stays 0
  (identity stays unknown) and `GetGameThreadId()` keeps throwing the
  not-yet-initialized error.
- Identity never publishes but a worker thread delivers callbacks: this is
  the fatal impostor case — any record whose id key is set from a thread we
  control (init/update/payload worker) is an automatic FAIL plus an honesty
  violation (ProgramFactory.hpp's own prohibition).
- A crash or hang inside Tick (C3 never reaches a terminal line; process
  gone).

### 1.4 NEGATIVE evidence (what you'd see if it didn't work)

- C1 shows only startup records, no tick-correlated lines after the
  ~1-minute injection point.
- C2 counter snapshots show the refusal counters moving ONLY
  (`refused_identity_unknown` incrementing every poll) with zero accepted
  dispatches.
- C3 last line stuck at `engine-ready` or `mods-started`; mtime goes stale.
- The mod-side symptom (cross-check): Item Spawner's F7 callback dies at
  `ExecuteInGameThread` (main.lua:695) with runtime-side Lua error
  `attempt to call a nil value (global 'ExecuteInGameThread')` — the
  retained baseline-trial marker that a REAL game-thread provider is absent.

### 1.5 NON-EVIDENCE (does not count, alone or combined)

- Any host fixture/harness/callback run (including the 358-check harness host
  suite, seam_host_test, the tick-file-owner 81-assertion suite).
- A successful `UE4SSProgram` constructor on console (checkpoint
  `native_program_construction_console`) — construction is NOT engine-bound
  startup.
- `engine-ready` in C3 by itself (an engine-INIT callback, not a Tick).
- EngineInitialized observed from our own init thread, or any callback
  delivered on an injected payload worker "pretending" to be the game thread.
- Plain Lua arithmetic, a log line from our own thread, init-array
  constructors, a compiled/linked/composed PRX/.o, or a static file-scan
  candidate (`*_file_image` endpoints — file labels are not callable
  runtime functions).
- A game launch where the module loaded but no structured C1/C2 record of
  the dispatch exists.

---

## RUNG 2 — valid object query (`GetNumElements`)

**Claim under test:** a read-only `UObjectArray::GetNumElements()` through
the published layout maps returns plausible values on the real process, read
twice on consecutive accepted Ticks through the published identity, with the
S2 layout gate genuinely armed.

### 2.1 The JSONL record (C2) and its fields

One terminal record written by the controller driver via
`FormatRecordJsonLine` (off-thread snapshot; never partial). Required
fields/content:

- `status`: `"completed"` (terminal) — the only advancing value.
- `count_source`: `"engine_uobject_array"`.
- `counts.recorded`: `2`.
- the two count values themselves, both `> 10000` (strictly greater; exactly
  10000 is Inconclusive by design — the record carries `threshold: 10000` so
  the controller can re-evaluate the boundary).
- `delta` and `monotonic` verdict: `Equal` or `NonDecreasing` (a `Decreasing`
  verdict is recorded honestly and does NOT advance the rung).
- `game_thread_id_key`: non-zero, equal for both reads; `identity_checks: 2`.
- `ticks_accepted`: `2`; `refusals_between_counts: 0` (holds by the no-leak
  construction: any refusal between the reads discards the staged count).
- engine/axes/storage facts equal to the validated profile and real scan
  (`"5.6"` + resolved axes; storage claim from the profile), gate provenance
  label naming `PublishStagedNativeLayoutMaps`.
- Counter channel (separate snapshots): ≥ 1 `refused_wrong_thread` /
  `refused_identity_unknown` count; **zero** `refused_layout_gate_closed` on
  the accepted path.

### 2.2 PASS

1. A `status:"completed"` record with `count_source:"engine_uobject_array"`,
   `counts.recorded:2`, both counts `> 10000`, verdict `Equal` or
   `NonDecreasing` (non-decreasing on consecutive ticks).
2. Identity coherence per §2.1 (key non-zero, equal, `identity_checks:2`).
3. Guard evidence present (≥ 1 wrong-thread/identity-unknown refusal
   somewhere in the session) and zero gate-closed refusals on the accepted
   path.
4. Facts/provenance per §2.1; lifetime preserved (C3 terminal `stopped`);
   `ticks_accepted` frozen at 2 after terminal.

### 2.3 FAIL (and how each failure announces itself)

- `status:"inconclusive_below_threshold"` (count ≤ 10000, one or both
  counts): Inconclusive, NOT refuted — the initializer heuristic's own
  boundary. Honest negative evidence; does not advance the rung; do not
  re-rate it as a pass.
- Counts present but `0`: the read executed but the layout maps are wrong
  (address/offset invalid) — a substantive refutation of the layout facts,
  worse than inconclusive. Expect it alongside `monotonic` weirdness.
- Gate-closed refusal: every dispatch returns `RefusedLayoutGateClosed` and
  the counter climbs while no record ever appears. Root cause today is
  factual, not hypothetical: `PublishStagedNativeLayoutMaps` has NO
  production caller yet (SEAM S2 GAP) — until the controller wires it, this
  refusal is the CORRECT outcome for every on-target dispatch.
- `Decreasing` verdict with plausible counts: recorded honestly; not a pass;
  investigate before any re-run.

### 2.4 NEGATIVE evidence (what you'd see if it didn't work)

- Only refusal-counter movement in C2 (`refused_layout_gate_closed` or
  `refused_identity_unknown` incrementing), no terminal record at all.
- An `inconclusive_below_threshold` record with `counts.recorded:1` or `2`.
- A completed-looking record whose engine/axes fields do NOT equal the
  validated profile — mis-wired facts, treat as FAIL even if counts look
  plausible.

### 2.5 NON-EVIDENCE

- The 358-check host suite (both storage claims, all refusal paths, no-leak,
  immutability, concurrency, JSONL sample) — host-only.
- The extra-unit compile probe (Prospero object 70,816 B, 0-new-undefined
  estimate) — compile evidence only.
- Any `TrySnapshot()` success from a host-built harness binary.
- A "completed" record replayed from a frozen historical artifact — records
  must come from the live run's C2.
- A Decreasing verdict re-rolled until it looks non-decreasing (re-rating
  unchanged evidence is prohibited).

---

## RUNG 3 — harmless reflected access (FindFirstOf / StaticFindObject; FC-1 FName test)

**Claim under test:** reflected queries resolve and execute harmlessly
through the composed runtime — the require chain resolves, `FindFirstOf` /
`StaticFindObject` are callable and return engine objects, and (if FC-1 is
validated in this run) the FName constructor candidate is byte-verified
in-process.

### 3.1 Artifacts

- **C1 (log, mod-side):** the discriminating `[TownfallItemSpawner]` lines
  produced by the mod's own code paths (see RUNG 4 §4.3 for the full
  phase-2 semantics):
  - `[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.`
    — proves the require chain resolved (main.lua IS:15), `FindFirstOf` was
    CALLABLE (IS:66 reached after the query machinery ran), and the game
    genuinely has no save loaded.
  - In a PlayerController-present context (e.g. main-menu) without a save:
    `WidgetBlueprintLibrary not found.` (IS:501) or
    `UI creation failed: <err>` (IS:636) — proves `ExecuteInGameThread`
    delivered (IS:695) and `StaticFindObject` EXECUTED (IS:499).
- **FC-1 probe records (C2):** if the FC-1 runtime probe add-on
  (`ig_run_fc1_probe`, worker-fname-runtime-probe) is in the staged build,
  each check emits a JSON record line through `IgSink` under its own
  `build_id`:
  - **FC1-A**: 64-byte prologue read at runtime `0x63d590` — expected bytes
    `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 0f b7 38 48 89 c6 31 c9 66 85 ff 74 2a 66 90`
    (+ strict second-32 check `0f b7 ff 09 f9 … 48`). Hard gate.
  - **FC1-B**: all 9 call-site windows — first instruction
    `48 8D 35 <disp32>` with `start+7+disp == anchor_string_runtime`;
    exactly one `lea rdi` store; exactly one `BA 01 00 00 00`; no
    call/jump/ret/RSI-write in-window; terminator `E8 <rel32>` with
    `site+5+rel32 == 0x63d590`.
  - **FC1-C**: uniqueness summary — FAIL CLOSED if recorded sites disagree
    (one passes FC1-B, another fails).
  - **FC1-D** (gated, only behind a prot-readable check): UTF-16LE
    `Name\0` anchor strings at the five content addresses.
  - Every exec-span read is mprotect-FIRST (add READ on exactly the covering
    16 KiB page(s), read, restore, record anomaly on restore failure).
- **Optional secondary (same run):** one FName roundtrip through the
  preserved `Conv_NameToString` fallback — roundtrip equality logged (validates
  the explicit-output ABI `void fn(ptr out, ptr text, i32 mode)`).

### 3.2 PASS

1. At least one reflected query demonstrably executed against the live
   engine and returned a verdict: the `Item database not ready.` tail (the
   query ran, DB genuinely absent) or the phase-2-POSITIVE
   StaticFindObject-context lines — and explicitly NOT the
   `attempt to call a nil value (global 'FindFirstOf')` tail.
2. No crash, no hang, no game-state mutation from the query itself
   (harmlessness): the process continues; C3 still reaches a terminal state
   later.
3. If FC-1 is claimed: FC1-A equality, all 9 FC1-B windows pass, FC1-C
   fail-closed summary reports no disagreement, and (if gated in) FC1-D
   matches — all as C2 records under the probe's build_id.
4. If the FName roundtrip is claimed: logged roundtrip equality true.

### 3.3 FAIL

- `[TownfallItemSpawner] Scan failed: attempt to call a nil value (global 'FindFirstOf')`
  — the B2-unresolved discriminator: reflected query machinery absent.
- A crash/scan-failure tail other than the two designed ones
  (`Scan failed: <unexpected err>`) — an engine touch went wrong.
- FC-1 refutation conditions (FC1-E): FC1-A mismatch while Tick/SCO
  prologues still hold (candidate-specific refutation); any recorded site
  whose runtime bytes are not `E8` (mapping refuted — distinguish from
  rel32 drift by comparing the static image at `vaddr + 0x4000`); a window
  missing `mov edx,1` or the `lea rdi` store (different overload — contract
  void); a second address satisfying FC1-B for all five anchors
  (ambiguity — pinning invalid).

### 3.4 NEGATIVE evidence (what you'd see if it didn't work)

- The nil-`FindFirstOf` tail above, or silence after F7 with a runtime-side
  Lua error trace (callback died before any query).
- FC-1 probe records with explicit per-check `mismatch`/`fail-closed`
  verdicts, or no probe records at all plus the other probes' records
  present (add-on not staged — absence of the build_id, not a silent skip).
- Process death on first query (C1 stops mid-line; C3 never terminal).

### 3.5 NON-EVIDENCE

- FC1-CHECKS.md's offline materialization (9 sites revalidated in the frozen
  capture) — static analysis only; the native resolver binary never ran.
- worker-fname-runtime-probe's host tests on SYNTHETIC bytes (all-pass,
  prologue-mismatch, fail-closed scenarios) — host-only.
- "The anchor table exists and is consistent" as a substitute for runtime
  FC1-A/B verdicts. Runtime/live verification of FC1-A..D has NOT been
  performed; verdicts remain unclaimed until an in-game run under its own
  build_id.
- A reflected write, spawn, or property set performed "to prove access
  works" — the rung is defined on harmless (read-only / find-only) access.

---

## RUNG 4 — unchanged Item Spawner activation

**Claim under test:** the UNMODIFIED Item Spawner Lua mod (intake main.lua
sha256 `f586043a…658`, verified at activation) loads, registers, and starts
inside the composed runtime — its own code observes the runtime and reports
in its own words.

### 4.1 The mod's own log line and where it appears

Byte-exact, exactly ONE new mod-tag line (mod source IS:746 via its `log`
IS:6, `print(MOD_TAG .. " " .. tostring(s) .. "\n")`):

```
[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
```

**Where:** in the ACTIVE retained Lua log path — the path the composed
runtime routes the mod's `print()` into. The oracle is path-agnostic
(OBSERVABLES.md): grep the active log for `[TownfallItemSpawner]`. Current
lineage: `/data/townfall_runtime_aligned_entry.log` (C1); records appear
~1 minute after launch at injection completion. `ConsoleEnabled=1` keeps the
channel enabled. Which path a given composed build actually uses is a
property of that build — if the tag does not appear in C1, check the other
retained paths (`/data/townfall_runtime_buffered_lua.log`,
`/data/townfall_mod.log`) before concluding absence, and record which path
held it.

**Routing cross-check (C3 + registry):** `runtime-status.txt` reached
`mods-started`; the enabled-mod list contains `TownfallItemSpawner`
(`enabled.txt` 0-byte marker present in `ue4ss/Mods/TownfallItemSpawner/`;
NativeModSource.cpp:134-135); runtime-side records show 6/6 keybinds
registered (baseline-trial precedent).

### 4.2 The UEHelpers-style require chain resolving

The mod's require chain (IS:15) resolving far enough is proven by reaching
the phase-2 checkpoint: after one real F7 press with NO scene/save loaded,
byte-exactly ONE new mod line:

```
[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.
```

then designed SILENCE (`buildUI` returns false with no log call when no
PlayerController exists, IS:493–497; `menuOpen` stays false). This one line
simultaneously proves: chunk load, 6/6 registration, map-hook pcall, the
log channel, `ExecuteInGameThread` delivery (IS:695), the require chain
(IS:15), the `FindFirstOf` query path (IS:66), and graceful
database-absence handling — the whole ladder below rung 3, scene-free.
Absence list for this scenario: `Menu created.` (IS:643),
`UI creation failed:` (IS:636), `WidgetBlueprintLibrary not found.`
(IS:501), `Spawned` (IS:335), `Failed at` (IS:341/342).

### 4.3 PASS

Phase 1: the IS:746 load line present exactly ONCE in the active log, and
NOTHING else tagged `[TownfallItemSpawner]` at load time (all other call
sites — IS:189, 194, 335, 342, 501, 636, 643 — are unreachable in the load
path). Pre-flight held: layout pair merged/listed, `enabled.txt` present,
`ConsoleEnabled=1`, fresh log mtime, C3 reached `mods-started`, main.lua
sha256 verified at activation.

Phase 2 (only if phase 1 passed): the no-save `Scan failed:` line exactly
once, then silence. Full phase-1 + phase-2-no-save pass validates
everything below rung 3 without any 3D scene or save.

### 4.4 FAIL (discriminated)

- No records at all at the usual post-injection time → load/injection
  failure: check the ploader OK/FAIL popup; retry per ritual; do not replay
  without changed implementation or a specific open question.
- Probe records present but no mod line → the Lua chunk aborted BEFORE
  IS:746. Only engine-touching calls exist after IS:692: the six
  `RegisterKeyBind` (694–729) and the pcall-guarded `RegisterLoadMapPreHook`
  (731 — cannot abort). A nil `RegisterKeyBind`/`Key` aborts at the first
  bind: the mod dies SILENTLY (no mod tag ever); look RUNTIME-side (C1) for
  the Lua load error.
- Phase-2 discriminator tail: `Scan failed: attempt to call a nil value
  (global 'FindFirstOf')` → runtime lacks the query API (rung 3 gap, not a
  mod defect).
- Ambiguity warning: no new mod line after F7 is ambiguous between (1) the
  key never fired and (2) the callback died at IS:695 on nil
  `ExecuteInGameThread` (unprotected; mod logs nothing). Discriminator: the
  runtime-side Lua error trace — `attempt to call a nil value (global
  'ExecuteInGameThread')` present = positive delivery+registration evidence
  (phase-2 partial pass); absent = input question open (belongs to rung 5's
  workstream, NOT a re-press).
- Pre-flight failure mode: with the layout pair not merged, the run ends in
  `setup_unreal()` with "Native activation requires both member and vtable
  layout files" and NO mod evidence is possible at all.

### 4.5 NEGATIVE evidence (what you'd see if it didn't work)

- Stale/absent active log at the usual time; ploader FAIL popup text.
- Runtime-side Lua load error with zero `[TownfallItemSpawner]` lines.
- C3 ending `failed` before `mods-started`, or enabled-mod list lacking the
  mod (marker missing).
- A DIFFERENT byte-string than the two designed phase-1/phase-2 lines — any
  other tag-bearing line at load time means the mod was modified or a
  different build is active (sha256 check would also disagree).

### 4.6 NON-EVIDENCE

- BUNDLE-SPEC §3.6 host/process-side observables — correlation only, not
  activation evidence.
- The bundle's host test (`test_activation.c`, `run_host_test.sh`) —
  host-only.
- "The files are installed in the right directories" — installation layout
  is a precondition, not activation evidence.
- The mod's startup running under a scheduling stub or a host Lua VM.
- An edited main.lua (even a one-line "fix") — the rung's claim is precisely
  that the UNCHANGED mod activates; any edit voids the rung.

---

## RUNG 5 — visible spawn through real input

**Claim under test:** a physical pad press causes, end to end, a visible
item spawn in a loaded 3D scene. This is the VISIBILITY-TEST acceptance
(worker-real-input §1–§4) plus the pad-bridge chain (INPUT-MAP §1–§3).

### 5.1 The full chain (what must be observable at each hop)

```
physical DualSense press (scePadReadState, game process)
  → PS5PadSource::poll            [rising edge; subscriptions/allow gate]
  → PadEdgeAdapter::poll          [debounce: one press = one event; no phantom on reconnect]
  → Handler::process_event        [key+modifier match, e.g. CROSS → RETURN]
  → Lua keybind callback          [RegisterKeyBind ref; main.lua:694–729]
  → ExecuteInGameThread(fn)       [main.lua:695/700/705/710/716/725]
  → EngineTick delivery gate      [refuses unknown/other identity → logged refusal, action NOT run]
  → callback body on game thread  [menu toggle / scan / buildUI / trace + spawn]
  → actor visible in frame
```

Mapping profile (`ue4ss/PS5Pad.ini`): OPTIONS = F7 (menu toggle), UP/DOWN =
UP_ARROW/DOWN_ARROW, CROSS = RETURN (activate/spawn), LEFT/RIGHT =
LEFT_ARROW/RIGHT_ARROW.

### 5.2 What the user observes on screen (C4 capture)

1. OPTIONS press → the Item Spawner menu overlay appears.
2. UP, then DOWN → the selection visibly changes.
3. CROSS on a selected row → within ~1 second, a NEW pickup actor appears
   in front of the player at the player-facing trace hit — an actor absent
   from the immediately-prior frame of the same fixed camera, spawn point
   unobstructed and in view, in a loaded save/world (post
   ItemDatabaseSubsystem availability).
4. Navigating to a different row and pressing CROSS → a second, DIFFERENT
   pickup.

### 5.3 Console-side evidence (C1/C2/C4 + count table)

- Per-press correlated lines in C1: keybind callback (keybind registration
  6/6 + per-key log/status change), delivery (no refusal), spawn line
  (`Spawned`, IS:335 / failure `Failed at`, IS:341/342 when applicable).
- C2 records (if probe/emitters staged) timestamped within the same window.
- The count table from the operator checklist: press # → item id → frame
  timestamp, for N ≥ 5 presses → N distinct spawns, mixing rows.
- One continuous capture (C4) correlating all of the above by timestamps +
  frame ordering — "eventually appeared" is not correlation.
- Negative controls, same session: CROSS with menu CLOSED → no spawn
  (Lua-side `menuOpen` gating intact); pad powered off mid-session → no
  phantom events (disconnect clears edge state; reconnect needs a fresh
  press); before first identity publication or on refused delivery →
  callback may fire but NO mod action runs, and the logged refusal proves
  the gate is real.

### 5.4 PASS

VISIBILITY-TEST §1 items 1–6 ALL hold from the step-9 artifacts of one
session: (1) real input origin via `scePadReadState` — no injected/synthetic
events, no keyboard emulation; (2) loaded 3D scene, unobstructed in-view
spawn point; (3) visible new actor absent from the prior frame; (4)
input-correlated timing across press → callback → delivery → spawn →
visible, within one observable window; (5) repeatability N ≥ 5 with one-
press-one-spawn debounce, plus a different item spawned after navigation,
plus OPTIONS toggling open/closed; (6) negative controls behaved.

### 5.5 FAIL

- Any §1 item missing → a documented rung-5 gap, NOT a partial pass.
- A log line alone (callback fired, delivered) without the actor in a
  captured frame.
- Menu overlay appears but no spawn on CROSS (menu-only visibility is the
  D-family milestone, not rung 5).
- Double-spawn on a single press (debounce failure), or spawns continuing
  after pad disconnect (edge-state failure).
- `Failed at` tails (IS:341/342) on press — the spawn callable threw.

### 5.6 NEGATIVE evidence (what you'd see if it didn't work)

- Presses with zero mod lines and zero runtime Lua errors → input never
  reached the handler (pad/poll/subscription path broken) — resolve via the
  pad workstream, not re-presses.
- Mod lines up to delivery but a logged EngineTick-gate refusal per press →
  rung-1 identity/delivery regression.
- `Scan failed:` even with a save loaded → database/scene-state detection
  regression.
- Captured frames show no new actor while logs claim `Spawned` →
  trace/spawn coordinates or visibility claim broken; the capture outranks
  the log line.

### 5.7 NON-EVIDENCE

- Everything in `vendored/` (host build, synthetic PadBackend sequences, the
  rung-5 mini-chain in `vendored/host_test.cpp`) — its ALL PASS receipt is
  NOT rung-5 evidence.
- Title screen / main menu / any non-gameplay world state.
- Spawns from console-injected commands, scripted input replay,
  pre-existing actors, or timer-based spawns without a correlated press.
- The earlier `controller_input_conversion` keyboard route (its Townfall
  D-pad/keyboard test read ambiguously and produced no gameplay change) —
  explicitly NOT evidence.
- A single lucky frame with no repeat trials and no negative controls.
- First real-pad read caveats: the pad provider is
  `hardware_runtime_validated: false` until this rung's first console
  attempt; ScePad open/read failures there are expected findings to record,
  never papered over.

### 5.8 Dependencies (what must already be accepted)

Rungs 1–4; worker-3 game-thread delivery live on console (A9); live
PlayerController/Pawn (C5); trace + spawn callables (C1/C2); the UMG menu
family (D1–D5) for the menu-path steps. Without D-family, the mod has no
menu-less auto-spawn path — rung 5 waits on those rungs' acceptance.

---

## Appendix: global NON-EVIDENCE (applies to every rung)

- Host evidence of any kind (fixtures, host suites, compile/link/compose
  success, static scans, synthetic-byte tests) — delivery evidence only.
- Frozen historical artifacts re-presented as a live run (records must be
  born in the run's own C1/C2/C3 with fresh mtimes and the staged
  build's `build_id`).
- Screenshots or user impressions without underlying file artifacts.
- Any re-rate of unchanged evidence, any replay of a failed/reserved job,
  any "simulate-runtime" output standing in for the real injected runtime
  (host file-format coverage only).
- A claim of ">8 quality" or completion derived from this spec: the spec
  defines what WOULD count; it never certifies that it happened.
