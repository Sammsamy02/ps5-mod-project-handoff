import capstone
import hashlib
import json
import struct
from pathlib import Path

p = Path('outputs/2026-10-07-l19-property-subtype-evidence/SUBTYPE-FACTS.json')
d = json.loads(p.read_text())
sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
assert sha(p) == '5429bed5e3e6a05c5d44f2baec856fdc221e30abced63c7a11ec6460147be033'
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
for key, r in d['evidence'].items():
    v = int(r['elf_image_vaddr'], 16)
    b = bytes.fromhex(r['instruction_bytes'])
    o = fmap(v, len(b))
    assert o == int(r['file_byte_offset'], 16) and data[o:o + len(b)] == b
    ins = list(md.disasm(b, v))
    assert len(ins) == 1 and ins[0].size == len(b)
    assert (ins[0].mnemonic + ' ' + ins[0].op_str).strip() == r['instruction'].strip(), key
for call, callee in d['direct_call_targets'].items():
    v = int(call, 16)
    o = fmap(v, 5)
    assert data[o] == 0xe8
    assert v + 5 + struct.unpack_from('<i', data, o + 1)[0] == int(callee, 16)
names = 0
relas = {}
for key, r in d['data_evidence'].items():
    b = bytes.fromhex(r['bytes'])
    o = int(r['file_byte_offset'], 16)
    assert data[o:o + len(b)] == b
    if 'elf_image_vaddr' in r:
        assert o == fmap(int(r['elf_image_vaddr'], 16), len(b))
        assert b.decode('utf-16le') == r['decoded_utf16le']
        names += 1
    else:
        target = int(r['r_offset'], 16)
        assert target - int(r['vptr_elf_image_vaddr'], 16) == int(r['slot_byte_offset'], 16)
        assert struct.unpack('<QQq', b) == (target, int(r['r_info'], 16), int(r['r_addend'], 16))
        relas[target] = (key, o, int(r['r_info'], 16), int(r['r_addend'], 16))
dyn = {}
pt = [r for r in ph if r[0] == 2][0]
for o in range(pt[2], pt[2] + pt[5], 16):
    k, v = struct.unpack_from('<QQ', data, o)
    if k == 0:
        break
    dyn[k] = v
assert dyn[9] == 24 == d['relocation_table']['entry_size']
assert dyn[7] == int(d['relocation_table']['vaddr'], 16)
assert dyn[8] == d['relocation_table']['size']
o = fmap(dyn[7], dyn[8])
assert o == int(d['relocation_table']['file_offset'], 16)
matches = {target: [] for target in relas}
for a in range(o, o + dyn[8], 24):
    ro, ri, ra = struct.unpack_from('<QQq', data, a)
    if ro in matches:
        matches[ro].append((a, ri, ra))
for target, (key, a, ri, ra) in relas.items():
    assert matches[target] == [(a, ri, ra)], key
pins = json.loads((p.parent / 'SOURCE-HASHES.json').read_text())
for f, expected in pins.items():
    assert sha(p.parent / f) == expected, f
record = {
    'status': 'SUBTYPE_STATIC_AUDIT_PASS',
    'scope': 'Independent frozen saved-image subtype audit; no source, engine, console, or network actions',
    'fact_receipt_sha256': sha(p), 'image_sha256': d['image']['sha256'],
    'instruction_windows_exact_PT_LOAD_and_single_instruction_decoding': len(d['evidence']),
    'direct_rel32_call_targets_exact': len(d['direct_call_targets']),
    'utf16_names_exact': names, 'unique_actual_DT_RELA_slot_bindings': len(relas),
    'all_source_pins_exact': len(pins), 'accepted_facts': d['facts'],
    'semantic_review': {
        'totals': 'Four named UTF16 static descriptors pass ConstructFn in R9 to common descriptor+30 storage. Each exact callback requests80/80/78/78 from native allocator B7A20, preserves complete-object RAX through RBX, installs its named vptr, and returns the same unadjusted pointer. This is named native construction size, not largest touched extent.',
        'array': 'Copy/destroy named virtuals use qword78 as nested FProperty and its ElementSize30/PropertyFlags38/alignment170. Byte70 has a separate flags role.',
        'struct': 'Named alignment callback uses qword70 then exact UStruct.MinAlignment5C word. Copy/destroy use the same struct pointer.',
        'bool': 'Byte71 selects intra-element address; byte72 is true-value OR; byte73 is destination clear/source mask. Native bool assigns72=1 versus73=FF, excluding swapped labels.',
        'enum': 'Named EnumProperty callback and constructor establish type context. Qword70 dispatches nested FProperty alignment and numeric get/set/serialization. Qword78 receives genparams enum-provider result, has separate archive reference serialization, and supplies Names40/count48 plus enum-value mapping. Combined independent roles support UnderlyingProp70 and Enum78; provider callback alone would not establish exact type.'
    },
    'findings': [], 'source_mutations': 0, 'console_network_contact': False,
    'limitations': 'Saved native static construction and layout evidence only; no live allocation, mod menu, or spawn proof.'
}
out = Path(__file__).parent / 'SUBTYPE-AUDIT.json'
out.write_text(json.dumps(record, indent=2) + '\n')
print('PASS', len(d['evidence']), 'instructions;', names, 'names;', len(relas), 'unique RELA;', len(pins), 'pins; SHA', sha(out))
