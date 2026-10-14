#!/usr/bin/env python3
"""Isolated compile proof for the L11PB VT-R1 prebuild (host-only, no console).

1. Disarmed compile: the staged initializer TU with the EXACT composed argv from the
   launch-10 run (190-arg prospero-clang++ command, commands.json unreal-unrealinitializer)
   -> object must compile; recorded sha/bytes.
2. Armed compile: same argv + -DUE4SS_DISPATCH_DATA_BACKEND=1 -> must compile (the
   verdict-independent R1 backend abstraction claim, measured).
3. Disarm token identity: preprocess a minimal TU that includes the port DetourInstance
   chain vs the staged seamed chain (same argv -I/-D set, -E); strip line markers and
   compare -> disarmed ODR-safety is MEASURED, not argued.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
MY = Path(__file__).resolve().parent
STG = MY / 'staging/initializer-vtable-pre'
LOGS = MY / 'logs'
LLVM_CONFIG = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config'

argv_rec = json.loads((LOGS / 'initializer-argv.json').read_text())
argv = [str(a) for a in argv_rec['argv']]
src_idx = next(i for i, a in enumerate(argv) if a.endswith('UnrealInitializer.cpp'))
out_idx = argv.index('-o') + 1
env_argv_without_src = argv[:src_idx] + argv[src_idx + 1:]

def run(tag, extra, src, out_obj):
    a = env_argv_without_src + [str(src)]
    a[out_idx - 1] = str(out_obj)
    # redirect -MF into MY logs dir (never write into another worker's run dir)
    mf_idx = a.index('-MF') + 1
    a[mf_idx] = str(MY / f'logs/initobj-{tag}.d')
    if extra:
        a = a[:1] + extra + a[1:]
    p = subprocess.run(a, capture_output=True, text=True, cwd=str(ROOT),
                       env={'PATH': str(LLVM_CONFIG.parent) + ':/usr/bin:/bin:/usr/sbin:/sbin',
                            'LLVM_CONFIG': str(LLVM_CONFIG), 'HOME': str(Path.home())})
    (LOGS / f'compile-{tag}.stderr').write_text(p.stderr)
    ok = p.returncode == 0 and out_obj.is_file()
    print(f'[{tag}] exit={p.returncode} object={"yes" if out_obj.is_file() else "no"} '
          f'({out_obj.stat().st_size if out_obj.is_file() else 0} bytes)')
    if not ok:
        print(p.stderr[-3000:])
        sys.exit(f'COMPILE PROOF FAILURE [{tag}]')
    return out_obj

# 1. disarmed (staged default)
dis = run('disarmed', [], STG / 'UnrealInitializer.cpp', MY / 'logs/initobj-disarmed.o')
# 2. armed
arm = run('armed', ['-DUE4SS_DISPATCH_DATA_BACKEND=1'], STG / 'UnrealInitializer.cpp',
          MY / 'logs/initobj-armed.o')

import hashlib
print('disarmed object sha256:', hashlib.sha256(dis.read_bytes()).hexdigest())
print('armed   object sha256:', hashlib.sha256(arm.read_bytes()).hexdigest())

# 3. token identity, disarmed
probe_tu = MY / 'logs/token-identity-tu.cpp'
probe_tu.write_text('#include "DetourInstance.hpp"\n#include "DetourSubclasses.hpp"\n', encoding='utf-8')
base = argv  # full argv; the filter below keeps only -I pairs / -D / -std flags
# keep the full flag set the composed TU used: -I pairs, -D defines, -std
keep, i = [], 1
while i < len(base):
    x = base[i]
    if x == '-I':
        keep += ['-I', base[i + 1]]; i += 2
    elif x.startswith('-D') or x.startswith('-std'):
        keep.append(x); i += 1
    else:
        i += 1
def preprocess(tag, include_dir, defines):
    a = [argv[0], '-E'] + keep + defines + ['-I', str(include_dir), str(probe_tu)]
    p = subprocess.run(a, capture_output=True, text=True,
                       env={'PATH': str(LLVM_CONFIG.parent) + ':/usr/bin:/bin',
                            'LLVM_CONFIG': str(LLVM_CONFIG), 'HOME': str(Path.home())})
    (LOGS / f'pp-{tag}.stderr').write_text(p.stderr)
    if p.returncode != 0:
        print(p.stderr[-2000:])
        sys.exit(f'PP FAILURE [{tag}]')
    out = p.stdout
    out = re.sub(r'^#.*$', '', out, flags=re.M)   # strip line markers
    # TOKEN-level comparison: whitespace/newlines left by stripped directives are not
    # tokens; the preprocessed token stream is what ODR and the compiler see.
    return ' '.join(out.split())

PORT = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal'
pp_port = preprocess('port', PORT, [])
pp_staged = preprocess('staged', STG, [])
identical = pp_port == pp_staged
print('disarmed token identity vs port header chain:', 'IDENTICAL' if identical else 'DIFFERENT')
(LOGS / 'pp-port.i').write_text(pp_port, encoding='utf-8', errors='replace')
(LOGS / 'pp-staged.i').write_text(pp_staged, encoding='utf-8', errors='replace')
if not identical:
    # show first divergence
    import difflib
    diff = list(difflib.unified_diff(pp_port.splitlines(), pp_staged.splitlines(), lineterm=''))
    (LOGS / 'pp-diff.txt').write_text('\n'.join(diff[:200]), encoding='utf-8')
    print('divergence sample written to logs/pp-diff.txt')
    sys.exit('DISARM TOKEN IDENTITY FAILED')
# also: armed must DIFFER (sanity: the seam actually selects the backend)
pp_armed = preprocess('armed-staged', STG, ['-DUE4SS_DISPATCH_DATA_BACKEND=1'])
print('armed tokens differ from disarmed:', pp_armed != pp_staged)
if pp_armed == pp_staged:
    sys.exit('ARMED SEAM INERT - seam does nothing, refusing to claim arming capability')
print('ALL COMPILE/DISARM PROOFS PASS')
