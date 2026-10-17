# SESSION ANALYSIS — l26-v2 + v6 console round, 2026-10-08 night (agent CZ)

Read-only over logs/trees. Derived readable copies (mixed ASCII/UTF-16LE decoder,
UTF-16 runs converted, source files untouched): `/tmp/l26v2.norm.log`,
`/tmp/v6-run.norm.log`, `/tmp/l26crash.norm.log`, `/tmp/l25B.norm.log`.
All times below are console-local wallclock (UE4SS log clock) = Mac CDT (UTC-5),
cross-checked against the eboot monitors and klog uptime anchors.

## 0. Executive summary

1. **l26-v2 is the first fully-readable UE4SS session ever** (CL logwriter fix
   confirmed working: zero interleaved/truncated lines, full tracebacks). It ran
   the post-revert 21:18 build (PRX e13b1d0b…), ended by a NORMAL user quit.
2. **The GC storm did NOT survive the revert** (or was never pathological):
   l26-v2 delete-listener counter reached 349 in 54 s (~6.4/s, menu churn) vs
   l26-v1's 6,181 → 13,970 (≈100–128/s) — and l26-v1's counts coincide exactly
   with the only save/level load of the night. No crash in either.
3. **l26-v1 was NOT a kernel crash.** Klog shows pid 110 ended via the normal
   suspend/quit path (no signal, no coredump, no SYSCORE_APP_CRASH). Its log
   tail died at 19:38:57 under the OLD logwriter during save-load churn; the
   game ran until the user quit it (~19:39:09).
4. **The only real crash tonight** is an unlogged 20-second boot crash at
   20:58:56–20:59:15 (pid 305, RTHeartBeat thread, SIGSEGV executing
   libkernel.sprx+0x10ec, xotext page-not-present; coredump attempt failed
   0x80aa000c). It launched with the ploader already dead → effectively
   unmodded, pre-UE4SS-logging. Klog-only evidence; monitor polls missed it.
5. **All FindXOf probes tonight were menu-timed** (no save loaded in l26-v2 or
   v6; l26-v1's single probe also preceded its save load). db=invalid /
   ks=invalid / N1–N4=-1 are the CA-predicted menu-state results (CDO-only,
   excluded by IsValidObjectForFindXOf). **No in-game FindXOf datapoint exists
   in any session to date.** Zero [FFO]/[FN]/[UF] markers anywhere.
6. **The white dot was not drawn by the mod** — buildUI() provably exits at the
   getPlayerController() gate before any widget construction; zero widget log
   lines. Ranked hypotheses in §3.

## 1. Session map (all five game launches, tonight)

Console booted ~19:14:22 (uptime anchors from klog EXEC lines converge:
boot ∈ [00:14:14, 00:14:30] UTC — the post-19:09-crash reboot; monitor shows
daemons missing 00:15:31Z, payload reload 00:15:46Z). No further reboots
tonight (kstuff=87/pldmgr=85/ftpsrv=89/klogsrv=96 constant in both receipts).

| # | Session | Game pids | Window (CDT) | Payload/PRX | UE4SS log | End |
|---|---------|-----------|--------------|-------------|-----------|-----|
| 1 | l26-v1 (first l26 build) | 110 (+cd 113) | 19:34:38–19:39:09 | injector 109 armed 19:34:19; pre-crash1 tree (OLD logwriter) | 19:36:02–19:38:57 (`/tmp/l26-crash.log`) | user quit (normal path); log tail lost to old writer |
| 2 | ghost A (crash) | 305 | 20:58:56–20:59:15 (~20 s) | NONE — ploader 304 died 20:49:49 | none (died pre-UE4SS-logging) | **REAL SIGSEGV**, RTHeartBeat, libkernel.sprx+0x10ec; coredump failed 0x80aa000c |
| 3 | ghost B | 332 | 20:59:36–21:01:51 | none (ploader still dead) | none (no injection) | user quit (normal path) |
| 4 | **l26-v2** | 353 (15 s) → 364 | 21:23:10–21:26:10 | injector 363; PRX **e13b1d0b** (21:18 build, post-revert) | 21:24:50–21:26:08 (`/tmp/l26v2.log`) | user quit (normal path) |
| 5 | **v6** | 376 (15 s) → 387 | 21:29:26–21:31:41 | injector 386; same PRX e13b1d0b; zzdiag v4→v6 | 21:31:02–21:31:43 (`/tmp/v6-run.log`) | user quit (normal path), overlapping the 2nd probe |

Receipt provenance: `harvest/LAUNCH-READY-A-20261009T014749Z.md` (20:47:49 CDT;
PRX 23c8483d… — **never actually executed**: its only launch windows, sessions
2–3, had a dead ploader) and `harvest/LAUNCH-READY-A-20261009T021939Z.md`
(21:19:39 CDT; PRX e13b1d0b…, resident for sessions 4–5). The two-stage eboot
pattern (stage-1 pid ~15 s, then stage-2 pid) is normal for this title and
repeats in every session.

The "revert": `typescan-r4/candidate/include/LuaType/LuaUObject.hpp:821-829` —
"l26 postmortem revert (root): FNAME_Add for MEMBER names … returns to upstream
FNAME_Find on ALL platforms". A CHANGE-SELECTION diff shows LuaUObject.hpp was
the ONLY source row that changed between the never-run 20:46 build and the
deployed 21:18 build. So both readable sessions = post-revert binary.

## 2. l26-v2 full timeline (`/tmp/l26v2.log`, 1000 lines, complete)

**Init (21:24:50–21:24:53)**
- 21:24:50 UE4SS v3.0.1 (SHA e3ba1016562d6c0868c4); native console/GUI/crash
  dumping/fs watching unavailable; config /data/plugins/UE4SS-settings.ini;
  mods dir ue4ss/Mods.
- 21:24:51 [NativeLayout] + [Seeds] slots armed; ~445 BuildProfile PA checks
  (image 3b6cfad4…, base 0x400000, engine 5.6, all PA); LayoutPreflight
  checked=66 missing-required=0.
- 21:24:51 Pool admission elapsed=2646 ms → **admitted**; FName pool validated
  at 0x9765ec0 ('None' id 0x0, 'ByteProperty' id 0x3, CurrentBlock 68,
  Blocks[0] 0x22C0000000); L18 ctor signature authenticated; L15 FName output
  verified (ByteProperty probe id == pool id); L25 copyin STOPPED (elapsed
  2710 ms, oracle-probes=0); slot 4 StaticConstructObject_Internal bound
  0x890aa0.
- 21:24:51 Object construction waits; KismetStringLibrary + CDO +
  Conv_NameToString located (0x3000A01D90 / CDO 0x3000AC0F40); CDO list incl.
  /Script/UMG Widget + ComboBoxString; GameEngine 0x19000390A0, GameModeBase,
  GameViewportClient; LoadMap 0x3c4d460; GameEngine::Tick vtable-only (signature
  scan failed → fallback, pre-existing); R3 refuses EngineTick pre-callbacks;
  InitGameState 0x33db2b0; ProcessEvent 0x859830.
- 21:24:53 TypeScan complete (missing-keys=0, core-ready=true); FText 0x10,
  FName 0x8 align 4; MEMBER OFFSETS block prints the l25-corrected values
  (UObjectDeleteListeners=0x68, UObjectCreateListeners=0x58,
  UClass::ClassCastFlags=0xD8 — staged profile value live, CD's port-header
  0xD0 discrepancy never surfaces).
- 21:24:53 [HashTables] FUObjectHashTables unavailable → GUObjectArray
  lookups; ProcessLocalScriptFunction N/A; GNatives not found.
- 21:24:53 **DeleteListener registrations** (all markers, no-re path,
  growth_required=0 every time, GMalloc values identical to l25):
  class-delete Add Num 1→2 (11 ms, listener 0x827edfb8); shutdown Add Num 2→3
  (12 ms, 0x827edfc0).
- 21:24:53 "Starting callback garbage collector!"; StaticConstructObject
  ObjectSearcherPoolHook **Failed to add hook** (pre-existing; also in
  l25-B-final).
- 21:24:53 [l25] event loop started; on_program_start entered; **third Add**
  Num 3→4 (listener 0x827ede40, marker-less — the callback-GC path, same shape
  as l25's third Add).
- 21:24:53 R3 carrier scan: Actor:ReceiveTick and
  AnimInstance:BlueprintUpdateAnimation **not found** ("not interned, or class
  not constructed yet"); 4 FuncPtr carriers armed — names as RAW FName ids
  ([n:F7BD]×2, [n:28D0]×2; fname_tostring unbound by design). LoadMap
  deferral note (R3 owns the pump).
- 21:24:53 Lua type-size exposures (one failure: 'N2RC6Unreal18FOb' internal
  value invalid — benign); spawner keybinds 118/38/40/13/37/39 registered;
  "[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release."; EIGT
  self-test queued ref=12 (l23, deadline 30000 ms); zzdiag v4 ([zz] Z0 v4) +
  its F7. Event loop start.

**Play (21:24:59–21:26:08)**
- 21:24:59 engine-called listeners: Object 0x1600467400 Index=40482 **calls=2**
  (both FShutdown + FClass print together).
- 21:25:05 marshal hook first invocation (carrier fired; **12 s warm-up**,
  boot-phase as in l25-B); LoadMap.LuaModImpl hook Failed ×2 + deferred arm
  refused (pre-existing R3, loud by design); self-test drained
  thread=101788 **drain=11600 ms** late=false; returned.
- 21:25:11 **F7 #1**: spawner EIGT queued ref=12 → drained → "[TownfallItemSpawner]
  Scan failed: …main.lua:66: Item database not ready. Load a save first." →
  returned (same second, no queue→drain gap). zzdiag: K1 → queued ref=6 → K2 →
  drained → `D1 db=invalid`, `S1 ks=invalid`, `S2 conv=no`, S3 chunked error
  "Tried calling a member function but the UObject instance is nullptr"
  (in method 'Conv_NameToString', main.lua:32), D2 same error (in method
  'GetItemIDFromDeprecatedEnum', main.lua:42) → returned. **Error text is the
  CN-hardened null-wrapper text — the instances (db, ks) are the null side.**
- 21:25:12–15 nav keys 40/39/38 (callbacks no-op: menuOpen false).
- 21:25:16 F7 #2 — identical cycle, identical results.
- 21:25:37–38 nav 40/38/39/37; F7 #3 21:25:38 (D/S lines spill into :39).
- 21:25:40 F7 #4; 21:25:41 F7 #5 (spill :41–42). 21:25:44–45 nav 39/40/38/37.
- 21:25:53 engine-called listeners: Object 0x16005d1000 Index=71962
  **calls=349** (≈347 deletes in 54 s ≈ 6.4/s — menu-screen churn, NO storm).
- 21:25:57/59 key 13 (Enter) ×2; 21:26:07 F7 #6 (identical); 21:26:08 nav
  40/39/38/37 — final log lines.
- 21:26:10 user quit — klog shows the normal path (APP_SUSPENDED → STOP →
  "Terminating pid=364" → PlayGoCore GameSuspended → SaveData term). **No
  crash, no coredump, no SYSCORE_APP_CRASH.**

**Session tallies**: F7 presses 6 (12 key=118 entries — two per press);
navigation 37×3 / 38×5 / 39×4 / 40×4 / 13×2; mod scan-fails 6 (plus 1 unrelated
init-time "GameEngine::Tick scan failed"); zzdiag lines 43; EIGT 13 queued /
13 drained / 13 returned (0 stranded); engine-called listener lines 2
(calls=2 → calls=349); carrier re-scan: none after init (4 carriers stayed
live; contrast v6 below).

## 3. The WHITE DOT — what drew it?

The user reported a white dot on screen during l26-v2. Established from log +
mod source (`TownfallItemSpawner/Scripts/main.lua`):

- F7 → `toggleMenu` → `showMenu(true)` → `scanItems()` (fails at line 66,
  caught internally, logs "Scan failed") → **`buildUI()` still runs** → first
  statement chain: `getPlayerController()` → `pcall(require,"UEHelpers")`
  path → fallback `FindFirstOf("PlayerController")` → **both nil** →
  `status="Load a save first."; return false` — BEFORE any
  StaticConstructObject/CreateWidget call.
- Corroborated negative: zero buildUI log lines in ANY session
  ("WidgetBlueprintLibrary not found." / "UI creation failed:" / "Menu
  created." — all absent everywhere), and menuOpen never became true
  (navigation keybind callbacks are menuOpen-gated no-ops; their entries log
  with no other effect — exactly what the log shows).
- zzdiag creates nothing (v4 = pure logging probes).

Ranked hypotheses:
1. **The game's own title-screen UI** (menu cursor / selection pip / loading
   dot) — most likely: the whole session stayed at the main menu; no code
   path from the mod stack drew anything.
2. **PS5 system overlay element** (notification / controller hint / capture
   indicator) — possible; invisible to both logs.
3. **Partially-created mod widget** — REJECTED (proof above).
4. **UE4SS GUI** — REJECTED: "GUI … unavailable" logged at init; no GUI
   subsystem in this build.
5. **R3 marshal carrier BP-override side effect** — armed carriers intercept
   function pointers only ([n:F7BD]/[n:28D0] = BP tick/update overrides); no
   draw path; names unresolved (fname_tostring unbound by design).
6. **GPU/compositor artifact of the HEN environment** — klog window shows only
   routine boot lines (PGraphics init, one "[Performance Warning] framedrop
   count : 2" at menu init); no GPU faults. A single-dot compositor glitch
   would not necessarily log; cannot be excluded, only unlikely.

## 4. UEHelpers load-time behavior (task 2)

Verdict: **silent — indeterminate from this log, by construction.**

- Zero UEHelpers-pattern lines in EVERY readable session (l25-B-final,
  l26-v1, l26-v2, v6). No load-time or call-time diagnostics exist to print.
- The spawner's `require("UEHelpers")` is lazy (inside getPlayerController,
  under pcall). Every failure mode — module absent, lookup error, nil return —
  is swallowed silently; the FindFirstOf fallback then fails for the same
  reason as every other FindXOf.
- What can be said: UEHelpers' cached lookups (GetEngine/GetPlayerController
  family) ride the same FindXOf machinery that returns empty for every class
  probed tonight; if require() works, its lookups still cannot succeed.
- zzdiag-v7 (staged locally, not yet deployed) carries U1 (require) / U2
  (GetKismetStringLibrary) / U3 (GetEngine) precisely to discriminate — run it
  IN-GAME (save loaded), not at the menu.

## 5. v6 session (task 3) — all-nil FindAllOf, timings

`/tmp/v6-run.log`, 21:31:02–21:31:43, same PRX e13b1d0b (only the zzdiag Lua
mod changed v4→v6: N-count probes instead of chunked error dumps).

- Init identical to l26-v2 (same digests, same addresses) except: **warm
  pump** — marshal carrier first invocation DURING init (no 12 s warm-up),
  self-test **drain=7 ms** (vs 11600 ms cold in l26-v2; confirms l25 §6
  warm-up is boot-phase), and **74 carriers armed at init** (vs 4 in l26-v2 —
  more BPs interned by init time), then a lazy re-arm at the first press
  (+66 found, 138 armed total). No re-arm in l26-v2 (its 4 carriers held).
- 21:31:08 engine-called listeners calls=2 (Index=35550 — same object/index as
  the l25 cache-on session).
- **Cycle 1, 21:31:23**: F7 → spawner "Scan failed" (line 66, same) → zzdiag
  `N1pc=-1  N2db=-1  N3en=-1  N4ks=-1` — **all four FindAllOf nil**
  (PlayerController, ItemDatabaseSubsystem, Engine, Kismet*Library). Press →
  both callbacks returned within the SAME wallclock second: the spawner scan
  (2 FindFirstOf-family lookups incl. the UEHelpers path) + 4 full
  GUObjectArray walks + the 66-override carrier re-scan all completed in
  **< 1 s** (1 s log resolution).
- **Cycle 2, 21:31:42→:43**: crossed the 1 s boundary — total ∈ (0, 2 s],
  consistent with ~1 s. This is the user-perceptible hitch (v7 header's
  "walk runs, lag seen"): per-press ≈ 0.5–2 s of game-thread work, i.e.
  **per-full-array-walk ≈ 100–400 ms** at the observed live-object count
  (l26-v2 saw Index=71962 at 21:25:53).
- Zero [FFO] add-fallback lines, zero [FN], zero [UF] — with CP's Find-first
  fix in the binary, a miss would have logged; silence means every probed
  name RESOLVED via Find and the WALK returned nothing (or the class genuinely
  has no live non-CDO instance at menu state — see §6).
- 21:31:41 user initiated quit (monitor: eboot [387] gone at 21:31:41 poll)
  overlapping cycle 2; the last N-lines + "callback returned" printed at
  21:31:43 as the process tore down. Normal-path termination in klog.
- EIGT 5 queued / 5 drained / 5 returned.

## 6. Interpretation — what the probes actually measured (important caveat)

- All six l26-v2 probe cycles and both v6 cycles ran **~17–21 s after UE4SS
  start**, i.e. at the TITLE/MENU state (game boot → menu ≈ 100 s; UE4SS
  starts ~96–100 s after game EXEC; first press 18–21 s later). No save was
  loaded in either session (delete churn ≈ 6/s vs ~100/s during l26-v1's save
  load; no load-map-scale object churn).
- At menu state, db/ks/en/pc queries plausibly have only CDOs or nothing —
  and upstream IsValidObjectForFindXOf excludes CDOs (CA's amendment). So
  `db=invalid`, `ks=invalid`, and v6's all-nil are the PREDICTED menu-state
  outcomes even with a perfectly working FindXOf. They do NOT prove the walk
  is broken — and conversely the l25 "FindFirstOf WORKS" line was retracted by
  CA (null-wrapper false-positive), so **no session ever recorded an in-game
  FindXOf result in either direction**.
- The genuinely new hard facts tonight: (a) the CP Find-first fix produces
  zero [FFO] fallbacks (names resolve via Find), (b) the CN null-wrapper
  hardening produces the clean standard error text on null instances,
  (c) 4 full-array walks execute on the game thread in ≤1 s without harm.
- The decisive experiment remains unrun: **the same probes with a save
  loaded** (v7's U/F/S token set is built for it).

## 7. GC storm / delete-listener counts across the session (task 1)

| Session | Counter prints | Deltas | Reading |
|---|---|---|---|
| l26-v1 | 19:36:53 calls=2 → 19:37:54 calls=6,181 → 19:38:55 calls=13,970 | +6,179/61 s (≈101/s); +7,789/61 s (≈128/s) | coincides with the night's only save/level load (~19:36:5x onward); first time the repaired listeners observed a full level load — plausibly normal UE churn, not pathology; counts plateau per print, no runaway after load completes |
| l26-v2 | 21:24:59 calls=2 → 21:25:53 calls=349 | +347/54 s (≈6.4/s) | menu-screen churn only. **Storm did NOT survive the revert.** |
| v6 | 21:31:08 calls=2 | — | session too short for a second print |

Print cadence ≈ 54–61 s (throttled heartbeat; both listeners print together
with the same cumulative count).

## 8. Other anomalies (task 5)

1. **Ghost crash, session 2 (20:58:56–20:59:15)** — the only real crash: pid
   305, thread "RTHeartBeat 0", SIGSEGV, "page fault (user read instruction,
   page not present)", fault/rip 0x8000010ec (xotext:"libkernel.sprx"+0x10ec).
   Coredump prepared (epoch 1791511155 = 01:59:15Z),
   sceApplicationKickCoredump3 failed 0x80aa000c, deleted. Launched with
   ploader dead → no injection → an effectively stock boot crash ~20 s in,
   before UE4SS logging starts (~100 s). Fits the known HEN fragility family
   (AGENTS rule 5) rather than UE4SS. Monitor polls (15 s) missed the 20 s
   life. Flag: don't attribute this to any PRX.
2. **"CRASH KILL_END" klog events are the NORMAL quit path** here:
   APP_SUSPENDED → SUSPENDING → CRASH KILL_END event → STOP → BlockingKill
   forceKill=0 → "Terminating pid=…" → PlayGoCore GameSuspended → SaveData
   term. Verified for pids 110, 364, 387 (and 332). The event NAME is a trap —
   do not mine klogs for "CRASH" without the surrounding context.
3. **"SIGSEGV: normal" klog lines are a Mono/PSM banner** (TLS/SIGSEGV:
   normal/kqueue/amd64/GC: sgen) printed at common-dialog startup — not
   crashes. Appears after every game launch.
4. **Ploader lifecycle failure mode**: receipt 014749Z armed ploader 304
   (20:47); it exited 20:49:49 without a launch; the 20:58/20:59 launches ran
   unmodded; **PRX 23c8483d (the 20:46 build) never executed on console**.
   The 021939Z re-arm (fresh /loadpayload → pid 352, later 375) fixed it.
   Recommendation: the launch gate should assert a LIVE ploader immediately
   before each user launch.
5. ObjectSearcherPoolHook "Failed to add hook" + LoadMap.LuaModImpl ×2
   refusals: PRE-EXISTING (both present in l25-B-final), unchanged in l26.
6. "Failed to expose size of 'N2RC6Unreal18FOb', internal value invalid" —
   first readable sighting of a Lua size-exposure failure; benign (manual-set
   escape hatch exists).
7. Marshal carriers print raw FName ids ([n:F7BD]/[n:28D0]/[n:16AFB]) because
   fname_tostring is unbound by design — expected, but hurts readability of
   carrier identity in future sessions.
8. UClass::ClassCastFlags prints 0xD8 (staged profile) — CD's port-header 0xD0
   discrepancy never reaches runtime; staged value is live. No traversal
   misbehavior observed tonight.
9. GameEngine::Tick signature-scan failure → vtable fallback: every session,
   pre-existing.
10. v6's second probe overlapped the user's quit (monitor quit 21:31:41; last
    log lines 21:31:43) — results still printed; no teardown hazard.

## 9. Run matrix (l26 round)

| | l26-v1 (first build) | ghost A | ghost B | **l26-v2 (post-revert)** | **v6 (same build + zzdiag v6)** |
|---|---|---|---|---|---|
| Game window (CDT) | 19:34:38–19:39:09 | 20:58:56–20:59:15 | 20:59:36–21:01:51 | 21:23:10–21:26:10 | 21:29:26–21:31:41 |
| PRX / build | first l26 comp (old logwriter) | none (ploader dead) | none (ploader dead) | e13b1d0b, 21:18 build (CN+CL+CP + fix-2a revert) | e13b1d0b (identical) |
| UE4SS log | 19:36:02–19:38:57, interleaved/truncated | none | none | 21:24:50–21:26:08, **fully readable** | 21:31:02–21:31:43, fully readable |
| Admission | admitted (elapsed ≈2.6 s) | — | — | admitted (2646 ms) | admitted |
| DeleteListener Adds | 3 (Num→4, incl. markerless 3rd) | — | — | 3: 1→2 (11 ms), 2→3 (12 ms), 3→4 markerless | 3, same shape |
| Engine-called counts | 2 → 6,181 → 13,970 (save load) | — | — | 2 → **349** (no storm) | 2 only |
| Probes | 1 cycle (menu-timed): db/ks=invalid, null-instance errors | — | — | 6 cycles: db/ks=invalid, S2 conv=no, null-instance errors (CN text) | 2 cycles: N1pc/N2db/N3en/N4ks **all -1** |
| Scan fails (mod) | 1 | — | — | 6 (line-66 error, readable) | 2 |
| [FFO]/[FN]/[UF] markers | 0/0/0 | — | — | 0/0/0 | 0/0/0 |
| EIGT q/d/r | 3/3/3 | — | — | 13/13/13 | 5/5/5 |
| Pump warm-up | first invoke at init (warm) | — | — | 12 s cold; self-test drain 11600 ms | warm; self-test drain 7 ms; 74→138 carriers |
| Crash | none at kernel level (log tail lost; user quit) | **SIGSEGV RTHeartBeat libkernel+0x10ec; coredump failed** | none | none | none |
| End | user quit | crashed (~20 s, unmodded) | user quit | user quit (normal klog path) | user quit (normal klog path) |

## 10. Provenance

- Logs: /tmp/l26v2.log, /tmp/v6-run.log, /tmp/l26-crash.log (all raw, mixed
  encoding, untouched; normalized copies in /tmp/*.norm.log).
- Klogs: /tmp/l25-variant-A/klog-20261009-014748.log (covers sessions 1–3 via
  live+buffered replay; contains the session-2 crash block lines 1536–2966),
  klog-20261009-021938.log (sessions 4–5), klog-20261008-234131/-234839/
  003412 (session 1 era).
- Monitors: eboot-monitor-20261008T234131Z, -20261009T014749Z, -20261009T021938Z.
- Receipts: harvest/LAUNCH-READY-A-20261009T014749Z.md, -021939Z.md.
- Build identity: CHANGE-SELECTION diff (l26-A.crash1-210234 vs l26-A) shows
  LuaUObject.hpp as the only changed row; the revert text is at
  typescan-r4/candidate/include/LuaType/LuaUObject.hpp:821-829.
- Mod sources: TownfallItemSpawner main.lua (l16 provenance copy, v1.0),
  zzdiag-v3/v7 local copies (v4/v6 bodies inferred from their printed tokens;
  v4 = v3 chain + D/S tokens, v6 = N-count tokens).

— agent CZ (ZCode), 2026-10-08 ~21:50 CDT. Read-only over logs/trees.
