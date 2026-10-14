# EVIDENCE-RETENTION LAW — PS5 UE4SS port (agent-HA, 2026-10-10)

## 1. The law

Claude's evidence-retention law (2026-10-10), standing from now on:

> **Nothing evidential lives in /tmp.** Move every receipt, screenshot and
> harvest into the durable archive with sha manifests, and audit for any
> other evidence still in /tmp.

Operationally, "durable" means the worktree outputs tree
(`outputs/2026-10-08-l25-prep/...`), which survives reboots and /tmp purges.
"Sha manifests" means every preserved item is recorded in
`evidence/tmp-preserved-20261010/MANIFEST.json` with sha256 + mtime + a
dated classification, and every deliberately-skipped item is sha-recorded
in a sidecar. Two corollaries:

- **Screenshots:** every console screenshot lands in
  `evidence/SCREENSHOTS/` at capture time, sha-recorded, named
  `<session>-<timestamp>.<ext>` — never /tmp, never chat-only. The
  purged Leon-glasses screenshot (the shader-fallback investigation's
  only observed defect, now surviving only as prose) is the motivating
  loss. See `evidence/SCREENSHOTS/README.md`.
- **Tools:** no tool in this project writes evidential output to /tmp by
  default. Defaults point into
  `outputs/2026-10-08-l25-prep/evidence/runtime/<label>/`; CLI overrides
  stay available. (Section 3.)

## 2. Migration record

### 2.1 GU pass (2026-10-10T21:20:29Z) — the big corpus

`evidence/tmp-preserved-20261010/` — 1,240 files copied (278 MB),
1,269 manifest entries (PRESERVE 54 / ARCHIVE-CHEAP 1,186 /
SKIPPED_LARGE_IN_DIR 17 / HASH-ONLY 12), plus 10 already-archived sha
verifications and 4 documented purged gaps. Full detail in its
`MANIFEST.json` + `CRITICAL-NOTES.md`.

### 2.2 HA extension (2026-10-10, dated section `HA-2026-10-10-audit-sweep`)

New /tmp evidence that appeared after GU's pass, plus stragglers GU's
baseline sweep missed. Method: full /tmp walk (regular files only),
file-level diff against GU's manifest, sha256 twin-check against every
file under `outputs/2026-10-08-l25-prep/` (excluding the archive itself),
citation scan over landed .md docs. Classifier script preserved at
`evidence/tmp-preserved-20261010/HA-AUDIT-SCRIPT-20261010.py`
(sha256 recorded in the manifest `extensions` block).

| Class | Count | Meaning |
|---|---|---|
| PRESERVE | 23 | copied + hashed. Breakdown: 7 cited-by-landed-docs (dec1.log→agent-ES; now.log/now2.log→STATE-L26-WAVE + packet INDEX; v7-run.log→agent-EC/EG + INDEX; lfb-result.log→CLAUDE-SUMMARY-LFB-DEAD-CODE; fr-syntax-luauobject-final.log + fr-syntax-luauobject.log→agent-FR), 6 GZ in-flight decode snapshots (gz-*.log/.txt; GZ's landed report stays authoritative), 4 GS regression prechecks (gs-*.log, gs-keymap-repro.ini), 2 FK pre-edit src snapshots (fk-snap/), 2 GT + 1 GW + 1 GY post-GU artifacts |
| ARCHIVE-CHEAP | 347 | copied + hashed — audit sweep: loose console-session/host logs not in GU's manifest with no durable twin (THEMOMENT*, ctrl1-5, final1-9, live1-6, watch1-5, st/world/rf1/v4, seamresult, st-live, eh-l26v2.out, fg-baseline/fg-postedit plan stdout, onionhen, sm_debug*, ps5_stderr, ps5_startup, eta_*, morning-*, autoload-*, fast-cycle receipts, era provenance scripts) |
| HASH-ONLY (>=5MB) | 41 | listed + hashed, not copied — T1/T2/T3-eboot.bin (194 MB each), prelaunch.prx, fakesign/ ee-variantc/ wrapper-validation/ smp-investigate/ large binaries |
| DURABLE-TWIN-AGGREGATE | 102 dirs / 19,304 files | every file byte-identical (sha-verified) to a durable copy — nothing evidential only in /tmp; full list in TWIN-HASHES-HA-20261010.txt |
| SKIP-RECREATABLE-AGGREGATE | 534 dirs / 9,547 files | recreatable scratch (build/test/perf, GP's pinned-zip extraction — zips durable at outputs/kstuff-lite-pinned/, pytest mktemp trees, .o/.d objects); full sha list in SKIP-HASHES-HA-20261010.txt |
| SKIP-INFLIGHT-AGGREGATE | 3 dirs / 70 files | agent-GQ mid-flight state (gq-inputs/gq-mm-store/gq-work), not disturbed; GQ's landed outputs are the durable record |

Totals: 370 files copied (0 sha mismatches after copy); manifest grew
1,269 → 2,319 entries. Post-GU artifacts with durable twins verified
individually: rerun-check.json (= shader-fallback/leon-classification.json),
wake-settings.ini (= settings-arm-both-on.ini), post-RESULTS.json (=
packet attachment). Task-named checks: /tmp/l26-fk/* — already fully in
GU's manifest, verified; plan-claude2/3/4.jsonl — already in GU's manifest,
verified; /tmp/bs-test — skip, hash-recorded (recreatable); GS regression
logs — the per-suite logs are durable under host-tests/regression-logs/,
the four /tmp gs-* precheck logs were NOT durable → preserved.

Integrity: GU's entire 1,240-file tree re-hashed against its manifest at
extension time — 0 missing, 0 mismatches. Named /tmp originals (l26-fk/*,
plan-claude*, claude2-live.log, build_variants.py.pre-FY.bak) show no
drift.

## 3. Tool changes (the workflow fix — /tmp defaults redirected)

All syntax-checked (`py_compile` / `bash -n`). CLI overrides preserved
everywhere. GQ's in-flight launch-manifests/ tree was NOT touched.

| Tool | Change |
|---|---|
| `harvest/prep_modded_launch.py` | session out-dir default `/tmp/l25-variant-<X>` → `outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-<X>/` (klog taps, eboot-monitor, decrypt-off readbacks, settings/ploader readbacks, arm responses). `--out-dir` now visible + documented (was hidden). SETTINGS_LOCAL / PLOADER_INI_LOCAL resolve DURABLE-FIRST (`../evidence/` copies; legacy /tmp fallback; sha+size gates unchanged). Header documents the law. |
| `harvest/harvest.py` | **path-constant-only edit** (GZ concurrently patches log parsing — different regions): KLOG_RANKED_DIRS now searches `evidence/runtime/l25-variant-{A,B}` BEFORE the legacy /tmp dirs (kept for pre-2026-10-10 sessions). Search is per-dir glob, hence explicit per-variant entries. |
| `harvest/wakeup_prep.sh` | decrypt-off PRX readback + settings readback → `evidence/runtime/wake/`; klog tap + stderr → `evidence/runtime/l25-variant-A/`; pinned upload sources (cache-on ini, zzdiag-v8 main.lua, loadlist) resolve durable-first. |
| `harvest/bind_launch.sh` | `LOGROOT` `/tmp/bind-pipeline` → `evidence/runtime/bind-pipeline/` (env-overridable `LOGROOT=...`). |
| `harvest/fast_cycle.sh` | FCTMP stays transient STATE (pidfiles/runners/watch buffers) but every RECEIPT (prep log, verdict quicklog + fetched UE4SS.log, next-chain log via runner EXIT trap) is copied durable to `evidence/runtime/fast-cycle/`. STEP-4/5 remediation texts now cite the durable backup paths. |
| `harvest/lua_fast_deploy.sh` | klog tap out-dir → durable `evidence/runtime/l25-variant-A/`. (loadlist fetch/push scratch stays /tmp — re-fetchable console state.) |
| `harvest/sign_and_verify.sh` | doc template + JSON receipt guidance now teach `$EV/readback.prx` (evidence runtime) instead of `/tmp/readback.prx`. |
| `outputs/2026-10-07-l18-full-build/console-test-*/klog_capture.py` | **deliberately NOT edited**: it is a provenance-locked copy of the tool that ran a recorded session; every live caller passes `--out-dir` explicitly (now durable). Its `/tmp/l12-harvest` default only matters for manual l12-era invocations. |

Landed docs annotated ("legacy, now durable at ...", history preserved):
`WAKE-RUNBOOK-20261010.md` (+ top note), `mod-ladder/LADDER-V2.md`,
`LAUNCH-PLAN.md` (§1.2, §2, backup-dependency section — now resolved
durable-first), `RUNBOOK-MOD-MENU-UP.md` (§4 readback note).

## 4. Residual /tmp allow-list (what may still live in /tmp, and why)

**Recreatable/transient scratch ONLY.** /tmp is acceptable exactly for
things that are (a) regenerable from durable sources or live console
state, (b) process-internal state, or (c) in-flight agent working state
whose durable record lands when the agent reports. Concretely, as of
2026-10-10:

- **In-flight agent state** — `gq-inputs/ gq-mm-store/ gq-work/` (agent GQ;
  hash-recorded in SKIP-HASHES sidecar; GQ's landed outputs are durable).
  Same rule for any future mid-flight agent's /tmp working dirs.
- **Transient watch/poll buffers** — fast_cycle.sh watch-* files,
  bind_launch.sh `/tmp/bind-watch-ue4ss.log` loop output, lua_fast_deploy
  loadlist fetch/push files: overwritten in place, re-fetchable from the
  console at any time.
- **Process state** — pidfiles, runner scripts, mktemp staging
  (`tmp-mount-*`, `mb-pe-file-*`, `ps5pl_*`, `l1[36]-*` test trees,
  `st2-host-*` ueselftest staging): recreatable from durable parts.
- **Recreatable extractions/builds** — `gp-kstuff-lite-src/` (re-extract
  from the PINNED zips at `outputs/kstuff-lite-pinned/`), `.o`/`.d`
  objects, build/check logs of local compiles (make_check*, ps5pc_*).
- **Large binaries already hash-recorded** — T1/T2/T3-eboot.bin,
  prelaunch.prx, fakesign/ ee-variantc/ etc.: sha256-recorded in the
  manifest extension; copies exist or content is reconstructable from
  pinned sources.
- **System/app runtime** — macOS/temp app state (claude-501, zcode-cua-501,
  com.apple.*, MSBuild*, steam*, zeb_def_*): never project evidence
  (GU-classified; honored).

NOT allowed in /tmp, ever: klog taps, eboot-monitor logs, decrypt-off
readbacks, LAUNCH-READY/FAILED receipts, session logs fetched from the
console, settings/ploader backups, screenshots, driver plan-run outputs,
agent decode/analysis outputs. If a new tool needs a scratch location for
any of these, its default is `outputs/2026-10-08-l25-prep/evidence/runtime/<label>/`.

## 5. Enforcement hooks (existing, unchanged)

- `harvest.py` refuses to guess a missing klog: its not-found note lists
  the ranked dirs (now durable-first), so a /tmp-purged tap is an explicit
  failure, not a silent gap.
- `LAUNCH-PLAN.md` "Local backup dependency" now documents the
  durable-first resolution of the pinned ini backups.
- Every future audit can diff /tmp against
  `evidence/tmp-preserved-20261010/MANIFEST.json` (now containing both GU's
  baseline and the HA extension) — the sidecars give full skip/twin sha
  coverage, so "new unclassified evidence in /tmp" is a mechanical check.
