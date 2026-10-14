from pathlib import Path
import json, subprocess, os

# [LFB] agent-EY target-compile verification (2026-10-09): compiles the five
# units touched by the Lua->FName bridge with the REAL prospero-clang++ and
# the archived l24-build-r2 composition argv (same recipe as target_compile.py
# / EU-TARGET-COMPILE), with the headers-ey.vfs.json overlay that routes
# BOTH header replacements (LuaType/LuaUObject.hpp + Unreal/NameTypes.hpp)
# to the l26 candidates (E3 law). The tree include/ and candidate/include/
# go FIRST on the -I list so <LuaFNameBridge.hpp>, <LuaType/LuaUObject.hpp>
# and <Unreal/NameTypes.hpp> resolve to the l26 candidates for every unit.
r = Path.cwd()
o = r / 'outputs/2026-10-08-l26-runtime/typescan-r4'
comp = json.load(open(r / 'outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json'))

want = {
    'luamod': 'outputs/2026-10-08-l23-runtime/thread/src/LuaMod.cpp',
    'lua-luauobject': 'outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp',
    'lua-luafname': '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaFName.cpp',
    'unreal-nametypes': '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/NameTypes.cpp',
    'unreal-uobjectglobals': 'outputs/2026-10-08-l22-runtime/diagnostics/src/UObjectGlobals.cpp',
}
results = []
for u in comp['selected_units']:
    name = u['name']
    if name not in want:
        continue
    a = u['argv'][:]
    ins = [x for inc in [str(o / 'include'), str(o / 'candidate/include'), str(o / 'candidate/generated_include')] for x in ('-I', inc)]
    a[1:1] = ins
    i = a.index('-ivfsoverlay')
    a[i:i] = ['-ivfsoverlay', str(o / 'results/headers-ey.vfs.json')]
    # swap the ORIGINAL source for the candidate (position-stable, not
    # flag-adjacency dependent)
    original = str(r / want[name])
    candidate_src = {
        'luamod': 'candidate/src/LuaMod.cpp',
        'lua-luauobject': 'candidate/src/LuaUObject.cpp',
        'lua-luafname': 'candidate/src/LuaFName.cpp',
        'unreal-nametypes': 'candidate/src/NameTypes.cpp',
        'unreal-uobjectglobals': 'candidate/src/UObjectGlobals.cpp',
    }[name]
    assert original in a, f'{name}: original source not in argv'
    a[a.index(original)] = str(o / candidate_src)
    # outputs
    if '-o' in a:
        a[a.index('-o') + 1] = str(o / 'results' / f'ey-{name}.o')
    if '-MF' in a:
        a[a.index('-MF') + 1] = str(o / 'results' / f'ey-{name}.d')
    e = os.environ.copy()
    e['LLVM_CONFIG'] = u['LLVM_CONFIG']
    p = subprocess.run(a, env=e, capture_output=True, text=True)
    (o / 'results' / f'ey-{name}.log').write_text(p.stdout + p.stderr)
    results.append(dict(name=name, exit=p.returncode, source=str(o / candidate_src), argv=a))
    print(name, p.returncode)
    if p.returncode:
        print(p.stderr[-6000:])

(o / 'results/EY-TARGET-COMPILE.json').write_text(json.dumps(results, indent=2))
assert all(x['exit'] == 0 for x in results), 'compile failed'
print('EY target compile: ALL PASS')
