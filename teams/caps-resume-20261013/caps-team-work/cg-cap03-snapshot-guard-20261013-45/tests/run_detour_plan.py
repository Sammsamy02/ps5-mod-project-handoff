#!/usr/bin/env python3
"""Build the pinned decoder and exercise the read-only x64 detour planner."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'build/host/detour'
build.mkdir(parents=True, exist_ok=True)
commands = [
    ['clang', '-arch', 'x86_64', '-std=c11', '-O2', '-fPIC', '-Werror',
     '-Wno-unused-function', '-Wno-unused-const-variable',
     '-Ivendor/zydis', '-c', 'vendor/zydis/Zydis.c',
     '-o', str(build / 'Zydis.o')],
    ['clang++', '-arch', 'x86_64', '-std=c++23', '-O1', '-g', '-Wall',
     '-Wextra', '-Werror', '-Isrc', '-Ivendor/zydis',
     'src/DetourPlan.cpp', 'tests/detour_plan_test.cpp',
     str(build / 'Zydis.o'), '-o', str(build / 'detour-plan-host')],
    ['arch', '-x86_64', str(build / 'detour-plan-host')],
]
for command in commands:
    result = subprocess.run(command, cwd=root, capture_output=True,
                            text=True, timeout=90)
    print(result.stdout + result.stderr, end='')
    if result.returncode:
        raise SystemExit(result.returncode)
