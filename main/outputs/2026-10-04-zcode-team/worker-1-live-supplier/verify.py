#!/usr/bin/env python3
"""worker-1-live-supplier verification.

1. Host-compiles and runs the injected-fake-ops supplier tests (contract
   behavior only; no target claims).
2. Compiles ONLY this worker's new sources with the pinned v0.43 SDK wrapper
   (prospero-clang++) into AMD64 ELF relocatable objects; no link, no PRX,
   no target contact, no shared-source edits.
3. Records exact commands, hashes and the SCE TU's undefined symbols (the
   complete set of OS APIs the implementation imports) into build-receipts/.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]  # .../translation-core/what
SDK_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
LLVM_DEFAULT = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(step: str, argv, env=None) -> str:
    printable = ' '.join(str(a) for a in argv)
    print(f'[{step}] {printable}')
    proc = subprocess.run([str(a) for a in argv], capture_output=True, text=True, env=env)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr, file=sys.stderr)
        raise SystemExit(f'{step} failed with code {proc.returncode}')
    if proc.stdout.strip():
        print(proc.stdout.strip())
    return proc.stdout


def main() -> int:
    sdk = Path(os.environ.get('LMF_SDK', SDK_DEFAULT)).resolve()
    llvm = Path(os.environ.get('LMF_LLVM', LLVM_DEFAULT)).resolve()
    out = HERE / 'build-receipts' / 'run-1'
    out.mkdir(parents=True, exist_ok=True)
    receipt = {'steps': {}, 'source_files': {}}

    for path in sorted((HERE / 'src').glob('*')) + sorted((HERE / 'host_tests').glob('*')):
        receipt['source_files'][str(path.relative_to(HERE))] = sha(path)

    cxx = os.environ.get('LMF_HOST_CXX', 'clang++')
    flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', HERE / 'src']
    sources = [HERE / 'src/live_module_facts.cpp']
    test_src = [HERE / 'host_tests/test_live_supplier_host.cpp']
    run('host-compile', [cxx, *flags, *sources, *test_src, '-o', out / 'live-supplier-host-tests'])
    receipt['steps']['host_tests'] = run('host-run', [out / 'live-supplier-host-tests']).strip()

    compiler = sdk / 'bin/prospero-clang++'
    if not compiler.is_file():
        raise SystemExit(f'pinned SDK compiler missing: {compiler}')
    receipt['toolchain'] = {'compiler': str(compiler), 'compiler_sha256': sha(compiler)}
    env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config')) if (llvm / 'llvm-config').exists() else dict(os.environ)
    native_flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fPIC', '-I', HERE / 'src']
    objects = []
    for source in sources + [HERE / 'src/live_module_os_ops_sce.cpp']:
        obj = out / (source.stem + '.o')
        run(source.stem + '-sdk-compile', [compiler, *native_flags, '-c', source, '-o', obj], env)
        data = obj.read_bytes()
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (1, 62):
            raise SystemExit(f'unexpected object format for {obj}')
        objects.append(obj)
    undefined = run('sce-tu-undefined', [llvm / 'llvm-nm', '--undefined-only', '--format=posix', objects[-1]])
    receipt['steps']['sce_tu_undefined_symbols'] = undefined.strip().splitlines()
    # Neither the pinned LLVM dir nor the SDK wrapper can produce llvm-ar here
    # (LLVM_BINDIR gap prior art native-module-metadata also recorded). The
    # archive is a convenience bundle only; host ar is format-agnostic for the
    # already-verified AMD64 objects. Real evidence = the two compiled .o files.
    run('archive', ['/usr/bin/ar', 'rcs', out / 'liblmf_live_supplier.a', *objects])
    receipt['status'] = 'HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED'
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f"receipt: {out / 'receipt.json'}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
