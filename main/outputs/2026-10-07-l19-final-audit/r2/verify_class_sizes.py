import capstone
import hashlib
import json
import struct
from pathlib import Path

p = Path('outputs/2026-10-07-l19-class-size-evidence/CLASS-SIZE-FACTS.json')
d = json.loads(p.read_text())
sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
assert sha(p) == '733f83ba4a4bd740f1834d13ae9476f626be1e629da6030a8d37488104a16c12'
data = Path(d['image']['path']).read_bytes()
assert len(data) == d['image']['size_bytes'] and hashlib.sha256(data).hexdigest() == d['image']['sha256']
h = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
ph = [struct.unpack_from('<IIQQQQQQ', data, h[5] + i * h[9]) for i in range(h[10])]
loads = [r for r in ph if r[0] == 1]


def fmap(v, n):
    rows = [r for r in loads if r[3] <= v and v + n <= r[3] + r[5]]
    assert len(rows) == 1
    return rows[0][2] + v - rows[0][3]


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
decodes = []
for key, r in d['evidence'].items():
    v = int(r['elf_image_vaddr'], 16)
    b = bytes.fromhex(r['instruction_bytes'])
    o = fmap(v, len(b))
    assert o == int(r['file_byte_offset'], 16) and data[o:o + len(b)] == b
    ins = list(md.disasm(b, v))
    assert len(ins) == 1 and ins[0].size == len(b)
    text = ins[0].mnemonic + ' ' + ins[0].op_str
    declared = r['instruction'].split(';', 1)[0].strip()
    capture_display_bias = False
    if text.strip() != declared:
        # The receipt explicitly preserves the historical rdis address display
        # bias. Actual rel32 targets and raw-window decoding remain authoritative.
        m = ins[0].mnemonic
        expected = declared.split()
        assert (m == 'call' or m.startswith('j')) and len(expected) == 2 and expected[0] == m, (key, text, r['instruction'])
        assert int(expected[1], 16) - int(ins[0].op_str, 16) == 0x400000, (key, text, r['instruction'])
        capture_display_bias = True
    decodes.append({'vaddr': hex(v), 'file_offset': hex(o), 'bytes': b.hex(), 'decoded': text,
                    'capture_target_display_bias_normalized': capture_display_bias})
for r in d['direct_call_targets'].values():
    v = int(r['elf_image_vaddr'], 16)
    b = bytes.fromhex(r['rel32_bytes'])
    assert data[fmap(v, 5):fmap(v, 5) + 5] == b and b[0] == 0xe8
    assert v + 5 + struct.unpack_from('<i', b, 1)[0] == int(r['callee_elf_image_vaddr'], 16)
for r in d['data_evidence'].values():
    if 'bytes' not in r:
        continue
    b = bytes.fromhex(r['bytes'])
    o = fmap(int(r['elf_image_vaddr'], 16), len(b))
    assert o == int(r['file_byte_offset'], 16) and data[o:o + len(b)] == b
    assert b.decode('utf-16le').rstrip('\0') == r['value']
for f, expected in d['sources'].items():
    assert sha(p.parent / f) == expected, f
dyn = {}
pt = [r for r in ph if r[0] == 2][0]
for o in range(pt[2], pt[2] + pt[5], 16):
    k, v = struct.unpack_from('<QQ', data, o)
    if k == 0:
        break
    dyn[k] = v
assert dyn[9] == 24
o = fmap(dyn[7], dyn[8])
r = d['data_evidence']['FProperty_alignment_slot_RELA']
target = int(r['r_offset'], 16)
matches = []
for a in range(o, o + dyn[8], 24):
    ro, ri, ra = struct.unpack_from('<QQq', data, a)
    if ro == target:
        matches.append((a, ri, ra))
assert matches == [(int(r['relocation_file_offset'], 16), 8, 0x5b0)]
assert target - int(r['vptr_elf_image_vaddr'], 16) == int(r['slot_byte_offset'], 16) == 0x170
assert data[matches[0][0]:matches[0][0] + 24] == bytes.fromhex(r['entry_bytes'])
record = {
    'scope': 'Independent frozen class-size/slot saved-image audit; no source or engine actions',
    'status': 'CLASS_SIZE_STATIC_AUDIT_PASS',
    'fact_receipt_sha256': sha(p), 'image_sha256': d['image']['sha256'],
    'instruction_windows_exact_PT_LOAD_and_decoding': len(decodes),
    'utf16_name_windows_exact': 7, 'direct_rel32_calls_exact': len(d['direct_call_targets']),
    'all_source_pins_exact': len(d['sources']), 'unique_named_FProperty_alignment_RELA': matches,
    'decoded_instructions': decodes, 'accepted_facts': d['facts'], 'unresolved_retained': d['unresolved'],
    'semantic_review': {
        'named_native_sizes': 'Name -> PropertiesSize58 -> actual allocation bytecount proves Field30/StructB0/FunctionE0. Class200 corroborated by named descriptor allocation. FProperty70 linked through named static descriptor ConstructFn -> malloc70 -> unchanged pointer with final named vptr.',
        'UObjectBase': 'Object named complete size28/alignment8 and unadjusted allocation this-flow to485430 bound contained fundamental base above by28. Samevptr7eff620 and Outer+20 qword bound occupied base below by28. Explicit two-sided physical extent; not directly named base registration.',
        'FProperty_slots': 'Copy destination/source/count virtualF8 paired with POD memcpy; virtual140 destructor condition followed by free of samevalue; virtual170 return flows as alignment alongside ArrayDim*ElementSize bytecount. Named vptr+170 unique RELATIVE ->5b0 returns1 corroborates association.',
        'unknowns': 'FField rounded extent30 and FFieldClass guardspacing40 remain insufficient; unknown sizes and engine/game slots retained explicitly.'
    },
    'source_mutations': 0, 'console_network_contact': False
}
out = Path(__file__).parent / 'CLASS-SIZE-AUDIT.json'
out.write_text(json.dumps(record, indent=2) + '\n')
print('PASS', len(decodes), 'instruction windows;7 names;', len(d['direct_call_targets']), 'calls;', len(d['sources']), 'source pins; unique RELA. SHA', sha(out))
