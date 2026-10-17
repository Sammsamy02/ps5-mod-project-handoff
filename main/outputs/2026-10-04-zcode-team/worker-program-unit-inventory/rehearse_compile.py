#!/usr/bin/env python3
"""worker-program-unit-inventory rehearsal compile driver.

Runs the composition-4 run-3 'program' unit argv verbatim with ONLY:
  - the input source (argv[169]) swapped per rehearsal variant
  - -o (argv[172]) and -MF (argv[175]) redirected into this worker dir
  - optional staged include shadow for the FIRST -I dir (rehearsal B only,
    indexes 2,46,54,64 — same directory repeated in the attested argv)
LLVM_CONFIG env set from the receipt, exactly as build_runtime_composition4_fixed.py does.
Writes only inside this worker dir. Never touches the frozen tree.
"""
import json, os, subprocess, sys, time, hashlib, shutil, pathlib

MY = pathlib.Path(__file__).resolve().parent
WHAT = MY.parents[2]  # .../what  (my dir is outputs/2026-10-04-zcode-team/worker-program-unit-inventory)
assert WHAT.name == 'what', WHAT

AV = json.load(open(MY / 'program-argv.json'))
ARGV = list(AV['argv'])
LLVM_CONFIG = AV['LLVM_CONFIG']
SRC_FROZEN = AV['source']                       # frozen original UE4SSProgram.cpp
SRC_PATCHED = str(WHAT / 'outputs/2026-10-04-zcode-controller/comp6-integration/UE4SSProgram.cpp')
INC_FROZEN = str(WHAT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include')
INC_STAGED = str(MY / 'staged' / 'ue4ss-include')

def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def build_argv(variant):
    a = list(ARGV)
    assert a[169] == SRC_FROZEN and a[170] == '-c' and a[171] == '-o' and a[173] == '-MD' and a[174] == '-MF', 'argv shape drifted'
    if variant in ('control', 'patched-literal'):
        a[169] = SRC_PATCHED if variant == 'patched-literal' else SRC_FROZEN
    elif variant == 'patched-staged':
        a[169] = SRC_PATCHED
        # swap all occurrences of the ue4ss include dir (2,46,54,64) to staged shadow
        swapped = 0
        for i, x in enumerate(a):
            if x == INC_FROZEN:
                a[i] = INC_STAGED; swapped += 1
        assert swapped == 4, swapped
    a[172] = str(MY / f'{variant}.o')
    a[175] = str(MY / f'{variant}.d')
    return a

def run(variant):
    a = build_argv(variant)
    json.dump({'variant': variant, 'argv': a, 'env_LLVM_CONFIG': LLVM_CONFIG},
              open(MY / f'{variant}.commands.json', 'w'), indent=2)
    t0 = time.time()
    p = subprocess.run(a, env=dict(os.environ, LLVM_CONFIG=LLVM_CONFIG),
                       capture_output=True, text=True)
    dt = time.time() - t0
    (MY / f'{variant}.stderr').write_text(p.stderr)
    (MY / f'{variant}.stdout').write_text(p.stdout)
    rec = {'variant': variant, 'exit': p.returncode, 'seconds': round(dt, 3),
           'object': a[172], 'dep_file': a[175]}
    if p.returncode == 0 and os.path.exists(a[172]):
        rec['object_sha256'] = sha256(a[172]); rec['object_bytes'] = os.path.getsize(a[172])
    json.dump(rec, open(MY / f'{variant}.receipt.json', 'w'), indent=2)
    print(json.dumps(rec, indent=2))
    return rec

if __name__ == '__main__':
    run(sys.argv[1])
