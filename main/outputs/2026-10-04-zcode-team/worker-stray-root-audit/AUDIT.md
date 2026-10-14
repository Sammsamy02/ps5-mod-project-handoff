# AUDIT — stray `outputs/2026-10-04-zcode-team/.superpowers` tree

worker-stray-root-audit · 2026-10-05 · LOCAL-ONLY (read/analyze; zero mutations; controller owns disposition)

Paths below are relative to the workspace root `/WORKSPACE/.codex/worktrees/translation-core/what` unless absolute. "TEAM" abbreviates `outputs/2026-10-04-zcode-team`.

---

## 1. The stray tree, completely

Single mkdir event, all 6 paths mtime `2026-10-05 05:42:47`:

```
TEAM/.superpowers/                                                          dir  (96 B entry)
└── native-dependency-spike/                                                dir
    └── owned-dll-sdk-candidate-1/                                          dir
        └── b33f79f5a88b40e6bdfd1aff15816f99/                               dir
            ├── owned_profile.h                               568 B  05:42:47
            └── receipt.json                               13,728 B  05:42:47
```

Total content: 14,353 B in 2 files. (walkguard REPORT estimated "20 KB" — close; exact bytes above.)

**Classification: a botched staging copy of ONE FAILED owned-dll-sdk-candidate spike attempt.** Not a cache, not a second ledger, not game data:

- `owned_profile.h` is the standard 18-`#define` build-profile header emitted by the `windows-candidate/build.py` harness (build.py:58).
- `receipt.json` is that harness's receipt: `status: "FAILED"`, `commands: []` (it died on the FIRST lld-link exec, before any command was appended), full `source_files` hash map, build_id `b33f79f5a88b40e6bdfd1aff15816f99`, and error:
  `[Errno 2] No such file or directory: '.../what/.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/bin/gcc-ld/lld-link'` — a path that has NEVER existed (the real lld-link lives at `.../1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link`; the `bin/gcc-ld` variant is a mis-joined path). So this run was doubly doomed: wrong lld-link argument AND (because of the root bug, §2) staged under TEAM instead of the workspace root.

**Not recorded anywhere legitimate:**
- NOT present in the canonical spike tree: `.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1/` holds 24 other build-id dirs; `b33f79f5…` is absent.
- NOT in the shared ledger `.superpowers/native-observations/transport-experiments-progress.json`: `b33f79f5` appears nowhere; the `zcode_worker_seh64_trampoline_fix` entry (2026-10-05T11:33:27Z) describes only the final F1–F4 delivered work, never this staging attempt.
- No other file under TEAM references the build id (workspace-wide grep: only the stray's own 2 files).

## 2. Owner attribution: worker-seh64-trampoline-fix (HIGH confidence, mechanical)

Evidence chain, strongest first:

1. **The receipt's own path algebra names its ROOT.** `source_files` keys are `str(path.relative_to(ROOT))` (build.py:102) and every key begins with `worker-seh64-trampoline-fix/vendor/…` — which is only possible if `ROOT == TEAM` (the parent of `worker-seh64-trampoline-fix`). That is the exact bug, recorded by the perpetrator.
2. **The receipt lists the build script itself** (`Path(__file__).resolve()` is in `sources`, build.py:83) as key `worker-seh64-trampoline-fix/vendor/windows-candidate/build.py` with sha256 `c45a2c6878fb77cd…` — **byte-identical to the file still sitting in that worker's vendor tree today** (re-hashed during this audit).
3. Three more spot-checked source hashes match that worker's vendor tree exactly: `owned_runtime.c` `c505cf9b…`, `sdk_driver.c` `201e8ca5…`, plus build.py above. (`mb_win_seh64.c` differs — expected: the worker kept editing it at 06:11–06:13, after the 05:42 snapshot.)
4. **Timeline bracket:** stray mtime 05:42:47 sits inside the worker's E-run sequence — `out-e1` 05:39:29, `out-e1b` 05:41:36, **stray 05:42:47**, `out-e2` 05:46:12, … worker dir active 05:27–06:30.
5. **Receipt shape is build.py's writer, exactly** (mkdir build-id dir line 42 → write `owned_profile.h` line 58 → `save()` receipt line 118 → on exception `status='FAILED', error=…` line 355).
6. The worker's own REPORT.md/CLAIM.json never mention this staging (self-report: none) — consistent with walkguard's "created by another session".
7. Prior art: `worker-crt-integrate/host-out/c6c6737e…/receipt.json` (2026-10-04 17:08, PRE-stray) shows the SAME vendor-build.py misroot failing on lld-link under `TEAM/.superpowers/…` — the bug class predates the stray; the 05:42 run is simply the instance whose staging dirs happened to get left behind.

**Verdict: worker-seh64-trampoline-fix, executing its own `vendor/windows-candidate/build.py` between out-e1b and out-e2, with `--seh64 --entry --workers`-profile flags and a mis-specified lld-link path.** Inferred (not directly verified): the exact argv — `commands: []` means the argv of the fatal first command was never recorded, and the bogus `bin/gcc-ld` lld-link path must have come from the invoking driver/edition, since the vendor build.py's own default is the correct `lib/rustlib/…` form.

## 3. Shadow mechanics (file:line)

Frozen packaging script: `TEAM/worker-prx-link-gaps/package_prx.py`, sha256 `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313` (re-verified unchanged today; pinned by drivers, §4).

```python
71: BASE = Path(__file__).resolve().parent
74: def _find_root() -> Path:
75:     """Walk up to the workspace root (the vendored copy may sit deeper in
76:      outputs/ than the controller original)."""
77:     p = BASE
78:     for _ in range(6):
79:         if (p / ".superpowers").is_dir():
80:             return p
81:         p = p.parent
82:     raise RuntimeError("workspace root containing .superpowers not found")
85: ROOT = _find_root()
86: SPIKE = ROOT / ".superpowers/native-dependency-spike"
```

- Marker: a directory literally named `.superpowers`, ANYWHERE up to 6 levels above the script dir. No other root validation.
- Normal walk from `TEAM/worker-prx-link-gaps/`: worker dir (no) → TEAM (no `.superpowers` pre-incident) → `outputs/` (no) → workspace root (YES) → `ROOT = what`. Correct.
- Post-05:42 walk: worker dir (no) → **TEAM (stray `.superpowers` — YES, stop)** → `ROOT = TEAM`, one level too deep.
- Everything derived from ROOT then mis-points: `SPIKE` (line 86) and every ROOT-derived `PIN` entry (`ledger`, `composition_dir`, `base_receipt`, `lineage`, `frozen_prx`, `import_contract`, `finalize_sdk`, `make_fself`, `dotnet`, `llvm`, `sdk`, `lua_include`) resolve to nonexistent `TEAM/.superpowers/...` / `TEAM/outputs/...` paths.
- Failure is **fail-closed**: the 12 lineage checks (packaging script's `check(...)` calls, last one `tool_lua_include` at line 411) report GAPs and the script refuses to link ("12 gap(s) recorded; PRX candidate NOT approved"). No wrong-root artifact was ever produced or approved.

## 4. Blast radius

### 4a. Scripts sharing the marker-walk pattern

| Script | Root logic | Affected by stray? |
|---|---|---|
| `TEAM/worker-prx-link-gaps/package_prx.py` (FROZEN, sha `e6fbe6c5…`) | `_find_root()` marker walk, lines 74–85 | **YES — the only marker-walk in the tree** (workspace-wide grep for `_find_root` / `.superpowers.*is_dir`) |
| `TEAM/worker-prx-packaging/package_prx.py` (older edition) | `ROOT = BASE.parents[2]` (line 61), fixed depth | No (sits at depth 1 → parents[2] = `what`) |
| all `outputs/ps5-mod-bridge/**/verify_*.py`, `windows-execution/*`, `native-cppmod-source/build.py` | `ROOT = HERE.parents[2]`, depth-correct under `outputs/ps5-mod-bridge` | No |
| worker-vendored `*/vendor/windows-candidate/build.py` copies | `ROOT=HERE.parents[2]` (build.py:14) | Not shadowed — but this is the **bug class that CREATES strays**: vendored at `TEAM/<worker>/vendor/windows-candidate/`, parents[2] = TEAM. (crt-integrate 2026-10-04 17:08; seh64-trampoline-fix 2026-10-05 05:42 = the stray) |

### 4b. Consumers of the frozen packaging script (load it via importlib, all `check_sha` it)

| Driver | Last run | Post-05:42 status |
|---|---|---|
| `worker-comp6-drybuild/build_runtime_composition6_dry1.py` | 10-04 21:35 | PRE-incident; receipt valid |
| `worker-comp6-driver/build_runtime_composition6.py` | 10-04 23:55 | PRE; valid |
| `worker-d1-determinism-diagnosis/build_runtime_composition6_d1fix.py` | 10-04 23:58 | PRE; valid |
| `worker-launch11-vtable-prebuild/build_runtime_composition6_l11pre.py` | 10-05 02:35 | PRE; valid |
| `worker-launch11-dry-compose` | 10-05 03:56 | PRE; valid |
| `worker-launch10-prebuild/build_runtime_composition6_l10pre.py` | pre-incident run (candidates l10-a/-b valid) | **UNPATCHED — any re-run now fails with the same 12 gaps** |
| `worker-launch10-walkguard-prep/build_runtime_composition6_walkguard.py` | 10-05 06:58–07:08 | **HIT IT LIVE**; recovered via in-memory WGP E4 |
| `worker-launch10-r3-armed/build_runtime_composition6_l10r3.py` | 10-05 07:02–07:17 | **HIT IT LIVE**; recovered via in-memory L10R3 E3 |
| older `composition-3/4-fix/5-rebuild/5-minimal` packagers | 10-04 | PRE; valid |

### 4c. Receipts that already ran with the WRONG root

| Receipt / evidence | Time (local) | Outcome |
|---|---|---|
| `worker-launch10-walkguard-prep/prx-candidate-composition6-l10wg-a.rootgap-firstattempt/` (build-receipt.json `utc: 2026-10-05T07:00:29`, GAPS.json 12 gaps, lineage-verification.json) + `logs/run-a.console.log` (GAP lines 40–51) | 07:00 | fail-closed; nothing linked; preserved |
| `worker-launch10-r3-armed/logs/run-a.console.log` (GAP lines 38+, same 12) | ~07:06 | fail-closed; nothing linked; preserved in log |

**Wrong-root receipts that APPROVED anything: none.** Both victims failed closed before any static check or link. All packaging receipts before 05:42 used the correct root (their drivers ran pre-stray; their `pinned.frozen_prx` etc. resolve under `what/.superpowers/…`).

## 5. Disposition options, ranked

### (a) RELOCATE / RENAME — RECOMMENDED

Preserve every byte, remove the marker dir from `_find_root`'s path. Exact command for the controller:

```sh
mv "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/.superpowers" \
   "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/.superpowers.QUARANTINED-stray-20261005T0542-b33f79f5-worker-seh64-trampoline-fix"
```

(Any name without a `.superpowers` path-component works; keeping the `.superpowers.` prefix keeps it grep-able. Do it while no packaging run is mid-flight.)

Effects: `_find_root` from `TEAM/worker-prx-link-gaps/` walks TEAM (no marker now) → `outputs/` → `what` — correct root; every unpatched driver (launch10-prebuild, l11pre, d1fix, comp6-driver…) works unmodified again; the frozen file stays byte-identical. Optionally, as a SEPARATE deliberate act, the controller can later reconcile the orphan `b33f79f5…` dir into the canonical `.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1/` — not required for the unblock.

### (b) HARDEN `_find_root()` — valid idea, BLOCKED in-place by attestation pins

The frozen file is sha-pinned (`e6fbe6c5…`) by at least 7 drivers (`check_sha(PKG_FIXED, …)` — walkguard driver line 580, launch10-prebuild line 541, and the other comp5/6 lineage drivers), and its bytes are part of past receipts' provenance. An in-place edit breaks every one of those pins and with them the reproducibility story of launch-10/11/walkguard/r3 receipts. If hardening is wanted, it must ship as a NEW versioned file with NEW pins across all drivers. The minimal change (for a future edition, NOT to apply in place):

```diff
--- package_prx.py (frozen e6fbe6c5…)
+++ package_prx_v2.py (new pin)
@@
     p = BASE
     for _ in range(6):
-        if (p / ".superpowers").is_dir():
+        if (p / ".superpowers/native-observations").is_dir():
             return p
```

(`native-observations` exists only at the true root; the stray has only `native-dependency-spike`. Both existing in-memory workarounds remain compatible: WGP E4 re-points `m.ROOT/PIN` directly; L10R3 E3's virtual `__file__` at `ROOT/.superpowers/vendor-origin-l10r3/package_prx.py` still walks up to `ROOT`, which does have `native-observations`.)

### (c) DELETE — controller-owned; not recommended now

The tree is provably low-value: FAILED attempt, `commands: []`, zero artifacts, unique build id, recorded in no ledger. But it is the only surviving provenance of the 05:42 misrun, and worker-seh64-trampoline-fix never acknowledged it. Quarantine (a) achieves the full unblock with zero data loss; delete only after the controller explicitly waives reconciliation.

**Safest for reproducibility of past receipts: (a).** It changes no pinned bytes (frozen script sha still `e6fbe6c5…`), invalidates no RECEIPTS pin, keeps both preserved first-attempt artifacts (`*.rootgap-firstattempt`, run-a logs) meaningful, and needs no driver edits. (b)-in-place would invalidate ~7 driver pins and every receipt that attests those bytes; (c) destroys provenance.

## 6. Walkguard WGP E4 validity under option (a) — VERIFIED

Driver `worker-launch10-walkguard-prep/build_runtime_composition6_walkguard.py` (sha `72519454…`), WGP E4 block (~lines 612–640):

```python
_wrong = Path(m.ROOT)
if _wrong != ROOT:                      # <-- guard
    for _k, _v in list(m.PIN.items()):
        ... m.PIN[_k] = ROOT/_rel; if not _nv.exists(): raise SystemExit('WGP E4: ... refusing')
    if not str(m.SPIKE).startswith(str(_wrong)): raise SystemExit('WGP E4: unexpected module SPIKE location — refusing')
    m.SPIKE = ROOT / '.superpowers/native-dependency-spike'
    m.ROOT = ROOT
    ... writes staging/wgp-e4-root-rebase.json ...
```

- **With the stray present (today):** E4 fires; every repointed PIN path is existence-refused (fail-closed if the true-root artifact went missing); evidence at `staging/wgp-e4-root-rebase.json` + `evidence/wgp-e4-root-rebase.json` records `wrong_root_found_by__find_root = TEAM`, `true_root = what`, and `stray_tree_preserved_untouched`. Proven working: `logs/run-a2.console.log:9` and `run-b.console.log:38` print "WGP E4 root re-base applied", both passes end 17/17 gate PASS.
- **After relocation (a):** `_find_root` returns `ROOT == what`, so `_wrong != ROOT` is False → E4 is a verified **no-op**; the existence-refusal machinery stays armed for any future genuine misroot. VALID.
- **Sibling workaround L10R3 E3** (`worker-launch10-r3-armed/build_runtime_composition6_l10r3.py` ~line 575–587, virtual `m.__file__ = ROOT/'.superpowers/vendor-origin-l10r3/package_prx.py'`): also remains valid post-relocation — the ancestor walk from the virtual path still terminates at the true root (its build-receipt.json `script` field documents the virtual origin: `prx-candidate-composition6-l10r3-a/build-receipt.json:3`), and its `ROOT = Path('…/what')` constant (line 81) is already hard-pinned.
- Only cosmetic caveat: after relocation, E4's recorded `stray_tree_preserved_untouched` path no longer exists live; that JSON is a historical record of the incident state and the quarantine name preserves the provenance thread.
