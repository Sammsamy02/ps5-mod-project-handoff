#!/usr/bin/env python3
"""worker-supplier-ingame-update verify: host suite + pinned-SDK compile.

1. Compiles and runs the vendored s1-adapter host suite (original 102 checks
   + N1..N5 in-game update checks) against THIS worker's patched vendored
   supplier/core/adapter/base-scan copies and the immutable spike chain.
2. Cross-compiles the patched SCE TU (+ core + base scan) with the pinned
   ps5-payload-sdk prospero-clang++ into AMD64 ELF relocatable objects, in
   BOTH dl-retention variants (default + LMF_DISABLE_DL_RETENTION), and
   records the SCE TU's undefined symbols.
No link to a PRX, no target execution, no console contact.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]  # .../worktrees/translation-core/what
SP = ROOT / '.superpowers/native-dependency-spike'
SDK_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
LLVM_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
OUT = HERE / 'build-receipts/run-1'


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(name: str, argv, env=None, allow_fail=False) -> str:
    proc = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                          env=env, cwd=str(ROOT))
    (OUT / f'{name}.stderr').write_text(proc.stdout + proc.stderr)
    if proc.returncode != 0 and not allow_fail:
        print(f'--- {name} FAILED ---')
        print(proc.stdout)
        print(proc.stderr)
        raise SystemExit(proc.returncode)
    return proc.stdout


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    receipt = {'worker': 'worker-supplier-ingame-update', 'steps': {},
               'source_files': {}}
    for path in sorted((HERE / 'src').glob('*')) + \
            sorted((HERE / 'include').glob('*')) + \
            sorted((HERE / 'host_tests').glob('*.cpp')):
        receipt['source_files'][str(path.relative_to(HERE))] = sha(path)

    cxx = os.environ.get('LMF_HOST_CXX', '/usr/bin/clang++')
    host_flags = ['-std=c++23', '-O1', '-g', '-fno-omit-frame-pointer',
                  '-DPLH_OWNED_MEMORY_HOST_TEST', '-DRC_HELPERS_BUILD_STATIC',
                  '-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC']
    includes = [HERE / 'src', HERE / 'include',
                SP / 'ue4ss-native-tick-file-owner-1/include',
                SP / 'ue4ss-native-tick-witness-1/include',
                SP / 'ue4ss-native-mapped-candidate-selection-1/include',
                SP / 'ue4ss-native-mapped-candidates-1/include',
                SP / 'ue4ss-native-mapped-candidate-selection-1/tests',
                SP / 'ue4ss-native-ps-scan-provider-1/include',
                SP / 'ue4ss-native-mapped-collector-properties-1/include',
                SP / 'ue4ss-native-mapped-collector-properties-1/tests',
                SP / 'ue4ss-native-build-properties-1/include',
                SP / 'ue4ss-native-read-lease-1/include',
                SP / 'ue4ss-native-ps-scan-abi-1/include',
                SP / 'ue4ss-unreal-native-build-1/port/unreal/include',
                SP / 'ue4ss-unreal-native-build-1/port/ue4ss/include',
                SP / 'ue4ss-unreal-native-build-1/port/first/include',
                SP / 'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include',
                SP / 'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include',
                SP / 'ue4ss-polyhook-platform-port-1/patched/include',
                SP / 'ue4ss-unreal-native-build-1/source-dependencies/polyhook']
    inc = []
    for d in includes:
        inc += ['-I', d]

    units = {
        'lmf_core.o': HERE / 'src/live_module_facts.cpp',
        'lmf_base_scan.o': HERE / 'src/live_module_base_scan.cpp',
        'adapter.o': HERE / 'src/lmf_to_native_loader_facts.cpp',
        'NativeLoaderContext.o': SP / 'ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp',
        'OwnedMappedScanSource.o': SP / 'ue4ss-native-tick-file-owner-1/src/OwnedMappedScanSource.cpp',
        'OwnedSerializedElfSource.o': SP / 'ue4ss-native-tick-file-owner-1/src/OwnedSerializedElfSource.cpp',
        'OwnedMappedCandidateSelection.o': SP / 'ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedCandidateSelection.cpp',
        'OwnedMappedResolverCollector.o': SP / 'ue4ss-native-mapped-collector-properties-1/src/OwnedMappedResolverCollector.cpp',
        'test_lmf_adapter_host.o': HERE / 'host_tests/test_lmf_adapter_host.cpp',
    }
    objects = []
    for obj, src in units.items():
        argv = [cxx, *host_flags, *inc, '-DADAPTER_FIXTURE_EBOOT="' +
                str(HERE / 'host_tests/fixtures/eboot.bin') + '"',
                '-DADAPTER_FIXTURE_MODULE="' + str(HERE / 'host_tests/fixtures/ue4ss.so') + '"',
                '-c', src, '-o', OUT / obj]
        run(obj.replace('.o', '-compile'), argv)
        objects.append(OUT / obj)

    memory_o = SP / 'ue4ss-native-mapped-candidate-selection-1/host-harness-1/memory.o'
    argv = [cxx, '-Wl,-undefined,dynamic_lookup', *objects, memory_o, '-o',
            OUT / 'supplier-ingame-host-tests']
    run('link', argv)
    out = run('run', [OUT / 'supplier-ingame-host-tests'])
    receipt['steps']['host_run'] = out.strip()
    print(out.strip())

    # ---- pinned SDK cross-compile of the target TUs -----------------------
    sdk = Path(os.environ.get('LMF_SDK', SDK_DEFAULT)).resolve()
    compiler = sdk / 'bin/prospero-clang++'
    if not compiler.is_file():
        raise SystemExit(f'pinned SDK compiler missing: {compiler}')
    receipt['toolchain'] = {'compiler': str(compiler), 'compiler_sha256': sha(compiler)}
    llvm = Path(os.environ.get('LMF_LLVM', LLVM_DEFAULT)).resolve()
    env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config')) \
        if (llvm / 'llvm-config').exists() else dict(os.environ)
    native_flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fPIC',
                    '-I', HERE / 'include',
                    '-I', SP / 'ue4ss-native-tick-file-owner-1/include']
    sce = HERE / 'src/live_module_os_ops_sce.cpp'
    for label, extra in [('default', []),
                         ('dl-disabled', ['-DLMF_DISABLE_DL_RETENTION'])]:
        obj = OUT / f'live_module_os_ops_sce-{label}.o'
        run(f'sce-{label}-compile',
            [compiler, *native_flags, *extra, '-c', sce, '-o', obj], env)
        data = obj.read_bytes()
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (1, 62):
            raise SystemExit(f'unexpected object format for {obj}')
        nm = llvm / 'llvm-nm'
        if nm.is_file():
            receipt['steps'][f'sce_{label}_undefined'] = run(
                f'sce-{label}-nm',
                [nm, '--undefined-only', '--format=posix', obj]).strip().splitlines()
    for tu in ['src/live_module_facts.cpp', 'src/live_module_base_scan.cpp']:
        obj = OUT / (Path(tu).stem + '-sdk.o')
        run(f'{Path(tu).stem}-sdk-compile',
            [compiler, *native_flags, '-c', HERE / tu, '-o', obj], env)
        data = obj.read_bytes()
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (1, 62):
            raise SystemExit(f'unexpected object format for {obj}')

    receipt['status'] = 'SUPPLIER_INGAME_UPDATE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED'
    receipt['objects'] = {o.name: sha(o) for o in OUT.glob('*.o')}
    (OUT / 'receipt.json').write_text(json.dumps(receipt, indent=1))
    print(receipt['status'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
