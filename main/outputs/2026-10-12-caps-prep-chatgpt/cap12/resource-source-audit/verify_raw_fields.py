#!/usr/bin/env python3
"""Check retained object-reference rows against original frozen metadata bytes.

This checks byte locations and values, not engine field semantics or code identity.
No UCAS, shader-library payload, full legacy object decoder or compiler is used.
"""
import sys
sys.dont_write_bytecode = True
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
LEGACY = ROOT / 'outputs/2026-10-08-l25-prep/round6-prep-20261012/cap12-resourceindex'
CACHE = ROOT / '.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Engine/GlobalShaderCache-SP_PS5.bin'
EXPECTED = {
    CACHE: 'ed52f157bd00b4ee810344ddcac29e9b90d1a154728095903254beb94c1db809',
    LEGACY / 'gsc_codec_m1a_copy.py': '9d1e9de4213bfccc34600c83a0c1db67de60df95f821f862e4c1c7f544656917',
    LEGACY / 'shader-objects-summary.json': 'de35d0ad5189f80ba984dbe8cc7a464dfeb67d5bee278045325e881441fa1127',
}


def need(ok, why):
    if not ok:
        raise ValueError(why)


def read_bound(path, digest):
    need(0 < path.stat().st_size <= 8 * 1024 * 1024, 'input size bound')
    data = path.read_bytes()
    need(hashlib.sha256(data).hexdigest() == digest, 'input hash mismatch: ' + str(path))
    return data


def main():
    raw = {p: read_bound(p, h) for p, h in EXPECTED.items()}
    parser_path = LEGACY / 'gsc_codec_m1a_copy.py'
    spec = importlib.util.spec_from_file_location('raw_field_m1a_reader', parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    data = raw[CACHE]
    model = parser.decode(data)
    summary = json.loads(raw[LEGACY / 'shader-objects-summary.json'])
    need(summary['input_sha256'] == EXPECTED[CACHE], 'summary input identity')
    cols = summary['columns']
    rows = summary['rows']
    need(rows and model['sections'], 'zero items checked')
    expected_rows = {}
    for row in rows:
        row = dict(zip(cols, row))
        key = (row['section'], row['k'])
        need(key not in expected_rows, 'duplicate summary reference')
        expected_rows[key] = row
    checked = 0
    distinct_objects = set()
    slots_by_section = {}
    for section in model['sections']:
        f = section['frozen']
        need(f is not None, 'unstructured section')
        ad = f['array_data']
        image = data[section['frozen_offset']:section['frozen_offset'] + section['frozen_size']]
        slots = {}
        for k, ptr in enumerate(ad['shaders']['items']):
            need(ptr is not None, 'null object reference')
            at = ad['shaders']['data_at'] + k * 8 + ptr['off']
            need(f['objects_at'] <= at and at + 220 <= len(image), 'object base bounds')
            row = expected_rows.pop((section['index'], k))
            need(at == row['object_at'], 'object reference mismatch')
            need(ad['shader_types']['items'][k] == row['type_hash'], 'type identity mismatch')
            need(ad['shader_permutations']['items'][k] == row['permutation'], 'permutation mismatch')
            target, index, key = struct.unpack_from('<IiI', image, at + 208)
            need((target, index, f'{key:08x}') ==
                 (row['target'], row['resource_index'], row['sort_key']), 'raw scalar mismatch')
            need(index >= 0, 'negative map-local slot')
            previous = slots.setdefault(index, (target, key))
            need(previous == (target, key), 'same-slot target/key conflict')
            distinct_objects.add((section['index'], at))
            checked += 1
        need(slots and sorted(slots) == list(range(len(slots))), 'non-dense map-local slots')
        slots_by_section[section['index']] = slots
    need(not expected_rows and checked == len(rows), 'reference coverage mismatch')
    for path, digest in EXPECTED.items():
        read_bound(path, digest)
    result = {
        'label': 'Fresh Mac-only raw frozen-metadata field replay',
        'shader_references_checked': checked,
        'distinct_object_locations_checked': len(distinct_objects),
        'sections_checked': len(slots_by_section),
        'dense_map_local_slots': sum(map(len, slots_by_section.values())),
        'source_files_checked': len(EXPECTED),
        'sources_unchanged': True,
        'sources': [{'path': str(p), 'sha256': h, 'bytes': len(raw[p])} for p, h in EXPECTED.items()],
        'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'field_offsets_checked': {'target_label': 208, 'map_local_index_label': 212, 'sort_key_label': 216},
        'engine_field_semantics_proven': False,
        'library_numbering_proven': False,
        'code_identity_proven': False,
        'limits': ['M1a reader supplies section/reference geometry; direct struct reads check retained rows.',
                   'Byte correspondence and dense map-local indexing do not establish engine field naming, a generic ABI, full hashes or shader code.',
                   'This does not rerun the full legacy object decoder, roundtrip or mutation controls.'],
    }
    (HERE / 'raw-field-replay.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('shader_references_checked', 'distinct_object_locations_checked',
                                           'sections_checked', 'dense_map_local_slots', 'sources_unchanged')}))


if __name__ == '__main__':
    main()
