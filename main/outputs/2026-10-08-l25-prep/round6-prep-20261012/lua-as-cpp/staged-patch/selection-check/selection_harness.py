# Runs the PATCHED compute_selection() read-only, with __file__ pointing at the
# real l26-runtime dir so every path resolves exactly as in the real driver.
import json, os, sys
sys.dont_write_bytecode = True
real = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/build_variants.py'
src = open(sys.argv[1]).read()
g = {'__file__': real, '__name__': 'bv_patched'}
exec(compile(src, real, 'exec'), g)
gates = []
def record(name, ok, detail=None): gates.append((name, ok, detail))
sel = g['compute_selection'](record)
lua = [x for x in gates if x[0].startswith('G-LUACPP')]
print('LUA_AS_CPP =', g['LUA_AS_CPP'])
print('affected units:', len(sel['affected']), '| luaraw C++ units:', len(sel['lua_cpp_units']), '| lua.hpp define units:', len(sel['lua_hpp_units']), '| BASE-pinned deps:', len(sel['base_dep_pins']))
for n, ok, d in lua: print(n, ok, json.dumps({k: v for k, v in d.items() if k != 'declared_import_delta'}))
print('lua.hpp define units:', ' '.join(sorted(sel['lua_hpp_units'])))
print('failed gates:', [n for n, ok, d in gates if ok is False])
