#!/usr/bin/env python3
"""worker-composition-prep: re-run worker-supplier-ingame-update's 169-check
host suite + pinned-SDK cross-compile against the PATCHED VENDORED TREE
(staging mirror of the spike + s1-adapter trees), not the worker's own copies.

Derived verbatim from verify.py of worker-supplier-ingame-update; only the
source/include/output roots change. The worker's area is read-only for us:
its host_tests (suite + fixtures) are referenced read-only, all writes go to
build-receipts/vendored-run-1 under THIS worker's output area.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent          # this worker's output dir
ROOT = HERE.parents[2]                          # .../worktrees/translation-core/what
SP = ROOT / '.superpowers/native-dependency-spike'
SDK_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
LLVM_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
VSP = HERE / 'staging/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
VS1 = HERE / 'staging/outputs/2026-10-04-zcode-team/worker-s1-adapter/src'
WK = ROOT / 'outputs/2026-10-04-zcode-team/worker-supplier-ingame-update'
OUT = HERE / 'build-receipts/vendored-run-1'


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
    receipt = {'worker': 'worker-composition-prep', 'variant': 'vendored-tree',
               'steps': {}, 'source_files': {}}
    vendored = sorted(VSP.glob('include/*')) + sorted(VSP.glob('src/*')) + [VS1 / 'lmf_to_native_loader_facts.hpp']
    for path in vendored:
        receipt['source_files'][str(path.relative_to(HERE / 'staging'))] = sha(path)

    cxx = os.environ.get('LMF_HOST_CXX', '/usr/bin/clang++')
    host_flags = ['-std=c++23', '-O1', '-g', '-fno-omit-frame-pointer',
                  '-DPLH_OWNED_MEMORY_HOST_TEST', '-DRC_HELPERS_BUILD_STATIC',
                  '-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC']
    includes = [VSP / 'include', VS1,
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
        'lmf_core.o': VSP / 'src/live_module_facts.cpp',
        'lmf_base_scan.o': VSP / 'src/live_module_base_scan.cpp',
        'adapter.o': VS1 / 'lmf_to_native_loader_facts.cpp',
        'NativeLoaderContext.o': SP / 'ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp',
        'OwnedMappedScanSource.o': SP / 'ue4ss-native-tick-file-owner-1/src/OwnedMappedScanSource.cpp',
        'OwnedSerializedElfSource.o': SP / 'ue4ss-native-tick-file-owner-1/src/OwnedSerializedElfSource.cpp',
        'OwnedMappedCandidateSelection.o': SP / 'ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedCandidateSelection.cpp',
        'OwnedMappedResolverCollector.o': SP / 'ue4ss-native-mapped-collector-properties-1/src/OwnedMappedResolverCollector.cpp',
        'test_lmf_adapter_host.o': WK / 'host_tests/test_lmf_adapter_host.cpp',
    }
    objects = []
    for obj, src in units.items():
        argv = [cxx, *host_flags, *inc, '-DADAPTER_FIXTURE_EBOOT="' +
                str(WK / 'host_tests/fixtures/eboot.bin') + '"',
                '-DADAPTER_FIXTURE_MODULE="' + str(WK / 'host_tests/fixtures/ue4ss.so') + '"',
                '-c', src, '-o', OUT / obj]
        run(obj.replace('.o', '-compile'), argv)
        objects.append(OUT / obj)

    memory_o = SP / 'ue4ss-native-mapped-candidate-selection-1/host-harness-1/memory.o'
    argv = [cxx, '-Wl,-undefined,dynamic_lookup', *objects, memory_o, '-o',
            OUT / 'supplier-ingame-host-tests-vendored']
    run('link', argv)
    out = run('run', [OUT / 'supplier-ingame-host-tests-vendored'])
    receipt['steps']['host_run'] = out.strip()
    print(out.strip())

    # ---- pinned SDK cross-compile of the target TUs (vendored sources) ----
    sdk = Path(os.environ.get('LMF_SDK', SDK_DEFAULT)).resolve()
    compiler = sdk / 'bin/prospero-clang++'
    if not compiler.is_file():
        raise SystemExit(f'pinned SDK compiler missing: {compiler}')
    receipt['toolchain'] = {'compiler': str(compiler), 'compiler_sha256': sha(compiler)}
    llvm = Path(os.environ.get('LMF_LLVM', LLVM_DEFAULT)).resolve()
    env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config')) \
        if (llvm / 'llvm-config').exists() else dict(os.environ)
    native_flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fPIC',
                    '-I', VSP / 'include']
    sce = VSP / 'src/live_module_os_ops_sce.cpp'
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
            [compiler, *native_flags, '-c', VSP / tu, '-o', obj], env)
    # NOTE: the adapter TU is deliberately NOT compiled here with -Wall -Wextra
    # -Werror: the worker suite never SDK-compiled it, and composition compiles
    # it via the loader-context c++23 template (host adapter.o above covers its
    # symbol evidence).
        data = obj.read_bytes()
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (1, 62):
            raise SystemExit(f'unexpected object format for {obj}')

    receipt['status'] = 'VENDORED_TREE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED'
    receipt['objects'] = {o.name: sha(o) for o in OUT.glob('*.o')}
    (OUT / 'receipt.json').write_text(json.dumps(receipt, indent=1))
    print(receipt['status'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
