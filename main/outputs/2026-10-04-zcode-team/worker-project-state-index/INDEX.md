# INDEX — ZCode team day 2026-10-04 → 10-05 (Townfall UE4SS / Item Spawner)

Authoritative workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`
Ledger: `.superpowers/native-observations/transport-experiments-progress.json`
(532 top-level keys, **201 `zcode_*` entries** at read time, frozen at `2026-10-05T02:31:14Z`, entry `zcode_fresh_r11_gate_pair`).
**Note:** the ledger grew to 204 `zcode_*` entries (667,184 B) DURING this synthesis — three post-freeze entries landed at 02:35–02:36Z (`worker_app_backend_integration` DELIVERED 42/42, `worker_8fam_console_prep`, `comp6_drybuild` PIPELINE_PROVEN_GATE_GO_8_8); they are incorporated below and flagged in REPORT.md.
This index covers every `zcode_*` entry plus on-disk worker dirs that postdate the ledger.
Compiled by `worker-project-state-index` (read-only). See `CURRENT-STATE-DIGEST.md` for the 3-minute version.

Conventions: `L##` = ledger key (prefix `zcode_`); `W/…` = `outputs/2026-10-04-zcode-team/…`; `C/…` = `outputs/2026-10-04-zcode-controller/…`. sha16 = first 16 hex of sha256.

---

## (a) Per-track state

### Track 1 — UE4SS startup: composition-5 launches 1–8 (bridge revisions r2→r11)

Goal of each launch: `module_start → mfa_activate → StartActivatedRuntimeWithAck → engine-ready → mods-started → Item Spawner`. Every launch produced honest diagnostic evidence; each blocker got exactly one fix revision.

| Launch | Candidate | Blocker observed | Fix revision (bridge) | Evidence |
|---|---|---|---|---|
| pre-launch diag | sdg1/sdg2 (startup checkpoint PRXs) | composition-4 zero output — root-caused to **missing module_start→UE4SS startup wiring** (imports exonerated: sdg2 DIAG line proved all 196 imports bind) | r2 (wired bridge; W2 gate added) | L`composition_startup_diagnosis` (ROOT_CAUSE_FOUND/CONFIRMED), L`sdg2_target_run`, L`path_test_target_run` |
| 1 | comp5 r2 (fself 04869ab3) | `NoImageAnchor` — lmf identity anchoring impossible (DT_DEBUG dead, eboot header not in readable span) | r3 = igf7 data-window anchor (E: AES S-box + A: GNU build-id) | L`composition5_launch1_result`, L`composition5_r3_staged` |
| 2 | r3 | `KERN_PROC_VMMAP` sysctl ENOMEM with save loaded (1,408-entry buffer too small) | r4 = bridge-owned 16,384-entry buffer (~19 MiB .bss) | L`composition5_launch2_result`, L`composition5_r4_staged` |
| 3 | r4 | fixed `sizeof(kinfo_vmentry)`=1160 stride parse fails (kernel records are variable-size, kve_structsize=144) | r5 = per-record kve_structsize walk | L`composition5_launch3_result`, L`composition5_r5_staged` |
| 4 | r5 | walk works (22,828 entries) but `window_anchor_verify` failed — window A never igf7-verified; A found uncovered | r6 = walk-contract fix + E-only gating per user directive | L`composition5_launch4_result` |
| 5 | r6 | **anchor VERIFIED (identity established, VmScan)**; new blocker: `game_executable_path` required | r7 = multi-window gate B+D+E+(C\|\|C2) + kernel-observed kve_path attempt | L`composition5_launch5_result` |
| 6 | r7→r8 | (no dedicated ledger entry — gap) STAT existence of game_executable_path CONFIRMED enforced | r8 = user corrections: kve_path honest (8 B can't fit in 144 B record), per-capture cache, configured fallback labeled | L`worker_deliveries_batch2` ("launch-6 proved"), L`bridge_r8_user_corrections`, L`r8_production_validation` (12/12) |
| 7 | r8 (v4) | path ladder refused honestly; kve discovery scan all-zero (no kernel path on this kernel) | r9 = kve discovery + own-module rung source=3 | L`composition5_launch7_result` |
| 8 | r9 | **conversion + context PASSED first time**; read-lease threw "unregistered bytes" — extent clamp 0xfc602c8 not page-aligned | r11 = substitution REMOVED (user directive), stat-both diagnostics, page-quantized lease registration; r12 exists for comp6 | L`composition5_launch8_result`, W/worker-module-start-wiring/`runtime_start_bridge_r12.cpp` (sha16 feed5254, on disk) |

**Current staged build:** composition-5 **r11**, fself sha256 `0911af0c…` (recomputed on disk at W/worker-composition-5-rebuild/prx-candidate-composition5-1/modbridge_supplier_composition.prx) — live + v2 offline gates re-run **8/8 PASS both**, readback 30,772,344 B = parent − strip (L`fresh_r11_gate_pair`).
Gate receipts: W/worker-composition5-gate/`gate-receipt-composition4.json`, `-composition5-cycle.json`, `-composition5{-r2,-r3,-r4,-r5}.json` (6 receipts + CLAIM.json); v2 gate W/worker-gate-evolution/`gate_check_composition5_v2.py` + `baseline-composition5-r11.json`.

Related promotion milestones (pre-launch ladder):
- **Rung 1→2 PROMOTED** (L`PROMOTION_SIGNED`): C1 GUA layout A validated (L`predicate_gc_fix`, L`anchor_validation_RESULTS` — GUA 0x97ba110, 71,959 objects, read+write); C2 alternative correspondence 4/4 readable windows + base 0x400000 (L`c2_alternative_RESULT`, igf7-3dd949879a0a); C3 axes SHIPPING + STATS-OFF from 244-marker cost model (L`worker_axes_detector`); C4 identity 5.6.1-0+++UE5 / build-id 04bb4330 (L`worker_layout_identity`); C5 final staged pair emitted, ParsePair-verified (L`worker_emit_final_pair`).
- Tick/SCO byte-read explicitly UNRESOLVED: igf5 inline-syscall probe → console+game crash, zero records (L`console_crash_2`, L`igf5_diagnosis`); igf6 direct read → module died at first exec-span read, game survived (L`igf6_RESULTS`, amended: mechanism hypothesis, not proven fact). PS5 exec-only pages refuse reads from a ploader module (leading hypothesis).
- v1–v3 in-game facts probes established the mapping: single-load bias base 0x400000, LOAD0/LOAD7 correspondence, GUA covered by RW span (L`ingame_v1_results`, L`ingame_v2_results`, L`v3_MAPPING_BREAKTHROUGH`).

### Track 2 — composition-6 (next build: configured roots + lease fix + D2)

| Item | State | Reference |
|---|---|---|
| Scaffold (9 files) | On disk in C/comp6-integration/ (UE4SSProgram.cpp, lmf adapter .cpp/.hpp, NativeLoaderContext .cpp/.hpp, Platform-NativeProgramContext.hpp, PsMappedImageCaller.hpp, OwnedMappedImageCaller.hpp) + W/worker-module-start-wiring/`runtime_start_bridge_r12.cpp` (feed5254) + `runtime_module_entry_r4.c` (c97055f4) | C/comp6-integration/ (listed, hashed) |
| Change manifest | `change-manifest.json` (baseline composition5-r11; 3 changed units, 5 staged headers, 2 packaging units; import delta expected EMPTY; identity invariants listed) | C/comp6-integration/change-manifest.json |
| Designed by | Resolution B configured roots: W/worker-configured-roots-design/ (RESOLUTION-DESIGN.md, CONSUMER-USES.md — "directories only" confirmed with exact exceptions; ODR sha-equality gate required); D2 text-base guards: L`worker_d2_design` (option c; premise correction: adapter `derive_readable_ranges` lacks protection filter); replace mechanics proven: L`worker_replace_mechanics` (strong+strong duplicate error; Procedure B is the only route for `program`); program-unit inventory: L`worker_program_unit_inventory` (176-element argv, bit-exact control replay) | worker dirs + ledger |
| Dry build (D2-only rehearsal) | **GO**: composed object ef19a4b8 (117,781,064 B), wrapped PRX 35ba028e (30,792,663 B), double-build bit-identical, offline gate 8/8 PASS (W/worker-comp6-drybuild/DRYBUILD-RECEIPT.md + gate-receipt-composition6-dry1.json). Configured-roots-only files deferred (need Procedure B). Now LEDGERED: L`comp6_drybuild` (PIPELINE_PROVEN_GATE_GO_8_8_DETERMINISTIC; caught+fixed 4th includer + stale pin + 21-header staging list) | W/worker-comp6-drybuild/ |
| One-pass runner | C/comp6_one_pass.py (pre-flight scaffold shas + ODR port-vs-scaffold identity → driver → gate → upload only after GO; never launches) | C/comp6_one_pass.py |
| Driver / gate v2 | W/worker-comp6-driver/`build_runtime_composition6_fixed.py` (+regen receipt scripts); W/worker-gate-evolution/`gate_check_composition5_v2.py` 30/30 fail-closed (I1 exact UND delta, B1 configured-roots symbols, B2 substitution-refusal callee set, B3 identity-path guard, D2 honesty strings) | worker dirs, L`worker_gate_evolution` |
| In flight at close | worker-required-slots-feasibility (anchor-measurement.json + offline_anchor_scan.py, **no REPORT** — PS-scan slot question open); worker-fname-ctor-audit (empty dir) | worker dirs |

### Track 3 — Evidence tooling

| Tool | What it does | Where |
|---|---|---|
| post_launch_check.py | Fetches UE4SS.log + runtime-status.txt + observer log; classifies NOT_STARTED/FAILED_stage/ACTIVATED/ENGINE_READY/MODS_STARTED; 20/20 host cases | W/worker-post-launch-tools/, L`worker_post_launch_tools` |
| rung_check.py | Rung evidence reader; honest UNVERIFIED_DRIVER until qdrv1 runs; replays RUNG1+RUNG2 PASS from qdrv1 records | L`worker_deliveries_batch` (rung_evidence_reader), L`worker_deliveries_batch2` (record_sink_addon) |
| Menu runbook CP0–CP7 | Merged 10 audits into one ordered procedure PRERAN→activated→engine-ready→mods-started→banner→OPTIONS/menu→CROSS/spawn with per-checkpoint actions; embedded post_launch_check + rung_check | W/worker-menu-visibility-runbook/, L`worker_menu_runbook` |
| EVIDENCE-SPEC.md | Per-rung artifacts/PASS/FAIL/negative; channels pinned: aligned_entry.log (C1), ingame-facts.jsonl (C2), runtime-status.txt (C3), console capture (C4), mod print() tag | W/worker-acceptance-evidence/, L`worker_acceptance_evidence` |
| Batch cycle runner | stage_composition5_cycle.py: exit-code-gated build×2 → sha equality → gate GO → upload → readback band check | C/stage_composition5_cycle.py, L`composition5_launch4_result` (directive 2) |
| Console log parser | Repairs v1/v2 JSONL writer bugs; adopted after validation on all 9 real logs (3,450 lines, 13 build_ids, 2 more bugs fixed) | W/worker-ingame-log-parser/, W/worker-parser-validation/ |
| Ridealongs stage plan | Combined observer+qdrv1 ploader.ini plan (6 entries; mode-0 consumes no slot; MAX_PLUGINS=10/title; 5 registered) + non-interference audit; Step-0 pre-image recorded | W/worker-game-session-ridealongs/ (STAGE-PLAN-COMBINED.md, VERIFICATION-RECEIPTS.json), L`worker_ridealongs_combined`, L`ploader_ini_preimage` |

### Track 4 — DLL families (Windows-service emulation for real DLL mods)

| Family | Status | Evidence |
|---|---|---|
| InitOnce | TARGET-PROVEN | console build 1a6b37b7, 46/10 imports (L`worker_4_integration`, CURRENT-PS5-TEST/CURRENT-BUILD.md) |
| Mutex (+CreateMutexA adapter) | TARGET-PROVEN | 6f5bf85a, 51/10 (L`mutex_console_target`, L`worker_createmutexa`) |
| File I/O | TARGET-PROVEN | 664f83a5, 61/10 after /data/modbridge namespace-root fix (L`fileio_console_target`) |
| CRT startup (_initterm/_seh_filter_dll) | TARGET-PROVEN | 53808597, 68/10 (L`crt_console_target`) |
| Combined 6-family (InitOnce+Mutex+FileIO+CRT+ModuleEnum+VM) | TARGET-PROVEN in one build | 133c5e9e, ALLFAM_DLL_TEST.elf, TEN family keys, ledger matched (L`allfam_dll_console_run`) |
| Static TLS | host+SDK verified, console run PENDING — now inside the **8-family build preferred plan** (dll8 first, 7fam 83ee2ae9 kept as frozen fallback; memory.last.length oracle CORRECTED for 8fam: 114688/163840) | L`worker_static_tls`, L`worker_deliveries_batch2` (static_tls_7fam), L`worker_7fam_stage_prep`, post-freeze L`worker_8fam_console_prep` |
| Module Enum / Virtual Memory | console-proven inside 6-family build (individual SDK candidates also exist) | L`worker_module_enum`, L`worker_virtual_memory`, L`allfam_dll_console_run` |
| SEH64 | 8th family: host-verified raise→dispatch→__except/__finally; SDK ELF 222,408 B compiled NOT executed | L`worker_seh64_family` |
| MSVCP140 | ASSESSED only (282 slots, ~125-150 essential; recommendation: bounded narrow-only owner) | L`worker_msvcp140_design` |
| Family summary snapshot | L`dll_family_summary` (4 target-proven at 22:45Z; superseded same-night by 6-family run) |

### Track 5 — Installer / E2E

| Item | State |
|---|---|
| Transactional install slice (modforge-service/1) | worker-7: 108/108 host tests, ASan/UBSan (L`worker_7_installer`) |
| PS5 I/O adapter + runtime-ack contract | ps5io 64/64, bridge 28/28 (L`worker_installer_ps5io`); F1 O_RDWR closed, native download linked (L`worker_ps5io_rdwr`) |
| Installer payload ELF | 116 imports resolved; 26/26 host smoke (L`worker_installer_composition`); + zip→mfb bridge console payload cfa96ab5 (L`worker_zip_bridge_console_link`) |
| F-R1 (zip can't install directly) | CLOSED by zip-to-mfb auto-bundle, 75/75 (L`worker_zip_to_mfb`) |
| F-R2 (dangling begin line) | CLOSED, 87/87 (host flavor; console chain identical-not-rebuilt) (L`worker_f_r2_fix`) |
| D1 journal-order data-loss bug | Fixed in uninstall-fault worker; 186/186; **propagation to E2E chain root pending** (L`worker_uninstall_fault_expansion`, L`worker_risk_e2e` merged-chain design) |
| E2E rehearsal | 61/61 host, conditional GO; gates G0-G6 + G1 argv-dispatch probe (argv probe later crashed console — G1 unresolved) (L`worker_e2e_rehearsal`, L`worker_e2e_console_staging`, L`console_unreachable`) |
| Master staging packages | V1 (9-entry manifest, composition-3 placeholder) → V2 (composition-4 active Tier B) (L`worker_console_staging_bundle`, L`worker_staging_pkg_c4`) |
| E2E suite completion | 348/348 host checks; patches.json promotion PREPARED NOT APPLIED (51→54); schema amendment (ACK exit 6) recommended before RUN-CONFIG B; console E2E staging of a21771f8 PENDING user (L`worker_e2e_suite_completion`) |
| Execution guides | 13-step EXECUTION-GUIDE + 5 fallbacks; one-page USER-LAUNCH-GUIDE (L`worker_post_promo_exec`, L`worker_user_launch_guide`) |
| Auto-setup | ps5io auto-creates /data/modbridge root; default report path fixed to /data (L`worker_installer_auto_setup`) — proven needed by fileio first-attempt stage-41 failure |

### Track 6 — App (standalone PS5 Nexus app)

| Item | State |
|---|---|
| App backend drop-in (records/actions/caps/service) | worker-8 redo 61/61 real-binary tests; 3 diffs for controller to apply (L`worker_8_redo`) |
| Mod manager core loop | app/modmgr.py list/install/disable, 18/18 (L`worker_app_modmanager`); **app-backend-integration DELIVERED** (post-freeze L`worker_app_backend_integration`): vendored app tree + worker-8 patches applied, GET /api/installed + gated mutation routes, disable desired-set bug caught+fixed, desktop build live-smoked; console EFFECT of mutations pending controller session |
| Web UI patches | 5 canonical patches (3 backend + 2 frontend), regenerated after hunk miscounts; 5/5 git apply --check (L`worker_web_ui_connect`) |
| Records view UI | RECORDS-VIEW.md + PATCH-OUTLINE.md, provenance caveat (wrong-workspace export) (L`worker_e2e_records_ui`) |
| Multi-mod | N-mod state machine + topo load order, 184 checks (L`worker_multi_mod`) |
| Product roadmap | M1 E2E console run → M5 daily-use polish (L`worker_product_roadmap`) |
| Nexus acquisition runbook | EXECUTE-ACQUISITION.md; fixture OAuth must be rebuilt with real config; user prereqs: real OAuth client_id, sign-in, explicit GO (L`worker_dll_mod_acquire`) |

### Track 7 — Observer / qdrv1 artifacts (ridealongs)

| Artifact | Facts |
|---|---|
| Observer PRX | modbridge_runtime_aligned_observer.prx, 30,152,151 B, sha256 1afa0997… (recomputed on disk); 232/2 init/fini = frozen lineage; 191-row import contract exact; two builds byte-identical; NOT staged (on deck) (L`worker_observer_packaged`) |
| qdrv1 (record sink add-on) | modbridge_qh_driver.prx, 26,157 B, sha256 52c32216… (recomputed); 11 imports; writes FormatRecordJsonLine records to /data/modbridge-ingame-facts.jsonl; rung_check RUNG1+RUNG2 replay PASS; single-writer preserved (ingame_facts stays =0) (L`worker_deliveries_batch2`, ridealongs CLAIM) |
| Combined audit | 6-entry ploader.ini parser-proven; 5 plugins register at mode 2; MAX_PLUGINS=10/title → ≥4 slots spare; no-interference (observer sole UE4SS.log reader, qdrv1 sole jsonl writer) (L`worker_ridealongs_combined`) |
| Pre-image | ploader.ini 4-entry layout (aligned_entry=2, ingame_facts=0, composition=2, path_test=2); ledger sha 8d331b3f; copy at C/console/ploader-ini-preimage.txt (L`ploader_ini_preimage`) |

### Track 8 — Console inventory (as last verified)

Staged `/data/plugins/`: `modbridge_supplier_composition.prx` slot = **composition-5 r11 fself 0911af0c** (overwrote sdg2 → backup /tmp/sdg2-final-backup.prx; earlier composition-4 backup /tmp/composition4-backup-from-console.prx); `aligned_entry` = 2 (retained, proven); `path_test` = 2 (freshness oracle); `ingame_facts` = 0 (parser-inert). ploader.ini pre-image sha 8d331b3f.
Activation root `/data/modbridge/roots/PPSA33286/`: UE4SS-settings.ini 779 B (9f3fa127, recomputed on disk), MemberVariableLayout 588 B (f291d552), VTableLayout 186 B (9c20f7c9), PS5Pad.ini 2960 B (1d19fbf), main.lua 21,415 B (f586043a, 746 lines — recomputed), enabled.txt (e3b0c442), UEHelpers.lua 9,962 B (5a8f6609), loadlist 446 B 7 rows (49ab7a3a). (L`UE4SS_STAGING_COMPLETE` + later verified swaps.)
Payloads `/data/pldmgr/payloads/`: INITONCE_DLL_TEST (1a6b37b7), MUTEX_DLL_TEST (6f5bf85a), FILEIO_DLL_TEST (664f83a5), ALLFAM_DLL_TEST (133c5e9e); next: ALLFAM7_DLL_TEST (83ee2ae9), MODBRIDGE_INST_E2E (a21771f8).
Console incidents today (context for caution): argv double-dispatch crash (L`console_unreachable`), igf5 crash #2 (L`console_crash_2`), power outage mid-session with full recovery + re-dispatch (L`wiring_delivered_controller_attested`, L`outage_recovery`). Safety procedure: W/worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md (≥30 s dispatch gap, one-dispatch rule, timeout=crash-signal).

---

## (b) PENDING console tests

| # | What | Plan file | Expected evidence | User effort |
|---|---|---|---|---|
| 1 | **Launch 9: composition-6** (configured roots Resolution B + r11 page-quantized lease + D2 guards). Build+gate+stage first (controller), then launch. | C/comp6_one_pass.py; W/worker-comp6-driver/; gate v2; C/stage_composition5_cycle.py; W/worker-menu-visibility-runbook/ (CP0-CP7) | UE4SS.log: conversion passes (bridge_cfg_gamedir/workdir=1) → context → lease/D2 first exercise; runtime-status.txt activated→engine-ready→mods-started; failure = loud `modbridge module_start failed: <stage> code=<n>` with timestamp+pid prefixes (entry r4). Honest stop possible at PS-scan ("required slots" open) | Launch Townfall **with a loaded save**, wait ~1 min after launch for records; then OPTIONS (mapped F7) for the spawner |
| 2 | Observer + qdrv1 ridealong (=2 alongside composition) | W/worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md | observer log growth; /data/modbridge-ingame-facts.jsonl C2 records → rung_check RUNG1/RUNG2 replay on live data | None beyond test-1 launch (same session) |
| 3 | Combined DLL run with Static TLS — **8-family build preferred, 7-family frozen fallback** | W/worker-7fam-stage-prep/ + post-freeze W/worker-8fam-console-prep plan (L`worker_8fam_console_prep`) | records 37-40 (7fam) or 41-44 (8fam) in target JSONL; static_tls enabled; imports 55 exact; payload readback law exact | Game CLOSED; payload-only dispatch (one); batch order dll8 → e2e → stage-ridealongs → game-evidence |
| 4 | E2E installer console run (download→prepare→install→activate→uninstall) | W/worker-e2e-console-staging/ (STEP-BY-STEP, GO-NOGO G0-G6); e2e suite promotion (apply patches.json + schema amendment first) | e2e-status.jsonl stages 0-6 exit 0; check_transcript.py clean; MODBRIDGE_INST_E2E a21771f8 readback exact | Game CLOSED; possibly sign-in prerequisites for stage-1 native download |
| 5 | Pad hardware probe (standalone) | W/worker-pad-hardware-probe/ STAGE-PLAN (isolation required) | /data/pad-probe-events.jsonl; OPTIONS→F7 events; 7 PASS criteria | Press OPTIONS within 30 s poll window; no game needed (isolation) |
| 6 | Rung-3 combined probe (igf7-695a17bd: C2 reads + chunk/object reads) — optional, "adds rung-3 data for later use" | W/worker-combined-igf7/ | 299+ records incl. Objects**→chunk→FUObjectItem→UObjectBase; batch2 UStruct predicates in-process | One more Townfall launch |
| 7 | FName FC1 runtime probe (mprotect-first; held until exec-span approach resolved) | W/worker-fname-runtime-probe/ INTEGRATION.md | FC1-A 64-byte gate at 0x63d590; 9 call-site windows | Held — do NOT launch before exec-span decision (igf5/igf6 crash history) |

---

## (c) Open questions

| Question | Worker(s) | Expected deliverable | State |
|---|---|---|---|
| **Required slots** (PS-scan slot policy: will ScanGame's patternsleuth scan find its anchors without serialized-ELF witness, or stop honestly "PS scan timed out"?) | worker-required-slots-feasibility (IN FLIGHT: anchor-measurement.json + offline_anchor_scan.py, no REPORT); related: worker-tick-witness-design (serialized-ELF route + "ScanOverrides experiment for the four required slots") | Feasibility answer for launch 9's PS-scan stop risk; slot requirements; possibly tick-witness fallback design | OPEN — change-manifest explicitly: "honest stops possible at PS-scan (required-slots question in flight)" |
| **FName construction** (string→FName ctor; distinct from Conv_NameToString conversion which is solved) | worker-fname-construction (single lead: call-site candidate 0x23d590, static evidence); worker-fname-fc1-materialize (9 call sites, E8 rel32, offline); worker-fname-fc1-prep (FC-1 spec: 64-byte assertion + S1-S3/T1-T3); worker-fname-runtime-probe (mprotect-first FC1 module, 8/8 host); worker-fname-ctor-audit (empty dir) | In-process FC-1 runtime verification (blocks FindFirstOf/StaticFindObject name lookups + UFunction marshalling for the Spawner). Blocked by PS5 exec-only-page reads: direct read crashes (igf6), inline syscall crashed console (igf5); needs composed-runtime-init or C2-style safe route | OPEN — explicit rule L`ue4ss_integration_rules`: never bundle ctor with conversion |
| **ODR safety** for composition replacement (`Platform/NativeProgramContext.hpp` exists as port copy + staged overlay copy; `program` unit embeds its own compiled copy of the old inline validation) | worker-configured-roots-design (ODR hazard identified; ODR sha-equality gate required); worker-composition-replace-mechanics (strong+strong duplicate = error; Procedure B script revision + sha re-pin); enforced in comp6 driver (ODR port-vs-scaffold identity pre-flight in comp6_one_pass.py) | Mechanical byte-identity enforcement between port and staged header copies before any composition-6 build; attested retained-unit replacement set initially `{"program"}` | Design CLOSED, enforcement IN the comp6 pipeline; runtime proof lands with launch 9 |

Also tracked (smaller): ploader arm latency variance (~1.1 s–60 s; HTTP OK is transfer ack, not armed — wait for OK popup before launching, L`ingame_v1_results`); FTP file-caching delay for freshly written logs (~1 min; wait 15-30 s, fresh connection, size-change check, L`ingame_v3_MAPPING_BREAKTHROUGH` ftp_note + safety procedure); axes non_shipping/stats resolved from game evidence (SHIPPING/STATS-OFF, user must not be asked — L`predicate_gc_fix` user_confirmation); 4699-vs-4544 pak delta settled (two different files; saved pak = 4699, L`worker_pak_index`).

---

## Session-governance snapshot (for continuity)

- Concurrency: Flash trial measured 30 simultaneous > sustainable; **user final: 12 total including controller, paced starts, refill-on-completion** (L`flash_capacity_trial_1`, L`worker_budget_policy` deep budgets 100-150 calls).
- Handoffs: outputs/ZCODE-HANDOFF-2026-10-04/ (+zip, sha 26a98060) → L`handoff`, L`takeover_2026_10_04`; fork handoff V2 (W/worker-handoff-update/ZCODE-FORK-HANDOFF-V2.md, supersedes V1); checkpoint reconcile: W/worker-checkpoint-reconcile/ (CURRENT-STATE.md + RECONCILIATION.md, 22 actions).
- Ledger backup: transport-experiments-progress.json.bak-zcode-2026-10-04 (L`takeover_2026_10_04`).
