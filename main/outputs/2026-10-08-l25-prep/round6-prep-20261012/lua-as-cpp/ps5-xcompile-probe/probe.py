#!/usr/bin/env python3
"""Mac-only PS5 cross-compile PROBE: compile the 33 LuaRaw units as C++.

NOT a PRX build and NOT a build step: no link, no packaging, nothing leaves
this directory. Each luaraw unit of the B4 build (outputs/2026-10-08-l26-
build-variants/l26-A, read-only) is compiled ONCE, sequentially (-j1), with
the proposed C++ argv into ./objects/, after tools/memcheck.sh says >= 2 GB
is available. Then three read-only analyses:

  G-inc  replica of build_variants.py's include-graph gate (:920-930):
         new deps not in the unit's BASE dep set, not under l26-runtime/,
         and whose (name, sha) is not a BASE-approved header identity.
  SYM    import-delta prediction: external undefined symbols of the 33 C++
         objects vs the 33 retained C objects; each new one is checked
         against what runtime-l26-A.o already defines or imports.
  LINK   every C-linkage Lua symbol that a non-Lua object references today
         must have a C++ definition (same base name) in the probe objects.

Prints how many units/symbols it checked and exits non-zero if any count is
zero, any compile fails, or the results file cannot be written.
Usage: python3 -I probe.py [--analyze-only]
"""
import hashlib, json, os, shlex, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[4]  # .../what
L26 = WHAT / 'outputs/2026-10-08-l26-runtime'
BUILD = WHAT / 'outputs/2026-10-08-l26-build-variants/l26-A'
BASE = WHAT / 'outputs/2026-10-08-l25-build-variants/l25-A'
LLVM = WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
NM, FILT = LLVM / 'llvm-nm', '/usr/bin/c++filt'
MEMCHECK = HERE.parent / 'tools/memcheck.sh'
OBJ = HERE / 'objects'
LUA_PREFIXES = ('lua_', 'luaL_', 'luaopen_', 'LuaLock', 'LuaUnlock')

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def deps(p): return [Path(x).absolute() for x in shlex.split(Path(p).read_text().replace('\\\n', ' '))[1:]]

def proposed_argv(u, prefix):
    """BASE C argv -> proposed C++ argv (the staged patch does the same)."""
    a = list(u['argv'])
    assert a[0].endswith('/prospero-clang'), a[0]
    a[0] = a[0] + '++'                                  # adds -frtti -fexceptions + libc++
    assert a.count('-std=c11') == 1
    i = a.index('-std=c11')
    a[i:i + 1] = ['-std=c++23', '-DLUA_COMPILED_AS_CPP', '-Wno-deprecated-enum-enum-conversion']
    # C++ units carry NO -resource-dir (the pinned LLVM has no lib/clang/21/
    # include), so their stddef.h/stdarg.h/limits.h come from the SDK. Keeping
    # the C units' -resource-dir in C++ mode would put clang's builtin headers
    # first instead (probe run 1: 19 such new deps for ldo alone). Drop it so
    # C++ Lua resolves headers exactly like the other C++ units.
    if os.environ.get('PROBE_KEEP_RESOURCE_DIR') != '1':
        assert a.count('-resource-dir') == 1
        j = a.index('-resource-dir'); del a[j:j + 2]
    assert a.count('-c') == 1
    a[a.index('-c'):a.index('-c')] = ['-x', 'c++']      # .c sources compiled as C++
    for flag, val in (('-o', OBJ / (u['name'] + '.o')), ('-MF', OBJ / (u['name'] + '.d'))):
        assert a.count(flag) == 1
        a[a.index(flag) + 1] = str(val)
    # the driver's fresh-unit prefix (overlay + declared include dirs + -I <src dir>)
    a[1:1] = prefix + ['-I', str(Path(u['source']).parent)]
    return a

def fresh_prefix(units_by_name):
    """Recover the driver's fresh-unit prefix from a fresh unit of the same build."""
    a = units_by_name['luamod']['argv']
    base_src_parent = None
    basesel = json.loads((BASE / 'pass1/COMPOSITION-RECEIPT.json').read_text())
    for bu in basesel['selected_units']:
        if bu['name'] == 'luamod': base_src_parent = str(Path(bu['source']).parent)
    assert a[1] == '-ivfsoverlay' and base_src_parent
    for j in range(3, len(a) - 1, 2):
        assert a[j] == '-I', ('unexpected prefix shape', j, a[j])
        if a[j + 1] == base_src_parent: return a[1:j]
    raise SystemExit('could not recover fresh-unit prefix')

def nm_rows(path):
    r = subprocess.run([str(NM), str(path)], capture_output=True, text=True)
    rows = []
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2: rows.append((parts[-2], parts[-1]))
    return rows

_DEMANGLED = {}
def demangle_all(syms):
    """Batch-demangle Itanium names with the system c++filt (-n: ELF names keep '_Z')."""
    todo = sorted({s for s in syms if s.startswith('_Z') and s not in _DEMANGLED})
    if todo:
        r = subprocess.run([FILT, '-n'], input='\n'.join(todo) + '\n', capture_output=True, text=True)
        out = r.stdout.splitlines()
        if r.returncode != 0 or len(out) != len(todo): raise SystemExit('c++filt failed')
        _DEMANGLED.update(zip(todo, out))

def demangle_base(sym):
    demangle_all([sym])
    return _DEMANGLED.get(sym, sym).split('(')[0]

def main():
    analyze_only = '--analyze-only' in sys.argv
    rec = json.loads((BUILD / 'pass1/COMPOSITION-RECEIPT.json').read_text())
    units = {u['name']: u for u in rec['selected_units']}
    lua_units = [u for u in rec['selected_units'] if u['name'].startswith('luaraw-')]
    print(f'units in B4 receipt: {len(units)}; luaraw units: {len(lua_units)}')
    if not lua_units: print('FAIL: zero luaraw units'); return 1
    prefix = fresh_prefix(units)
    OBJ.mkdir(exist_ok=True)
    results = {'build_dir': str(BUILD), 'runtime_object_sha256': sha(BUILD / 'pass1/runtime-l26-A.o'),
               'prefix_from_unit': 'luamod', 'units': {}}
    fails = 0
    for u in lua_units:
        if sha(u['source']) != u['source_sha256']:
            print(f"FAIL {u['name']}: source sha drift"); fails += 1; continue
        argv = proposed_argv(u, prefix)
        row = {'source': u['source'], 'source_sha256': u['source_sha256'], 'argv': argv}
        if not analyze_only:
            m = subprocess.run(['sh', str(MEMCHECK)], capture_output=True, text=True)
            row['memcheck'] = m.stdout.strip()
            if m.returncode != 0:
                print('STOP: ' + m.stdout.strip()); return 3
            # same env as build_runtime.py / build_variants.py (LLVM_CONFIG
            # tells the SDK wrapper where the pinned LLVM 21.1.8 lives)
            env = dict(os.environ, LLVM_CONFIG=str(LLVM / 'llvm-config'), CCACHE_DISABLE='1',
                       PYTHONDONTWRITEBYTECODE='1')
            r = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=600)
            (OBJ / (u['name'] + '.stderr')).write_text(r.stderr)
            (OBJ / (u['name'] + '.argv.json')).write_text(json.dumps(argv, indent=1) + '\n')
            row['exit'] = r.returncode
            row['warnings'] = r.stderr.count('warning:')
            row['errors'] = r.stderr.count('error:')
        else:
            row['exit'] = 0 if (OBJ / (u['name'] + '.o')).exists() else 1
            err = OBJ / (u['name'] + '.stderr')
            if err.exists():
                t = err.read_text(); row['warnings'] = t.count('warning:'); row['errors'] = t.count('error:')
        if row['exit'] != 0: fails += 1
        results['units'][u['name']] = row
        print(f"compile {u['name']:<18} exit={row['exit']} warnings={row.get('warnings','-')}")
    compiled = [n for n, r in results['units'].items() if r['exit'] == 0]
    print(f'compiled OK: {len(compiled)}/{len(lua_units)}')
    if len(compiled) != len(lua_units): print('FAIL: not every luaraw unit compiled as C++'); fails += 1

    # ---- G-inc: include-graph gate replica ---------------------------------
    oldsel = json.loads((BASE / 'SOURCE-SELECTION.json').read_text())
    approved = {(Path(p['path']).name, p['sha256']) for p in oldsel['source_pins']
                if Path(p['path']).suffix in ['.h', '.hpp', '.inc', '.inl']}
    base = json.loads((BASE / 'pass1/COMPOSITION-RECEIPT.json').read_text())
    unitdeps = {bu['name']: deps(bu['dependency']) for bu in base['selected_units']}
    universe = {}
    for ds in unitdeps.values():
        for p in ds: universe[str(p.resolve())] = None
    pinned = {str(Path(p['path']).resolve()): p['sha256'] for p in oldsel['source_pins']}
    gate = {'units_checked': 0, 'new_dep_rows': 0, 'would_fail_units': {}, 'universe_rule_unresolved': []}
    for n in compiled:
        gate['units_checked'] += 1
        ds = deps(OBJ / (n + '.d'))
        oldset = {str(p.resolve()) for p in unitdeps[n]}
        new = [p for p in ds if str(p.resolve()) not in oldset]
        gate['new_dep_rows'] += len(new)
        extra = [str(p) for p in new if not p.resolve().is_relative_to(L26) and (p.name, sha(p)) not in approved]
        if extra: gate['would_fail_units'][n] = extra
        for p in new:
            rp = str(p.resolve())
            in_universe = rp in universe
            same_pin = pinned.get(rp) == sha(p)
            if not (in_universe and same_pin):
                gate['universe_rule_unresolved'].append({'unit': n, 'path': rp, 'in_base_universe': in_universe,
                                                         'pinned_same_sha': same_pin})
    print(f"G-inc replica: units checked={gate['units_checked']} new dep rows={gate['new_dep_rows']} "
          f"units the CURRENT gate would refuse={len(gate['would_fail_units'])} "
          f"paths not covered by the proposed universe rule={len(gate['universe_rule_unresolved'])}")
    results['include_gate'] = gate

    # ---- SYM: import-delta prediction ---------------------------------------
    def ext_undef(objs):
        defined, undef = set(), set()
        for o in objs:
            for t, s in nm_rows(o):
                (undef if t == 'U' else defined).add(s)
        return undef - defined, defined
    old_objs = [u['object'] for u in lua_units]
    new_objs = [OBJ / (n + '.o') for n in compiled]
    old_ext, old_def = ext_undef(old_objs)
    new_ext, new_def = ext_undef(new_objs)
    rt = nm_rows(BUILD / 'pass1/runtime-l26-A.o')
    rt_def = {s for t, s in rt if t != 'U'}
    rt_undef = {s for t, s in rt if t == 'U'}
    added = sorted(new_ext - old_ext)
    demangle_all(added)
    removed = sorted(old_ext - new_ext)
    added_rows = [{'symbol': s, 'demangled': demangle_base(s) if s.startswith('_Z') else s,
                   'defined_in_runtime': s in rt_def, 'already_imported': s in rt_undef} for s in added]
    new_imports = [r['symbol'] for r in added_rows if not r['defined_in_runtime'] and not r['already_imported']]
    # a removed reference disappears from the imports only if no other object still needs it
    other_refs = set()
    lua_obj_names = {Path(o).name for o in old_objs}
    link = json.loads((BUILD / 'pass1/LINK-COMMAND.json').read_text())
    scanned_inputs = 0
    consumer_refs = {}
    for inp in link['ordered_inputs']:
        scanned_inputs += 1
        if Path(inp).name in lua_obj_names: continue
        for t, s in nm_rows(inp):
            if t == 'U':
                other_refs.add(s)
                if s.startswith(LUA_PREFIXES): consumer_refs.setdefault(s, set()).add(Path(inp).name)
    dropped_imports = [s for s in removed if s in rt_undef and s not in other_refs]
    print(f'SYM: C objects external refs={len(old_ext)} C++ objects external refs={len(new_ext)} '
          f'added={len(added)} removed={len(removed)} link inputs scanned={scanned_inputs}')
    print(f'SYM: predicted NEW imports={new_imports} predicted REMOVED imports={dropped_imports}')
    results['symbols'] = {'old_external_refs': len(old_ext), 'new_external_refs': len(new_ext),
                          'added_refs': added_rows, 'removed_refs': removed,
                          'predicted_new_imports': new_imports, 'predicted_removed_imports': dropped_imports,
                          'link_inputs_scanned': scanned_inputs}

    # ---- LINK: consumers of C-linkage Lua symbols ---------------------------
    cxx_defs = {}
    demangle_all(new_def)
    for s in new_def:
        if s.startswith('_Z'):
            b = demangle_base(s)
            if b.startswith(LUA_PREFIXES): cxx_defs.setdefault(b, []).append(s)
    c_defs_left = sorted(s for s in new_def if s.startswith(LUA_PREFIXES))
    unresolved = sorted(s for s in consumer_refs if s not in cxx_defs)
    consumers = sorted({o for objs in consumer_refs.values() for o in objs})
    print(f'LINK: Lua symbols referenced by non-Lua objects={len(consumer_refs)} by {len(consumers)} objects; '
          f'C++ definitions found={len(consumer_refs) - len(unresolved)}; unresolved={unresolved}; '
          f'C-linkage Lua definitions left in C++ objects={len(c_defs_left)}')
    results['linkage'] = {'consumer_symbols': len(consumer_refs), 'consumer_objects': consumers,
                          'unresolved_after_cpp': unresolved, 'c_linkage_lua_defs_left': c_defs_left}
    (HERE / 'PROBE-RESULTS.json').write_text(json.dumps(results, indent=1, default=str) + '\n')
    counts = [len(lua_units), gate['units_checked'], len(new_ext), scanned_inputs, len(consumer_refs)]
    if 0 in counts: print(f'FAIL: a zero count {counts}'); return 1
    if unresolved: fails += 1
    print(('PASS' if not fails else 'FAIL') + f' probe ({fails} failures); results in PROBE-RESULTS.json')
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main())
