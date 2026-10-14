#!/usr/bin/env python3
"""Checks the staged lua_cpp_argv() (v2 patch) against the argv the PS5 probe v2
actually compiled: same flags apart from -o/-MF/prefix-path/driver prefix.
Prints how many units it compared; exits non-zero on zero or any mismatch."""
import json, sys
from pathlib import Path
sys.dont_write_bytecode = True
D = Path(__file__).resolve().parents[3]
real = str(D.parents[2] / '2026-10-08-l26-runtime/build_variants.py')
g = {'__file__': real, '__name__': 'bv_patched'}
exec(compile((D / 'staged-patch/v2/build_variants.patched.py').read_text(), real, 'exec'), g)
g['LUA_C_ABI_PREFIX'] = D / 'host-test-v2/lua_c_abi_prefix.h'
rec = json.loads((D.parents[2] / '2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json').read_text())
n = bad = 0
for u in rec['selected_units']:
    if not u['name'].startswith('luaraw-'): continue
    n += 1
    staged = g['lua_cpp_argv'](u['argv'])
    probe = json.loads((D / 'ps5-xcompile-probe-v2/objects' / (u['name'] + '.argv.json')).read_text())
    strip = lambda a: [x for i, x in enumerate(a) if not (i and a[i - 1] in ('-o', '-MF')) and x not in ('-o', '-MF')]
    s, p = strip(staged), strip(probe)
    # the probe inserted the driver's fresh-unit prefix (overlay + include dirs + -I <src dir>) at argv[1];
    # in the real driver compile_unit inserts it before lua_cpp_argv runs. Remove that slice, then compare exactly.
    k = len(p) - len(s)
    if k < 0 or [p[0]] + p[1 + k:] != s: bad += 1; print('MISMATCH', u['name'])
print(f'units compared: {n}; mismatches: {bad}')
sys.exit(1 if n == 0 or bad else 0)
