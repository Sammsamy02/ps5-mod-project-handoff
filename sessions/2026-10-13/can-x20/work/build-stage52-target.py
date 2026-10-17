#!/usr/bin/env python3
"""Build every isolated loader TU with the pinned PS5 SDK and package evidence."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile

workspace = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20')
stage = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-deferred-thread-20261013-52')
sdk = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk')
llvm = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
out = stage / 'build/ps5-obj'
out.mkdir(parents=True, exist_ok=True)
outputs = workspace / 'outputs'
env = os.environ.copy()
env['LLVM_CONFIG'] = str(llvm / 'llvm-config')
driver = str(sdk / 'bin/prospero-clang++')
sources = sorted((stage / 'loader/src').glob('*.cpp')) + sorted((stage / 'loader/src').glob('*.S'))
header_files = sorted((stage / 'loader/src').glob('*.h')) + sorted((stage / 'loader/include').glob('*.h'))
assert len(sources) == 15, [s.name for s in sources]
flags = ['-std=c++17', '-Os', '-fPIC', '-ffunction-sections', '-fdata-sections',
         '-D_XOPEN_SOURCE=700', '-Wall', '-Wextra', '-Werror',
         '-Iloader/include', '-Iloader/src']
records = []

def run(label, argv):
    result = subprocess.run(argv, cwd=stage, env=env, capture_output=True, text=True, timeout=120)
    log = out / (label + '.log')
    log.write_text('argv=' + json.dumps(argv) + '\n' + result.stdout + result.stderr +
                   '\nexit=' + str(result.returncode) + '\n')
    print(f'{label}: exit={result.returncode}', flush=True)
    if result.returncode:
        raise SystemExit(f'{label} failed; see {log}')
    return log

run('compiler-version', [driver, '--version'])
run('target-macros', [driver, '-dM', '-E', '-x', 'c++', '/dev/null'])
objects = []
for source in sources:
    obj = out / (source.stem + '.o')
    run('compile-' + source.stem, [driver, *flags, '-c', str(source), '-o', str(obj)])
    objects.append(obj)

probe_source = out / 'target_link_probe.cpp'
probe_source.write_text('// Link-only stub; no console execution is claimed.\n#include \"verified_gs_segment_switch.h\"\nint main() { cap13::VerifiedGsSegmentSwitch sw(cap13::ps5_sysarch_gs_operations()); return 0; }\n')
probe_obj = out / 'target_link_probe.o'
run('compile-link-stub', [driver, *flags, '-c', str(probe_source), '-o', str(probe_obj)])
elf = out / 'loader-link-probe.elf'
run('full-link', [driver, '-std=c++17', '-Os', '-fPIC', '-Wl,--no-gc-sections',
                  '-o', str(elf), str(probe_obj), *map(str, objects)])
run('elf-headers', [str(llvm / 'llvm-readelf'), '-h', '-l', '-d', str(elf)])
run('elf-undefined', [str(llvm / 'llvm-nm'), '-u', str(elf)])
run('mapped-os-symbols', [str(llvm / 'llvm-nm'), '-u', str(out / 'pe_loader.o')])
run('gs-os-symbols', [str(llvm / 'llvm-nm'), '-u', str(out / 'verified_gs_segment_switch.o')])

macro_log = (out / 'target-macros.log').read_text()
if '#define __PROSPERO__ 1' not in macro_log:
    raise SystemExit('target macro absent')
headers = (out / 'elf-headers.log').read_text()
for expected in ['Machine:                           Advanced Micro Devices X86-64',
                 'Shared library: [libkernel_web.sprx]',
                 'Shared library: [libSceLibcInternal.sprx]']:
    if expected not in headers:
        raise SystemExit(f'ELF property missing: {expected}')
symbols = (out / 'mapped-os-symbols.log').read_text()
for name in ['mmap', 'mprotect', 'munmap']:
    if ' U ' + name not in symbols:
        raise SystemExit(f'mapper POSIX symbol missing: {name}')
if ' U sysarch' not in (out / 'gs-os-symbols.log').read_text():
    raise SystemExit('PS5 sysarch binding missing')
if ' U sysarch' not in (out / 'elf-undefined.log').read_text():
    raise SystemExit('linked ELF does not import sysarch')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

files = [*sources, *header_files, *objects, probe_source, probe_obj, elf]
files += sorted(out.glob('*.log'))
manifest_data = {
    'stage': str(stage), 'sdk_driver': driver, 'llvm_config': env['LLVM_CONFIG'],
    'purpose': 'compile and link evidence only; no PS5 runtime execution',
    'compile_flags': flags, 'source_count': len(sources),
    'header_count': len(header_files),
    'all_compile_exit_zero': True, 'full_link_exit_zero': True,
    'elf_type': 'FreeBSD x86-64 PIE as reported by file/readelf',
    'files': {str(p.relative_to(stage)): {'sha256': sha(p), 'size': p.stat().st_size}
              for p in files},
}
manifest = outputs / 'guest-deferred-thread-target-build-manifest-20261013.json'
manifest.write_text(json.dumps(manifest_data, indent=2) + '\n')
archive_path = outputs / 'guest-deferred-thread-target-build-evidence-20261013.zip'
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
    for item in files:
        archive.write(item, str(item.relative_to(stage)))
    archive.write(manifest, manifest.name)
with zipfile.ZipFile(archive_path) as archive:
    bad = archive.testzip()
    if bad:
        raise SystemExit(f'ZIP CRC failure: {bad}')
print(f'target build evidence: PASS; {len(sources)} sources; {archive_path.stat().st_size} bytes')
