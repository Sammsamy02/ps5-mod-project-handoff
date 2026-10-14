#!/usr/bin/env python3
"""Focused owner tests and real-backend PS5 SDK compilation, never console I/O."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

root = Path(__file__).resolve().parents[5]
source = Path(__file__).resolve().parent.parent
out = root / '.superpowers/native-dependency-spike/native-runtime-bootstrap-owner-1'
out.mkdir(parents=True, exist_ok=True)
recipe_path = root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/consolidated-5/program/command.json'
recipe = json.loads(recipe_path.read_text())
environment = dict(os.environ, LLVM_CONFIG=recipe['LLVM_CONFIG'])
records = []

def run(name, argv):
    result = subprocess.run([str(x) for x in argv], cwd=root, env=environment,
                            capture_output=True, text=True, timeout=90)
    (out / (name + '.stdout')).write_text(result.stdout)
    (out / (name + '.stderr')).write_text(result.stderr)
    records.append(dict(name=name, argv=[str(x) for x in argv], exit_code=result.returncode))
    (out / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(name + ': PASS')

run('host-compile', ['/usr/bin/clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
    '-pthread', '-I', source, source / 'runtime_owner.cpp',
    source / 'tests/runtime_owner_test.cpp', '-o', out / 'owner_test'])
run('host-lifecycle', [out / 'owner_test'])

for name in ('runtime_owner', 'ue4ss_backend'):
    argv = list(recipe['argv'])
    argv[argv.index('-c') + 1] = str(source / (name + '.cpp'))
    argv[argv.index('-o') + 1] = str(out / (name + '.o'))
    run('native-' + name, argv)

inputs = sorted(source.rglob('*.cpp')) + sorted(source.rglob('*.hpp'))
port = root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/port'
dependency_inputs = [port / p for p in (
    'ue4ss/include/Platform/NativeProgramContext.hpp', 'ue4ss/include/UE4SSProgram.hpp',
    'ue4ss/src/UE4SSProgram.cpp', 'unreal/include/Unreal/StartupWaitControl.hpp',
    'unreal/include/Unreal/UnrealInitializer.hpp', 'unreal/src/UnrealInitializer.cpp')]
record = {
    'status': 'HOST_OWNER_TESTS_AND_NATIVE_BACKEND_COMPILE_PASSED',
    'source_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'object_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.o')},
    'dependency_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in dependency_inputs},
    'LLVM_CONFIG': recipe['LLVM_CONFIG'],
    'actual_ue4ss_backend_compiled': True,
    'actual_ue4ss_backend_executed': False,
    'full_runtime_linked': False,
    'console_contact': False,
    'test_scope': 'Host controlled backend tests ownership, callback states, shutdown/join and failures; no engine init on fake bytes.',
    'limitation': 'Production backend requires validated live NativeProgramContext. Startup waits are cooperative; hooks must be quiescent before destruction or unload. Rebuild all consumers of changed native program/context headers before full composition.',
}
(out / 'verification.json').write_text(json.dumps(record, indent=2) + '\n')
