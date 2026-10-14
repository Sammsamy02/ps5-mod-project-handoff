#!/usr/bin/env python3
"""Mutation test for b7_source_gate.py (Mac-only). Copies the STAGED B7+H4-on-b5final tree into a scratch
overlay, injects one defect at a time, and asserts the gate FAILS on each mutant and PASSES on the clean
copy. A gate that cannot fail is not a gate. Scratch dir: argv[1] (use the session scratchpad).
Prints how many mutants were checked; exits 1 if any mutant survives or the clean copy fails."""
import re, shutil, subprocess, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
GATE = HERE / 'b7_source_gate.py'
TREE = HERE.parent / 'overlay-b7'   # [round10 B7] overlay root whose candidate dir is the round10 b7/tree (apply_b7.py output)
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'


def run(overlay, *extra):
    p = subprocess.run([sys.executable, '-I', str(GATE), '--overlay', str(overlay), *extra], capture_output=True, text=True)
    return p.returncode, p.stdout


def mutant(scratch, name, fn):
    d = scratch / name
    if d.exists(): shutil.rmtree(d)
    shutil.copytree(TREE, d)
    fn(d / REL)
    return d


def edit(path, f):
    t = path.read_text()
    n = f(t)
    if n == t: raise SystemExit(f'mutation did not change {path}')
    path.write_text(n)


def main():
    scratch = Path(sys.argv[1]); scratch.mkdir(parents=True, exist_ok=True)
    results = []
    rc, out = run(TREE)
    results.append(('clean staged tree passes', rc == 0, out.strip().splitlines()[-1]))

    def m1(r):  # remove E1
        edit(r / 'src/LuaUObject.cpp', lambda t: re.sub(r'        catch \(void\*\)\n        \{.*?\n        \}\n', '', t, count=1, flags=re.S))
    rc, out = run(mutant(scratch, 'm1', m1))
    results.append(('M1 E1 removed (protected_ufunction_call swallows Lua errors)', rc == 1 and 'protected_ufunction_call' in out, out.strip().splitlines()[-1]))

    def m2(r):  # new swallowing catch(...) around a Lua call
        edit(r / 'src/LuaMod.cpp', lambda t: t + '\nstatic void b7_mutant_swallow(lua_State* L)\n{\n    try { lua_getglobal(L, "x"); lua_call(L, 0, 0); }\n    catch (...) { }\n}\n')
    rc, out = run(mutant(scratch, 'm2', m2))
    results.append(('M2 new catch(...) around a Lua call', rc == 1 and 'b7_mutant_swallow' in out, out.strip().splitlines()[-1]))

    def m3(r):  # raise under the lock
        edit(r / 'src/LuaMod.cpp', lambda t: t + '\nstatic void b7_mutant_lock(const LuaMadeSimple::Lua& lua)\n{\n    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};\n    lua.throw_error("x");\n}\n')
    rc, out = run(mutant(scratch, 'm3', m3))
    results.append(('M3 lua.throw_error under m_thread_actions_mutex', rc == 1 and 'FINDING' in out and 'lock scope' in out, out.strip().splitlines()[-1]))

    def m4(r):  # unguarded raw callback that allocates
        edit(r / 'src/LuaMod.cpp', lambda t: t + '\nstatic int b7_mutant_cfunc(lua_State* L)\n{\n    std::string s(lua_tostring(L, 1));\n    lua_pushstring(L, (s + "x").c_str());\n    return 1;\n}\nstatic void b7_mutant_reg(lua_State* L) { lua_pushcfunction(L, b7_mutant_cfunc); }\n')
    rc, out = run(mutant(scratch, 'm4', m4))
    results.append(('M4 new raw Lua callback that allocates, no TRY', rc == 1 and 'b7_mutant_cfunc' in out, out.strip().splitlines()[-1]))

    def m5(r):  # noexcept function that reaches Lua
        edit(r / 'src/LuaMod.cpp', lambda t: t + '\nstatic void b7_mutant_noexcept(lua_State* L) noexcept\n{\n    lua_getglobal(L, "x");\n}\n')
    rc, out = run(mutant(scratch, 'm5', m5))
    results.append(('M5 noexcept function that calls Lua', rc == 1 and 'b7_mutant_noexcept' in out, out.strip().splitlines()[-1]))

    def m6(r):  # remove the game-boundary containment (review 2026-10-12 finding P1)
        edit(r / 'src/ScriptHookThunks.cpp', lambda t: re.sub(r'\n[ \t]*catch \(\.\.\.\) \{ (?:try \{ )?Output::send\(STR\("\[B7-H4\][^\n]*', '', t))
    rc, out = run(mutant(scratch, 'm6', m6))
    results.append(('M6 game-boundary catch(...) removed from ScriptHookThunks.cpp (check E)', rc == 1 and 'game-facing try blocks=3 without catch(...)=3' in out, out.strip().splitlines()[-1]))

    def m7(r):  # [round10 B7] remove E11 (mod_name_of runs inside Lua C functions, under the mod's pcall)
        edit(r / 'src/LuaMod.cpp', lambda t: re.sub(r'            catch \(void\*\)\n            \{\n(?:[^\n]*\n){3}            \}\n(?=            catch \(\.\.\.\)\n            \{\n                out = StringType)', '', t, count=1))
    rc, out = run(mutant(scratch, 'm7', m7))
    results.append(('M7 E11 removed (mod_name_of swallows Lua errors)', rc == 1 and 'mod_name_of' in out, out.strip().splitlines()[-1]))

    rc, out = run(TREE, '--strict-h4')
    results.append(('--strict-h4 fails while the 3 latent engine-boundary sites remain', rc == 1 and 'latent engine-boundary' in out, out.strip().splitlines()[-1]))

    bad = 0
    for name, ok, last in results:
        print(('PASS  ' if ok else 'FAIL  ') + name + '   [' + last + ']')
        bad += (not ok)
    print(f'gate_mutation_test: {len(results)} cases, {bad} failed')
    return 1 if bad or not results else 0


if __name__ == '__main__':
    sys.exit(main())
