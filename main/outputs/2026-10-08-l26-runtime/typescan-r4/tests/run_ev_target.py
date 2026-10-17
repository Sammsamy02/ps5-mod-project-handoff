from pathlib import Path
import json, subprocess, os, hashlib

# [L27][EV string-seam] Prospero target compile (agent-EV 2026-10-09, verify
# step). Re-runs the three Z13 recipes proven by the earlier agents against
# the CURRENT seam-fixed sources (every prior receipt predates the 18:19-
# 18:22 edits): unreal-nametypes (ES recipe), luamod (EU recipe) and
# unreal-uobjectglobals (agent-CP FINDXOF recipe). The durable
# results/headers-ev.json overlay = the FINDXOF /tmp overlay (profile-unit
# mappings) merged under the ES overlay (all l26-current candidate headers)
# plus the two seam-fixed Helpers headers the older overlays never mapped —
# without those roots a local smoke resolves <Helpers/String.hpp> and
# <Helpers/Utf16Utf8.hpp> from the unfixed port tree and the Hop-B half of
# the fix is never compiled. All exits must be 0.

p = Path(__file__).resolve().parent.parent
results = p / 'results'

es = json.load(open(results / 'ES-TARGET-COMPILE.json'))
eu = json.load(open(results / 'EU-TARGET-COMPILE.json'))
fx = json.load(open(results / 'L26-FINDXOF-PROSPERO-COMPILE.json'))

recipes = {
    'unreal-nametypes': next(x['argv'] for x in es if any(a.endswith('candidate/src/NameTypes.cpp') for a in x['argv'])),
    'luamod': next(x['argv'] for x in eu if any(a.endswith('candidate/src/LuaMod.cpp') for a in x['argv'])),
    'unreal-uobjectglobals': fx['candidate']['argv'],
}

# ---- durable overlay: /tmp FINDXOF roots, then ES roots (l26-current
#      overrides), then the seam-fixed Helpers roots (last wins). ----
tmp_vfs = Path('/tmp/l26-harden-smoke/headers-l26.json')
roots = []
if tmp_vfs.exists():
    roots += json.load(open(tmp_vfs))['roots']
roots += json.load(open(results / 'headers-es.vfs.json'))['roots']
port_first = Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers')
for name in ('String.hpp', 'Utf16Utf8.hpp'):
    assert (port_first / name).exists()
    roots.append({'type': 'file', 'name': str(port_first / name), 'external-contents': str(p / 'candidate/include/Helpers' / name)})
overlay = {'version': 0, 'roots': roots}
(results / 'headers-ev.json').write_text(json.dumps(overlay, indent=1))

# The seam-fixed files the overlay must pin (guard against silent fallback
# to the unfixed port headers).
for rel in ('candidate/include/Helpers/String.hpp', 'candidate/include/Helpers/Utf16Utf8.hpp',
            'candidate/include/Unreal/NameTypes.hpp', 'candidate/src/NameTypes.cpp',
            'candidate/src/UObjectGlobals.cpp', 'candidate/src/LuaMod.cpp'):
    assert (p / rel).exists(), rel

env = os.environ.copy()
env['LLVM_CONFIG'] = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config'

out = []
for name, argv in recipes.items():
    a = argv[:]
    # Route every one of the recipe's own overlays to the merged durable one.
    a = [str(results / 'headers-ev.json') if i > 0 and a[i - 1] == '-ivfsoverlay' else x for i, x in enumerate(a)]
    # Replace the TU and outputs with the current EV targets.
    src = next(i for i, x in enumerate(a) if x.endswith('.cpp') and '/candidate/src/' in x)
    a[src] = str(p / 'candidate/src' / Path(a[src]).name)
    obj = results / f'ev-{name}.o'
    dep = results / f'ev-{name}.d'
    if '-o' in a:
        a[a.index('-o') + 1] = str(obj)
    if '-MF' in a:
        a[a.index('-MF') + 1] = str(dep)
    r = subprocess.run(a, env=env, capture_output=True, text=True)
    (results / f'ev-{name}.log').write_text(r.stdout + r.stderr)
    out.append(dict(name=name, exit=r.returncode, source=a[src], overlay=str(results / 'headers-ev.json'), argv=a))
    print(name, r.returncode)
    if r.returncode:
        print(r.stderr[-4000:])

(results / 'EV-TARGET-COMPILE.json').write_text(json.dumps(out, indent=2))
assert all(x['exit'] == 0 for x in out), 'EV target compile failed'
print('ev-target: ALL PASS')
