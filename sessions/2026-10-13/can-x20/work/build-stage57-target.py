#!/usr/bin/env python3
"""Compile/link isolated cap03 near trampoline without contacting a console."""
import os
from pathlib import Path
import subprocess
import sys

workspace = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20')
stage_name = 'cg-cap03-detour-transaction-20261013-57'
stage = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work') / stage_name
sdk = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin')
llvm = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
build = stage / 'build/ps5-obj'
build.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config'))

commands = [
    ('zydis', [str(sdk / 'prospero-clang'), '-std=c11', '-Os', '-fPIC',
               '-D_XOPEN_SOURCE=700', '-Werror', '-Wno-unused-function',
               '-Wno-unused-const-variable', '-Ivendor/zydis', '-c',
               'vendor/zydis/Zydis.c', '-o', str(build / 'Zydis.o')]),
    ('planner', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
                 '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Werror',
                 '-Isrc', '-Ivendor/zydis', '-c', 'src/DetourPlan.cpp',
                 '-o', str(build / 'DetourPlan.o')]),
    ('near', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
              '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Werror',
              '-Isrc', '-Ivendor/zydis', '-c', 'src/NearTrampoline.cpp',
              '-o', str(build / 'NearTrampoline.o')]),
    ('transaction', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
                      '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Werror',
                      '-Isrc', '-Ivendor/zydis', '-c', 'src/DetourTransaction.cpp',
                      '-o', str(build / 'DetourTransaction.o')]),
    ('codepatch', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
                   '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Isrc', '-c',
                   'src/CodePatch.cpp', '-o', str(build / 'CodePatch.o')]),
    ('diagnostic', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
                    '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Isrc', '-c',
                    'src/PatchDiagnostic.cpp', '-o', str(build / 'PatchDiagnostic.o')]),
    ('link', [str(sdk / 'prospero-clang++'), '-std=c++23', '-Os', '-fPIC',
              str(build / 'Zydis.o'), str(build / 'DetourPlan.o'),
              str(build / 'NearTrampoline.o'),
              str(build / 'DetourTransaction.o'),
              str(build / 'CodePatch.o'), str(build / 'PatchDiagnostic.o'),
              '-o', str(build / 'cap03-detour-offline.elf')]),
]
for label, command in commands:
    result = subprocess.run(command, cwd=stage, env=env, capture_output=True,
                            text=True, timeout=180)
    (build / f'{label}.log').write_text(
        ' '.join(command) + '\n' + result.stdout + result.stderr +
        f'\nexit={result.returncode}\n')
    print(f'{label}: exit={result.returncode}')
    if result.returncode:
        print((result.stdout + result.stderr)[-4000:])
        raise SystemExit(result.returncode)

readelf = subprocess.run([str(llvm / 'llvm-readelf'), '--symbols',
                          str(build / 'cap03-detour-offline.elf')],
                         capture_output=True, text=True, timeout=30)
(build / 'symbols.log').write_text(readelf.stdout + readelf.stderr)
print('readelf:', readelf.returncode,
      'plan symbol:', '_ZN5cap0311plan_detour' in readelf.stdout,
      'near symbol:', 'prepare_near_detour' in readelf.stdout,
      'transaction symbol:', 'DetourTransaction' in readelf.stdout)
if readelf.returncode or '_ZN5cap0311plan_detour' not in readelf.stdout or \
   'prepare_near_detour' not in readelf.stdout or \
   'DetourTransaction' not in readelf.stdout:
    raise SystemExit(1)

host = subprocess.run(['bash', 'tests/run_host_test.sh'], cwd=stage,
                      capture_output=True, text=True, timeout=120)
(build / 'host-protocol.log').write_text(host.stdout + host.stderr +
                                        f'\nexit={host.returncode}\n')
print('host protocol:', host.returncode)
print('\n'.join((host.stdout + host.stderr).splitlines()[-10:]))
raise SystemExit(host.returncode)
