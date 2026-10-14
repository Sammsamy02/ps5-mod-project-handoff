# REPORT — worker-project-state-index

**Deliverables (in order):** `CLAIM.json` → `INDEX.md` → `CURRENT-STATE-DIGEST.md` (64 lines, 3-min read) → this `REPORT.md`. All in
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-project-state-index/`.

## What was done

1. Read the ENTIRE ledger `.superpowers/native-observations/transport-experiments-progress.json` (665,517 B; 532 top-level keys; **all 201 `zcode_*` entries** read in full across 21 slices; non-zcode metadata read at summary depth). **The ledger was not modified.**
2. Opened late-session worker/controller dirs that carry state beyond the ledger: `worker-configured-roots-design`, `worker-comp6-drybuild` (DRYBUILD-RECEIPT + gate receipt), `worker-comp6-driver`, `worker-required-slots-feasibility`, `worker-game-session-ridealongs`, `worker-module-start-wiring` (r10–r12, entry r4), `outputs/2026-10-04-zcode-controller/comp6-integration/` (change-manifest.json, comp6_one_pass.py), `ZCODE-FORK-HANDOFF-V2.md`, `outputs/CURRENT-PS5-TEST/CURRENT-BUILD.md`.
3. Recomputed sha256 on disk for the claims below. No console contact; nothing executed beyond read-only hashing/JSON parsing.
4. Built INDEX.md (8 tracks, 7 pending console tests, 3 open questions + 4 minor tracked items) and the digest.

## 20-claim cross-check (honesty over polish)

Legend: **LEDGER** = quoted from the named `zcode_*` key; **DISK** = file opened / hash recomputed by this worker; **BOTH** = ledger claim confirmed on disk.

| # | Digest/INDEX claim | Source | Result |
|---|---|---|---|
| 1 | Ledger: 532 top-level keys, 201 `zcode_*`, frozen `2026-10-05T02:31:14Z` (`zcode_fresh_r11_gate_pair` last) | DISK (json parse) | PASS — exact |
| 2 | Staged build is composition-5 **r11**, fself `0911af0c…`, both gates 8/8 GO | BOTH — LEDGER `fresh_r11_gate_pair` ("staged fself 0911af0c… confirmed current in both candidates"); DISK sha256 of `W/worker-composition-5-rebuild/prx-candidate-composition5-1/modbridge_supplier_composition.prx` = `0911af0cd5b4a1f2…` | PASS |
| 3 | GUA validated layout A at 0x97ba110, 71,959 objects | LEDGER `anchor_validation_RESULTS` (decoded NumElements 71959, writeback success) + `worker_predicate_gc_fix` (replay: "VALIDATED, layout A … num=71959") | PASS |
| 4 | C2 alternative PASS: B/C/D/E match, A honestly refused, base 0x400000 | LEDGER `c2_alternative_RESULT` (B_L_UE5/C/D/E MATCH, A REFUSED not-in-span; base_check matches_expected true) | PASS |
| 5 | Promotion scope = "FUObjectArray + TUObjectArray subset (rung 1→2)"; Tick/SCO unresolved | LEDGER `PROMOTION_SIGNED` (scope + tick_sco_status fields) | PASS |
| 6 | Identity `5.6.1-0+++UE5` / build-id `04bb4330a7d71dcefe1cc2c4d19769d5` from retained image | LEDGER `worker_layout_identity` (identity_values; engine 5.6.1 is a NEW fact) | PASS |
| 7 | Axes SHIPPING (margin 60) + STATS OFF (margin 12) from 244-marker cost model | LEDGER `worker_axes_detector` major_finding | PASS |
| 6' | (fold) InitOnce/Mutex/FileIO/CRT target-proven; 6-family combined build 133c5e9e passed with TEN family keys | LEDGER `dll_family_summary` + `allfam_dll_console_run` | PASS |
| 8 | Launch 8: conversion+context PASSED first time; lease "unregistered bytes"; root cause extent clamp 0xfc602c8 not page-aligned; r11 page-quantized | LEDGER `composition5_launch8_result` | PASS |
| 9 | Launch 5: anchor_E_covered=1, anchor_E_match=1, lmf_identity_source=2 VmScan (identity first established) | LEDGER `composition5_launch5_result` | PASS |
| 10 | **Launch 6 has no dedicated ledger entry**; "launch-6 proved" STAT enforcement | DISK+LEDGER: grep of all 201 keys finds "launch-6" only inside `worker_deliveries_batch2` and worker-configured-roots-design/REPORT.md citations | CONFIRMED GAP (recorded as gap, not smoothed over) |
| 11 | sdg2 DIAG line `DIAG: module_start reached … build=sdg2-6f404cee6b94 variant=imp` proves all 196 composition-4 imports bind | LEDGER `sdg2_target_run` (line + proof incl. dynsym-oracle equality) | PASS |
| 12 | comp6 D2-only dry build GO: wrapped PRX `35ba028e…`, object `ef19a4b8…`, run1==run2 bit-identical, gate 8/8 | BOTH — DISK `W/worker-comp6-drybuild/DRYBUILD-RECEIPT.md` + `gate-receipt-composition6-dry1.json` (verdict "GO (offline gate)", failed_checks []); sha256 of `prx-candidate-composition6-dry1-a/modbridge_supplier_composition.prx` = `35ba028e9dd3ed91…` | PASS |
| 13 | comp6 change manifest: baseline composition5-r11; bridge r12 `feed5254`, entry r4 `c97055f4`; expected import delta empty | BOTH — DISK `C/comp6-integration/change-manifest.json`; sha256 `W/worker-module-start-wiring/runtime_start_bridge_r12.cpp` = `feed525440233603…`, `runtime_module_entry_r4.c` = `c97055f40f5a6dd9…` | PASS |
| 14 | Item Spawner main.lua 746 lines, `f586043a`, 21,415 B, byte-identical intake | BOTH — LEDGER `UE4SS_STAGING_COMPLETE`; DISK `wc -l` = 746 (both bundle copies), sha256 prefix `f586043a` | PASS |
| 15 | Corrected UE4SS-settings.ini = 779 B, `9f3fa127`, all gates pass | BOTH — LEDGER `UE4SS_STAGING_COMPLETE` + `worker_settings_bundle_merge` (221→779 fix); DISK `wc -c` = 779, sha256 prefix `9f3fa127` | PASS |
| 16 | qdrv1 `52c32216…` 26,157 B; observer `1afa0997…` 30,152,151 B; ploader MAX_PLUGINS=10, 5 registered, ≥4 spare | BOTH — DISK sha256 of `W/worker-record-sink-addon/modbridge_qh_driver.prx` = `52c3221665a423dd…`, `W/worker-observer-packaging/modbridge_runtime_aligned_observer.prx` = `1afa0997682cc6ec…`; LEDGER `worker_ridealongs_combined` + ridealongs CLAIM limits (pinned ploader main.c) | PASS |
| 17 | igf5 probe → console+game crash, zero records; igf6 → module died at first exec-span read, game survived. (Digest says "crashed the module") | LEDGER `console_crash_2` + `igf6_RESULTS` — with the recorded user amendment: the exec-only-hardware-enforcement *mechanism* is a "leading hypothesis, not a proven fact"; what is confirmed is 3-of-8 records then stop, game alive. Digest wording kept at observed-fact level | PASS (wording deliberately conservative) |
| 18 | ploader.ini pre-image: 4-entry layout (aligned_entry=2, ingame_facts=0, composition=2, path_test=2), sha `8d331b3f…` | LEDGER `ploader_ini_preimage`; DISK `C/console/ploader-ini-preimage.txt` exists. **Qualification:** the ledger `content` field is a human description (its own sha differs); `8d331b3f` is the recorded sha of the console-side pre-image file, not recomputable from the ledger alone | PASS with qualification |
| 19 | required-slots question OPEN: worker in flight (anchors measured, no REPORT); comp6 manifest warns "honest stops possible at PS-scan" | DISK `W/worker-required-slots-feasibility/` contains only `anchor-measurement.json` + `offline_anchor_scan.py`; LEDGER silent (post-freeze); change-manifest expected_next_launch quote | PASS |
| 20 | Concurrency: user final = 12 total incl. controller; 30-simultaneous trial hit ≥5 hard 1302 failures (10 final) | LEDGER `flash_capacity_trial_1` (user_decision_final; final_tally_interim failed_1302: 10) | PASS |

Additional spot-verification beyond the 20 (not counted): 7 gate receipts exist in `W/worker-composition5-gate/`; E2E suite 348/348 + "promotion prepared NOT applied" (`worker_e2e_suite_completion`); 7fam plan `83ee2ae9`/334,656 B (`worker_7fam_stage_prep`, ledger-only — not recomputed); argv crash + ≥30 s rule (`console_unreachable`, `worker_console_safety`).

## Gaps and corrections found during synthesis

- **Concurrent ledger growth (honesty note):** the ledger was 665,517 B / 201 `zcode_*` keys when read; it grew to 667,184 B / 204 keys DURING synthesis — three post-freeze entries landed 02:35–02:36Z: `worker_app_backend_integration` (DELIVERED 42/42 host-proven; supersedes this index's "dispatched, not yet in ledger" row), `worker_8fam_console_prep` (8-family DLL plan preferred over 7fam; memory.last.length oracle correction 114688/163840), `comp6_drybuild` (the drybuild receipt now ledgered: PIPELINE_PROVEN_GATE_GO_8_8_DETERMINISTIC). INDEX.md rows were updated to incorporate them; the 20-claim table below was verified against the 201-entry snapshot and is unaffected except where noted.
- **Launch-6 ledger gap** (claim 10): evidenced only transitively. INDEX/digest record it as a gap.
- **Post-freeze state**: `zcode_fresh_r11_gate_pair` (02:31Z) is the last entry; comp6 scaffold/drybuild/one-pass runner exist only as on-disk artifacts (some referenced by mirror ledgers under worker dirs, deliberately not merged into the real ledger). INDEX tracks them as "on disk".
- **Empty worker dir**: `worker-fname-ctor-audit/` exists with zero files — treated as not-delivered.
- **Ledger `content` vs sha mismatch risk**: documented in claim 18 to prevent a future session from hashing the description string and calling the pre-image corrupt.
- Two descriptive labels were kept conservative on purpose: igf6 crash *mechanism* (amended to hypothesis in the ledger itself) and "PS5 exec-only pages refuse reads" (stated as observed behavior of two probes, not kernel fact).

## Numbers

- Ledger entries read: 201/201 `zcode_*` + 331 non-zcode keys summarized.
- Worker dirs on disk: 198 under `outputs/2026-10-04-zcode-team/`.
- Disk hashes recomputed: 9 files; line/byte counts: 3 files; JSON parses: 3.
- Console contact: 0. Ledger writes: 0. Nested subagents: 0.
- Digest length: 64 lines (target ~60; kept 4 extra to preserve the honest-outcome matrix and safety ritual).
