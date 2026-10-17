# CORRECTED-LINES — session-eve coherence sweep, 2026-10-04

Read-only verification. NO line below has been applied to another worker's file;
each is the confirmed-stale CURRENT text and the corrected replacement. Paths
relative to `outputs/2026-10-04-zcode-team/` unless noted. W = this team dir.

## (a) Stale tick cite `0x37ac8c0` -> `0x33ac8c0` — CONFIRMED in both docs

- **A1.** `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` line 52
  - CURRENT: ``   + Tick `0x37ac8c0`, mods-started, Item Spawner load banner; plus `post_launch_check.py` + `rung_check.py`. ``
  - FIXED: ``   + Tick `0x33ac8c0`, mods-started, Item Spawner load banner; plus `post_launch_check.py` + `rung_check.py`. ``
  - Expected console line (worker-tick-correction-propagation, INDEX-v2): `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`. Live tool already gates on `33ac8c0` (six occurrences, zero `37ac8c0`, in `console_batch.py`).

- **A2.** `worker-controller-decision-brief/CONTROLLER-DECISION-BRIEF.md` line 190 (CP2 row)
  - CURRENT: ``| CP2: no `GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)` equality | …``
  - FIXED: ``| CP2: no `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)` equality | …`` (row remainder unchanged)

## (b) Master plan stale v2-settings -> v3 — CONFIRMED (settings 1448 B `d8faa4ec…` / loadlist 447 B `4d98ecd7…`)

- **B1.** master plan line 4
  - CURRENT: ``· NEW-SETTINGS.md (settings v2) · MASTER-VERIFICATION-RUNBOOK.md (CP0–CP7) · CURRENT-STATE-DIGEST.md.``
  - FIXED: ``· NEW-SETTINGS.md (settings v2 — SUPERSEDED for this session by settings v3, W/worker-settings-v3-package/package-v3/) · MASTER-VERIFICATION-RUNBOOK.md (CP0–CP7) · CURRENT-STATE-DIGEST-v2.md.``

- **B2.** master plan lines 39–42 (Phase 2 step 5)
  - CURRENT: ``5. TOOL — settings+loadlist restage, NEW-SETTINGS §3 (manual; game CLOSED): freeze-check live pair = 779 B `9f3fa127…` / 446 B `49ab7a3a…` (abort on divergence) → `curl -sS -T W/worker-engineversion-ini/package-v2/ue4ss/UE4SS-settings.ini "$FTP/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"` and `-T package-v2/loadlist.txt "$FTP/data/modbridge/roots/PPSA33286/loadlist.txt"` → readback BOTH: 834 B `9570ead3…`, 446 B `3afd10ce…` (verified on disk 2026-10-04). …``
  - FIXED: ``5. TOOL — settings+loadlist restage, NEW-SETTINGS §3 shape with v3 bytes (manual; game CLOSED): freeze-check live pair = 779 B `9f3fa127…` / 446 B `49ab7a3a…` (abort on divergence) → `curl -sS -T W/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini "$FTP/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"` and `-T W/worker-settings-v3-package/package-v3/loadlist.txt "$FTP/data/modbridge/roots/PPSA33286/loadlist.txt"` → readback BOTH: 1448 B `d8faa4ec…`, 447 B `4d98ecd7…` (v3; re-hashed on disk 2026-10-04). …`` (Lockstep-law sentence unchanged.)

- **B3.** master plan line 85 (Prerequisite 5)
  - CURRENT: ``5. **Settings package-v2**: READY — 834 B `9570ead3…` + 446 B `3afd10ce…` re-hashed on disk 2026-10-04.``
  - FIXED: ``5. **Settings package-v3**: READY — 1448 B `d8faa4ec…` + 447 B `4d98ecd7…` (W/worker-settings-v3-package/package-v3/; re-hashed 2026-10-04; v2 834 B `9570ead3…` + 446 B `3afd10ce…` is superseded).``

- **B4.** master plan line 89 (command cross-check)
  - CURRENT: ``… · package-v2 bytes ✓ re-hashed · …``
  - FIXED: ``… · package-v3 bytes ✓ re-hashed (1448 B `d8faa4ec…` + 447 B `4d98ecd7…`) · …``

- **B5 (no change needed).** master plan line 68 rollback pair 779 B `9f3fa127…` + 446 B `49ab7a3a…` remains the correct restore target — it is the pre-v3 pair actually on the console root.

## (c) "registered exactly 5" gates -> exactly 7 — CONFIRMED (the 8-entry FINAL-INI-LAYOUT law: 8 entries, `modbridge_ingame_facts.prx = 0` is skipped at parse, so exactly 7 register)

- **C1.** master plan line 38
  - CURRENT: ``… → registered count exactly 5; STOR + RETR diff byte-exact.``
  - FIXED: ``… → registered count exactly 7 (FINAL-INI-LAYOUT 8-entry law; the `= 0` igf entry never registers); STOR + RETR diff byte-exact.``

- **C2.** master plan line 43
  - CURRENT: ``- GATE→Phase 3: all flows' verdicts green; ini registers exactly 5 plugins; readbacks byte-exact.``
  - FIXED: ``- GATE→Phase 3: all flows' verdicts green; ini registers exactly 7 plugins (8-entry FINAL-INI-LAYOUT, one `= 0` entry skipped); readbacks byte-exact.``

- **C3.** decision brief line 130
  - CURRENT: ``… → registered count **exactly 5** → STOR + RETR diff byte-exact.``
  - FIXED: ``… → registered count **exactly 7** (8-entry final ini law) → STOR + RETR diff byte-exact.``

- **C4.** decision brief line 147
  - CURRENT: ``| Both readbacks exact | **GATE→Phase 3**: all flows green; ini registers exactly 5; readbacks byte-exact | Phase 3 | — |``
  - FIXED: same row with ``ini registers exactly 7 (8-entry final ini law)``.

- **C5 (tool-side, PROPOSAL-ONLY — briefed, not applied).** `worker-console-batch-runner/console_batch.py` lines 2853, 2940, 2945, 2960 still carry the tool-only end-state ("exactly 5 registered", "6 entries", "EXACTLY 5 `OK (<ms>)` lines"); adopt the constants in `worker-final-ini-layout/CONSOLE-BATCH-CONSTANTS-BRIEF.md` (7 registered / 8 entries) before the session. `worker-game-session-ridealongs/simulate_ploader_ini.py` still hard-wires `VARIANTS` (line 143) and ignores its argv path — it cannot count a 7-registered end-state (desk-rehearsal correction 6 stands).

## (d) Items the INDEX-v2/digest worker flagged — CONFIRMED still present

- **D1.** master plan prerequisites §1–4/§6 (lines 81–86): PENDING markers superseded — freshness fixes LANDED (rung O2 gate in file), 8fam patch LANDED (dll8 code in `console_batch.py`), observer staging LANDED in-tool (worker-observer-staging-extension), patches.json promotion APPLIED. Prereq 1 comp6 is BUILT + GATE-APPROVED 13/13 (`76ee6af3…`); only its console staging remains, and that IS session-open command 3. Suggested marker per line: ``[LANDED 2026-10-04 — see INDEX-v2]``.
- **D2.** decision brief line 28 (its "stale item-6")
  - CURRENT: ``| patches.json 51→54 promotion | PREPARED, NOT APPLIED (PROMOTION-PREP.md) | Still NOT APPLIED (PROMOTION-PREP.md) | …``
  - FIXED: ``| patches.json 51→54 promotion | APPLIED — `252ad699`→`63c28979`, 3-line diff, G1 endpoint `43ea66c1` byte-exact (worker-e2e-promotion-apply) | Operative gate stays the tool's own 13/13 + payload preflight; do NOT re-apply | …``
- **D3.** decision brief line 278: the `[PENDING] branches at brief time:` list (exec-writability decision tree, observer staging tool extension, patches.json 51→54 apply) — all three landed; annotate ``[LANDED — see INDEX-v2]``. (comp6 required-slots SLOTS-FEASIBILITY stays as written: proposals landed, bypass not built into comp6.)

## (e) Writability-probe REPORT stale armed-artifact pin — CONFIRMED

- **E1.** `worker-writability-probe-build/REPORT.md` line 18
  - CURRENT: ``| `modbridge_exec_write_probe_armed.prx` | ARMED-empty-table staged-next artifact igd1-27618a58f062, 19277 B, sha256 `7b9e030c74b501a5aa578a533a4f52f32303284730ff15b3926f3c87d8489402` |``
  - FIXED: ``| `modbridge_exec_write_probe_armed.prx` | ARMED-empty-table staged-next artifact igd1-6ebab07f2111, 20013 B, sha256 `48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e` (post-D-1 identity; build-receipt-armed.json run1==run2==final_sha256; re-hashed on disk 2026-10-04 by session-eve sweep) |``
  - Verification: on-disk `modbridge_exec_write_probe_armed.prx` = 20013 B, sha256 `48949316…` (recomputed); `build-receipt-armed.json` `final_sha256` and both `determinism/run-armed-{1,2}` receipts agree; matches desk-rehearsal correction 5 (igd1-6ebab07f2111).

## (f) Desk-rehearsal fixes list — verification status of its 6 corrections

1. Brief CP2 row stale tick — STANDS (see A2).
2. Master plan Phase-3 item 7 stale tick — STANDS (see A1).
3. Registered-count 5 -> 7 — STANDS in all four doc gates (see C1–C4); tool gates too (C5).
4. cp56_verdict shape-a hint citing v2-era `9f3fa127…` — **RESOLVED SINCE THE REHEARSAL**: the hint in `console_batch.py` (`cp56_verdict`, def line 1612) now reads "check whether UE4SS-settings.ini was restaged" with no sha pin; `9f3fa127` appears nowhere in `post_launch_check.py`, `console_batch.py`, or `rung_check.py`.
5. Writability-probe REPORT stale pin — STANDS (see E1).
6. simulate_ploader_ini.py argv — STANDS (see C5).
