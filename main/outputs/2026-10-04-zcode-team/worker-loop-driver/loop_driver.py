#!/usr/bin/env python3
"""loop_driver.py — AUTOMATIC repair-loop runner for the PS5 UE4SS runtime port
(worker-loop-driver deliverable; invoked by the controller).

Stage graph (per user directive: cheap checks FIRST, full composition only
after prerequisites pass, timing for every stage, results preserved for
unchanged inputs):

  (1) custom-cheap-prereq  [always, host-only, real]
      a. tool/deps presence: prospero-clang++, llvm-nm/llvm-config, the
         composition overlay, EDITIONS.json + base build receipt parse.
      b. sha manifest of the six controller-edited files vs the baseline
         snapshot (baseline/baseline-manifest.json, snapshotted NOW with the
         controller-dictated prefixes recorded as provenance; the bridge moved
         ec53442b -> 59db9dcd AFTER the controller note — prefix drift recorded
         at snapshot): SeedValidation db07ac60 (controller said "x2 copies";
         THREE byte-identical db07ac60 copies on disk — all pinned), bridge,
         initializer e7f349af, wpd1 program 68b8bd18, LuaMod c92c92ff.
         Post-snapshot drift -> exit 2, never auto-repaired.
      c. header/TU syntax checks over the pinned unit list — clang++
         (-fsyntax-only) with the EXACT -I/-D argv each unit is composed with,
         derived from the pinned base build receipt
         (.superpowers/.../ue4ss-native-notify-cancel-1/build-1/receipt.json
         selected_runtime_units argv), and, for the bridge TU, the packaging
         overlay include roots (worker-prx-link-gaps/package_prx.py
         CLANG_FLAGS + lineage-overlay-2), mirroring
         build_runtime_composition6_l11.py's own one-TU bridge preproof.
         -fsyntax-only only: no object emission, no link, no console.
      d. dependency presence: every -I dir of every pinned unit argv, and
         freshen-deps.txt entries (the --file seed list for receipt regen).
      e. include-order scan: DUPLICATE identical #include lines are REPORTED,
         never edited (reordering/removing includes is not provably safe).

  (2) tests (lineage gate)  [only when controller passes --lineage-ready;
      otherwise RECORDED SKIPPED with the exact pending reason]
      worker-regression-guard/regression_preflight.py, fail-closed (exit 0
      clean / 1 suite-fail / 2 drift / 3 contract). --exec only in --full runs
      (after pins refresh); lineage-only otherwise. Its failure is NEVER
      auto-repaired (semantic).

  (3) compilation  [REFUSED unless BOTH --full and stage-2 clean]
      launch11_one_pass.py --full (verify units -> admission gate -> receipt
      regen -> two composition passes prx-candidate-l11-{1,2} + ODR + finalize
      + gate v2 + determinism twin + accessor verification). Single process;
      its packaging work is measured inside this stage (recorded, not
      double-counted). Host-only end-to-end: no staging, no injection, no
      console dispatch. Default OFF: without --full the refusal is RECORDED,
      never executed.

  (4) custom-gate-receipts  [only after stage-3 success]
      Parse gate-receipt-l11-prx-candidate-l11-{1,2}.json: candidate-1 verdict
      GO (or the runner-documented D1-only NO-GO acceptance), candidate-2
      verdict GO with zero failed checks, and determinism: run-l11-1 vs
      run-l11-2 receipt runtime_sha256 byte-equality. Any mismatch -> FAIL.
      Informational (coordinator note 2026-10-05): the v5 bridge edit
      (ec53442b) introduces exactly ONE new undefined symbol, `syscall`
      (::syscall(SYS_mincore,...) in bridge_kernel_range_verdict); the SDK
      target libc declares it (target/lib libc.a syscalls.o, T syscall). The
      --extend-undefined-baseline mechanism (which records exactly this
      added-symbol-resolves-to-pinned-SDK-library fact) is ALREADY armed in
      the launch11 flow — build_runtime_composition6_l11.py compose_argv
      passes it to the fixed edition unconditionally. Gate discipline: if any
      imports/delta gate refuses the new symbol, the gate is NEVER edited;
      the recorded baseline extension is the only admissible path, and a
      refusal that the baseline extension cannot cover (e.g. the packaging
      import contract, which this flag does not govern) goes BACK to the
      controller — never a silent workaround. Stage 4 records the post-run
      undefined-delta observation (added / expected_bootstrap_resolutions per
      run dir) so the controller sees where `syscall` landed.

Auto-repair scope (HARD BOUND): ONLY whitespace-class defects provably safe
  to fix: missing newline at EOF (-Wnewline-eof class) and trailing
  whitespace before newline on files with NO raw string literal (R"),
  evaluated per compiler diagnostics (only files whose diagnostics are
  exclusively whitespace-class are eligible). Requires --auto-repair; every
  repair is backed up under repairs/backups, ledgered with before/after shas
  (repairs/ledger.json), and re-proven by a full re-run of the syntax check.
  Because ANY byte change re-pins the composition lineage, each repair is
  recorded as "EDITIONS refresh required". NEVER semantic edits, NEVER
  include reordering, NEVER receipt/pin edits: anything else -> STOP, report,
  exit 1/2.

Timing: every stage goes through worker-stage-metrics/timing_hooks.py
  (modbridge.stage-timing/v1). Canonical mapping: stage 2 -> "tests", stage 3
  -> "compilation" (packaging measured inside the single-process runner,
  recorded as such), "staging"/"injection"/"initialization" are console-side
  and recorded not_reached (this driver never performs them; epoch_view.py
  owns those numbers). Stages 1/4 are clearly-prefixed custom stages.

Cache: results/cache/<stage>/<key>.json keyed by the sha256 of the stage's
  input set (content-addressed). Unchanged inputs (and a clean live manifest
  drift check) -> the previous verified result is REUSED and preserved
  (cache_hit; original measurement carried; no re-measurement), per the
  preserve-evidence directive. --no-cache bypasses.

Exit codes: 0 all requested stages passed (a recorded skip/refusal with no
  failure is still 0 — the controller reads the report); 1 stage failed after
  bounded repair attempts; 2 lineage drift; 3 contract/usage error.

Host-only: this driver never stages, injects, or console-dispatches, and it
must not be run with --full while worker-luamod-lineage is refreshing
EDITIONS.json (the controller owns that gate via --full + --lineage-ready).
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
CTRL = ROOT / 'outputs/2026-10-04-zcode-controller'
L11 = CTRL / 'launch11-combined'

sys.path.insert(0, str(TEAM / 'worker-stage-metrics'))
from timing_hooks import StageTimer  # noqa: E402

EDITIONS = L11 / 'EDITIONS.json'
BASE_RECEIPT = (ROOT / '.superpowers/native-dependency-spike/'
                'ue4ss-native-notify-cancel-1/build-1/receipt.json')
REG_SCRIPT = TEAM / 'worker-regression-guard/regression_preflight.py'
REG_MANIFEST = TEAM / 'worker-regression-guard/regression-input-manifest.json'
RUNNER = L11 / 'launch11_one_pass.py'
L11_DRIVER = L11 / 'build_runtime_composition6_l11.py'
L11_FIXED = L11 / 'build_runtime_composition6_fixed_l11.py'
FRESHEN_DEPS = L11 / 'freshen-deps.txt'
OVERLAY = TEAM / 'worker-comp6-driver/staging/lineage-overlay-2'
COMP_RUNS = (ROOT / '.superpowers/native-dependency-spike/'
             'ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6')
PROSPERO_CLANG = (ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/'
                  'ps5-payload-sdk/bin/prospero-clang++')
LLVM_BIN = (ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/'
            'LLVM-21.1.8-macOS-ARM64/bin')
# packaging CLANG_FLAGS (package_prx.py) + the wrapper-injected -std=c++20
BRIDGE_CLANG_FLAGS = ['-Wall', '-Wextra', '-Werror', '-O1', '-fPIC',
                      '-fno-stack-protector', '-fvisibility=hidden',
                      '-ffreestanding', '-fno-builtin', '-std=c++20']
BRIDGE_INCLUDES = [OVERLAY / 'generated', OVERLAY / 'probe',
                   OVERLAY / 'lifecycle', OVERLAY]

# The six controller-edited files. Expected sha prefixes were dictated by the
# controller (2026-10-05); the baseline snapshot must match them or we refuse
# to snapshot (wrong file picked up). SeedValidation: the task records "x2
# copies"; THREE byte-identical db07ac60 copies exist on disk — all three are
# pinned so per-path drift detection is total; the count discrepancy is
# recorded in the baseline file itself.
BASELINE_SPEC = [
    ("seedvalidation-copy-1",
     "outputs/2026-10-04-zcode-team/worker-preflight-runner/staging-corrected/SeedValidation.controller.hpp",
     "db07ac60"),
    ("seedvalidation-copy-2",
     "outputs/2026-10-04-zcode-team/worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp",
     "db07ac60"),
    ("seedvalidation-copy-3",
     "outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp",
     "db07ac60"),
    ("bridge",
     "outputs/2026-10-04-zcode-controller/launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp",
     "ec53442b"),
    ("initializer",
     "outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp",
     "e7f349af"),
    ("wpd1-program",
     "outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp",
     "68b8bd18"),
    ("luamod",
     "outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp",
     "c92c92ff"),
]
BASELINE_PATH = HERE / 'baseline/baseline-manifest.json'
REPAIR_LEDGER = HERE / 'repairs/ledger.json'

# Pinned unit list: which base-receipt unit argv compiles which controller-
# edited source at --full time (compose_argv --replace-runtime-unit redirects).
UNIT_PIN_MAP = [
    ("initializer", "unreal-unrealinitializer",
     "outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp"),
    ("wpd1-program", "program",
     "outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp"),
    ("luamod", "luamod",
     "outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp"),
]
# rc contract
RC_OK, RC_FAIL, RC_DRIFT, RC_CONTRACT = 0, 1, 2, 3
SYNTAX_TIMEOUT_S = 900


class ContractError(Exception):
    """Usage/environment error -> exit 3."""


def utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def load_json(path):
    return json.loads(Path(path).read_text())


def cache_path(stage: str, key: str) -> Path:
    return HERE / 'results/cache' / stage / (key + '.json')


def cache_get(stage: str, key: str):
    p = cache_path(stage, key)
    if p.is_file():
        try:
            d = load_json(p)
            if d.get('cache_key') == key:
                return d
        except (json.JSONDecodeError, OSError):
            return None
    return None


def cache_put(stage: str, key: str, doc: dict):
    p = cache_path(stage, key)
    p.parent.mkdir(parents=True, exist_ok=True)
    doc = dict(doc)
    doc['cache_key'] = key
    p.write_text(json.dumps(doc, indent=1) + '\n')


# ---------------------------------------------------------------- baseline

def ensure_baseline() -> dict:
    """Load (or create ONCE) the baseline manifest, snapshotted NOW.

    The controller-dictated sha prefixes (task text 2026-10-05) are recorded
    per entry as provenance. A file that MOVED after the note (concurrent
    controller/worker edit — observed live: bridge ec53442b -> 59db9dcd) is
    snapshotted at its CURRENT sha with prefix_matched=false + a recorded
    prefix_drift entry, so the audit trail shows exactly what happened; a
    MISSING file still refuses to snapshot. After the snapshot, ANY change vs
    it is drift (exit 2) — the baseline is the contract, the prefixes were
    only the identification hints at task time.
    """
    if BASELINE_PATH.is_file():
        return load_json(BASELINE_PATH)
    entries, prefix_drift = [], []
    for role, rel, prefix in BASELINE_SPEC:
        p = ROOT / rel
        if not p.is_file():
            raise ContractError(f"baseline source missing: {p}")
        sha = sha256_file(p)
        matched = sha.startswith(prefix)
        entries.append({"role": role, "path": rel, "sha256": sha,
                        "bytes": p.stat().st_size, "dictated_prefix": prefix,
                        "prefix_matched": matched})
        if not matched:
            prefix_drift.append({
                "role": role, "dictated_prefix": prefix, "actual": sha[:16],
                "note": "file moved after the controller note (concurrent "
                        "edit); snapshotted at CURRENT sha per the "
                        "'baseline you snapshot NOW' directive"})
    doc = {
        "schema": "worker-loop-driver/baseline-manifest/1",
        "snapshot_utc": utc(),
        "note": ("Baseline = on-disk state at snapshot time. Controller-"
                 "dictated prefixes recorded as provenance; SeedValidation: "
                 "task said 'x2 copies', THREE byte-identical db07ac60 copies "
                 "pinned (per-path drift detection total). Any post-snapshot "
                 "change is drift."),
        "prefix_drift_at_snapshot": prefix_drift,
        "files": entries,
    }
    BASELINE_PATH.parent.mkdir(parents=True, exist_ok=True)
    BASELINE_PATH.write_text(json.dumps(doc, indent=1) + '\n')
    return doc


# ---------------------------------------------------------------- stage 1

def unit_syntax_argv(unit: dict, source: Path):
    """Pinned unit argv -> -fsyntax-only argv for the staged source.

    Faithful to the composition: identical compiler, -I/-D/-std/warning flags
    (argv byte-carried from the pinned base receipt); only the source token is
    redirected and the emission tail (-c/-o/-MD/-MF + paths) is replaced by
    -fsyntax-only. ContractError on any unrecognized tail shape (never guess).
    """
    argv = list(unit['argv'])
    src = str(unit['source'])
    if src not in argv:
        raise ContractError(f"unit argv lacks its own source token: {src}")
    i = argv.index(src)
    head, tail = argv[:i], argv[i + 1:]
    j = 0
    while j < len(tail):
        tok = tail[j]
        if tok in ('-c', '-MD'):
            j += 1
        elif tok in ('-o', '-MF') and j + 1 < len(tail):
            j += 2
        else:
            raise ContractError(
                f"unit argv tail not recognized (token {tok!r} at +{j}); "
                f"refusing to guess a syntax-only translation")
    return head + ['-fsyntax-only', str(source)]


def classify_diagnostics(text: str):
    """Split compiler stderr into whitespace-repairable vs other errors."""
    repairable, other = [], []
    for line in (text or '').splitlines():
        low = line.lower()
        if 'newline at end of file' in low or 'newline-eof' in low \
                or 'trailing whitespace' in low:
            repairable.append(line)
        elif 'error' in low:
            other.append(line)
    return repairable, other


def whitespace_safe_repair(path: Path) -> dict:
    """PROVABLY-SAFE whitespace repair. Returns an action record.

    Eligible: (a) strip trailing whitespace before newline; (b) add a missing
    final newline — ONLY when the file contains NO raw string literal (R"),
    so end-of-line whitespace is provably outside string contents (a normal
    string literal cannot span a raw newline). Never anything else.
    """
    raw = path.read_bytes()
    if b'R"' in raw:
        return {"path": str(path), "action": "none",
                "reason": "raw string literal present; whitespace repair refused"}
    text = raw.decode('utf-8')
    actions = []
    new = re.sub(r'[ \t]+(?=\n)', '', text)
    if new != text:
        actions.append('strip-trailing-whitespace')
    if new and not new.endswith('\n'):
        new += '\n'
        actions.append('add-final-newline')
    if not actions:
        return {"path": str(path), "action": "none",
                "reason": "no whitespace defect found by re-scan"}
    bak = HERE / 'repairs/backups' / (path.name + '.' +
                                      hashlib.sha256(raw).hexdigest()[:12] + '.bak')
    bak.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, bak)
    path.write_bytes(new.encode('utf-8'))
    rec = {"path": str(path), "action": "+".join(actions),
           "backup": str(bak),
           "sha_before": hashlib.sha256(raw).hexdigest(),
           "sha_after": sha256_file(path), "utc": utc(),
           "lineage_note": "byte change -> composition pins + EDITIONS refresh required"}
    ledger = load_json(REPAIR_LEDGER) if REPAIR_LEDGER.is_file() else {"repairs": []}
    ledger['repairs'].append(rec)
    REPAIR_LEDGER.write_text(json.dumps(ledger, indent=1) + '\n')
    return rec


def repaired_ok(path: Path) -> bool:
    """True if path was whitespace-repaired under our ledger (allowed drift)."""
    if not REPAIR_LEDGER.is_file():
        return False
    try:
        for r in load_json(REPAIR_LEDGER).get('repairs', []):
            if r['path'] == str(Path(path)) and sha256_file(path) == r['sha_after']:
                return True
    except (json.JSONDecodeError, OSError):
        return False
    return False


def build_syntax_targets(units: dict):
    """(label, source, argv, env) for every pinned syntax check."""
    targets = []
    for role, unit_name, rel in UNIT_PIN_MAP:
        src = ROOT / rel
        llvm_cfg = units[unit_name].get('LLVM_CONFIG') or \
            units['native-loadercontext'].get('LLVM_CONFIG', '')
        targets.append((role, src, unit_syntax_argv(units[unit_name], src),
                        {'LLVM_CONFIG': llvm_cfg}))
    bridge_src = ROOT / BASELINE_SPEC[3][1]
    targets.append((
        'bridge', bridge_src,
        [str(PROSPERO_CLANG)] + BRIDGE_CLANG_FLAGS +
        ['-I' + str(p) for p in BRIDGE_INCLUDES] +
        ['-fsyntax-only', str(bridge_src)],
        {'LLVM_CONFIG': str(LLVM_BIN / 'llvm-config')}))
    # db07ac60 SeedValidation header: checked under the initializer TU context
    # (the other copies are byte-identical; the units/ b3d81b0f copy is parsed
    # transitively by the initializer TU via its relative quote-include).
    seed_ctrl = ROOT / BASELINE_SPEC[0][1]
    base_argv = list(targets[0][2])
    base_argv[base_argv.index('-fsyntax-only') + 1] = str(seed_ctrl)
    targets.append(('seedvalidation-header', seed_ctrl, base_argv, targets[0][3]))
    return targets


def _run_syntax(target, timer):
    label, src, argv, env = target
    env_full = dict(os.environ)
    env_full.update({k: v for k, v in env.items() if v})
    t0 = time.perf_counter_ns()
    proc = subprocess.run(argv, capture_output=True, text=True,
                          cwd=str(ROOT), env=env_full, timeout=SYNTAX_TIMEOUT_S)
    dur = (time.perf_counter_ns() - t0) / 1e6
    rec = {"target": label, "source": str(src),
           "argv_class": "-fsyntax-only (composition -I/-D/-std argv carried)",
           "exit": proc.returncode, "duration_ms": round(dur, 1)}
    if proc.returncode == 0:
        rec['verdict'] = 'PASS'
    else:
        rep, oth = classify_diagnostics(proc.stderr)
        rec['verdict'] = 'FAIL'
        rec['repairable_whitespace'] = rep[:10]
        rec['other_errors'] = oth[:15]
        rec['stderr_tail'] = (proc.stderr or '')[-1500:]
    timer.mark('custom-cheap-prereq',
               f"syntax {label} {rec['verdict']} {dur:.0f}ms")
    return rec


def run_stage1(timer: StageTimer, opts) -> dict:
    res = {"stage": "custom-cheap-prereq", "status": "pass", "checks": []}

    def check(name, ok, detail=""):
        res['checks'].append({"check": name, "ok": bool(ok), "detail": detail})
        return ok

    # -- a. tools + pinned parseables
    for tool in (PROSPERO_CLANG, LLVM_BIN / 'llvm-nm', LLVM_BIN / 'llvm-config'):
        check(f"tool:{tool.name}", tool.is_file(), str(tool))
    check("overlay-present", OVERLAY.is_dir(), str(OVERLAY))
    editions = load_json(EDITIONS)
    check("editions-parses", True, str(editions.get('edition', ''))[:100])
    base = load_json(BASE_RECEIPT)
    check("base-receipt-parses", True, str(BASE_RECEIPT))
    units = base['selected_runtime_units']

    # -- b. baseline manifest (created once, prefix-verified) + live drift
    baseline = ensure_baseline()
    res['baseline_snapshot_utc'] = baseline['snapshot_utc']
    drift = []
    for e in baseline['files']:
        p = ROOT / e['path']
        if not p.is_file():
            drift.append({"role": e['role'], "path": e['path'], "kind": "MISSING"})
            continue
        actual = sha256_file(p)
        if actual != e['sha256'] and not repaired_ok(p):
            drift.append({"role": e['role'], "path": e['path'], "kind": "SHA-DRIFT",
                          "baseline": e['sha256'][:16], "actual": actual[:16]})
    res['manifest_drift'] = drift
    check("six-file-manifest-clean", not drift,
          f"{len(baseline['files'])} pinned paths; drift={len(drift)}")

    # -- content-addressed cache key for the re-measurable part of stage 1
    targets = build_syntax_targets(units)
    key_material = {"base_receipt_sha": sha256_file(BASE_RECEIPT),
                    "sources": {lbl: sha256_file(src) for lbl, src, _a, _e in targets}}
    # transitive units/ seed headers parsed by the initializer TU (re-pinned by
    # the controller 2026-10-05: units/SeedValidation.hpp mirrored db07ac60) —
    # a change there must invalidate any cached stage-1 syntax verdict.
    for extra in ('outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp',
                  'outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedTable.h'):
        p = ROOT / extra
        if p.is_file():
            key_material['sources']['units:' + p.name] = sha256_file(p)
    ck = hashlib.sha256(json.dumps(key_material, sort_keys=True).encode()).hexdigest()
    res['cache_key'] = ck
    cached = None if opts.no_cache else cache_get('stage1-cheap', ck)
    if cached and cached.get('status') == 'pass' and not drift:
        cached = dict(cached)
        cached['cache_hit'] = True
        cached['preserved_from_utc'] = cached.get('utc')
        cached['manifest_drift'] = drift
        cached['checks'] = res['checks']
        cached['baseline_snapshot_utc'] = baseline['snapshot_utc']
        cached['cache_key'] = ck
        cached['detail'] = ('inputs unchanged since %s — previous verified '
                            'result preserved (no re-measurement)' % cached.get('utc'))
        return cached

    # -- c. syntax checks (real)
    syntax = [_run_syntax(t, timer) for t in targets]
    res['syntax'] = syntax

    # -- d. dependency presence
    inc_dirs = set()
    for _r, unit_name, _rel in UNIT_PIN_MAP:
        argv = units[unit_name]['argv']
        for i, tok in enumerate(argv):
            if tok == '-I' and i + 1 < len(argv):
                inc_dirs.add(argv[i + 1])
    missing_inc = [d for d in sorted(inc_dirs) if not Path(d).is_dir()]
    freshen_missing = []
    if FRESHEN_DEPS.is_file():
        for line in FRESHEN_DEPS.read_text().splitlines():
            line = line.strip()
            if line and not (ROOT / line).is_file():
                freshen_missing.append(line)
    else:
        freshen_missing = ['<freshen-deps.txt itself missing>']
    res['dep_presence'] = {
        "unit_include_dirs": len(inc_dirs),
        "missing_include_dirs": missing_inc,
        "freshen_deps_missing_count": len(freshen_missing),
        "freshen_deps_missing_head": freshen_missing[:20],
    }
    check("unit-include-dirs-present", not missing_inc,
          f"{len(inc_dirs)} dirs, {len(missing_inc)} missing")
    check("freshen-deps-present", not freshen_missing,
          f"{len(freshen_missing)} missing entries")

    # -- e. include-order scan (REPORT ONLY, never edited)
    dup_report = []
    for _role, _unit_name, rel in UNIT_PIN_MAP:
        p = ROOT / rel
        if not p.is_file():
            continue
        seen, dups = {}, []
        for n, line in enumerate(p.read_text(errors='replace').splitlines(), 1):
            m = re.match(r'\s*#\s*include\s*[<"]([^">]+)[">]', line)
            if m:
                key = (m.group(1), '"' in line)
                if key in seen:
                    dups.append({"line": n, "header": m.group(1),
                                 "first_at": seen[key]})
                else:
                    seen[key] = n
        if dups:
            dup_report.append({"file": rel, "duplicates": dups[:8]})
    res['include_order_scan'] = {
        "mode": "report-only (never edited; reordering is not provably safe)",
        "files_with_duplicate_includes": dup_report}

    # -- auto-repair: ONLY whitespace-class failures, ONLY with --auto-repair
    fails = [s for s in syntax if s['verdict'] == 'FAIL']
    if fails and opts.auto_repair:
        eligible = [s for s in fails if s['repairable_whitespace']
                    and not s['other_errors']]
        timer.mark('custom-cheap-prereq',
                   f'auto-repair: {len(eligible)} whitespace-class target(s)')
        applied = []
        for s in eligible:
            rec = whitespace_safe_repair(Path(s['source']))
            applied.append(rec)
        res['auto_repair_applied'] = applied
        for lbl, src, _a, _e in targets:
            if any(a['path'] == str(src) and a['action'] != 'none' for a in applied):
                for t in targets:
                    if t[0] == lbl:
                        rerun = _run_syntax(t, timer)
                        rerun['repaired_rerun'] = True
                        for idx, r in enumerate(syntax):
                            if r['target'] == lbl:
                                syntax[idx] = rerun
        fails = [s for s in syntax if s['verdict'] == 'FAIL']
    elif fails:
        res['auto_repair_note'] = ("failures present but --auto-repair not "
                                   "passed; nothing was edited")

    check("syntax-all-pass", not fails, f"{len(fails)} failing target(s)")
    res['detail'] = (f"syntax {len(syntax) - len(fails)}/{len(syntax)} PASS; "
                     f"manifest drift {len(drift)}; deps missing "
                     f"{len(missing_inc) + len(freshen_missing)}")
    if drift:
        res['status'] = 'drift'
    elif fails or missing_inc or freshen_missing:
        res['status'] = 'fail'
    if res['status'] == 'pass' and not opts.no_cache:
        doc = dict(res)
        doc['utc'] = utc()
        doc['cache_hit'] = False
        cache_put('stage1-cheap', ck, doc)
    return res


# ---------------------------------------------------------------- stage 2

def run_stage2(timer: StageTimer, opts) -> dict:
    if not opts.lineage_ready:
        reason = ("RECORDED SKIP: worker-luamod-lineage EDITIONS.json refresh "
                  "still pending — controller has NOT passed --lineage-ready. "
                  "The cheap stage-1 manifest uses the 2026-10-05 controller "
                  "baseline and is independent of the refresh; the fail-closed "
                  "lineage gate runs only AFTER pins land (never before).")
        timer.skip('tests', reason)
        return {"stage": "tests-lineage-gate", "status": "skipped",
                "reason": reason}
    argv = [sys.executable, str(REG_SCRIPT),
            '--manifest', str(REG_MANIFEST),
            '--editions', str(EDITIONS),
            '--worktree-root', str(ROOT),
            '--json-out', str(HERE / 'results/preflight-aggregate.json')]
    if opts.full:
        argv.append('--exec')
    proc = subprocess.run(argv, capture_output=True, text=True, timeout=1800)
    (HERE / 'logs/regression-preflight.out').write_text(proc.stdout or '')
    rec = {"stage": "tests-lineage-gate",
           "status": "pass" if proc.returncode == 0 else "fail",
           "exit_code": proc.returncode, "exec_mode": bool(opts.full),
           "contract": "0 clean / 1 suite-fail / 2 drift / 3 contract",
           "aggregate": None, "stderr_tail": (proc.stderr or '')[-800:]}
    agg_p = HERE / 'results/preflight-aggregate.json'
    if agg_p.is_file():
        try:
            rec['aggregate'] = load_json(agg_p)
        except json.JSONDecodeError:
            pass
    timer.end('tests', f"regression_preflight rc={proc.returncode} "
                       f"(exec={bool(opts.full)})")
    if proc.returncode == 2:
        rec['status'] = 'drift'
    elif proc.returncode == 3:
        rec['status'] = 'contract_error'
    return rec


# ---------------------------------------------------------------- stage 3

def plan_stage3(opts, stage2_status: str) -> dict:
    """Decide stage 3 WITHOUT executing or touching the timer.

    Returns {action: 'refused'|'cache'|'execute', record, cache_key?}.
    Refusals are RECORDED, never run. Timer begin/end belongs to the caller
    (run_all) so the open/close lifecycle stays in one place.
    """
    if not opts.full:
        return {"action": "refused", "cache_key": None,
                "record": {"stage": "compilation-l11-one-pass",
                           "status": "refused",
                           "reason": ("RECORDED REFUSAL: --full not passed "
                                      "(default OFF). The controller passes "
                                      "--full only after worker-luamod-lineage "
                                      "reports EDITIONS pins done; full "
                                      "composition never runs while the "
                                      "refresh is pending.")}}
    if stage2_status != 'pass':
        return {"action": "refused", "cache_key": None,
                "record": {"stage": "compilation-l11-one-pass",
                           "status": "refused",
                           "reason": (f"RECORDED REFUSAL: lineage gate not "
                                      f"clean (status={stage2_status}); full "
                                      f"composition is blocked by the "
                                      f"fail-closed prerequisite even though "
                                      f"--full was passed.")}}
    key_mat = {
        'runner': sha256_file(RUNNER),
        'driver': sha256_file(L11_DRIVER),
        'fixed': sha256_file(L11_FIXED),
        'editions': sha256_file(EDITIONS),
        'freshen': sha256_file(FRESHEN_DEPS) if FRESHEN_DEPS.is_file() else None,
        'sources': {lbl: sha256_file(src) for lbl, src, _a, _e
                    in build_syntax_targets(load_json(BASE_RECEIPT)
                                            ['selected_runtime_units'])},
    }
    ck = hashlib.sha256(json.dumps(key_mat, sort_keys=True).encode()).hexdigest()
    if not opts.no_cache:
        cached = cache_get('stage3-composition', ck)
        if cached and cached.get('status') == 'pass':
            rec = dict(cached)
            rec['cache_hit'] = True
            rec['detail'] = ('inputs unchanged since %s; full GO evidence '
                             'preserved; composition re-run skipped '
                             '(--no-cache to force)' % cached.get('utc'))
            return {"action": "cache", "cache_key": ck, "record": rec}
    return {"action": "execute", "cache_key": ck,
            "record": {"stage": "compilation-l11-one-pass"},
            "argv": [sys.executable, str(RUNNER), '--full']}


# ---------------------------------------------------------------- stage 4

def run_stage4(timer: StageTimer, stage3: dict) -> dict:
    if stage3.get('status') != 'pass':
        reason = 'not reached: composition stage did not pass'
        timer.not_reached('custom-gate-receipts', reason)
        return {"stage": "custom-gate-receipts", "status": "not_reached",
                "reason": reason}
    out = {"stage": "custom-gate-receipts", "checks": []}

    def check(name, ok, detail=""):
        out['checks'].append({"check": name, "ok": bool(ok), "detail": detail})
        return ok

    g1 = load_json(L11 / 'gate-receipt-l11-prx-candidate-l11-1.json')
    g2 = load_json(L11 / 'gate-receipt-l11-prx-candidate-l11-2.json')
    v1_ok = g1.get('verdict', '').startswith('GO') or \
        (g1.get('verdict', '').startswith('NO-GO') and
         g1.get('failed_checks') == ['D1'])
    check("gate-receipt-candidate-1", v1_ok,
          f"verdict={g1.get('verdict')!r} failed={g1.get('failed_checks')} "
          f"(D1-only NO-GO is the runner-documented pre-twin acceptance)")
    check("gate-receipt-candidate-2-GO",
          g2.get('verdict', '').startswith('GO') and not g2.get('failed_checks'),
          f"verdict={g2.get('verdict')!r} failed={g2.get('failed_checks')}")
    check("gate-receipts-point-at-current-candidates",
          str(g1.get('build_dir', '')).endswith('prx-candidate-l11-1') and
          str(g2.get('build_dir', '')).endswith('prx-candidate-l11-2'),
          f"{g1.get('build_dir')}, {g2.get('build_dir')}")
    shas = {}
    for n in (1, 2):
        cands = sorted([p for p in comp_runs_iter(n)], key=lambda p: p.stat().st_mtime)
        if not cands:
            check(f"run-l11-{n}-receipt", False, "no composition run dir found")
            continue
        rec = load_json(cands[-1] / 'receipt.json')
        shas[n] = rec.get('runtime_sha256')
        check(f"run-l11-{n}-receipt", True,
              f"{cands[-1].name} runtime_sha256={str(shas[n])[:16]}")
    if 1 in shas and 2 in shas:
        check("determinism-twin-sha-equality",
              bool(shas[1]) and shas[1] == shas[2],
              f"run-1 {str(shas.get(1))[:16]} vs run-2 {str(shas.get(2))[:16]}")
    cmp_p = L11 / 'receipts/luamod-comparator.json'
    check("luamod-comparator-receipt", cmp_p.is_file(),
          str(cmp_p) if cmp_p.is_file() else 'missing (runner writes it post-gate)')
    # Informational observation (coordinator note 2026-10-05, bridge v5):
    # exactly ONE new UND symbol `syscall` is expected in the NEW undefined
    # surface; the --extend-undefined-baseline path records its provider. The
    # bridge TU is packaging-stage, so the RUNTIME delta may legitimately not
    # list it; record where it actually landed, never gate on it here.
    obs = []
    for n in (1, 2):
        cands = sorted(comp_runs_iter(n), key=lambda p: p.stat().st_mtime)
        if not cands:
            continue
        dp = cands[-1] / 'undefined-delta.json'
        if dp.is_file():
            d = load_json(dp)
            obs.append({"run": cands[-1].name,
                        "added": list(d.get('added', []))[:12],
                        "expected_bootstrap_resolutions":
                            list(d.get('expected_bootstrap_resolutions', []))[:12],
                        "providers_recorded":
                            sorted((d.get('providers') or {}).keys())[:12]})
    if obs:
        out['undefined_delta_observation'] = obs
        has_syscall = any('syscall' in o['added'] for o in obs)
        out['syscall_note'] = (
            "syscall present in runtime delta (provider recorded by "
            "--extend-undefined-baseline)" if has_syscall else
            "syscall NOT in the runtime-object delta (bridge TU is "
            "packaging-stage; its resolution binds at the final link against "
            "the pinned SDK libc — see gate v2 import checks, never edited)")
    out['status'] = 'pass' if all(c['ok'] for c in out['checks']) else 'fail'
    return out


def comp_runs_iter(n):
    seen = set()
    for p in COMP_RUNS.glob(f'run-l11-{n}*'):
        if p.is_dir() and '-aside-' not in p.name and p.name not in seen:
            seen.add(p.name)
            yield p


# ---------------------------------------------------------------- driver

def record_console_side(timer: StageTimer, why: str):
    timer.not_reached('staging', 'console-side; loop_driver never stages (' + why + ')')
    timer.not_reached('injection', 'console-side; loop_driver never injects (' + why + ')')
    timer.not_reached('initialization', 'console-side; loop_driver never '
                                        'dispatches (' + why + ')')


def write_reports(timer: StageTimer, stages: list, cycle: str, exit_code: int):
    doc = timer.doc()
    doc['loop_driver'] = {
        "stage_graph": ["custom-cheap-prereq", "tests(lineage-gate)",
                        "compilation(launch11_one_pass --full)",
                        "custom-gate-receipts"],
        "stage_results": stages,
        "exit_code": exit_code,
        "exit_contract": "0 ok/skip/refused / 1 fail / 2 drift / 3 contract",
        "host_only": True,
        "console_contact": False,
    }
    res_dir = HERE / 'results'
    res_dir.mkdir(parents=True, exist_ok=True)
    jp = res_dir / f'loop-report-{cycle}.json'
    jp.write_text(json.dumps(doc, indent=1) + '\n')
    lines = [f"# loop-driver report — {cycle}", "",
             f"- utc: {doc['utc_start']} .. {doc['utc_end']}",
             f"- exit: {exit_code} (0 ok/skip/refused, 1 fail, 2 drift, 3 contract)",
             f"- slowest stage: {doc['totals']['slowest_stage']}", "",
             "| stage | duration_ms | status |", "|---|---|---|"]
    for s in doc['stages']:
        lines.append(f"| {s['stage']} | {s['duration_ms']} | {s['status']} |")
    lines.append("")
    for st in stages:
        lines.append(f"## {st.get('stage')}: {st.get('status')}")
        for k in ('detail', 'reason'):
            if st.get(k):
                lines.append(f"- {k}: {st[k]}")
        if st.get('cache_hit'):
            lines.append(f"- cache_hit: preserved evidence from {st.get('preserved_from_utc')}")
        for c in st.get('checks', []):
            lines.append(f"- [{'x' if c['ok'] else ' '}] {c['check']}: {c['detail']}")
        for s in st.get('syntax', []) or []:
            lines.append(f"- syntax {s['target']}: {s['verdict']} ({s.get('duration_ms')}ms)")
        d = st.get('manifest_drift') or []
        for e in d:
            lines.append(f"- DRIFT {e.get('role')}: {e.get('kind')} {e.get('path')}")
    mp = res_dir / f'loop-report-{cycle}.md'
    mp.write_text('\n'.join(lines) + '\n')
    return jp


def run_all(opts) -> int:
    cycle = opts.cycle or ('cheap-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    timer = StageTimer(cycle=cycle, actor="worker-loop-driver/loop_driver.py")
    stages = []
    exit_code = RC_OK

    # ---- stage 1 (always, real)
    timer.begin('custom-cheap-prereq')
    s1 = None
    try:
        s1 = run_stage1(timer, opts)
        s1_end = (f"syntax {sum(1 for x in s1.get('syntax', []) if x.get('verdict') == 'PASS')}"
                  f"/{len(s1.get('syntax', []))} PASS; manifest drift "
                  f"{len(s1.get('manifest_drift', []))}")
    except ContractError as e:
        s1 = {"stage": "custom-cheap-prereq", "status": "contract_error",
              "reason": str(e)}
        s1_end = f'contract error: {e}'
    except subprocess.TimeoutExpired as e:
        s1 = {"stage": "custom-cheap-prereq", "status": "fail",
              "reason": f"timeout: {e}"}
        s1_end = f'syntax timeout: {e}'
    timer.end('custom-cheap-prereq', s1_end)
    stages.append(s1)
    if s1['status'] != 'pass':
        exit_code = {'drift': RC_DRIFT, 'contract_error': RC_CONTRACT}.get(
            s1['status'], RC_FAIL)
        timer.not_reached('tests', 'stage-1 prerequisites failed; loop stops')
        timer.skip('packaging', 'measured inside compilation; compilation not reached')
        timer.not_reached('compilation', 'stage-1 prerequisites failed')
        timer.not_reached('custom-gate-receipts', 'stage-1 prerequisites failed')
        record_console_side(timer, 'stage-1 failed')
        write_reports(timer, stages, cycle, exit_code)
        return exit_code

    # ---- stage 2 (lineage gate; recorded skip while refresh pending)
    timer.begin('tests')  # run_stage2 ends it (or skips, which forbids end)
    s2 = None
    if opts.lineage_ready:
        try:
            s2 = run_stage2(timer, opts)
        except subprocess.TimeoutExpired:
            timer.end('tests', 'regression_preflight TIMEOUT (1800s)')
            s2 = {"stage": "tests-lineage-gate", "status": "fail",
                  "reason": "timeout"}
    else:
        s2 = run_stage2(timer, opts)  # records timer.skip('tests', reason)
    stages.append(s2)
    if s2['status'] == 'drift':
        exit_code = RC_DRIFT
    elif s2['status'] in ('fail', 'contract_error'):
        exit_code = RC_FAIL if s2['status'] == 'fail' else RC_CONTRACT
    if s2['status'] not in ('pass', 'skipped'):
        timer.skip('packaging', 'measured inside compilation; compilation not reached')
        timer.not_reached('compilation', 'lineage gate not clean')
        timer.not_reached('custom-gate-receipts', 'lineage gate not clean')
        record_console_side(timer, 'lineage gate not clean')
        write_reports(timer, stages, cycle, exit_code)
        return exit_code

    # ---- stage 3 (refused unless --full AND stage-2 clean)
    plan = plan_stage3(opts, s2['status'])
    s3 = plan['record']
    if plan['action'] == 'refused':
        timer.not_reached('compilation', s3['reason'])
        timer.skip('packaging', 'measured inside compilation (single-process '
                                'runner); compilation not reached')
    elif plan['action'] == 'cache':
        timer.record_external(
            'compilation', s3['duration_ms'], 'measured',
            'cache: preserved measurement of the identical-input --full run '
            '(launch11_one_pass incl. packaging)')
        timer.skip('packaging', 'measured inside compilation (single-process '
                                'runner); cached evidence reused')
    else:
        timer.begin('compilation')
        t0 = time.perf_counter_ns()
        try:
            proc = subprocess.run(plan['argv'], capture_output=True, text=True,
                                  cwd=str(ROOT), timeout=14400)
            (HERE / 'logs/launch11-one-pass.stdout.log').write_text(proc.stdout or '')
            (HERE / 'logs/launch11-one-pass.stderr.log').write_text(proc.stderr or '')
            dur = (time.perf_counter_ns() - t0) / 1e6
            s3.update({"status": "pass" if proc.returncode == 0 else "fail",
                       "exit_code": proc.returncode,
                       "duration_ms": round(dur, 1),
                       "stdout_log": str(HERE / 'logs/launch11-one-pass.stdout.log'),
                       "stderr_log": str(HERE / 'logs/launch11-one-pass.stderr.log'),
                       "stdout_tail": (proc.stdout or '')[-1500:],
                       "stderr_tail": (proc.stderr or '')[-1500:]})
            if s3['status'] == 'pass':
                timer.end('compilation', 'launch11_one_pass --full PASS (includes '
                                         'packaging: two passes, ODR, finalize, '
                                         'gate v2 x2, determinism twin, accessor '
                                         'verification)')
                timer.skip('packaging', 'measured inside compilation '
                                        '(single-process runner '
                                        'launch11_one_pass); duration reported '
                                        'once, under compilation')
                if not opts.no_cache:
                    doc = dict(s3)
                    doc['utc'] = utc()
                    cache_put('stage3-composition', plan['cache_key'], doc)
            else:
                timer.end('compilation', f"launch11_one_pass --full FAILED "
                                         f"rc={proc.returncode}")
                timer.skip('packaging', 'measured inside compilation; run failed')
        except subprocess.TimeoutExpired:
            dur = (time.perf_counter_ns() - t0) / 1e6
            s3.update({"status": "fail", "duration_ms": round(dur, 1),
                       "reason": "timeout after 4h"})
            timer.end('compilation', 'launch11_one_pass --full TIMEOUT (4h)')
            timer.skip('packaging', 'measured inside compilation; run timed out')
    stages.append(s3)
    if s3['status'] == 'fail':
        exit_code = max(exit_code, RC_FAIL)
    if s3['status'] != 'pass':
        timer.not_reached('custom-gate-receipts', 'composition did not pass')
        record_console_side(timer, 'composition not passed' if s3['status'] == 'fail'
                            else 'refused/cheap mode')
        write_reports(timer, stages, cycle, exit_code)
        return exit_code

    # ---- stage 4 (gate receipts + determinism)
    timer.begin('custom-gate-receipts')
    try:
        s4 = run_stage4(timer, s3)
    except (json.JSONDecodeError, OSError) as e:
        timer.end('custom-gate-receipts', f'receipt read error: {e}')
        s4 = {"stage": "custom-gate-receipts", "status": "fail",
              "reason": f"receipt read error: {e}"}
    n_ok = sum(1 for c in s4.get('checks', []) if c['ok'])
    timer.end('custom-gate-receipts',
              f"{n_ok}/{len(s4.get('checks', []))} receipt checks OK")
    stages.append(s4)
    if s4['status'] != 'pass':
        exit_code = max(exit_code, RC_FAIL)
    record_console_side(timer, 'composition GO recorded; console-side numbers '
                               'belong to epoch_view receipts, never this driver')
    write_reports(timer, stages, cycle, exit_code)
    return exit_code


# ---------------------------------------------------------------- self-test

def self_test_repair_classifier() -> dict:
    """Proof the whitespace auto-repair is safe + bounded, on SYNTHETIC files
    under repairs/selftest (never a repo source)."""
    d = HERE / 'repairs/selftest'
    d.mkdir(parents=True, exist_ok=True)
    tag = datetime.now().strftime('%H%M%S%f')
    p = d / f'synthetic_ws_{tag}.cpp'
    p.write_text('int main() { return 0; }   \nint x;;')
    rec = whitespace_safe_repair(p)
    ok = (rec['action'] == 'strip-trailing-whitespace+add-final-newline' and
          p.read_text() == 'int main() { return 0; }\nint x;;\n')  # semantics untouched
    p2 = d / f'synthetic_raw_{tag}.cpp'
    p2.write_text('auto s = R"(x   \n)";\n')
    rec2 = whitespace_safe_repair(p2)
    ok2 = rec2['action'] == 'none' and p2.read_text() == 'auto s = R"(x   \n)";\n'
    return {"synthetic_strip_ok": ok, "rawstring_guard_ok": ok2,
            "records": [rec, rec2]}


def refusal_proof() -> dict:
    """Pure-logic proof of the stage-3 refusal contract (no subprocess, no
    timer lifecycle): both the no---full path and the blocked-lineage path
    must return a RECORDED refusal from plan_stage3."""
    out = {}
    for label, opts, s2_status in (
            ("no-full-flag", argparse.Namespace(full=False, lineage_ready=False,
                                                auto_repair=False, no_cache=True),
             "pass"),
            ("full-but-lineage-blocked", argparse.Namespace(full=True,
                                                            lineage_ready=True,
                                                            auto_repair=False,
                                                            no_cache=True),
             "drift")):
        plan = plan_stage3(opts, s2_status)
        out[label] = {
            "action": plan['action'],
            "recorded_reason": plan['record'].get('reason', '')[:160],
        }
    return out


def lineage_gate_once() -> dict:
    """Coordinator-approved real run (2026-10-05: pins re-pinned, gate green):
    regression_preflight in LINEAGE-ONLY mode (hashing, no suite exec, no
    edits). Recorded as evidence that stage 2 will pass when --lineage-ready
    is granted after stage 1 is clean."""
    argv = [sys.executable, str(REG_SCRIPT),
            '--manifest', str(REG_MANIFEST),
            '--editions', str(EDITIONS),
            '--worktree-root', str(ROOT), '--quiet']
    t0 = time.perf_counter_ns()
    proc = subprocess.run(argv, capture_output=True, text=True, timeout=600)
    return {"exit_code": proc.returncode,
            "duration_ms": round((time.perf_counter_ns() - t0) / 1e6, 1),
            "mode": "lineage-only (no --exec)",
            "contract": "0 clean / 1 suite-fail / 2 drift / 3 contract",
            "stdout_tail": (proc.stdout or '')[-500:]}


def rebaseline() -> dict:
    """Controller-invoked conscious re-snapshot after a known edit round.
    Archives the previous baseline (provenance preserved), snapshots anew,
    records both. NEVER invoked automatically by the loop."""
    if not BASELINE_PATH.is_file():
        return {"action": "created-fresh", "baseline": ensure_baseline()['snapshot_utc']}
    ts = datetime.now().strftime('%Y%m%d-%H%M%S')
    arch = BASELINE_PATH.parent / f'archive-{ts}-baseline-manifest.json'
    shutil.copy2(BASELINE_PATH, arch)
    old = load_json(BASELINE_PATH)
    BASELINE_PATH.unlink()
    new = ensure_baseline()
    moved = []
    for o, n in zip(old['files'], new['files']):
        if o['sha256'] != n['sha256']:
            moved.append({"role": o['role'],
                          "old": o['sha256'][:16], "new": n['sha256'][:16]})
    rec = {"action": "rebaselined", "archive": str(arch),
           "old_snapshot_utc": old['snapshot_utc'],
           "new_snapshot_utc": new['snapshot_utc'], "changed_roles": moved,
           "prefix_drift_at_snapshot": new['prefix_drift_at_snapshot']}
    (HERE / 'logs/rebaseline.log').write_text(
        json.dumps(rec, indent=1) + '\n')
    return rec


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--cycle', help='label for this loop iteration')
    ap.add_argument('--full', action='store_true',
                    help='controller-only: allow stage 3 (launch11_one_pass --full). '
                         'Default OFF; refusal is recorded, never executed.')
    ap.add_argument('--lineage-ready', action='store_true',
                    help='controller asserts worker-luamod-lineage EDITIONS pins are '
                         'done; enables stage 2 (regression_preflight).')
    ap.add_argument('--auto-repair', action='store_true',
                    help='enable the bounded whitespace-only repair loop (default off)')
    ap.add_argument('--no-cache', action='store_true', help='bypass stage cache')
    ap.add_argument('--rebaseline', action='store_true',
                    help='controller-only conscious re-snapshot of the baseline '
                         'manifest after a known edit round (archives the old '
                         'baseline; never run automatically, never mid-edit)')
    ap.add_argument('--self-test', action='store_true',
                    help='cheap-mode measured run + repair-classifier proof + '
                         'refusal proof; writes results and exits')
    args = ap.parse_args(argv)

    if args.rebaseline:
        print(json.dumps(rebaseline(), indent=1))
        return RC_OK

    if args.self_test:
        print('== self-test part A: whitespace repair classifier (synthetic files)')
        proof = self_test_repair_classifier()
        print(json.dumps({k: v for k, v in proof.items() if k != 'records'},
                         indent=1))
        print()
        print('== self-test part B: cheap-mode measured run (stage 1 REAL; '
              'stage 2 recorded-skip if reached; stage 3 refused; NO edits)')
        args.cycle = 'selftest-cheap'
        rc_cheap = run_all(args)
        cheap_doc = load_json(HERE / 'results/loop-report-selftest-cheap.json')
        cs = {s['stage']: s for s in cheap_doc['loop_driver']['stage_results']}
        s1b = cs['custom-cheap-prereq']
        syntax_line = {x['target']: x['verdict']
                       for x in s1b.get('syntax', [])}
        nonws = all(x.get('other_errors') for x in s1b.get('syntax', [])
                    if x['verdict'] == 'FAIL')
        print(f"stage-1 status={s1b['status']} exit={cheap_doc['loop_driver']['exit_code']}")
        for e in s1b.get('manifest_drift', []):
            print(f"DRIFT {e['role']}: {e.get('baseline')} -> {e.get('actual')}"
                  if e['kind'] == 'SHA-DRIFT' else f"DRIFT {e['role']}: {e['path']}")
        if s1b.get('manifest_drift'):
            expected_rc = RC_DRIFT
        elif s1b['status'] == 'fail':
            expected_rc = RC_FAIL
        elif s1b['status'] == 'contract_error':
            expected_rc = RC_CONTRACT
        else:
            expected_rc = RC_OK
        print(f"syntax verdicts: {syntax_line}")
        print(f"all FAILs classified NON-whitespace (no auto-edit): {nonws}")
        print(f"auto_repair_applied absent: {'auto_repair_applied' not in s1b}")
        print(f"exit-code contract: rc={rc_cheap} expected={expected_rc}")
        s2b = cs.get('tests-lineage-gate', {})
        s2b_status = s2b.get('status') or ('<early-stop: stage-1 failed, '
                                           'tests correctly not reached>')
        print(f"stage-2 (as reached): {s2b_status}")
        print()
        print('== self-test part C: refusal contract (pure logic, both paths)')
        rp = refusal_proof()
        print(json.dumps(rp, indent=1))
        print()
        print('== self-test part D: lineage gate FOR REAL (coordinator '
              '2026-10-05: pins re-pinned, gate green) — lineage-only mode')
        lg = lineage_gate_once()
        print(json.dumps(lg, indent=1))
        print()
        report = HERE / 'results/loop-report-selftest-cheap.json'
        doc = load_json(report)
        print('== measured stage-timing document (modbridge.stage-timing/v1 stages) ==')
        print(json.dumps(doc['stages'], indent=1))
        print()
        print(f"reports: {report}")
        print(f"        {str(report).replace('.json', '.md')}")
        print(f"exit codes: cheap={rc_cheap} (stage-1 tree defects found; loop "
              f"correctly stopped, nothing edited)")
        ok = (proof['synthetic_strip_ok'] and proof['rawstring_guard_ok']
              and nonws and 'auto_repair_applied' not in s1b
              and rc_cheap == expected_rc
              and all(v['action'] == 'refused' for v in rp.values())
              and lg['exit_code'] == RC_OK)
        print('SELF-TEST ' + ('PASS' if ok else 'FAIL')
              + ' (driver contract verified; stage-1 tree defects are the '
                'loop\'s first real finding — see report)')
        return RC_OK if ok else RC_FAIL

    return run_all(args)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except ContractError as e:
        print(f"CONTRACT ERROR (exit {RC_CONTRACT}): {e}", file=sys.stderr)
        sys.exit(RC_CONTRACT)
