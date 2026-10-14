# EDIT-LOG — worker-session-docs-final-sync, 2026-10-04

Documentation-only sync of the session doc trilogy to the FINAL batch tool revision
(`worker-batch-final-ini-adoption/console_batch.py`, sha256 `b00b3a69ea5d7b56785cc2dd6eff00236058d2c3a42bd321ef827de7fa14e63a`,
selftest 372 ok / 0 failed; ownership chain ends `…worker-observer-staging-extension fd21d171… → b00b3a69…`).
Every edit below was applied IN PLACE to the controller-synced docs; as-found snapshots + unified diffs live in
`as-found/` and `diffs/` in this directory. No tool file was touched (tools FROZEN); no console contact.

Ground-truth facts were read from the tool source itself: `FLOWS` map (10 flows), `add_flow`/`add_parser` CLI
registrations, `FINAL_INI["registered_expected_final"] = 7`, the bootstrap branch (`ini-bootstrap-verdict.txt`,
`INI-BOOTSTRAPPED`), `stage-settings` freeze law (v2 inventory `9570ead3…`/`3afd10ce…`, v3 `d8faa4ec…`/`4d98ecd7…`),
`RIDEALONG_HANDOFF` (EXACTLY 7 `OK (<ms>): <file>` lines, ZERO FAIL), step ids `ride.0–ride.10`, `set.0–set.4`,
`ge.0–ge.10` (+ `ge.3b`, "seven evidence sinks").

## 1. RETURN-SESSION-MASTER-PLAN.md (in place at `worker-return-session-master-plan/`)

as-found sha256 `1fc69f32…` → synced sha256 `0ab2c2aa…` · diff: `diffs/RETURN-SESSION-MASTER-PLAN.patch`

| # | Location | Before | After | Why |
|---|---|---|---|---|
| M1 | Header | NEW-SETTINGS.md "(settings v2)" | "(settings v3 restage; v2 = recorded freeze/rollback inventory)" | The restage lands v3; v2 is only the freeze/rollback inventory (set.0/set.4) |
| M2 | After the Law line | — | DOC-SYNC note naming the final revision `b00b3a69…` (372/0) and this edit log | Provenance for controller-synced edits |
| M3 | Phase 2 preamble | "Run subcommands separately (observer + settings restage must land between staging and launch; `all` would reach the launch gate too early)" | "Run subcommands separately, in the tool's own session order (`all` = dll8 → e2e → stage-ridealongs → stage-settings → game-evidence and blocks at the arm-ploader gate, so one-session `all` is lawful…; never run game-evidence before stage-ridealongs + stage-settings are green)" | The old rationale is factually false against the final tool: `all` now INCLUDES both staging flows and stops at the interactive launch gate |
| M4 | Phase 2 step 3 | `stage-qdrv1` (append `2=/data/plugins/modbridge_qh_driver.prx`) | `stage-ridealongs` — backup, FINAL 8-entry ini law (ensure ops / **INI-BOOTSTRAPPED** one-shot whole-file STOR of the 400 B canonical `bbc34c16…`), end state exactly **7 registered**, then fresh pre-launch baselines | Definitive flow; INI-BOOTSTRAPPED is its own step-class in the tool (ride.2b) |
| M5 | Phase 2 step 4 | Manual observer ridealong per STAGE-PLAN-COMBINED §3–4 ("NOT yet in the tool") [PENDING stage-qdrv1 extension] | The FOUR PRX uploads under the §3 SHT-strip readback law (xwp 67832/`daea2ff4…`, fc1 67848/`7e2dce27…`, qdrv1 87112/`71406d3c…`, observer 30143976/`3f89a5b6…`; one re-STOR then abort) — inside the tool | Observer staging LANDED; step order matches the tool (ini adoption ride.2b–4 BEFORE PRX STORs ride.6–9) |
| M6 | Phase 2 step 5 | Manual settings curls; freeze-check "779 B `9f3fa127…` / 446 B `49ab7a3a…`" | `stage-settings` flow; freeze against the recorded v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…` (already-v3 = honest no-op); v3 upload/readback 1448 `d8faa4ec…` + 447 `4d98ecd7…` | Tool flow + correct freeze law (the 779 B pair is not what the tool freezes against) |
| M7 | Phase 3 step 7 | "fetches sinks (S, L, D1, C2)" | "fetches the seven evidence sinks (S, L, D1, C2 + observer log + the two probe verdict sinks)" + the `ge.3b` probe-verdict freshness gate (igd1-6ebab07f2111 / fc1pool1-78508a2e2940, `validated` 11/11, refusals = WARN) | ge.2 now says "seven evidence sinks"; ge.3b is a new gate class |
| M8 | Rollback section | "flip the two ridealong lines to `= 0`" | `python3 console_batch.py rollback-ridealongs` (probes/igf untouched, fresh `.bak` + byte-verified readback) | Named tool path |
| M9 | Rollback section | Settings rollback "old pair 779 B `9f3fa127…` + 446 B `49ab7a3a…`" | "the recorded v2 rollback pair 834 B `9570ead3…` + 446 B `3afd10ce…`" | Matches the tool's recorded freeze/rollback inventory |
| M10 | Time table | "stage-qdrv1+observer ~10–15 (30 MB upload) · settings restage ~5" | "stage-ridealongs (4 PRXs + probes) ~10–15 (~30 MB upload) · stage-settings ~5" | Flow names; timings remain the original host-side estimates (nothing new invented) |
| M11 | Prereq 2 | freshness gate "NOT applied [PENDING worker-evidence-freshness-fixes…]" | APPLIED (`92b56889…`), superseded in place by `b00b3a69…` (372/0) | Stale [PENDING] (the decision brief had already verified it applied) |
| M12 | Prereq 3 | 8fam receipts "EMPTY [PENDING worker-batch-runner-8fam-patch]" | LANDED + verified (162 selftest + 31 fake-drive checks), carried through `b00b3a69…` | Stale [PENDING] |
| M13 | Prereq 4 | "Observer staging in stage-qdrv1 [PENDING console_batch.py extension]" | LANDED as `stage-ridealongs` (definitive; legacy partial flows remain as documented fallbacks) | The extension exists |
| M14 | Command cross-check | subcommand list "plan/dll8/dll7/e2e/stage-qdrv1/game-evidence/all/selftest"; "observer curl … (manual) · settings curl … (manual)" | full set plan/dll8/dll7/e2e/stage-ridealongs/stage-observer/stage-qdrv1/stage-settings/rollback-ridealongs/game-evidence/all/selftest + flags, sha `b00b3a69…`, 372/0; observer+probe staging in-tool; settings restage in-tool | Match the final CLI exactly |

Left unchanged on purpose: Prereq 6 (patches.json promotion [PENDING worker-e2e-suite-completion]) — not a batch-tool
fact; EVE-STATE already supersedes it ("APPLIED"), flagged in REPORT.md. Phase-0/Phase-1 steps and the SESSION-EVE
UPDATE are controller-history, untouched.

## 2. CONTROLLER-DECISION-BRIEF.md (in place at `worker-controller-decision-brief/`)

as-found sha256 `285b8a8c…` → synced sha256 `2a35d322…` · diff: `diffs/CONTROLLER-DECISION-BRIEF.patch`

| # | Location | Before | After | Why |
|---|---|---|---|---|
| B1 | Header | — | DOC-SYNC note (final revision `b00b3a69…`, 372/0, sections updated, edit-log pointer) | Provenance |
| B2 | §0 law item 7 | "Run subcommands separately (dll8 → e2e → stage-qdrv1+observer → settings restage → game-evidence); NEVER `all` (it would reach the launch gate before the observer/settings ridealongs land)" | Tool's session order (dll8 → e2e → stage-ridealongs → stage-settings → game-evidence); `all` is lawful (blocks at the arm-ploader gate); separate runs keep a verdict check; never game-evidence before both staging flows are green | The NEVER-`all` reason is gone in the final tool |
| B3 | §0b freshness row | "`FRESH_SESSION_MS_THRESHOLD` at line 811 … file sha256 `92b56889…`" | same gate carried into `b00b3a69…` (line-number claim dropped) | Line numbers shifted in the final file; sha superseded in place |
| B4 | §0b observer row | "Observer staging inside stage-qdrv1 … STILL PENDING … Phase 2 step 4 remains MANUAL" | **LANDED (DEFINITIVE)** `stage-ridealongs` (4 PRXs + FINAL ini law, 372/0); legacy fallbacks noted | Landed |
| B5 | §0b settings row | "Settings package-v2 READY … Ready to restage" | v3 package (1448 `d8faa4ec…` + 447 `4d98ecd7…`); `stage-settings` freezes against v2 inventory 834 `9570ead3…` + 446 `3afd10ce…`; already-v3 = no-op | v3 supersedes v2 |
| B6 | §0b new row | — | FINAL-ini adoption row: terminal revision `b00b3a69…`, 10 flows + plan/selftest, 7 registered, INI-BOOTSTRAPPED, EXACTLY-7-OK-lines gate, 372/0 | The revision this doc is synced to |
| B7 | §3.2 branch table | "ADVANCE to stage-qdrv1 | `python3 console_batch.py stage-qdrv1`" | "ADVANCE to stage-ridealongs | `python3 console_batch.py stage-ridealongs`" | Order |
| B8 | §3.3 (whole section) | `stage-qdrv1` chain + table | `stage-ridealongs` DEFINITIVE chain (ride.0→ride.10) + table incl. the **INI-BOOTSTRAPPED verdict branch** (sanctioned one-time whole-file STOR of `ploader.ini.canonical` 400 B `bbc34c16…`, `.bak` + byte-exact RETR diff, `ini-bootstrap-verdict.txt` receipt, never a second attempt, EXPECTED branch on the Track-8 pre-image), ensure-only branch, `.bak`-FAIL row kept, AMBIGUOUS-ini STOP (duplicate/missing pre-existing/mode divergence/unknown line/unparseable/mode 3), PRX readback FAIL (one re-STOR); legacy `stage-qdrv1`/`stage-observer` paragraph | The task's new-verdict-class requirement |
| B9 | §3.4 (was manual observer §3.4) | manual SC-5 observer procedure with "count == 5 / simulate count ≠ 5" table | `stage-settings` flow chain (set.0→set.4) + table (freeze already-v3 no-op / divergence ABORT / readback lockstep law) | The old count == 5 rows contradicted the FINAL law (7 registered) and the manual path is superseded |
| B10 | §3.5 (was manual settings restage: freeze 779 B `9f3fa127…`, upload package-v2, readback 834 B) | manual v2 procedure | "Legacy manual paths (fallback only)" section + `rollback-ridealongs` pointer | The manual v2 upload targeted superseded bytes |
| B11 | §4.1 ge.2/ge.3 | "fetch five sinks S, L, D1, C2" | "fetch the seven evidence sinks — S, L, D1, C2, the igf facts jsonl, and the two probe verdict sinks"; new **ge.3b** bullet (fresh `igd1-6ebab07f2111` / `fc1pool1-78508a2e2940` records, `validated` 11/11, refusals = WARN, evidence-only) | ge.2 gate text in the tool says seven; ge.3b exists |
| B12 | §4.2 CP5 shape (a) | "restage PROOF (sha would leave `9f3fa127…`)" | expected live sha after v3 restage `d8faa4ec…`; v2 `9570ead3…` / pre-v2 `9f3fa127…` = restage did not land | The comparison anchor moved with the v3 restage |
| B13 | §5 timing row | "ploader injection of 5 plugins" | "of 7 plugins" + EXACTLY-7-OK/ZERO-FAIL expectation + `rollback-ridealongs` on FAIL | Count mismatch (7 registered) |
| B14 | §7 row 1 | subcommand list "plan/dll8/dll7/e2e/stage-qdrv1/game-evidence/all"; sha `92b56889…` | full flow set + plan/selftest + `--run-root`; `FLOWS` = 10 flows; STOP_ON_FAILURE default True with `--continue-on-failure` opt-out; sha `b00b3a69…` | Final CLI |
| B15 | §7 selftest row | "162 checks green per 8fam-patch receipts" | "**372 ok / 0 failed** per worker-batch-final-ini-adoption receipts (incl. fake console drive 90 green / 0 failed)" | New receipts |
| B16 | §7 settings row | "package-v2/* re-hashed 834 B / 446 B ✓" | "package-v3/* … ✓ staged by `stage-settings` (v2 = recorded freeze/rollback inventory)" | v3 |
| B17 | §7 new row | — | `stage-settings` flow row (freeze → v3 lockstep upload → byte-exact readback; already-v3 no-op) | New flow in table |
| B18 | §7 observer rows | "Observer staging inside console_batch.py | absent (grep clean) | ✗ manual §3.4 commands only" | "PRESENT — `stage-ridealongs` (DEFINITIVE…) | ✓ tool path (§3.3)"; SHT-strip row "✓ manual" → "enforced by `stage-ridealongs` (ride.1 host recompute); constants 87112/30143976/67832/67848 + all four body shas | ✓ in-tool" | Landed |
| B19 | §7 PENDING footer | "…observer staging tool extension; …" listed as PENDING | removed from the PENDING list with a DOC-SYNC note | It landed |

## 3. USER-LAUNCH-GUIDE.md (in place at `worker-user-launch-guide/`)

as-found sha256 `c1f4ce7e…` → synced sha256 `b1dd6d69…` · diff: `diffs/USER-LAUNCH-GUIDE.patch`
Plain-language register preserved (no tool jargon added).

| # | Location | Before | After | Why |
|---|---|---|---|---|
| U1 | §1 bullets | controller "arms the loader" directly; loader shows "a short message … then reports OK or FAIL" | new bullet: controller finishes ALL preparation (modules, loader list, settings files) BEFORE the game launches, none of it needs you; loader messages = "exactly seven 'OK' lines (one per module) and no 'FAIL' line" | Handoff moments: settings staging is pre-launch; post-launch gate is the 7-OK-line gate |
| U2 | §4 success | "If you saw the loader 'OK' and the menu opens" | "If you saw the loader's **seven OK lines** (and no FAIL line) and the menu opens" | 7-OK-line gate |
| U3 | §5 failure | "loader shows FAIL instead of OK" | "a FAIL line instead of (or among) the seven OK lines" | Same gate, failure side |
| U4 | §7 waiting | "about 1 minute for the loader's OK/FAIL message" | "about 1 minute for the loader's messages — the seven OK lines; any FAIL line is the trouble signal" | Same |
| U5 | "In short" footer | "controller arms the loader → … wait ~1 minute for 'OK'" | "controller stages everything (modules, loader list, settings) and arms the loader → … wait ~1 minute for the **seven OK lines**" | Both handoff moments in one line |

## Deliberately NOT edited

- `worker-console-batch-runner/USER-SESSION-GUIDE.md` (+ identical copy in `worker-batch-final-ini-adoption/`):
  already rewritten by the adopter for this exact revision (sha `49414a28…`); verified current, untouched.
- `worker-session-eve-coherence/EVE-STATE.md`: verify-only item — the first five commands
  (list_payloads curl → ploader.ini RETR curl → `comp6_one_pass.py` → `console_batch.py dll8` → `console_batch.py e2e`)
  all exist UNCHANGED in the final tool (no renames: dll8/e2e keep exact names and flags; comp6_one_pass.py is a
  controller-dir script the batch revision never touched). No edit required. Flag (not fixed, outside the trilogy):
  its "Then (not in the first five…)" paragraph still narrates `stage-qdrv1` + a separate observer ridealong +
  a separate FINAL-ini adoption, which the final tool folds into `stage-ridealongs`; its registered-count-7 and
  v3-settings lines already match.
- Master plan Prereq 6 (patches.json 51→54 [PENDING]) — not a batch-tool fact; EVE-STATE's quick-reference says
  APPLIED, so the controller should trust EVE-STATE there (its own standing rule). Flagged in REPORT.md.
