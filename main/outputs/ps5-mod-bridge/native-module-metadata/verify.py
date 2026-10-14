#!/usr/bin/env python3
"""Run owned-provider host tests and compile an isolated native metadata archive.

No native executable, entry point, deployment, provider resolution or target calls.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-cc', default='clang')
    parser.add_argument('--sdk', type=Path, default=ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk')
    parser.add_argument('--llvm-config', type=Path, default=ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
    parser.add_argument('--llvm-ar', type=Path, default=ROOT / '.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar')
    parser.add_argument('--output', type=Path, default=ROOT / '.superpowers/native-dependency-spike/native-module-metadata-1')
    parser.add_argument('--host-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True, exist_ok=False)
    receipt = {'status': 'IN_PROGRESS', 'commands': [], 'source_files': {},
               'target_execution': False, 'console_contact': False,
               'native_enumeration_available': False,
               'mapping_ownership_or_read_permission_proven': False}

    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')

    def run(label, argv, env=None):
        command = [str(item) for item in argv]
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (label + '.stdout')).write_text(result.stdout)
        (out / (label + '.stderr')).write_text(result.stderr)
        receipt['commands'].append({'label': label, 'argv': command, 'exit': result.returncode})
        save()
        if result.returncode:
            raise RuntimeError(f'{label} failed: {out / (label + ".stderr")}')
        return result.stdout

    try:
        for path in sorted(HERE.rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts:
                receipt['source_files'][str(path.relative_to(HERE))] = sha(path)
        provenance = json.loads((HERE / 'provenance.json').read_text())
        for pinned in provenance['files']:
            if sha(HERE / pinned['path']) != pinned['sha256']:
                raise RuntimeError('Pinned upstream source/license changed: ' + pinned['path'])
        sources = [HERE / 'module_metadata.c', HERE / 'native_provider.c']
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I', HERE]
        run('host-compile', [args.host_cc, *flags, *sources, HERE / 'tests/test_module_metadata.c', '-o', out / 'owned-provider-tests'])
        receipt['host_test_output'] = run('host-tests', [out / 'owned-provider-tests']).strip()
        if args.host_only:
            receipt['status'] = 'OWNED_PROVIDER_TESTS_PASSED'
        else:
            compiler = args.sdk.resolve() / 'bin/prospero-clang'
            llvm = args.llvm_config.resolve().parent
            env = dict(os.environ, LLVM_CONFIG=str(args.llvm_config.resolve()))
            receipt['toolchain'] = {'compiler': str(compiler), 'compiler_sha256': sha(compiler),
                                    'llvm_config': str(args.llvm_config.resolve()),
                                    'llvm_config_sha256': sha(args.llvm_config),
                                    'archiver': str(args.llvm_ar.resolve()),
                                    'archiver_sha256': sha(args.llvm_ar)}
            objects = []
            for source in sources:
                obj = out / (source.stem + '.o')
                run(source.stem + '-native-compile', [compiler, *flags, '-fPIC', '-MD', '-MF', out / (source.stem + '.d'), '-c', source, '-o', obj], env)
                data = obj.read_bytes()
                # ELF64 little-endian ET_REL, AMD64. No runnable ELF is produced.
                if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (1, 62):
                    raise RuntimeError('Unexpected target object format')
                objects.append(obj)
            symbols = run('native-undefined', [llvm / 'llvm-nm', '--undefined-only', '--format=posix', *objects])
            names = {line.split()[0] for line in symbols.splitlines() if len(line.split()) >= 2}
            if 'sceKernelGetModuleInfo' not in names or 'sceKernelGetModuleList' in names:
                raise RuntimeError('Unexpected native API import boundary')
            definitions = run('native-defined', [llvm / 'llvm-nm', '--defined-only', '--extern-only', '--format=posix', *objects])
            names = {line.split()[0] for line in definitions.splitlines() if len(line.split()) >= 2}
            expected = {'mb_module_metadata_read', 'mb_module_metadata_capture',
                        'mb_module_metadata_native_read', 'mb_module_metadata_native_capture'}
            if names != expected:
                raise RuntimeError('Unexpected exported symbols: ' + repr(names))
            archive = out / 'libmb_module_metadata.a'
            run('native-archive', [args.llvm_ar.resolve(), 'rcD', archive, *objects])
            receipt['native_outputs'] = {p.name: {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in [*objects, archive]}
            deps = {}
            for source in sources:
                text = (out / (source.stem + '.d')).read_text().replace('\\\n', ' ')
                for name in text.split()[1:]:
                    p = Path(name)
                    if p.is_file():
                        deps[str(p)] = sha(p)
            receipt['native_dependencies'] = deps
            receipt['status'] = 'OWNED_PROVIDER_TESTS_AND_NATIVE_ARCHIVE_PASSED'
        save()
        print(json.dumps({'receipt': str(out / 'receipt.json'), 'status': receipt['status']}, indent=2))
    except Exception as error:
        receipt['status'] = 'FAILED'
        receipt['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    main()
