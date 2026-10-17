# LADDER-V2 — one-launch-per-type generality check (Claude round-5 item 6b)

Agent GC, 2026-10-10. ANALYSIS/DOCS ONLY (Mac on battery; no builds, no code,
no console contact). This file SUPERSEDES `MANIFEST.md` and `VERDICT-SCHEMA.md`
(agent FO's 12-mod one-launch-per-mod ladder — both retain their data value as
cited below; only the LADDER plan is replaced). The other inputs remain
authoritative and are cited throughout: `mod-ladder/STATIC-SCAN.md` (FT,
per-mod API census + predicted first failures), `host-lua-harness/RESULTS.md`
(FU, load-clean verdicts), `API-PARITY-TABLE.md` (FS), `agent-FV-keymap.md` +
`keymap/MAPPING.md` (pad/chord availability), `multimod-prep/TEST-PLAN.md`
(EF, staging/rollback mechanics), `WAKE-RUNBOOK-20261010.md` (§10 round-4
launch law), `ACCEPTANCE-LADDER.md` (CK, marker law).

## 0. What changed and why

- **V1 (FO)**: 7 RUN mods, each its own console launch, cumulative, with the
  zzladder probe riding every boot. Cost: up to 7+ user launches, each with
  MFA re-pins and restart discipline — and one launch per mod buys attribution
  that the UE4SS self-test now provides at the API-family level.
- **V2 (Claude round-5 item 6b)**: after the UE4SS self-test (agent GB,
  running in parallel; its `ueselftest/` artifacts will land — do not block on
  them), run ONE REAL MOD PER TYPE from the 13 candidates, ALL TOGETHER in
  ONE launch: event/hook mod, value/array-write mod, config-file (io) mod,
  sustained-tick mod; the Item Spawner covers the menu type. Acceptance law:
  the same pass markers reproduced across 3 launches (one launch + up to 2
  restarts). **This becomes the standard check for every future game:
  profile → self-test → ~3 mods** (§3).
- Attribute mapping: each mod runs in its OWN Lua state (upstream UE4SS
  semantics; one mod's script error is contained to that mod's `[l22]` start
  line), so a single combined launch keeps per-mod attribution clean via each
  mod's distinct markers.

## 1. TYPE → MOD SELECTION

### 1.1 The picks (final five, one launch, all enabled simultaneously)

| Type | Primary mod | Key evidence carrier (FT cite) | Load status (FU) |
|---|---|---|---|
| menu | **TownfallItemSpawner** (resident, never re-upload) | proven baseline; co-residency control (EF TEST-PLAN law) | LOADS-CLEAN (calibration) |
| event/hook | **CheatManagerEnablerMod** (1,407 B TREE-B LF) | `RegisterHook(ClientRestart)` at load :3 + `self:get()` :4 + object-ref Set :27 (FT §2) | LOADS-CLEAN; hook fired clean (already-valid branch) |
| value/array-write | **ConsoleEnablerMod** (3,173 B TREE-B LF) | array growth write `ConsoleKeys[#ConsoleKeys+1].KeyName` :36 — "deepest write in the ladder" (FT §3) + built-in read-back print :42 | LOADS-CLEAN; load-time EIT(CreateConsole) + hook + UnregisterHook all clean |
| config-file (io) | **TownfallMiniMap v3.1** (196,743 B TREE-A CRLF, sha256 d8f8d64f…) | the ONLY RUN-list io consumer (FT gap #5; FS §5); `io.open(settingsPath,"r")` :251 nil-degrade, save `:293` pcall+assert | LOADS-CLEAN on composed source (io open per FT §0.1.3); io-off FAILS-AT :251 brackets the absent case |
| sustained-tick (primary) | **TownfallMiniMap** — same mod, second type | `LoopInGameThreadWithDelay(TICK_MS=50, safeTick)` :4268 unguarded at load (FT §6) — the LIGWD engine, P7's whole question | as above; Loop* recorded-not-invoked host-side (FU limitation) |
| sustained-tick (secondary) | **StaminaMod** (2,811 B, sha256 7667fed7e4d7b4cfa9c9f0ed2a794ac815f2121218f43f41192147f62294e335, identical in all 3 local copies; FT §E.1) | `LoopAsync(1500, …)` :89 — sustained 1.5 s loop on the PROVEN engine (spawner family, FT category (a)) | LOADS-CLEAN (FU bonus row) |

Counting: FOUR new mods (CheatManagerEnabler, ConsoleEnabler, MiniMap,
StaminaMod) + the resident spawner = FIVE. MiniMap legitimately carries two
type slots — see the overlap analysis §1.3.

### 1.2 Selection rationale and rejected alternatives

**event/hook → CheatManagerEnablerMod.** Exactly one `RegisterHook`
(`/Script/Engine.PlayerController:ClientRestart`, :3), fired by save load
(pawn possession) — no keybind, no pad row, no user step beyond loading the
save. The hook body also exercises the hook-param unwrap (`self:get()` :4,
RemoteUnrealParam — FT gap #6) and, on the construct path, the object-ref
property Set (:27 — FT gap #7) plus 2-arg StaticConstructObject (:23). FU
fired this exact hook cleanly host-side. Its three failure shapes each name a
distinct owner (resolution / SCO / hook machinery), giving crisp attribution.
REJECTED: ModMenu (9 hooks — richest but heaviest; its nav binds co-fire with
the spawner, see §1.3.1; a single hook is the type's core, not nine);
MiniMap's :515 / ModMenu's hooks are pcall-guarded (a guarded failure degrades
to a logged miss — weaker type evidence than an unguarded, marker-producing
hook); ConsoleEnabler's hook rides along anyway as corroboration.

**value/array-write → ConsoleEnablerMod.** The single deepest write in the
pool (FT §3): TArray `#` length + Index read (:29/:41), **array growth
NewIndex + struct-field Set** (`ConsoleKeys[#ConsoleKeys + 1].KeyName =
KeyName`, :36 — FT gap #4 "deepest write"), FName ctor FNAME_Find variant via
`UEHelpers.FindFName` (:12-13, gap #10), `== NAME_None` / FName Equal
compares (:27/:30), FName userdata `:ToString()` (:42, gap #1), 2-arg SCO
(:57), and object-ref Set `GameViewport.ViewportConsole = CreatedConsole`
(:60, gap #7). Crucially the mod has a BUILT-IN READ-BACK: after writing it
re-reads the whole array and prints every entry (:41-43) — "write sticks" is
proven by the mod's own print, not by inference. The load-time
`ExecuteInGameThread(CreateConsole)` (:75, proven dispatch family) triggers it
without any key press. REJECTED: SplitScreenMod's bool Set (:13-14) — a
strictly simpler write (no array growth, no FName roundtrip), whose keybind
features (CTRL+Y/U/I) would add 3 PS5Pad.ini chord rows and a side-effectful
`CreatePlayer` if ever fired. SplitScreen stays the documented FALLBACK for
this slot (its Init-at-load bool Set + `UseSplitScreen: true` read-back print
is self-contained) if ConsoleEnabler's engine-object construction proves
disruptive.

**config-file (io) → TownfallMiniMap.** The only io consumer that matters
among runnable mods (FT gap #5: of the four io consumers, two are SKIP mods,
dump_object's io is a sidecar `.txt` save, leaving MiniMap's settings as the
only real config round-trip). FS §5: "the ONLY RUN/LOAD mod that needs io."
Read path :251 is nil-degrading (failed open → settings disabled, mod behaves
on defaults); write path :293 is pcall-wrapped with `assert(io.open(…, "w"))`
inside — a failed write is swallowed by the mod's own pcall, so the settings
save is non-fatal to the HUD (verified in source this pass). FS §4.2/§5
caveat: on the PS5 native-source mode the settingsPath is a
`debug.getinfo`-derived logical path with a literal `"\\settings.txt"`
(:241-245) that fopen cannot resolve → read fails open (non-fatal) and the
write throws inside the pcall until the io shim (§5 design, NOT in round-4)
lands. The io-type PASS bar is therefore tiered — see §2.3.

**sustained-tick → who truly ticks (the overlap check, as ordered).**
Census of long-running loops across the 13 candidates (FT):

| Candidate | Long-running loop? | Engine | Observable tick evidence |
|---|---|---|---|
| TownfallMiniMap | YES — `LoopInGameThreadWithDelay(50, safeTick)` :4268 (+ `(1, M.frame)` :4034) | LIGWD (BOUND-UNTESTED, P7) | YES — the 50 ms tick drives the live minimap HUD (panel visible + markers/player indicator updating continuously; `S.tickN` counts internally, FT §6) |
| TownfallModMenu | YES — `LoopInGameThreadWithDelay(100, gameplayTick)` :2431, pcall-guarded with `LoopAsync(100,…)` fallback :2437 | LIGWD + LoopAsync | NO without its menu — gameplayTick (:2364) no-ops unless features are toggled via the menu, which needs BACKSPACE/UP/DOWN/RETURN |
| StaminaMod | YES — `LoopAsync(1500, …)` :89 | LoopAsync (PROVEN, spawner) | Weak — loop body is gated by `InfiniteStaminaActive = false` (never set true in this file) and `Notify()` (:51) is never called: the loop registers, re-fires, and survives, but prints nothing per tick |
| SplitScreenMod | **NO LOOP AT ALL** — keybind + ExecuteInGameThread driven only (FT §5 census: zero Loop*/delay calls) | — | — |
| Spawner / others | spawner LoopAsync only; CheatManager/ConsoleEnabler are event-driven | LoopAsync (proven) | — |

Findings:
1. **SplitScreenMod does not tick** — it is disqualified for the
   sustained-tick slot (it is a value-write candidate only, and that slot is
   better filled).
2. **ModMenu ticks but is rejected for the combined launch**: (a) its
   BACKSPACE/UP_ARROW/DOWN_ARROW/RETURN binds co-fire with the spawner's nav
   binds — the spawner-free-boot law (EF finding 5; FO VERDICT-SCHEMA RUN-7)
   is INCOMPATIBLE with a launch that keeps the spawner as menu type +
   control; (b) input-mode mutations (`SetInputMode_GameAndUIEx`,
   `SetGamePaused` in its AI/fog features) risk disturbing the spawner menu
   and the MiniMap HUD; (c) 9 hooks + ForEachUObject + NotifyOnNewObject +
   ProcessConsoleExec — the heaviest co-residency surface in the pool, in the
   launch that needs clean attribution; (d) with its binds unmapped its tick
   is silent — 128 KB of dead weight. ModMenu remains the designated
   spawner-free follow-up session if distinct LIGWD-in-a-second-mod evidence
   is ever needed.
3. **MiniMap is the only candidate whose sustained tick is observable**
   (EF S9 USER-VISIBLE-first law; MiniMap's sole print is its failure line
   :4270 — acceptance is the live HUD updating over time, FT/FU both note
   this). It is therefore the sustained-tick PRIMARY even though it is also
   the io pick.
4. **StaminaMod is the sustained-tick SECONDARY** (the 13th candidate): the
   lowest-risk real mod in the pool (FT §E.1), zero keybinds/hooks/io, 2,811
   B, LOADS-CLEAN (FU). It adds engine-family breadth to the tick type
   (LoopAsync vs LIGWD) and de-risks MiniMap being the sole tick provider —
   if MiniMap fails to load (its :4268 LIGWD call is unguarded at load, FT
   §6), the launch still carries a LoopAsync sustained loop. Honest limit,
   stated: stamina's loop body no-ops by design, so its evidence is
   registration + session survival (its init print + the ABSENCE of error
   chunks), not per-tick output; per-tick counts come from MiniMap's visible
   behavior and the self-test's delay-family rows.

### 1.3 ONE-LAUNCH-ALL-TOGETHER — interference assessment

Interference data is static (FT census + FV keymap + FS parity); FU's harness
ran each mod in an isolated OS process, so no host co-residency execution
evidence exists — the residual risk is exactly the pump/hook-chaining
behavior that this launch is designed to expose.

1. **Keybinds — ZERO new PS5Pad.ini rows.** Spawner: F7 (L3+R3) + nav
   (UP/DOWN/RETURN/LEFT/RIGHT) all on console-proven PINNED rows (FV). The
   four new mods register ZERO `RegisterKeyBind` keys: CheatManager and
   MiniMap and Stamina have none at all; ConsoleEnabler's Tilde/F10 are
   ENGINE `InputSettings.ConsoleKeys` additions, not UE4SS binds — invisible
   to the pad extractor by design (FV divergence note; MAPPING.md §2).
   Consequence: the live 3,718 B console-proven PS5Pad.ini stays
   byte-identical — no regeneration, no MFA re-pin of its config row, and no
   key chord can accidentally fire any mod. FV's GENERATED-BASELINE.ini
   remains an unused staging candidate. This eliminates the pad-chord budget
   and no-chord-contains-chord risks entirely.
2. **Hooks — chained ClientRestart, one watch-item.** CheatManager and
   ConsoleEnabler BOTH hook
   `/Script/Engine.PlayerController:ClientRestart` (CheatManager :3;
   ConsoleEnabler :79). Chained same-function hooks are standard UE4SS
   semantics (both fire; each mod's markers are distinct). WATCH-ITEM:
   ConsoleEnabler unhooks after its first fire (`UnregisterHook(path, Pre,
   Post)` :86 — BOUND-UNTESTED, P6 family). If the port's UnregisterHook
   mis-removes the shared chain, CheatManager's hook could go silent on
   LATER restarts — the 3-restart law is the detector: CheatManager's marker
   must appear in EVERY launch's ClientRestart window, while ConsoleEnabler's
   is expected only in the first per-boot window (by design, WasConsoleCreated).
   MiniMap adds one pcall-guarded RegisterHook (:515) and two guarded
   NotifyOnNewObject (:4248-4249); the spawner carries its LoadMapPreHook.
   Total hook surface ≈ 5 registrations — modest.
3. **Console handlers — none.** No selected mod registers a
   RegisterConsoleCommandHandler (ConsoleCommandsMod is deliberately NOT in
   the set; its trigger would need the console UI). ConsoleEnabler's UConsole
   construction + ConsoleKeys append cannot open on PS5 (no pad row
   synthesizes Tilde/F10), so there is no input-layer interference; its
   acceptance is log-based (FT §3 note).
4. **Game-state mutations.** CheatManager: one object on the PlayerController
   (invisible). ConsoleEnabler: `ViewportConsole` object-ref Set (engine
   state, no visible effect expected — FT). MiniMap: HUD overlay
   (AddToViewport) + `SetGamePaused` ×3 / `SetInputMode_GameAndUIEx` — all
   reachable only through its input-key polling, which is pcall-guarded and
   degrades (`S.pc = nil`, FT §6); on PS5 the FKey struct-param shape is
   BOUND-UNTESTED, so the map menu may simply not open — non-fatal. Session
   guidance: exercise one UI at a time; if both the spawner menu and
   MiniMap's map are ever open, close one before the other.
   Stamina: nothing (loop body no-ops). Spawner: SetIgnoreMove/LookInput
   while its menu is open (proven behavior).
5. **Tick co-residency.** Three concurrent loops: MiniMap LIGWD(50 ms) +
   Stamina LoopAsync(1500 ms) + spawner LoopAsync (proven). One game-thread
   LIGWD carrier + two async-thread LoopAsync loops — different carriers, no
   collision; combined load ≈ 20 Hz + 1.4 Hz. If the shared game-thread pump
   has a co-residency defect, MiniMap's tick stalls while the spawner's F7
   dispatch keeps working — the control distinguishes pump-wide from
   LIGWD-specific.
6. **io.** Only MiniMap touches io; its failures are self-contained
   (nil-degrade read, pcall-swallowed write). No other mod's behavior varies
   with io state.
7. **Load-time ordering.** All five load at boot in loadlist order; a script
   error in one mod is contained to its own Lua state (§0). The riskiest
   load-time call in the set is MiniMap's unguarded `:4268` LIGWD — a red P7
   names itself (error chunk at that line; owner LuaMod.cpp:6564 carrier,
   FT §6) without touching the other four mods' evidence.

**INTERFERENCE VERDICT: GO.** No shared keybinds, no console-handler
contention, no un-guarded cross-mod state mutation; two watch-items (the
ClientRestart unhook chain, MiniMap's guarded pause/input paths) are both
observable in the harvest and none can corrupt another mod's markers.

## 2. PASS / FAIL per mod per type (exact log lines)

Evidence law (CK): every verdict needs the actual line in the harvested
UE4SS.log (`grep -a` always — mixed-encoding logs) or, where marked
USER-VISIBLE, the player's on-screen observation/screen capture. klog clean
gates every launch. Absent lines read ABSENT, never guessed.

### 2.0 Admission (all five, every launch)

- `[l22] Lua mod started: <ModName> (main script returned)` for each of the
  five mods, after `[l25] event loop started` + `Starting callback garbage
  collector!`.
- Stamina's own load line: `[Stamina] Mod script initializing...` (main.lua:9).
- FAIL shape: missing `[l22]` line for a mod + the Lua error chunk in the
  harvest names the failing line (each mod's predicted-first-failure line per
  FT: CheatManager :3, ConsoleEnabler :36, MiniMap :4268/:251, stamina none).

### 2.1 event/hook — CheatManagerEnablerMod (trigger: save load → ClientRestart)

- **PASS** = hook fired + param unwrap + (construct path) write landed:
  - `[CheatManagerEnabler] Constructed CheatManager [0x…]` (:25) AND
    `[CheatManagerEnabler] Enabled CheatManager` (:28) — full proof: hook
    fire, `self:get()`, property reads, 2-arg SCO, object-ref Set (:27);
  - OR skip-shape `[CheatManagerEnabler] CheatManager already exist,
    skipping restoration` (:7) = hook FIRED + unwrap worked = PASS with note
    (FO VERDICT-SCHEMA S5; FU observed exactly this branch host-side).
- **FAIL shapes** (owner named by the line):
  - silence after in-world wait → hook registered but never dispatched
    (hook-dispatch owner; cross-check spawner still works);
  - load error near :3 → hook registration (P6);
  - `Controller:CheatClass is nullptr, using default CheatClass instead`
    (:13) alone is benign; followed by `Couldn't find default CheatClass…`
    (:19) → resolution owner;
  - `Was unable to construct CheatManager…` (:30) → SCO owner.

### 2.2 value/array-write — ConsoleEnablerMod (trigger: load-time EIT :75 + ClientRestart re-fire)

- **PASS** = write sticks, proven by the mod's OWN read-back:
  `[ConsoleEnabler] ConsoleKey[%d]: %s` lines (:42) listing the game defaults
  PLUS the appended **Tilde** and **F10** rows. That single print family
  proves: FindFName/FNAME_Find ctor (:12-13), `#ConsoleKeys` length (:29),
  Index read + FName `==` (:30), the array growth + struct-field Set (:36),
  and FName `:ToString()` (:42). Secondary: absence of
  `ConsoleClass, GameViewport, or ViewportConsole is invalid` (:64) and of
  `[CreateConsole] Was unable to construct an UConsole object` (:58) ⇒ the
  `ViewportConsole = CreatedConsole` object-ref Set (:60) path completed.
- **FAIL shapes**:
  - `[ConsoleEnabler] InputSettings not found, could not change console key`
    (:23) → CDO resolution owner;
  - `[ConsoleEnabler] Was unable to find an instance of UEngine` (:48) →
    GetEngine/FindFirstOf owner;
  - error chunk naming :36 (the `for` at :26 is NOT pcall-wrapped — FT §3) ⇒
    TArray NewIndex / struct-field Set owner; the :42 prints never appear;
  - mod started but no ConsoleKey lines and no failure print after full
    load + one save load → EIT-dispatch timing (retry-in-world per FM11
    precedent, ONE in-boot retry allowed, never a relaunch).

### 2.3 config-file (io) — TownfallMiniMap (trigger: load; settings round-trip optional)

- **io PRESENT (P0-equivalent)**: the self-test's io rows PASS (`type(io)` —
  FT §0.1.3: io is structurally linked, patched Sony liolib; GB's
  `ueselftest/` artifact carries the row names). Mod-level: MiniMap loads
  with NO error chunk naming :251 — a failed settings OPEN is expected to
  degrade to nil (non-fatal; settings disabled, mod runs on defaults =
  "the mod behaves").
- **ROUND-TRIP (tiered by shim availability)**:
  - if the io shim / writable mod-dir path is in the build: open MiniMap's
    map menu → change a setting → clean exit → relaunch → the setting
    persists (loadSettings :251 reads back what saveSettings :293 wrote) =
    FULL io PASS;
  - if not (expected for round-4): io verdict = **PARTIAL — lib present,
    paths degrade non-fatally**: no `:251`/`:293` error chunk (the write is
    pcall-swallowed, the read nil-degrades) + self-test io rows PASS.
    Record as documented-gap, owner = the §5 io-shim design (FS); do NOT
    count MiniMap's inert settings as a failure of the port's io linkage.
- **FAIL shapes**: error chunk `attempt to index a nil value (global 'io')`
  at :251 → io lib absent on console (would contradict FT §0.1.3 — a NEW
  finding; FU's io-off mode is the bracketed prediction); `[TownfallMiniMap]
  this UE4SS is too old, install the experimental build` (:4270) → LIGWD
  global missing (kills the tick too).

### 2.4 sustained-tick — MiniMap primary + Stamina secondary

- **MiniMap tick PASS** = ALL of:
  1. absence of the `:4270` too-old line (LIGWD registered);
  2. `[l22] Lua mod started: TownfallMiniMap (main script returned)`;
  3. USER-VISIBLE (screen capture, primary artifact — EF S9 law): the
     minimap panel visible in a corner during play AND updating over time —
     captures at t0 and t0+30 s showing the player indicator/markers moved =
     the 50 ms `safeTick` callback count increasing, made visible. The mod
     has NO other print (its sole print is the failure line).
  4. UE4SS.log free of Lua error chunks from MiniMap across the session
     (corroboration only, never sole acceptance).
- **Engine-level tick counts**: the self-test's delay-family rows (EITWD /
  LIGWD tick counters; zzladder P7 precedent `[P7] ligwd 1` / `2`) — read
  from GB's `ueselftest/` artifact when it lands.
- **Stamina secondary PASS** = `[Stamina] Mod script initializing...` at load
  + ZERO `[Stamina]` lines and zero async-error chunks for the whole session
  (LoopAsync(1500) registered and survived; body no-ops by design — stated
  limit, §1.2). Any async error naming stamina's closure = FAIL (LoopAsync
  carrier owner).
- **FAIL shapes**: `:4270` line present → no LIGWD; MiniMap mod-start missing
  with error at :4268 → LIGWD call-site error (owner LuaMod.cpp:6564); panel
  absent while mod started and no error chunk → tick registered but
  widget/pump failure (capture + harvest; distinguish via spawner F7 pump
  control).

### 2.5 menu + control — TownfallItemSpawner (resident, original bytes per FN freeze)

- `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7
  opens the menu.` at load; `[TownfallItemSpawner] Menu created.` on L3+R3;
  optional spawn rung per ACCEPTANCE-LADDER rung 3. **Control law**: the
  spawner's markers must keep passing WHILE the four run — co-residency is
  itself part of the claim (EF TEST-PLAN). Spawner regression = the combined
  launch's first suspect list (pump/hook-chain interference), not a spawner
  bug.

## 3. THE STANDARD CHECK — canonical procedure (reusable for every new game)

> This section is the doc-of-record for the pipeline: **profile → self-test
> → ~3 mods**, then the 3-passes-across-restarts law.

### 3.1 Profile (per new game)

1. **Acquire the decrypted eboot** via the documented decrypt-on FTP download
   (default connection); provenance cross-check on a second connection with
   `-Q "SITE SELF"` (decrypt-off) recording BOTH sizes + sha256s. NEVER
   upload any downloaded/modified executable back (POSTMORTEM rules).
2. **Engine classification** — ref_axes (244-marker engine classifier): a
   non-UE binary must REFUSE (validated on Astro Bot: 0/244 markers, no
   anchors — FZ dry-run). No profile, no ladder.
3. **Anchor pins** — GUObjectArray / GMalloc / FName ctor / vtable resolvers;
   every address classified by PT_LOAD span; multi-pattern agreement
   required; unknown/mismatch/conflict = refusal.
4. **Member/vtable offsets** — static-layout scans, two-source agreement.
5. **Unit emission** — `generate_profile.py` emits the RuntimeBuildProfile /
   layout units; profile's own `tests/run_tests.py` green; build the
   composition against the new profile. Live-base caveat: PS5 eboots are
   ASLR'd (`ET_SCE_EXEC_ASLR`) — the base must be resolved live before any
   pin is used.

### 3.2 Self-test (the UE4SS parity self-test, agent GB)

Built into the PRX (`ueselftest/`); run in-session via its combo press.
**PASS bar (proposed, for adoption):**
- **>= 95% of all self-test rows PASS**, AND
- **0 FAIL in the four type-critical families**:
  1. **hook family** — RegisterHook registration + fire + param get +
     UnregisterHook;
  2. **property/array write family** — bool/struct-field Set, array growth
     write, FName ctor/roundtrip/compare, object-ref Set;
  3. **io family** — `type(io)`/`type(os)` + open-read-write-close
     round-trip inside a mod folder;
  4. **delay/loop family** — ExecuteInGameThreadWithDelay,
     LoopInGameThreadWithDelay with per-tick counts, LoopAsync.
- A FAIL OUTSIDE the critical families: record it; proceed only if no
  selected mod touches that API (judgment recorded in the receipt).
- A FAIL INSIDE a critical family: STOP — fix the owner, rebuild, re-run the
  self-test; mods are never run against a red critical family (the V1 S4
  gating law, carried over).
- If the self-test missed a build window: the launch may still run; type
  evidence then rests on the mods' own markers (§2), with the self-test
  rows appended when the next build lands — note the gap in the verdict.

### 3.3 ~3 mods — selection rules for an unknown game's mod pool

1. One REAL mod per type (event/hook, value/array-write, config/io,
   sustained-tick; menu if a UI mod exists), prefer game-community mods over
   stock examples. Target 3 new mods; accept 3-5 — a single real mod may
   cover two types when each has DISTINCT evidence streams (MiniMap: io +
   tick); never count one evidence stream twice.
2. **Smallest mod whose PRIMARY function is the type** (CheatManager 1.4 KB
   over ModMenu 128 KB for hooks). Load-clean host-harness status preferred
   where available.
3. **Built-in observable required**: the mod's own print/read-back marker for
   the core function, or user-visible function. A silent mod proves nothing.
4. **Exclusion laws (never selected)**: hardcoded engine offsets (offset
   law — ActorDumper precedent); crash-law APIs (FindFunction/bpfn/
   class-children — grep the pool before selection); BP-loader dependencies
   (no caller can exist); profiler/debug-hook mods; core-function-unreachable
   machinery (pak mounting).
5. **Pad footprint**: prefer zero-keybind mods; add PS5Pad.ini chord rows
   only when the type itself needs a keypress (e.g. a keybind-triggered
   type), and then via the FV generator + MFA re-pin, honoring the
   no-chord-contains-chord antichain law.
6. **Interference screen before the combined launch**: shared keybinds
   (co-fire law), same-function hooks, console-command handlers, input-mode/
   pause mutations, multiple tick engines, io contention — the set must keep
   per-mod attribution clean (distinct markers per mod).
7. Mods are NEVER edited. A FAIL routes to the port-side owner named by the
   failing marker; fallback/alternate mods are staged only as new distinct
   rows in a subsequent cycle.

### 3.4 The 3-passes-across-restarts acceptance law

- A type verdict after launch 1 is **PROVISIONAL-PASS**.
- **ACCEPTED** requires the same pass markers in **3 total game launches**
  (the original + up to 2 restarts), each a user-launched, clean-exit session
  with klog clean. Flaky = failed: any launch's FAIL makes the type verdict
  FAIL, owner named from the marker.
- Staging must be byte-identical across the 3 launches (same loadlist rows,
  same file bytes). A PRX/build change RESETS the cycle: new build ⇒ new
  3-launch count (label suffixes `-r1..r3` per build).
- Never rapid launches: one launch per clean boot; ploader Z2 ≤ 120 s wait if
  `payload.elf` resident; a crash (CE-108255-1) ⇒ harvest, note code + WHEN,
  REBOOT before anything; the crashed launch's verdict is FAIL at the crashed
  stage regardless of earlier passes.

## 4. CONSOLE TEST PLAN (one launch, then up to 2 restarts)

### 4.0 Preconditions (before ANY staging)

- Round-4 PRX built + manifest-PASS (`verify_build.sh --manifest`) + deployed
  with decrypt-off readback (magic 4f153d1d + sha256 match) per WAKE-RUNBOOK
  §10.3 — round-3 stays fallback. FY equivalence check if concurrent builds
  ran.
- Spawner original bytes verified against the FN freeze doc
  (`evidence/townfall-main.lua.console-copy`).
- Console awake, no game running (`processes_list` eboot count 0); daemon
  census; restart klogsrv ONCE via loadpayload if dead; klog tap (TCP 3232,
  NEW file per launch) running BEFORE launch.
- Keep whatever probe mod is currently resident (zzdiag-v11) — its F7 press
  is free corroboration ([zz]/[PRB-] families); do not add/remove rows for
  it as part of this plan.

### 4.1 Staging (game CLOSED; one-time, before launch 1)

Files (text-only — no SITE SELF concern; upload to the mod root
`/data/modbridge/roots/PPSA33286/ue4ss/Mods/…`):

| Mod | Source copy | Bytes (wc -c of exact uploaded bytes) |
|---|---|---|
| CheatManagerEnablerMod | TREE-B LF `…/ue4ss/assets/Mods/CheatManagerEnablerMod/Scripts/main.lua` | 1407 (+ enabled.txt 0) |
| ConsoleEnablerMod | TREE-B LF `…/assets/Mods/ConsoleEnablerMod/Scripts/main.lua` | 3173 (+ 0) |
| TownfallMiniMap | TREE-A CRLF `…/townfall-ue4ss-real-script-audit-1/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` (sha256 d8f8d64f…) | 196743 (+ 0; ships enabled.txt) |
| StaminaMod | `…/real-mod-lua-runner-1/inputs/stamina_main.lua` (sha256 7667fed7…), staged as a NEW mod dir | 2811 (+ 0) |

Loadlist: pull the LIVE loadlist and reconcile first (EF law); add exactly 8
file rows (4 × main.lua + 4 × enabled.txt), `wc -c`-pinned size-exact (MFA
law — any mismatch = `mfa_activate_failed code=-8`, game continues unmodded).
UEHelpers row untouched (resident, never re-upload). **PS5Pad.ini row
untouched** (3,718 B — zero new binds, §1.3.1). Readback `cmp` every uploaded
file; `grep -c $'\t'` must show the expected file-row count (base 7 + 8 = 15
file rows). Record all rows/bytes in the receipt.

### 4.2 Launch 1 (user; ONE launch, no rapid-fire)

1. LAUNCH Townfall. 2. Load the SAME save. 3. Get IN-WORLD (controls
   character — ClientRestart window: CheatManager + ConsoleEnabler markers
   expected here). 4. Wait ~10-15 s (MiniMap tick spool-up). 5. Press the
   SELF-TEST COMBO once (agent GB's combo, per its `ueselftest/` receipt; if
   GB missed the build window, skip and note it). 6. L3+R3 once: spawner
   menu opens (control rung) — navigate, close. 7. Play 60-90 s; screen
   captures of the minimap HUD at t0 and t0+30 s (tick evidence). 8.
   Optional, only if MiniMap's map menu opens on this build: change one
   setting, note it (io round-trip attempt). 9. CLEAN EXIT via in-game quit
   to dashboard — never a power event. If it crashes: note code + WHEN,
   harvest immediately, REBOOT before any next launch.

### 4.3 Harvest (read-only, BEFORE any relaunch)

```bash
PREP=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep
python3 $PREP/harvest/harvest.py --label ladder2-combo-r1
L=$(ls -t $PREP/evidence/*ladder2-combo-r1*/UE4SS.log 2>/dev/null | head -1)  # fresh harvest log
grep -a "Lua mod started" "$L"                                   # 5 mods admitted
grep -a "CheatManagerEnabler" "$L"                               # hook type
grep -a "ConsoleEnabler\|ConsoleKey\[" "$L"                      # write type
grep -a "TownfallMiniMap\|too old" "$L"                          # io/tick type (absence law)
grep -a "\[Stamina\]" "$L"                                       # tick secondary
grep -a "Menu created\|Loaded v1.0" "$L"                         # spawner control
grep -a "error\|Error" "$L" | grep -a -v "no error" | head       # error chunks
# klog: $PREP/evidence/runtime/l25-variant-A/klog-*.log (legacy /tmp/l25-variant-A/ —
# now durable at the evidence runtime dir, agent-HA 2026-10-10) — no copyin bursts, no nonsleeping, no fatal backtrace
```

Verdict table per §2 (per type per launch), klog clean gate, screen captures
filed. The self-test's verdict rows are read from GB's `ueselftest/`
artifacts when they land (do not block on them).

### 4.4 Restarts 2 and 3 (the 3-pass law)

Nothing changes between launches (byte-identical loadlist — no re-pin, no
file churn). User launch → save → in-world → self-test combo → 60-90 s →
clean exit; harvest `--label ladder2-combo-r2` / `-r3`. TYPE ACCEPTED = the
type's pass markers present in ALL THREE harvests (§3.4). Watch-item check:
CheatManager's marker must appear in every launch's ClientRestart window
(ConsoleEnabler's only in the first per-boot window — by design).

### 4.5 Verdicts

- Per type: PASS (3/3), PROVISIONAL-PASS (1-2/3 pending), FAIL (any launch's
  fail shape; owner named from the marker per §2). MiniMap io is tiered
  (FULL / PARTIAL — §2.3) until the io shim lands.
- Overall LADDER-V2 pass = all four types PASS-or-PARTIAL(io-tier) across
  3/3 launches + spawner control green in all three + klog clean throughout.
- Any FAIL: harvest + owner named; NO identical re-launch; the alternate mod
  for that type (§1.2 fallbacks: SplitScreen for write; ModMenu
  spawner-free session for tick) is a NEW cycle with distinct rows/labels,
  only after the owner change lands.

### 4.6 Rollback (loadlist rows off)

1. Pull live loadlist; delete the 8 rows added in §4.1 (and ONLY those);
   upload; `cmp` readback; `grep -c $'\t'` back to the base file-row count.
2. Remote DELE of the four mod files optional (unlisted lua files are never
   admitted; note it in the receipt) — same curl pattern as EF TEST-PLAN.
3. PS5Pad.ini: nothing to roll back (never touched). UEHelpers/spawner/PRX:
   never touched by this plan.
4. Next launch confirms: the four `[l22]` mod-start lines absent, no new
   markers, spawner control unaffected, clean exit.

— agent GC (ZCode), 2026-10-10. No console contact, no builds, no code, no
mod edits were made writing this file. FT's STATIC-SCAN, FU's RESULTS, FS's
API-PARITY-TABLE and FV's keymap docs remain the cited data of record.
