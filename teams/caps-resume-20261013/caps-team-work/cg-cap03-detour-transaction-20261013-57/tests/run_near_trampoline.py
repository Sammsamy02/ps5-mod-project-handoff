#!/usr/bin/env python3
"""Host-check near trampoline reservation, relocation, seal and execution."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
asan = '--asan' in sys.argv[1:]
build = root / ('build/host/near-asan' if asan else 'build/host/near')
build.mkdir(parents=True, exist_ok=True)
sanitizer = ['-fsanitize=address', '-fno-omit-frame-pointer'] if asan else []
commands = [
    ['clang', '-arch', 'x86_64', '-std=c11', '-O2', '-fPIC', '-Werror',
     *sanitizer,
     '-Wno-unused-function', '-Wno-unused-const-variable',
     '-Ivendor/zydis', '-c', 'vendor/zydis/Zydis.c',
     '-o', str(build / 'Zydis.o')],
    ['clang++', '-arch', 'x86_64', '-std=c++23', '-O1', '-g', '-Wall',
     '-Wextra', '-Werror', *sanitizer, '-Isrc', '-Ivendor/zydis',
     'src/DetourPlan.cpp', 'src/NearTrampoline.cpp',
     'tests/near_trampoline_test.cpp', str(build / 'Zydis.o'),
     '-o', str(build / 'near-trampoline-host')],
    ['arch', '-x86_64', str(build / 'near-trampoline-host')],
]
for command in commands:
    run = subprocess.run(command, cwd=root, capture_output=True,
                         text=True, timeout=90)
    print(run.stdout + run.stderr, end='')
    if run.returncode:
        raise SystemExit(run.returncode)
