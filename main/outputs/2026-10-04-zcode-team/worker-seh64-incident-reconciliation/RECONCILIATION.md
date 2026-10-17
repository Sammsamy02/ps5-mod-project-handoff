# RECONCILIATION — worker-seh64-trampoline-fix artifacts vs the 05:42 stray misrun

worker-seh64-incident-reconciliation · 2026-10-05 · LOCAL-ONLY (read/verify; zero mutations outside this deliverable dir; ledger edits are controller-owned)

Subject workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`
TEAM abbreviates `outputs/2026-10-04-zcode-team`. Subject dir: `TEAM/worker-seh64-trampoline-fix/` (2,651 files, all hashed in `MANIFEST.tsv`).

Times: worker-dir mtimes are LOCAL (this Mac, CDT = UTC-5). The misrun at local 05:42:47 = **10:42:47Z**. The task's "bracket" 05:41:36–05:46:12 local = 10:41:36Z–10:46:12Z. Ledger UTC stamps (e.g. 11:33:27Z) = local minus 5h.

---

## 1. Established timeline (local CDT)

| Time | Event | Evidence |
|---|---|---|
| 05:27:37–05:28:07 | `vendor/`, `diffs/*.orig`, `baseline-tree/` staged from worker-seh64-family | file mtimes |
| 05:28:25–05:28:30 | `out-family-baseline` run — PASSED (pre-fix bytes via `vendor/` pre-edit state) | receipt 8866fee6 |
| 05:39:29–05:39:31 | `out-e1` debug run — FAILED (-37 garbage) | receipt 4140cb84 |
| 05:41:34–05:41:36 | `out-e1b` debug run — FAILED | receipt 6d6924fa |
| **05:42:47 (10:42:47Z)** | **STRAY MISRUN** — vendored `build.py` invoked with DEFAULT `--output` (→ `TEAM/.superpowers/...`), died on FIRST `lld-link` exec: mis-joined `.../toolchains/1.90.0-aarch64-apple-darwin/bin/gcc-ld/lld-link` under the TRUE root prefix. `commands: []`. Left 2 files (13,728 B receipt + 568 B profile), build id `b33f79f5…` | quarantined receipt; audit §1–2 |
| 05:46:12–05:46:14 | `out-e2` debug run — FAILED (ran while marker existed) | receipt ca68a56d |
| 06:11–06:12 | `out-e3` debug run PASSED; final `vendor/` edits (F-3/F-4 fixes) | receipt 1f3960da |
| 06:13–06:24 | Delivered: `out-family-fixed`, `-fixed2`, `negative-control/` tree, `out-negative-control` ×3 | receipts |
| 06:26–06:28 | Delivered: `vendor-8fam/`, `out-host-8fam` ×2, `out-sdk-family`, `out-sdk-8fam`, `out-sdk-elf-delta.json`, final `diffs/`, CLAIM.json | receipts |
| 06:29–06:30 | `OWNING-LINEAGE.md`, `REPORT.md` | mtimes |
| 06:33:27 (11:33:27Z) | Ledger entry `zcode_worker_seh64_trampoline_fix` — F1–F4 only, misrun NOT mentioned | `.superpowers/native-observations/transport-experiments-progress.json` |
| between ~07:44 and 08:09 | Controller renamed stray to `.superpowers.QUARANTINED-stray-20261005T0542-…` | present at this worker's first listing 08:09; exact second unrecorded |

Wrong-root condition (a dir named `.superpowers` at TEAM) existed 05:42:47 → quarantine. Everything the worker wrote after 05:42:47 ran while that marker existed.

## 2. Why the marker could not affect this worker's runs (mechanism)

1. The run driver is the worker's OWN vendored harness `vendor/windows-candidate/build.py` (sha256 `c45a2c6878fb77cd…` — the exact file the stray receipt hashes, confirming the audit's attribution). Its root is `ROOT = HERE.parents[2]` (line 14) = TEAM — **static, marker-independent**, wrong by one level before, during, and after the stray existed. It contains no `_find_root`, no `.is_dir()` marker walk (grep over every `.py` in the worker dir: zero hits). The tree-wide ONLY marker-walk resolver remains `TEAM/worker-prx-link-gaps/package_prx.py`, never present in the seh64 worker dir.
2. `vendor/native-host/archive/link_layout.py` (loaded for SDK ELF candidates) resolves no root at all — it takes `sdk_root`/`output_dir` as arguments and pins the SDK linker script by sha256.
3. Every successful run therefore had to pass explicit true-root overrides for `--sdk`, `--llvm-bin`, `--lld-link` (the static TEAM-based defaults do not exist), plus an explicit `--output` under the worker dir. All of these are recorded verbatim in each receipt's argv — and all recorded tool paths are TRUE-root (`what/...`).
4. Residual static wrong-root properties of the vendored harness (present in ALL runs including the pre-stray 05:28 baseline, so NOT misrun-window artifacts):
   - receipt `source_files` keys are `str(path.relative_to(ROOT))` → TEAM-relative (`worker-seh64-trampoline-fix/vendor/...`); the run-time hash gate (build.py:349–350) re-hashed each under `TEAM/<key>` = the worker's own tree and passed;
   - subprocess `cwd=ROOT` = TEAM for every command; all argv paths are absolute, so no command resolved anything through cwd.

## 3. Per-artifact inventory and verdicts

Verdict rubric (from the task): CLEAN (pre-window or no root-derived paths) / TAINTED-SAFE (wrong-root recorded but bytes independently verified) / TAINTED (wrong-root provenance, no independent verification). Full per-file mtime+sha: `MANIFEST.tsv`. Group aggregate = sha256 of the sorted per-file sha list.

| # | Artifact group | Files | mtime range (local) | Window | Role | Verdict | Evidence |
|---|---|---|---|---|---|---|---|
| 1 | `diffs/` | 6 | 05:27:37–06:28:19 | PRE + AFTER | DELIVERED (F1–F4 diffs + .orig) | **CLEAN** | hand-generated diff text; zero wrong-root strings |
| 2 | `baseline-tree/` | 219 | 05:28:07 | PRE | DELIVERED (pre-fix reference tree) | **CLEAN** | static copy; `.superpowers` refs are the same static-default pattern, not executed paths |
| 3 | `vendor/` | 219 | 05:27:37–06:12:48 | PRE + AFTER | DELIVERED (fixed source tree) | **CLEAN** | build.py `c45a2c68…`, owned_runtime.c `c505cf9b…`, sdk_driver.c `201e8ca5…` match stray receipt's source hashes (audit anchor) |
| 4 | `out-family-baseline/` + log | 63+1 | 05:28:25–05:28:30 | **BEFORE** | DELIVERED (D4) | **CLEAN** | PASSED; receipt true-root tools; evidence independently re-verified vs worker-seh64-family receipts (§6) |
| 5 | `out-e1/` + log | 51+1 | 05:39:29–05:39:31 | BEFORE | debug (not delivered) | **CLEAN** | FAILED as designed; worker-dir-only paths |
| 6 | `out-e1b/` + log | 51+1 | 05:41:34–05:41:36 | DURING bucket, but **entire run completed 71 s before stray creation** | debug | **CLEAN** | last write 05:41:36 < 05:42:47 |
| 7 | `out-e2/` + log | 51+1 | 05:46:12–05:46:14 | DURING (marker existed) | debug (REPORT cites as provenance) | **CLEAN** | receipt ca68a56d: true-root tools, worker-dir outputs, zero wrong-root paths |
| 8 | `out-e3/` + log | 63+1 | 06:11:24–06:11:27 | AFTER | debug | **CLEAN** | PASSED; same receipt audit |
| 9 | `out-family-fixed/` + log | 63+1 | 06:13:07–06:13:10 | AFTER | DELIVERED (F1–F4 green ×1) | **CLEAN** | PASSED; true-root tools |
| 10 | `out-family-fixed2/` + log | 63+1 | 06:13:16–06:13:19 | AFTER | DELIVERED (determinism ×2) | **CLEAN** | evidence byte-identical to #9 modulo build id (re-verified §6) |
| 11 | `negative-control/` | 657 | 06:13:37–06:23:12 | AFTER | DELIVERED (control trees) | **CLEAN** | static trees; control-only diagnostics documented by the worker |
| 12 | `out-negative-control/` + log | 51+1 | 06:13:51–06:15:22 | AFTER | DELIVERED (bite proof; FAILED-by-design) | **CLEAN** | receipt add92799: wedge at -138 as claimed |
| 13 | `out-negative-control-probeonly/` + log | 51+1 | 06:20:16–06:21:48 | AFTER | DELIVERED control | **CLEAN** | receipt d737902c |
| 14 | `out-negative-control-probeonly2/` + log | 51+1 | 06:23:12–06:24:44 | AFTER | DELIVERED control | **CLEAN** | receipt 75ea212d |
| 15 | `vendor-8fam/` | 225 | 06:26:09 | AFTER | DELIVERED (grafted 8fam tree) | **CLEAN** | static graft |
| 16 | `out-host-8fam/` + logs ×2 | 93+2 | 06:26:14–06:26:19 | AFTER | DELIVERED | **CLEAN** | PASSED; receipt 8fd6c382 |
| 17 | `out-host-8fam2/` | 93 | 06:26:31–06:26:36 | AFTER | DELIVERED (determinism ×2) | **CLEAN** | evidence byte-identical to #16 modulo build id (re-verified §6) |
| 18 | `out-sdk-family/` + log | 203+1 | 06:26:54–06:27:01 | AFTER | DELIVERED (compile-only ELF) | **CLEAN** | receipt 07872399: prospero-clang/crt1.o etc. all true-root; COMPILED_NOT_EXECUTED |
| 19 | `out-sdk-8fam/` + log | 360+1 | 06:27:06–06:27:16 | AFTER | DELIVERED (compile-only ELF) | **CLEAN** | receipt c22279d9; same audit |
| 20 | `out-sdk-elf-delta.json` | 1 | 06:28:04 | AFTER | DELIVERED | **CLEAN** | derived from receipts #18/#19 |
| 21 | top-level run logs (14) | 14 | 05:28:30–06:27:16 | mixed | console captures | **CLEAN** | tracebacks/paths all worker-dir or true-root |
| 22 | `CLAIM.json` | 1 | 06:28:45 | AFTER | DELIVERED | **CLEAN** (see caveat §4) | sha `7d9eef67…` |
| 23 | `OWNING-LINEAGE.md` | 1 | 06:29:17 | AFTER | DELIVERED | **CLEAN** | sha `8c6d933f…` |
| 24 | `REPORT.md` | 1 | 06:30:25 | AFTER | DELIVERED | **CLEAN** (see caveat §4) | sha `10f1b558…` |

Receipt shas (receipt.json per build dir): e1 `3397880f…`, e1b `8f80d161…`, e2 `aba196e9…`, e3 `a425ec33…`, baseline `c0544a4c…`, fixed `d2716908…`, fixed2 `7a63c566…`, negctl `f3fab1ee…`, probeonly `3e6ab540…`, probeonly2 `8160bba9…`, 8fam `dcb05f38…`, 8fam2 `4d3a326a…`, sdk-family `8fd75f1c…`, sdk-8fam `d485b560…`.

**Verdict summary: 24/24 CLEAN. Zero TAINTED, zero TAINTED-SAFE.** No re-verification action is REQUIRED for any artifact. What would have needed re-verification had any group been TAINTED: that group's receipt-recorded output hashes re-checked against the bytes — unnecessary here.

## 4. Receipt / claim path audit (grep evidence)

Across ALL 2,651 files of the worker dir:

- `2026-10-04-zcode-team/.superpowers` (wrong-root absolute prefix): **0 hits**.
- stray build id `b33f79f5`: **0 hits** (matches audit §1's workspace-wide result).
- mis-joined lld-link (`toolchains/1.90.0-aarch64-apple-darwin/bin/gcc-ld` without `lib/rustlib`): **0 hits**.
- `.superpowers` hits that DO exist are: (a) true-root tool paths inside receipt argv (`what/.superpowers/.../bin/gcc-ld/lld-link` — the CORRECT lld-link), and (b) static default-path strings inside the copied `build.py`/verify scripts of `baseline-tree/`, `negative-control/` trees (never executed as defaults — all runs overrode them; the ONE run that used a default `--output` was the misrun itself, which died before any compile).

Caveats recorded for honesty (properties, not taint):
- Every receipt's `source_files` keys are TEAM-relative (mechanism §2.4) — identical in the pre-stray 05:28 baseline receipt, proving this is a static vendored-harness property, not misrun contamination. Note this is the same property that, combined with a default `--output`, PRODUCED the stray.
- `CLAIM.json`/`REPORT.md` are accurate about their runs but silent on the misrun — the unreported-event finding of the audit stands; this document and the proposed addendum record it.

## 5. Consumer map (who consumed seh64 artifacts)

| Potential consumer | Result |
|---|---|
| Other TEAM worker dirs | `grep -r 'worker-seh64-trampoline-fix'` excluding the worker dir: only `worker-stray-root-audit/` (AUDIT/CLAIM/REPORT — incident documentation) and `worker-stray-root-hardening/` (LEDGER-ADDENDUM, find_root_v2.py — incident response). **No artifact consumer.** |
| Any of the 14 build ids outside the worker dir | **0 hits** (TEAM-scoped and workspace-wide). |
| New F-3/F-4 symbols `call_at_frame2` / `svc_raise_body` outside the worker dir | **0 hits** workspace-wide — the fixed SEH64 code is UNINTEGRATED; no downstream tree, staging, or composition contains it. |
| Composition / launch drivers (`worker-comp6-*`, `worker-launch10-*`, `worker-launch11-*`, controller `launch10-build/`) | No reference to the worker, its dirs, ids, or symbols. |
| Controller `launch10-build/EDITION.json` (read in full) | Edits reference worker-facts-geometry-fix, worker-launch10-readpath-guard, worker-diagnostic-sink, controller-staged header + bridge_cli only. **Overlap with seh64 artifacts: NONE.** The l10 driver scripts contain no `seh64` reference. |
| Shared ledger `.superpowers/native-observations/transport-experiments-progress.json` | Entry `zcode_worker_seh64_trampoline_fix` (11:33:27Z) describes only F1–F4 deliveries; never the misrun (audit-consistent). |
| Quarantined stray receipt | Hashes 4 files of the worker's vendor tree (attribution anchor); produces no dependency in the reverse direction. |

**Conclusion: the seh64 worker's delivered artifacts have ZERO downstream consumers today. Even if an artifact had been tainted, nothing in launch-10 (or anywhere else) inherits it.**

## 6. Independent spot-checks performed (beyond the required scope)

1. `out-family-fixed` vs `out-family-fixed2` `host-normal.jsonl`: byte-identical after normalizing build ids — the worker's determinism claim holds.
2. `out-host-8fam` vs `out-host-8fam2` `host-normal.jsonl`: byte-identical modulo build ids — holds.
3. `out-family-baseline` `host-normal.jsonl`: byte-equal (modulo build id + worker-dir prefix) to BOTH original `worker-seh64-family` receipts `out-host-validation/133de5d3…` and `out-sdk-composition/5f7e67b3…` — the baseline evidence stream has independent cross-worker provenance, so even the pre-window baseline run is verified outside its own receipt.

These checks verify internal consistency and provenance; they are not re-executions and add no new engine evidence.
