#!/usr/bin/env python3
"""Read-only binary and package-receipt audit; writes only the requested receipt."""
import argparse
import base64
import hashlib
import json
import pathlib
import re
import struct


def capture(path):
    path = pathlib.Path(path)
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('package', type=pathlib.Path)
    parser.add_argument('runtime', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--expected-prx-sha256', required=True)
    args = parser.parse_args()
    package = args.package.resolve()
    prx = package / 'modbridge_supplier_composition.prx'
    bare = package / 'bare-sdk/modbridge_supplier_composition.prx'
    receipt_names = ['artifacts-receipt.json', 'package-profile.json',
                     'supplier-contract.json', 'build-receipt.json',
                     'R4-WHOLE-SELECTED.json',
                     'verification-evidence/static-checks.json',
                     'verification-evidence/lineage-verification.json',
                     'verification-evidence/bootstrap-only-imports.json']
    watched = [prx, bare, args.runtime.resolve()] + [package / n for n in receipt_names]
    before = {str(p): capture(p) for p in watched}
    reports = {n: json.loads((package / n).read_text()) for n in receipt_names}
    wrapped = prx.read_bytes()
    elf = bare.read_bytes()
    checks = {}

    def check(name, condition, detail=None):
        checks[name] = {'pass': bool(condition), 'detail': detail}

    check('notified_prx_hash', before[str(prx)]['sha256'] == args.expected_prx_sha256,
          before[str(prx)])
    check('sony_self_magic', wrapped[:4] == bytes.fromhex('4f153d1d'), wrapped[:4].hex())
    check('bare_elf64_little_x86_64', elf[:7] == b'\x7fELF\x02\x01\x01'
          and struct.unpack_from('<H', elf, 18)[0] == 62, elf[:16].hex())
    hdr = struct.unpack_from('<HHIQQQIHHHHHH', elf, 16)
    elf_type, machine, version, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstridx = hdr
    check('sce_dynamic_module_type', elf_type == 0xfe18 and flags == 0,
          {'type': hex(elf_type), 'flags': flags, 'machine': machine})
    check('elf_table_geometry', ehsize == 64 and phentsize == 56 and shentsize == 64,
          {'program_headers': phnum, 'sections': shnum})
    ph = [struct.unpack_from('<IIQQQQQQ', elf, phoff + i * phentsize) for i in range(phnum)]
    sh = [struct.unpack_from('<IIQQQQIIQQ', elf, shoff + i * shentsize) for i in range(shnum)]
    section_strings = elf[sh[shstridx][4]:sh[shstridx][4] + sh[shstridx][5]]

    def string_at(data, offset):
        assert 0 <= offset < len(data)
        return data[offset:data.index(0, offset)].decode('ascii')

    named = {string_at(section_strings, row[0]): row for row in sh}
    module_params = [row for row in ph if row[0] == 0x61000002]
    sdk = struct.unpack_from('<I', elf, module_params[0][2] + 20)[0] if len(module_params) == 1 else None
    check('single_module_param_sdk', len(module_params) == 1 and sdk == 0x11000000,
          {'count': len(module_params), 'sdk': hex(sdk) if sdk is not None else None})

    # Decode the SELF container using its recorded segment indices, not ELF offsets
    # applied to the outer header. make_fself preserves all selected ELF segments.
    key, header_size, meta_size, file_size, self_count, self_flags = struct.unpack_from('<I2HQ2H4x', wrapped, 8)
    elf_at = 32 + self_count * 32
    header_end = max(ehsize, phoff + phentsize * phnum)
    expected_header = bytearray(elf[:header_end])
    struct.pack_into('<H', expected_header, 60, 0)
    check('self_embedded_elf_header', wrapped[elf_at:elf_at + header_end] == expected_header,
          {'self_entries': self_count, 'embedded_elf_offset': elf_at})
    info_at = (elf_at + header_end + 15) & ~15
    check('self_full_bare_sha256', wrapped[info_at + 32:info_at + 64] == hashlib.sha256(elf).digest())
    data_segments = []
    selected_indices = [i for i, row in enumerate(ph) if row[0] in (1, 0x61000010, 0x61000000, 0x6fffff00)]
    for i in range(self_count):
        props, offset, stored, memory = struct.unpack_from('<4Q', wrapped, 32 + i * 32)
        # Data entries have block bit 11; digest entries have digest bit 16.
        if not props & (1 << 11):
            continue
        index = (props >> 20) & 0xffff
        row = ph[index]
        equal = (not props & ((1 << 1) | (1 << 3)) and stored == memory == row[5]
                 and wrapped[offset:offset + stored] == elf[row[2]:row[2] + row[5]])
        data_segments.append({'index': index, 'bytes': stored, 'pass': equal})
    check('self_all_selected_segments_equal', len(data_segments) == len(selected_indices)
          and sorted(x['index'] for x in data_segments) == selected_indices
          and all(x['pass'] for x in data_segments), data_segments)

    dyn = named['.dynamic']
    dynamic = [struct.unpack_from('<QQ', elf, dyn[4] + i) for i in range(0, dyn[5], 16)]
    tags = dict(dynamic)
    check('explicit_init_fini_and_no_loader_arrays', tags.get(12, 0) and tags.get(13, 0)
          and all(tags.get(x, 0) == 0 for x in (25, 26, 27, 28)),
          {'DT_INIT': tags.get(12), 'DT_FINI': tags.get(13),
           'array_tags': {str(x): tags.get(x, 0) for x in (25, 26, 27, 28)}})
    check('exception_sections_present', all(named.get(x, (0,) * 6)[5] > 0
          for x in ('.eh_frame', '.eh_frame_hdr', '.gcc_except_table')))

    sym = named['.dynsym']
    strings = sh[sym[6]]
    dynstr = elf[strings[4]:strings[4] + strings[5]]
    symbols = []
    for i in range(sym[5] // sym[9]):
        name, info, other, section, value, size = struct.unpack_from('<IBBHQQ', elf, sym[4] + i * sym[9])
        if name:
            symbols.append({'name': string_at(dynstr, name), 'binding': info >> 4,
                            'type': info & 15, 'section': section, 'value': value, 'size': size})
    imported = [s for s in symbols if s['section'] == 0]
    modules, libraries = {}, {}
    for tag, value in dynamic:
        if tag in (0x61000045, 0x61000049):
            item = {'name': string_at(dynstr, value & 0xffffffff),
                    'version': (value >> 32) & 0xffff}
            (modules if tag == 0x61000045 else libraries)[value >> 48] = item
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'

    def decode_id(text):
        result = 0
        for char in text:
            result = result * 64 + alphabet.index(char)
        return result

    suffix = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    contract = reports['supplier-contract.json']['contract']['imports']
    by_nid = {}
    for item in contract:
        identity = item['ImportSymbol']
        digest = hashlib.sha1(identity['PublishedName'].encode('ascii') + suffix).digest()
        nid = base64.b64encode(digest[:8][::-1]).decode()[:11].replace('/', '-')
        assert nid not in by_nid
        by_nid[nid] = item
    identity_rows = []
    for symbol in imported:
        match = re.fullmatch(r'(.{11})#([A-Za-z0-9+\-]+)#([A-Za-z0-9+\-]+)', symbol['name'])
        valid = match is not None and match[1] in by_nid
        item = by_nid.get(match[1]) if match else None
        if item is None:
            identity_rows.append({'encoded_name': symbol['name'], 'pass': False})
            continue
        identity = item['ImportSymbol']
        library = libraries.get(decode_id(match[2]))
        module = modules.get(decode_id(match[3]))
        valid = valid and library == {'name': identity['LibraryName'], 'version': identity['LibraryVersion']}
        valid = valid and module == {'name': identity['ModuleName'], 'version': identity['ModuleVersion']}
        valid = valid and symbol['binding'] == {'GLOBAL': 1, 'WEAK': 2}[item['binding']]
        valid = valid and symbol['type'] == {'NOTYPE': 0, 'OBJECT': 1, 'FUNC': 2}[item['symbol_type']]
        identity_rows.append({'name': identity['Name'], 'encoded_name': symbol['name'],
                              'module': module, 'library': library, 'pass': valid})
    check('all_206_import_contract_identities', len(imported) == len(contract) == 206
          and len({x.get('name') for x in identity_rows}) == 206
          and all(x['pass'] for x in identity_rows),
          {'count': len(imported), 'expected': len(contract), 'rows': identity_rows})

    producer_statuses = {}
    for name in ('verification-evidence/static-checks.json', 'verification-evidence/lineage-verification.json'):
        report = reports[name]
        rows = report['checks']
        failed = [k for k, row in rows.items() if not row.get('pass')]
        check('producer_rows_' + pathlib.Path(name).stem, not failed,
              {'count': len(rows), 'failed': failed})
        producer_statuses[name] = {'status': report['status'], 'failed_rows': failed}
    commands = reports['build-receipt.json']['commands']
    check('all_recorded_package_commands_successful', len(commands) == 27 and all(x['rc'] == 0 for x in commands),
          {'count': len(commands), 'nonzero': [i for i, x in enumerate(commands) if x['rc']]})
    runtime_sha = before[str(args.runtime.resolve())]['sha256']
    check('runtime_matches_package_profile', runtime_sha == reports['package-profile.json']['composed_runtime_sha256'],
          before[str(args.runtime.resolve())])
    artifact_checks = []
    for role, record in reports['artifacts-receipt.json']['artifacts'].items():
        if isinstance(record, dict) and all(k in record for k in ('path', 'bytes', 'sha256')):
            observed = capture(record['path'])
            artifact_checks.append({'role': role, 'pass': observed == record, 'observed': observed})
    check('all_artifact_receipt_files_match', bool(artifact_checks) and all(x['pass'] for x in artifact_checks), artifact_checks)
    after = {str(p): capture(p) for p in watched}
    check('watched_files_unchanged', before == after)
    passed = all(x['pass'] for x in checks.values())
    result = {'status': 'PASS1_ARTIFACT_AUDIT_PASS' if passed else 'PASS1_ARTIFACT_AUDIT_REFUSE',
              'pass': passed, 'scope': 'Frozen pass1 only; pair, fresh pass2 and final guards pending.',
              'native_executed': False, 'console_contact': False, 'deployment_ready': False,
              'checks': checks, 'producer_statuses': producer_statuses,
              'qualification': 'LINEAGE_GAPS top status is retained despite passing lineage rows; root must explicitly adjudicate in final additive receipt.',
              'before': before, 'after': after, 'audit_script': capture(pathlib.Path(__file__).resolve()),
              'binary_format_sources': [capture(package / 'harness/tools/SharpProspero.Prx/SceNid.cs'),
                  capture(package / 'harness/tools/SharpProspero.Link/DynamicWriter.cs')]}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'checks': len(checks),
                      'failures': [k for k, v in checks.items() if not v['pass']],
                      'receipt': capture(args.output)}))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
