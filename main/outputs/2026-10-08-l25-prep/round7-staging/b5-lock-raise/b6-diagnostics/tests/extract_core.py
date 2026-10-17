#!/usr/bin/env python3
"""Slices the shipped B6 text out of the STAGED source files so the host test compiles
exactly what the build will compile (no re-typed copy).
usage: extract_core.py OUT_DIR
Writes OUT_DIR/{regionA.inc,regionB.inc,api.inc,glue.inc,pump.inc,shimfail.inc}. Prints how many lines it extracted
from each slice; exits 1 if any slice is missing or empty."""
import os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLICES = [
    ('api.inc',     'work/include/LuaMadeSimple/LuaMadeSimple.hpp', '// [B6-API-BEGIN]',      '// [B6-API-END]'),
    ('regionA.inc', 'work/src/LuaMadeSimple/LuaMadeSimple.cpp',     '// [B6-LEAKCORE-BEGIN]', '// [B6-LEAKCORE-END]'),
    ('regionB.inc', 'work/src/LuaMod.cpp',                          '// [B6-CORE-BEGIN]',     '// [B6-CORE-END]'),
    ('glue.inc',    'work/src/LuaMod.cpp',                          '// [B6-GLUE-BEGIN]',     '// [B6-GLUE-END]'),
    ('shimfail.inc','work/src/LuaPathShim.cpp',                    '// [B6-SHIMFAIL-BEGIN]', '// [B6-SHIMFAIL-END]'),
    ('pump.inc',    'work/src/LuaMod.cpp',                          '// [B6-PUMP-BEGIN]',     '// [B6-PUMP-END]'),
]
def main(argv):
    if len(argv) != 1:
        print(__doc__); return 2
    out = argv[0]; os.makedirs(out, exist_ok=True)
    ok = True
    for name, rel, b, e in SLICES:
        lines = open(os.path.join(root, rel), encoding='utf-8').read().split('\n')
        bi = [i for i, l in enumerate(lines) if b in l]
        ei = [i for i, l in enumerate(lines) if e in l]
        if len(bi) != 1 or len(ei) != 1 or ei[0] <= bi[0] + 1:
            print('FAIL %s: markers begin=%s end=%s' % (name, bi, ei)); ok = False; continue
        body = lines[bi[0]:ei[0] + 1]
        open(os.path.join(out, name), 'w', encoding='utf-8').write('\n'.join(body) + '\n')
        print('%-12s %4d lines from %s:%d-%d' % (name, len(body), rel, bi[0] + 1, ei[0] + 1))
    return 0 if ok else 1
if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
