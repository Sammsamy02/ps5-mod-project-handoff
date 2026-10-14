#!/usr/bin/env python3
"""Host-check the backend policy with injected GS operations."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
binary = root / 'tests/verified-gs-segment-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-pthread', '-I' + str(root / 'src'),
    str(root / 'tests/verified_gs_segment_test.cpp'),
    str(root / 'src/verified_gs_segment_switch.cpp'),
    str(root / 'src/guest_teb_lifecycle.cpp'),
    str(root / 'src/static_tls_bank.cpp'),
    str(root / 'src/win64_bridge.S'), '-o', str(binary),
], check=True, timeout=30)
subprocess.run(['arch', '-x86_64', str(binary)], check=True, timeout=20)
