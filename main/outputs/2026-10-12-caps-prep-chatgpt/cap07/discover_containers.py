#!/usr/bin/env python3
"""Read-only candidate selection over captured TOCs, never a runtime mount gate.

Uses the existing overlay builder's admitted v8 parser. Package ownership is
inferred ONLY within supplied files from ExportBundleData chunk IDs (type 1).
No UCAS or game executable is read, changed, built, or contacted remotely.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import types

MAX_CAPTURE_BYTES = 128 * 1024 * 1024


def discover(base_paths, mod_paths, parse_toc):
    report = {
        'schema': 1, 'label': 'Mac-only', 'result': 'refused',
        'runtime_mount_ready': False, 'catalogue_scope': 'provided-files-only',
        'catalogue_completeness_verified': False, 'bases': [], 'mods': [], 'targets': [],
    }

    def refuse(code, **detail):
        report.update(code=code, **detail)
        return report

    def normalized(paths):
        return sorted(Path(p).resolve() for p in paths)

    bases, mods = normalized(base_paths), normalized(mod_paths)
    if not bases:
        return refuse('empty_base_catalogue')
    if not mods:
        return refuse('empty_mod_catalogue')
    if len(set(bases)) != len(bases) or len(set(mods)) != len(mods):
        return refuse('duplicate_input_path')
    if set(bases) & set(mods):
        return refuse('base_mod_path_overlap')

    def read(path):
        if path.suffix != '.utoc' or not path.is_file():
            raise ValueError('expected a local .utoc file')
        if path.stat().st_size > MAX_CAPTURE_BYTES:
            raise ValueError('capture exceeds 128 MiB cap')
        raw = path.read_bytes()
        if len(raw) > MAX_CAPTURE_BYTES:
            raise ValueError('capture grew beyond cap')
        toc = parse_toc(raw)
        if toc.partition_count != 1 or toc.partition_size != (1 << 64) - 1:
            raise ValueError('candidate sidecar planning requires unbounded single-partition input')
        packages = {int.from_bytes(cid[:8], 'little') for cid in toc.ids if cid[11] == 1}
        record = {'path': str(path), 'sha256': hashlib.sha256(raw).hexdigest(),
                  'bytes': len(raw), 'container_id_hex': toc.container_id.hex(),
                  'export_package_count': len(packages)}
        return record, packages

    owners, base_records, base_ids = {}, {}, set()
    for path in bases:
        try:
            record, packages = read(path)
        except (ValueError, OSError) as error:
            return refuse('toc_parse_refused', input_path=str(path), detail=str(error))
        if record['container_id_hex'] in base_ids:
            return refuse('duplicate_base_container_identity', input_path=str(path))
        base_ids.add(record['container_id_hex'])
        report['bases'].append(record)
        base_records[str(path)] = record
        for pid in packages:
            owners.setdefault(pid, []).append(str(path))

    target_packages, target_mods = {}, {}
    unmatched_mods, ambiguous = [], []
    for path in mods:
        try:
            record, packages = read(path)
        except (ValueError, OSError) as error:
            return refuse('toc_parse_refused', input_path=str(path), detail=str(error))
        if not packages:
            return refuse('mod_has_no_export_packages', input_path=str(path))
        matched, unmatched = 0, 0
        for pid in sorted(packages):
            locations = owners.get(pid, [])
            if len(locations) > 1:
                ambiguous.append({'package_id': '%016x' % pid, 'owners': locations})
            elif locations:
                matched += 1
                destination = locations[0]
                target_packages.setdefault(destination, set()).add(pid)
                target_mods.setdefault(destination, set()).add(str(path))
            else:
                unmatched += 1
        record.update(matched_package_count=matched, unmatched_package_count=unmatched)
        report['mods'].append(record)
        if not matched:
            unmatched_mods.append(str(path))

    for path in sorted(target_packages):
        stem = Path(path).stem
        report['targets'].append({
            'captured_index_path': path, 'index_name': Path(path).name,
            'base_ucas_name': stem + '.ucas', 'sidecar_name': stem + '_s1.ucas',
            'base_index_sha256': base_records[path]['sha256'],
            'matched_package_count': len(target_packages[path]),
            'mod_paths': sorted(target_mods[path]),
        })
    if ambiguous:
        return refuse('ambiguous_package_owner', ambiguous_packages=ambiguous)
    if unmatched_mods:
        return refuse('no_base_package_match', unmatched_mods=unmatched_mods)
    if len(report['targets']) != 1:
        return refuse('multi_container_overlay_required')
    report['result'] = 'candidate_selected'
    report['limits'] = [
        'Unmatched IDs are unknown within this capture, not proven new packages.',
        'Capture completeness, live normalized paths and UCAS identities remain unverified.',
        'No overlay composed; imports, conflicts, payloads and shader compatibility remain unverified.',
        'Runtime timing, pins, read seams and handle ABIs require separate admission.',
    ]
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-dir', type=Path, required=True)
    parser.add_argument('--mod-utoc', type=Path, action='append', required=True)
    parser.add_argument('--overlay-module', type=Path, default=(
        Path(__file__).resolve().parents[3] /
        'outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py'))
    args = parser.parse_args(argv)
    try:
        # Compile exactly the hashed source snapshot; no pycache or module-file writes.
        source = args.overlay_module.read_bytes()
        module = types.ModuleType('captured_overlay_parser')
        module.__file__ = str(args.overlay_module.resolve())
        exec(compile(source, module.__file__, 'exec'), module.__dict__)
        result = discover(args.base_dir.glob('*.utoc'), args.mod_utoc, module.parse_toc)
        result['parser_path'] = module.__file__
        result['parser_sha256'] = hashlib.sha256(source).hexdigest()
    except (ValueError, OSError, AttributeError) as error:
        result = {'label': 'Mac-only', 'result': 'refused', 'runtime_mount_ready': False,
                  'code': 'parser_or_input_unavailable', 'detail': str(error)}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result['result'] == 'candidate_selected' else 2


if __name__ == '__main__':
    sys.exit(main())
