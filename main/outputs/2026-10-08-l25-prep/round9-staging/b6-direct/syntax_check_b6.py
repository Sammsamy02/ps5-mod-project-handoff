#!/usr/bin/env python3
"""-fsyntax-only check of the staged B6 sources with the archived l24 prospero-clang++
argv per unit (same recipe as round7 sites/tools/syntax_check_luamod.py). The staged
include dir is searched FIRST so the staged LuaMadeSimple.hpp wins. One compile at a
time; no object written; not a build. Prints checked count; exit 1 on any error/zero."""
import json, os, re, subprocess, sys
from pathlib import Path

R = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
O = R / 'outputs/2026-10-08-l26-runtime/typescan-r4'
B = Path(__file__).resolve().parent
UNITS = {'luamod': B / 'src/LuaMod.cpp', 'program': B / 'src/UE4SSProgram.cpp',
         'lms-luamadesimple': B / 'src/LuaMadeSimple/LuaMadeSimple.cpp'}

comp = json.load(open(R / 'outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json'))
checked = fails = 0
for name, src in UNITS.items():
    u = next(x for x in comp['selected_units'] if x['name'] == name)
    a = u['argv'][:]
    incs = (B / 'include', O / 'include', O / 'candidate/include', O / 'candidate/generated_include')
    a[1:1] = [x for inc in incs for x in ('-I', str(inc))]
    if '-ivfsoverlay' in a:
        i = a.index('-ivfsoverlay')
        a[i:i] = ['-ivfsoverlay', str(O / 'results/headers-ey.vfs.json')]
    orig = next(x for x in a if x.endswith('.cpp'))
    a[a.index(orig)] = str(src)
    for flag in ('-o', '-MF'):
        if flag in a:
            k = a.index(flag); del a[k:k + 2]
    if '-MD' in a: a.remove('-MD')
    a[a.index('-c')] = '-fsyntax-only'
    e = os.environ.copy(); e['LLVM_CONFIG'] = u['LLVM_CONFIG']
    p = subprocess.run(a, env=e, capture_output=True, text=True)
    log = B / ('syntax-%s.log' % name)
    log.write_text(p.stdout + p.stderr)
    errs = len(re.findall(r'\berror:', p.stderr))
    print('%-18s exit=%d errors=%d log=%s' % (name, p.returncode, errs, log.name))
    checked += 1
    fails += p.returncode != 0
print('checked %d units, %d failed' % (checked, fails))
sys.exit(1 if fails or not checked else 0)
