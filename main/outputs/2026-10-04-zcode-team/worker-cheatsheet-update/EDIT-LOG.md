# EDIT-LOG — RETURN-SESSION-COMMANDS.md (controller-owned file, edited in place by worker-cheatsheet-update, 2026-10-04)

File: outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md
Basis: prior rev (as-found, 35 lines) + the post-write tool changes. New rev tagged `2026-10-04b` in the H1.

## Diffs applied

1. **Header**: added the exact CLI surface (flow list + `--dry-run/--assume-yes/--dll {7,8}`/`plan --only`/`--run-root`/`--continue-on-failure`) and the archive-scaffold one-liner; pinned manager port 8084 (never 9223) and anonymous FTP `ps5-host-not-configured.invalid:2121`; r11 sha `0911af0c` named. Was: bare prose.
2. **Phase 0**: `ftp://...` ellipsis replaced with the tool's literal FTP base; comp6 fself bytes (30,773,943 B) added beside sha `76ee6af3…` (manifest match). Pre-image gate `8d331b3f…` 247 B kept (still pinned by worker-index-final-edition/INDEX-FINAL.md console-state + master plan Phase 0). Added a one-line session_run archive note.
3. **Phase 1 (was stale)**:
   - `all` order now `dll8 -> e2e -> stage-ridealongs -> stage-settings -> game-evidence` (tool `add_flow("all")` help). The old text's `stage-ridealongs[4 PRXs + INI-BOOTSTRAPPED]` became **5 PRXs** (xwp-armed + fc1pool + qdrv1 + observer + **cc1-carrier**, SHT-strip readback law each).
   - **cc1 staging lives INSIDE stage-ridealongs** (adopted law; tool help: "supersedes stage-qdrv1/stage-observer inside `all`") — not a separate step.
   - Ini law: FINAL **9-entry ploader.ini, 8 registered** (mode-0 igf consumes no slot, cc1 last); INI-BOOTSTRAPPED = the ONE sanctioned whole-file STOR only when historical line order requires it, else content-anchored ensure/noop (final_ini_plan, FINAL-INI-LAYOUT §3.3).
   - stage-settings spelled out: v3 lockstep pair settings 1448 B `d8faa4ec…` + loadlist 447 B `4d98ecd7…`, byte-exact readback both.
   - Added the settled **dll8=37–40** rule (RECONCILED: 41–44 only if the dll7 fallback ran first) on the `--dll 7` line.
   - Added a one-line session_run note (one run dir per flow).
4. **Phase 2**:
   - Post-launch gate is now the **8-registered SC-5b gate**: EXACTLY 8 `OK (<ms>): <file>` lines, ZERO FAIL (was "injecting N plugins"); FAIL -> STOP + `console_batch.py rollback-ridealongs` (tool handoff wording).
   - Evidence set widened **CP0-CP4 -> CP0-CP7** + probe verdict sinks (xwp/fc1pool/cc1 census jsonl) per the game-evidence flow.
   - Records line now includes the **cc1 census** (/data/modbridge-carrier-facts.jsonl, ~2–3 min, 30 polls x 4 s).
   - **Corrected tick constant** pinned on the CP2 evidence cite: `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`; 0x37AC8C0 named as the base-convention error (worker-ue4ss-docs-fidelity/FIDELITY-REPORT.md §1; runner :394/:3872).
   - Added a one-line session_run note (--flow game-launch9).
5. **Phase 3**: carrier-facts added to the read list; **LAUNCH10-MATRIX §4** clarified as "read FIRST" (it is the pre-conditions checklist). Armed-candidate line kept and firmed: `prx-candidate-composition6-l10-a`, fself `d8cb4a76…` 30,818,247 B, offline gate **15/15 GO** (gate-receipt-launch10-armed-a-vs-b.json). Added a one-line session_run note (--flow game-launch10).
6. **Rollbacks**: ini line corrected to the tool behavior — `rollback-ridealongs` performs exactly **two** single-character **2->0** flips (qdrv1 + observer; the cc1 carrier is not part of that flow), rollback-runbook §4.3 for reconcile variants; settings v2 pair sized (`9570ead3…` 834 B) per the freeze inventory; r11 backup path unchanged (verified present).

## Verification against the final tool
Every command/name/flag in the sheet was checked against `worker-console-batch-runner/console_batch.py`
(subcommand table + argparse, lines ~4986–5058 and per-flow definitions) and
`worker-session-archive-scaffold/session_run.sh` usage (README). Constants cross-checked: HTTP/FTP bases (:97–98),
canonical ini 440 B `fd9c7393…` (:329–330), CP2 tick line (:394), SC-5b 8-OK gate (RIDEALONG_HANDOFF, :2770–2780),
registered_expected_final=8 (:327), dll8 37–40 (RECONCILED.md), candidate gate 15/15 (launch10-prebuild REPORT.md).

One page: 49 short physical lines (the prior rev was 35 very long lines; rendered footprint is comparable — single page).
