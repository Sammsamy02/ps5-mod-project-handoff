#!/usr/bin/env python3
"""Focused offline writer tests/cross-tool checks or unexecuted PS5 link."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[5]
HERE = Path(__file__).resolve().parent
FORMAT = HERE.parent
PAK = FORMAT.parent / 'pak'
REPAK_SHA256 = '57b321cca5714997e6aad7300e713c5137a5ea7fa60b3281bd8eb322c4e36960'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mbedtls-source', type=Path, required=True)
    parser.add_argument('--zlib-source', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--ps5-sdk', type=Path)
    parser.add_argument('--llvm-config', type=Path)
    parser.add_argument('--resource-dir', type=Path)
    args = parser.parse_args()
    native = bool(args.ps5_sdk)
    if native != bool(args.llvm_config and args.resource_dir):
        parser.error('SDK, llvm-config and resource-dir required together')
    if not native and (args.llvm_config or args.resource_dir):
        parser.error('target options require SDK')
    if not native and not args.zlib_source:
        parser.error('canonical host reader requires pinned Zlib source')
    mb = args.mbedtls_source.resolve()
    zlib = args.zlib_source.resolve() if args.zlib_source else None
    lock_path = PAK/'dependency-lock.json'
    lock = json.loads(lock_path.read_text())
    for kind, directory in [('mbedtls', mb)] + ([] if native else [('zlib', zlib)]):
        for row in lock[kind]['files']:
            p = directory/row['path']
            if not p.is_file() or digest(p) != row['sha256']:
                raise SystemExit('pinned dependency mismatch: ' + str(p))
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    compiler = args.cc
    flags = ['-std=c11', '-O2', '-fno-strict-aliasing', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"', '-I'+str(FORMAT), '-I'+str(PAK),
             '-I'+str(mb/'include'), '-I'+str(mb/'library')]
    if native:
        compiler = str(args.ps5_sdk.resolve()/'bin/prospero-clang')
        env['LLVM_CONFIG'] = str(args.llvm_config.resolve())
        for name in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'SDKROOT'):
            env.pop(name, None)
        flags += ['-resource-dir', str(args.resource_dir.resolve())]
        selected_manifest = FORMAT.parent/'llvm_selected_manifest.json'
        if digest(selected_manifest) != '7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896':
            raise SystemExit('selected resource manifest mismatch')
        if digest(args.llvm_config.resolve()) != '39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5':
            raise SystemExit('pinned llvm-config mismatch')
        if digest(Path(compiler)) != '3dcf3139662db7b0f796c2073bc8f673a5557e48bc24d2e413896d33847a69db':
            raise SystemExit('pinned SDK C wrapper mismatch')
    else:
        flags += ['-DZ_SOLO', '-DNO_GZIP', '-DZLIB_CONST', '-I'+str(zlib)]
    sources = [FORMAT/'pak_writer.c', mb/'library/sha1.c', mb/'library/platform_util.c']
    if not native:
        sources += [PAK/n for n in ('pak_candidate.c', 'pak_candidate_io.c', 'pak_candidate_reconcile.c',
            'pak_primary_capture.c', 'pak_secondary_integrity.c', 'pak_directory_parser.c',
            'pak_reference_graph.c', 'pak_physical_header.c', 'pak_reconciled_payload.c',
            'pak_reader.c', 'pak_entry_decoder.c', 'bounded_zlib.c')]
        sources += [zlib/n for n in ('inflate.c', 'inftrees.c', 'inffast.c', 'adler32.c', 'zutil.c')]
    callers = [HERE/'native_caller.c'] if native else [HERE/'writer_test.c', HERE/'candidate_check.c']
    inputs = sources + callers + [FORMAT/'pak_writer.h', lock_path, HERE/'inspect_generated.py',
                                 Path(__file__).resolve(), *sorted(PAK.glob('*.h'))]
    receipt = {'target': 'ps5-link-only' if native else 'host-controlled-fixtures', 'target_executed': False,
        'format': 'classic_v11_None', 'runtime_supported': False, 'installation_supported': False,
        'commands': [], 'result': 'INCOMPLETE',
        'inputs': [{'path': str(p), 'sha256': digest(p)} for p in inputs],
        'environment_overrides': {'LLVM_CONFIG': env['LLVM_CONFIG']} if native else {},
        'source_units': len(sources) + len(callers)}

    def save():
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')

    def run(name, argv, expected_exit=0):
        argv = [str(x) for x in argv]
        start = time.monotonic()
        p = subprocess.run(argv, env=env, capture_output=True, timeout=60)
        (out/(name+'.stdout')).write_bytes(p.stdout)
        (out/(name+'.stderr')).write_bytes(p.stderr)
        receipt['commands'].append({'argv': argv, 'exit': p.returncode,
            'elapsed_seconds': round(time.monotonic()-start, 6),
            'stdout': name+'.stdout', 'stderr': name+'.stderr'})
        save()
        if p.returncode != expected_exit:
            receipt['result'] = 'FAIL'
            receipt['failed_command'] = name
            save()
            raise SystemExit('command failed: '+name+'\n'+p.stdout.decode(errors='replace')[-3000:]+
                             p.stderr.decode(errors='replace')[-3000:])
        return p.stdout

    save(); run('compiler', [compiler, '--version'])
    objects = []
    for source in sources:
        obj = out/(source.stem+'.o')
        run(source.stem, [compiler, *flags, '-MD', '-MF', out/(source.stem+'.d'),
                          '-c', source, '-o', obj])
        objects.append(obj)
    binaries = {}
    for caller in callers:
        obj = out/(caller.stem+'.o')
        run(caller.stem+'-compile', [compiler, *flags, '-MD', '-MF', out/(caller.stem+'.d'),
                                    '-c', caller, '-o', obj])
        binary = out/(caller.stem+('.elf' if native else ''))
        closure = objects[:3] if caller.stem == 'writer_test' else objects
        run(caller.stem+'-link', [compiler, *flags, *closure, obj,
                                *(['-Wl,-z,now'] if native else []), '-o', binary])
        binaries[caller.stem] = binary
    if native:
        llvm_bin = args.llvm_config.resolve().parent
        if digest(llvm_bin/'llvm-readobj') != '73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30':
            raise SystemExit('pinned readobj mismatch')
        dependencies = set()
        for dep in out.glob('*.d'):
            text = dep.read_text().replace('\\\n', ' ')
            dependencies.update(Path(p).resolve() for p in shlex.split(text.split(':', 1)[1]))
        resource = args.resource_dir.resolve()
        members = json.loads(selected_manifest.read_text())['members']
        rows = []
        for path in sorted(dependencies):
            sha = digest(path)
            resource_pin = False
            if path.is_relative_to(resource):
                member = 'lib/clang/21/'+path.relative_to(resource).as_posix()
                if members.get(member) != sha:
                    raise SystemExit('used resource header mismatch: '+str(path))
                resource_pin = True
            rows.append({'path': str(path), 'sha256': sha, 'selected_resource_pin_verified': resource_pin})
        receipt['used_dependencies'] = rows
        receipt['toolchain_inputs'] = [{'path': str(p), 'sha256': digest(p)} for p in
            (Path(compiler), args.llvm_config.resolve(), llvm_bin/'clang', selected_manifest)]
        info = run('elf', [llvm_bin/'llvm-readobj', '--file-headers', '--dynamic-table',
                           '--dyn-symbols', binaries['native_caller']]).decode()
        if ('EM_X86_64' not in info or 'Type: SharedObject' not in info or
                'OS/ABI: FreeBSD' not in info or 'BIND_NOW' not in info):
            raise SystemExit('expected native ELF/BIND_NOW missing')
        receipt['elf_contract'] = 'FreeBSD x86_64 ET_DYN/BIND_NOW link-only; entry/runtime unproved'
        imports = []
        for block in info.split('Symbol {')[1:]:
            name = re.search(r'Name: (\S+)', block)
            if name and 'Section: Undefined' in block:
                imports.append(name.group(1))
        allowed = {'memcpy', 'memmove', 'memset', 'free'}
        if not set(imports) <= allowed:
            raise SystemExit('unexpected import: '+repr(imports))
        writer_symbols = run('writer-symbols', [llvm_bin/'llvm-nm', '-u', out/'pak_writer.o']).decode()
        if re.search(r'\b(open|read|write|socket|malloc|calloc|realloc|system|execve)\b', writer_symbols):
            raise SystemExit('writer unexpectedly depends on I/O/allocation')
        receipt['imports'] = sorted(imports)
        receipt['native_tool_inputs'] = [{'path': str(llvm_bin/n), 'sha256': digest(llvm_bin/n)}
                                        for n in ('llvm-readobj', 'llvm-nm')]
    else:
        summary = run('writer-test', [binaries['writer_test'], out]).decode()
        match = re.search(r'groups=(\d+) checks=(\d+) failures=0 ', summary)
        if not match:
            raise SystemExit('writer success summary missing')
        receipt['writer_groups'], receipt['writer_checks'] = map(int, match.groups())
        run('independent-geometry', [sys.executable, HERE/'inspect_generated.py', out])
        receipt['independent_geometry_receipt'] = 'independent-geometry.json'
        tool_receipt = ROOT/'.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/repak-build-receipt.json'
        tool = json.loads(tool_receipt.read_text()); repak = Path(tool['binary']['path'])
        if tool['source_commit'] != '355b5f62f51959c7cc6dd5a51708646ef483065d' or digest(repak) != REPAK_SHA256:
            raise SystemExit('repak identity mismatch')
        receipt['repak'] = {'path': str(repak), 'sha256': REPAK_SHA256,
            'source_commit': tool['source_commit'], 'build_receipt': str(tool_receipt)}
        from inspect_generated import EXPECTED
        extracted = []
        for name in ('prepared', 'custom', 'empty'):
            archive = out/(name+'.pak')
            mount = '../Shared/' if name == 'custom' else '../../../'
            run('repak-'+name+'-info', [repak, 'info', archive])
            listed = run('repak-'+name+'-list', [repak, 'list', '--strip-prefix', mount, archive]).decode().splitlines()
            expected = EXPECTED if name != 'empty' else {}
            if listed != sorted(expected):
                raise SystemExit('unexpected independent listed names: '+repr(listed))
            for i, path in enumerate(sorted(expected)):
                data = run('repak-'+name+'-get-'+str(i), [repak, 'get', '--strip-prefix', mount, archive, path])
                if data != expected[path]:
                    raise SystemExit('independent extraction mismatch: '+path)
                extracted.append({'archive': name, 'path': path, 'bytes': len(data),
                                  'sha256': hashlib.sha256(data).hexdigest(), 'exact_match': True})
        receipt['independent_extractions'] = extracted
        summary = run('candidate-test', [binaries['candidate_check'], out]).decode()
        if 'failures=0 outside_reads=0' not in summary:
            raise SystemExit('candidate success summary missing')
        receipt['candidate_summary'] = summary.strip()
    receipt['artifacts'] = [{'path': str(b), 'bytes': b.stat().st_size, 'sha256': digest(b)}
                            for b in binaries.values()]
    receipt['inputs_unchanged'] = all(digest(Path(row['path'])) == row['sha256'] for row in receipt['inputs'])
    if not receipt['inputs_unchanged']:
        raise SystemExit('input changed during check')
    receipt['result'] = 'PASS'; save()
    print(json.dumps({k: receipt[k] for k in ('result', 'target', 'source_units', 'target_executed')}))


if __name__ == '__main__':
    main()
