#!/usr/bin/env python3
"""Build and test guarded virtual-memory imports through Win64 thunks."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
asan = '--asan' in sys.argv[1:]
build = root / ('tests/guest-virtual-memory-asan' if asan else
                'tests/guest-virtual-memory-host')
sources = sorted((root / 'src').glob('*.cpp')) + sorted((root / 'src').glob('*.S'))
command = ['clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g',
           '-Wall', '-Wextra', '-Werror', '-pthread',
           *(['-fsanitize=address', '-fno-omit-frame-pointer'] if asan else []),
           '-I' + str(root / 'include'), '-I' + str(root / 'src'),
           str(root / 'tests/guest_virtual_memory_test.cpp'),
           *map(str, sources), '-o', str(build)]
subprocess.run(command, cwd=root, check=True, timeout=90)
subprocess.run(['arch', '-x86_64', str(build)], cwd=root, check=True, timeout=30)
