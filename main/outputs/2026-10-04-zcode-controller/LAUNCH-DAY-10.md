# LAUNCH-DAY-10 — one-page execution runbook (controller; game session launch-10)

> **REV 2026-10-05b** (worker-launchday-update): adds the post-popup evidence layer that landed AFTER this runbook was written. Sources integrated: `worker-post-popup-evidence/POST-POPUP.md` (+ its `FAILURE-TAXONOMY.md`), `worker-mod-load-readiness/READINESS.md`, `worker-launch10-acceptance-criteria/ORACLE.md`. The ORACLE verdict definitions are UNCHANGED — the post-popup layer only EXTENDS the BEST verdict's post-load evidence set (§4 sub-table; every rung there is G4-class, NOT a launch-10 verdict input). Gate §1 note (rev b): the build-receipt pins now live in `launch10-build/EDITION.json` (edition reads `launch10-final-a …` on disk) and the stage-header receipt must show BOTH `53b1188f` (anchor cap) AND `67170b21` (capture capacity) — §1 updated accordingly. All rev-a content otherwise preserved byte-for-byte.

Consolidates COMPLETED deliverables: `ORACLE.md` (worker-launch10-acceptance-criteria) · `evidence.sh` + `STAGE-TABLE.md` (worker-launch10-evidence-upgrade) · staging flow = `console_batch.py` `stage-launch10` (the worker-launch10-staging-packet dispatch spec — NOTE: that worker dir was EMPTY at consolidation time (`run_stage_launch10.sh`/`DIFF.md` not yet delivered); the integrated `stage-launch10` flow l10.0–l10.7 is used verbatim; substitute the standalone script 1:1 when it lands — contract identical) · `launch10-build/EDITION.json` + `launch10_one_pass.py` · `launch10-prep` baselines. Placeholders `<L10_…>` are marked and MUST be filled before their step. Nothing here is an outcome promise — only the console launch is evidence. If the build has not landed (EDITION.json still reads "launch10-c pending"), gate §1 is simply not green: do not stage.

## 1. PRE-LAUNCH GATE (controller; game CLOSED; every box green before the user is called)

Build receipts (`launch10-build/`):
- [ ] `launch10_one_pass.py --full` completed: `prx-candidate-l10-1` + `prx-candidate-l10-2` exist; EDITION.json unit shas re-verified (the runner refuses drift).
- [ ] gate v2 GO ×2: `gate-receipt-l10-prx-candidate-l10-1.json` and `…-2.json`.
- [ ] determinism: one-pass printed the SAME composed-object sha for both runs (`coherent-tick-file-witness-runtime.o`).
- [ ] witness post-build PASS: each run's `receipt.json` status = `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED` and its `runtime_sha256` equals the on-disk object.
- [ ] stage-header pin (rev b, BOTH pins): `shasum -a 256 launch10-build/staging/live_module_facts.hpp` = `67170b21fc3b…` on disk (EDITION.json edit #4 = `launch10_one_pass.py --stage-header` arg) — the stage-header receipt must show BOTH `53b1188f` (anchor cap, kMaxAnchorMappings 8→16) AND `67170b21` (capture capacity, kMaxVmMappings 1024→16384); pins live in `launch10-build/EDITION.json`.

Console pre-state (read-only): G0 `http://ps5-host-not-configured.invalid:8084/list_payloads` → 200 · ploader.ini still the 2-plugin config `1216aab6…` (composition=2, path_test=2, all else 0) · settings v3 `d8faa4ec` · mod tree verified (main.lua 21415 B, UEHelpers 9962 B) · current PRX pre-image readback `d86887ec…` / 30,756,152 B.

Staging flow (the ONE console write; the ini is NEVER written by it):
`python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py stage-launch10 --fself <L10_FSELF_LOCAL_PATH> --expect-sha <L10_FSELF_SHA256>`
- [ ] l10.0 FSELF identity pinned (refuses placeholders; BUILD-ID law) · l10.1 ini frozen byte-exact `1216aab6…` · l10.2 old-PRX pre-image + console `.bak` secured FIRST · l10.3 new FSELF STORed EXACTLY ONCE to `/data/plugins/modbridge_supplier_composition.prx` · l10.4 readback content law OK (full byte identity, or ONLY the ftpsrv SHT-strip class; readback sha/size recorded = `<L10_READBACK_SHA>` / `<L10_SIZE>`) · l10.5 ini STILL `1216aab6…` after · l10.6 fresh baselines re-fetched into the run dir · l10.7 `l10-go-nogo.json` = **GO**. Any failed verification → the OLD PRX is restored ONCE byte-exact, reconcile, never blind-retry.
- [ ] drift-free proof: `evidence.sh --from-dir <staging-run-dir> --baseline-dir outputs/2026-10-04-zcode-controller/launch10-prep` → `NOT_STARTED` with ZERO warnings.

## 2. USER INSTRUCTION (one line — read verbatim)
"Launch Townfall normally; when the menu appears, note any plugin toasts, then LOAD A SAVE — after loading a save, press OPTIONS (opens the F7 menu), navigate with the D-pad, confirm a spawn with CROSS (X), and report what appears." (standing rule, ORACLE G4 / ge.5 gate: no OPTIONS/spawn attempt until the `[Lua] [TownfallItemSpawner] Loaded` line is seen in UE4SS.log — mods-started evidence first; full copy-paste pad walkthrough: POST-POPUP.md §1)

## 3. POST-LAUNCH EVIDENCE (controller; after the user reports; records appear ~1 min post-launch — retry once at +60 s)
```
POST=<stage-run-dir>/post; mkdir -p "$POST"
curl -fsS "ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log" -o "$POST/UE4SS.log"                                        # PRIMARY (PATH_TEST/ACTIVATION_TEST lines ride here)
curl -fsS "ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt" -o "$POST/runtime-status.txt"    # PRIMARY (session truth; fresh session != 273)
curl -fsS "ftp://ps5-host-not-configured.invalid:2121/data/modbridge-supplier-diag.jsonl" -o "$POST/modbridge-supplier-diag.jsonl"        # IF PRESENT (absence is normal)
curl -fsS "ftp://ps5-host-not-configured.invalid:2121/data/modbridge-exec-write-probe.jsonl" -o "$POST/modbridge-exec-write-probe.jsonl"  # drift guard only
curl -fsS "ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log" -o "$POST/entry.log"                       # history guard only
sh outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh \
   --from-dir "$POST" --baseline-dir outputs/2026-10-04-zcode-controller/launch10-prep --out "$POST/summary.txt" --json-tail
```
- exit 4 = both primaries unreachable → wait ~60 s, re-fetch, re-run (do NOT touch staging on this signature).
- ANY CONFIG-DRIFT warning (entry-log/xwp growth, DIAG lines) → STOP; verify ini `1216aab6…` before attributing anything.
- Timing table = the summary's `timing (per-writer…)` block: pathtest monotonic-ns stamps, r4 `[t=<ms>ms pid=<pid>]` per-pid ms, supplier-diag `t_ms` deltas. GUARD: per-writer sequences only — NEVER subtract across writers. PATH_TEST stamps prove full ini traversal (path_test is the TERMINATOR, after composition `module_start` returned).

## 4. VERDICT (controller declares EXACTLY ONE, per ORACLE.md §2)
Read order: runtime-status (session truth) → supplier-diag/observer (stage attribution) → UE4SS.log (detail); all sinks must agree on the fresh session. Justify with 2–4 verbatim evidence lines.
- **BEST** = conversion clean (D1+D2 extents, anchor_count>0, intervals>0, `bridge_cfg_*`, `runtime_start_ok`, `activated`; no D6/D7/E1/E2/C13) + armed marker F1 + the five bind lines (F6 slot0 / F14 pool / F9 slot3 "V1 instance + V2 vptr match" / F18 slot2 behaviorally verified / F21 slot4) + `engine-ready` + `mods-started` + mod load ATTEMPTED (G1 `start_mods<LuaMod>`, then G2 `[TownfallItemSpawner]` prints OR G3 mod-error lines) + NO-CRASH. Post-mods-started failures do not demote.
- **GOOD** = BEST-1 + F1/F6/F14/F9 (past the GMalloc V1/V2 gate) + a NAMED later refusal durable in status failed detail (F30) + NO engine-ready + NO-CRASH. F15 (heap-shape refusal) caps the verdict at GOOD.
- **MINIMAL-ACCEPT** = D1+D2 present, anchor_count>0, intervals>0, `bridge_cfg_*`, `activated` present; D6 did NOT occur; NO-CRASH. Everything after conversion may be any named failure; a C13 recurrence or C10/C11 anomaly additionally raises a REGRESSION flag (r14 reconciliation before launch-11).
- **FAIL** = any of: D6 `…no observed mapping interval intersects the module image extent with valid geometry`; crash (NO-CRASH sense, incl. the first-vtable-walk site F29); B4 rebuild-dead or F0 silent no-op; irreconcilable sinks.
- **S-30S (special):** a composition FAIL toast at dt≈30 s is the ploader cap, NOT a verdict — the module keeps running; judge by the durable sinks; if they show completion, apply the ladder normally and record "C_comp > 30 s" as a launch-11 latency item.
- **NO-CRASH (special, independent):** the game must SURVIVE every acceptable outcome; crash = FAIL regardless of prior progress; a silent no-op (no `module_start_entered`, no failure line, no status change) is also FAIL.
- **Predicted pair:** `lmf_anchor_count=7` + `bridge_lease_reg_spans=6` + `bridge_lease_reg=6` + `lmf_intervals_found=1` — intervals is the COALESCED count (the six readable records are pairwise ADJACENT); 6 is NOT expected and would be a page-gap investigation trigger (not automatic failure); live 1 is NOT a fix failure despite the geometry fixture suite assuming 6. The hard gate is >0 (0 = the launch-9 geometry death).

### 4b. Post-load evidence (rev b — EXTENDS the BEST verdict's post-load evidence set; verdict definitions above are UNCHANGED; every row here is ORACLE G4-class and NOT a launch-10 verdict input — POST-POPUP.md §2/§3)

| Probe | Evidence law (POST-POPUP.md) |
|---|---|
| First OPTIONS press after map load = free drain probe | The map-load pre-hook resets mod state, so the FIRST press always scans and always logs: ≥1 NEW `[TownfallItemSpawner]` line MUST appear (`<N> spawnable pickups found. Pages=<M>` or `Scan failed: <err>`). ZERO new lines = keybind-unreachable OR drain-stalled, discriminated by the init-era `[R3] refused:` / `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` lines (present ⇒ R3-variant drain stall, expected on that variant, not new info); no `[R3]` lines ⇒ input-source problem (also possible, at the FIRST press only in this mod: `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)`). |
| Navigation (D-pad UP/DOWN marker, LEFT/RIGHT pages) | MUTE BY DESIGN — UP/DOWN/LEFT/RIGHT and the Quantity/Page rows print nothing; their evidence is screen-only (red `>>` marker, `Page: n/m`). Silence during navigation is NOT a stall. OPTIONS press #2+ on an already-built menu is also mute by design (buildUI early-return; scan only runs when `items` is empty) — only the FIRST press after a map load is the probe. |
| `Spawned <name> x<Q> ID=<id>` | Closes the SPAWN STAGE (rung 4): the full spawn path ran (pawn reads → ground trace → pickup-class load → deferred spawn → configure). VISIBLE-spawn confirmation additionally needs the user's visual report / a captured frame showing the actor (rung 5) — per the oracle's G4 discipline, visible spawn is NOT a launch-10 verdict input: record it, never claim it. |

## 5. IF-THEN BRANCHES (one line per named refusal class; ORACLE §3 + named bridge/entry classes)
| Refusal | Means | Immediate next action |
|---|---|---|
| `base 0x<derived> != … (image moved)` | ASLR/rebase bias moved the image | re-derive seeds with the new base evidence; never replay |
| read-path guard `slot <n> …: seed class <class>; refusing …` (slots 0/1/3 data, 2/4 exec-only) | guard refused a misclassified seed — it worked | derive afresh with the printed class; new ids |
| `slot 0 g_uobject_array: seeded address is null / not 8-aligned` | admission probe failed | derive afresh |
| `slot 3 gmalloc: … not a live aligned FMalloc*: <probe.failure>` | weak live-pointer probe refused | derive afresh |
| `slot 3 gmalloc V1 … [0x<malloc>] != derived instance 0x97089b8` | [GMalloc] holds a live-but-foreign pointer | rank-1 rule refused; re-derive instance anchor |
| `slot 3 gmalloc V2 … vptr != derived vtable 0x82c26e0` | RELRO vptr mismatch class | re-derive (constant compared, never dereferenced) |
| `FName pool … interned ids still zero after the wait budget` | pool never initialized in budget | timing class; launch-11 latency/budget item; derive afresh |
| pool-shape refusal `CurrentBlock < 1 / Blocks[0] is null / heap span is empty / Blocks[0] outside the readable heap span` (+ observed values) | vintage-transient heap moved off [0x8018c000,0x100118000) | observed CurrentBlock + Blocks[0] ARE printed → feeds launch-11 worker-fname-heap-runtime-bounds; verdict cap GOOD |
| `pool entry id …: Blocks[<n>] is null / …past CurrentBlock / past CurrentByteOffset / exceeds FNameMaxBlocks` | pool bounds chain | derive afresh |
| `pool entry id … (expected 'None'/'ByteProperty'): wide/length/payload/characters` | interned-content mismatch | derive afresh; terminal once shape holds |
| `slot 2 … behavioral probe … NAME_None / id diverges` | ctor call did not take effect | ctor label refuted; derive afresh |
| `slot '<name>' is not present in the seed table` | wrong table shipped (all-or-nothing) | DORMANT→REGRESSION: verify which SeedTable.h is in the staged fself (readback sha) |
| `[Seeds] Required-slot seed table incomplete…` | armed 4/4 did NOT ship | REGRESSION (evidence.sh SHAPE_A_SEEDS_GATE): readback first; no in-game interpretation |
| `mfa_activate_failed code=<mfa> detail=<file>` (B7) | loadlist/roots drifted; line names the file | game unmodded; reconcile loadlist `4d98ecd7`/settings; verdict FAIL with named cause |
| `bridge_exepath_source=0` / GameExecutablePathMissing (C7) | /data mount shape changed | r10 `stat_app0/ownmod/rootdir` events are the evidence; re-derive the exepath route |
| `lease coverage gap: requested [0x<lo>,…` (C12) | r14 hardening WORKING (loud pre-lease refusal, MRS_ERR_CONVERSION by design) | read `bridge_lease_gap/ref/near` tags; reconcile registry derivation for launch-11 |
| `Owned read range includes unregistered bytes` recurrence (C13) | rebuild did NOT carry the r14 gate | REGRESSION: verify staged readback sha first; r14 audit before launch-11; verdict ≤ MINIMAL-ACCEPT |
| D6 `no observed mapping interval intersects…` | the launch-9 geometry death, live | FAIL; capture the `lmf_*` fan-out; reconcile with worker-facts-geometry-fix; never replay |
| `runtime_start_failed code=2 / code=3 detail=<what()>` (E1/E2) | named post-conversion refusal (context / prepare-start) | capture `what()` verbatim; verdict ≥ MINIMAL-ACCEPT |
| F30 `state … failed 1 <what()>` (any armed-path throw) | the durable verdict row for every `[Seeds] REFUTED:` | route per the ladder; init-thread detail NEVER goes to bridge CONVERSION triage |
| `runtime_stop_failed` / `MRS_ERR_STOP_EXN` (S2) | owner stop reported failure | bounded reconcile, no replay |
| `Failed at <stage>. See UE4SS.log.` (mod spawn stages) | the mod's spawn path executed and FAILED — 16 error payloads across the 5 spawn stages (player / ground trace / load pickup class / spawn actor / configure pickup); mod never crashes the game (pcall-caught, busy resets, partial actor auto-destroyed) | read `<stage>` + payload against the stage table in `worker-post-popup-evidence/FAILURE-TAXONOMY.md` §C; GROUND-TRACE HIGHEST-RISK: `LineTraceSingle` out-param marshalling (`hit.ImpactPoint`/`.ImpactNormal`) is the historically highest-risk path — retry elsewhere / on flat ground before concluding an API gap |

## 6. RESTORE (only on controller reconciliation; never blind)
- **Old PRX .bak:** console `/data/plugins/modbridge_supplier_composition.prx.<TAG>.bak` (written at l10.2 BEFORE the swap) + local pre-image `<run-dir>/l10-composition.prx.pre` (readback-verified `d86887ec…` / 30,756,152 B). The flow's guard already restores ONCE byte-exact (proven by fresh RETR) on any failed verification — never a second attempt.
- **r11 fallback path:** re-upload the r11 fself from `worker-composition-5-rebuild/prx-candidate-composition5-1/` (RESTORE-PATH §A.5; r11 backup chain `0911af0c`) — deep fallback if the launch-10 rebuild must leave the comp6 slot entirely.
- **9-entry canonical ini:** preserved at `launch9-evidence/ploader-ini-9entry-restore.txt` (`fd9c7393…`, 440 B) — SUPERSEDED history; the current ini law is the 2-plugin `1216aab6…` config and the staging flow never wrote it; restore the 9-entry bytes only on explicit controller reconciliation, never as the default launch config.
- After any restore: re-verify settings v3 `d8faa4ec` + mod tree (main.lua 21415 B, UEHelpers 9962 B) unchanged.
