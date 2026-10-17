# Ledger Session-Eve Index — frozen TOC of all `zcode_*` entries

- Frozen copy: `ledger-session-eve.json` (same directory)
- Snapshot sha256: `5a31de8fc2b39e887b1969bc3231223319daa3c22e717fdcb89429381d804cad`
- Freeze utc: 2026-10-05T08:00:38.934285+00:00
- Ledger source: `.superpowers/native-observations/transport-experiments-progress.json`
- Entries: 280 `zcode_*` | utc span: 2026-10-04T15:29:51.005185+00:00 .. 2026-10-05T07:57:06.857311+00:00

Track rule (first match wins; `zcode_*` keys not starting with `worker_` are controller milestones):
sessions-governance, diagnostics, runtime-build, mod-pipeline, layout-memory, gameplay-verification, worker-general

Line format: `key` — utc — status (truncated 120 chars) — serialized bytes.

| Track | Entries |
|---|---|
| [milestones-controller](#milestones-controller) | 73 |
| [sessions-governance](#sessions-governance) | 36 |
| [diagnostics](#diagnostics) | 12 |
| [runtime-build](#runtime-build) | 57 |
| [mod-pipeline](#mod-pipeline) | 33 |
| [layout-memory](#layout-memory) | 33 |
| [gameplay-verification](#gameplay-verification) | 10 |
| [worker-general](#worker-general) | 26 |

## milestones-controller

- `zcode_PROMOTION_SIGNED` — 2026-10-04T22:19:59.841586+00:00 — LAYOUT_FACTS_PROMOTED_RUNG_1_2_SCOPE — 858 B
- `zcode_UE4SS_STAGING_COMPLETE` — 2026-10-04T22:26:30.521473+00:00 — ALL_FILES_STAGED_VERIFIED_PLOADER_ARMED — 950 B
- `zcode_acceptance_ladder` — 2026-10-04T16:18:43.450616+00:00 — (no status field; keys: utc,source,ladder,fname_rule,dll_rule,installer_rule) — 1184 B
- `zcode_allfam_dll_console_run` — 2026-10-04T23:26:52.478889+00:00 — PASSED_ALL_SIX_FAMILIES_FIRST_TARGET_RUN — 963 B
- `zcode_anchor_validation_RESULTS` — 2026-10-04T19:59:58.334008+00:00 — GUA_VALID_DATA_PREDICATE_HAS_GC_ORDER_BUG_CODE_ANCHORS_EXEC_ONLY — 3636 B
- `zcode_bridge_r2_revision` — 2026-10-04T23:52:43.226671+00:00 — R2_CREATED_SYNTAX_CLEAN_BUILD_RUNNING — 1511 B
- `zcode_bridge_r8_user_corrections` — 2026-10-05T01:03:14.236922+00:00 — R8_BUILT_CYCLE_RUNNING — 1077 B
- `zcode_c2_alternative_RESULT` — 2026-10-04T22:19:59.841586+00:00 — C2_ALTERNATIVE_PASS_PROMOTION_GATE_OPEN — 1681 B
- `zcode_comp6_armed` — 2026-10-05T02:43:55.836443+00:00 — VERIFY_ONLY_PASS_REAL_SOURCES_LANDED — 1057 B
- `zcode_comp6_build_result` — 2026-10-05T04:46:06.579133+00:00 — BUILT_GATE_12_OF_13_D1_DETERMINISM_OPEN — 1522 B
- `zcode_comp6_drybuild` — 2026-10-05T02:36:11.867748+00:00 — PIPELINE_PROVEN_GATE_GO_8_8_DETERMINISTIC — 399 B
- `zcode_comp6_gate_final_go` — 2026-10-05T04:55:25.549799+00:00 — GATE_13_OF_13_GO_TRIPLE_DETERMINISM_PROVEN — 662 B
- `zcode_comp6_preflight_applied` — 2026-10-05T03:00:17.069581+00:00 — GO_WITH_FIXES_ALL_APPLIED — 1012 B
- `zcode_comp6_staging_state` — 2026-10-05T04:56:32.151787+00:00 — CONSOLE_UNREACHABLE_STAGING_PENDING — 541 B
- `zcode_composition5_launch1_result` — 2026-10-05T00:11:54.912158+00:00 — RUNTIME_STARTED_ACTIVATION_PASSED_CONVERSION_REFUSED_NoImageAnchor — 2063 B
- `zcode_composition5_launch2_result` — 2026-10-05T00:21:17.226190+00:00 — VMENTRY_ENOMEM_ROOT_CAUSE_r4_FIX_BUILT — 1255 B
- `zcode_composition5_launch3_result` — 2026-10-05T00:29:31.513862+00:00 — VMMAP_FIXED_STRIDE_PARSE_ROOT_CAUSE_r5_FIX_BUILT — 1242 B
- `zcode_composition5_launch4_result` — 2026-10-05T00:37:50.434775+00:00 — VMMAP_WALK_WORKS_WINDOW_VERIFY_FAILED_r6_BUILT — 1742 B
- `zcode_composition5_launch5_result` — 2026-10-05T01:00:53.895100+00:00 — ANCHOR_VERIFIED_NEW_BLOCKER_game_executable_path_r7_BUILT — 1920 B
- `zcode_composition5_launch6_result` — 2026-10-04T23:30:00+00:00 — RETROACTIVE_DEDICATED_ENTRY — 568 B
- `zcode_composition5_launch7_result` — 2026-10-05T01:33:52.457919+00:00 — PATH_LADDER_REFUSED_HONESTLY_DISCOVERY_DIAGNOSTICS_PARTIAL_r9_BUILT — 1157 B
- `zcode_composition5_launch8_result` — 2026-10-05T01:46:17.112322+00:00 — PATH_GATE_PASSED_LEASE_GATE_HIT_kve_EMPTY_ResolutionB_only_r11_BUILT — 1508 B
- `zcode_composition5_r3_staged` — 2026-10-05T00:12:49.086369+00:00 — GATE_GO_STAGED_VERIFIED_AWAITING_LAUNCH_2 — 890 B
- `zcode_composition5_r4_staged` — 2026-10-05T00:22:35.314542+00:00 — GATE_GO_STAGED_VERIFIED_AWAITING_LAUNCH_3 — 563 B
- `zcode_composition5_r5_staged` — 2026-10-05T00:32:08.183047+00:00 — GATE_GO_STAGED_VERIFIED_AWAITING_LAUNCH_4 — 382 B
- `zcode_composition_5_gate_go_staged` — 2026-10-04T23:57:33.262182+00:00 — OFFLINE_GATE_GO_STAGED_VERIFIED_AWAITING_LAUNCH — 2122 B
- `zcode_composition_startup_diagnosis` — 2026-10-04T22:40:38.408422+00:00 — NO_OUTPUT_STARTUP_STAGE_UNRESOLVED — 4382 B
- `zcode_console_crash_2` — 2026-10-04T20:43:17.176144+00:00 — This is the SECOND console crash. The inline syscall approach may be riskier than assessed. If the igf5 probe ran and... — 1713 B
- `zcode_console_unreachable` — 2026-10-04T18:44:27.034337+00:00 — (no status field; keys: utc,event,argv_probe_status,confirmed_by_user,cause,timeline) — 1951 B
- `zcode_controller_cycle_record` — 2026-10-05T01:49:21.550602+00:00 — (no status field; keys: utc,actor,active_workers,worker_list,automation,concrete_progress) — 1467 B
- `zcode_crt_console_target` — 2026-10-04T22:45:37.361471+00:00 — PASSED_ON_TARGET — 213 B
- `zcode_cycle_script_guard` — 2026-10-05T03:06:05.722497+00:00 — FIXED — 244 B
- `zcode_d1_root_cause` — 2026-10-05T04:54:20.144405+00:00 — PROVEN_LINEAGE_BUG_FIX_IN_FLIGHT — 815 B
- `zcode_dll_family_summary` — 2026-10-04T22:45:37.361471+00:00 — (no status field; keys: utc,families) — 797 B
- `zcode_fileio_console_target` — 2026-10-04T21:33:47.616205+00:00 — PASSED_ON_TARGET_AFTER_NAMESPACE_ROOT_FIX — 570 B
- `zcode_flash_capacity_trial_1` — 2026-10-04T18:01:36.876630+00:00 — User-set steady state: 12 total incl. controller. Refills code-first, absolute paths, one-per-completion. — 3249 B
- `zcode_fresh_r11_gate_pair` — 2026-10-05T02:31:14.311836+00:00 — BOTH_GATES_GO_8_8_ALL_SHAS_RECONCILED — 486 B
- `zcode_gmalloc_hunt_dispatched` — 2026-10-05T03:48:10.264132+00:00 — worker-gmalloc-derivation in flight — 92 B
- `zcode_gmalloc_seed_integrated` — 2026-10-05T04:50:01.944516+00:00 — SLOT3_FILLED_TABLE_4_OF_4_COMPLETE — 1127 B
- `zcode_handoff` — NO-UTC — ZCODE_STARTED_BY_USER — 1330 B
- `zcode_igf5_diagnosis` — 2026-10-04T20:45:27.102818+00:00 — (no status field; keys: utc,igd1_verification,igf5_failure,action,user_correction) — 1108 B
- `zcode_igf6_RESULTS` — 2026-10-04T21:13:50.056090+00:00 — MODULE_LOADED_MAPSCAN_OK_EXEC_READ_CRASHED_MODULE_GAME_SURVIVED — 3003 B
- `zcode_igw_probe_filled` — 2026-10-05T03:40:59.518651+00:00 — ARMED_PROBE_BUILT_WITH_DESIGN_LADDER — 458 B
- `zcode_ingame_v1_results` — 2026-10-04T17:19:10.753668+00:00 — IN_GAME_RUN_COMPLETED_7_RECORDS_NO_WRITE_FAILURES — 2803 B
- `zcode_ingame_v2_results` — 2026-10-04T17:27:53.816836+00:00 — FULL_RUN_56_RECORDS_BASE_ESTABLISHMENT_REMAINS — 2100 B
- `zcode_launch10_armed_table_ready` — 2026-10-05T04:58:32.961127+00:00 — ARMED_VARIANT_GENERATED_OFFLINE — 634 B
- `zcode_launch10_prebuild_dispatched` — 2026-10-05T05:19:06.155809+00:00 — worker-launch10-prebuild in flight (armed 4-slot candidate prebuilt host-side via the d1fix lineage; launch-9 dirs un... — 182 B
- `zcode_launch11_branches_dispatched` — 2026-10-05T06:48:36.595984+00:00 — ALL_THREE_VERDICT_BRANCHES_IN_FLIGHT — 465 B
- `zcode_libarchive_skew_resolution` — 2026-10-04T15:56:54.244673+00:00 — RESOLVED_HEADERS_ALIGNED_TO_MATCHED_CLOSURE — 1122 B
- `zcode_loader_facts` — 2026-10-04T16:37:42.421279+00:00 — (no status field; keys: utc,standalone_payload_env,supplier_design_impact,module_list_qualification) — 1170 B
- `zcode_mutex_console_target` — 2026-10-04T21:27:30.560322+00:00 — PASSED_ON_TARGET_ALL_EXPECTATIONS_MET — 785 B
- `zcode_onepass_triggered` — 2026-10-05T03:58:19.500474+00:00 — EXECUTING — 478 B
- `zcode_outage_recovery` — 2026-10-04T23:41:41.542263+00:00 — ALL_9_SUBAGENTS_REDISPATCHED_CONSOLE_BACK — 385 B
- `zcode_path_test_target_run` — 2026-10-04T23:05:57.757572+00:00 — PASSED_BOTH_QUESTIONS — 937 B
- `zcode_ploader_ini_preimage` — 2026-10-05T02:27:06.259792+00:00 — (no status field; keys: utc,sha,content,content_sha256,content_note) — 441 B
- `zcode_prx_packaging_fixes` — 2026-10-04T18:35:14.554344+00:00 — link rerun 3 in background (candidate pending) — 379 B
- `zcode_r8_production_validation` — 2026-10-05T01:25:51.275833+00:00 — PRODUCTION_TEST_ALL_PASS_12_12_SECTIONS_1_4 — 1234 B
- `zcode_sdg2_staged` — 2026-10-04T23:11:53.422102+00:00 — STAGED_VERIFIED_AWAITING_LAUNCH — 725 B
- `zcode_sdg2_target_run` — 2026-10-04T23:17:33.064758+00:00 — DIAG_APPEARED_IMPORT_QUESTION_CLOSED — 929 B
- `zcode_seed_table_launch9_shape_confirmed` — 2026-10-05T04:51:44.699835+00:00 — STAGING_TABLE_3SLOT_MATCHES_CANDIDATE — 553 B
- `zcode_session_close_2026_10_04` — 2026-10-04T17:56:34.395333+00:00 — SESSION_CLOSED_BY_USER_FORK_CONTINUES_ELSEWHERE — 1507 B
- `zcode_supplier_composition_1` — 2026-10-04T16:42:28.921901+00:00 — COMPOSED_SUPPLIER_ADAPTER_INTEGRATED_BASELINE_EXTENDED — 2743 B
- `zcode_supplier_composition_2` — 2026-10-04T18:30:05.041169+00:00 — COMPOSED_VMMAP_PRIMARY_SUPPLIER_WITH_BASE_SCAN — 865 B
- `zcode_supplier_composition_3` — 2026-10-04T19:53:35.295100+00:00 — COMPOSED_ACK_ENABLED_20_OF_20_STATIC_CHECKS — 1282 B
- `zcode_supplier_composition_4` — 2026-10-04T21:39:34.954665+00:00 — COMPOSED_ACK_ENABLED_HARNESS_ARMED_17_OF_17_STATIC — 1190 B
- `zcode_supplier_composition_6` — 2026-10-05T05:00:16.324997+00:00 — NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED — 757 B
- `zcode_takeover_2026_10_04` — 2026-10-04T15:29:51.005185+00:00 — CONTROLLER_CLAIMED_HANDOFF_VALIDATED_NEXT_DLL_PASSED — 3879 B
- `zcode_ue4ss_critical_path` — 2026-10-04T16:03:41.575955+00:00 — S2_MECHANISM_VERIFIED_DATA_GAPPED_S1_PROBE_DISPATCHED — 787 B
- `zcode_ue4ss_integration_rules` — 2026-10-04T15:52:35.881570+00:00 — (no status field; keys: utc,actor,fname_rule,item_spawner_intersection,dll_track_rule) — 1469 B
- `zcode_v3_MAPPING_BREAKTHROUGH` — 2026-10-04T19:15:31.443970+00:00 — SINGLE_BIAS_CONFIRMED_LOAD7_PROVEN_GUA_ADDRESS_ESTABLISHED — 3238 B
- `zcode_v3_final` — 2026-10-04T18:43:48.069132+00:00 — STAGED_ZERO_RECORDS_CAUSE_UNCONFIRMED — 3935 B
- `zcode_v3_prx_interim` — 2026-10-04T18:37:22.379209+00:00 — BUILT but worker STILL RUNNING (depth extension active); do NOT stage until the worker delivers its final REPORT; thi... — 690 B
- `zcode_wiring_delivered_controller_attested` — 2026-10-04T23:41:41.542263+00:00 — ENTRY_DELIVERED_ATTESTED_BUILD_RUNNING — 1612 B

## sessions-governance

- `zcode_worker_acceptance_evidence` — 2026-10-04T21:04:09.632714+00:00 — DELIVERED_COMPLETE_EVIDENCE_SPEC_PER_RUNG — 641 B
- `zcode_worker_budget_policy` — 2026-10-04T18:32:08.288244+00:00 — (no status field; keys: utc,source,policy) — 413 B
- `zcode_worker_cheatsheet_update` — 2026-10-05T07:35:01.188548+00:00 — DELIVERED_ONE_PAGE_CURRENT — 379 B
- `zcode_worker_checkpoint_reconcile` — 2026-10-04T21:26:35.632752+00:00 — DELIVERED_22_ACTIONS_9_STALE_6_MISSING — 366 B
- `zcode_worker_controller_decision_brief` — 2026-10-05T03:13:28.920772+00:00 — DELIVERED_BRANCH_TABLES_TIMING_DISCIPLINE — 673 B
- `zcode_worker_deliveries_batch` — 2026-10-05T01:00:53.895100+00:00 — (no status field; keys: utc,batch2_layouts,tick_fallback_decision,e2e_stage_plan,rung_evidence_reader,lua_bootstrap) — 1130 B
- `zcode_worker_deliveries_batch2` — 2026-10-05T01:25:51.275833+00:00 — (no status field; keys: utc,record_sink_addon,static_tls_7fam,anchor_fallback_worker,post_anchor_hazards) — 1100 B
- `zcode_worker_docs_final_sync` — 2026-10-05T05:53:28.255651+00:00 — DELIVERED_TRILOGY_SYNCED_TO_FINAL_TOOL — 411 B
- `zcode_worker_e2e_promotion_applied` — 2026-10-05T03:11:22.974229+00:00 — DELIVERED_PROMOTED_54_TUPLES_VERIFIED — 419 B
- `zcode_worker_eve_coherence` — 2026-10-05T05:11:08.852273+00:00 — DELIVERED_ALL_CONFIRMED_DOC_FIXES_APPLIED — 473 B
- `zcode_worker_fname_ctor_audit` — 2026-10-05T02:44:56.428714+00:00 — DELIVERED_VERDICT_NO_AS_COMPOSED_REMEDY_DERIVED — 491 B
- `zcode_worker_handoff_update` — 2026-10-04T21:54:09.606019+00:00 — DELIVERED_COMPLETE_REWRITE_CURRENT — 386 B
- `zcode_worker_handoff_v3` — 2026-10-04T22:51:17.953722+00:00 — DELIVERED_CURRENT_STATE — 142 B
- `zcode_worker_index_final` — 2026-10-05T06:46:36.222848+00:00 — DELIVERED_15_15_VERIFIED_FINAL_READ — 423 B
- `zcode_worker_index_v2` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_DIGEST_V2 — 301 B
- `zcode_worker_itemspawner_lua_audit` — 2026-10-04T23:26:52.478889+00:00 — DELIVERED_API_MATRIX_RISK_RANK — 944 B
- `zcode_worker_knowledge_consolidation` — 2026-10-05T06:08:11.562708+00:00 — DELIVERED_KNOWLEDGE_BASE_PLUS_ANTIPATTERNS — 191 B
- `zcode_worker_mfa_activation_audit` — 2026-10-04T23:52:43.226671+00:00 — DELIVERED_GO_ACTIVATION_STAGE — 626 B
- `zcode_worker_post_promotion_runtime` — 2026-10-04T21:15:27.379696+00:00 — DELIVERED_COMPLETE_STAGING_PLAN_AND_TRIAGE — 751 B
- `zcode_worker_pre_session_inventory` — 2026-10-05T06:45:16.573716+00:00 — DELIVERED_SESSION_NOT_BLOCKED_4_FIXES_APPLIED — 418 B
- `zcode_worker_product_roadmap` — 2026-10-04T22:03:54.211769+00:00 — DELIVERED_5_MILESTONES_PRIORITIZED — 407 B
- `zcode_worker_program_unit_inventory` — 2026-10-05T02:27:06.259792+00:00 — DELIVERED_BIT_EXACT_CONTROL — 364 B
- `zcode_worker_project_state_index` — 2026-10-05T02:46:01.512673+00:00 — DELIVERED_INDEX_AND_DIGEST — 359 B
- `zcode_worker_promotion_decision` — 2026-10-04T21:02:26.529291+00:00 — DELIVERED_FORMAL_FRAMEWORK_C2_PENDING — 682 B
- `zcode_worker_risk_e2e` — 2026-10-04T19:46:03.433341+00:00 — DELIVERED_24_RISKS_D1_MERGE_DESIGNED — 788 B
- `zcode_worker_risk_update` — 2026-10-04T21:39:40.851555+00:00 — DELIVERED_26_ROWS_UPDATED_4_NEW_15_REFILL — 441 B
- `zcode_worker_roster` — 2026-10-04T17:56:14.571419+00:00 — (no status field; keys: utc,open_requests,controller,workers_in_flight,budget,in_flight) — 1246 B
- `zcode_worker_session_archive_scaffold` — 2026-10-05T06:20:41.772074+00:00 — DELIVERED_44_0_LAW_VERIFIED — 360 B
- `zcode_worker_session_audit` — 2026-10-04T18:37:22.379209+00:00 — DELIVERED_SUBSTANTIALLY_ACCURATE_6_DRIFTS — 822 B
- `zcode_worker_signatures_inventory` — 2026-10-05T03:02:02.617548+00:00 — DELIVERED_33_DEPS_3_NEW_DEAD_CLASSES — 1080 B
- `zcode_worker_startup_checkpoint` — 2026-10-04T23:02:31.176992+00:00 — DELIVERED_TWO_DIAGNOSTICS_DETERMINISM_PROVEN — 901 B
- `zcode_worker_tick_callback_audit` — 2026-10-05T00:00:33.976830+00:00 — DELIVERED_FULL_PATH_PLUS_TITLE_SCREEN_ANSWER — 1023 B
- `zcode_worker_ue4ss_docs_fidelity` — 2026-10-05T06:35:01.410173+00:00 — DELIVERED_1_INVALIDATION_3_STALE — 631 B
- `zcode_worker_user_launch_guide` — 2026-10-04T21:58:28.424274+00:00 — DELIVERED_ONE_PAGE_JARGON_FREE — 266 B
- `zcode_worker_walk_site_transitions` — 2026-10-05T07:35:40.496407+00:00 — DELIVERED_SINGLE_POINT_GUARD_144_144 — 748 B
- `zcode_worker_weakify_decision` — 2026-10-04T23:27:20.036462+00:00 — DELIVERED_STAY_STRONG_VINDICATED_BY_SDG2 — 876 B

## diagnostics

- `zcode_worker_d1_diagnosis` — 2026-10-05T04:59:12.686109+00:00 — DELIVERED_ROOT_CAUSE_PROVEN_TRIPLE_IDENTICAL — 560 B
- `zcode_worker_divergence_reconciliation` — 2026-10-05T06:14:53.293089+00:00 — DELIVERED_BOTH_RESOLVED — 481 B
- `zcode_worker_error_path` — 2026-10-04T22:54:32.227431+00:00 — DELIVERED_CONFIRMED_MISSING_WIRING — 395 B
- `zcode_worker_f_r2_fix` — 2026-10-04T18:30:05.041169+00:00 — DELIVERED_87_OF_87 — 501 B
- `zcode_worker_freshness_fixes` — 2026-10-05T03:09:57.757057+00:00 — DELIVERED_BOTH_FALSE_PASSES_NOW_REFUSE — 448 B
- `zcode_worker_igf5_diagnosis_rebuild` — 2026-10-04T21:03:25.193559+00:00 — DELIVERED_IGF6_ZERO_SYSCALL_DIRECT_READ — 1067 B
- `zcode_worker_mapping_recovery` — 2026-10-04T17:47:00.030617+00:00 — DELIVERED_2_TRUSTED_53_PLAUSIBLE_SPANS — 923 B
- `zcode_worker_mutex_deadlock` — 2026-10-04T17:42:13.689637+00:00 — ROOT_CAUSED_FIX_APPLIED_CONTROLLER_RERUN_PENDING — 869 B
- `zcode_worker_mutex_fix_2` — 2026-10-04T18:53:06.375642+00:00 — ROOT_CAUSED_FIX_APPLIED_BY_CONTROLLER — 907 B
- `zcode_worker_mutex_fix_review` — 2026-10-04T18:13:35.688998+00:00 — NOT_ALL_CLEAR_ONE_HIGH_GAP_FIXED_BY_CONTROLLER — 861 B
- `zcode_worker_predicate_gc_fix` — 2026-10-04T20:13:34.813431+00:00 — DELIVERED_VALIDATED_LAYOUT_A_ORIGINAL_PRESERVED — 2498 B
- `zcode_worker_uninstall_fault_expansion` — 2026-10-04T18:39:35.398391+00:00 — DELIVERED_1_DEFECT_FIXED_186_OF_186 — 902 B

## runtime-build

- `zcode_worker_1_live_supplier` — 2026-10-04T15:52:35.881570+00:00 — DELIVERED_TYPED_CONTRACT_HOST_VERIFIED_NO_TARGET — 1379 B
- `zcode_worker_bridge_r12_review` — 2026-10-05T03:40:59.518651+00:00 — DELIVERED_GO_WITH_FIXES_NOTHING_BLOCKS — 490 B
- `zcode_worker_bridge_r13` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_R13_SHA_10385e22 — 394 B
- `zcode_worker_carrier_probe_build` — 2026-10-05T06:46:00.287941+00:00 — DELIVERED_CC1_DETERMINISTIC_24_24 — 548 B
- `zcode_worker_combined_dll_allfam` — 2026-10-04T23:11:53.422102+00:00 — DELIVERED_HOST_CONTROLS_PASSED_SDK_COMPILED_NOT_EXECUTED — 804 B
- `zcode_worker_combined_igf7` — 2026-10-04T22:21:16.748911+00:00 — DELIVERED_FROZEN_READY_FOR_RUNG3_LAUNCH — 704 B
- `zcode_worker_composition5_gate` — 2026-10-04T23:28:36.040476+00:00 — DELIVERED_GATE_PLUS_NEGATIVE_CONTROL — 1183 B
- `zcode_worker_composition_4` — 2026-10-04T21:25:34.597516+00:00 — FAILED_AT_SCRIPT_GATE_ORDERING_DEFECT_CONTROLLER_FIXING — 716 B
- `zcode_worker_console_staging_bundle` — 2026-10-04T21:26:35.632752+00:00 — DELIVERED_MASTER_STAGING_PACKAGE_VERIFIED — 554 B
- `zcode_worker_createmutexa` — 2026-10-04T21:55:40.300619+00:00 — DELIVERED_7_OF_7_HOST_ZERO_EXTENSION_VERIFIED — 310 B
- `zcode_worker_crt_integrate` — 2026-10-04T22:45:04.989465+00:00 — DELIVERED_HOST_AND_SDK_PASSED — 525 B
- `zcode_worker_crt_startup_design` — 2026-10-04T18:18:20.541038+00:00 — DELIVERED_AND_ADOPTED_CENSUS_FILLED — 698 B
- `zcode_worker_crt_startup_implement` — 2026-10-04T21:48:18.588654+00:00 — DELIVERED_9_OF_9_HOST_PS5_CROSS_COMPILE_CLEAN — 701 B
- `zcode_worker_dll_mod_acquire` — 2026-10-04T21:28:55.907919+00:00 — DELIVERED_COMPLETE_CONTROLLER_RUNBOOK — 682 B
- `zcode_worker_dll_mod_retention` — 2026-10-04T16:37:42.421279+00:00 — DELIVERED_INVENTORY_NEGATIVE_TOOLING_READY — 795 B
- `zcode_worker_dll_track_closure` — 2026-10-05T06:10:43.165066+00:00 — DELIVERED_STATE_AND_COMPLETE_CRITERIA — 455 B
- `zcode_worker_e2e_console_staging` — 2026-10-04T18:30:42.777252+00:00 — DELIVERED_STAGING_PLAN_TWO_NEW_GATES — 1041 B
- `zcode_worker_emit_final_pair` — 2026-10-04T20:58:36.848733+00:00 — DELIVERED_FINAL_PAIR_ALL_IDENTITY_REAL_VERIFIED — 835 B
- `zcode_worker_exec_writability` — 2026-10-05T03:22:44.393603+00:00 — DELIVERED_EVIDENCE_RESET_PROBE_DESIGNED_DECISION_TREE — 1139 B
- `zcode_worker_fileio` — 2026-10-04T16:37:42.421279+00:00 — DELIVERED_ALL_GREEN_INTEGRATION_QUEUED — 646 B
- `zcode_worker_fileio_apply` — 2026-10-04T21:29:10.089352+00:00 — DELIVERED_HOST_AND_SDK_PASSED — 1199 B
- `zcode_worker_fileio_prep` — 2026-10-04T18:35:14.554344+00:00 — READY_TO_APPLY_rebased_on_mutex_tree — 778 B
- `zcode_worker_fileio_rebuild` — 2026-10-04T21:32:54.051389+00:00 — DELIVERED_CORRECT_REPORT_PATH_VERIFIED — 387 B
- `zcode_worker_final_staging_rehearsal` — 2026-10-04T21:47:43.999433+00:00 — DELIVERED_READY_TO_EXECUTE_ZERO_CORRECTIONS — 525 B
- `zcode_worker_gate_evolution` — 2026-10-05T02:27:06.259792+00:00 — DELIVERED_V2_30_30_FAIL_CLOSED — 528 B
- `zcode_worker_gmalloc_derivation` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_SEEDABLE — 619 B
- `zcode_worker_import_fallback` — 2026-10-04T22:55:11.271259+00:00 — DELIVERED_WEAK_IMPORTS_RECOMMENDED_18_OF_18 — 521 B
- `zcode_worker_installer_composition` — 2026-10-04T17:07:29.495291+00:00 — DELIVERED_PAYLOAD_ELF_ALL_IMPORTS_RESOLVED — 1074 B
- `zcode_worker_launch10_matrix` — 2026-10-05T04:00:37.558942+00:00 — DELIVERED_9_ROWS_4_DIMENSIONS_ONE_LOOKUP — 764 B
- `zcode_worker_launch10_prebuild` — 2026-10-05T05:37:01.137251+00:00 — DELIVERED_ARMED_CANDIDATE_GATE_15_15_GO — 701 B
- `zcode_worker_launch11_matrix` — 2026-10-05T07:50:52.655470+00:00 — DELIVERED_ONE_LOOKUP_ROUTER_LEDGER — 557 B
- `zcode_worker_launch11_vtable_prebuild` — 2026-10-05T07:36:16.144111+00:00 — DELIVERED_PREPARED_UNARMED_GATE_15_15 — 658 B
- `zcode_worker_launch9_prediction` — 2026-10-05T03:48:10.264132+00:00 — DELIVERED_HONEST_STOP_BEFORE_ENGINE_READY_EXPECTED — 929 B
- `zcode_worker_mutex` — 2026-10-04T19:09:09.811144+00:00 — HOST_CONTROLS_PASSED_AFTER_6_BUILDS — 804 B
- `zcode_worker_mutex_sdk_retry` — 2026-10-04T21:25:42.329347+00:00 — DELIVERED_SDK_COMPOSITION_PASSED_81_OF_81 — 697 B
- `zcode_worker_mutex_sdk_target` — 2026-10-04T19:52:29.562955+00:00 — SDK_COMPOSITION_FAILED_ONE_LINE_FIX_APPLIED — 902 B
- `zcode_worker_no_exec_gate` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_DESIGN_PLUS_DIFF — 504 B
- `zcode_worker_observer_packaged` — 2026-10-05T00:13:53.513367+00:00 — DELIVERED_OBSERVER_PRX_VERIFIED_DETERMINISTIC — 981 B
- `zcode_worker_observer_staging_extension` — 2026-10-05T03:51:18.176121+00:00 — DELIVERED_BATCH_TOOL_SESSION_COMPLETE — 442 B
- `zcode_worker_odr_emitter_check` — 2026-10-05T03:20:18.529078+00:00 — DELIVERED_16_CHECKS_NON_VACUOUS — 480 B
- `zcode_worker_oodle_verify` — 2026-10-04T22:11:09.019461+00:00 — DELIVERED_PASS_NO_OODLE_NEEDED — 475 B
- `zcode_worker_polyhook_prospero` — 2026-10-05T07:49:51.212259+00:00 — DELIVERED_ALL_14_SYMBOL_FAMILIES_COMPILE_GREEN — 525 B
- `zcode_worker_post_launch_tools` — 2026-10-05T00:13:53.513367+00:00 — DELIVERED_20_20_HOST_CASES — 390 B
- `zcode_worker_post_promo_exec` — 2026-10-04T21:58:28.424274+00:00 — DELIVERED_13_STEPS_5_FALLBACKS_30MIN_WINDOW — 349 B
- `zcode_worker_prx_link_gaps` — 2026-10-04T18:55:22.866473+00:00 — DELIVERED_PASSING_CANDIDATE_20_OF_20_STATIC — 1040 B
- `zcode_worker_prx_packaging` — 2026-10-04T17:06:38.512049+00:00 — DELIVERED_SCRIPT_LINK_PENDING_ONE_RUN — 781 B
- `zcode_worker_r3_carrier_prep` — 2026-10-05T06:11:17.193826+00:00 — DELIVERED_F1_NARROWED_3_CANDIDATES — 634 B
- `zcode_worker_r3_diffs` — 2026-10-05T07:28:40.438471+00:00 — DELIVERED_BOTH_DIFFS_COMPILED_INCL_ARMED — 759 B
- `zcode_worker_r3_zsyscall_tier` — 2026-10-05T04:05:27.433866+00:00 — DELIVERED_CONDITIONALLY_YES_WITH_DIFFS — 899 B
- `zcode_worker_ridealongs_combined` — 2026-10-05T02:21:04.479435+00:00 — DELIVERED_COMBINED_STAGING_VERIFIED — 552 B
- `zcode_worker_seh64_design` — 2026-10-04T22:26:30.521473+00:00 — DELIVERED_DESIGN_3_TIERS_COMPLEXity_ASSESSED — 343 B
- `zcode_worker_seh64_family` — 2026-10-05T02:04:49.138048+00:00 — DELIVERED_HOST_AND_SDK_PASSED_8TH_FAMILY — 623 B
- `zcode_worker_staged_pair_emission` — 2026-10-04T19:45:56.708842+00:00 — DELIVERED_FULL_CHAIN_PREPARED_SYNTHETIC_VERIFIED — 961 B
- `zcode_worker_staged_pair_live` — 2026-10-04T20:31:39.248338+00:00 — DELIVERED_FIRST_LIVE_DATA_PAIR_PARSEPAIR_VERIFIED — 1019 B
- `zcode_worker_staging_pkg_c4` — 2026-10-04T21:52:10.027610+00:00 — DELIVERED_V2_WITH_COMPOSITION_4_ACTIVE_TIER_B — 423 B
- `zcode_worker_supplier_ingame_update` — 2026-10-04T17:42:13.689637+00:00 — DELIVERED_169_CHECKS_RECOMPOSITION_READY — 643 B
- `zcode_worker_zip_bridge_console_link` — 2026-10-04T18:15:38.327341+00:00 — DELIVERED_CONSOLE_PAYLOAD_WITH_BRIDGE — 977 B

## mod-pipeline

- `zcode_worker_7_installer` — 2026-10-04T15:52:35.881570+00:00 — DELIVERED_108_OF_108_HOST_TESTS — 1162 B
- `zcode_worker_8_app_backend` — 2026-10-04T15:56:54.244673+00:00 — IN_FLIGHT — 517 B
- `zcode_worker_app_backend_integration` — 2026-10-05T02:35:09.684234+00:00 — DELIVERED_42_42_HOST_PROVEN_API — 588 B
- `zcode_worker_app_download_runtime` — 2026-10-05T02:48:36.233040+00:00 — DELIVERED_GAP_CLOSED_HOST_COMPLETE — 534 B
- `zcode_worker_app_modmanager` — 2026-10-05T01:58:38.038088+00:00 — DELIVERED_CORE_LOOP_18_18_HOST_TESTS — 681 B
- `zcode_worker_app_payload_repin` — 2026-10-05T06:30:46.841564+00:00 — DELIVERED_REPINNED_SUITES_GREEN — 362 B
- `zcode_worker_app_track_closure` — 2026-10-05T06:09:59.155187+00:00 — DELIVERED_SESSION_SLOT_AND_RECIPE — 483 B
- `zcode_worker_app_ui_flows` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_52_CHECKS_2_REAL_FIXES — 425 B
- `zcode_worker_batch_final_ini` — 2026-10-05T05:34:25.268232+00:00 — DELIVERED_SESSION_TOOL_FINAL_372_0 — 659 B
- `zcode_worker_e2e_records_ui` — 2026-10-04T18:12:59.242264+00:00 — DELIVERED_WITH_PROVENANCE_CAVEAT — 966 B
- `zcode_worker_e2e_suite_completion` — 2026-10-05T01:57:52.647198+00:00 — DELIVERED_348_OF_348_HOST_CHECKS — 675 B
- `zcode_worker_engineversion_ini` — 2026-10-05T02:55:50.293090+00:00 — DELIVERED_SLOT6_SETTINGS_ROUTE_VERIFIED — 604 B
- `zcode_worker_expected_bytes` — 2026-10-04T19:37:20.923158+00:00 — DELIVERED_ALL_THREE_ANCHORS_VERIFIED — 1207 B
- `zcode_worker_final_ini_layout` — 2026-10-05T04:02:14.792815+00:00 — DELIVERED_ORDER_COMMUTATIVE_LAYOUT — 660 B
- `zcode_worker_ingame_log_parser` — 2026-10-04T18:16:29.204795+00:00 — DELIVERED_AND_ACCEPTED_ON_REAL_LOGS — 468 B
- `zcode_worker_initializer_merge` — 2026-10-05T03:58:19.500474+00:00 — DELIVERED_SHAPE_A_COMPLETE — 817 B
- `zcode_worker_installer_auto_setup` — 2026-10-04T21:48:30.803725+00:00 — DELIVERED_4_DIFFS_AUTO_CREATE_AND_DATA_PATH_DEFAULT — 555 B
- `zcode_worker_installer_ps5io` — 2026-10-04T16:42:28.921901+00:00 — DELIVERED_HOST_TESTS_GREEN_COMPOSITION_QUEUED — 816 B
- `zcode_worker_itemspawner_bundle` — 2026-10-04T20:30:35.873798+00:00 — DELIVERED_32_OF_32_BUNDLE_READY_FOR_STAGING — 736 B
- `zcode_worker_itemspawner_dryrun` — 2026-10-04T18:43:48.069132+00:00 — DELIVERED_FULL_STATIC_ANALYSIS — 652 B
- `zcode_worker_layout_minimum` — 2026-10-04T16:37:42.421279+00:00 — DELIVERED_MINIMUM_SET_AND_GATED_ASSEMBLER — 678 B
- `zcode_worker_manifest_finalizer` — 2026-10-05T03:13:03.241234+00:00 — DELIVERED_FAIL_CLOSED_TOOL — 449 B
- `zcode_worker_modreqs` — 2026-10-04T15:52:35.881570+00:00 — DELIVERED_CENSUS_WITH_EVIDENCE — 987 B
- `zcode_worker_module_enum` — 2026-10-04T22:33:47.245517+00:00 — DELIVERED_HOST_AND_NATIVE_OBJECTS_PASSED — 399 B
- `zcode_worker_multi_mod` — 2026-10-04T22:26:30.521473+00:00 — DELIVERED_184_CHECKS_0_FAILURES — 329 B
- `zcode_worker_pak_index` — 2026-10-04T16:37:42.421279+00:00 — DELIVERED_DELTA_SETTLED — 575 B
- `zcode_worker_parser_validation` — 2026-10-04T21:50:50.788708+00:00 — DELIVERED_VALIDATED_2_BUGS_FIXED_ALL_3450_LINES_CLEAN — 606 B
- `zcode_worker_seed_table` — 2026-10-05T03:18:53.655715+00:00 — DELIVERED_3_OF_4_SEEDED_SLOT3_UNSEEDED_HONESTLY — 967 B
- `zcode_worker_settings_bundle_merge` — 2026-10-04T21:01:09.337319+00:00 — DELIVERED_MERGED_PACKAGE_VERIFIED — 774 B
- `zcode_worker_settings_v3` — 2026-10-05T03:25:42.966215+00:00 — DELIVERED_SINGLE_CANONICAL_PACKAGE_KEYS_REQUIRED — 712 B
- `zcode_worker_ue4ss_settings` — 2026-10-04T20:48:38.946709+00:00 — DELIVERED_CANONICAL_SETTINGS_HOST_VERIFIED — 953 B
- `zcode_worker_web_ui_connect` — 2026-10-04T22:15:04.405360+00:00 — DELIVERED_5_CANONICAL_PATCHES_VERIFIED — 572 B
- `zcode_worker_zip_to_mfb` — 2026-10-04T17:56:34.395333+00:00 — DELIVERED_F_R1_CLOSED_75_OF_75 — 669 B

## layout-memory

- `zcode_worker_2_layout_facts` — 2026-10-04T15:40:30.182793+00:00 — DELIVERED_VALIDATED_HOST_TOOLING — 1623 B
- `zcode_worker_6_containers` — 2026-10-04T15:40:30.182793+00:00 — DELIVERED_MATRIX_AND_ROUNDTRIP_EVIDENCE — 2445 B
- `zcode_worker_anchor_validation` — 2026-10-04T19:53:47.343734+00:00 — DELIVERED_FROZEN_ALL_CHECKS_PASS — 1088 B
- `zcode_worker_axes_detector` — 2026-10-04T20:45:27.102818+00:00 — DELIVERED_TOWNFALL_AXES_RESOLVED — 1089 B
- `zcode_worker_c2_alt_correspondence` — 2026-10-04T21:37:54.996417+00:00 — DELIVERED_SPEC_COMPLETE_NEEDS_ONE_PROBE_LAUNCH — 1489 B
- `zcode_worker_c2_probe` — 2026-10-04T22:10:16.041181+00:00 — DELIVERED_FROZEN_HOST_VERIFIED_DETERMINISM_PROVEN — 710 B
- `zcode_worker_code_anchor_reads` — 2026-10-04T20:35:25.138730+00:00 — DELIVERED_FROZEN_MPROTECT_FIRST_SYSCALL_SHIM — 1357 B
- `zcode_worker_container_headers` — 2026-10-04T16:16:51.543199+00:00 — DELIVERED_COMPLETE_HEADER_SPEC_AND_V2_RULES — 1616 B
- `zcode_worker_fc1_validator_probe` — 2026-10-05T03:52:45.593167+00:00 — DELIVERED_SAFE_DATA_SIDE_VALIDATOR — 574 B
- `zcode_worker_fname_construction` — 2026-10-04T17:04:13.330715+00:00 — DELIVERED_GAP_NARROWED_ONE_LEAD — 826 B
- `zcode_worker_fname_fc1_materialize` — 2026-10-04T20:01:33.124735+00:00 — DELIVERED_9_CALL_SITES_OFFLINE_PORT_FC1_CHECKS_DEFINED — 796 B
- `zcode_worker_fname_fc1_prep` — 2026-10-04T20:04:58.555603+00:00 — DELIVERED_WITH_BLOCKER_OFFLINE_MATERIALIZED — 769 B
- `zcode_worker_fname_runtime_probe` — 2026-10-04T21:21:44.278665+00:00 — DELIVERED_8_OF_8_HOST_MPROTECT_FIRST_FC1_MODULE — 851 B
- `zcode_worker_layout_batch2` — 2026-10-04T17:04:24.251422+00:00 — DELIVERED_43_OF_43 — 825 B
- `zcode_worker_layout_batch3` — 2026-10-04T19:12:08.540061+00:00 — DELIVERED_58_OF_58_END_TO_END_DRY_RUN_VERIFIED — 1252 B
- `zcode_worker_layout_identity` — 2026-10-04T20:51:17.263674+00:00 — DELIVERED_IDENTITY_RESOLVED_FROM_RETAINED_IMAGE — 1289 B
- `zcode_worker_layout_validator` — 2026-10-04T16:21:20.972662+00:00 — DELIVERED_23_OF_23_HOST_TESTS — 986 B
- `zcode_worker_liveprobe` — 2026-10-04T16:20:31.447610+00:00 — BUILT_AND_TARGET_COMPLETED_8_OF_8 — 942 B
- `zcode_worker_odr_safety` — 2026-10-05T02:49:49.576760+00:00 — DELIVERED_TWO_LOUD_FINDINGS_FIXED_BY_CONTROLLER — 1088 B
- `zcode_worker_predicate_compile` — 2026-10-04T19:40:50.375486+00:00 — DELIVERED_29_OF_29_PS5_OBJECT_CLEAN — 720 B
- `zcode_worker_query_harness` — 2026-10-04T17:02:34.090051+00:00 — DELIVERED_COMPOSABLE_ZERO_NEW_SYMBOLS — 1102 B
- `zcode_worker_query_harness_integration` — 2026-10-04T18:25:00.139627+00:00 — DELIVERED_ZERO_DELTA_COMPOSABLE — 956 B
- `zcode_worker_query_s2_arm` — 2026-10-04T21:12:29.506582+00:00 — DELIVERED_COMPLETE_INTEGRATION_PATCH_40_OF_40 — 771 B
- `zcode_worker_real_input` — 2026-10-04T17:31:24.957794+00:00 — DELIVERED_HOST_TESTS_GREEN — 1044 B
- `zcode_worker_real_input_pad` — 2026-10-04T20:35:53.220955+00:00 — DELIVERED_HARDWARE_TEST_READY — 814 B
- `zcode_worker_span_arbitration` — 2026-10-04T18:24:37.316324+00:00 — DELIVERED_VERDICT_REAL_MECHANISM_PROVEN — 950 B
- `zcode_worker_static_tls` — 2026-10-04T23:00:55.614864+00:00 — DELIVERED_HOST_NATIVE_CLEAN — 485 B
- `zcode_worker_tick_address_validation` — 2026-10-05T00:32:08.183047+00:00 — DELIVERED_DETOUR_SAFE_FOR_LAUNCH4 — 1105 B
- `zcode_worker_tick_propagation` — 2026-10-05T03:40:59.518651+00:00 — DELIVERED_93_OCCURRENCES_LIVE_TOOLS_FIXED — 379 B
- `zcode_worker_vaddr_crosscheck` — 2026-10-04T18:13:35.688998+00:00 — BLOCKED_WRONG_TREE_arithmetic_verified — 443 B
- `zcode_worker_vaddr_semantics` — 2026-10-04T17:42:13.689637+00:00 — DELIVERED_VERDICT_PVADDR_SEGMENTS_PARTIALLY_SOLVED — 1514 B
- `zcode_worker_virtual_memory` — 2026-10-04T22:40:38.408422+00:00 — DELIVERED_164_CHECKS_HOST_NATIVE_CLEAN — 330 B
- `zcode_worker_vtable_swap_design` — 2026-10-05T03:46:22.478360+00:00 — DELIVERED_PREMISE_CORRECTED_COMPLETE_HEDGE — 1230 B

## gameplay-verification

- `zcode_worker_desk_rehearsal` — 2026-10-05T04:48:28.311257+00:00 — DELIVERED_78_GREEN_CONDITIONAL_GO — 483 B
- `zcode_worker_e2e_rehearsal` — 2026-10-04T17:42:13.689637+00:00 — DELIVERED_61_OF_61_HOST_CONDITIONAL_GO — 751 B
- `zcode_worker_e2e_transcript` — 2026-10-04T21:49:31.451232+00:00 — DELIVERED_CHECKER_VALIDATED_BOTH_WAYS — 415 B
- `zcode_worker_engine_ready_walkthrough` — 2026-10-05T01:59:40.196808+00:00 — DELIVERED_NO_SURPRISE_BLOCKS_ORDERING_CORRECTED — 1081 B
- `zcode_worker_evidence_rehearsal` — 2026-10-05T02:52:27.386894+00:00 — DELIVERED_5_5_DISTINGUISHED_2_FALSE_PASS_BUGS_FOUND — 586 B
- `zcode_worker_ingame_probe` — 2026-10-04T16:57:44.567074+00:00 — BUILT_AND_STAGED_AWAITING_GAME_LAUNCH — 4304 B
- `zcode_worker_menu_runbook` — 2026-10-05T01:52:02.720419+00:00 — DELIVERED_MASTER_RUNBOOK_CP0_CP7 — 579 B
- `zcode_worker_pad_hardware_probe` — 2026-10-04T22:13:02.245528+00:00 — DELIVERED_FROZEN_HOST_VERIFIED_READY_TO_STAGE — 725 B
- `zcode_worker_post_spawn_closure` — 2026-10-05T06:04:49.635255+00:00 — DELIVERED_STABILITY_ACCEPTANCE_RESTORE — 549 B
- `zcode_worker_rung3_chunk_probe` — 2026-10-04T21:46:07.128644+00:00 — DELIVERED_20_OF_20_156_ASSERTIONS_ASAN_UBSAN_CLEAN — 663 B

## worker-general

- `zcode_worker_3_gamethread` — 2026-10-04T15:56:54.244673+00:00 — DELIVERED_SEAM_MAP_AND_REFUSAL_ADAPTERS — 3781 B
- `zcode_worker_4_integration` — 2026-10-04T15:56:54.244673+00:00 — INTEGRATED_HOST_AND_TARGET_PASSED — 4234 B
- `zcode_worker_7fam_stage_prep` — 2026-10-05T01:51:26.618028+00:00 — DELIVERED_ZERO_DECISION_PLAN — 635 B
- `zcode_worker_8_redo` — 2026-10-04T18:53:06.375642+00:00 — DELIVERED_61_OF_61_REAL_BINARY_TESTS — 716 B
- `zcode_worker_8fam_batch_patch` — 2026-10-05T03:06:05.722497+00:00 — DELIVERED_DLL8_FIRST_162_0_31_0 — 377 B
- `zcode_worker_8fam_console_prep` — 2026-10-05T02:35:39.013340+00:00 — DELIVERED_ZERO_DECISION_PLAN — 517 B
- `zcode_worker_9_evidence_review` — 2026-10-04T15:52:35.881570+00:00 — MILESTONE_STANDS_41_OF_44 — 837 B
- `zcode_worker_ack_integration` — 2026-10-04T19:05:47.784876+00:00 — DELIVERED_HOST_VERIFIED_COMPOSITION_READY — 960 B
- `zcode_worker_activation_bundle` — 2026-10-04T16:18:43.450616+00:00 — DELIVERED_BUNDLE_SPEC_AND_HOST_TESTS_LAYOUT_GATED — 1351 B
- `zcode_worker_batch2_live_prep` — 2026-10-04T20:53:21.099219+00:00 — DELIVERED_111_OF_111_RUNG3_READS_PREPARED — 782 B
- `zcode_worker_composer_splice` — 2026-10-05T03:49:33.395737+00:00 — DELIVERED_RECEIPT_TREE_DIVERGENCE_CLOSED — 457 B
- `zcode_worker_config_final_review` — 2026-10-04T23:26:52.478889+00:00 — DELIVERED_GO_ALL_8_FILES_ZERO_MUST_FIX — 963 B
- `zcode_worker_console_batch_runner` — 2026-10-05T02:44:56.428714+00:00 — DELIVERED_122_CHECK_SELFTEST_FAKE_DRIVE_GREEN — 445 B
- `zcode_worker_console_safety` — 2026-10-04T21:46:13.386854+00:00 — DELIVERED_STANDING_PROCEDURE — 519 B
- `zcode_worker_d2_design` — 2026-10-05T01:52:02.720419+00:00 — DELIVERED_OPTION_C_WITH_CORRECTIONS — 1186 B
- `zcode_worker_entry_review` — 2026-10-05T00:00:33.976830+00:00 — DELIVERED_GO_WITH_FIXES_0_P0 — 1252 B
- `zcode_worker_integration_checklist` — 2026-10-04T20:12:38.410764+00:00 — DELIVERED_15_STEPS_PROMOTION_TO_ACCEPTANCE — 781 B
- `zcode_worker_loadlist_final` — 2026-10-04T21:29:10.089352+00:00 — DELIVERED_45_OF_45_FINAL_STAGEABLE_PACKAGE — 409 B
- `zcode_worker_msvcp140_design` — 2026-10-04T22:26:30.521473+00:00 — DELIVERED_ASSESSMENT_282_SLOTS_ANALYZED — 338 B
- `zcode_worker_ninth_slot_adoption` — 2026-10-05T07:29:51.768302+00:00 — DELIVERED_9ENTRY_LAW_405_0_FULL_PERMUTATION — 577 B
- `zcode_worker_preflight` — 2026-10-04T23:02:10.876201+00:00 — DELIVERED_GO_CONDITIONAL_ALL_9_FILES_PASS — 338 B
- `zcode_worker_ps5io_rdwr` — 2026-10-04T17:56:14.571419+00:00 — DELIVERED_F1_CLOSED_NATIVE_DOWNLOAD_LINKED — 691 B
- `zcode_worker_replace_mechanics` — 2026-10-05T02:01:31.884798+00:00 — DELIVERED_EMPIRICALLY_PROVEN_PROCEDURE — 977 B
- `zcode_worker_runbook_eighth_sink` — 2026-10-05T07:57:06.857311+00:00 — DELIVERED_8SINK_SYNCED_CATCH_FIXED — 463 B
- `zcode_worker_runtime_ack` — 2026-10-04T17:06:38.512049+00:00 — DELIVERED_DIFFS_HOST_TESTED — 648 B
- `zcode_worker_s1_adapter` — 2026-10-04T16:37:42.421279+00:00 — DELIVERED_102_HOST_CHECKS — 680 B
