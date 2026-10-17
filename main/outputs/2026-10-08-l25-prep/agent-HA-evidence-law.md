# agent-HA — evidence-retention law: /tmp audit + workflow fix (2026-10-10)

Mac-only, battery-safe: file copies/moves within the worktree + tool/doc
edits. No console, no builds. Classifier script durably preserved at
`evidence/tmp-preserved-20261010/HA-AUDIT-SCRIPT-20261010.py` (sha in the
manifest `extensions` block).

## 1. /tmp audit (baseline: GU's MANIFEST.json, 1,269 entries, 2026-10-10T21:20:29Z)

Full /tmp walk (regular files only), file-level diff vs GU, sha256
twin-check against every file under `outputs/2026-10-08-l25-prep/`,
citation scan over landed .md docs.

**New-preserved: 370 files copied + sha-verified (0 mismatches), 411
individual manifest entries + 639 dir aggregates; manifest 1,269 → 2,319
entries.** Composition:

- **PRESERVE 23** — 7 cited-by-landed-docs stragglers GU missed
  (dec1.log, now.log, now2.log, v7-run.log, lfb-result.log,
  fr-syntax-luauobject-final.log + initial), 6 GZ in-flight decode
  snapshots, 4 GS regression prechecks (gs-*.log, gs-keymap-repro.ini —
  NOT previously durable; the per-suite regression logs are under
  host-tests/regression-logs/), 2 fk-snap pre-edit sources, 5 GT/GW/GY
  post-GU artifacts.
- **ARCHIVE-CHEAP 347** — loose UE4SS/console-session logs (THEMOMENT*,
  ctrl1-5, final1-9, live1-6, watch1-5, st/world/rf1/v4, seamresult,
  st-live, eh-l26v2.out, fg-* plan stdout, onionhen, sm_debug*, ps5_*,
  eta_*, morning-*, autoload-*, fast-cycle receipts, era scripts) — the
  exact sibling class GU preserved selectively (now3.log, v6-run.log...).
- **HASH-ONLY 41** (>=5MB rule): T1/T2/T3-eboot.bin (194 MB each),
  prelaunch.prx, fakesign/, ee-variantc/, wrapper-validation/,
  smp-investigate/ large binaries.
- **DURABLE-TWIN-AGGREGATE 19,304 files / 102 dirs**: sha-verified
  byte-identical to durable copies — law satisfied, not re-copied
  (TWIN-HASHES-HA-20261010.txt).
- **SKIP-RECREATABLE 9,547 files / 534 dirs** sha-recorded
  (SKIP-HASHES-HA-20261010.txt): gp-kstuff-lite-src extraction (pinned
  zips durable), bs-test, st2-host staging, build/perf scratch, .o/.d.
- **SKIP-INFLIGHT 70 files / 3 dirs**: agent-GQ state — not disturbed.

Task-named dispositions: l26-fk/* already in GU's manifest (verified);
plan-claude2/3/4.jsonl already preserved (verified; no newer plan files);
gp-kstuff-lite-src hash-recorded not copied; bs-test hash-recorded;
GS regression logs verified durable except the 4 prechecks → preserved;
rerun-check.json / wake-settings.ini / post-RESULTS.json = sha-verified
durable twins. GU's whole 1,240-file tree re-hashed: 0 missing, 0
mismatches; named originals show no drift.

## 2. Workflow fix — tools redirected (all syntax-checked; CLI overrides kept)

- `harvest/prep_modded_launch.py` — out-dir default →
  `outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-<X>/` (taps,
  monitors, readbacks, receipts); `--out-dir` documented+overridable;
  SETTINGS/PLOADER backups resolve DURABLE-FIRST (../evidence/, legacy
  /tmp fallback, sha gates unchanged); header documents the law;
  dry-run verified to print the durable path.
- `harvest/harvest.py` — **path-constant-only** KLOG_RANKED_DIRS edit:
  durable runtime variant dirs searched FIRST, legacy /tmp dirs kept.
  GZ's concurrent parser edits are separate regions — noted in-code and
  here; py_compile + ranked-dirs resolution verified.
- `harvest/wakeup_prep.sh` — readbacks → evidence/runtime/wake/; tap →
  durable variant-A dir; pinned sources durable-first.
- `harvest/bind_launch.sh` — LOGROOT → evidence/runtime/bind-pipeline/
  (env-overridable).
- `harvest/fast_cycle.sh` — receipts (prep/verdict/next logs, fetched
  UE4SS.log) copied durable to evidence/runtime/fast-cycle/ incl. runner
  EXIT-trap; FCTMP stays transient state; remediation texts updated.
- `harvest/lua_fast_deploy.sh` — tap out-dir durable.
- `harvest/sign_and_verify.sh` — readback template/receipt guidance now
  teaches $EV (evidence runtime), not /tmp.
- klog_capture.py (l18 console-test copy) deliberately NOT edited —
  provenance-locked run artifact; all live callers pass --out-dir
  explicitly (now durable).

Docs annotated "legacy, now durable at ..." (history preserved):
WAKE-RUNBOOK-20261010.md (+ top note), mod-ladder/LADDER-V2.md,
LAUNCH-PLAN.md, RUNBOOK-MOD-MENU-UP.md. GQ's in-flight launch-manifests/
NOT touched.

## 3. Screenshot law + residual allow-list

- `evidence/SCREENSHOTS/` created with README (capture-time filing,
  sha-recorded via SCREENSHOTS.sha256, `<session>-<timestamp>.<ext>`
  naming, composes with GM's AB-<A|B>-<region>-<UTC>.png) and the Leon
  glasses purged-screenshot loss recorded as the motivating failure.
- Residual /tmp allow-list (recreatable scratch ONLY) written into
  `EVIDENCE-RETENTION.md` §4: in-flight agent state (hash-recorded),
  transient watch/poll buffers, process state (pidfiles/mktemp staging),
  recreatable extractions/build scratch, hash-recorded large binaries,
  system/app runtime. Explicit NOT-allowed list: taps, monitors,
  readbacks, receipts, fetched session logs, pinned backups, screenshots,
  plan-run outputs, decode outputs.

Central law document: `outputs/2026-10-08-l25-prep/EVIDENCE-RETENTION.md`.
