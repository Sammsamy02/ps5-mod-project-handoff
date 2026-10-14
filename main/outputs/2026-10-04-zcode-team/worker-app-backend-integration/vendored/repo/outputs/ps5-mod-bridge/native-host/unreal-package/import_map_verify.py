#!/usr/bin/env python3
"""Focused synthetic and retained-header verification for import_map.c."""

from collections import Counter
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]

# Bounded retained header slices and logical lengths from the independent
# Townfall PC/PS5 header comparison receipts. These are fixtures, not dispatch.


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def run(argv, stdout_path, stderr_path):
    proc = subprocess.run([str(x) for x in argv], capture_output=True, check=False)
    stdout_path.write_bytes(proc.stdout)
    stderr_path.write_bytes(proc.stderr)
    if proc.returncode:
        raise RuntimeError(f'command failed ({proc.returncode}): {argv!r}')
    return proc.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cc', default='/usr/bin/clang')
    parser.add_argument('--actual-profile', type=Path, default=HERE / 'tests' / 'import_map_actual_header_profile.json')
    args = parser.parse_args()
    profile_path = args.actual_profile.resolve(strict=True)
    fixture = json.loads(profile_path.read_text())
    if fixture.get('schema_version') != 1 or not isinstance(fixture.get('samples'), list) or not 1 <= len(fixture['samples']) <= 16:
        raise ValueError('invalid or excessive actual fixture profile')
    samples = fixture['samples']
    ids = [row['id'] for row in samples]
    if len(set(ids)) != len(ids):
        raise ValueError('duplicate actual sample id')
    equal_pair = fixture['equal_sequence_pair']
    multiset_pair = fixture['multiset_pair']
    if any(not isinstance(pair, list) or len(pair) != 2 or any(key not in ids for key in pair) for pair in (equal_pair, multiset_pair)):
        raise ValueError('invalid comparison pair')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = {
        'status': 'INCOMPLETE',
        'scope': 'focused pure import-map reader host tests and four retained bounded package headers',
        'target_executed': False,
        'payload_bytes_read': 0,
        'platform_compatible': False,
        'runtime_or_install_supported': False,
        'commands': [],
        'inputs': [],
    }
    receipt_path = out / 'receipt.json'

    def save():
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')

    save()
    sources = [profile_path, HERE / 'summary.c', HERE / 'summary.h', HERE / 'import_map.c',
        HERE / 'import_map.h', HERE / 'tests' / 'import_map_test.c']
    receipt['inputs'].extend({
        'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha256(path.read_bytes())
    } for path in sources)
    sample_bytes = {}
    for row in samples:
        if not 52 <= row['bytes'] <= 256 * 1024:
            raise ValueError('sample exceeds shared header ceiling')
        path = (ROOT / row['path']).resolve(strict=True)
        if not path.is_relative_to(ROOT) or not path.is_file():
            raise ValueError('sample escaped root or is not a file')
        data = path.read_bytes()
        if len(data) != row['bytes'] or sha256(data) != row['sha256']:
            raise ValueError('retained sample identity mismatch: ' + row['id'])
        sample_bytes[row['id']] = data
        receipt['inputs'].append({'sample_id': row['id'], 'path': str(path),
            'bytes': len(data), 'sha256': sha256(data), 'logical_package_bytes': row['logical_bytes']})
    save()

    binary = out / 'import-map-test'
    argv = [args.cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-I' + str(HERE), HERE / 'summary.c', HERE / 'import_map.c',
        HERE / 'tests' / 'import_map_test.c', '-o', binary]
    run(argv, out / 'compile.stdout', out / 'compile.stderr')
    receipt['commands'].append({'argv': [str(x) for x in argv], 'exit': 0,
        'stdout': 'compile.stdout', 'stderr': 'compile.stderr'})
    test_output = run([binary], out / 'synthetic.stdout', out / 'synthetic.stderr').decode('ascii').strip()
    receipt['commands'].append({'argv': [str(binary)], 'exit': 0,
        'stdout': 'synthetic.stdout', 'stderr': 'synthetic.stderr'})
    if test_output != 'zen-import-map checks=32 failures=0':
        raise RuntimeError('unexpected focused test summary: ' + test_output)
    receipt['synthetic_checks'] = 32

    observed = {}
    for sample in samples:
        path = ROOT / sample['path']
        stdout_name = sample['id'] + '.stdout'
        stderr_name = sample['id'] + '.stderr'
        output = run([binary, '--actual', path, str(sample['logical_bytes'])],
            out / stdout_name, out / stderr_name).decode('ascii').strip()
        receipt['commands'].append({'argv': [str(binary), '--actual', str(path),
            str(sample['logical_bytes'])], 'exit': 0, 'stdout': stdout_name, 'stderr': stderr_name})
        data = json.loads(output)
        for field, expected in (
            ('header_bytes', sample['header_bytes']), ('import_map_offset', sample['map_offset']),
            ('import_map_bytes', sample['map_bytes']), ('reference_count', sample['map_bytes'] // 8),
            ('public_hash_count', sample['public_hash_count']), ('kind_counts', sample['kind_counts']),
        ):
            if data.get(field) != expected:
                raise RuntimeError(f'{sample["id"]}: {field} mismatch {data.get(field)!r} != {expected!r}')
        raw_values = data['raw_refs']
        if len(raw_values) != data['reference_count']:
            raise RuntimeError(sample['id'] + ': output reference count mismatch')
        computed = [0, 0, 0, 0]
        for raw_text in raw_values:
            raw = int(raw_text, 16)
            computed[raw >> 62] += 1
            if raw >> 62 == 2 and (raw & 0xFFFFFFFF) >= data['public_hash_count']:
                raise RuntimeError(sample['id'] + ': package import hash index outside hash table')
        if computed != sample['kind_counts']:
            raise RuntimeError(sample['id'] + ': raw kind counts disagree with reader result')
        observed[sample['id']] = data

    if observed[equal_pair[0]]['raw_refs'] != observed[equal_pair[1]]['raw_refs']:
        raise RuntimeError('retained material imports unexpectedly differ')
    ps5_mesh = Counter(observed[multiset_pair[1]]['raw_refs'])
    pc_mesh = Counter(observed[multiset_pair[0]]['raw_refs'])
    common = ps5_mesh & pc_mesh
    receipt['actual_observations'] = {
        key: {k: value[k] for k in ('header_bytes', 'import_map_offset', 'import_map_bytes',
            'reference_count', 'public_hash_count', 'kind_counts')}
        | {'raw_sequence_sha256': sha256('\n'.join(value['raw_refs']).encode('ascii'))}
        for key, value in observed.items()
    }
    receipt['comparison'] = {
        'material_raw_import_sequences_equal': True,
        'mesh_raw_records_pc': sum(pc_mesh.values()),
        'mesh_raw_records_ps5': sum(ps5_mesh.values()),
        'mesh_multiset_common_records': sum(common.values()),
        'mesh_pc_only_records': sum((pc_mesh - ps5_mesh).values()),
        'mesh_ps5_only_records': sum((ps5_mesh - pc_mesh).values()),
    }
    receipt['interpretation_limits'] = [
        'raw import-map references and package-import hash-index bounds only',
        'no global object resolution, dependency closure, asset decode, platform compatibility, or runtime/install claim',
        'same declared engine-layout hypothesis as the retained header comparison; not a trusted game/version inference',
    ]
    receipt['status'] = 'PASS'
    save()
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
