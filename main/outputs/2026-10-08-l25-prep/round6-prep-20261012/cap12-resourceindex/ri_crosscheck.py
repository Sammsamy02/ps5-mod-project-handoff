#!/usr/bin/env python3
"""Mac-only.  Independent cross-check of the decoded per-shader ResourceIndex /
SortKey against the ShaderTypeInfo .stinfo sidecar, and the expectations file
for the later M1b code-library fetch (M1b itself needs the user's OK).

.stinfo layout (M1a, parsed to EOF): u32 count, count x {u32 n, u64 type_hash x n}.

Checks (all must hold, each prints how many items it checked):
  A  every (map, resource_index) slot has ONE sort_key and ONE target
  B  unique sort_keys over all slots == stinfo entry count
  C  stinfo entry i  ==  set of shader types of the i-th NEW sort_key, walking
     maps in file order and resource_index ascending (positional, all i)
  D  total distinct (entry, type) refs == stinfo hash refs

Usage: ri_crosscheck.py <GlobalShaderCache.bin> <ShaderTypeInfo-Global.stinfo> <out-dir>
Writes <out-dir>/stinfo-crosscheck.json and <out-dir>/m1b-expectations.json
(the latter carries the per-map predictions AND the numbered M1b checks 1-6,
including the stop rule, under 'm1b_checks').
"""
import sys
sys.dont_write_bytecode = True
import collections  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import struct  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gsc_codec_ri as g  # noqa: E402

data = open(sys.argv[1], 'rb').read()
st = open(sys.argv[2], 'rb').read()
outdir = sys.argv[3]
model = g.decode(data)

# stinfo
cnt = struct.unpack_from('<I', st, 0)[0]
p, entries = 4, []
for _ in range(cnt):
    n = struct.unpack_from('<I', st, p)[0]
    p += 4
    entries.append(['%016x' % x for x in struct.unpack_from('<%dQ' % n, st, p)])
    p += 8 * n
stinfo_eof = p == len(st)

# slots: (section, resource_index) -> shaders
slots = collections.OrderedDict()
for r in g.shader_rows(model):
    o = r['obj']
    slots.setdefault((r['section'], o['resource_index']), []).append(r)
slot_keys = sorted(slots)  # file order of maps, then resource_index ascending
A_checked = A_bad = 0
slot_info = {}
for key in slot_keys:
    rs = slots[key]
    A_checked += 1
    sks = {x['obj']['sort_key'] for x in rs}
    tgs = {x['obj']['target'] for x in rs}
    if len(sks) != 1 or len(tgs) != 1:
        A_bad += 1
    types = []
    for x in rs:
        if x['type_hash'] not in types:
            types.append(x['type_hash'])
    slot_info[key] = {'sort_key': rs[0]['obj']['sort_key'], 'target': rs[0]['obj']['target'], 'types': types,
                      'n_shaders': len(rs), 'classes': sorted({x['class'] for x in rs})}

# predicted library entry numbering: first occurrence of each sort_key
lib_index = {}
lib_types = []
pred_index = {}
for key in slot_keys:
    sk = slot_info[key]['sort_key']
    if sk not in lib_index:
        lib_index[sk] = len(lib_types)
        lib_types.append(list(slot_info[key]['types']))
    else:
        for t in slot_info[key]['types']:
            if t not in lib_types[lib_index[sk]]:
                lib_types[lib_index[sk]].append(t)
    pred_index[key] = lib_index[sk]

C_checked = min(len(entries), len(lib_types))
C_match = sum(1 for i in range(C_checked) if set(entries[i]) == set(lib_types[i]))
C_order_match = sum(1 for i in range(C_checked) if entries[i] == lib_types[i])
refs_mine = sum(len(t) for t in lib_types)
refs_st = sum(len(e) for e in entries)
shared = collections.defaultdict(list)
for key in slot_keys:
    shared[slot_info[key]['sort_key']].append(list(key))
shared = {('%08x' % k): v for k, v in shared.items() if len(v) > 1}

res = {
    'label': 'Mac-only',
    'input_sha256': hashlib.sha256(data).hexdigest(),
    'stinfo': sys.argv[2], 'stinfo_sha256': hashlib.sha256(st).hexdigest(),
    'stinfo_entries': cnt, 'stinfo_parsed_to_eof': stinfo_eof,
    'A_slots_checked': A_checked, 'A_slots_with_conflicting_sort_key_or_target': A_bad,
    'B_unique_sort_keys': len(lib_types), 'B_equals_stinfo_entries': len(lib_types) == cnt,
    'C_positions_checked': C_checked, 'C_positional_type_set_matches': C_match,
    'C_positional_exact_order_matches (observation)': C_order_match,
    'D_type_refs_mine': refs_mine, 'D_type_refs_stinfo': refs_st, 'D_equal': refs_mine == refs_st,
    'map_code_slots': len(slot_keys),
    'sort_keys_used_by_2plus_slots': len(shared),
    'extra_slots_deduplicated': len(slot_keys) - len(lib_types),
    'shared_sort_keys': shared,
}
ok = (A_checked > 0 and A_bad == 0 and res['B_equals_stinfo_entries'] and C_checked > 0
      and C_match == C_checked == cnt and res['D_equal'] and stinfo_eof)
res['verdict'] = 'PASS' if ok else 'FAIL'
json.dump(res, open(os.path.join(outdir, 'stinfo-crosscheck.json'), 'w'), indent=1)

# expectations for M1b (predictions to be CHECKED against the fetched library, never shipped)
maps = []
for s in model['sections']:
    keys = [k for k in slot_keys if k[0] == s['index']]
    maps.append({
        'section': s['index'],
        'resource_hash_sha1_20B': s['resource_hash'],
        'platform_name': s['platform_name']['text'],
        'num_shader_refs': s['frozen']['arrays']['shaders']['num'],
        'num_code_slots (max resource_index + 1)': len(keys),
        'slots': [{'resource_index': k[1], 'predicted_library_entry': pred_index[k],
                   'sort_key_u32': '%08x' % slot_info[k]['sort_key'],
                   'frequency_low4_of_target': slot_info[k]['target'] & 0xF,
                   'target_u32': slot_info[k]['target'],
                   'shader_type_hashes': slot_info[k]['types'], 'n_shader_refs': slot_info[k]['n_shaders']}
                  for k in keys],
    })
freq_hist = collections.Counter()
for i, key in enumerate(slot_keys):
    pass
first_slot_of_entry = {}
for key in slot_keys:
    first_slot_of_entry.setdefault(pred_index[key], key)
for e, key in first_slot_of_entry.items():
    freq_hist[slot_info[key]['target'] & 0xF] += 1
# within-map slot order vs the sort_key's 4 stored bytes (little-endian u32)
order_multi = order_ok = order_num = 0
for m_ in maps:
    ks = [int(x['sort_key_u32'], 16) for x in m_['slots']]
    if len(ks) > 1:
        order_multi += 1
        sb = [struct.pack('<I', k) for k in ks]
        order_ok += int(all(sb[i] < sb[i + 1] for i in range(len(sb) - 1)))
        order_num += int(all(ks[i] < ks[i + 1] for i in range(len(ks) - 1)))
ex0 = maps[0]['slots'][0]['sort_key_u32']
M1B_CHECKS = [
    {'id': 1, 'check': 'set of the fetched index\'s shader-map hashes == set of the 492 resource_hash_sha1_20B values here',
     'on_fail': 'STOP'},
    {'id': 2, 'check': 'unique code entries == library_totals.unique_code_entries (9388); sum of per-map shader counts == '
     'map_code_slots_total (9414); for every map, its shader count == num_code_slots', 'on_fail': 'STOP'},
    {'id': 3, 'check': 'for every map, its shader-index list (in stored order) == [slot.predicted_library_entry for slot in '
     'slots] (resource_index order); a reused sort_key reuses its entry', 'on_fail': 'STOP (numbering rule refuted)'},
    {'id': 4, 'check': 'if per-entry stage/frequency codes are stored: their histogram == library_totals.entries_by_frequency_low4',
     'on_fail': 'STOP'},
    {'id': 5, 'check': 'HYPOTHESIS: each entry\'s code hash bytes[0:4] == the sort_key\'s 4 stored bytes, i.e. '
     'int.from_bytes(hash[0:4], "little") == int(sort_key_u32, 16). Example: map 0 slot 0 sort_key_u32 %s -> stored '
     'bytes %s. Supporting observation: in %d/%d maps with 2+ slots the resource_index order is ascending in those '
     'stored bytes (numeric u32 order only %d/%d).' % (ex0, bytes.fromhex(ex0)[::-1].hex(), order_ok, order_multi,
                                                     order_num, order_multi),
     'on_fail': 'not a stop: sort_key becomes check-only (never used to look up code); record the count that matched'},
    {'id': 6, 'check': 'any count mismatch in checks 1-4 (or a zero count anywhere) stops the M1b decode; nothing from the '
     'fetched library is used or shipped until 1-4 all pass', 'on_fail': 'STOP'},
]
exp = {
    'label': 'Mac-only; PREDICTIONS for checking the M1b fetch, not values to ship',
    'input_sha256': res['input_sha256'],
    'library_totals': {
        'shader_maps': len(maps),
        'unique_resource_hashes': len({m['resource_hash_sha1_20B'] for m in maps}),
        'map_code_slots_total (sum of per-map shader counts in the library index)': len(slot_keys),
        'unique_code_entries': len(lib_types),
        'stinfo_entries': cnt,
        'entries_by_frequency_low4': dict(sorted(freq_hist.items())),
    },
    'numbering_rule (predicted)': 'library entry numbers assigned in order of first appearance, walking maps in '
                                  'file order and resource_index ascending; a slot whose sort_key was already seen '
                                  'reuses that entry (sort_key = 32-bit per-code key; full code hash not in this file)',
    'slot_order_observation (Mac-only, this file)': {
        'maps_with_2plus_slots': order_multi,
        'resource_index_order == ascending order of sort_key stored bytes': order_ok,
        'numeric u32 ascending (contrast)': order_num,
    },
    'm1b_checks': M1B_CHECKS,
    'maps': maps,
}
json.dump(exp, open(os.path.join(outdir, 'm1b-expectations.json'), 'w'))
print(json.dumps({k: res[k] for k in ('stinfo_entries', 'A_slots_checked', 'A_slots_with_conflicting_sort_key_or_target',
                                      'B_unique_sort_keys', 'C_positions_checked', 'C_positional_type_set_matches',
                                      'D_type_refs_mine', 'D_type_refs_stinfo', 'extra_slots_deduplicated',
                                      'verdict')}))
print(json.dumps({'m1b_checks_written': len(M1B_CHECKS), 'slot_order_maps_checked': order_multi,
                  'slot_order_ok': order_ok}))
if A_checked == 0 or C_checked == 0 or order_multi == 0 or not M1B_CHECKS:
    print('FAIL: zero items checked')
    sys.exit(1)
sys.exit(0 if ok else 1)
