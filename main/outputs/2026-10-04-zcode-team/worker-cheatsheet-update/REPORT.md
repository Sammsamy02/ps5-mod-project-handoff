# REPORT — worker-cheatsheet-update (2026-10-04)

## What was done
Rewrote `outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md` in place (rev `2026-10-04b`, 40 lines — one page), syncing it to the current tool. Edit recorded in `EDIT-LOG.md`; claims in `CLAIM.json` (written first). No console contact; documentation-only.

## The four requested content updates — all landed
1. **Phase 1 `all` order + cc1 staging**: sheet now shows the tool's actual order `dll8 -> e2e -> stage-ridealongs -> stage-settings -> game-evidence`, with cc1 staged **inside stage-ridealongs** (5 PRXs total: xwp-armed, fc1pool, qdrv1, observer, cc1-carrier) — verified against `console_batch.py`'s `add_flow` help, which says stage-ridealongs "supersedes stage-qdrv1/stage-observer inside `all`"; it is NOT a separate step. Ini law stated as the FINAL 9-entry/8-registered layout with the ONE sanctioned INI-BOOTSTRAPPED whole-file STOR only when line order requires it (FINAL-INI-LAYOUT §3.3 / `final_ini_plan`).
2. **8-registered post-launch gate**: Phase 2 now states the SC-5b gate — EXACTLY 8 `OK (<ms>): <file>` lines, ZERO FAIL, FAIL -> `rollback-ridealongs` — replacing the stale "injecting N plugins" phrasing.
3. **Corrected tick constant**: the CP2 evidence cite pins `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`; 0x37AC8C0 is named as the base-convention error (fidelity report; runner :394/:3872).
4. **Armed-candidate line**: kept and firmed — `worker-launch10-prebuild/prx-candidate-composition6-l10-a`, fself `d8cb4a76…` (30,818,247 B), offline gate 15/15 GO; LAUNCH10-MATRIX **§4** is the read-first pre-conditions checklist (verified: §4 is that checklist).

Also folded in the settled **dll8=37–40** JSONL append rule (41–44 only after a dll7 fallback) and widened Phase-2 evidence to CP0-CP7 + the cc1 census sink (carrier-facts, ~2–3 min window).

## Command verification (task 2)
Every command in the sheet was checked against the final tool's argparse/subcommand table (flows `plan dll8 dll7 e2e stage-ridealongs stage-observer stage-qdrv1 stage-settings rollback-ridealongs game-evidence all selftest`; `--dry-run/--assume-yes/--dll {7,8}`, `plan --only`, `--run-root`, `--continue-on-failure`) and literal constants (HTTP 8084/FTP 2121, canonical ini 440 B `fd9c7393…`, `registered_expected_final=8`, rollback = exactly two single-char 2->0 flips on qdrv1+observer). The old `ftp://.../` ellipsis became the tool's literal base. The archive scaffold (`worker-session-archive-scaffold/session_run.sh`: start/note/capture/verdict/close/checklist) is noted with one line at each evidence step (Phase 0 staging, Phase 1 per-flow run dirs, Phase 2 game-launch9, Phase 3 game-launch10), using its exact flags.

## Files
- Edited: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md` (in place, controller-owned — edit logged)
- Created in `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-cheatsheet-update/`: `CLAIM.json`, `EDIT-LOG.md`, `REPORT.md`

## Honesty bounds
Console-state facts (down at close, r11 slot, pre-image `8d331b3f…` 247 B) are quoted from INDEX-FINAL/manifest as-found records, not re-verified live. The sheet remains a session plan; nothing in it asserts a completed run or engine acceptance.
