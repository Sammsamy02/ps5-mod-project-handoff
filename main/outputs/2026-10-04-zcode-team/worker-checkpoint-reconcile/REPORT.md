# REPORT.md — worker-checkpoint-reconcile (2026-10-04)

Task: reconcile `.superpowers/native-observations/transport-experiments-progress.json` with `outputs/2026-10-04-zcode-team/`. Checkpoint treated as read-only; deliverables are `RECONCILIATION.md` (22 numbered controller actions: 12 status updates, 6 entries to add, 4 documented contradictions) and `CURRENT-STATE.md` (per-track snapshot). Writes confined to `outputs/2026-10-04-zcode-team/worker-checkpoint-reconcile/`. ~13 tool calls of ≤40 budget; no network, no console, no nested subagents.

## 1. Checkpoint zcode_* inventory (step 1)

104 `zcode_*` top-level keys (plus `zcode_handoff`/`zcode_takeover_2026_10_04` bookends and controller-run result entries). The 42-entry `experiments` list is the older Codex-era experiment log and contains no zcode entries; all zcode state is top-level. Statuses span `DELIVERED_*` (majority), controller integration records (`INTEGRATED_HOST_AND_TARGET_PASSED`, `COMPOSED_*`), in-game run results (`zcode_ingame_v1/v2_results`, `zcode_v3_MAPPING_BREAKTHROUGH`, `zcode_anchor_validation_RESULTS`, `zcode_igf6_RESULTS`), and event records (`zcode_console_unreachable`, `zcode_console_crash_2`). Stale entries found: `zcode_worker_8_app_backend` (IN_FLIGHT, superseded by redo), `zcode_worker_ingame_probe` (awaiting launch, run since completed), `zcode_worker_mutex_deadlock` (rerun pending, rerun passed), `zcode_worker_prx_packaging` + `zcode_prx_packaging_fixes` (link pending, v3 built), `zcode_worker_roster` (17:56Z, 0 in flight), `zcode_ue4ss_critical_path` (16:03Z, S2 "data-gapped" — since live-validated), `next_action` (pre-fork queue text), `zcode_session_close_2026_10_04` agents_at_close (all four UNKNOWN/STOPPED/KILLED since resolved), `zcode_console_unreachable` (action-needed, resolved).

## 2. Worker-dir inventory (step 2)

97 worker dirs at reconciliation time (92 at first listing; 5 new claims appeared mid-reconciliation — the session is live):
- **83 CLAIM.json + REPORT.md** (fully delivered).
- **5 REPORT.md without CLAIM.json** — CLAIM.md convention workers, all recorded in checkpoint: worker-9-evidence-review, worker-initonce-review, worker-composition-review, worker-crt-startup-design, worker-ingame-log-parser.
- **9 CLAIM.json without REPORT.md** (in flight or orphan): c2-alt-correspondence, composition-4, rung3-chunk-probe, console-staging-bundle, dll-mod-acquire, fileio-apply, loadlist-final, mutex-sdk-retry (all fresh, in flight) + launch-automator (13:07Z orphan, never delivered).

## 3. Findings (step 3)

- **(a) Stale statuses**: 9 entries listed above; full old→new values with citations in RECONCILIATION.md items 1–12.
- **(b) Delivered workers missing checkpoint entries**: worker-argv-probe (covered only indirectly via `zcode_console_unreachable`), worker-e2e-final, worker-composition-prep (also stale-named in old `next_action` "blocked re-runs"), worker-e2e-console-dry, worker-fname-runtime-probe (REPORT landed during reconciliation), plus 8 in-flight registrations lacking their own entries (items 13–18).
- **(c) Contradictions**: session-close recorded at 17:56Z while the same workspace continued to 21:17Z+; `zcode_igf6_RESULTS` self-amends its own exec-enforcement claim per user directive (handled via cross-pointer, item 9); handoff budget 10 vs trial steady-state 12 (later user directive wins); igf6 `promotion_evidence_summary` still asserts code-anchor conclusions the `accurate_recording` amendment retracts — controller should treat `accurate_recording` as authoritative (noted in items 9/11).
- **(d) Most current per-track status**: see CURRENT-STATE.md — critical path blocked only on C2 code anchors (correspondence check in flight; igf6 interruption not localized per user); DLL host-green with SDK retry in flight; installer chain closed through F1/F-R1/F-R2/D1 with staging plans ready (console contact gated); app backend delivered 61/61 (with a provenance caveat on the UI audit); container facts settled (utoc header spec; 4699 vs 4544 resolved).

## 4. Deliverables
- `CLAIM.json` (this dir, written first)
- `RECONCILIATION.md` — numbered exact JSON updates for the controller to apply
- `CURRENT-STATE.md` — one-page track snapshot
- `REPORT.md` (this file, written last)

Caveats: checkpoint is a moving target (live session); reconciliation reflects checkpoint state at `updated_at` 21:21:44Z plus outputs observed ~21:30Z. Worker-fname-runtime-probe's REPORT.md appeared between two inventory reads — verify hashes before recording it as delivered (RECONCILIATION.md item 17).
