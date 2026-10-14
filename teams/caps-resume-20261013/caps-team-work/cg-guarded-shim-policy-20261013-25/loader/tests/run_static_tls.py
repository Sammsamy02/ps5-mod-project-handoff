#!/usr/bin/env python3
"""Build and execute only the isolated host static-TLS state tests."""

import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
for source, extra in [
    ('static_tls_bank_test.cpp', []),
    ('pe_tls_registration_test.cpp', [str(root / 'src/pe_tls_registration.cpp')]),
]:
    out = root / 'tests' / (source.replace('_test.cpp', '-host'))
    subprocess.run([
        'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-pthread', '-I' + str(root / 'src'),
        str(root / 'tests' / source), str(root / 'src/static_tls_bank.cpp'),
        *extra, '-o', str(out),
    ], check=True, timeout=30)
    subprocess.run(['arch', '-x86_64', str(out)], check=True, timeout=20)
