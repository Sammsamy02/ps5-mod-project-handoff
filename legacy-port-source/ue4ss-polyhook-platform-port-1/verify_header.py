#!/usr/bin/env python3
"""Compile the real PolyHook x64 header closure; never link or execute it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
SPIKE = Path(__file__).resolve().parent
DEPS = ROOT / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies'
SDK = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
LLVM = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64'
SOURCE = SPIKE / 'tests/header_contract.cpp'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--variant', choices=['baseline', 'patched'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(SPIKE) or output.exists():
        raise SystemExit('output must be a fresh directory in the assigned spike')
    output.mkdir(parents=True)
    selected = DEPS / 'polyhook'
    if args.variant == 'patched':
        selected = SPIKE / 'patched/include'
    includes = [selected, DEPS / 'polyhook', DEPS / 'zydis/include',
                DEPS / 'zycore/include', DEPS / 'asmjit/src']
    env = os.environ.copy()
    env['LLVM_CONFIG'] = str(LLVM / 'bin/llvm-config')
    common = [str(SDK / 'bin/prospero-clang++'), '-std=c++23', '-O0', '-fno-lto',
              '-fPIC', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-pragmas',
              '-ferror-limit=10',
              '-DZYDIS_STATIC_BUILD', '-DZYCORE_STATIC_BUILD', '-DASMJIT_STATIC']
    for include in includes:
        common.extend(['-I', str(include)])
    macro_argv = [str(SDK / 'bin/prospero-clang++'), '-std=c++23', '-dM', '-E', '-x', 'c++', '/dev/null']
    macro = subprocess.run(macro_argv, env=env, capture_output=True, text=True)
    (output / 'compiler-macros.txt').write_text(macro.stdout)
    (output / 'compiler-macros.stderr').write_text(macro.stderr)
    argv = common + ['-MD', '-MF', str(output / 'header_contract.d'), '-c', str(SOURCE),
                     '-o', str(output / 'header_contract.o')]
    compile_result = subprocess.run(argv, env=env, capture_output=True, text=True)
    (output / 'stdout.txt').write_text(compile_result.stdout)
    (output / 'stderr.txt').write_text(compile_result.stderr)
    header_names = ['PolyHookOs.hpp', 'PolyHookOsIncludes.hpp', 'IHook.hpp']
    receipt = {'variant': args.variant, 'argv': argv, 'exit': compile_result.returncode,
               'LLVM_CONFIG': env['LLVM_CONFIG'], 'source': str(SOURCE),
               'source_sha256': digest(SOURCE), 'selected_include_root': str(selected),
               'polyhook_pin': '298d56210b9d9e66cde8f96481d6053925c6ae15',
               'compiler_macro_exit': macro.returncode,
               'compiler_platform_macros': [line for line in macro.stdout.splitlines()
                  if any(token in line for token in ['__PROSPERO__', '__FreeBSD__', '__linux__',
                      '__APPLE__', '_WIN32', '__x86_64__', '__SIZEOF_POINTER__', '__SIZEOF_WCHAR_T__'])],
               'canonical_headers': {name: digest(DEPS / 'polyhook/polyhook2' / name)
                                     for name in header_names},
               'selected_headers': {name: digest(selected / 'polyhook2' / name)
                                    for name in header_names},
               'object_exists': (output / 'header_contract.o').exists(),
               'linked': False, 'executed': False, 'console_contact': False,
               'game_memory_read': False, 'executable_page_operation': False}
    if (output / 'header_contract.o').exists():
        receipt['object_sha256'] = digest(output / 'header_contract.o')
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'exit': compile_result.returncode, 'receipt': str(output / 'receipt.json'),
                      'object_exists': receipt['object_exists']}))
    print(compile_result.stderr[:12000])
    return compile_result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
