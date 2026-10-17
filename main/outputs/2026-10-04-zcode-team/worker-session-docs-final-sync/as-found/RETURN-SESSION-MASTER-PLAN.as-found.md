# RETURN-SESSION-MASTER-PLAN — one linear timeline, controller walks the user through this

Merged: USER-SESSION-GUIDE.md (batch runner, dll8 edition) · STAGE-PLAN-COMBINED.md (ridealongs)
· NEW-SETTINGS.md (settings v2) · MASTER-VERIFICATION-RUNBOOK.md (CP0–CP7) · CURRENT-STATE-DIGEST.md.
`HTTP=http://ps5-host-not-configured.invalid:8084` (manager; never 9223) · `FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous)
· `W=outputs/2026-10-04-zcode-team` (under workspace root `.codex/worktrees/translation-core/what`).
Law: dispatch EXACTLY ONCE; ≥30 s gaps; timeout=crash-signal; gate mismatch = STOP+preserve+reconcile,
never re-send, never relaunch an identical rejected layout. **[PENDING x] = artifact not yet on disk; see Prerequisites.**

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
- Run subcommands separately (observer + settings restage must land between staging and launch; `all` would reach the
  launch gate too early):
1. TOOL `python3 console_batch.py dll8` — ALLFAM8_DLL_TEST.elf sha `79281191…` 336552 B; upload, readback EXACT,
   dispatch ONCE, wait 15 s, verdict vs EXPECTED-DELTA-8FAM. Records 41–44 + static_tls + imports oracle.
2. TOOL `python3 console_batch.py e2e` — 13/13 cmd SHASUMS + payload `a21771f8…` 681888 B; MODE=B gate; 15-dispatch
   stage 0..6 cycle, verbatim ledger lines, consumed-twin proof per dispatch; §7 full-tree reset (only `/data/modbridge/**`).
   Interrupted stage-3 commit → ONE permitted `recover.json` dispatch then HALT.
3. TOOL `python3 console_batch.py stage-qdrv1` — backs up ploader.ini (`.bak` + local pre-image), uploads
   `modbridge_qh_driver.prx` sha `52c32216…` 26157 B, appends `2=/data/plugins/modbridge_qh_driver.prx`, re-reads ini.
4. TOOL — observer ridealong, per STAGE-PLAN-COMBINED §3–4 (NOT yet in the tool) [PENDING stage-qdrv1 extension]:
   `curl -sS -T W/worker-observer-packaging/modbridge_runtime_aligned_observer.prx "$FTP/data/plugins/modbridge_runtime_aligned_observer.prx"`
   → RETR readback → SHT-strip snippet (§3) vs `build-1/bare-sdk/…`: expect size 30143976, body sha `3f89a5b6…`;
   local sha `1afa0997…` 30152151 B. Then SC-5 append-only ini edit from the pre-image (2 lines, `= 2`, after composition);
   `python3 W/worker-game-session-ridealongs/simulate_ploader_ini.py <new.ini>` → registered count exactly 7; STOR + RETR diff byte-exact.
5. TOOL — settings+loadlist restage, NEW-SETTINGS §3 (manual; game CLOSED): freeze-check live pair = 779 B `9f3fa127…` /
   446 B `49ab7a3a…` (abort on divergence) → `curl -sS -T W/worker-engineversion-ini/package-v3/ue4ss/UE4SS-settings.ini "$FTP/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"`
   and `-T package-v3/loadlist.txt "$FTP/data/modbridge/roots/PPSA33286/loadlist.txt"` → readback BOTH: 1448 B `d8faa4ec…`,
   447 B `4d98ecd7…` (verified on disk 2026-10-04). Lockstep law: mismatch = re-transfer, never launch on partial upload.
- GATE→Phase 3: all flows' verdicts green; ini registers exactly 7 plugins; readbacks byte-exact.
- Any gate mismatch anywhere: tool halts, preserves every byte, prints the divergence. USER: answer `abort` at any gate;
  do not re-run a dispatch by hand — reconcile first (USER-SESSION-GUIDE abort semantics).

## Phase 3 — the game session (only the user can do this; ~15–20 min)
6. TOOL `python3 console_batch.py game-evidence` prints the gate and waits:
   - USER: **"arm ploader"** (Payload Manager UI / loadpayload) with game CLOSED. WAIT for the OK popup — 1–60 s; HTTP OK ≠ armed.
   - USER: **"launch Townfall"**. Records appear ~1 min after launch.
7. TOOL fetches sinks (S, L, D1, C2) and runs CP0–CP4: module ran (`module_start_entered`), activation consumed, engine-ready
   + Tick `0x33ac8c0`, mods-started, Item Spawner load banner; plus `post_launch_check.py` + `rung_check.py`.
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
- ploader.ini: restore `/data/plugins/ploader.ini.<TAG>.bak` or flip the two ridealong lines to `= 0` (mode 0 = skipped at parse) — STAGE-PLAN-COMBINED §7.
- Settings: re-upload old pair 779 B `9f3fa127…` + 446 B `49ab7a3a…` (NEW-SETTINGS rollback).
- Launch anomalies: runbook §6 triage index (F0–F10, outcomes #1–9); ploader quirk: any FAIL mid-list retries the whole list — read every notification line, STOP on FAIL.

## Time and effort
| Phase | Wall clock | User actions |
|---|---|---|
| 0 sanity | ~5 min | keep game closed (check) |
| 1 comp6 build+stage | ~15–30 min (dry build+gate ≈ 7–8 min; ×2 builds + stage/readback) | 0 |
| 2 batch: dll8 ~5–10 · e2e ~15–25 (15 dispatches, ≥30 s gaps) · stage-qdrv1+observer ~10–15 (30 MB upload) · settings restage ~5 | ~40–70 min | keep game closed |
| 3 game session | ~15–20 min (boot ~5 min; records ~1 min in; ridealong windows 60 s / ~2 min) | see below |
| TOTAL | ~75–125 min | **7 required** (close-check, arm ploader, launch Townfall, load save, press OPTIONS once, navigate, press CROSS) + 2 eye-confirmations + 1 optional re-press |

## Honest prerequisites (must have landed before the session; in-flight owners)
1. **comp6 PRX staged** [PENDING controller one-pass run]. On disk: scaffold (8 files), `build_runtime_composition6.py`, gate v2. In flight: required-slots diff set — required-slots-feasibility REPORT landed ("ps_scan structurally dead", slots 2/4 need bypass; SLOTS-FEASIBILITY §4b/§5 proposals not yet built into comp6).
2. **Freshness-fixed tools**: rung_check.py O2 gate LANDED (exit 5 INCONCLUSIVE-STALE); post_launch_check.py GAP2/3 applied in place; console_batch.py GAP 1a/1b freshness gate NOT applied [PENDING worker-evidence-freshness-fixes patch+brief; worker in_progress, receipts empty].
3. **8fam patch**: dll8 code LANDED in console_batch.py (verified on disk); its verification receipts EMPTY [PENDING worker-batch-runner-8fam-patch].
4. **Observer staging in stage-qdrv1** [PENDING console_batch.py extension per STAGE-PLAN-COMBINED §8]; manual §3–4 commands verified against the plan and artifacts.
5. **Settings package-v3**: READY — 1448 B `d8faa4ec…` + 447 B `4d98ecd7…` re-hashed on disk 2026-10-04.
6. **patches.json 51→54 promotion** PREPARED, NOT APPLIED [PENDING worker-e2e-suite-completion apply]; digest lists apply + ACK-exit-6 amendment ahead of the E2E run — operative gate is the tool's own 13/13+payload preflight.

## Command cross-check (against disk, 2026-10-04)
comp6_one_pass.py ✓ exists (controller dir) · console_batch.py plan/dll8/dll7/e2e/stage-qdrv1/game-evidence/all/selftest ✓ in CLI · post_launch_check.py ✓ · rung_check.py ✓ (freshness gate in file) · simulate_ploader_ini.py ✓ · package-v3 bytes ✓ re-hashed · observer curl commands = STAGE-PLAN-COMBINED §3 verbatim (manual) · settings curl = NEW-SETTINGS §3 verbatim (manual). Every [PENDING] above names its owner. Nothing in this plan has been executed on the console; timings are host-side estimates, console behavior stays [TARGET] until the session runs.

> **SESSION-EVE UPDATE (controller):** composition-6 is BUILT and GATE-APPROVED 13/13 (sha 76ee6af3..., preserved at outputs/2026-10-04-zcode-controller/console/comp6-staged-fself.prx). The console homebrew servers went DOWN before staging (FTP refused / HTTP empty while user away) — the slot still holds r11. PHASE 0 ADDS: retry the comp6 upload + readback FIRST (guarded law; console/comp6-staged-fself.prx); confirm the pldmgr/FTP are back before any batch step.
