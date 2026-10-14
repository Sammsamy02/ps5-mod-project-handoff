#!/usr/bin/env python3
"""Build and run the isolated x86-64 guest TEB lifecycle host checks."""

import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'tests/teb-lifecycle-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-pthread', '-I' + str(root / 'src'),
    str(root / 'tests/teb_lifecycle_test.cpp'),
    str(root / 'src/static_tls_bank.cpp'),
    str(root / 'src/guest_teb_lifecycle.cpp'),
    str(root / 'src/win64_bridge.S'), '-o', str(out),
], check=True, timeout=30)
subprocess.run(['arch', '-x86_64', str(out)], check=True, timeout=20)
