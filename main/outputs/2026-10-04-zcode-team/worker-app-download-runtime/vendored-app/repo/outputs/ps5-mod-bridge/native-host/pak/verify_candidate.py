#!/usr/bin/env python3
"""Offline focused candidate checks or PS5 compile/link; never dispatch."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--mbedtls-source', type=Path, required=True)
    parser.add_argument('--zlib-source', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--ps5-sdk', type=Path)
    parser.add_argument('--llvm-config', type=Path)
    parser.add_argument('--resource-dir', type=Path)
    args = parser.parse_args()
    if bool(args.ps5_sdk) != bool(args.llvm_config and args.resource_dir):
        parser.error('PS5 compilation requires SDK, llvm-config and resource-dir together')
    if not args.ps5_sdk and (args.llvm_config or args.resource_dir):
        parser.error('target options require --ps5-sdk')
    library = args.library.resolve()
    deps = {'mbedtls': args.mbedtls_source.resolve(), 'zlib': args.zlib_source.resolve()}
    lock = json.loads((library / 'dependency-lock.json').read_text())
    for kind, root in deps.items():
        for item in lock[kind]['files']:
            p = root / item['path']
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != item['sha256']:
                raise SystemExit('Missing or mismatched pinned dependency: ' + str(p))
    fixtures = library / 'tests/fixtures'
    for item in json.loads((fixtures / 'provenance.json').read_text())['records']:
        data = (fixtures / item['local']).read_bytes()
        if len(data) != item['bytes'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise SystemExit('Independent fixture mismatch: ' + item['local'])
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    compiler = args.cc
    flags = ['-std=c11', '-O2', '-fno-strict-aliasing', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-DZ_SOLO', '-DNO_GZIP', '-DZLIB_CONST', '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"',
             '-I' + str(library), '-I' + str(deps['zlib']), '-I' + str(deps['mbedtls'] / 'include'),
             '-I' + str(deps['mbedtls'] / 'library')]
    if args.ps5_sdk:
        compiler = str(args.ps5_sdk.resolve() / 'bin/prospero-clang')
        env['LLVM_CONFIG'] = str(args.llvm_config.resolve())
        flags += ['-resource-dir', str(args.resource_dir.resolve())]
    sources = [library / name for name in (
        'pak_candidate.c', 'pak_candidate_io.c', 'pak_candidate_reconcile.c',
        'pak_primary_capture.c', 'pak_secondary_integrity.c', 'pak_directory_parser.c',
        'pak_reference_graph.c', 'pak_physical_header.c', 'pak_reconciled_payload.c',
        'pak_reader.c', 'pak_entry_decoder.c', 'bounded_zlib.c')]
    sources += [deps['zlib'] / n for n in ('inflate.c', 'inftrees.c', 'inffast.c', 'adler32.c', 'zutil.c')]
    sources += [deps['mbedtls'] / 'library' / n for n in ('sha1.c', 'platform_util.c')]
    callers = {name: library / 'tests' / (name + '.c') for name in (
        'candidate_fixture_test', 'large_none_candidate_test')}
    generators = [library / 'tests' / name for name in (
        'generate_candidate_fixtures.py', 'generate_large_none_fixtures.py')]
    receipt = {'target': 'ps5-link-only' if args.ps5_sdk else 'host', 'target_executed': False,
               'installation_supported': False, 'commands': [],
               'common_source_units': len(sources), 'source_units': len(sources) + len(callers),
               'inputs': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                          for p in sources + list(callers.values()) + list(library.glob('*.h')) + generators + [Path(__file__).resolve()]]}
    def run(argv, name):
        proc = subprocess.run([str(p) for p in argv], env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=60)
        (out / name).write_bytes(proc.stdout)
        receipt['commands'].append({'argv': [str(p) for p in argv], 'exit_code': proc.returncode, 'log': name})
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if proc.returncode:
            raise SystemExit(proc.stdout.decode(errors='replace')[-6000:] or 'command failed')
        return proc.stdout
    run([compiler, '--version'], 'compiler.log')
    objects = []
    for source in sources:
        obj = out / (source.stem + '.o')
        run([compiler, *flags, '-c', source, '-o', obj], source.stem + '.log')
        objects.append(obj)
    binaries = {}
    for name, caller in callers.items():
        obj = out / (name + '.o')
        run([compiler, *flags, '-c', caller, '-o', obj], name + '.log')
        binary = out / (name + ('.elf' if args.ps5_sdk else ''))
        run([compiler, *flags, *objects, obj, *(['-Wl,-z,now'] if args.ps5_sdk else []), '-o', binary],
            'link.log' if name == 'candidate_fixture_test' else 'large_none_link.log')
        binaries[name] = binary
    if not args.ps5_sdk:
        # This script constructs only local test inputs in the chosen output dir.
        run(['python3', generators[0], out / 'synthetic'], 'generate.log')
        run([binaries['candidate_fixture_test'], fixtures, out / 'synthetic'], 'tests.log')
        run(['python3', generators[1], out / 'large-none'], 'large_none_generate.log')
        run([binaries['large_none_candidate_test'], out / 'large-none'], 'large_none_tests.log')
    receipt['artifacts'] = {name: {'path': str(binary), 'bytes': binary.stat().st_size,
                                 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
                            for name, binary in binaries.items()}
    receipt['artifact'] = receipt['artifacts']['candidate_fixture_test']
    receipt['inputs_unchanged'] = all(hashlib.sha256(Path(i['path']).read_bytes()).hexdigest() == i['sha256']
                                      for i in receipt['inputs'])
    receipt['result'] = 'PASS' if receipt['inputs_unchanged'] else 'SOURCE_CHANGED'
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if not receipt['inputs_unchanged']:
        raise SystemExit('Source changed during verification')
    print(json.dumps({'result': 'PASS', 'target': receipt['target'], 'target_executed': False,
                      'installation_supported': False}))

if __name__ == '__main__':
    main()
