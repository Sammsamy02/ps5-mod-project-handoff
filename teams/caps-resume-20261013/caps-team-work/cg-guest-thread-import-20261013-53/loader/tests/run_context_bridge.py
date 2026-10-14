#!/usr/bin/env python3
"""Build and run context-bound integer ABI checks on an x86-64 host."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'tests/context-bridge-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-I' + str(root / 'src'),
    str(root / 'tests/context_bridge_test.cpp'),
    str(root / 'tests/context_bridge_nonvolatiles.S'),
    str(root / 'src/win64_bridge.S'), '-o', str(out),
], check=True, timeout=30)
subprocess.run(['arch', '-x86_64', str(out)], check=True, timeout=20)
