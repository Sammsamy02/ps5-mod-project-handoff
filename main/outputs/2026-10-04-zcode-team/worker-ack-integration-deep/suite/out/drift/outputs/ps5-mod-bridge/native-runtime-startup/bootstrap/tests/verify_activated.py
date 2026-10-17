#!/usr/bin/env python3
"""Exercise the real activation/source bridge locally; never contact a console."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[5]
parser = argparse.ArgumentParser()
parser.add_argument('--claude-root', type=Path, default=root / '.superpowers/native-dependency-spike/claude-takeover-43a96969/source')
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent
port = root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/port'
out = root / '.superpowers/native-dependency-spike/native-activated-mod-source-1'
out.mkdir(parents=True, exist_ok=True)
runtime = args.claude_root / 'runtime'
records = []

def run(name, argv, env=None):
    result = subprocess.run([str(x) for x in argv], cwd=root, env=env,
                            capture_output=True, text=True, timeout=120)
    (out / (name + '.stdout')).write_text(result.stdout)
    (out / (name + '.stderr')).write_text(result.stderr)
    records.append(dict(name=name, argv=[str(x) for x in argv], exit_code=result.returncode))
    (out / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(name + ': PASS')

run('host-activation', ['/usr/bin/clang', '-std=c99', '-D_POSIX_C_SOURCE=200809L',
    '-Wall', '-Wextra', '-Werror', '-c', runtime / 'modforge_activation.c', '-o', out / 'activation.o'])
run('host-adapter', ['/usr/bin/clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
    '-I', source, '-I', port / 'ue4ss/include', '-I', runtime,
    source / 'activated_mod_source.cpp', port / 'ue4ss/src/Platform/NativeModSource.cpp',
    source / 'tests/activated_mod_source_test.cpp', out / 'activation.o', '-o', out / 'activation_test'])
with tempfile.TemporaryDirectory(prefix='modbridge-activation-') as directory:
    run('host-owned-files', [out / 'activation_test', Path(directory) / 'installed'])

recipe = json.loads((root / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/consolidated-5/program/command.json').read_text())
argv = list(recipe['argv'])
argv[argv.index('-c') + 1] = str(source / 'activated_mod_source.cpp')
argv[argv.index('-o') + 1] = str(out / 'activated_mod_source.o')
argv += ['-I', str(runtime)]
run('native-adapter', argv, dict(os.environ, LLVM_CONFIG=recipe['LLVM_CONFIG']))
inputs = [runtime / 'modforge_activation.c', runtime / 'modforge_activation.h',
    runtime / 'modforge_loadlist.h', source / 'activated_mod_source.cpp',
    source / 'activated_mod_source.hpp', source / 'tests/activated_mod_source_test.cpp',
    port / 'ue4ss/include/Platform/NativeModSource.hpp',
    port / 'ue4ss/src/Platform/NativeModSource.cpp']
(out / 'verification.json').write_text(json.dumps({
    'status': 'HOST_ACTIVATION_OWNERSHIP_AND_NATIVE_ADAPTER_COMPILE_PASSED',
    'claude_commit': '43a96969e3bf1f4c9d1e43a55a386328dbac19e6',
    'inputs_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'scope': 'Real activation C reader, retained native source, and adapter; remove original files, retain buffer after source destruction, reject wrong title/framework, unbuffered files and native binary roles.',
    'engine_initialized': False, 'console_contact': False, 'full_runtime_linked': False,
}, indent=2) + '\n')
