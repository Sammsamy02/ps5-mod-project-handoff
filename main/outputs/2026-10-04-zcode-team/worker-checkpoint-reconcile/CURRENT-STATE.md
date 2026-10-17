# CURRENT-STATE.md — every track as of 2026-10-04 ~21:30Z

Snapshot basis: checkpoint `updated_at` 21:21:44Z + worker-dir claims observed during reconciliation. All evidence is host/local or in-game probe records; nothing here is full-game acceptance.

## Critical path — Townfall UE4SS engine integration
- **Identity/axes RESOLVED**: engine 5.6.1-0+++UE5, build_id 04bb4330…, SHIPPING (margin 60), STATS OFF (margin 12) — from retained image + marker model (`worker-layout-identity`, `worker-axes-detector`).
- **GUA (S2) LIVE-VALIDATED**: layout A, address 0x97ba110, 71,959 objects, read+write confirmed (`zcode_anchor_validation_RESULTS`, `zcode_igf6_RESULTS`). Predicate GC-order bug fixed and re-validated (`worker-predicate-gc-fix`).
- **Staged pair COMPLETE**: FINAL pair ParsePair-verified, 17 members, all identity real — stageable today (`worker-emit-final-pair`); first live-data pair earlier (`worker-staged-pair-live`).
- **igf6 in-game**: module loaded, entry/startup clean, mapscan OK (10,047 records, 8,685 spans); interrupted before first exec-span read. Mechanism **NOT localized** (user-amended); code-anchor byte-read from a ploader module blocked — leading hypothesis exec-only page enforcement. User directive: no broad investigation, no repeated text-read attempts.
- **Remaining blocker = C2 code anchors**: alternative correspondence check in flight (`worker-c2-alt-correspondence`); FName ctor candidate single lead runtime 0x63d590, 9 call sites materialized offline (`worker-fname-construction`, `worker-fname-fc1-*`); FC-1 runtime probe delivered (`worker-fname-runtime-probe`).
- **Ready to apply**: query-harness + S2-arm one-shot integration patch (40/40) (`worker-query-harness*`); rung-3 read sequence prepared (111/111) (`worker-batch2-live-prep`, `worker-rung3-chunk-probe` in flight); recomposition via composition-4 in flight; staging plan + failure triage ready (`worker-post-promotion-runtime`); corrected settings bundle + Item Spawner + pad profile staged package verified (`worker-settings-bundle-merge`, `worker-itemspawner-bundle`, `worker-ue4ss-settings`, `worker-real-input-pad` hardware test ready).
- **Acceptance ladder** (user directive): genuine game-thread callback → valid object query → harmless reflected access → unchanged Item Spawner activation → visible spawn through real input. Rungs 1–2 evidence spec complete (`worker-acceptance-evidence`); promotion framework delivered, C2 pending (`worker-promotion-decision`).

## DLL track (Windows runtime / mutex / init_once / fileio)
- Mutex: integrated, host controls PASS after 6 builds (`zcode_worker_mutex` 19:09Z); SDK target build failed once (`ms_abi` on x86_64-sie-ps5), one-line fix applied — retry in flight (`worker-mutex-sdk-target`, `worker-mutex-sdk-retry`).
- InitOnce + sync runtime: integrated, host+target passed (`zcode_worker_4_integration`).
- FileIO: 16 census names + 4 fail-fast stubs, all green; diffs rebased on mutex tree, ready to apply — apply worker in flight (`worker-fileio`, `worker-fileio-prep`, `worker-fileio-apply`).
- Third-party DLL retention: none retained anywhere; acquisition plan + candidates ready (`worker-dll-mod-retention`, `worker-dll-mod-acquire` in flight).

## Installer track (PS5 mod-manager chain)
- Vertical slice 108/108 (`worker-7-installer`); composed payload ELF all 116 imports resolved (`worker-installer-composition`).
- F1 native download link CLOSED (O_RDWR mapping; 123 imports, 0 missing) (`worker-ps5io-rdwr`); F-R1 zip→mfb bridge CLOSED 75/75 (`worker-zip-to-mfb`, console-linked `worker-zip-bridge-console-link`); F-R2 exit shim + terminal event 87/87 (`worker-f-r2-fix`); uninstall D1 journal-order defect fixed 186/186 (`worker-uninstall-fault-expansion`).
- E2E rehearsal 61/61 GO; host-side gap closure (prepare→records + download v1 binding) delivered host-only (`worker-e2e-rehearsal`, `worker-e2e-final`); console dry-run plan + staging manifests ready, download ELF staged-deferred pending signed-in owner session (`worker-e2e-console-dry`, `worker-e2e-console-staging`, `worker-console-staging-bundle` in flight).

## App track (PS5 app backend)
- Worker-8 redo DELIVERED: 61/61 real-binary tests — records store (TSV, exact transitions), register/prepare/install/uninstall actions delegating to composed CLI, honest capability JSON (running only from ack), modforge-service/1 ABI (`zcode_worker_8_redo`).
- Records/UI audit delivered with provenance caveat — applies to an export copy; controller must re-verify app.js/index.html against the authoritative worktree (`zcode_worker_e2e_records_ui`).

## Container track (utoc/pak)
- Capability matrix + roundtrip evidence across sample archives (`worker-6-containers`); complete 144-byte utoc header spec + v2 rules, every field cited (`worker-container-headers`).
- Pak index delta SETTLED: saved pak authoritative count = 4699 (byte-exact closure); 4544 belongs to the original on-disc pak — never cross-apply (`worker-pak-index`).

## Console safety state
- Two crashes on record: argv double-dispatch (console unreachable, recovered) and igf5 inline-syscall suspect (console + game crash; replaced by igf6 zero-syscall direct read). igf6 interruption was module-contained — game and console survived. Next console contact must follow `worker-post-promotion-runtime` RUNTIME-STAGING-PLAN and e2e-console-dry GO gates; no ploader re-dispatch until verified.

## Session state
- Fork session continues in the authoritative workspace (`.codex/worktrees/translation-core/what`); steady state 12 contexts incl. controller (`zcode_flash_capacity_trial_1`); deep budgets for Flash workers (`zcode_worker_budget_policy`). 8 workers in flight at reconciliation (see RECONCILIATION.md item 2); `zcode_session_close_2026_10_04` is historical.
