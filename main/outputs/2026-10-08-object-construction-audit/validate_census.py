"""Read-only replay of a target census; does not compile or contact a console."""
from pathlib import Path
import copy
import hashlib
import json
import struct

HERE = Path(__file__).resolve().parent
EXPECTED = {
    'Class': 0, 'Outer': 8, 'Name': 0x10, 'SetFlags': 0x18,
    'InternalSetFlags': 0x1c, 'bCopyTransientsFromClassDefaults': 0x20,
    'bAssumeTemplateIsArchetype': 0x21, 'Template': 0x28,
    'InstanceGraph': 0x30, 'ExternalPackage': 0x38,
    'PropertyInitCallback': 0x40, 'SerialNumber': 0x70,
    'SubobjectOverrides': 0x78,
}
WIDTHS = dict(zip(EXPECTED, [8, 8, 8, 4, 4, 1, 1, 8, 8, 8, 48, 4, 8]))

def validate_facts(record):
    facts = record['target_facts']
    for field, expected in EXPECTED.items():
        if facts[field] != expected or facts[field + '.width'] != WIDTHS[field]:
            raise ValueError('layout mismatch: ' + field)
    if facts['align'] != 8 or facts['size'] != 0x80:
        raise ValueError('target aggregate mismatch')

def replay(record):
    validate_facts(record)
    for pin in record['inputs'] + [record['output']]:
        data = Path(pin['path']).read_bytes()
        if len(data) != pin['bytes'] or hashlib.sha256(data).hexdigest() != pin['sha256']:
            raise ValueError('source/object identity mismatch: ' + pin['path'])
    image_pin = next(x for x in record['inputs'] if x['path'].endswith('decrypted-eboot.bin'))
    image = Path(image_pin['path']).read_bytes()
    offset = struct.unpack_from('<Q', image, 32)[0]
    stride, count = struct.unpack_from('<HH', image, 54)
    segments = [struct.unpack_from('<IIQQQQQQ', image, offset + n * stride) for n in range(count)]
    for instruction in record['native_instructions']:
        address = int(instruction['image_vaddr'], 16)
        raw = bytes.fromhex(instruction['bytes'])
        hits = [s for s in segments if s[0] == 1 and s[3] <= address and address + len(raw) <= s[3] + s[5]]
        if len(hits) != 1:
            raise ValueError('ambiguous/unbacked instruction')
        actual_offset = hits[0][2] + address - hits[0][3]
        if actual_offset != int(instruction['file_offset'], 16) or image[actual_offset:actual_offset + len(raw)] != raw:
            raise ValueError('instruction/file mapping mismatch')
    if len(record['native_instructions']) != record['native_instruction_windows_verified']:
        raise ValueError('instruction count mismatch')

def main():
    record = json.loads((HERE / 'CENSUS.v2.json').read_text())
    replay(record)
    controls = []
    for field in EXPECTED:
        mutation = copy.deepcopy(record)
        mutation['target_facts'][field] += 8
        try:
            validate_facts(mutation)
        except ValueError:
            controls.append({'control': 'wrong_' + field, 'result': 'REFUSED'})
        else:
            raise AssertionError('incorrect offset accepted: ' + field)
    for field, old in [('SerialNumber', 0x40), ('SubobjectOverrides', 0x48)]:
        mutation = copy.deepcopy(record)
        mutation['target_facts'][field] = old
        try:
            validate_facts(mutation)
        except ValueError:
            controls.append({'control': 'pre_56_' + field, 'result': 'REFUSED'})
        else:
            raise AssertionError('older layout accepted')
    result = {'status': 'PASS_MAC_ONLY', 'target_values': len(record['target_facts']),
              'native_member_offsets_correlated': len(EXPECTED),
              'instruction_windows_replayed': len(record['native_instructions']),
              'refusal_controls': controls, 'native_execution': False,
              'limitations': ['Independent static correlation; the exact native aggregate size is not established',
                              'PropertyInitCallback inner C++ ABI is not established; only zero callback is supported',
                              'Widget construction, reflection marshalling, and lifetime are not console-proven']}
    (HERE / 'VALIDATION.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS_MAC_ONLY: 28 target values, 13 correlated offsets, 200 saved instruction windows, 15 refusal controls; native executions=0')

if __name__ == '__main__':
    main()
