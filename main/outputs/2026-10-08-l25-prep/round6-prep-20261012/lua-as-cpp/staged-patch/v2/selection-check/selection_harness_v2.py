#!/usr/bin/env python3
"""Runs the PATCHED (v2) compute_selection() read-only, with __file__ pointing
at the real l26-runtime dir so every path resolves as in the real driver. The
staged prefix header is not applied yet, so LUA_C_ABI_PREFIX is pointed at the
staged copy in host-test-v2/ (same sha the patch pins).
Usage: [L26_LUA_AS_CPP=1] python3 -I selection_harness_v2.py <patched build_variants.py> <expect: on|off>
Exit 0 only if: no gate recorded False, the affected count is non-zero, and
  on : G-LUACPP-scope recorded True with 33 luaraw units and >0 BASE pins, and
       affected = OFF set + exactly the 33 luaraw units;
  off: no G-LUACPP gate recorded and zero luaraw units selected."""
import json, os, sys
from pathlib import Path
sys.dont_write_bytecode = True
real = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/build_variants.py'
here = Path(__file__).resolve().parent
src = open(sys.argv[1]).read(); expect = sys.argv[2]
g = {'__file__': real, '__name__': 'bv_patched'}
exec(compile(src, real, 'exec'), g)
g['LUA_C_ABI_PREFIX'] = here.parents[2] / 'host-test-v2/lua_c_abi_prefix.h'
gates = []
def record(name, ok, detail=None): gates.append((name, ok, detail))
sel = g['compute_selection'](record)
lua = [x for x in gates if x[0].startswith('G-LUACPP')]
affected = list(sel['affected']); luaraw = sorted(n for n in affected if n.startswith('luaraw-'))
print('LUA_AS_CPP =', g['LUA_AS_CPP'], '| gates recorded:', len(gates), '| affected units:', len(affected), '| luaraw units selected:', len(luaraw),
      '| BASE-pinned deps:', len(sel['base_dep_pins']))
for n, ok, d in lua: print(n, ok, json.dumps({k: v for k, v in d.items() if k != 'declared_import_delta'}))
failed = [n for n, ok, d in gates if ok is False]
print('failed gates:', failed)
(here / f'AFFECTED-{expect.upper()}.txt').write_text('\n'.join(sorted(affected)) + '\n')
problems = []
if not gates: problems.append('zero gates recorded')
if not affected: problems.append('zero affected units')
if failed: problems.append('failed gates ' + repr(failed))
if expect == 'on':
    if not (lua and lua[0][1] is True and lua[0][2]['luaraw_units'] == 33 and lua[0][2]['base_pinned_dependencies'] > 0): problems.append('G-LUACPP-scope not PASS with 33/>0')
    off = here / 'AFFECTED-OFF.txt'
    if off.exists():
        offset = set(off.read_text().split())
        if set(affected) - offset != set(luaraw) or not offset <= set(affected): problems.append('ON set != OFF set + 33 luaraw units')
        else: print(f'ON = OFF ({len(offset)}) + {len(luaraw)} luaraw units: OK')
    else: problems.append('run with off first (AFFECTED-OFF.txt missing)')
else:
    if lua or luaraw: problems.append('opt-in off but Lua-as-C++ gates/units present')
print('RESULT:', 'PASS' if not problems else 'FAIL ' + '; '.join(problems))
sys.exit(1 if problems else 0)
