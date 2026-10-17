#!/usr/bin/env python3
"""Focused Zen summary/name-map host fixtures or pinned PS5 compile/link only."""

from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(1024 * 1024):
            value.update(block)
    return value.hexdigest()


def rooted_file(relative, expected_bytes=None, expected_sha256=None):
    path = (ROOT / relative).resolve(strict=True)
    if not path.is_relative_to(ROOT):
        raise ValueError('profile path escaped workspace root')
    if not path.is_file():
        raise ValueError('profile input is not a regular file')
    if expected_bytes is not None and path.stat().st_size != expected_bytes:
        raise ValueError('profile input size mismatch: ' + str(path))
    actual = digest(path)
    if expected_sha256 is not None and actual != expected_sha256:
        raise ValueError('profile input SHA256 mismatch: ' + str(path))
    return path, actual


def load_schema_inputs(lock_path):
    lock = json.loads(lock_path.read_text())
    if lock.get('cue4parse_commit') != '28f385e622468c4ab84997d596b46bb1c3adfcfa':
        raise ValueError('unexpected CUE4Parse schema pin')
    acquisition, acquisition_sha = rooted_file(
        lock['acquisition_receipt'], expected_sha256=lock['acquisition_receipt_sha256'])
    rows = [{'path': str(acquisition), 'sha256': acquisition_sha}]
    for relative, expected in lock['sources'].items():
        path, actual = rooted_file(relative, expected_sha256=expected)
        rows.append({'path': str(path), 'sha256': actual})
    return rows


def run_command(receipt, out, name, argv, env, timeout=60):
    argv = [str(part) for part in argv]
    started = time.monotonic()
    proc = subprocess.run(argv, env=env, capture_output=True, timeout=timeout)
    stdout_name, stderr_name = name + '.stdout', name + '.stderr'
    (out / stdout_name).write_bytes(proc.stdout)
    (out / stderr_name).write_bytes(proc.stderr)
    receipt['commands'].append({
        'argv': argv,
        'exit': proc.returncode,
        'elapsed_seconds': round(time.monotonic() - started, 6),
        'stdout': stdout_name,
        'stderr': stderr_name,
    })
    save_receipt(receipt, out)
    if proc.returncode:
        raise RuntimeError(name + ' failed: ' +
            proc.stdout.decode(errors='replace')[-2000:] +
            proc.stderr.decode(errors='replace')[-2000:])
    return proc.stdout


def save_receipt(receipt, out):
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


def host_run(args, receipt, out, schema_inputs):
    profile_path = args.actual_profile.resolve(strict=True)
    if not profile_path.is_relative_to(ROOT) or not profile_path.is_file():
        raise ValueError('actual fixture profile must be a workspace file')
    profile = json.loads(profile_path.read_text())
    if profile.get('schema_version') != 1 or profile.get('profile') != {
        'summary_layout': 'UE5_3_PLUS', 'verse_cells_present': True
    }:
        raise ValueError('actual profile is outside the explicit test layout')
    headers = profile.get('headers')
    if not isinstance(headers, list) or len(headers) != 2:
        raise ValueError('expected exactly two bounded actual header slices')

    inputs = list(schema_inputs)
    inputs.extend([
        {'path': str(HERE / 'summary.c'), 'sha256': digest(HERE / 'summary.c')},
        {'path': str(HERE / 'summary.h'), 'sha256': digest(HERE / 'summary.h')},
        {'path': str(HERE / 'tests' / 'summary_test.c'), 'sha256': digest(HERE / 'tests' / 'summary_test.c')},
        {'path': str(profile_path), 'sha256': digest(profile_path)},
    ])
    ucas, ucas_sha = rooted_file(profile['ucas']['path'], profile['ucas']['bytes'], profile['ucas']['sha256'])
    utoc, utoc_sha = rooted_file(profile['utoc']['path'], profile['utoc']['bytes'], profile['utoc']['sha256'])
    inputs += [
        {'path': str(utoc), 'sha256': utoc_sha},
        {'path': str(ucas), 'sha256': ucas_sha},
    ]
    receipt['inputs'] = inputs
    receipt['actual_fixture_profile'] = str(profile_path)
    receipt['actual_read_scope'] = {'utoc_bytes_hashed': utoc.stat().st_size, 'ucas_bytes_hashed': ucas.stat().st_size,
        'selected_package_header_bytes_read': sum(int(row['header_bytes']) for row in headers)}

    cc = args.cc
    run_command(receipt, out, 'compiler-version', [cc, '--version'], os.environ.copy())
    binary = out / 'summary-test'
    run_command(receipt, out, 'host-build', [cc, '-std=c11', '-O2', '-Wall', '-Wextra',
        '-Wpedantic', '-Werror', '-I' + str(HERE), HERE / 'summary.c',
        HERE / 'tests' / 'summary_test.c', '-o', binary], os.environ.copy())
    output = run_command(receipt, out, 'synthetic-tests', [binary], os.environ.copy()).decode()
    match = re.search(r'zen-summary groups=(\d+) checks=(\d+) failures=0\s*$', output)
    if not match:
        raise RuntimeError('focused C test success summary missing')
    receipt['synthetic_groups'], receipt['synthetic_checks'] = map(int, match.groups())

    observations = []
    for index, row in enumerate(headers):
        offset, length = int(row['physical_offset']), int(row['header_bytes'])
        if not 1 <= length <= 256 * 1024 or offset > ucas.stat().st_size or length > ucas.stat().st_size - offset:
            raise ValueError('actual header slice outside the bounded profile')
        target = out / ('actual-header-' + str(index) + '.bin')
        with ucas.open('rb') as stream:
            stream.seek(offset)
            captured = stream.read(length)
        if len(captured) != length or hashlib.sha256(captured).hexdigest() != row['sha256']:
            raise ValueError('selected actual header receipt mismatch')
        target.write_bytes(captured)
        raw = run_command(receipt, out, 'actual-header-' + str(index),
            [binary, '--actual', target, str(row['logical_package_bytes'])], os.environ.copy()).decode()
        parsed = json.loads(raw.strip())
        expected = row['expected']
        if any(parsed.get(key) != expected_value for key, expected_value in expected.items()):
            raise RuntimeError('actual header disagrees with retained source-backed receipt: ' + str(index))
        observations.append({
            'fixture_id': row['fixture_id'], 'offset': offset, 'bytes': length,
            'sha256': hashlib.sha256(captured).hexdigest(), 'observed': parsed,
            'matches_source_backed_receipt': True,
        })
    receipt['actual_header_observations'] = observations
    receipt['actual_full_payload_decoded'] = False
    receipt['target_executed'] = False
    receipt['result'] = 'PASS'


def native_run(args, receipt, out, schema_inputs):
    sdk = args.ps5_sdk.resolve(strict=True)
    config = args.llvm_config.resolve(strict=True)
    resource = args.resource_dir.resolve(strict=True)
    compiler = sdk / 'bin' / 'prospero-clang'
    llvm_bin = config.parent
    selected = HERE.parent / 'llvm_selected_manifest.json'
    expected = {
        compiler: '3dcf3139662db7b0f796c2073bc8f673a5557e48bc24d2e413896d33847a69db',
        config: '39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
        llvm_bin / 'clang': '8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
        llvm_bin / 'llvm-readobj': '73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30',
        llvm_bin / 'llvm-nm': 'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
        selected: '7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896',
    }
    # Keep the exact pinned paths and hashes; never fall back to host tools.
    for path, expected_sha in expected.items():
        if digest(path) != expected_sha:
            raise ValueError('pinned compiler/tool manifest mismatch: ' + str(path))
    if not resource.is_dir():
        raise ValueError('pinned resource directory unavailable')
    manifest = json.loads(selected.read_text())
    members = manifest['members']
    inputs = list(schema_inputs) + [
        {'path': str(HERE / 'summary.c'), 'sha256': digest(HERE / 'summary.c')},
        {'path': str(HERE / 'summary.h'), 'sha256': digest(HERE / 'summary.h')},
        {'path': str(HERE / 'tests' / 'native_caller.c'), 'sha256': digest(HERE / 'tests' / 'native_caller.c')},
    ]
    inputs += [{'path': str(path), 'sha256': digest(path)} for path in expected]
    receipt['inputs'] = inputs
    receipt['native_profile'] = 'pinned PS5 compile/link only; source/profile fields remain external'
    env = dict(os.environ)
    env['LLVM_CONFIG'] = str(config)
    for key in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'SDKROOT'):
        env.pop(key, None)
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-resource-dir', str(resource), '-I' + str(HERE)]
    run_command(receipt, out, 'compiler-version', [compiler, '--version'], env)
    objects = []
    for source in (HERE / 'summary.c', HERE / 'tests' / 'native_caller.c'):
        obj = out / (source.stem + '.o')
        dep = out / (source.stem + '.d')
        run_command(receipt, out, source.stem, [compiler, *flags, '-MD', '-MF', dep,
            '-c', source, '-o', obj], env)
        objects.append(obj)
    binary = out / 'summary-probe.elf'
    run_command(receipt, out, 'link', [compiler, *flags, *objects, '-Wl,-z,now', '-o', binary], env)

    dependencies = set()
    for dep in out.glob('*.d'):
        text = dep.read_text().replace('\\\n', ' ')
        dependencies.update(Path(item).resolve() for item in shlex.split(text.split(':', 1)[1]))
    used_headers = []
    for path in sorted(dependencies):
        is_resource = path.is_relative_to(resource)
        actual = digest(path)
        if is_resource and members.get('lib/clang/21/' + path.relative_to(resource).as_posix()) != actual:
            raise RuntimeError('used compiler resource header differs from pinned manifest: ' + str(path))
        used_headers.append({'path': str(path), 'sha256': actual, 'selected_resource_pin_verified': is_resource})
    receipt['used_headers'] = used_headers
    info = run_command(receipt, out, 'elf-inspection',
        [llvm_bin / 'llvm-readobj', '--file-headers', '--dynamic-table', '--dyn-symbols', binary], env).decode(errors='replace')
    if not all(token in info for token in ('EM_X86_64', 'Type: SharedObject', 'OS/ABI: FreeBSD', 'BIND_NOW')):
        raise RuntimeError('expected pinned PS5 ELF contract missing')
    imports = []
    for block in info.split('Symbol {')[1:]:
        name = re.search(r'Name: (\S+)', block)
        if name and 'Section: Undefined' in block:
            imports.append(name.group(1))
    if not set(imports) <= {'memcpy', 'memset'}:
        raise RuntimeError('unexpected native import: ' + repr(sorted(set(imports))))
    undefined = run_command(receipt, out, 'summary-object-undefined',
        [llvm_bin / 'llvm-nm', '-u', objects[0]], env).decode(errors='replace')
    if re.search(r'\b(open|read|write|socket|malloc|calloc|realloc|free|system|execve|__udivti3)\b', undefined):
        raise RuntimeError('summary parser unexpectedly imports I/O/allocation/runtime helper')
    receipt['imports'] = sorted(set(imports))
    receipt['elf_contract'] = 'FreeBSD x86_64 ET_DYN/BIND_NOW; compile/link only, never executed'
    receipt['artifact'] = {'path': str(binary), 'bytes': binary.stat().st_size, 'sha256': digest(binary)}
    receipt['target_executed'] = False
    receipt['result'] = 'PASS'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--actual-profile', type=Path, default=HERE / 'tests' / 'actual_header_profile.json')
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--ps5-sdk', type=Path)
    parser.add_argument('--llvm-config', type=Path)
    parser.add_argument('--resource-dir', type=Path)
    args = parser.parse_args()
    native = args.ps5_sdk is not None
    if native != bool(args.llvm_config and args.resource_dir) or (not native and (args.llvm_config or args.resource_dir)):
        parser.error('PS5 SDK, llvm-config and resource-dir must be supplied together')
    if native and args.actual_profile != HERE / 'tests' / 'actual_header_profile.json':
        parser.error('native target is compile/link only; actual corpus is host-only')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = {'result': 'INCOMPLETE', 'target': 'ps5-compile-link-only' if native else 'host-captured-package-headers',
        'target_executed': False, 'platform_compatible': False, 'asset_decoded': False,
        'commands': [], 'inputs': []}
    save_receipt(receipt, out)
    try:
        schema = load_schema_inputs(HERE / 'schema-lock.json')
        receipt['schema_inputs'] = schema
        save_receipt(receipt, out)
        if native:
            native_run(args, receipt, out, schema)
        else:
            host_run(args, receipt, out, schema)
    except Exception as error:
        receipt['result'] = 'FAIL'
        receipt['failure'] = str(error)
        raise
    finally:
        save_receipt(receipt, out)
    print(json.dumps({key: receipt[key] for key in ('result', 'target', 'target_executed')}))


if __name__ == '__main__':
    main()
