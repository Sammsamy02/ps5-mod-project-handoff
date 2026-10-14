#!/usr/bin/env python3
"""Build and run Win32 SRW/condition import host checks."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'tests/win32-sync-import-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-pthread', '-I' + str(root / 'src'),
    '-I' + str(root / 'include'),
    str(root / 'tests/win32_sync_import_test.cpp'),
    str(root / 'src/win_api_shim.cpp'),
    str(root / 'src/win64_bridge.S'), '-o', str(out),
], check=True, timeout=30)
subprocess.run(['arch', '-x86_64', str(out)], check=True, timeout=20)
