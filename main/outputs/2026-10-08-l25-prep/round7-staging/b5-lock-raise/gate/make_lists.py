#!/usr/bin/env python3
"""Builds the compiled-file lists the gate runs on (stdlib only; writes only into this directory).
Base list = the file list of the B4 build (evidence/builds-20261012/B4-shim-final-stack/compiled-files.txt, read-only).
  compiled-files-b4.txt     B4 list, candidate LuaMod.cpp replaced by the PRISTINE base copy (sites/base-candidate, sha 4d6bc63b; the live
                            candidate file was patched with final-b5 by another workflow at 10:04)
  compiled-files-b5.txt     B4 list, candidate LuaMod.cpp replaced by the final-b5 copy (patched/.../LuaMod.cpp)
  compiled-files-b5-b6.txt  B4 list, the four B6-touched candidate files replaced by the staged B6 work copies
                            (final-b5 + b6.diff): LuaMod.cpp, LuaPathShim.cpp, LuaMadeSimple.cpp, LuaMadeSimple.hpp
Prints how many entries it read, replaced and wrote; exits 1 when a replacement matched nothing."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
WHAT = os.path.realpath(os.path.join(HERE, '../../../../..'))
B4 = os.path.join(WHAT, 'outputs/2026-10-08-l25-prep/evidence/builds-20261012/B4-shim-final-stack/compiled-files.txt')
CAND = os.path.join(WHAT, 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate')
B6W = os.path.join(WHAT, 'outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work')
B5P = os.path.join(HERE, 'patched/outputs/2026-10-08-l26-runtime/typescan-r4/candidate')
B4P = os.path.join(HERE, '../sites/base-candidate/outputs/2026-10-08-l26-runtime/typescan-r4/candidate')
ENTRIES = ['src/LuaMod.cpp', 'src/LuaPathShim.cpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp', 'include/LuaMadeSimple/LuaMadeSimple.hpp']
def build(name, repl):
    base = [l.rstrip('\n') for l in open(B4) if l.strip()]
    out, n = [], 0
    for p in base:
        hit = None
        for rel, new in repl.items():
            if p == os.path.join(CAND, rel):
                hit = new; break
        if hit:
            if not os.path.isfile(hit):
                print('FAIL missing replacement file', hit); sys.exit(1)
            out.append(hit); n += 1
        else:
            out.append(p)
    if n != len(repl):
        print('FAIL %s: replaced %d of %d' % (name, n, len(repl))); sys.exit(1)
    open(os.path.join(HERE, name), 'w').write('\n'.join(out) + '\n')
    print('%-28s read=%d replaced=%d written=%d' % (name, len(base), n, len(out)))
build('compiled-files-b4.txt', {'src/LuaMod.cpp': os.path.join(B4P, 'src/LuaMod.cpp')})
build('compiled-files-b5.txt', {'src/LuaMod.cpp': os.path.join(B5P, 'src/LuaMod.cpp')})
build('compiled-files-b5-b6.txt', {r: os.path.join(B6W, r) for r in ENTRIES})
