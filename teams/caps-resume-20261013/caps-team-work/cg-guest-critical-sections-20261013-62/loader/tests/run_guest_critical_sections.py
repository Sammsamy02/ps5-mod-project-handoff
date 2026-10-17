#!/usr/bin/env python3
"""Build and test critical-section Win64 import thunks."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
asan = '--asan' in sys.argv[1:]
build = root / ('tests/guest-critical-sections-asan' if asan else
                'tests/guest-critical-sections-host')
sources = sorted((root / 'src').glob('*.cpp')) + sorted((root / 'src').glob('*.S'))
command = ['clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g',
           '-Wall', '-Wextra', '-Werror', '-pthread',
           *(['-fsanitize=address', '-fno-omit-frame-pointer'] if asan else []),
           '-I' + str(root / 'include'), '-I' + str(root / 'src'),
           str(root / 'tests/guest_critical_sections_test.cpp'),
           *map(str, sources), '-o', str(build)]
subprocess.run(command, cwd=root, check=True, timeout=90)
subprocess.run(['arch', '-x86_64', str(build)], cwd=root, check=True, timeout=30)
