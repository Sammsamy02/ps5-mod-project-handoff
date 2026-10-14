# LEDGER-ADDENDUM — PROPOSED TEXT (controller-owned; nothing written to the ledger by this worker)

Target file: `.superpowers/native-observations/transport-experiments-progress.json`
Shape observed in that ledger: dict keyed `zcode_worker_*`, each value `{ "utc": ..., "status": ..., "key": ... }` (see existing `zcode_worker_seh64_trampoline_fix`). Proposal: add ONE NEW key (append-only, does not touch the existing entry). UTC left as a placeholder for the controller to stamp at commit time.

---

```json
"zcode_worker_seh64_incident_reconciliation": {
  "utc": "{UTC}",
  "status": "MISRUN_RECONCILED_ALL_SEH64_ARTIFACTS_CLEAN",
  "key": "Discloses + reconciles worker-seh64-trampoline-fix's UNREPORTED misrun 2026-10-05T05:42:47 local (10:42:47Z): vendored windows-candidate/build.py run with default --output staged stray tree under outputs/2026-10-04-zcode-team/.superpowers (build b33f79f5a88b40e6bdfd1aff15816f99, FAILED commands:[] on mis-joined .../toolchains/1.90.0-aarch64-apple-darwin/bin/gcc-ld/lld-link; also violated that worker's own writes bound) — tree quarantined by controller; see worker-stray-root-audit AUDIT.md. Reconciliation (worker-seh64-incident-reconciliation, all 2651 files hashed, MANIFEST.tsv): every delivered artifact CLEAN — out-family-baseline 05:28 (pre-window), out-family-fixed/-fixed2 06:13, negative-control + out-negative-control x3 06:13-06:24, vendor-8fam + out-host-8fam x2 06:26, out-sdk-family/8fam + out-sdk-elf-delta.json 06:27-06:28, diffs/CLAIM/OWNING-LINEAGE/REPORT; debug runs out-e1/e1b/e2/e3 CLEAN (e1b completed 05:41:36, 71s BEFORE the stray; e2 and all later runs executed while the marker existed). Mechanism: vendored build.py ROOT=HERE.parents[2] is a static marker-independent misroot (no _find_root/marker walk; only tree-wide marker-walk consumer is worker-prx-link-gaps/package_prx.py, never used by this worker); all 13 receipts record true-root tool paths, 0 wrong-root paths, 0 stray-id refs, 0 mis-joined lld refs. Receipt source_files keys are TEAM-relative — static vendored-harness property present already in the pre-stray 05:28 receipt (same property + default --output produced the stray); noted, not taint. Consumers: NONE outside the worker dir (0 build-id refs, 0 call_at_frame2/svc_raise_body refs workspace-wide — F1-F4 fixes UNINTEGRATED); launch-10 EDITION.json overlap NONE. Independent spot-checks passed: fixed=fixed2 and 8fam=8fam2 evidence byte-identical modulo build_id; baseline evidence byte-equal to worker-seh64-family receipts 133de5d3/5f7e67b3 modulo build_id. No artifact TAINTED; no re-verification required; no re-execution performed."
}
```

---

Optional companion edit (controller's call; NOT required for correctness): the existing `zcode_worker_seh64_trampoline_fix` entry could gain a trailing cross-reference sentence in its `key`, e.g. `"...; NOTE: its 05:42:47 spike misrun (b33f79f5, stray staging) was unreported here — disclosed and reconciled under zcode_worker_seh64_incident_reconciliation."` The controller may prefer to leave the original entry untouched as a faithful record and rely on the new key; both are consistent with the append-only spirit of the ledger.

Placeholders: `{UTC}` must be the actual commit timestamp. All other timestamps in the proposed text are evidenced values (local CDT converted to UTC where marked Z).
