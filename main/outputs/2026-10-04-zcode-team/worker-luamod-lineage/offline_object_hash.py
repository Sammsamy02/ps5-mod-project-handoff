#!/usr/bin/env python3
"""worker-luamod-lineage OFFLINE OBJECT-HASH proof tool (proposal-side, host only).

Replicates the SEC32-3 retained-replacement measurement for ONE unit:
the unit's own pinned argv from a composition run receipt, with ONLY the
source / -o / -MF tokens redirected (same discipline the fixed edition uses).
Writes every artifact under worker-luamod-lineage/offline-proof/.
No composition, no packaging, no console, no game claims.

Usage:
  python3 offline_object_hash.py --run-dir <composition run dir> \
      --unit luamod --source <path to .cpp to measure> --label <name>
"""
import argparse, hashlib, json, subprocess, sys, time
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
MY = ROOT / 'outputs/2026-10-04-zcode-team/worker-luamod-lineage'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--run-dir', required=True, type=Path)
    ap.add_argument('--unit', required=True)
    ap.add_argument('--source', required=True, type=Path)
    ap.add_argument('--label', required=True)
    a = ap.parse_args()

    rec = json.loads((a.run_dir / 'receipt.json').read_text())
    unit = rec['selected_runtime_units'][a.unit]
    argv = list(unit['argv'])  # pinned compose argv (172 tokens for luamod)
    si = argv.index(unit['source'])
    oi = argv.index('-o') + 1
    mi = argv.index('-MF') + 1
    drifted = [i for i in range(len(argv)) if argv[i] != unit['argv'][i]]

    out = MY / 'offline-proof' / a.label
    out.mkdir(parents=True, exist_ok=True)
    src_copy = out / Path(a.source).name
    src_copy.write_bytes(Path(a.source).read_bytes())
    obj = out / (a.unit + '.o')
    dep = out / (a.unit + '.d')
    new_argv = list(argv)
    new_argv[si] = str(src_copy)
    new_argv[oi] = str(obj)
    new_argv[mi] = str(dep)
    check = [i for i in range(len(new_argv)) if new_argv[i] != argv[i]]
    assert set(check) == {si, oi, mi}, f'argv drift beyond source/-o/-MF: {check}'

    t0 = time.time()
    import os
    env = dict(os.environ, LLVM_CONFIG=unit['LLVM_CONFIG'])  # same env class the fixed edition passes (SEC32-3 run())
    p = subprocess.run(new_argv, capture_output=True, text=True, env=env)
    dt = time.time() - t0
    result = {
        'label': a.label,
        'unit': a.unit,
        'run_dir': str(a.run_dir),
        'pinned_argv_tokens': len(argv),
        'redirected_tokens': sorted(check),
        'measured_source': str(a.source),
        'source_sha256': sha(src_copy),
        'object': str(obj),
        'object_sha256': sha(obj) if obj.is_file() else None,
        'object_bytes': obj.stat().st_size if obj.is_file() else None,
        'receipt_pin_object_sha256': unit['object_sha256'],
        'matches_receipt_pin': (obj.is_file() and sha(obj) == unit['object_sha256']),
        'compile_exit': p.returncode,
        'stderr_tail': p.stderr[-2000:] if p.stderr else '',
        'seconds': round(dt, 1),
        'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'scope': 'host-side single-TU measurement for comparator regen; no composition/packaging/console/game claim',
    }
    dep_txt = dep.read_text() if dep.is_file() else ''
    result['dependency_file_entries'] = len(dep_txt.replace('\\\n', ' ').split()) - 1 if dep_txt else 0
    (out / 'measurement.json').write_text(json.dumps(result, indent=1) + '\n')
    (out / 'compile.stderr').write_text(p.stderr or '')
    (out / 'compile.stdout').write_text(p.stdout or '')
    (out / 'argv-used.json').write_text(json.dumps(new_argv, indent=0) + '\n')
    print(json.dumps({k: result[k] for k in
        ('label', 'unit', 'source_sha256', 'object_sha256', 'object_bytes',
         'receipt_pin_object_sha256', 'matches_receipt_pin', 'compile_exit', 'seconds')}, indent=1))
    sys.exit(0 if p.returncode == 0 else 1)

if __name__ == '__main__':
    main()
