# RECONCILIATION.md — checkpoint vs 2026-10-04 zcode-team outputs

Base: `.superpowers/native-observations/transport-experiments-progress.json` (`updated_at` 2026-10-04T21:21:44Z, 104 `zcode_*` top-level entries) vs `outputs/2026-10-04-zcode-team/` (97 worker dirs at reconciliation time). The checkpoint file is read-only; the controller should apply the updates below. Evidence citations reference checkpoint keys or worker output dirs under `outputs/2026-10-04-zcode-team/`.

## A. Status updates to existing entries

1. **`zcode_worker_8_app_backend.status`**: `"IN_FLIGHT"` → `"SUPERSEDED_BY_REDO_DELIVERED_61_OF_61"`.
   Evidence: `zcode_worker_8_redo` (utc 18:53:06Z, `DELIVERED_61_OF_61_REAL_BINARY_TESTS`, same output dir `worker-8-app-backend/`). The 15:56Z in-flight record was never closed out.

2. **`zcode_worker_roster`** (frozen at 17:56:14Z: `workers_in_flight: 0`, 35-name delivered list): refresh to reconciliation time →
   - `utc` → reconciliation time;
   - `workers_in_flight: 0` → `8`; `in_flight: {}` → `["worker-c2-alt-correspondence", "worker-composition-4", "worker-rung3-chunk-probe", "worker-console-staging-bundle", "worker-dll-mod-acquire", "worker-fileio-apply", "worker-loadlist-final", "worker-mutex-sdk-retry"]` (all CLAIM.json-only at reconciliation time);
   - `delivered_and_recorded`: append every worker recorded after 17:56Z — mutex, mutex-fix-review, mutex-fix-2, mutex-deadlock (resolved), mutex-sdk-target, zip-to-mfb, e2e-records-ui, zip-bridge-console-link, ingame-log-parser, crt-startup-design, span-arbitration, query-harness-integration, f-r2-fix, e2e-console-staging, fileio-prep, session-audit, uninstall-fault-expansion, itemspawner-dryrun, worker-8-redo, prx-link-gaps, ack-integration-deep, layout-batch3, expected-bytes, predicate-compile, staged-pair-emission, risk-e2e, anchor-validation (+results), fname-fc1-materialize, fname-fc1-prep, integration-checklist, predicate-gc-fix, itemspawner-bundle, staged-pair-live, code-anchor-reads, real-input-pad, axes-detector, ue4ss-settings, layout-identity, batch2-live-prep, emit-final-pair, settings-bundle-merge, promotion-decision, igf5-diagnosis-rebuild, acceptance-evidence, query-s2-arm, post-promotion-runtime, composition-3, ingame-probe-v3 (v3 interim+final), argv-probe, e2e-final, composition-prep, e2e-console-dry, fname-runtime-probe (pending record, item 18).
   Evidence: all `zcode_*` entries with utc > 2026-10-04T17:56:14Z; per-dir CLAIM+REPORT presence.

3. **`zcode_ue4ss_critical_path`** (utc 16:03:41Z): status `"S2_MECHANISM_VERIFIED_DATA_GAPPED_S1_PROBE_DISPATCHED"` → `"S2_GUA_LIVE_VALIDATED_STAGED_PAIR_COMPLETE_C2_CODE_ANCHORS_BLOCKED"`.
   - `open[]`: S1 resolved (standalone halves: `zcode_loader_facts` 16:37Z; in-game halves: `zcode_ingame_v1_results`/`zcode_ingame_v2_results`/`zcode_v3_MAPPING_BREAKTHROUGH`). S2: GUA validated live — layout A, `0x97ba110`, 71959 objects, read+write (`zcode_anchor_validation_RESULTS` 19:59Z + `zcode_igf6_RESULTS.promotion_evidence_summary.gua`); FINAL staged pair ParsePair-verified, all identity real (`zcode_worker_emit_final_pair` 20:58Z). Remaining open item: C2 code anchors — byte-read from a ploader module blocked (leading hypothesis exec-only enforcement, **mechanism not localized** per user amendment in `zcode_igf6_RESULTS.accurate_recording`); C2 alternative = independent correspondence check, `worker-c2-alt-correspondence` dispatched (`zcode_igf6_RESULTS.c2_alternative` 21:17Z).
   - `in_flight_workers`: `["worker-8-app-backend","worker-mutex","worker-liveprobe"]` → the 8 in item 2.
   Evidence: cited checkpoint keys.

4. **`zcode_worker_ingame_probe.status`**: `"BUILT_AND_STAGED_AWAITING_GAME_LAUNCH"` (16:57Z) → `"SUPERSEDED_RUN_COMPLETED_V1"`.
   Evidence: `zcode_ingame_v1_results` (17:19Z, `IN_GAME_RUN_COMPLETED_7_RECORDS_NO_WRITE_FAILURES`).

5. **`zcode_worker_mutex_deadlock.status`**: `"ROOT_CAUSED_FIX_APPLIED_CONTROLLER_RERUN_PENDING"` (17:42Z) → `"RESOLVED_CONTROLLER_RERUN_PASSED"`.
   Evidence: `zcode_worker_mutex.integration_status` (19:09Z, `RESOLVED`, build 6 `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`); supplementary root cause `zcode_worker_mutex_fix_2` (18:53Z).

6. **`zcode_worker_prx_packaging.status`**: `"DELIVERED_SCRIPT_LINK_PENDING_ONE_RUN"` (17:06Z) → `"DELIVERED_LINK_COMPLETED_VIA_V3"`.
   Evidence: `zcode_v3_prx_interim` (18:37Z, deterministic PRX built) and `zcode_v3_final` (18:43Z, staged).

7. **`zcode_prx_packaging_fixes.status`**: `"link rerun 3 in background (candidate pending)"` (18:35Z) → `"RESOLVED_V3_PRX_INTERIM_BUILT"`.
   Evidence: `zcode_v3_prx_interim` (18:37:22Z, artifact sha256 prefix `1dfbf3d540575b04`).

8. **`zcode_console_unreachable`** (18:44Z): append `"resolved": true` + note. "USER ACTION NEEDED" is no longer pending — the console recovered the same evening and served igf4/igf5/igf6 dispatches.
   Evidence: `zcode_anchor_validation_RESULTS` (19:59Z) and `zcode_igf6_RESULTS` (21:13Z) both derived from live console records; first crash attributed to argv double-dispatch (this entry's own context + `zcode_worker_argv_probe`).

9. **`zcode_console_crash_2`** (20:43Z): append `"assessment_amended_by": "zcode_igf6_RESULTS.accurate_recording (21:17:12Z, user directive)"`. The "exec-only hardware enforcement" mechanism is a leading hypothesis, not a proven fact; igf6 interruption is NOT YET LOCALIZED; no broad investigation or repeated text-read attempts per user.

10. **`zcode_session_close_2026_10_04`** (17:56Z): add `"superseded_note": "Fork session continued in this same workspace after close (zcode entries 18:01Z→21:17Z); this entry is historical."` and update `agents_at_close`:
    - `worker-ingame-probe-v3` "STOPPED MID-BUILD" → RESOLVED: delivered + staged (`zcode_v3_prx_interim`, `zcode_v3_final`).
    - `worker-fileio-prep` "STOPPED MID-TASK" → RESOLVED: delivered 18:35Z (`zcode_worker_fileio_prep`, READY_TO_APPLY rebased diffs).
    - `worker-8-app-backend` "STATUS UNKNOWN" → RESOLVED: `zcode_worker_8_redo` DELIVERED_61_OF_61 (18:53Z).
    - `mutex-host-build` "KILLED at fork" → RESOLVED: rerun passed (`zcode_worker_mutex` 19:09Z, build 6).

11. **`zcode_worker_vaddr_crosscheck`** (18:13Z): append `"superseded_by": ["zcode_worker_span_arbitration (mechanism proven 18:24Z)", "zcode_v3_MAPPING_BREAKTHROUGH (19:15Z)"]; "re-run_not_required_unless_a_specific_fact_blocks"`. No re-run delivery exists; the arithmetic re-derivation it was re-queued for is moot given the later mapping evidence.

12. **Top-level `next_action`** (stale: "Refill model active at ~15 sustained… v3 (decisive) running. Mutex builder in driver phase under watchdog."): replace with current state, e.g.:
    > igf6 recorded and amended (entry/startup/mapscan clean; interruption before first exec-span read NOT localized — no broad investigation per user). C2 alternative correspondence check in flight (worker-c2-alt-correspondence). In flight: composition-4, rung3-chunk-probe, console-staging-bundle, dll-mod-acquire, fileio-apply, loadlist-final, mutex-sdk-retry. fname-runtime-probe delivered (record it). Next: record new deliveries, apply query-s2-arm one-shot integration patch, recompose via composition-4, then next console session per post-promotion-runtime RUNTIME-STAGING-PLAN and e2e-console-dry GO gates.
    Evidence: `zcode_igf6_RESULTS`, `zcode_worker_query_s2_arm`, `zcode_worker_post_promotion_runtime`, live worker-dir claims.

## B. Missing entries to ADD (delivered workers with no checkpoint entry)

13. **ADD `zcode_worker_argv_probe`**: `DELIVERED` — ARGV_ECHO.elf G1 probe (ploader GET /loadpayload argv forwarding); dispatch #1 OK, dispatch #2 30s timeout; temporally linked to the first console unavailability. Evidence: `worker-argv-probe/REPORT.md` + `zcode_console_unreachable`.
14. **ADD `zcode_worker_e2e_final`**: `DELIVERED_HOST_SIDE_ONLY` — E2E gap closure: prepare-archive→records durable wiring + download stage bound to exclusive curl-owner v1 interface (`mb_curl_runtime_start_v1`/`mb_curl_download_v1`/`mb_curl_runtime_close_v1`) + E2E-RUNBOOK-V2. Nothing ran on console. Evidence: `worker-e2e-final/REPORT.md`.
15. **ADD `zcode_worker_composition_prep`**: `DELIVERED` — composition preparation with vendored inputs under its dir; supersedes the stale "blocked re-runs (composition-prep…)" mention in the old `next_action`. Evidence: `worker-composition-prep/REPORT.md`.
16. **ADD `zcode_worker_e2e_console_dry`**: `DELIVERED` — console E2E execution plan (stages 0/1-owner/2a/2b/3/4-5/6 with D1-fixed merged-chain payload, staging re-pin, G1 config-file-driven entry design, verbatim transcripts). Evidence: `worker-e2e-console-dry/REPORT.md`.
17. **ADD `zcode_worker_fname_runtime_probe`**: `DELIVERED_PENDING_CONTROLLER_VERIFICATION` — FC-1 runtime verification probe add-on (mprotect-first technique) validating FName ctor candidate runtime `0x63d590` for the next probe iteration. REPORT.md appeared during reconciliation; verify artifact hashes before recording. Evidence: `worker-fname-runtime-probe/REPORT.md`.
18. **ADD in-flight registrations** (CLAIM only, no REPORT at reconciliation time): `worker-c2-alt-correspondence` (dispatch already noted inside `zcode_igf6_RESULTS.c2_alternative` — give it its own entry), `worker-composition-4`, `worker-rung3-chunk-probe`, `worker-console-staging-bundle`, `worker-dll-mod-acquire`, `worker-fileio-apply`, `worker-loadlist-final`, `worker-mutex-sdk-retry`.

## C. Contradictions noted (documented; no JSON change required)

19. Budget: `zcode_handoff.open_request_budget: 10` vs `zcode_flash_capacity_trial_1` "User-set steady state: 12 total incl. controller" — the later user directive supersedes the handoff budget; keep both as history.
20. Five review-style workers have `CLAIM.md`/no `CLAIM.json` (`worker-9-evidence-review`, `worker-initonce-review`, `worker-composition-review`, `worker-crt-startup-design`, `worker-ingame-log-parser`) — all five ARE recorded in checkpoint entries/roster; convention difference, not a gap.
21. `worker-launch-automator` (13:07Z) has only a bare CLAIM.json and never delivered; superseded in practice by the manual/ritual launch flow (igf runs). Optionally record as `ORPHANED_NEVER_DELIVERED`.
22. `zcode_v3_final` (18:43Z, "STAGED_ZERO_RECORDS_CAUSE_UNCONFIRMED") is historical — superseded by `zcode_v3_MAPPING_BREAKTHROUGH` (19:15Z, 1035 v3 records). Optional cross-pointer.
