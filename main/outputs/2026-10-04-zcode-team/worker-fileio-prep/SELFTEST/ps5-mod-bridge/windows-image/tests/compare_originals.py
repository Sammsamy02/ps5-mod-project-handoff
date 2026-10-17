#!/usr/bin/env python3
"""Compare prepared original DLL data against LLVM's independent PE records.

Only ordinary ctypes buffers are used. No input DLL is passed to CDLL, mapped
executable, or called. The only loaded library is our locally compiled C parser.
"""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile


def read_originals(archive, reader, output):
    """Extract two named entries, never paths chosen by archive contents."""
    archive, reader = archive.resolve(), reader.resolve()
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    records = []
    with zipfile.ZipFile(archive) as z:
        for member, name in [('dwmapi.dll', 'dwmapi.dll'), ('ue4ss/UE4SS.dll', 'UE4SS.dll')]:
            matches = [x for x in z.infolist() if x.filename == member]
            if len(matches) != 1 or not 0 < matches[0].file_size <= 128 * 1024 * 1024:
                raise ValueError(f'Expected exactly one bounded original member: {member}')
            data = z.read(matches[0])
            path = output / name
            path.write_bytes(data)
            command = [str(reader), '--file-headers', '--sections', '--coff-imports',
                       '--coff-exports', '--coff-basereloc', str(path)]
            result = subprocess.run(command, capture_output=True, timeout=60, check=True)
            readout = output / (name + '.llvm.txt')
            readout.write_bytes(result.stdout)
            (output / (name + '.llvm.stderr')).write_bytes(result.stderr)
            records.append({'archive_member': member, 'path': str(path), 'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(), 'reference_reader': str(reader),
                'reader_command': command, 'reader_exit': result.returncode,
                'reader_output': str(readout)})
    inputs = {'source_archive': str(archive),
              'source_archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
              'reference_reader_sha256': hashlib.sha256(reader.read_bytes()).hexdigest(),
              'files': records, 'executed_input_code': False, 'console_contact': False}
    (output / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    return inputs


def number(text, key):
    return int(re.search(r'^\s*' + key + r': (0x[0-9A-Fa-f]+|[0-9]+)', text, re.M)[1], 0)


def reference(record):
    text = Path(record['reader_output']).read_text()
    sections = []
    for body in re.findall(r'^  Section \{\n(.*?)^  \}', text, re.M | re.S):
        sections.append({key: number(body, key) for key in
                         ('VirtualAddress', 'VirtualSize', 'RawDataSize', 'PointerToRawData')})
    imports = []
    for body in re.findall(r'^Import \{\n(.*?)^\}', text, re.M | re.S):
        library = re.search(r'^  Name: (.*)$', body, re.M)[1]
        iat = number(body, 'ImportAddressTableRVA')
        for index, (name, hint) in enumerate(re.findall(r'^  Symbol: (.*) \((\d+)\)$', body, re.M)):
            imports.append({'library': library, 'name': name, 'by_ordinal': not bool(name),
                            'ordinal': int(hint) if not name else 0,
                            'hint': int(hint) if name else 0, 'iat_rva': iat + 8 * index})
    exports = []
    for body in re.findall(r'^Export \{\n(.*?)^\}', text, re.M | re.S):
        exports.append({'ordinal': number(body, 'Ordinal'), 'rva': number(body, 'RVA'),
                        'name': re.search(r'^  Name: (.*)$', body, re.M)[1]})
    relocations = [{'type': kind, 'rva': int(address, 16)} for kind, address in re.findall(
        r'  Entry \{\n    Type: (\w+)\n    Address: (0x[0-9A-Fa-f]+)\n  \}', text)]
    facts = {key: number(text, key) for key in
             ('ImageBase', 'SizeOfImage', 'SizeOfHeaders', 'AddressOfEntryPoint',
              'SectionCount', 'TLSTableRVA', 'ExceptionTableRVA', 'LoadConfigTableRVA',
              'DelayImportDescriptorRVA', 'CLRRuntimeHeaderRVA', 'ExportTableRVA', 'ExportTableSize')}
    return {'facts': facts, 'sections': sections, 'imports': imports,
            'exports': exports, 'relocations': relocations}


def image_from_llvm(data, ref, base):
    facts = ref['facts']
    image = bytearray(facts['SizeOfImage'])
    image[:facts['SizeOfHeaders']] = data[:facts['SizeOfHeaders']]
    for sec in ref['sections']:
        start, size, offset = sec['VirtualAddress'], sec['RawDataSize'], sec['PointerToRawData']
        image[start:start + size] = data[offset:offset + size]
    for rel in ref['relocations']:
        if rel['type'] == 'ABSOLUTE':
            continue
        assert rel['type'] == 'DIR64', rel
        old = struct.unpack_from('<Q', image, rel['rva'])[0]
        struct.pack_into('<Q', image, rel['rva'], (old + base - facts['ImageBase']) & ((1 << 64) - 1))
    return image


class String(c.Structure):
    _fields_ = [('data', c.c_void_p), ('size', c.c_size_t)]
    def value(self):
        return c.string_at(self.data, self.size).decode('ascii') if self.size else ''


class Directory(c.Structure):
    _fields_ = [('rva', c.c_uint32), ('size', c.c_uint32)]


class Section(c.Structure):
    _fields_ = [(key, c.c_uint32) for key in
                ('rva', 'virtual_size', 'raw_offset', 'raw_size', 'mapped_size', 'characteristics')]


class Info(c.Structure):
    _fields_ = [('preferred_base', c.c_uint64)] + [(key, c.c_uint32) for key in
        ('image_size', 'headers_size', 'entry_rva', 'section_alignment', 'file_alignment',
         'requirements', 'import_count', 'relocation_count', 'export_count', 'export_name_count')] + [
        ('section_count', c.c_uint16), ('characteristics', c.c_uint16), ('dll_characteristics', c.c_uint16),
        ('directories', Directory * 16), ('sections', Section * 96)]


class Import(c.Structure):
    _fields_ = [('library', String), ('name', String), ('iat_rva', c.c_uint32),
                ('ordinal', c.c_uint16), ('hint', c.c_uint16), ('by_ordinal', c.c_uint8)]
    def value(self):
        return {'library': self.library.value(), 'name': self.name.value(), 'iat_rva': self.iat_rva,
                'ordinal': self.ordinal, 'hint': self.hint, 'by_ordinal': bool(self.by_ordinal)}


class Error(c.Structure):
    _fields_ = [('code', c.c_int), ('rva', c.c_uint32), ('detail', c.c_uint32), ('binding', Import)]


class Report(c.Structure):
    _fields_ = [('logical_base', c.c_uint64), ('relocations_applied', c.c_uint32),
                ('imports_bound', c.c_uint32), ('imports_unresolved', c.c_uint32), ('image_prepared', c.c_uint8)]


class Export(c.Structure):
    _fields_ = [('ordinal', c.c_uint32), ('rva', c.c_uint32), ('name', String), ('forwarder', String)]


BINDER = c.CFUNCTYPE(c.c_int, c.c_void_p, c.POINTER(Import), c.POINTER(c.c_uint64))
VISITOR = c.CFUNCTYPE(c.c_int, c.c_void_p, c.POINTER(Export))


def compare(library, record, ref):
    data = Path(record['path']).read_bytes()
    assert hashlib.sha256(data).hexdigest() == record['sha256']
    source = c.create_string_buffer(data)
    info, error = Info(), Error()
    rc = library.mb_pe_inspect(source, len(data), c.byref(info), c.byref(error))
    assert rc == 0, (record['path'], rc, error.rva, error.detail)
    facts = ref['facts']
    for key, expected in [('image_size', facts['SizeOfImage']), ('preferred_base', facts['ImageBase']),
                          ('entry_rva', facts['AddressOfEntryPoint']), ('section_count', facts['SectionCount']),
                          ('import_count', len(ref['imports'])), ('export_count', len(ref['exports']))]:
        assert getattr(info, key) == expected, (key, getattr(info, key), expected)
    expected_requirements = sum(bit for key, bit in [('AddressOfEntryPoint', 1), ('TLSTableRVA', 2),
        ('ExceptionTableRVA', 4), ('DelayImportDescriptorRVA', 8), ('LoadConfigTableRVA', 16),
        ('CLRRuntimeHeaderRVA', 32)] if facts[key]) | (64 if ref['imports'] else 0)
    assert info.requirements == expected_requirements, (info.requirements, expected_requirements)
    outcomes = []
    for shift, bound in [(0, False), (0x200000, False), (-0x200000, False), (0x200000, True)]:
        base = facts['ImageBase'] + shift
        target = c.create_string_buffer(info.image_size + 1)
        target[info.image_size] = b'\xa5'
        expected = image_from_llvm(data, ref, base)
        report, observed = Report(), []
        @BINDER
        def binder(context, item, value):
            observed.append(item.contents.value())
            if not bound:
                return 0
            # Non-callable deterministic numbers verify IAT byte writes only.
            value[0] = 0x123400000000 + len(observed) * 16
            return 1
        rc = library.mb_pe_prepare(source, len(data), base, target, info.image_size,
            binder, None, c.byref(info), c.byref(report), c.byref(error))
        assert rc == (0 if bound or not ref['imports'] else 1), (rc, error.rva, error.detail)
        assert observed == ref['imports'], 'Import identities/order differ from LLVM'
        if bound:
            for index, entry in enumerate(ref['imports'], 1):
                struct.pack_into('<Q', expected, entry['iat_rva'], 0x123400000000 + index * 16)
        actual = bytes(target)[:info.image_size]
        assert target[info.image_size] == b'\xa5', 'Image capacity guard changed'
        assert actual == expected, 'Prepared image differs from independent LLVM reconstruction'
        assert report.image_prepared and report.logical_base == base
        assert report.imports_bound == (len(observed) if bound else 0)
        assert report.imports_unresolved == (0 if bound else len(observed))
        assert report.relocations_applied == (sum(r['type'] == 'DIR64' for r in ref['relocations'])
                                               if shift else 0)
        assert bytes(source)[:-1] == data, 'Original DLL was changed'
        outcomes.append({'base': base, 'binding': 'opaque_data_values' if bound else 'all_unresolved',
                         'status': rc, 'image_sha256': hashlib.sha256(actual).hexdigest(),
                         'imports': len(observed), 'relocations_applied': report.relocations_applied})
    exports = []
    @VISITOR
    def visit(context, item):
        e = item.contents
        exports.append({'ordinal': e.ordinal, 'rva': e.rva, 'name': e.name.value(),
                        'forwarder': e.forwarder.value()})
        return 0
    assert library.mb_pe_visit_exports(source, len(data), visit, None, c.byref(error)) == 0
    original = image_from_llvm(data, ref, facts['ImageBase'])
    expected_exports = []
    for export in ref['exports']:
        if not export['rva']:
            continue
        e = dict(export, forwarder='')
        if facts['ExportTableRVA'] <= e['rva'] < facts['ExportTableRVA'] + facts['ExportTableSize']:
            e['forwarder'] = original[e['rva']:original.index(0, e['rva'])].decode('ascii')
        expected_exports.append(e)
    assert sorted(exports, key=lambda e: (e['ordinal'], e['name'])) == sorted(
        expected_exports, key=lambda e: (e['ordinal'], e['name'])), 'Exports differ from LLVM'
    return {'original': record, 'image_size': facts['SizeOfImage'], 'requirements': expected_requirements,
            'import_libraries': sorted({x['library'] for x in ref['imports']}),
            'nonzero_exports': len(exports), 'trials': outcomes, 'dll_code_executed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True, help='Retained original UE4SS ZIP')
    parser.add_argument('--reader', type=Path, required=True, help='llvm-readobj executable')
    parser.add_argument('--output', type=Path, required=True, help='New local evidence directory')
    parser.add_argument('--library', type=Path, required=True,
                        help='Our locally built parser library; NEVER a downloaded input DLL')
    args = parser.parse_args()
    inputs = read_originals(args.archive, args.reader, args.output)
    references = [reference(record) for record in inputs['files']]
    (args.output / 'llvm-reference.json').write_text(json.dumps(references, indent=2) + '\n')
    lib = c.CDLL(str(args.library.resolve()))
    lib.mb_pe_inspect.argtypes = [c.c_void_p, c.c_size_t, c.POINTER(Info), c.POINTER(Error)]
    lib.mb_pe_prepare.argtypes = [c.c_void_p, c.c_size_t, c.c_uint64, c.c_void_p, c.c_size_t,
                                 BINDER, c.c_void_p, c.POINTER(Info), c.POINTER(Report), c.POINTER(Error)]
    lib.mb_pe_visit_exports.argtypes = [c.c_void_p, c.c_size_t, VISITOR, c.c_void_p, c.POINTER(Error)]
    results = [compare(lib, record, ref) for record, ref in zip(inputs['files'], references)]
    result = {'passed': True, 'production_library': str(args.library.resolve()),
              'production_library_sha256': hashlib.sha256(args.library.read_bytes()).hexdigest(),
              'results': results, 'console_contact': False, 'target_execution': False,
              'scope': 'Original DLL bytes prepared/rebased/import-bound only; opaque data bindings are not Windows services'}
    (args.output / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'passed': True, 'original_dlls': len(results), 'image_trials': sum(len(r['trials']) for r in results)}))


if __name__ == '__main__':
    main()
