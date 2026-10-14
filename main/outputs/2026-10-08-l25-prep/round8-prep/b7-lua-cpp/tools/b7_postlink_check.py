#!/usr/bin/env python3
"""B7 post-link gate (Mac-only, read-only): proves from the LINKED runtime object that Lua really is
(or is not) compiled as C++, BEFORE packaging. Uses the repo's pinned LLVM 21.1.8 llvm-nm (the one build_runtime.py uses).

  b7_postlink_check.py --expect cpp|c --object FILE.o            single Lua unit (self-test: luaraw-ldo.o)
  b7_postlink_check.py --expect cpp|c --runtime RUNTIME.o [--baseline-undefined undefined.stdout]

What it checks (every count is printed; any zero count fails with exit 2):
  expect=cpp  luaD_rawrunprotected / luaD_throw exist ONLY under their C++ names (the mangled ones),
              the public API keeps C names (lua_pcallk, lua_error, luaL_error, lua_yieldk, LuaLock,
              LuaUnlock), setjmp/longjmp are not referenced, __cxa_throw + __gxx_personality_v0 exist
              (defined in the runtime object or referenced), and -- with --baseline-undefined -- the
              undefined set is the baseline minus exactly {setjmp, longjmp} and nothing added.
  expect=c    the reverse (today's build): C names for the internals, setjmp/longjmp referenced.
Exit 0 PASS, 1 FAIL, 2 zero-count/usage.
"""
import re, subprocess, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[4]
NM = WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
MANGLED_INTERNALS = ['_Z20luaD_rawrunprotectedP9lua_StatePFvS0_PvES1_', '_Z10luaD_throwP9lua_Statei']
C_INTERNALS = ['luaD_rawrunprotected', 'luaD_throw']
PUBLIC_C = ['lua_pcallk', 'lua_error', 'luaL_error', 'lua_yieldk', 'LuaLock', 'LuaUnlock']


def nm(path, *flags):
    p = subprocess.run([str(NM), *flags, str(path)], capture_output=True, text=True)
    if p.returncode != 0: raise SystemExit(f'llvm-nm failed on {path}: {p.stderr[:200]}')
    return p.stdout


def symbols(path, undefined):
    out = nm(path, '-u') if undefined else nm(path, '--defined-only', '-g')
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if parts: names.add(parts[-1])
    return names


def main(a):
    if '--expect' not in a or not ('--object' in a or '--runtime' in a):
        print(__doc__); return 2
    expect = a[a.index('--expect') + 1]
    target = Path(a[a.index('--object') + 1] if '--object' in a else a[a.index('--runtime') + 1])
    single = '--object' in a
    if expect not in ('cpp', 'c') or not target.exists() or not NM.exists():
        print('usage/environment error'); return 2
    defined, undefined = symbols(target, False), symbols(target, True)
    print(f'b7_postlink_check: target={target.name} defined={len(defined)} undefined={len(undefined)} expect={expect}')
    if not defined or not undefined: print('FAIL: zero symbols read'); return 2
    results = []
    want_mangled = expect == 'cpp'
    for n in MANGLED_INTERNALS:
        results.append((f"{'defines' if want_mangled else 'does NOT define'} {n}", (n in defined) == want_mangled))
    for n in C_INTERNALS:
        results.append((f"{'does NOT define' if want_mangled else 'defines'} C-named {n}", (n in defined) != want_mangled))
    if not single:
        for n in PUBLIC_C:
            results.append((f'keeps C name {n}', n in defined))
    for n in ('setjmp', 'longjmp'):
        results.append((f"{'does NOT reference' if want_mangled else 'references'} {n}", (n in undefined) != want_mangled))
    eh = {'__cxa_throw', '__gxx_personality_v0'}
    results.append(('C++ exception runtime present (__cxa_throw, __gxx_personality_v0 defined or referenced)', (eh <= (defined | undefined)) if want_mangled else True))
    if '--baseline-undefined' in a:
        base = Path(a[a.index('--baseline-undefined') + 1])
        bset = {l.split()[-1] for l in base.read_text().splitlines() if l.split()}
        removed, added = bset - undefined, undefined - bset
        exp_removed = {'setjmp', 'longjmp'} if want_mangled else set()
        print(f'  undefined vs baseline: baseline={len(bset)} now={len(undefined)} removed={sorted(removed)} added={sorted(added)}')
        results.append((f'undefined set = baseline minus exactly {sorted(exp_removed)} and nothing added', removed == exp_removed and not added and len(bset) > 0))
    bad = 0
    for what, ok in results:
        print(('  PASS  ' if ok else '  FAIL  ') + what); bad += (not ok)
    print(f'b7_postlink_check: {len(results)} checks, {bad} failed')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
