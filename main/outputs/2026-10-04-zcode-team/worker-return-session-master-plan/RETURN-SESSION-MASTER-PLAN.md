# RETURN-SESSION-MASTER-PLAN — one linear timeline, controller walks the user through this

Merged: USER-SESSION-GUIDE.md (batch runner, dll8 edition) · STAGE-PLAN-COMBINED.md (ridealongs)
· NEW-SETTINGS.md (settings v3 restage; v2 = recorded freeze/rollback inventory) · MASTER-VERIFICATION-RUNBOOK.md (CP0–CP7) · CURRENT-STATE-DIGEST.md.
`HTTP=http://ps5-host-not-configured.invalid:8084` (manager; never 9223) · `FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous)
· `W=outputs/2026-10-04-zcode-team` (under workspace root `.codex/worktrees/translation-core/what`).
Law: dispatch EXACTLY ONCE; ≥30 s gaps; timeout=crash-signal; gate mismatch = STOP+preserve+reconcile,
never re-send, never relaunch an identical rejected layout. **[PENDING x] = artifact not yet on disk; see Prerequisites.**

> **DOC-SYNC 2026-10-04 (worker-session-docs-final-sync):** Phase-2 steps + prerequisites updated to the FINAL batch
> tool (worker-batch-final-ini-adoption, `console_batch.py` sha `b00b3a69…`, selftest 372 ok / 0 failed): steps 3–4 are
> now `stage-ridealongs` (both launch probes + qdrv1 + observer + the FINAL 8-entry ini with its INI-BOOTSTRAPPED
> branch) and `stage-settings` (v3 lockstep restage); the former stage-qdrv1 + manual observer/settings steps are
> superseded. Edit log: `outputs/2026-10-04-zcode-team/worker-session-docs-final-sync/EDIT-LOG.md`.

> **DOC-SYNC NINTH SLOT (worker-ninth-slot-ini-adoption, 2026-10-04):** the cc1 carrier probe
> (worker-carrier-probe-build/STAGE-PLAN.md) is ADOPTED as the 9th ini entry — the FINAL ini law is now the
> **9-entry / 8-registered** layout (canonical `ploader.ini.canonical` 440 B `fd9c7393…`; supersedes the 8-entry
> 400 B `bbc34c16…` law everywhere below). Batch tool revision carries `stage-ridealongs` = FIVE PRX uploads
> (both launch probes + qdrv1 + observer + `modbridge_carrier.prx` `d83410aa…` 26845 B, readback 71528 B body
> `131ff1c2…` — host-recomputed under the §3 prefix law; the STAGE-PLAN's `1d17a27d…` hashed bare[64:EOF], see the
> adoption receipts), post-launch gate EXACTLY **8** OK lines / ZERO FAIL, and the `ge.3c` cc1 census evidence gate
> (`/data/modbridge-carrier-facts.jsonl`, build `cc1-0864d65c1baf`). Receipts:
> `outputs/2026-10-04-zcode-team/worker-ninth-slot-ini-adoption/`.

## Phase 0 — sanity, game CLOSED (controller, ~5 min)
- USER: keep Townfall fully CLOSED; no ploader arm pending. Nothing below launches anything until Phase 4.
- TOOL `curl -sS --connect-timeout 10 --max-time 60 "$FTP/data/plugins/ploader.ini" -o /tmp/pl.now && shasum -a 256 /tmp/pl.now`
  → expect pre-image sha `8d331b3f…` (INDEX Track 8). Divergent = STOP, reconcile before anything.
- GATE→Phase 1: manager answers on 8084; staged inventory matches Track 8.

## Phase 1 — comp6 one-pass build result staging (controller tool; game stays CLOSED; ~15–30 min)
- TOOL `python3 outputs/2026-10-04-zcode-controller/comp6_one_pass.py`
  → ODR pre-flight (scaffold shas, port-vs-scaffold identity) → REAL composition-6 build → gate v2 GO → stage via
  `stage_composition5_cycle.py` (upload + readback band check). Never launches.
- Waits on: in-flight comp6 pieces (see Prereq 1). `JOB FAILED` at any step = STOP for the whole session's launch path.
- GATE→Phase 2: gate v2 verdict GO + staged readback verified. Rollback pointer: worker-rollback-runbook (never replay).

## Phase 2 — game-closed console batch (one tool window; keep game CLOSED throughout; ~40–70 min)
- TOOL `cd W/worker-console-batch-runner && python3 console_batch.py plan` (optional preview; touches nothing)
- Run subcommands separately, in the tool's own session order (`all` = dll8 → e2e → stage-ridealongs → stage-settings →
  game-evidence and blocks at the arm-ploader gate, so one-session `all` is lawful in the final tool; separate runs keep
  a verdict check between stages — never run game-evidence before stage-ridealongs + stage-settings are green):
1. TOOL `python3 console_batch.py dll8` — ALLFAM8_DLL_TEST.elf sha `79281191…` 336552 B; upload, readback EXACT,
   dispatch ONCE, wait 15 s, verdict vs EXPECTED-DELTA-8FAM. Records 41–44 + static_tls + imports oracle.
2. TOOL `python3 console_batch.py e2e` — 13/13 cmd SHASUMS + payload `a21771f8…` 681888 B; MODE=B gate; 15-dispatch
   stage 0..6 cycle, verbatim ledger lines, consumed-twin proof per dispatch; §7 full-tree reset (only `/data/modbridge/**`).
   Interrupted stage-3 commit → ONE permitted `recover.json` dispatch then HALT.
3. TOOL `python3 console_batch.py stage-ridealongs` — DEFINITIVE ridealong+probe staging (FINAL-INI-LAYOUT §3, NINTH-SLOT
   revision; supersedes the legacy stage-qdrv1/stage-observer partial flows): backs up ploader.ini (console `.bak` + local
   pre-image), then applies the FINAL 9-entry ini law from the Step-0 pre-image — content-anchored ensure ops (each line
   inserted after its canonical predecessor; the cc1 carrier line inserted immediately AFTER the igf `= 0` line) when the
   present lines already sit in canonical order; when the historical
   aligned_entry/path_test-before-composition shape is found, the ONE sanctioned whole-file STOR of the 440 B canonical
   template (`fd9c7393…`, `.bak` + byte-exact RETR diff) runs as its own **INI-BOOTSTRAPPED** verdict — never a silent
   rewrite, never a second attempt. End state gated BYTE-EXACT vs the canonical template: exactly **8 registered**
   (mode-0 igf line consumes no slot; carrier is the 9th entry / 8th registration); anomalous pre-image (duplicate /
   missing pre-existing / wrong mode / unknown line) = STOP + preserve + reconcile. The flow then records fresh
   pre-launch baselines (UE4SS.log, observer log
   absent, the three probe sinks if present).
4. TOOL — same flow, the FIVE PRX uploads under the §3 SHT-strip readback law, each under its OWN name: launch probes
   `modbridge_exec_write_probe_armed.prx` sha `48949316…` 20013 B (readback 67832 B `daea2ff4…`),
   `modbridge_fc1pool.prx` `d2431bf8…` 22733 B (67848 B `7e2dce27…`), `modbridge_qh_driver.prx` `52c32216…` 26157 B
   (87112 B `71406d3c…`), `modbridge_runtime_aligned_observer.prx` `1afa0997…` 30152151 B (30143976 B `3f89a5b6…`),
   and the cc1 carrier probe `modbridge_carrier.prx` `d83410aa…` 26845 B (readback 71528 B body `131ff1c2…` —
   host-recomputed under the §3 prefix law; the STAGE-PLAN's `1d17a27d…` hashed bare[64:EOF], adoption receipts
   record the divergence);
   a failed readback re-STORs from the verified local file EXACTLY ONCE, then aborts.
5. TOOL `python3 console_batch.py stage-settings` — settings+loadlist LOCKSTEP restage, SETTINGS-V3-DECISION.md (v3 identities) (game CLOSED):
   freeze-check the staged pair against the recorded v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…` (divergent/unowned
   = FAIL, never overwritten; an already-v3 console is an honest no-op) → upload the v3 pair as ONE lockstep restage
   (`W/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini` → `$FTP/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`
   and `package-v3/loadlist.txt` → `$FTP/data/modbridge/roots/PPSA33286/loadlist.txt`) → readback BOTH: 1448 B `d8faa4ec…`
   (`[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0`), 447 B `4d98ecd7…` (verified on disk 2026-10-04).
   Lockstep law: mismatch = re-transfer, never launch on partial upload.
- GATE→Phase 3: all flows' verdicts green; ini registers exactly 8 plugins; readbacks byte-exact.
- Any gate mismatch anywhere: tool halts, preserves every byte, prints the divergence. USER: answer `abort` at any gate;
  do not re-run a dispatch by hand — reconcile first (USER-SESSION-GUIDE abort semantics).

## Phase 3 — the game session (only the user can do this; ~15–20 min)
6. TOOL `python3 console_batch.py game-evidence` prints the gate and waits:
   - USER: **"arm ploader"** (Payload Manager UI / loadpayload) with game CLOSED. WAIT for the OK popup — 1–60 s; HTTP OK ≠ armed.
   - USER: **"launch Townfall"**. Records appear ~1 min after launch.
7. TOOL fetches the eight evidence sinks (S, L, D1, C2 + observer log + the three probe verdict sinks) and runs CP0–CP4:
   module ran (`module_start_entered`), activation consumed, engine-ready + Tick `0x33ac8c0`, mods-started, Item Spawner
   load banner; plus the ge.3b probe-verdict freshness gate (`/data/modbridge-exec-write-probe.jsonl` build
   `igd1-6ebab07f2111`; `/data/modbridge-fc1pool-facts.jsonl` build `fc1pool1-78508a2e2940`, `fc1pool_result`
   `validated` 11/11; honestly recorded fc1 refusals = WARN with reason codes, never auto-retried); plus the ge.3c cc1
   census evidence gate (`/data/modbridge-carrier-facts.jsonl` build `cc1-0864d65c1baf`: entry/startup 30x4s/mapscan,
   30 `qh_counter` lines, `census_result` `CENSUS:VALIDATED` with both controls + `PostRender` negative 0, `summary`
   `write_failures` 0, exactly ONE `CC1:` UE4SS.log line; `CENSUS:REFUSED` = honest terminal WARN — a NEW staged
   window, never a re-rating); plus
   `post_launch_check.py` + `rung_check.py`.
   Standalone equivalents (runbook §2): `python3 W/worker-post-launch-tools/post_launch_check.py` then
   `python3 W/worker-rung-evidence-reader/rung_check.py` (and `--rung RUNG1`, `--rung RUNG2` re-run every ~20–30 s to
   straddle the 8 s 0→1→2 ramp; qdrv1 now supplies the driver). FTP reads: wait 15–30 s, fresh connection.
   GATE: CP0–CP4 green. Any CP fails → tool stops with the runbook triage pointer (§6: read S → D1 → L); that is the honest result — no relaunch on top.
8. USER: **"load save"** — get in-game (P3: without it the OPTIONS test takes the expected "Load a save first" branch, runbook §4.3c).
9. USER: **"press OPTIONS once"** — brief press-release, ONE press (edge-triggered = exactly one F7; nav buttons silent while menu closed — by design).
   TOOL re-reads UE4SS.log → `spawnable pickups found` + `Menu created.`, or classifies one of four failure shapes (§4.3 a/b/c/d).
   USER confirms: **menu overlay VISIBLE** (title, 12 rows, controls line; game input locked). Optional: OPTIONS again closes silently (T4).
10. USER: UP/DOWN to highlight, LEFT/RIGHT to page (visual only — no log lines exist for navigation).
11. USER: **"CROSS to confirm spawn"**. TOOL checks `Spawned <Item> x<N> ID=<id>`; USER confirms a visible new pickup at the aim point.
    Runbook G4/G5: visible menu + visible pickup are decided by YOUR eyes — log lines never substitute. Failure names its stage (§5.4); retry-safe, but each observed failure gets a distinct revision.

## Abort / rollback branch pointers
- Tool level: Ctrl-C or `abort` at a gate stops; completed steps stay in `runs/<ts>-<flow>/receipt.json`; nothing re-dispatched.
- ploader.ini: restore `/data/plugins/ploader.ini.<TAG>.bak` or run `python3 console_batch.py rollback-ridealongs` — flips the two ridealong lines to `= 0` (mode 0 = skipped at parse; probes/igf untouched) with a fresh `.bak` + byte-verified readback — STAGE-PLAN-COMBINED §7.
- Settings: re-upload the recorded v2 rollback pair 834 B `9570ead3…` + 446 B `3afd10ce…` (`stage-settings` freezes against exactly this inventory before restaging; NEW-SETTINGS rollback).
- Launch anomalies: runbook §6 triage index (F0–F10, outcomes #1–9); ploader quirk: any FAIL mid-list retries the whole list — read every notification line, STOP on FAIL.

## Time and effort
| Phase | Wall clock | User actions |
|---|---|---|
| 0 sanity | ~5 min | keep game closed (check) |
| 1 comp6 build+stage | ~15–30 min (dry build+gate ≈ 7–8 min; ×2 builds + stage/readback) | 0 |
| 2 batch: dll8 ~5–10 · e2e ~15–25 (15 dispatches, ≥30 s gaps) · stage-ridealongs (4 PRXs + probes) ~10–15 (~30 MB upload) · stage-settings ~5 | ~40–70 min | keep game closed |
| 3 game session | ~15–20 min (boot ~5 min; records ~1 min in; ridealong windows 60 s / ~2 min) | see below |
| TOTAL | ~75–125 min | **7 required** (close-check, arm ploader, launch Townfall, load save, press OPTIONS once, navigate, press CROSS) + 2 eye-confirmations + 1 optional re-press |

## Honest prerequisites (must have landed before the session; in-flight owners)
1. **comp6 PRX staged** [PENDING controller one-pass run]. On disk: scaffold (8 files), `build_runtime_composition6.py`, gate v2. In flight: required-slots diff set — required-slots-feasibility REPORT landed ("ps_scan structurally dead", slots 2/4 need bypass; SLOTS-FEASIBILITY §4b/§5 proposals not yet built into comp6).
2. **Freshness-fixed tools**: rung_check.py O2 gate LANDED (exit 5 INCONCLUSIVE-STALE); post_launch_check.py GAP2/3 applied in place; console_batch.py GAP 1a/1b freshness gate APPLIED (worker-evidence-freshness-fixes `92b56889…`) and superseded in place by the final adoption (`b00b3a69…`, selftest 372 ok / 0 failed).
3. **8fam patch**: dll8 code LANDED and verified (worker-batch-runner-8fam-patch receipts: 162 selftest + 31 fake-drive checks); carried through the final adoption (`b00b3a69…`).
4. **Observer+probe staging in the tool**: LANDED — `stage-ridealongs` (worker-observer-staging-extension, then the DEFINITIVE FINAL-ini edition by worker-batch-final-ini-adoption, then the NINTH-SLOT edition by worker-ninth-slot-ini-adoption) stages both launch probes + qdrv1 + observer + the cc1 carrier probe and applies the FINAL 9-entry ini with its INI-BOOTSTRAPPED branch; the manual STAGE-PLAN-COMBINED §3–4 curl path is superseded (the legacy stage-qdrv1 / stage-observer partial flows remain in the tool only as documented fallbacks).
5. **Settings package-v3**: READY — 1448 B `d8faa4ec…` + 447 B `4d98ecd7…` re-hashed on disk 2026-10-04.
6. **patches.json 51→54 promotion** PREPARED, NOT APPLIED [PENDING worker-e2e-suite-completion apply]; digest lists apply + ACK-exit-6 amendment ahead of the E2E run — operative gate is the tool's own 13/13+payload preflight.

## Command cross-check (against disk, 2026-10-04)
comp6_one_pass.py ✓ exists (controller dir) · console_batch.py plan/dll8/dll7/e2e/stage-ridealongs/stage-observer/stage-qdrv1/stage-settings/rollback-ridealongs/game-evidence/all/selftest ✓ in CLI (final revision `b00b3a69…`, selftest 372 ok / 0 failed; flags --dll 7|8, --dry-run, --assume-yes, --run-root) · post_launch_check.py ✓ · rung_check.py ✓ (freshness gate in file) · package-v3 bytes ✓ re-hashed · observer+probe staging = `stage-ridealongs` (in-tool; legacy stage-qdrv1/stage-observer remain as fallbacks) · settings restage = `stage-settings` (in-tool). Every [PENDING] above names its owner. Nothing in this plan has been executed on the console; timings are host-side estimates, console behavior stays [TARGET] until the session runs.

> **SESSION-EVE UPDATE (controller):** composition-6 is BUILT and GATE-APPROVED 13/13 (sha 76ee6af3..., preserved at outputs/2026-10-04-zcode-controller/console/comp6-staged-fself.prx). The console homebrew servers went DOWN before staging (FTP refused / HTTP empty while user away) — the slot still holds r11. PHASE 0 ADDS: retry the comp6 upload + readback FIRST (guarded law; console/comp6-staged-fself.prx); confirm the pldmgr/FTP are back before any batch step.
