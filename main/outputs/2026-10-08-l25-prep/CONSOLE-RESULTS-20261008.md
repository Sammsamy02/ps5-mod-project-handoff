# CONSOLE RESULTS — l25 console round, 2026-10-08 evening (for Claude review)

Five live runs on PS5 ps5-host-not-configured.invalid (Townfall), 18:36–19:10 local (22:36–00:10 UTC).
All facts below are marker-cited from the harvested logs; nothing is inferred from
drafts. Raw provenance:

- B session (full): `harvest/harvest-l25-B-final-ue4ss.log` (+ `-supplier-diag.jsonl`,
  `-aligned-entry.log`). `harvest-l25-B-live1-*` is the earlier live-tail capture of
  the SAME session (identical first events; fewer keybinds) — not a separate run.
- A sessions: `/tmp/l25A-live2.log` (A, original settings), `/tmp/l25A-live3.log`
  (A, cache-on), `/tmp/zzdiag1.log` (A+cache+zzdiag-v1),
  `/tmp/zzdiag2.log` = `evidence/zzdiag-v2-session.log` (A+cache+zzdiag-v2, crashed).
- Findings docs: `evidence/UFUNCTION-NULLPTR-FINDING.md`, `evidence/SETTINGS-DEVIATION-NOTE.md`.
- Launch receipts: `harvest/LAUNCH-READY-B-20261008T233430Z.md` (B first, 31/31 PASS),
  `harvest/LAUNCH-READY-A-20261008T234132Z.md` (A second, 31/31 PASS; same daemon pids
  as B receipt → no reboot between B and A).
- Klog taps: `/tmp/l25-variant-B/klog-20261008-233429.log` (B),
  `/tmp/l25-variant-A/klog-20261008-234131.log` + `-234839.log` (A sessions through
  19:01). No tap was live for the 19:09 zzdiag-v2 crash session (post-reboot).

## 1. Run matrix

| | 1. B (orig settings) | 2. A (orig settings) | 3. A (cache-on) | 4. A+cache+zzdiag-v1 | 5. A+cache+zzdiag-v2 |
|---|---|---|---|---|---|
| Log | harvest-l25-B-final | /tmp/l25A-live2 | /tmp/l25A-live3 | /tmp/zzdiag1 | /tmp/zzdiag2 |
| Window (local) | 18:36:26–18:40:33 | 18:43:17–18:45:29 | 18:50:53–18:52:49 | 18:58:33–18:59:54 | 19:09:34–19:09:53 |
| Admission | admitted (L14 elapsed=2658ms/60s) | admitted (2686ms) | admitted (2689ms) | admitted (2637ms) | admitted (2652ms) |
| Init past the l24 crash point | yes — GMalloc line 18:36:29, registration SKIPPED by design (2 `[L25][DeleteListener] … SKIPPED` lines) | yes — GMalloc 18:43:21 | yes — GMalloc 18:50:56 | yes — GMalloc 18:58:36 | yes — GMalloc 19:09:37 |
| Listener registrations | 0 (skipped) | 2 Adds: Num 5→6 (shutdown, "returned in 11ms"), 6→7 | 3 Adds: Num 5→6 (class-delete, 11ms), 6→7 (shutdown, 12ms), 7→8 | 3 Adds, same shape (11/12ms) | 3 Adds, same shape (11/12ms) |
| Engine-called-listener lines | 0 | 1 (`FShutdownDeleteListener::NotifyUObjectDeleted Index=35550 calls=2`) | 4 (FClass + FShutdown, 2 objects, calls=12 and 2) | 2 (FClass + FShutdown, calls=2) | 2 (FClass + FShutdown, calls=2) |
| Mod start | TownfallItemSpawner 18:36:29 | TownfallItemSpawner 18:43:21 | TownfallItemSpawner 18:50:57 | spawner 18:58:37 + zzdiag (`[zz] Z0`) 18:58:37 | spawner + zzdiag 19:09:37 |
| Keybind fires | 25 entries (key 118/13/37/38/39/40) | 8 | 15 | 5 | 2 (key=118 ×2) |
| EIGT queued/drained | 16/16 | 6/6 | 11/11 | 5/5 | 3/3 |
| Pump (see §6) | armed 4 carriers 18:36:29; first invoke 18:36:41 (**12.2s warm-up**); steady-state same-second | armed 8 carriers; first drain same-second | armed 8; same-second | armed 8; same-second | armed 4; first invoke 9s; then same-second |
| Scan outcome | Scan failed ×16 (no menu) | Scan failed ×6 | Scan failed ×11 | Scan failed ×3 | Scan failed ×2 |
| Crash | none (klog: 0 fatal) | none (klog: 0 fatal) | none (klog: 0 fatal) | none | **CE-108255-1** at 19:09:53, log ends mid-chunk |
| End state | clean; no reboot before A (daemon pids identical) | clean quit 18:45:29 | clean quit 18:52:49 | clean quit 18:59:54 | console rebooted after crash (eboot monitor: daemons missing 00:15:31Z, payload reload 00:15:46Z) |

Totals: 46 EIGT closures queued, 46 drained, 0 stranded, 0 orphan drains across all
sessions. `[l22] keybind callback entry` fired in every run — the PS5Pad L3+R3→F7 →
keybind dispatch → EIGT → pump-drain chain is live end-to-end.

## 2. The three findings

### 2.1 Crash family SOLVED (l24 `AddUObjectDeleteListener` init crash)

l24 died with the log ending exactly at `Adding shutdown listener`
(`evidence/l24-ab2-ue4ss.log` last line, 14:07:39; Z11's symbolized stack:
`AddUObjectDeleteListener → TArray Append → FMemory::Realloc → FMalloc::Realloc →
call [rax+0x10] → abort`), root-caused to the profile member-offset error
(UObjectDeleteListeners 0x78→0x68, UObjectCreateListeners 0x68→0x58; AA/AB2/AB).

Tonight the same endpoint is passed and survived in four consecutive sessions:

- `[L25][DeleteListener] Add enter (GUObjectArray=0x97ba110 …)` →
  `header pre: Data=0x1280174d00 Num=5 Max=22 growth_required=0 …` →
  `header post: … Num=6 … (path=no-re)` → `registration returned in 11ms`
  (A sessions; second/third Add repeats Num 6→7→8) — the real array, not the
  crit fast-state l24 was reading at 0x78.
- The engine then dispatched deletions INTO our listeners during play —
  `engine called FShutdownDeleteListener::NotifyUObjectDeleted (… Index=35550
  calls=2/12)`, `engine called FClassDeleteListener::NotifyUObjectDeleted (…)`
  — the exact locked dispatch path that was suspect mechanism B. Zero crashes.
- Klog verdict: 0 `fatal signal`, 0 `copyin` hits in the B and A taps
  (`/tmp/l25-variant-B/klog-20261008-233429.log`,
  `/tmp/l25-variant-A/klog-20261008-234131.log`, `-234839.log`).
- Variant B (registration skipped) also completed — the skip arm behaved as
  predicted, but A's real registration makes B diagnostic-only surplus; never
  leave B resident (LAUNCH-PLAN 4.2).

### 2.2 UFunction-by-name resolution returns nullptr (THE remaining functional blocker)

From the zzdiag stepwise probe (30-char chunked lines because the wide-char Lua
log writer drops long tails; reassembled in `evidence/UFUNCTION-NULLPTR-FINDING.md`):

- `FindFirstOf("ItemDatabaseSubsystem")`: WORKS (object found; cache-on session).
- `db:GetItemIDFromDeprecatedEnum("Bandage")`: FAILS — Lua error
  `Tried calling <...> but the UFunction <is a> nullptr: in method
  'GetItemIDFromDeprecatedEnum'` — visible tonight as the interleaved
  `[l22] property: Tried calling a member function bu…` line plus chunks
  `E0 Tried callin / E1 n but the UO / E2 nullptr / E3 : in method /
  E4 ecatedEnum' … E7 /main.lua:14 …` in zzdiag2 (19:09:53).
- Consequence: the spawner's scan cannot build its item list → `Scan failed` in
  ALL five runs (16/6/11/3/2 occurrences) → menu never opens. This, not the
  pump, is what blocks unmodified PC Lua mods.
- Suspected cause (for l26): runtime-constructed FNames (from Lua strings) vs
  startup-pinned names — FName pool path / comparison-index mismatch
  (GATE-1 admission family).

### 2.3 zzdiag-v2 crash (FindFunction probe — removed)

zzdiag v2 added `db:GetClass():FindFunction(...)` — a raw class-children
traversal from Lua. The game crashed CE-108255-1 on repeated presses; the
preserved sequence in zzdiag2: `keybind callback entry key=118` → `[zz] K` →
`ExecuteInGameThread queued … ref=6` → `drained … ref=6 (callback entry)` →
`[l22] property: Tried calling…` → chunks E0–E11 → **log ends mid-chunk**
(19:09:53). No klog tap was live post-reboot, so no kernel backtrace for THIS
crash. v1 (without the probe) was crash-free across two probe cycles. The probe
is removed from any future diag. Console was rebooted (eboot monitor
transitions at 00:15:31–00:17:46Z).

## 3. Instrumentation dividends (what the new tags bought us)

- **Live GMalloc values, all five runs identical**, matching AB2's binary
  analysis exactly: `Post-initialization: GMalloc: 0x970add8 -> 0x97089b8` and
  `[L25] header pre: … GMallocVar=0x84dbef38 GMalloc=0x970add8
  GMallocInst=0x97089b8 vptr=0x82c26e0 slots +0x28=0x4000e0 +0x30=0x4b48e0
  +0x38=0x4b8a20 +0x40=0x4b49b0 +0xd0=0x4b2810` — Binned3 runtime instance,
  staged layout (true Realloc at +0x40) in use, not the compiled-in default.
- **Header dumps show the real array**: Data=0x1280174d00, Num 5→6→7→8, Max 22,
  growth never required (`growth_required=0` every time) — vs l24 reading the
  0x78 crit fast-state as "Data". No write-back, no refusal, no anomaly flag.
- **Engine-called-listener counts** (the dispatch path that crashed l24):
  0/1/4/2/2 lines across the five runs, up to `calls=12` on one object
  (Index=35550) in the cache-on A run — repeated engine dispatches into the
  repaired listeners, clean.
- **Runtime-phase tags**: `r3_marshal_pumped` heartbeat 280 events over the
  full 280s B session (never stopped); `eigt_queue_depth` 11 events;
  `[l25]`/`[R3]` arm/pump-publication lines in every session;
  `[Seeds][L25] copyin probe state after admission: kernel-write dispatch
  STOPPED (sink bank retired …; oracle-probes=0 used-vq=1 used-copyin=0
  elapsed≈2.7s)` in every session — the probe-retirement proof observed live.
- **Pad chain**: `pad_l3r3_f7_dispatched=15`, `pad_key_edge=25` in the B diag —
  the L3+R3→F7 rebind fires and reaches the Lua keybind every time.

## 4. Settings deviation note (documented, justified, reversible)

Between run 2 and run 3, `UE4SS-settings.ini` line 15
`bUseUObjectArrayCache=false → true`, size-exact 1947 B preserved (one `.` added
to comment line 60; admission-legal; `evidence/l25-settings-cacheon-1947.ini`).
Justification (full text: `evidence/SETTINGS-DEVIATION-NOTE.md`): the flag
predates the offset fix and was disabled when registration was unsafe; A's
registration is now proven safe on the real array and engine callbacks flow;
without the cache the FindFirstOf-based item scan cannot work at all. Rollback:
restore `/tmp/l24-settings-backup.ini` (sha b980cea7…).

## 5. Open items for l26 (fix list)

1. **UFunction-by-name nullptr resolution** (§2.2) — THE functional blocker:
   runtime FName construction/comparison-index vs stored FNames (GATE-1
   family). Fix or route resolution so `GetItemIDFromDeprecatedEnum`-style
   calls succeed → item scan → menu.
2. **Remove the zzdiag FindFunction probe permanently** (§2.3 crash source);
   any future diag must not traverse raw class children from Lua.
3. **Chunked-writer robustness**: the wide-char Lua log writer still collides
   with `[l22]` lines and drops long tails (most zzdiag v1 chunks lost). Keep
   diagnostic output in ≤30-char chunks or move it to the supplier-diag sink.
4. Carried from the earlier round (CLAUDE-REPORT §8l/§8m): correct the
   compiled-in default FMalloc vtable map (defense in depth — latent, staged
   layout saved us); full item-6 suite re-run; Z15 copyin-gating proposal
   decision; decide whether variant C (registration re-route post-init) is
   still wanted now that in-init registration is proven safe.
5. Optional evidence upgrade, not a tuning need: a per-drain
   `eigt_drained_dt` diag event (ms) — `r3_marshal_pumped` is 1s-throttled, so
   tonight's steady-state latencies are same-second bounds + heartbeat-derived
   upper bounds (§6); a direct per-drain measurement would close that gap.
6. Console state after tonight: rebooted post-crash; variant A resident,
   cache-on settings, loadlist 9 rows (zzdiag v2) — all verified post-reboot
   once (UFUNCTION-NULLPTR-FINDING.md); replace zzdiag v2's loadlist row
   before the next session.

## 6. Marshal pump timing (Task A detail)

Warm-up (boot enqueue → first drain; the pump publishes identity on the first
carrier invocation):

- B: queued t_ms=538964 → `r3_marshal_first_invoke` t_ms=551137 = **12,173ms**
  (log: 18:36:29→18:36:41). zzdiag-v2: 9s (19:09:37→19:09:46; post-reboot cold
  boot, 4 carriers). The three warm sessions (A-live2/live3/zzdiag1) armed and
  drained within the same wallclock second — the carrier was already being
  invoked when the boot EIGT closure queued.
- Warm-up cause is boot-phase, not pump: armed BP-override carriers
  (Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation /
  UserWidget:Tick override family; 4–8 instances armed per session) only fire
  once the engine reaches the phase that invokes them.

Steady state (after first invoke):

- All 43 post-warm-up queued→drained pairs landed in the SAME wallclock second
  (log resolution 1s) across all sessions; 46/46 total drained, none stranded.
- Millisecond upper bounds (B-final diag, enqueue → next `r3_marshal_pumped`;
  heartbeat is 1,001ms-throttled so these bound, not equal, the true drain):
  in-game window (18:39:55–18:40:33): 320 / 367 / 309 / 369 / 325 / 309 / 319 /
  **15ms**; title-screen window (18:36:44–47): 708 / 788ms.
- Carrier liveness: 280 consecutive heartbeats over 280s; worst inter-heartbeat
  gap 1,248ms (≈+3m38s, in-game) — i.e. the carrier never stalled more than
  ~250ms beyond the throttle floor.

**Verdict: adequate — not an l26 tuning item.** Mechanically the drain executes
synchronously inside the carrier's engine invocation, so in-game latency is one
carrier frame (~17ms @60fps / ~33ms @30fps); every in-game measured bound is
≤369ms and one direct bound is 15ms — comfortably inside a 500ms spawn-menu
budget. The 9–12s first-drain is one-time boot warm-up. The structural
dependency to remember: if ALL armed carrier instances stop being invoked
(full pause that halts their ticks, or a level transition tearing them down
before the ≤1-scan/s lazy re-arm), queued actions strand indefinitely — no
timeout exists by design. Tonight no such stall was observed (worst ~250ms).

— assembled 2026-10-08 by agent CH (ZCode), read-only over logs/trees.
