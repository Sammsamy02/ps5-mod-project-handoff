import capstone
import hashlib
import json
import struct
from pathlib import Path

sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
base = Path('outputs/2026-10-07-l19-unresolved-evidence')
expected = {'FIELDC': '76aa9df32429b26a378649719ce2bafffae21e100ac2dfa7e20b0475eb1e693c',
            'UCLASS': '10495e5740008dc389729934e6e1a36f00108f771fff52d58fb9c3754d3372f1'}
rows = []
data = None
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for name, digest in expected.items():
    p = base / (name + '-FACTS.json')
    assert sha(p) == digest
    d = json.loads(p.read_text())
    if data is None:
        data = Path(d['image']['path']).read_bytes()
        h = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
        ph = [struct.unpack_from('<IIQQQQQQ', data, h[5] + i * h[9]) for i in range(h[10])]
        loads = [r for r in ph if r[0] == 1]
    assert len(data) == d['image']['bytes'] and hashlib.sha256(data).hexdigest() == d['image']['sha256']
    def fmap(v, n):
        candidates = [r for r in loads if r[3] <= v and v + n <= r[3] + r[5]]
        assert len(candidates) == 1
        return candidates[0][2] + v - candidates[0][3]
    for key, r in d['evidence'].items():
        v = int(r['elf_image_vaddr'], 16)
        b = bytes.fromhex(r['instruction_bytes'])
        o = fmap(v, len(b))
        assert o == int(r['file_byte_offset'], 16) and data[o:o + len(b)] == b
        ins = list(md.disasm(b, v))
        assert len(ins) == 1 and ins[0].size == len(b)
        text = (ins[0].mnemonic + ' ' + ins[0].op_str).strip()
        declared = r['instruction'].split(';')[0].strip()
        if text != declared:
            tokens = declared.split()
            assert (ins[0].mnemonic == 'call' or ins[0].mnemonic.startswith('j')) and len(tokens) == 2
            assert tokens[0] == ins[0].mnemonic and int(tokens[1], 16) - int(ins[0].op_str, 16) == 0x400000, key
    for r in d['data_evidence'].values():
        b = bytes.fromhex(r['bytes'])
        o = int(r['file_byte_offset'], 16)
        assert data[o:o + len(b)] == b
        if 'elf_image_vaddr' in r:
            assert o == fmap(int(r['elf_image_vaddr'], 16), len(b))
            assert b.decode('utf-16le') == r['decoded_utf16le']
    for r in d['sources']:
        assert Path(r['path']).stat().st_size == r['bytes'] and sha(r['path']) == r['sha256']
    if name == 'UCLASS':
        pt = [r for r in ph if r[0] == 2][0]
        dyn = {}
        for o in range(pt[2], pt[2] + pt[5], 16):
            k, v = struct.unpack_from('<QQ', data, o)
            if k == 0:
                break
            dyn[k] = v
        assert dyn[9] == 24
        start = fmap(dyn[7], dyn[8])
        r = d['data_evidence']['UClass_CreateDefaultObject_rela']
        matches = []
        for o in range(start, start + dyn[8], 24):
            ro, ri, ra = struct.unpack_from('<QQq', data, o)
            if ro == int(r['r_offset'], 16):
                matches.append((o, ri, ra))
        assert matches == [(int(r['file_byte_offset'], 16), 8, 0x3222a0)]
        assert int(r['r_offset'], 16) == 0x7ed7a28 + 0x400
    rows.append({'receipt': str(p), 'sha256': digest, 'instruction_windows_exact': len(d['evidence']),
                 'data_windows_exact': len(d['data_evidence']), 'source_pins_exact': len(d['sources']),
                 'accepted_facts': d['facts'], 'unresolved': d['unresolved']})
record = {
    'status': 'FIELD_CLASS_STATIC_AUDIT_PASS', 'receipts': rows,
    'image_sha256': hashlib.sha256(data).hexdigest(),
    'semantic_review': {
        'FFieldClass': 'Named FField class descriptor constructor copies constructed FName qword0, stores CastFlags qword10 and SuperClass qword20. Independent class-id/cast-test/hierarchy walk corroborates roles. Neither descriptor guard spacing nor largest touched extent proves total size.',
        'UClass': 'Named Class descriptor sets ClassCastFlagsD8 and inherits superclass flags through qwordD8. CDO body checks cached qword110, recursively checks super110 using virtual400, calls StaticAllocateObjectInternal with this Class, stores returned object110 and returns it. Named UClass vptr7ED7A28+400 has exactly one actual DT_RELA RELATIVE binding3222A0.',
        'CDO_slot_scope': 'CreateDefaultObject400 is supplemental; absent selected caller trace does not make it a required slot.'
    }, 'findings': [], 'source_mutations': 0, 'console_network_contact': False,
    'limitations': 'Static saved image only; no live object/menu/spawn proof.'
}
out = Path(__file__).parent / 'FIELD-CLASS-AUDIT.json'
out.write_text(json.dumps(record, indent=2) + '\n')
print('PASS 49 instructions, 3 data windows,21 pins, unique CDO RELA; SHA', sha(out))
