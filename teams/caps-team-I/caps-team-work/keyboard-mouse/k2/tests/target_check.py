#!/usr/bin/env python3
"""K2 target check (Mac-only, Team I2). NOT a build: no link, no PRX, no package, no console.

For each changed unit (program = UE4SSProgram.cpp, luamod = LuaMod.cpp) it re-runs the EXACT compile argv of the
latest l26-A build (pass1/objects/<unit>.command.json) twice -- once on k2/base, once on k2/new -- writing the
objects into k2/tests/target-check/ only (the new/include dir is put in front so <Platform/KbmEngineKeys.hpp>
resolves as it will after the integrator copies it into typescan-r4/include). Then:
  1. compile result (exit code + stderr tail) for base and new;
  2. include graph: every dependency of the NEW object that the real build's include-graph gate
     (build_variants.py compile_unit) would refuse = not in the l25-A BASE unit deps, not under
     outputs/2026-10-08-l26-runtime after apply, and not an approved (name, sha256) header identity;
  3. symbols: undefined symbols NEW adds over BASE; each one must be DEFINED by an object of the link (the other
     new object, or the latest composed runtime-l26-A.o) -- otherwise it would be a new import.
Prints the counts it checked; exits 1 on any finding, 2 when it checked nothing.
usage: python3 -I target_check.py [--jobs 2]
"""
import argparse, concurrent.futures, hashlib, json, os, subprocess, sys
from pathlib import Path

W = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = W / 'outputs'
HERE = OUT / '2026-10-08-l26-runtime'
L26A = OUT / '2026-10-08-l26-build-variants/l26-A'
L25A = OUT / '2026-10-08-l25-build-variants/l25-A'
LLVM = OUT / 'ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
K2 = Path(__file__).resolve().parents[1]
TC = K2 / 'tests/target-check'
UNITS = {'program': 'candidate/src/UE4SSProgram.cpp', 'luamod': 'candidate/src/LuaMod.cpp'}
# where each k2/new file lands after apply (for the include-graph rule "under HERE is allowed")
APPLY_MAP = {str(K2 / 'new/include'): str(HERE / 'typescan-r4/include'),
             str(K2 / 'new/candidate'): str(HERE / 'typescan-r4/candidate')}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def deps(d):
    t = Path(d).read_text().replace('\\\n', ' ')
    return [x for x in t.split(':', 1)[1].split() if x]


def compile_one(unit, side):
    argv = json.load(open(L26A / f'pass1/objects/{unit}.command.json'))['argv'][:]
    src = K2 / side / UNITS[unit]
    live = HERE / 'typescan-r4' / UNITS[unit]
    i = argv.index(str(live))
    argv[i] = str(src)
    obj = TC / f'{unit}-{side}.o'
    dep = TC / f'{unit}-{side}.d'
    argv[argv.index('-o') + 1] = str(obj)
    argv[argv.index('-MF') + 1] = str(dep)
    argv[1:1] = ['-I', str(K2 / 'new/include')] if side == 'new' else []
    env = os.environ.copy()
    env['LLVM_CONFIG'] = str(LLVM / 'llvm-config')
    p = subprocess.run(argv, env=env, capture_output=True, text=True)
    (TC / f'{unit}-{side}.log').write_text(p.stdout + p.stderr)
    return unit, side, p.returncode, obj, dep


def nm(args, path):
    p = subprocess.run([str(LLVM / 'llvm-nm'), *args, str(path)], capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError('llvm-nm failed on %s: %s' % (path, p.stderr[-400:]))
    return p.stdout


def undefined(path):
    return {l.split()[-1] for l in nm(['-u'], path).splitlines() if l.strip()}


def defined(path):
    out = set()
    for l in nm(['--defined-only'], path).splitlines():
        parts = l.split()
        if len(parts) >= 3: out.add(parts[-1])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=2)
    a = ap.parse_args()
    TC.mkdir(parents=True, exist_ok=True)
    jobs = [(u, s) for u in UNITS for s in ('base', 'new')]
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(max(a.jobs, 1), 4)) as ex:
        for unit, side, rc, obj, dep in ex.map(lambda j: compile_one(*j), jobs):
            results[(unit, side)] = (rc, obj, dep)
            print('compile %-8s %-4s exit=%d log=%s' % (unit, side, rc, (TC / f'{unit}-{side}.log').relative_to(K2)), flush=True)
    findings = []
    for k, (rc, _, _) in results.items():
        if rc: findings.append('compile failed: %s/%s' % k)
    if findings:
        for f in findings: print('FINDING ' + f)
        return 1

    # 2. include graph (same rule as build_variants.py compile_unit, against the l25-A BASE unit deps)
    base_receipt = json.load(open(L25A / 'pass1/COMPOSITION-RECEIPT.json'))
    sel = json.load(open(L25A / 'SOURCE-SELECTION.json'))
    approved = {(Path(p['path']).name, p['sha256']) for p in sel['source_pins'] if Path(p['path']).suffix in ('.h', '.hpp', '.inc', '.inl')}
    checked_deps = 0
    for unit in UNITS:
        bu = next(u for u in base_receipt['selected_units'] if u['name'] == unit)
        oldset = {str(Path(p).resolve()) for p in deps(bu['dependency'])}
        for p in deps(results[(unit, 'new')][2]):
            checked_deps += 1
            rp = str(Path(p).resolve())
            for src, dst in APPLY_MAP.items():
                if rp.startswith(src + '/'): rp = dst + rp[len(src):]
            if rp in oldset or Path(rp).is_relative_to(HERE): continue
            if Path(p).exists() and (Path(p).name, sha(p)) in approved: continue
            findings.append('include graph: %s would be refused (not in the BASE deps of %s, not under the l26 runtime, not an approved identity)' % (p, unit))
    print('include-graph: units=%d deps_checked=%d refused=%d' % (len(UNITS), checked_deps, sum('include graph' in f for f in findings)))

    # 3. symbols: what NEW adds over BASE must be defined inside the link
    new_defined = set()
    for unit in UNITS: new_defined |= defined(results[(unit, 'new')][1])
    runtime_obj = L26A / 'pass1/runtime-l26-A.o'
    runtime_defined = defined(runtime_obj)
    runtime_undefined = {l.split()[-1] for l in (L26A / 'pass1/undefined.stdout').read_text().splitlines() if l.strip()}
    added_total = 0
    for unit in UNITS:
        added = sorted(undefined(results[(unit, 'new')][1]) - undefined(results[(unit, 'base')][1]))
        added_total += len(added)
        for s in added:
            where = 'new-objects' if s in new_defined else ('runtime-l26-A.o' if s in runtime_defined else ('ALREADY-IMPORTED' if s in runtime_undefined else None))
            print('  %-8s +U %s -> %s' % (unit, s, where or 'NOT DEFINED IN THE LINK'))
            if where is None: findings.append('symbol %s (from %s) is defined nowhere in the link: it would be a new import' % (s, unit))
            elif where == 'ALREADY-IMPORTED': print('    (already an import of the runtime: import set unchanged)')
    print('symbols: units=%d added_undefined=%d runtime_defined=%d runtime_imports=%d' % (len(UNITS), added_total, len(runtime_defined), len(runtime_undefined)))
    for f in findings: print('FINDING ' + f)
    if checked_deps == 0 or not runtime_defined: print('target_check: FAIL (nothing checked)'); return 2
    print('target_check: ' + ('PASS' if not findings else 'FAIL'))
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
