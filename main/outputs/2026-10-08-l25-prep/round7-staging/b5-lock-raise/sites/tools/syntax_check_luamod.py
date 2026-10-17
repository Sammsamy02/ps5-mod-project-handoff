#!/usr/bin/env python3
"""Single-TU -fsyntax-only check of a LuaMod.cpp with the archived l24 prospero-clang++ argv
(same recipe as candidate typescan-r4/ey_target_compile.py, but: no object, no .d, no
write inside the candidate tree; output goes to the path given as OUT_LOG).
NOT a PRX build and not packaging. One compile at a time; skips below 2 GB free.
Usage: syntax_check_luamod.py SOURCE.cpp OUT_LOG"""
import json, os, subprocess, sys, re
from pathlib import Path

def free_gb():
    out = subprocess.run(['vm_stat'], capture_output=True, text=True).stdout
    page = int(re.search(r'page size of (\d+) bytes', out).group(1))
    free = int(re.search(r'Pages free:\s+(\d+)', out).group(1))
    spec = int(re.search(r'Pages speculative:\s+(\d+)', out).group(1))
    return (free + spec) * page / 1e9

def main(src, out_log):
    g = free_gb()
    print(f'free memory approx {g:.2f} GB')
    if g < 2.0:
        print('SKIP: under 2 GB free'); return 2
    r = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
    o = r / 'outputs/2026-10-08-l26-runtime/typescan-r4'
    comp = json.load(open(r / 'outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json'))
    u = next(x for x in comp['selected_units'] if x['name'] == 'luamod')
    a = u['argv'][:]
    a[1:1] = [x for inc in (o / 'include', o / 'candidate/include', o / 'candidate/generated_include') for x in ('-I', str(inc))]
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
    print(f'exit={p.returncode} errors={errs} checked_sources=1 log={out_log}')
    return 0 if p.returncode == 0 else 1

if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
