#!/usr/bin/env python3
"""Focused owning-candidate proof; host corpus or target compile/link only."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import zipfile
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
ROOT = HERE.parents[3]

def digest(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda: f.read(65536), b''):
            h.update(b)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('host', 'native'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-receipt', type=Path, default=ROOT / '.superpowers/native-observations/nexus-real-mods-2026-10-01/leon-download-receipt.json')
    parser.add_argument('--ps5-sdk', type=Path)
    parser.add_argument('--llvm-config', type=Path)
    parser.add_argument('--resource-dir', type=Path)
    args = parser.parse_args()
    if args.phase == 'native' and not all((args.ps5_sdk, args.llvm_config, args.resource_dir)):
        parser.error('native requires SDK, LLVM config and resource directory')
    out = args.output.resolve()
    if out.exists():
        parser.error('use a fresh output directory')
    out.mkdir(parents=True)
    used = [HERE / f for f in ('prepared.h', 'prepared.c', 'groups.h', 'groups.c', 'intake.h', 'prepared_tests/prepared_test.c')]
    used.append(Path(__file__).resolve())
    receipt = {'scope': 'owning automatic container candidates; installation/platform/runtime unverified',
               'target_executed': False, 'installation_supported': False, 'commands': []}
    def run(name, argv, env=None, timeout=180):
        argv = [str(a) for a in argv]
        try:
            p = subprocess.run(argv, env=env, capture_output=True, timeout=timeout)
            status, stdout, stderr = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired as e:
            status, stdout, stderr = 'timeout', e.stdout or b'', e.stderr or b''
        (out / (name + '.stdout')).write_bytes(stdout)
        (out / (name + '.stderr')).write_bytes(stderr)
        receipt['commands'].append({'name': name, 'argv': argv, 'exit': status,
                                   'stdout': name + '.stdout', 'stderr': name + '.stderr'})
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if status != 0:
            raise RuntimeError(name + ' failed: ' + stderr.decode(errors='replace')[-1500:])
        return stdout, stderr
    def pin_inputs():
        receipt['inputs'] = [{'path': str(p), 'sha256': digest(p)} for p in used]
    common = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(HERE)]
    try:
        if args.phase == 'host':
            fixture = json.loads(args.fixture_receipt.read_text())
            archive = Path(fixture['archive']).resolve()
            if archive.stat().st_size != int(fixture['bytes']) or digest(archive) != fixture['sha256']:
                raise RuntimeError('actual fixture identity differs')
            deps = ROOT / '.superpowers/native-dependency-spike'
            closure = deps / 'native-libarchive-lzma-experiment-1/closure-3/descriptor.json'
            descriptor = json.loads(closure.read_text())
            crypto = PROJECT / 'native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8'
            mbed = crypto / 'sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5'
            libraries = [Path(descriptor['targets']['host'][k]['path']) for k in ('libarchive', 'liblzma', 'zlib')]
            for k, p in zip(('libarchive', 'liblzma', 'zlib'), libraries):
                if digest(p) != descriptor['targets']['host'][k]['sha256']:
                    raise RuntimeError('pinned library differs: ' + k)
            libraries.append(crypto / 'libmbedcrypto.a')
            used += [HERE / 'intake.c', HERE / 'private/posix_compat.c', HERE / 'private/posix_compat.h',
                     HERE / 'prepared_tests/corpus_prepared_test.c',
                     args.fixture_receipt.resolve(), archive, closure, *libraries]
            pin_inputs()
            unit = out / 'prepared-test'
            run('unit-compile', ['/usr/bin/clang', *common, HERE/'prepared.c', HERE/'groups.c', HERE/'prepared_tests/prepared_test.c', '-o', unit])
            stdout, _ = run('unit-test', [unit])
            if not re.search(rb'cases=\d+ checks=\d+ failures=0\n', stdout):
                raise RuntimeError('unit behavioral summary absent')
            corpus = out / 'corpus-prepared-test'
            flags = [*common, '-DMB_ARCHIVE_BUILTIN_XZ=1', '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"',
                     '-I'+str(mbed/'include'), '-I'+str(crypto/'config'), '-I'+str(deps/'libarchive-release/libarchive')]
            run('corpus-compile', ['/usr/bin/clang', *flags, HERE/'prepared.c', HERE/'groups.c', HERE/'intake.c',
                                   HERE/'private/posix_compat.c',
                                   HERE/'prepared_tests/corpus_prepared_test.c', *libraries, '-o', corpus])
            q = out / 'quarantine'
            q.mkdir(mode=0o700)
            stdout, _ = run('actual-corpus', [corpus, archive, fixture['sha256'], q])
            records = []
            for line in stdout.decode().splitlines():
                if line.startswith('FILE\t'):
                    _, row, role, size, sha, source, destination, device, inode = line.split('\t')
                    records.append({'row_index': int(row), 'role': int(role), 'bytes': int(size), 'sha256': sha,
                                    'source_name': source, 'destination_name': destination, 'device': int(device), 'inode': int(inode)})
            if len(records) != 3 or [r['role'] for r in records] != [2, 1, 3]:
                raise RuntimeError('actual complete trio/publication order not observed')
            with zipfile.ZipFile(archive) as z:
                entries = z.infolist()
                expected = {i for i, entry in enumerate(entries) if not entry.is_dir() and Path(entry.filename).suffix.lower() in {'.pak', '.utoc', '.ucas'}}
                if {r['row_index'] for r in records} != expected:
                    raise RuntimeError('automatic candidate membership differs from independent ZIP rows')
                for r in records:
                    entry = entries[r['row_index']]
                    h = hashlib.sha256()
                    with z.open(entry) as stream:
                        for b in iter(lambda: stream.read(65536), b''):
                            h.update(b)
                    original = h.hexdigest()
                    p = q / r['source_name']
                    if r['bytes'] != entry.file_size or r['sha256'] != original or p.stat().st_size != entry.file_size or digest(p) != original:
                        raise RuntimeError('unchanged materialized member differs from ZIP oracle')
                    if (p.stat().st_dev, p.stat().st_ino) != (r['device'], r['inode']):
                        raise RuntimeError('prepared owned identity differs from actual file')
                    r['original_member_name'] = entry.filename
            stems = {Path(r['destination_name']).stem for r in records}
            if len(stems) != 1 or any('/' in r['destination_name'] or '\\' in r['destination_name'] for r in records):
                raise RuntimeError('trio destination stem or flat namespace differs')
            receipt['actual'] = {'archive': str(archive), 'source_sha256': fixture['sha256'],
                                 'records': records, 'stdout': stdout.decode(), 'all_original_bytes_equal': True,
                                 'manifest_published': (q/'manifest-v1.bin').is_file(), 'game_written': False}
        else:
            sys.path.insert(0, str(HERE))
            spec = importlib.util.spec_from_file_location('prepared_existing_tool_pins', HERE/'groups_verify.py')
            pins = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(pins)
            from link_layout import write_linker_script, validate_dynamic_layout
            sdk = args.ps5_sdk.resolve()
            llvm = args.llvm_config.resolve()
            resource = args.resource_dir.resolve()
            compiler = sdk/'bin/prospero-clang'
            selected = HERE.parent/'llvm_selected_manifest.json'
            sdk_archive = PROJECT/'native-probe/build/deps/sdk.zip'
            required = {compiler: pins.TOOLS['prospero-clang'], llvm: pins.TOOLS['llvm-config'],
                        llvm.parent/'clang': pins.TOOLS['clang'], llvm.parent/'llvm-nm': pins.TOOLS['llvm-nm'],
                        llvm.parent/'llvm-readobj': pins.TOOLS['llvm-readobj'], selected: pins.LLVM_MANIFEST_SHA256,
                        sdk_archive: pins.SDK_ARCHIVE_SHA256}
            for p, sha in required.items():
                if digest(p) != sha:
                    raise RuntimeError('pinned tool/SDK differs: '+str(p))
            receipt['link_profile'] = write_linker_script(sdk, out)
            script = Path(receipt['link_profile']['caller_script'])
            used += [HERE/'groups_verify.py', HERE/'link_layout.py', sdk/'ldscripts/elf_x86_64.x', script]
            pin_inputs()
            env = dict(os.environ, LLVM_CONFIG=str(llvm))
            for k in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'SDKROOT'):
                env.pop(k, None)
            flags = [*common, '-resource-dir', str(resource)]
            objects = []
            for p in (HERE/'prepared.c', HERE/'groups.c', HERE/'prepared_tests/prepared_test.c'):
                obj = out/(p.stem+'.o')
                run(p.stem+'-compile', [compiler, *flags, '-MD', '-MF', out/(p.stem+'.d'), '-c', p, '-o', obj], env)
                objects.append(obj)
            elf = out/'archive-prepared.elf'
            run('strict-link', [compiler, *flags, *objects, '-Wl,--no-undefined,-z,now', '-Wl,-T,'+str(script),
                                '-Wl,-Map='+str(out/'link.map'), '-o', elf], env)
            layout = validate_dynamic_layout(elf)
            stdout, stderr = run('elf', [llvm.parent/'llvm-readobj', '--file-headers', '--needed-libs', '--dynamic-table', elf], env)
            if stderr or b'OS/ABI: FreeBSD' not in stdout or b'BIND_NOW' not in stdout:
                raise RuntimeError('native static ELF contract differs')
            imports, _ = run('imports', [llvm.parent/'llvm-nm', '--dynamic', '--undefined-only', elf], env)
            names = sorted({line.split()[-1].decode() for line in imports.splitlines() if line.strip()})
            exports = set()
            for library in ('libkernel_web.so', 'libSceLibcInternal.so', 'libSceNet.so'):
                text, _ = run('exports-'+library, [llvm.parent/'llvm-nm', '--dynamic', '--defined-only', sdk/'target/lib'/library], env)
                exports.update(line.split()[-1].decode() for line in text.splitlines() if line.strip())
            if set(names)-exports:
                raise RuntimeError('undefined project/non-SDK import')
            used_headers = set()
            for d in out.glob('*.d'):
                used_headers.update(Path(p).resolve() for p in shlex.split(d.read_text().replace('\\\n', ' ').split(':', 1)[1]))
            selected_data = json.loads(selected.read_text())
            headers = []
            with zipfile.ZipFile(sdk_archive) as z:
                for p in sorted(used_headers):
                    if p.is_relative_to(sdk):
                        if z.read('ps5-payload-sdk/'+p.relative_to(sdk).as_posix()) != p.read_bytes():
                            raise RuntimeError('SDK header differs: '+str(p))
                        kind = 'exact_sdk_zip'
                    elif p.is_relative_to(resource):
                        if digest(p) != selected_data['members'].get('lib/clang/21/'+p.relative_to(resource).as_posix()):
                            raise RuntimeError('resource header differs: '+str(p))
                        kind = 'selected_resource_manifest'
                    elif p.is_relative_to(HERE):
                        kind = 'owned_or_existing_archive_source'
                    else:
                        raise RuntimeError('foreign header dependency: '+str(p))
                    headers.append({'path': str(p), 'sha256': digest(p), 'kind': kind})
            for name in ('prepared.o', 'groups.o'):
                undefined, _ = run('pure-imports-'+name, [llvm.parent/'llvm-nm', '--undefined-only', out/name], env)
                pure = {line.split()[-1].decode() for line in undefined.splitlines() if line.strip()}
                if pure-{'memcmp', 'memcpy', 'memset', 'strlen', 'mb_archive_groups_v1'}:
                    raise RuntimeError('unexpected pure operation dependency: '+repr(pure))
            receipt['native'] = {'elf': str(elf), 'sha256': digest(elf), 'bytes': elf.stat().st_size,
                                 'imports': names, 'imports_missing_from_sdk': [], 'header_closure': headers,
                                 'dynamic_layout': layout, 'readobj_stderr': stderr.decode(), 'target_executed': False}
        receipt['inputs_unchanged'] = all(digest(x['path']) == x['sha256'] for x in receipt['inputs'])
        if not receipt['inputs_unchanged']:
            raise RuntimeError('frozen input changed during proof')
        receipt['result'] = 'PASS'
    except Exception as e:
        receipt['result'] = 'FAIL'
        receipt['error'] = str(e)
        raise
    finally:
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps({'phase': args.phase, 'result': receipt['result'], 'receipt': str(out/'receipt.json')}))

if __name__ == '__main__':
    main()
