#!/usr/bin/env python3
"""Compile the STAGED B7 runtime files once each with the PS5 toolchain and the unit's exact B4 argv
(Mac-only analysis compile: no link, no PRX, objects go to a scratch dir and are never uploaded).

How: the B4 COMPOSITION receipt holds each unit's argv. A clang virtual-file-system overlay (a second
-ivfsoverlay) maps the ORIGINAL candidate source path to the staged copy, so relative includes still
resolve exactly as in the build. -o / -MF are redirected to the scratch dir. One compile at a time;
before each one vm_stat "Pages free" must be >= 2 GB (else that unit is SKIPPED and the run fails).

Usage: ps5_compile_check.py BASE SCRATCH_DIR     (BASE = candidate | b5final | b6; staged tree = staged-patch/b7-h4-on-BASE/tree = B7 runtime edits + H1-H3)
Writes audit/ps5-compile-check-BASE.json (+ .log). Exit 0 only if every unit compiled with rc=0 and 0 errors.
"""
import hashlib, json, subprocess, sys, time
from pathlib import Path
HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[4]
REC = WHAT / 'outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json'
CAND = WHAT / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
MARKERS = {'luamod': [('[LUA-CPP] mode=cxx-throw err_status=', 'utf-16-le'), ('[LUA-CPP] mode=longjmp err_status=', 'utf-16-le'), ('[LUA-CPP] mode=UNKNOWN ran=', 'utf-16-le')],
           'lms-luamadesimple': [('b7-probe-void', 'utf-8')],
           'native-script-hook-thunks': [('[B7-H4] contained a non-std exception', 'utf-16-le')]}
UNITS = {'lua-luauobject': 'src/LuaUObject.cpp', 'lms-luamadesimple': 'src/LuaMadeSimple/LuaMadeSimple.cpp', 'luamod': 'src/LuaMod.cpp',
         'lua-pathshim': 'src/LuaPathShim.cpp', 'lua-luaftext': 'src/LuaFText.cpp',
         'native-script-hook-thunks': 'src/ScriptHookThunks.cpp'}   # H1-H3 (added after review P1)


def free_gb():
    out = subprocess.run(['vm_stat'], capture_output=True, text=True).stdout
    ps = int(out.split('page size of')[1].split()[0]); free = int([l for l in out.splitlines() if l.startswith('Pages free')][0].split()[-1].rstrip('.'))
    return free * ps / 1e9


def main(a):
    if len(a) != 2: print(__doc__); return 2
    base, scratch = a[0], Path(a[1]); scratch.mkdir(parents=True, exist_ok=True)
    tree = HERE.parent / f'staged-patch/b7-h4-on-{base}/tree/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
    rec = json.loads(REC.read_text())
    units = {u['name']: u for u in rec['selected_units']}
    results, log = [], []
    for name, rel in UNITS.items():
        u = units[name]
        orig = Path(u['source']); assert orig == CAND / rel, (orig, rel)
        staged = tree / rel
        gb = free_gb()
        if gb < 2.0:
            results.append({'unit': name, 'status': 'SKIPPED', 'free_gb': round(gb, 2)}); log.append(f'{name}: SKIPPED (free {gb:.2f} GB < 2 GB)'); continue
        vfs = {'version': 0, 'case-sensitive': 'true', 'roots': [{'name': str(orig.parent), 'type': 'directory',
               'contents': [{'name': orig.name, 'type': 'file', 'external-contents': str(staged)}]}]}
        ov = scratch / f'{name}.vfs.json'; ov.write_text(json.dumps(vfs))
        argv = list(u['argv'])
        i = argv.index('-ivfsoverlay'); argv[i + 2:i + 2] = ['-ivfsoverlay', str(ov)]
        argv[argv.index('-o') + 1] = str(scratch / f'{name}.o'); argv[argv.index('-MF') + 1] = str(scratch / f'{name}.d')
        t0 = time.time()
        p = subprocess.run(argv, cwd=str(WHAT), capture_output=True, text=True)
        dt = time.time() - t0
        err = p.stderr
        n_err = sum(1 for l in err.splitlines() if ': error:' in l); n_warn = sum(1 for l in err.splitlines() if ': warning:' in l)
        obj = scratch / f'{name}.o'
        row = {'unit': name, 'source_staged': str(staged.relative_to(WHAT)), 'rc': p.returncode, 'errors': n_err, 'warnings': n_warn, 'seconds': round(dt, 1),
               'object_bytes': obj.stat().st_size if obj.exists() else 0, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest() if obj.exists() else None, 'free_gb_before': round(gb, 2)}
        # the B4 object for size comparison
        b4 = Path(u['object']) if isinstance(u['object'], str) else Path(u['object']['path'])
        row['b4_object_bytes'] = b4.stat().st_size if b4.exists() else None
        if name in MARKERS and obj.exists():
            data = obj.read_bytes()
            row['markers'] = {t: data.count(t.encode(enc)) for t, enc in MARKERS[name]}
        # include-graph check: the staged unit must depend on exactly the same files as the B4 unit
        def deps(path):
            txt = Path(path).read_text(errors='replace').replace('\\\n', ' ')
            return {t for t in txt.split()[1:] if t.startswith('/') and not t.endswith(':')}
        try:
            old = deps(u['dependency']['path'] if isinstance(u['dependency'], dict) else u['dependency']); new = deps(scratch / f'{name}.d')
            row['deps_b4'] = len(old); row['deps_staged'] = len(new)
            row['deps_added'] = sorted(str(x).replace(str(WHAT) + '/', '') for x in new - old)
            row['deps_removed'] = sorted(str(x).replace(str(WHAT) + '/', '') for x in old - new)
        except Exception as e:
            row['deps_error'] = str(e)
        results.append(row); log.append(json.dumps(row))
        if err.strip(): (scratch / f'{name}.stderr').write_text(err)
        print(log[-1], flush=True)
    out = HERE.parent / 'audit' / f'ps5-compile-check-{base}.json'
    out.write_text(json.dumps({'base': base, 'results': results}, indent=1) + '\n')
    (HERE.parent / 'audit' / f'ps5-compile-check-{base}.log').write_text('\n'.join(log) + '\n')
    ok = bool(results) and all(r.get('rc') == 0 and r.get('errors') == 0 and all(v > 0 for v in r.get('markers', {}).values()) and not r.get('deps_added') for r in results)
    print('ps5_compile_check: ' + ('PASS' if ok else 'FAIL') + f' ({sum(1 for r in results if r.get("rc") == 0)} of {len(results)} units compiled)')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
