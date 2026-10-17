# CONTROLLER-DECISION-BRIEF — every decision point, gate, and branch I execute at the return session

Worker: worker-controller-decision-brief, 2026-10-04. Read-only synthesis.
Companion to `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` (that document = the USER's actions; this one = MINE).
Root `R = /WORKSPACE/.codex/worktrees/translation-core/what`; `W = outputs/2026-10-04-zcode-team`;
`HTTP = http://ps5-host-not-configured.invalid:8084` (manager; NEVER 9223, no alternate ports); `FTP = ftp://ps5-host-not-configured.invalid:2121` (anonymous).

> **DOC-SYNC 2026-10-04 (worker-session-docs-final-sync):** §0.7, §0b, §3.3–§3.5, §4.1/§4.2, §5 and §7 updated to the
> FINAL batch tool (worker-batch-final-ini-adoption, `console_batch.py` sha `5ca8d1c7…`, selftest **372 ok / 0 failed**):
> `stage-ridealongs` (both launch probes + qdrv1 + observer + the FINAL 8-entry ini with its INI-BOOTSTRAPPED branch)
> and `stage-settings` (v3 lockstep restage) replace the former stage-qdrv1 + manual observer/settings steps. Edit log:
> `outputs/2026-10-04-zcode-team/worker-session-docs-final-sync/EDIT-LOG.md`.

> **DOC-SYNC NINTH SLOT (worker-ninth-slot-ini-adoption, 2026-10-04):** the cc1 carrier probe is ADOPTED as the 9th
> ini entry (worker-carrier-probe-build/STAGE-PLAN.md): the FINAL ini law is now **9 entries / 8 registered**
> (canonical `ploader.ini.canonical` 440 B `fd9c7393…`; the 8-entry 400 B `bbc34c16…` law below is superseded), the
> post-launch gate is EXACTLY **8** `OK (<ms>): <file>` lines / ZERO FAIL, `stage-ridealongs` uploads FIVE PRXs
> (… + `modbridge_carrier.prx` `d83410aa…` 26845 B, readback 71528 B body `131ff1c2…` under the §3 prefix law — the
> STAGE-PLAN's `1d17a27d…` hashed bare[64:EOF]; see the adoption receipts), and `game-evidence` adds the `ge.3c` cc1
> census evidence gate (`/data/modbridge-carrier-facts.jsonl`, build `cc1-0864d65c1baf`). Receipts:
> `outputs/2026-10-04-zcode-team/worker-ninth-slot-ini-adoption/`.

## 0. Standing law (applies at every decision point)

1. **Dispatch EXACTLY ONCE.** ≥30 s between any two dispatches (crash 1: two dispatches 5 s apart killed the console).
2. **Timeout = crash signal, never a retry trigger.** A dispatch timeout means STOP + preserve + reconcile.
3. **Gate mismatch = STOP + preserve + reconcile.** Never re-send, never relaunch an identical rejected layout.
4. **No upload before gates.** Build/gate GO first, upload second, readback-EXACT third, dispatch last.
5. **Never re-rate unchanged evidence.** A new evidence window needs a NEW launch; each observed failure gets a distinct revision ID.
6. **Ctrl-C / `abort` at any gate** stops the flow; completed steps stay in `runs/<ts>-<flow>/receipt.json`; nothing is re-dispatched.
7. Run subcommands in the tool's session order (dll8 → e2e → stage-ridealongs → stage-settings → game-evidence). In the final tool `all` runs exactly that order and blocks at the arm-ploader USER ACTION gate, so one-session `all` is lawful; separate runs keep a verdict check between stages. Never run game-evidence before stage-ridealongs + stage-settings are green; never interleave a game session with the payload window.
8. Everything is host-tested, not console-proven. Console behavior stays [TARGET] until it happens.

## 0b. Tool-state cross-check (verified on disk at brief time — trust THIS table over the master plan's Prerequisites)

| Item | Master plan said | On disk now (I verified) | Consequence for me |
|---|---|---|---|
| console_batch.py freshness gate (GAP 1a/1b) | [PENDING worker-evidence-freshness-fixes] | **APPLIED.** `FRESH_SESSION_MS_THRESHOLD` + `cp_session_fresh`/`INCONCLUSIVE-STALE` labels present (applied at sha `92b56889…`, carried into the final revision `5ca8d1c7…`) | Stale-evidence protection is live inside `game-evidence`; no patch step needed |
| 8fam patch verification receipts | [PENDING worker-batch-runner-8fam-patch] | **LANDED.** worker-batch-runner-8fam-patch REPORT: complete, 162 selftest ok, fake-drive 31 ok; DLL8 identity re-verified | dll8 flow is verified; prereq cleared |
| comp6 one-pass | "`comp6_one_pass.py ✓` exists" | Exists **but is a pre-flight skeleton**: it hashes the scaffold, checks driver/gate v2 exist, writes `comp6-one-pass-state.json`, prints "awaiting driver/gate argv finalization" — it does NOT build/gate/stage | Phase 1 for me = the ONE-PASS-CHECKLIST commands (§2 below), not the single `comp6_one_pass.py` invocation |
| comp6 preconditions P1/P6/P8/P9 | — | **ALL GREEN:** port header edited (sha `0a068bb9…` ≠ pre-landing `66499615…`); bridge r12 sha `feed5254…` matches pin; `staging/lineage-overlay-2` already moved to `…-rehearsal`; no `run-1` / `prx-candidate-composition6-1` dirs exist | One-pass may run as soon as the session opens, if I choose to include it |
| Observer+probe staging in the tool | [PENDING console_batch extension] | **LANDED (DEFINITIVE).** `stage-ridealongs` stages all FIVE PRXs (both launch probes xwp-armed + fc1pool, plus qdrv1 + observer + the cc1 carrier probe) under the §3 SHT-strip readback law and applies the FINAL 9-entry ini law (worker-observer-staging-extension → worker-batch-final-ini-adoption `5ca8d1c7…` → worker-ninth-slot-ini-adoption, selftest 405/0); legacy `stage-qdrv1`/`stage-observer` remain only as fallback paths | Phase 2 staging = §3.3/§3.4 tool flows; the manual §3–4 curl path is superseded |
| patches.json 51→54 promotion | PREPARED, NOT APPLIED | Still NOT APPLIED (PROMOTION-PREP.md) | Operative gate stays the tool's own 13/13 + payload preflight; no apply attempt mid-session |
| worker-exec-writability (writability decision tree) | in flight | **EMPTY directory** → branch **[PENDING]** | Default rule stands: verify hashes before replacing owned files; never touch divergent/unowned content; write only owned proposal paths |
| Settings package-v3 (supersedes v2) | READY (v2 era staged) | v3 on disk: settings 1448 B `d8faa4ec…` + loadlist 447 B `4d98ecd7…`; the tool's `stage-settings` FREEZEs against the recorded v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…` (already-v3 = honest no-op; divergent/unowned = FAIL) | `stage-settings` restages v3 as ONE lockstep pair |
| qdrv1 / observer PRX | pinned | On disk: 26157 B (`52c32216…`) / 30152151 B (`1afa0997…`) | Ready to stage |
| console_batch.py FINAL-ini adoption | — | **SUPERSEDED-IN-PLACE by the NINTH SLOT.** worker-batch-final-ini-adoption `5ca8d1c7…` delivered the 8-entry law (exactly 7 registered; EXACTLY 7 OK lines; selftest 372/0); worker-ninth-slot-ini-adoption adopts the cc1 carrier probe: 10 flows + `plan`/`selftest`; FINAL **9-entry** ini = exactly **8 registered**; INI-BOOTSTRAPPED branch; post-launch gate = EXACTLY **8** `OK (<ms>): <file>` lines, ZERO FAIL; canonical 440 B `fd9c7393…`; selftest 405 ok / 0 failed, fake drive 92/0 | This brief's §3.3–§3.5, §4.2, §5 and §7 reflect the ninth-slot revision (DOC-SYNC NINTH SLOT 2026-10-04) |

Lesson (stale-receipt rule): the master plan's own [PENDING] marks were stale within hours. **Before every phase, re-verify the artifact bytes on disk; never act on a status string alone.**

---

## 1. Phase 0 — sanity (game CLOSED; ~5 min; controller-run)

Actions, in order:
1. ASK USER: "keep Townfall fully CLOSED; no ploader arm pending." (Only check; nothing launches until Phase 3.)
2. `curl -sS --connect-timeout 10 --max-time 60 "$FTP/data/plugins/ploader.ini" -o /tmp/pl.now && shasum -a 256 /tmp/pl.now`
   → expect pre-image sha `8d331b3f…` (INDEX Track 8).
3. Manager reachability is implicitly re-proven by each flow's G0 step; do NOT probe other ports.

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| sha `8d331b3f…` + Track-8 inventory matches | **ADVANCE** to Phase 1 | ONE-PASS-CHECKLIST §2 pre-flight | — |
| Divergent pre-image sha | **STOP.** Preserve `/tmp/pl.now`; reconcile against worker-rollback-runbook §4.3 before ANYTHING | none — reconciliation first | ABORT session launch path; console content wins over my expectation |
| FTP unreachable | Wait ≥60 s, ONE retry with a fresh connection; if still dead → STOP | same curl | ABORT (no staging possible; no alternate channels) |

GATE→Phase 1: manager answers; staged inventory matches Track 8.

## 2. Phase 1 — comp6 one-pass build + stage (game stays CLOSED; ~15–30 min; controller tool window)

The real procedure (ONE-PASS-CHECKLIST, from workspace root `R`; `MY = $W/worker-comp6-driver`):
1. Regenerate attestations if any §2 file changed since generation:
   `python3 $MY/regen_extra_receipt.py` and
   `python3 $MY/regen_replace_receipt.py --program-source outputs/2026-10-04-zcode-controller/comp6-integration/<program-dir>/UE4SSProgram.cpp`
   → `receipts/composition6-extra-receipt.json`, `receipts/composition6-replace-receipt.json`.
2. Cheap pre-flight: `python3 $MY/build_runtime_composition6.py --verify-only --program-source <same> --bridge-sha256 feed52544023360326c17f7015336f90c4906789b344a805cfb6debb67bdd7a9`
   Expect: fixed-script sha OK · ODR sha-equality OK · receipt coverage OK · 14/14 chain symbols. Seconds, no console contact.
3. THE pass: same command without `--verify-only`. Phase A bounded edition → Phase B `package_prx.py` 17/17 static checks.
4. Post-build acceptance (read artifacts, do not interpret loosely): run receipt status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`; `retained_units_replaced == ["program"]`; `undefined_delta` = added `[sceKernelGetModuleInfo]`, nothing removed; `staging/chain-symbols.json` 14/14; `staging/odr-gate.json` byte-identical at the EDITED port sha; 17/17 packaging, `gaps: []`. Record candidate `prx-candidate-composition6-1/modbridge_supplier_composition.prx` sha+bytes.
5. Gate v2 (`worker-gate-evolution/gate_check_composition5_v2.py`, manifest mode) must return GO.
6. Stage via `outputs/2026-10-04-zcode-controller/stage_composition5_cycle.py` (upload + readback band check). **Never launches.**

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| `--verify-only` all OK | ADVANCE to the real pass | step 3 | — |
| `ODR GATE REFUSED` / `UNEDITED` / `receipt does not cover` / `carry-forward CHANGED` / `bridge drift` | Fail-closed by design: fix the named cause (regen receipts, land the edit, byte-identical headers, use attested sha). **Never bypass — no bypass flags exist** | the named fix, then step 2 again | ADAPT (fix), fresh run dir for any failed run (kept for forensics) |
| `JOB FAILED: <step> exit N` anywhere | **STOP for the whole session's launch path** (master-plan law). Everything preserved | none | ABORT launch; the session can still do game-closed reconciliation only |
| Build OK but gate v2 ≠ GO | **STOP.** No staging without gate GO | none | ABORT launch path; candidate stays a candidate |
| Gate GO, stage readback band mismatch | Re-transfer per the tool's law only if the tool says re-transfer; otherwise STOP+preserve | tool-defined | ABORT on second failure — reconcile, never blind-retry |
| `composition run dir already exists` / `output dir exists` | Fresh-dir semantics: use new names or consciously remove (P8/P9 discipline) | — | ADAPT naming only |

Rollback pointer: worker-rollback-runbook — never replay a failed activation.

GATE→Phase 2: gate v2 GO **and** staged readback verified. (If comp6 is not runnable when the session opens, Phase 2's payload flows are still lawful — comp6 stages the composition PRX for launch 9; do not improvise a substitute.)

## 3. Phase 2 — game-closed console batch (one tool window; game CLOSED throughout; ~40–70 min)

`cd $W/worker-console-batch-runner` first. Optional preview: `python3 console_batch.py plan` (touches nothing).

### 3.1 `python3 console_batch.py dll8`

Chain (each step = a gate): preflight identity (`79281191…` / 336552 B + receipt flags) → G0 `GET $HTTP/list_payloads` → upload ONCE as `ALLFAM8_DLL_TEST.elf` → `manage:check` → FTP readback **EXACT** → dispatch ONCE → wait 15 s → fetch `/data/modbridge-owned-dll-candidate.jsonl` → verdict vs EXPECTED-DELTA-8FAM (4 new records, positions 37–40; nine family keys incl. static_tls+seh64; consumer_imports 90; helper_imports 10; results [86,93]; memory.last 114688→163840; cleanup {pending:0,retired:1}; first_failure all zeros).

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| All gates green | ADVANCE to e2e | `python3 console_batch.py e2e` | — |
| Preflight identity FAIL (`BUILD-ID LAW`) | **STOP** before any console step | re-hash the artifact; reconcile with worker-dll-8fam-combined | ABORT; never upload unverified bytes |
| G0: 8084 no answer | **STOP** (no alternate ports, never 9223) | none | ABORT |
| Upload rc≠0 / no OK | May re-upload **to the SAME alias only**; never overwrite a pre-existing payload name | retry upload once | ABORT on second failure |
| Readback sha ≠ `79281191…` or not ELF | **STOP. Do not dispatch bytes-divergent payloads** | none — preserve the readback | ABORT |
| Dispatch timeout | **Crash signal.** STOP, preserve, ping/user-check console; never resend | none | ABORT; console health check before anything else |
| Verdict ledger mismatch (any record field) | **STOP.** No re-dispatch; everything preserved for reconciliation | none | ABORT |
| SEH64-specific failure (-130..-137, stage-55/56 dual-owner stop, consumer_imports 88/89) **and** window budget remains | Optional documented fallback: `python3 console_batch.py dll7` (appends at 41–44; per-build verdicts; record which ran) | `dll7` explicitly | ADAPT (attribution), never automatic |

### 3.2 `python3 console_batch.py e2e`

Chain: preflight 13/13 cmd SHASUMS + payload `a21771f8…`/681888 B + ELF magic → G0 → upload ONCE as `MODBRIDGE_INST_E2E.elf` → `manage:check` → readback identity gate **MODE=B** → 15-dispatch stage 0..6 cycle (stages 4–5 simulated, labeled), each dispatch: cmd_put (STOR only if no stale cmd file; stale ⇒ DELE first — never overwrite) → cmd readback sha == staged → dispatch ONCE → cmd_gone (file GONE **and** `.consumed` hash == last cmdfiles.log line) → evid (new lines == verbatim ledger) → §7 full-tree reset (only `/data/modbridge/**`).

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| All green + §7 reset done | ADVANCE to stage-ridealongs | `python3 console_batch.py stage-ridealongs` | — |
| SHASUMS ≠ 13/13 | **STOP** before any upload | re-verify cmd dir on disk | ABORT |
| Readback sha == `e3489297…`/681528 (MODE=A) | **STOP** — argv flavor; cmd files must not run | none | ABORT |
| Any other sha/size | PAYLOAD IDENTITY NO-GO → **STOP** | none | ABORT (never widen the gate) |
| Dispatch ambiguity (gone? consumed?) | **NEVER re-send.** Reconcile via `cmd_gone` + `evid` fetches first | the tool's reconcile reads | ABORT if ambiguous after reads |
| Interrupted stage-3 commit | The tool performs the **ONE permitted `recover.json` dispatch** then HALTS; the 35-line green ledger no longer applies | none after recover — reconcile | ADAPT exactly once, then ABORT |
| Ledger line mismatch | HALT with divergence printed; every byte preserved | none | ABORT |

### 3.3 `python3 console_batch.py stage-ridealongs` — DEFINITIVE staging (supersedes stage-qdrv1 + the manual observer ridealong)

Chain (ride.0→ride.10): preflight all FOUR fself identities + host-side recompute of the §3 readback constants → RETR
live ploader.ini pre-image + hash → console `.bak` backup (`ploader.ini.<TAG>.bak`) → bootstrap-or-ensure decision on
the pre-image → local byte-exact end-state proof → ini STOR (skipped when already byte-exact canonical) + fresh RETR
byte-exact → freshness baselines (UE4SS.log stat, observer log absent OK, probe sinks if present) → STOR each PRX under
its OWN name with the §3 SHT-strip readback law: xwp-armed `48949316…`/20013 B (readback **67832 B** `daea2ff4…`),
fc1pool `d2431bf8…`/22733 B (**67848 B** `7e2dce27…`), qdrv1 `52c32216…`/26157 B (**87112 B** `71406d3c…`), observer
`1afa0997…`/30152151 B (**30143976 B** `3f89a5b6…`) — a failed readback re-STORs from the verified local file EXACTLY
ONCE, then aborts.

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| All steps PASS; ini readback byte-exact canonical, exactly **8 registered** (9-entry law; the mode-0 igf line consumes no slot; carrier is the 8th registration) | ADVANCE to stage-settings | §3.4 | — |
| **INI-BOOTSTRAPPED** (`ini-bootstrap-verdict.txt` in the run dir: the ONE sanctioned whole-file STOR of `ploader.ini.canonical`, 440 B `fd9c7393…`, with the `.bak` + byte-exact RETR diff — taken when the Step-0 pre-image is the historical aligned_entry/path_test-before-composition shape that NO sequence of appends can fix) | **Sanctioned one-time rewrite — ADVANCE.** Record the verdict file as the receipt; never trigger a second whole-file STOR in the same session; a re-run on the now-canonical console is an ensure/no-op | §3.4 | ADAPT (this is the EXPECTED branch on the Track-8 pre-image, not a fault); any proposal of a second bootstrap = STOP |
| Ensure-only convergence (canonical-order pre-image; missing lines inserted by CONTENT-ANCHORED ensure ops, never tail-append) | ADVANCE; ini STOR + readback byte-exact | §3.4 | — |
| `.bak` write FAILS | **STOP — never edit without a rollback artifact** | none | ABORT |
| AMBIGUOUS ini state (duplicate, missing pre-existing line, mode divergence, unknown line, unparseable, mode 3) | **STOP — pre-image preserved in the run dir; reconcile, never blind-edit** | none | ABORT |
| Any PRX §3 readback FAIL | Re-STOR from the verified local fself ONCE, re-verify | retry once | Second failure → ABORT staging (leave console as-is, reconcile) |

Legacy fallback only: `stage-qdrv1` (qdrv1-only: preflight `52c32216…`/26157 → `.bak` → minimal append
`2=/data/plugins/modbridge_qh_driver.prx` → STOR + readbacks) and `stage-observer` (observer-only, superseded 6-entry
COMBINED law, 5 registered) remain in the tool for pre-adoption console states; neither runs inside `all`. The manual
STAGE-PLAN-COMBINED §3–4 curl + SC-5 procedure they once required is superseded by this flow.

### 3.4 `python3 console_batch.py stage-settings` — settings+loadlist LOCKSTEP restage (NEW-SETTINGS §3; game CLOSED)

Chain (set.0→set.4): v3 pair identities + host lockstep proof (loadlist settings row == 1448 == settings bytes) →
FREEZE: RETR the staged pair and classify against the recorded v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…`
(an already-v3 console is an honest no-op) → UPLOAD the v3 pair as ONE lockstep restage (`worker-settings-v3-package/
package-v3/ue4ss/UE4SS-settings.ini` 1448 B `d8faa4ec…` with `[Hooks] HookProcessInternal=0` +
`HookProcessLocalScriptFunction=0`; `loadlist.txt` 447 B `4d98ecd7…`) → READBACK BOTH byte-exact → doc/rollback note.

| Verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| Both readbacks byte-exact | **GATE→Phase 3**: all flows green; ini registers exactly **7**; readbacks byte-exact | Phase 3 (§4) | — |
| Freeze = already-v3 | Honest no-op — proceed to the launch gate | Phase 3 (§4) | ADAPT (skip restage) |
| Freeze divergence (neither v2 nor v3) | **ABORT restage — divergent/unowned content is never overwritten** | reconcile first | — |
| Any readback mismatch | **Re-transfer (lockstep law) and re-verify; never launch on a partial upload** — that is exactly the MFA_E_SIZE (-8) failure mode | re-run `stage-settings` | ABORT if it will not converge |

### 3.5 Legacy manual paths (fallback only — not part of the recommended session)

The manual STAGE-PLAN-COMBINED §3–4 observer curls + SC-5 ini edit and the manual NEW-SETTINGS §3 curl restage
(package-v2 upload, 834 B readback) are superseded by §3.3/§3.4. Use them only when reconciling a console state that
predates the FINAL ini law, and reconcile vs worker-rollback-runbook §4.3 first. `rollback-ridealongs`
(STAGE-PLAN-COMBINED §7) flips BOTH ridealong lines to `= 0` (mode 0 = skipped at parse; probes/igf untouched) with a
fresh `.bak` + byte-verified readback — the recovery path after any post-launch FAIL line.

## 4. Phase 3 — the game session (~15–20 min; only the user can launch)

One command drives it: `python3 console_batch.py game-evidence` (steps ge.0→ge.10, stop-on-first-failure, rescue steps only where designed).

### 4.1 My actions and gates

- **ge.0 HANDOFF** (USER ACTION gate; tool waits for `done`/`abort`): ask the user to (1) arm ploader with game CLOSED, (2) launch Townfall, (3) wait ~60 s. I wait for the OK popup (1–60 s; **HTTP OK ≠ armed**).
- **ge.1 Step A**: `post_launch_check.py --json` (timeout 300). Routes per §4.2.
- **ge.2**: fetch the seven evidence sinks fetched (eighth checked by the staging flow) — S, L, D1, C2, the igf facts jsonl, and the three probe verdict sinks (bounded curl, fresh connection each; all-dead → wait ~60 s and retry once).
- **ge.3**: CP0–CP4 classifier over the fetched bytes → `cp0-cp4-verdict.json`. Not PASS ⇒ flow halts with the runbook triage pointer — **that is the honest result; no relaunch on top.**
- **ge.3b**: probe-verdict-sink freshness gate vs the staging baselines: `/data/modbridge-exec-write-probe.jsonl` must carry fresh `igd1-6ebab07f2111` records; `/data/modbridge-fc1pool-facts.jsonl` must carry fresh `fc1pool1-78508a2e2940` records with `fc1pool_result` verdict `validated` 11/11. Honestly recorded fc1 refusals are WARN with their reason codes (never auto-retried). These verdicts are EVIDENCE — the gate never certifies writability.
- **ge.3c** (ninth slot): cc1 census evidence gate vs the staging baseline: `/data/modbridge-carrier-facts.jsonl` must carry fresh `cc1-0864d65c1baf` records — success = `entry`/`startup` (polls 30, poll_s 4)/`mapscan`, 30 `qh_counter` lines, `census_match` lines, `census_result` verdict `CENSUS:VALIDATED` (controls `None_id_zero:1` + `ByteProperty_ascii_ok:1`, `negative_hits.PostRender:0`), `summary` `write_failures:0`, and exactly ONE `CC1:` line in UE4SS.log. `CENSUS:REFUSED` = honest terminal WARN (a NEW staged window, never a re-rating). EVIDENCE about which of the 53 fixed names are interned in the live FName pool for that launch only — it never certifies FUNC_Native, invocation rate, game-thread identity, or any UFunction-object walk.
- **ge.4/ge.4b**: `rung_check.py --rung RUNG1 --json` then `--rung RUNG2 --json` into the run dir (PASS/UNVERIFIED/CANNOT_ASSERT are all honest outcomes).
- **ge.5 CP5 HANDOFF**: ask user to load a save, get in-game, press OPTIONS once (brief press-release). Tool re-fetches L, classifies shapes a–d (§4.3).
- **ge.7**: ask for the VISUAL confirmation (menu overlay: title, 12 rows, controls line, input locked). Optional T4: OPTIONS again closes silently.
- **ge.8 CP6 HANDOFF**: UP/DOWN highlight, LEFT/RIGHT page (visual only), CROSS once.
- **ge.9**: re-fetch L; classify `Spawned …` vs `Failed at <stage>` shapes.
- **ge.10**: write `game-evidence-receipt.json` (honesty note: user's eyes decide; log lines never certify). CP7 optional: RUNG2 with driver; RUNG3 only via separate Rung3Probe per RUNG3-RECIPE — never blocks the goal.

### 4.2 Branch table — Step A / CP0–CP4 (post_launch verdicts + cp01234 cells)

Precedence top-down. "Next command" assumes I am inside `game-evidence` unless named.

| Evidence verdict | Meaning (fail-open semantics) | Decision | Next command | Abort vs adapt |
|---|---|---|---|---|
| exit 4 FETCH_FAILED (all sinks) | Transport, not evidence | Wait ~60 s, fresh connection, re-run | re-run Step A (bounded retries) | ABORT after repeated transport failure — no staging touch |
| `FAILED_<stage>` with mfa **-2** | MFA_E_NO_LOADLIST — /data timing window race | Relaunch **once** before re-staging anything | ask user to relaunch (counts as a NEW launch/window) | Second -2 → ABORT, reconcile staging |
| mfa **-6/-8** | staged-vs-final divergence / MFA_E_SIZE | **STOP** — detail names the file; fix staging | reconcile named file (re-stage + readback) | ABORT launch path until fixed |
| mfa **-3/-4/-5/-12** | loadlist grammar/title/limit/… | Regenerate loadlist from worker-loadlist-final/final-package — **adapt with a new revision, never hand-edit on console** | fix offline; restage + readback; then NEW launch | ABORT this window |
| bridge code **1/2/3/4** | CONVERSION/CONTEXT/START/INTERNAL stage | STOP at the named stage; runbook §6 triage (read S → D1 → L) | triage reads only | ABORT |
| `FAILED_STATUS` | F6/F7 silent trap: S says `failed` with no/other L line | **Trust S.** Treat as thread-spawn/init failure; read `detail` | triage reads | ABORT |
| `MODS_STARTED` | Best case so far | Proceed to CP file checks (ge.3) | continue flow | — |
| `ENGINE_READY` / `ACTIVATED` (WARN) | Still progressing | Re-run `game-evidence` in **30–60 s** | re-run | Frozen → F7/F8 bounded-wait hang check; only deadline = 5-min StartupWait (converts to `failed`, not a hang) |
| `STOPPED` | F10 stop/unload path | Record and reconcile | triage reads | ABORT |
| `UNKNOWN_NEW_LINES` | Non-baseline lines but no status/failure | Wait, then F0 (staging/ploader) | re-fetch after wait | Game running unmodded is **by-design safe** (fail-open) — no crash response |
| `NOT_STARTED` | Nothing beyond baseline + stale probe repeats | F0: check loadlist/staging/ploader arm | FTP reads of staged files | By-design safe; never relaunch before finding why the PRX never ran |
| CP cell `INCONCLUSIVE-STALE(session ms …)` | Evidence predates this launch (O2). **No PASS can come from a prior launch** — refuse-to-assert, not a crash | Re-fetch after confirming a fresh window; if the session ms is still ≤ `8866785`, the PRX never ran this launch → F0 path | re-run ge.2/ge.3 | Never green-light pad interaction from stale cells (the GAP-1a hazard) |
| CP0 `FAIL(F0 no NEW module_start_entered)` | PRX never ran this launch | Staging/ploader triage before anything | read ploader notifications; check ini | ABORT; do not relaunch the identical layout hoping for difference |
| CP0 `FAIL(F3/F4/F5 runtime_start_failed: …)` | runtime_start refused (capture/conversion/context/start) | Decode D1 `runtime_start_failed` value; F4 ⇒ verify `/data/plugins/modbridge_supplier_composition.prx` exists | runbook §6 | ABORT; new revision for the fix |
| CP0 `FAIL(F1 probe lifecycle)` | Probe refused wiring; `constructor_return` pairs name the failing ctor index | Triage reads | — | ABORT |
| CP1 FAIL (mfa_activate_failed / runtime_start_failed 0x1/2/3) | F2–F5 families; decode mfa code; `error.path` names the offending staged file | Triage; fix named file offline | — | ABORT this window |
| CP2: no `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)` equality | Outcome #2 acceptable (`Scan failed, falling back…` = vtable rescued — RECORD it); #3/#5 fatal (`Native game-thread initializer callback registration failed` + `failed` status); #8 `PS scan timed out` (the 120 s SecondsToScanBeforeGivingUp bound — an expected honest-stop shape) | #2: continue but record. #3/#5/#8: STOP, honest result | — | **Do NOT re-derive or patch addresses from here; report + reconcile with the director** |
| CP2/CP3: `mods-started` then crash right after | Outcome #6 (RISK: wrong-but-plausible hot address). Console crash report is the evidence | STOP; preserve crash report | — | ABORT; reconcile |
| CP3: `mods-started` + game alive + flat ticks | Outcome #7 (cold wrong address) — the ONLY "stuck at mods-started" shape | Distinguish via rung ticks; report | `rung_check --rung RUNG1` | ABORT; director reconciliation |
| CP3 FAIL(F7): `activated` then `state 2 failed 1 <what>` | Init thread threw before engine-ready; `what` is typed | Read `what`; triage | — | ABORT |
| CP3 FAIL(F8): frozen at `activated`, L static | Init hang | **Bounded wait watching mtimes** (the only deadline is the 5-min StartupWait) then unload + read `runtime_stop_result` | mtime checks | Bounded wait only; never indefinite |
| CP4 FAIL(F9) (`Error loading/executing script`, UEHelpers missing, main.lua not found, inventory mismatch) | Script layer, **NON-fatal** (S still reaches mods-started). Fix mod/activation content — **do NOT re-stage the PRX** | Verify staged shas (`f58604…` main.lua, `5a8f66…` UEHelpers.lua); fix content offline | — | ADAPT content with distinct revision; new launch for new evidence |
| CP4: whole-inventory `Native UE4SS inventory rejected: …` | FATAL unlike F9 | STOP | — | ABORT |
| CP4: banner absent with NO error line | Mod not in the startup list | Re-check activation inventory first | FTP read of the staged mod tree | ABORT until understood |
| Overall `INCOMPLETE` + stale hint | Session evidence unusable for PASS | Treat as the honest session result | record | No relaunch on top |

### 4.3 Branch table — CP5 (OPTIONS → menu) and CP6 (CROSS → spawn)

| Evidence verdict | Decision | Next command | Abort vs adapt |
|---|---|---|---|
| CP5 PASS: `<N> spawnable pickups found. Pages=<P>` + `Menu created.` (two new lines, in order) | Proceed to VISUAL gate | ge.7 handoff | — |
| CP5 shape (a) `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available` | **UNREACHABLE after mods-started ⇒ anomaly.** Check whether `UE4SS-settings.ini` was restaged; the `ProcessEvent …` variant is **restage PROOF** (expected live sha after the v3 restage: `d8faa4ec…`; v2 `9570ead3…` / pre-v2 `9f3fa127…` = the restage did not land). Keep KEEP-EngineTick; no ini change | reconcile staging provenance | ABORT; reconcile, never re-press |
| CP5 shape (b) `WidgetBlueprintLibrary not found.` / `UI creation failed: <err>` | UI build error; session healthy; retry-safe. Read `<err>`: UMG class/construct → layout gate; `Tried getting member variable '<Class>::<Key>'` → extended-pair gap (RUNG3-RECIPE F4) | iterate the named call offline | ADAPT with distinct revision IDs; never re-rate unchanged evidence |
| CP5 shape (c) `Scan failed: … 66: Item database not ready. Load a save first.` | Expected guard when P3 skipped (T3 passing) — **no fix needed** | ask user to load a save, then OPTIONS again | ADAPT (proceed to T2) |
| CP5 shape (d) ZERO new lines | Run the §4.3d ladder IN ORDER: (1) CP4 banner present? (2) staged `ue4ss/PS5Pad.ini` hash `1d919fbf…`? (3) clean single edge press? (4) liveness: rung ticks advancing (or outcome #7 check)? (5) known non-causes (nav silent menu-closed; non-exclusive binds; no ESC bind) | the ladder reads | If ALL hold ⇒ first genuine live dispatch-chain gap: **escalate to director with D1/S/L tails; do NOT re-press repeatedly** |
| T4: second OPTIONS closes silently | Success (closing is silent by design) | continue to ge.8 | — |
| CP6 PASS: `Spawned <Item> x<N> ID=<id>` **and** user sees the pickup | Runge-5 success. Log line alone is NOT enough; visible actor without a log line should be impossible | record; eyes-confirmation logged | — |
| CP6: `Failed at ground trace` | LineTraceSingle out-param marshalling (rank-3 B5 risk) | capture `<err>`; reconcile vs LUA-API-MATRIX B5; also check the user aimed at geometry | ADAPT distinct revision; retry-safe but never blind retry |
| `Failed at load pickup class` | `<err>` carries exact `Could not find class: <path>` | re-derive class path from the real build, new revision | ADAPT |
| `Failed at spawn actor` | Engine-side refusal (`Could not construct` family); game-thread identity already guaranteed | reconcile with rung-2/rung-3 evidence | ABORT-and-reconcile |
| `Failed at configure pickup` (`no PickupInteractable` / `no valid inventory ID`) | **NOT a successful spawn even if something appeared** | fix property/component assumptions, new revision | ADAPT |
| `Spawned` logged but nothing visible | Investigate (spawned into unloaded/hidden space) | reconcile | — |
| CP7 RUNG1/RUNG2 | `0` PASS (needs snapshots straddling the **8 s** 0→1→2 ramp; re-run every **~20–30 s**) · `1` FAIL/UNVERIFIED (incl. ticks-flat after the terminal record froze the file — honest negative) · `3` CANNOT_ASSERT · `5` INCONCLUSIVE-STALE (re-run only in a fresh window) · `4` FETCH_FAILED (retry ~60 s) | per code | Never re-rate unchanged evidence; `UNVERIFIED_DRIVER` is honest (G2), absence of rung evidence ≠ launch failure |
| CP7 RUNG3 `valid=false` | F1 (FName unresolved — NOT evidence of absence) vs F2 (genuine negative); `UNREACHABLE` = signature F11 | RUNG3-RECIPE §7 | F11 ⇒ STOP and reconcile |
| RUNG3-5 placeholders | Count-only; never affect exit codes; never advance a rung | record | — |

## 5. Timing discipline — what I check at each wait

| Window | Bound | What I actually check |
|---|---|---|
| Between ANY two dispatches | **≥30 s** | Count on the clock before the next dispatch (crash-1 lesson; e2e cycle is 15 dispatches ⇒ ~15–25 min by itself) |
| Dispatch response | timeout = crash signal | On timeout: STOP, preserve, ask user to check the console; NEVER resend |
| ploader arm | OK popup 1–60 s | Wait for the popup; **HTTP OK ≠ armed** |
| Records after launch | ~1 min | First fetch only after ~60 s of game run; `game-evidence`'s own gate encodes this |
| FTP reads after events | 15–30 s, fresh connection, bounded `--connect-timeout 10 --max-time 60 --max-filesize 32M` | Retry at ~60 s intervals, bounded; never hammer |
| dll8 post-dispatch | 15 s | Tool waits internally before reading the results JSONL |
| Still-progressing verdicts (ENGINE_READY/ACTIVATED) | 30–60 s | Re-run `game-evidence`; frozen ⇒ F7/F8 check |
| Init hang (F8) | **5-min StartupWait is the ONLY deadline** (converts to `failed`, not a hang) | Watch S/L mtimes during the bounded wait; then unload and read `runtime_stop_result` |
| PS scan | **120 s** `SecondsToScanBeforeGivingUp` (staged ini) | `PS scan timed out` = outcome #8 honest stop, not a hang |
| ploader injection of 8 plugins | ~10–15 s total inside the 120 s `INJECTOR_TIMEOUT_S` | Read every notification line: expect EXACTLY 8 `OK (<ms>): <file>` lines and ZERO FAIL (the definitive 9-entry layout registers exactly 8; mode-0 igf consumes no slot). ANY FAIL retries the whole list — STOP on FAIL, then `rollback-ridealongs`; never relaunch an identical rejected layout |
| Ridealong windows | observer 30×2 s ≈ **60 s**; qdrv1 30×4 s ≈ **2 min** (from each module's OWN module_start) | If the title is slower, windows close with honest negatives (stat_failed / admission_rcode ≠ 0) — design bounds, not interference; a new window needs a NEW launch |
| Rung ramp | 8 s 0→1→2 | `rung_check.py --rung RUNG1` re-run every **~20–30 s** to straddle; after the terminal NQH1 record the file freezes → later runs honestly report ticks-flat FAIL — do not re-rate |
| Boot overall | ~5 min | Everything above nests inside this; nothing I do may extend a deadline or bypass a timeout |

### The exact moments I hand to the user (7 required + 2 eye confirmations + 1 optional)

1. Phase 0 + Phase 2 precondition: "keep Townfall fully CLOSED" (check, twice).
2. **"arm ploader"** (Payload Manager UI / loadpayload) with game CLOSED — then WAIT for the OK popup.
3. **"launch Townfall"** — then wait ~60 s.
4. **"load save"** (P3) then **"press OPTIONS once"** (brief press-release; ONE press).
5. EYES: **"is the menu overlay VISIBLE?"** (title, 12 rows, controls line, input locked). Optional T4: OPTIONS again closes silently.
6. **"navigate UP/DOWN (LEFT/RIGHT pages), press CROSS once, watch for a new pickup."**
7. EYES: **"do you see the new pickup at the aim point?"**

## 6. Do-not items earned this session (carry into every branch)

1. **No re-dispatch on ambiguity.** The argv double-dispatch crashed the console (crash 1). Ambiguous ⇒ reconcile via `cmd_gone` + `evid` reads; the tool never re-sends on its own and neither do I.
2. **Timeout = crash signal, never a retry trigger.**
3. **Stale-receipt lesson:** the master plan's [PENDING] marks were stale (freshness patch now applied; 8fam receipts landed). Re-hash bytes on disk before acting; trust no status string over the file.
4. **No upload before gates.** Build/gate GO → upload → readback EXACT → dispatch. Never dispatch bytes-divergent payloads; never overwrite a pre-existing payload name; stale cmd file ⇒ DELE first.
5. **Fail-open meanings (do not over-react, do not under-react):** `NOT_STARTED`/`UNKNOWN_NEW_LINES` = game running unmodded is by-design safe → F0 checks, no crash response. `INCONCLUSIVE-STALE` = refuse-to-assert, evidence from a prior launch can never PASS. `UNVERIFIED_DRIVER`/`CANNOT_ASSERT` = honestly unevidenced, not a tool or launch failure. WARN verdicts = bounded wait, not abort. "delivery stdout_error 32 with stdout_records 1" = known console-only benign delta in the dll8 ledger.
6. **Gate mismatch ⇒ STOP + preserve + reconcile; never relaunch an identical rejected layout.** ploader quirk: any FAIL mid-list retries the WHOLE list — read every notification line, STOP on FAIL.
7. **No re-rating unchanged evidence.** New window ⇒ NEW launch; every observed failure gets a distinct revision ID.
8. **Visible menu + visible pickup are the USER's eyes; log lines never substitute** (runbook G4/G5; the tool's own receipt states this).
9. Silence by design is not breakage: nav binds are silent while the menu is closed; closing the menu is silent; navigation logs nothing. Do not chase these.
10. Never probe alternate ports (manager is 8084 only); never run both dll7+dll8 by default (dll7 = SEH64-specific fallback with budget); never re-derive/patch tick addresses from a CP2 failure (director reconciliation); never launch the FName FC1 probe (exec-page reads crash); never claim >current-rung acceptance from host tests; never bypass a comp6 gate (no bypass flags exist).

## 7. Command cross-check against tools on disk (2026-10-04, this worker)

| Command | Disk state | Verdict |
|---|---|---|
| `$W/worker-console-batch-runner/console_batch.py plan/dll8/dll7/e2e/stage-ridealongs/stage-observer/stage-qdrv1/stage-settings/rollback-ridealongs/game-evidence/all --dll 7\|8 --dry-run --assume-yes --run-root` | CLI verified against the FINAL revision (`FLOWS` = 10 flows + `plan`/`selftest`; `add_flow` parsers; STOP_ON_FAILURE default True, `--continue-on-failure` opts out; file sha `5ca8d1c7…`) | ✓ usable as-is |
| `console_batch.py selftest` | present; **372 ok / 0 failed** per worker-batch-final-ini-adoption receipts (incl. fake console drive 90 green / 0 failed) | ✓ optional re-run host-side |
| `$W/worker-post-launch-tools/post_launch_check.py [--json|--dry-run|--baseline-file|--from-dir|--host|--port|--timeout|--max-bytes]` | present; exit 0/4; verdict set matches §4.2 | ✓ |
| `$W/worker-rung-evidence-reader/rung_check.py [--rung RUNG1|RUNG2|--discover|--json|--from-dir]` | present; exit 0/1/2/3/4/5; O2 gate in file (line ~283) | ✓ |
| `$W/worker-game-session-ridealongs/simulate_ploader_ini.py` | present | ✓ |
| `$W/worker-settings-v3-package/package-v3/*` (v2 = recorded freeze/rollback inventory) | v3 re-hashed: settings 1448 B `d8faa4ec…` + loadlist 447 B `4d98ecd7…`; v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…` | ✓ staged by `stage-settings` |
| `$W/worker-record-sink-addon/modbridge_qh_driver.prx` (26157 B), `$W/worker-observer-packaging/modbridge_runtime_aligned_observer.prx` (30152151 B) | both on disk, sizes match pins | ✓ |
| Observer+probe staging inside console_batch.py | PRESENT — `stage-ridealongs` (DEFINITIVE: both probes + qdrv1 + observer + cc1-carrier + FINAL 9-entry ini; legacy `stage-observer`/`stage-qdrv1` fallbacks) | ✓ tool path (§3.3) |
| `stage-settings` flow | PRESENT (set.0 freeze v2 inventory → v3 lockstep upload → byte-exact readback BOTH; already-v3 = no-op) | ✓ tool path (§3.4) |
| `outputs/2026-10-04-zcode-controller/comp6_one_pass.py` | **pre-flight skeleton only — does not build/gate/stage** | ✗ use ONE-PASS-CHECKLIST commands instead |
| `$W/worker-comp6-driver/build_runtime_composition6.py` + `regen_extra_receipt.py` + `regen_replace_receipt.py` | present; P1/P6/P8/P9 preconditions verified green | ✓ |
| `$W/worker-gate-evolution/gate_check_composition5_v2.py`, `outputs/2026-10-04-zcode-controller/stage_composition5_cycle.py` | both present | ✓ |
| Observer staging inside console_batch.py | superseded row — see the `stage-ridealongs` row above (LANDED, definitive) | ✓ tool path |
| SHT-strip readback law (STAGE-PLAN §3) | enforced by `stage-ridealongs` (host recompute at ride.1); constants pinned (87112 / 30143976 / 67832 / 67848; body shas `71406d3c…` / `3f89a5b6…` / `daea2ff4…` / `7e2dce27…`) | ✓ in-tool |

**[PENDING] branches at brief time:** worker-exec-writability decision tree (empty dir — default verify-then-write-owned-files rule stands); patches.json 51→54 apply; comp6 required-slots SLOTS-FEASIBILITY §4b/§5 bypass build. (Observer/probe staging tool extension has LANDED as `stage-ridealongs` — removed from this list at DOC-SYNC.) None of these blocks the branch tables above; each has a manual fallback documented in its section.

> **SESSION-EVE UPDATE:** comp6 candidate ready+approved (76ee6af3); console unreachable at close — Phase 0 begins with the upload retry; if servers still down, the user reboots/re-arms the console homebrew before anything else. The r11 slot backup exists.
