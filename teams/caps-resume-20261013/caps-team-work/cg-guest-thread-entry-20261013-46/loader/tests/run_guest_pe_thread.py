#!/usr/bin/env python3
"""Host-check owned guest worker entry with an unchanged synthetic PE."""
from pathlib import Path
import hashlib
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
fixture = root.parent.parent / 'cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
digest = 'acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce'
if hashlib.sha256(fixture.read_bytes()).hexdigest() != digest:
    raise RuntimeError('unchanged fixture hash mismatch')
asan = '--asan' in sys.argv[1:]
out = root / ('tests/guest-pe-thread-asan' if asan else
              'tests/guest-pe-thread-host')
sources = [
    'tests/guest_pe_thread_test.cpp',
    'src/guest_pe_thread.cpp', 'src/guest_pe_batch.cpp',
    'src/pe_import_scan.cpp', 'src/guest_pe_loader.cpp',
    'src/pe_loader.cpp', 'src/pe_tls_registration.cpp',
    'src/static_tls_bank.cpp', 'src/guest_teb_lifecycle.cpp',
    'src/win_api_shim.cpp', 'src/win64_bridge.S',
]
compile_command = [
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g',
    '-Wall', '-Wextra', '-Werror', '-pthread',
    *(['-fsanitize=address', '-fno-omit-frame-pointer'] if asan else []),
    '-I' + str(root / 'include'), '-I' + str(root / 'src'),
    *[str(root / source) for source in sources], '-o', str(out),
]
subprocess.run(compile_command, check=True, timeout=60)
subprocess.run(['arch', '-x86_64', str(out), str(fixture)], check=True,
               timeout=30)
if hashlib.sha256(fixture.read_bytes()).hexdigest() != digest:
    raise RuntimeError('unchanged fixture bytes modified')
