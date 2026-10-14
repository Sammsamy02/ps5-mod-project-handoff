#!/usr/bin/env python3
"""Derive a compact v8 package subset for offline overlay composition.

Not a runtime mount tool. Original package/bulk payloads and nonheader metadata
are byte-identical. TOC/block/directory layout and subset store are derived.
"""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
import struct
import sys
import types


def project(api, mod, keep_packages):
    def require(ok, code):
        if not ok:
            raise ValueError(code)

    keep = set(keep_packages)
    require(keep, 'empty_package_selection')
    require(all(type(pid) is int and 0 <= pid < (1 << 64) for pid in keep),
            'invalid_package_id')

    def source_identity():
        return (api.sha256_file(mod.utoc_path), api.sha256_file(mod.ucas_path))

    expected = (mod.utoc_sha, mod.ucas_sha)
    require(source_identity() == expected, 'source_identity_changed')
    toc = mod.toc
    api.validate_lookup(toc)
    raw_header = api.read_mod_header_bytes(mod)
    source_header = api.parse_container_header(raw_header)
    require(source_header.container_id == int.from_bytes(toc.container_id, 'little'),
            'source_container_header_identity_mismatch')
    exports = {api.package_id_of_chunk(cid) for cid in toc.ids if cid[11] == 1}
    require(keep <= exports and keep <= set(source_header.packages),
            'selected_package_not_in_source')
    require(exports == set(source_header.packages), 'source_export_store_coverage_mismatch')
    require(toc.block_size > 0, 'invalid_block_size')
    # Nonpackage chunks cannot be silently dropped or arbitrarily allocated.
    for cid in toc.ids:
        if cid == mod.header_chunk_id:
            continue
        require(cid[11] in (1, 2, 3) and api.package_id_of_chunk(cid) in exports,
                'unsupported_nonpackage_chunk')

    header = api.ContainerHeader(source_header.container_id,
        {pid: source_header.packages[pid] for pid in sorted(keep)},
        source_header.soft_serial_region_absent, source_header.opaque_zero_suffix)
    header_bytes = api.serialize_container_header(header)
    ids = [mod.header_chunk_id] + sorted(cid for cid in toc.ids
        if cid != mod.header_chunk_id and api.package_id_of_chunk(cid) in keep)
    row_for = {cid: i for i, cid in enumerate(toc.ids)}
    offsets, blocks, metas, ucas = bytearray(), bytearray(), bytearray(), bytearray()
    ledger = []
    with open(mod.ucas_path, 'rb') as handle:
        with mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as store:
            for cid in ids:
                is_header = cid == mod.header_chunk_id
                original = raw_header if is_header else api.extract_chunk_bytes(toc, row_for[cid], store)
                require(api.blake3_20(original) == toc.meta(row_for[cid])[:20],
                        'source_payload_hash_mismatch')
                payload = header_bytes if is_header else original
                require(payload, 'empty_chunk_payload')
                logical = (len(blocks) // 12) * toc.block_size
                require(logical + len(payload) < (1 << 40), 'projected_logical_range_overflow')
                offsets.extend(logical.to_bytes(5, 'big') + len(payload).to_bytes(5, 'big'))
                for pos in range(0, len(payload), toc.block_size):
                    block = payload[pos:pos + toc.block_size]
                    require(len(block) < (1 << 24) and len(ucas) < (1 << 40),
                            'projected_block_range_overflow')
                    blocks.extend(len(ucas).to_bytes(5, 'little') +
                        len(block).to_bytes(3, 'little') * 2 + b'\0')
                    ucas.extend(block)
                    ucas.extend(bytes(api.align16(len(ucas)) - len(ucas)))
                    require(len(ucas) <= api.MAX_SIDECAR_BYTES, 'projection_over_size_cap')
                meta = api.blake3_20(payload) + bytes(4) if is_header else toc.meta(row_for[cid])
                metas.extend(meta)
                ledger.append({'chunk_id': cid.hex(), 'bytes': len(payload),
                    'source_payload_sha256': hashlib.sha256(original).hexdigest(),
                    'output_payload_sha256': hashlib.sha256(payload).hexdigest(),
                    'derived_header': is_header})

    directory_files = [row for row in mod.directory['files']
                       if api.package_id_of_chunk(row.chunk_id) in keep]
    directory, _ = api.emit_directory_index(mod.directory['mount_point'],
        mod.directory['root_name'], mod.directory['directories'], directory_files,
        {cid: i for i, cid in enumerate(ids)})
    out_header = bytearray(toc.header)
    count = len(ids)
    for off, value in ((24, count), (28, len(blocks)//12), (48, len(directory)),
                       (52, 1), (84, 1), (96, count)):
        struct.pack_into('<I', out_header, off, value)
    struct.pack_into('<Q', out_header, 88, (1 << 64)-1)
    out = (bytes(out_header) + b''.join(ids) + bytes(offsets) +
           struct.pack('<i', -(count+1)) + struct.pack('<%dI' % count, *range(count)) +
           bytes(blocks) + directory + bytes(metas))
    data = bytes(ucas)
    parsed = api.parse_toc(out)
    api.validate_lookup(parsed)
    api.validate_single_partition(parsed, len(data), permit_tail_padding=True)
    for row, cid in enumerate(ids):
        payload = api.extract_chunk_bytes(parsed, row, data)
        require(hashlib.sha256(payload).hexdigest() == ledger[row]['output_payload_sha256'],
                'projection_extraction_mismatch')
        if cid != mod.header_chunk_id:
            require(parsed.meta(row) == toc.meta(row_for[cid]), 'projected_payload_metadata_changed')
    reparsed = api.parse_container_header(api.extract_chunk_bytes(parsed, 0, data))
    require(set(reparsed.packages) == keep, 'projected_store_coverage_mismatch')
    require(source_identity() == expected, 'source_identity_changed')
    imports = {pid for entry in header.packages.values() for pid in entry.imported_packages}
    receipt = {'schema': 1, 'label': 'Mac-only', 'result': 'projection_verified',
        'runtime_mount_ready': False, 'source_mod_id': mod.mod_id,
        'source_utoc_sha256': expected[0], 'source_ucas_sha256': expected[1],
        'selected_package_ids': ['%016x' % p for p in sorted(keep)],
        'omitted_package_count': len(exports - keep),
        'imports_outside_projection': ['%016x' % p for p in sorted(imports - keep)],
        'source_ucas_bytes': mod.ucas_size, 'output_ucas_bytes': len(data),
        'output_utoc_sha256': hashlib.sha256(out).hexdigest(),
        'output_ucas_sha256': hashlib.sha256(data).hexdigest(), 'chunks': ledger,
        'verification': 'reparsed TOC/lookup/geometry/store; every selected payload hash/extraction and nonheader metadata; source hashes before/after',
        'limits': ['Package imports are preserved, not proven resolved across target containers.',
                   'This projection is a derived builder input, not an admitted standalone mount.',
                   'Runtime store precedence, complete base identity and mount seams remain unproved.']}
    return out, data, receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--utoc', type=Path, required=True)
    parser.add_argument('--ucas', type=Path, required=True)
    parser.add_argument('--keep-json', type=Path, required=True,
                        help='JSON array of 16-digit hexadecimal package IDs')
    parser.add_argument('--output-dir', type=Path, required=True,
                        help='new directory; existing output is refused')
    parser.add_argument('--overlay-module', type=Path, default=(
        Path(__file__).resolve().parents[3]/'outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py'))
    args = parser.parse_args(argv)
    try:
        if args.output_dir.exists() or args.output_dir.is_symlink():
            raise ValueError('output_directory_already_exists')
        ids = json.loads(args.keep_json.read_text())
        if not isinstance(ids, list) or not ids or any(not isinstance(v, str) or len(v)!=16 or
                any(c not in '0123456789abcdefABCDEF' for c in v) for v in ids):
            raise ValueError('invalid_package_selection_json')
        if len(set(v.lower() for v in ids)) != len(ids):
            raise ValueError('duplicate_package_selection')
        source = args.overlay_module.read_bytes()
        module = types.ModuleType('captured_projection_parser')
        module.__file__ = str(args.overlay_module.resolve())
        exec(compile(source, module.__file__, 'exec'), module.__dict__)
        mod = module.ModSpec('source', str(args.utoc), str(args.ucas))
        utoc, ucas, receipt = project(module, mod, {int(v, 16) for v in ids})
        receipt['parser_sha256'] = hashlib.sha256(source).hexdigest()
        args.output_dir.mkdir(parents=False, exist_ok=False)
        (args.output_dir/'projection.utoc').write_bytes(utoc)
        (args.output_dir/'projection.ucas').write_bytes(ucas)
        # Receipt is written last; partial output never has a success receipt.
        (args.output_dir/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    except (ValueError, OSError, AttributeError, KeyError) as error:
        print(json.dumps({'label':'Mac-only', 'result':'refused', 'runtime_mount_ready':False,
                          'detail':str(error)}, indent=2))
        return 2
    print(json.dumps({'label':'Mac-only','result':'projection_verified','runtime_mount_ready':False,
                      'output_dir':str(args.output_dir.resolve()),
                      'output_ucas_bytes':len(ucas),'selected_packages':len(ids)}, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
