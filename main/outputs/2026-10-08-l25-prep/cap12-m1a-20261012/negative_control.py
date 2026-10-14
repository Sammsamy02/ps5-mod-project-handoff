#!/usr/bin/env python3
"""Mac-only. Negative controls for gsc_codec: edit one decoded field, re-encode,
and confirm the first differing byte lands exactly where that field lives.
Optional: cross-check type hashes against a ShaderTypeInfo .stinfo sidecar
(assumed layout u32 count, then count x {u32 n, u64 hash x n}; reported as a
hypothesis check, not a proof)."""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gsc_codec as g

data = open(sys.argv[1], 'rb').read()
m = g.decode(data)
res = {'label': 'Mac-only', 'controls': []}

def control(name, mutate, expect):
    mm = json.loads(json.dumps(m))
    mutate(mm)
    out = g.encode(mm)
    off = g.first_diff(data, out)
    res['controls'].append({'edit': name, 'expected_first_diff': expect, 'first_diff': off,
                            'field': g.field_at(m, off) if off is not None else None, 'pass': off == expect})

s0 = m['sections'][0]
f0 = s0['frozen']
pa = f0['array_data']['shader_permutations']
def mut_perm(mm):
    mm['sections'][0]['frozen']['array_data']['shader_permutations']['items'][1] ^= 0x40
control('section 0 permutation[1]', mut_perm, s0['frozen_offset'] + pa['data_at'] + 4)
def mut_rh(mm):
    h = mm['sections'][0]['resource_hash']
    mm['sections'][0]['resource_hash'] = ('%02x' % (int(h[:2], 16) ^ 1)) + h[2:]
control('section 0 resource_hash byte 0', mut_rh, s0['end'] - 20)
last = m['sections'][-1]
def mut_type(mm):
    t = mm['sections'][-1]['ptr_types']
    t[0] = '%016x' % (int(t[0], 16) ^ 1)
# ptr types start after deps; locate by scanning the original bytes for the first type hash
first_type = struct.pack('<Q', int(last['ptr_types'][0], 16))
pos = data.index(first_type, last['frozen_offset'] + last['frozen_size'])
control('last section ptr_types[0] low byte', mut_type, pos)

if len(sys.argv) > 3:
    st = open(sys.argv[3], 'rb').read()
    cnt = struct.unpack_from('<I', st, 0)[0]
    p, hashes, ok = 4, [], True
    for _ in range(cnt):
        if p + 4 > len(st): ok = False; break
        n = struct.unpack_from('<I', st, p)[0]; p += 4
        if n > 1024 or p + 8 * n > len(st): ok = False; break
        hashes += ['%016x' % x for x in struct.unpack_from('<%dQ' % n, st, p)]; p += 8 * n
    known = set()
    for s in m['sections']:
        known.update(s['ptr_types'])
    res['stinfo_check'] = {'file': sys.argv[3], 'declared_entries': cnt, 'layout_parsed_to_eof': ok and p == len(st),
                           'hash_refs': len(hashes), 'unique_hashes': len(set(hashes)),
                           'unique_in_gsc_type_set': len(set(hashes) & known), 'gsc_unique_types': len(known)}
json.dump(res, open(sys.argv[2], 'w'), indent=1)
print(json.dumps(res, indent=1))
