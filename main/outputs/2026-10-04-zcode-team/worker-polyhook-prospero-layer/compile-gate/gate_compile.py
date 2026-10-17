#!/usr/bin/env python3
# gate_compile.py — worker-polyhook-prospero-layer compile gate.
# Compiles the proposed composition units with the EXACT polyhook unit argv
# of ue4ss-native-polyhook-sources-1/build.py (verified against
# baseline-polyhook/polyhook-MemProtector/command.json byte for byte, except
# -MF/-c/-o paths and the added units), then runs nm over the new object.
# Compile-only: never links, never executes any artifact (composition
# discipline: 'linked': false, 'executed': false).
from pathlib import Path
import json, hashlib, os, subprocess, sys

W = Path(__file__).resolve().parent
ROOT = W.parents[3]  # worker dir -> .../what
SPIKE = ROOT / '.superpowers/native-dependency-spike'
BASE = SPIKE / 'ue4ss-unreal-native-build-1'
DEPS = BASE / 'source-dependencies'
SDK = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
LLVM = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
HEADER = SPIKE / 'ue4ss-polyhook-platform-port-1/patched/include'
NEW_HDR = W.parent / 'integration/files/patched-include'
NEW_SRC = W.parent / 'integration/files/sources'

flags = ['-std=c++23', '-O0', '-fno-lto', '-fPIC', '-fms-extensions',
         '-ferror-limit=8', '-Wno-unknown-pragmas', '-DASMJIT_STATIC',
         '-DASMTK_STATIC', '-DZYDIS_STATIC_BUILD', '-DZYCORE_STATIC_BUILD']
# Include order mirrors the composition: patched/include (the port overlay,
# which now also carries PolyHookProsperoPlatform.hpp via integration/files)
# first, then the immutable dependency roots.
incs = [HEADER, NEW_HDR, DEPS / 'clang-x86-headers', DEPS / 'polyhook',
        DEPS / 'asmjit/src', DEPS / 'asmtk/src', DEPS / 'zydis/include',
        DEPS / 'zycore/include']

units = [
    ('polyhook', NEW_SRC / 'PolyHookProsperoPlatform.cpp', 'new'),
    ('polyhook', DEPS / 'polyhook/sources/MemAccessor.cpp', 'proposed-new-unit'),
    ('polyhook', DEPS / 'polyhook/sources/MemProtector.cpp', 'existing-unchanged'),
    # build.py selects the port overlay Misc.cpp for every non-baseline
    # label (the NULL->0 corrections); that is what the composition builds.
    ('polyhook', SPIKE / 'ue4ss-native-polyhook-sources-1/port/polyhook/sources/Misc.cpp',
     'existing-port-overlay'),
    ('polyhook', DEPS / 'polyhook/sources/PolyHookOs.cpp', 'existing-unchanged'),
]

label = sys.argv[1] if len(sys.argv) > 1 else 'gate-1'
out = W / label
out.mkdir(exist_ok=False)
results = []
for dep, src, kind in units:
    folder = out / (dep + '-' + src.stem)
    folder.mkdir()
    obj = folder / (src.stem + '.o')
    argv = [str(SDK / 'bin/prospero-clang++'), *flags,
            *[a for p in incs for a in ('-I', str(p))],
            '-MD', '-MF', str(folder / 'dependencies.d'),
            '-c', str(src), '-o', str(obj)]
    p = subprocess.run(argv, env=dict(os.environ,
                       LLVM_CONFIG=str(LLVM / 'llvm-config')),
                       capture_output=True, text=True, timeout=300)
    (folder / 'stdout.txt').write_text(p.stdout)
    (folder / 'stderr.txt').write_text(p.stderr)
    (folder / 'command.json').write_text(json.dumps(
        {'argv': argv, 'LLVM_CONFIG': str(LLVM / 'llvm-config'),
         'exit': p.returncode, 'executed': False}, indent=2) + '\n')
    u = {'dependency': dep, 'source': str(src), 'kind': kind,
         'source_sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
         'exit': p.returncode,
         'errors': p.stderr.count('error:'),
         'warnings': p.stderr.count('warning:')}
    if not p.returncode:
        u.update(object=str(obj),
                 object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                 object_bytes=obj.stat().st_size)
    results.append(u)
    print(json.dumps({'source': src.name, 'kind': kind, 'exit': p.returncode,
                      'errors': [s for s in p.stderr.splitlines()
                                 if 'error:' in s][:5]}), flush=True)

# nm proof: the symbols the port report listed as missing, now defined.
# Demangled substring match (robust to mangling details across ABIs).
nm = subprocess.run([str(LLVM / 'llvm-nm'), '-g', '-C',
                     str(out / 'polyhook-PolyHookProsperoPlatform'
                         / 'PolyHookProsperoPlatform.o')],
                    capture_output=True, text=True)
(out / 'nm-new-object.txt').write_text(nm.stdout)
(out / 'nm-new-object-mangled.txt').write_text(
    subprocess.run([str(LLVM / 'llvm-nm'), '-g',
                    str(out / 'polyhook-PolyHookProsperoPlatform'
                        / 'PolyHookProsperoPlatform.o')],
                   capture_output=True, text=True).stdout)
expect = [
    'PLH::TranslateProtection(PLH::ProtFlag)',
    'PLH::TranslateProtection(int)',
    'PLH::MemAccessor::mem_copy',
    'PLH::MemAccessor::safe_mem_write',
    'PLH::MemAccessor::safe_mem_read',
    'PLH::MemAccessor::mem_protect',
    'PLH::boundedAllocSupported',
    'PLH::boundAlloc(',
    'PLH::boundAllocLegacy',
    'PLH::boundAllocFree',
    'PLH::getAllocationAlignment',
    'PLH::getPageSize',
    'PolyHook2DebugBreak',
    'plh_prospero::active_backend',
]
defined = nm.stdout
missing = [s for s in expect if s not in defined]

# Branch-selection control: compile-only assert that the composition's
# patched overlay selects the Prospero branch and the new header is found.
ctrl = out / 'branch-control'
ctrl.mkdir()
ctrl_src = ctrl / 'branch_control.cpp'
ctrl_src.write_text('''
#include "polyhook2/PolyHookOs.hpp"
#if !defined(POLYHOOK2_OS_PROSPERO)
#error "branch control: POLYHOOK2_OS_PROSPERO not selected"
#endif
#if !defined(POLYHOOK2_ARCH_X64)
#error "branch control: POLYHOOK2_ARCH_X64 not selected"
#endif
#include "polyhook2/PolyHookProsperoPlatform.hpp"
#include "polyhook2/MemAccessor.hpp"
#include "polyhook2/MemProtector.hpp"
#include "polyhook2/Misc.hpp"
// The header core is platform-independent: translation sanity at compile time.
static_assert(plh_prospero::translate_plh_to_prot(plh_prospero::kPlhRwx) == 7, "rwx");
static_assert(plh_prospero::translate_prot_to_plh(3) ==
              (plh_prospero::kPlhR | plh_prospero::kPlhW), "rw");
static_assert(plh_prospero::kPage16K == 0x4000u, "page");
static_assert(plh_prospero::kVmHdr == 136u, "vmhdr");
static_assert(plh_prospero::kKveSizeofCap == 1160u, "kvecap");
int branch_control() { return 0; }
''')
cargv = [str(SDK / 'bin/prospero-clang++'), *flags,
         *[a for p in incs for a in ('-I', str(p))],
         '-MD', '-MF', str(ctrl / 'dependencies.d'),
         '-c', str(ctrl_src), '-o', str(ctrl / 'branch_control.o')]
cp = subprocess.run(cargv, env=dict(os.environ,
                    LLVM_CONFIG=str(LLVM / 'llvm-config')),
                    capture_output=True, text=True, timeout=300)
(ctrl / 'stdout.txt').write_text(cp.stdout)
(ctrl / 'stderr.txt').write_text(cp.stderr)
(ctrl / 'command.json').write_text(json.dumps(
    {'argv': cargv, 'LLVM_CONFIG': str(LLVM / 'llvm-config'),
     'exit': cp.returncode, 'executed': False}, indent=2) + '\n')
print(json.dumps({'source': 'branch_control.cpp', 'exit': cp.returncode,
                  'errors': [s for s in cp.stderr.splitlines()
                             if 'error:' in s][:5]}), flush=True)

summary = {'units': results,
           'all_compiled': all(u['exit'] == 0 for u in results),
           'branch_control_exit': cp.returncode,
           'nm_expected_symbols': len(expect),
           'nm_missing': missing,
           'linked': False,
           'executed': False}
(out / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps({'all_compiled': summary['all_compiled'],
                  'branch_control_exit': cp.returncode,
                  'nm_missing': missing}))
sys.exit(0 if summary['all_compiled'] and cp.returncode == 0 and not missing
         else 1)
