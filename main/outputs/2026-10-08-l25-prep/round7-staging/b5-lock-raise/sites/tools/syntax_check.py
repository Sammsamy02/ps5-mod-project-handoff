#!/usr/bin/env python3
"""Single-TU -fsyntax-only check of ONE source file with the archived l24 prospero-clang++ argv of the
`luamod` unit (same recipe as syntax_check_luamod.py, generalised): the source replaced by SOURCE.cpp, optional
include overlay dirs put FIRST on the include path (so a staged LuaMadeSimple.hpp wins over the candidate's).
No object, no .d, nothing written inside the candidate tree. NOT a PRX build, not packaging.
One compile at a time; exit 2 (SKIP) below 2 GB free (free + speculative pages).
Usage: syntax_check.py SOURCE.cpp OUT_LOG [--overlay-include DIR]...
Prints exit code, error count and warning count; exit 0 only when the compiler exits 0 AND the OUT_LOG exists."""
import json, os, re, subprocess, sys
from pathlib import Path

def free_gb():
    out = subprocess.run(['vm_stat'], capture_output=True, text=True).stdout
    page = int(re.search(r'page size of (\d+) bytes', out).group(1))
    free = int(re.search(r'Pages free:\s+(\d+)', out).group(1))
    spec = int(re.search(r'Pages speculative:\s+(\d+)', out).group(1))
    return (free + spec) * page / 1e9

def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    src, out_log = argv[0], argv[1]
    overlays = [str(Path(argv[i + 1]).resolve()) for i, a in enumerate(argv) if a == '--overlay-include' and i + 1 < len(argv)]
    g = free_gb()
    print(f'free memory approx {g:.2f} GB')
    if g < 2.0:
        print('SKIP: under 2 GB free'); return 2
    r = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
    o = r / 'outputs/2026-10-08-l26-runtime/typescan-r4'
    comp = json.load(open(r / 'outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json'))
    u = next(x for x in comp['selected_units'] if x['name'] == 'luamod')
    a = u['argv'][:]
    incs = [x for d in overlays for x in ('-I', d)] + \
           [x for inc in (o / 'include', o / 'candidate/include', o / 'candidate/generated_include') for x in ('-I', str(inc))]
    a[1:1] = incs
    i = a.index('-ivfsoverlay')
    a[i:i] = ['-ivfsoverlay', str(o / 'results/headers-ey.vfs.json')]
    original = str(r / 'outputs/2026-10-08-l23-runtime/thread/src/LuaMod.cpp')
    assert original in a
    a[a.index(original)] = str(Path(src).resolve())
    for flag in ('-o', '-MF'):
        k = a.index(flag); del a[k:k + 2]
    a.remove('-MD'); a[a.index('-c')] = '-fsyntax-only'
    a = [('-ferror-limit=50' if x == '-ferror-limit=8' else x) for x in a]
    e = os.environ.copy(); e['LLVM_CONFIG'] = u['LLVM_CONFIG']
    p = subprocess.run(a, env=e, capture_output=True, text=True)
    Path(out_log).write_text(p.stdout + p.stderr)
    errs = len(re.findall(r'\berror:', p.stderr))
    warns = len(re.findall(r'\bwarning:', p.stderr))
    print(f'exit={p.returncode} errors={errs} warnings={warns} checked_sources=1 overlays={len(overlays)} log={out_log}')
    return 0 if p.returncode == 0 and Path(out_log).exists() else 1

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
