#!/usr/bin/env python3
"""Generate genuine host full/interpreter AOT corlib in an owned dependency copy."""
import sys
sys.dont_write_bytecode = True
import hashlib
import json
import shutil
import subprocess
from build_probe import BASE, environment
from run_mono_host_check import ROOT


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    source = ROOT / 'lib/mono/4.5/mscorlib.dll'
    expected = '86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4'
    if sha(source) != expected:
        raise ValueError('matching real runtime corlib hash drift')
    output = BASE / 'host-check/owner-runtime/lib/mono/4.5'
    output.mkdir(parents=True, exist_ok=True)
    target = output / 'mscorlib.dll'
    aot = output / 'mscorlib.dll.dylib'
    if target.exists() or aot.exists():
        raise ValueError('existing host experiment preserved; use its receipt')
    shutil.copyfile(source, target)
    env = environment()
    env.update(MONO_CFG_DIR=str(ROOT / 'etc'), MONO_PATH=str(output), MONO_CRASH_NOFILE='1')
    command = ['/usr/bin/arch', '-x86_64', str(ROOT / 'bin/mono-sgen64'),
               '--aot=full,interp', str(target)]
    with (BASE / 'evidence/OWNER-CORLIB-AOT.log').open('w') as log:
        result = subprocess.run(command, cwd=output, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=180)
    if result.returncode or not aot.is_file() or sha(target) != expected or sha(source) != expected:
        raise RuntimeError('host corlib AOT failed or input changed')
    receipt = {'verdict': 'REAL_HOST_CORLIB_FULL_INTERP_AOT_PASS',
               'command': command, 'source_sha256': expected, 'aot_sha256': sha(aot),
               'aot_bytes': aot.stat().st_size, 'target_artifact': False,
               'host_runtime': 'Mono6.12.0.206 / 0cbf0e290c3 x86_64 Darwin'}
    (BASE / 'evidence/OWNER-CORLIB-AOT.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
