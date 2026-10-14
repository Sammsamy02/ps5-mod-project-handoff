#!/usr/bin/env python3
"""syntax_check_tree.py — PS5 prospero-clang++ -fsyntax-only check of runtime units in ANY tree laid out
like the candidate (src/, include/), using the archived l24 argv per unit (same recipe as
round9-staging/b6-direct/syntax_check_candidate.py). The tree's include dir is searched first.
No object written; not a build.

  python3 syntax_check_tree.py --root <tree> [--unit name=src/rel/path.cpp ...] [--log-dir DIR]

Default units: the six runtime units the R9 work touches. A unit name must exist in the l24
COMPOSITION-RECEIPT (its argv is reused). Prints the checked count; exit 1 on any error or zero units."""
import argparse, json, os, re, subprocess, sys
from pathlib import Path

R = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
O = R / 'outputs/2026-10-08-l26-runtime/typescan-r4'
CAND = O / 'candidate'
DEFAULT_UNITS = {
    'unreal-uobjectglobals': 'src/UObjectGlobals.cpp',
    'lua-luauobject': 'src/LuaUObject.cpp',
    'unreal-flat-classlistener': 'src/ClassListener.cpp',
    'luamod': 'src/LuaMod.cpp',
    'program': 'src/UE4SSProgram.cpp',
    'lms-luamadesimple': 'src/LuaMadeSimple/LuaMadeSimple.cpp',
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=str(CAND))
    ap.add_argument('--unit', action='append', default=[], help='name=src/rel/path.cpp (repeatable; replaces the default set)')
    ap.add_argument('--log-dir', default=None)
    a = ap.parse_args()
    root = Path(a.root).resolve()
    units = dict(x.split('=', 1) for x in a.unit) if a.unit else DEFAULT_UNITS
    log_dir = Path(a.log_dir) if a.log_dir else root / '.syntax-logs'
    log_dir.mkdir(parents=True, exist_ok=True)
    comp = json.load(open(R / 'outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json'))
    checked = fails = 0
    for name, rel in units.items():
        src = root / rel
        if not src.is_file():
            print('%-26s MISSING %s' % (name, src)); fails += 1; continue
        u = next((x for x in comp['selected_units'] if x['name'] == name), None)
        if u is None:
            print('%-26s UNKNOWN unit name (not in the l24 receipt)' % name); fails += 1; continue
        argv = u['argv'][:]
        incs = (root / 'include', O / 'include', CAND / 'include', CAND / 'generated_include')
        argv[1:1] = [x for inc in incs for x in ('-I', str(inc))]
        if '-ivfsoverlay' in argv:
            i = argv.index('-ivfsoverlay')
            argv[i:i] = ['-ivfsoverlay', str(O / 'results/headers-ey.vfs.json')]
        orig = next(x for x in argv if x.endswith(('.cpp', '.c')))
        argv[argv.index(orig)] = str(src)
        for flag in ('-o', '-MF'):
            if flag in argv:
                k = argv.index(flag); del argv[k:k + 2]
        if '-MD' in argv: argv.remove('-MD')
        argv[argv.index('-c')] = '-fsyntax-only'
        env = os.environ.copy(); env['LLVM_CONFIG'] = u['LLVM_CONFIG']
        p = subprocess.run(argv, env=env, capture_output=True, text=True)
        log = log_dir / ('syntax-%s.log' % name)
        log.write_text(p.stdout + p.stderr)
        errs = len(re.findall(r'\berror:', p.stderr))
        print('%-26s exit=%d errors=%d log=%s' % (name, p.returncode, errs, log))
        checked += 1
        fails += p.returncode != 0
    print('checked %d units, %d failed (root %s)' % (checked, fails, root))
    return 1 if fails or not checked else 0


if __name__ == '__main__':
    sys.exit(main())
