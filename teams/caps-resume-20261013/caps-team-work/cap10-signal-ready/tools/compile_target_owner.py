#!/usr/bin/env python3
"""Compile the changed owner against genuine target headers; never link or execute a game."""
from pathlib import Path
import hashlib, json, os, subprocess

BASE = Path(__file__).resolve().parents[1]
TEAM = BASE.parent
WORK = TEAM / 'cap10-port-closure/target-work'
local = json.loads((TEAM / 'cap10-port-closure/LOCAL-INPUTS.json').read_text())
SDK = Path(local['sdk'])
LLVM = Path(local['llvm_bin'])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    receipt = BASE / 'evidence/target-owner-compile.json'
    assert not receipt.exists(), 'Preserve completed evidence.'
    source = BASE / 'native/MonoOwner.cpp'
    obj = BASE / 'runtime-target/MonoOwner.o'
    inputs = {str(path): sha(path) for path in [source, BASE / 'native/MonoOwner.hpp', TEAM / 'cap10/native/InvocationBoundary.hpp', TEAM / 'cap10/native/Cap10InvocationObservation.hpp', TEAM / 'cap10/native/ProbeImage.hpp', WORK / 'config.h', WORK / 'mono/eglib/eglib-config.h', WORK / 'mono/mini/jit.h']}
    command = [str(SDK / 'bin/prospero-clang++'), '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-c', '-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1', '-DMODBRIDGE_CAP10_CORLIB_DIR="/cap10/runtime/lib"', '-DMODBRIDGE_CAP10_CONFIG_DIR="/cap10/runtime/etc"', '-I' + str(BASE / 'native'), '-I' + str(TEAM / 'cap10/native'), '-I' + str(WORK), '-I' + str(WORK / 'mono/eglib'), str(source), '-o', str(obj)]
    env = dict(os.environ, LLVM_CONFIG=str(LLVM / 'llvm-config'))
    result = subprocess.run(command, cwd=BASE, env=env, text=True, capture_output=True)
    (BASE / 'logs/target-owner-compile.log').write_text(result.stdout + result.stderr)
    proof = {'verdict': 'TARGET_OWNER_COMPILE_PASS' if result.returncode == 0 else 'TARGET_OWNER_COMPILE_FAIL', 'command': command, 'exit_code': result.returncode, 'input_sha256': inputs, 'inputs_unchanged': all(sha(Path(path)) == value for path, value in inputs.items()), 'compile_probe_admission_macro': True, 'production_admission': False, 'target_execution': False, 'linked': False, 'prx_build': False, 'max_compile_jobs': 1}
    if result.returncode == 0: proof.update(object=str(obj), object_sha256=sha(obj))
    receipt.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(proof, indent=2))
    assert result.returncode == 0 and proof['inputs_unchanged']

if __name__ == '__main__':
    main()
