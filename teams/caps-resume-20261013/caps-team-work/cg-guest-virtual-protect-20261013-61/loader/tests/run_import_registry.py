#!/usr/bin/env python3
"""Build and run isolated import-registry host tests."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'tests/import-registry-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-pthread', '-I' + str(root / 'src'),
    '-I' + str(root / 'include'),
    str(root / 'tests/import_registry_test.cpp'),
    str(root / 'src/win_api_shim.cpp'),
    str(root / 'src/win64_bridge.S'), '-o', str(out),
], check=True, timeout=30)
subprocess.run(['arch', '-x86_64', str(out)], check=True, timeout=5)
