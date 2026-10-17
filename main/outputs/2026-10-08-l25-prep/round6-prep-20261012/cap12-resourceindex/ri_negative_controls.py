#!/usr/bin/env python3
"""Mac-only.  Negative controls for gsc_codec_ri.py.

For EVERY newly decoded shader-object field: change that one field in the
decoded model, re-encode the whole file, and check that the first differing
byte is exactly that field's position AND that every differing byte lies
inside that field (no spill into later bytes: last differing byte and the
number of differing bytes are recorded and checked).  Each control edits a block that is
not shared with any other array (de-duplicated blocks are tested separately:
editing one sharer must make the encoder refuse).

Fail-closed controls: shared-block edit, wrong class size, null vf table,
unreferenced object -> the encoder must raise, never write.

Usage: ri_negative_controls.py <in.bin> <out.json>
Exit 0 only if >0 controls ran and all passed.
"""
import sys
sys.dont_write_bytecode = True
import collections  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gsc_codec_ri as g  # noqa: E402

data = open(sys.argv[1], 'rb').read()
model = json.loads(json.dumps(g.decode(data)))
secs = model['sections']
res = {'label': 'Mac-only', 'input_sha256': __import__('hashlib').sha256(data).hexdigest(),
       'field_controls': [], 'fail_closed_controls': []}

# reference counts of data blocks per section (root arrays + object arrays)
refcount = {}
for s in secs:
    rc = collections.Counter()
    for nm, a in s['frozen']['array_data'].items():
        if a['ptr'] is not None and a['items']:
            rc[a['data_at']] += 1
    for o in s['frozen']['shader_objects']['objects']:
        for label, pos, kind, a in g._arrays_of(o):
            if a['items']:
                rc[a['at']] += 1
    refcount[s['index']] = rc


def first_diff_fast(a, b):
    n = min(len(a), len(b))
    step = 1 << 16
    for i in range(0, n, step):
        if a[i:i + step] != b[i:i + step]:
            for j in range(i, min(i + step, n)):
                if a[j] != b[j]:
                    return j
    return None if len(a) == len(b) else n


def all_diffs(a, b, cap=4096):
    """every differing offset (capped); length change counts as a diff at the end"""
    out = []
    n = min(len(a), len(b))
    step = 1 << 16
    for i in range(0, n, step):
        if a[i:i + step] != b[i:i + step]:
            for j in range(i, min(i + step, n)):
                if a[j] != b[j]:
                    out.append(j)
                    if len(out) >= cap:
                        return out
    if len(a) != len(b):
        out.append(n)
    return out


def run_field(name, sec, mutate, restore, expect_img_off, width, extra_spans=()):
    """width: byte width of the edited field; extra_spans: further
    (image_offset, width) spans the edit is ALLOWED to change (data move)."""
    mutate()
    try:
        out = g.encode(model)
        diffs = all_diffs(data, out)
        err = None
    except Exception as e:  # a field control must not raise
        diffs, err = None, repr(e)
    finally:
        restore()
    fo = sec['frozen_offset']
    exp = fo + expect_img_off
    spans = [(exp, exp + width)] + [(fo + a, fo + a + w_) for a, w_ in extra_spans]
    off = diffs[0] if diffs else None
    inside = bool(diffs) and all(any(a <= d < b for a, b in spans) for d in diffs)
    res['field_controls'].append({'field': name, 'section': sec['index'], 'expected_first_diff': exp,
                                  'first_diff': off, 'last_diff': diffs[-1] if diffs else None,
                                  'n_diff_bytes': len(diffs) if diffs is not None else None,
                                  'allowed_spans': [list(x) for x in spans], 'all_diffs_inside_allowed_spans': inside,
                                  'image_offset': expect_img_off,
                                  'located_in': g.m1a.field_at(model, off) if off is not None else err,
                                  'pass': off == exp and inside})


def run_fail_closed(name, mutate, restore, expect_exc):
    mutate()
    try:
        g.encode(model)
        got = 'no exception (encoder wrote output)'
        ok = False
    except Exception as e:
        got = '%s: %s' % (type(e).__name__, str(e)[:160])
        ok = isinstance(e, expect_exc)
    finally:
        restore()
    res['fail_closed_controls'].append({'control': name, 'expected': expect_exc.__name__, 'got': got, 'pass': ok})


def objects():
    for s in secs:
        for o in s['frozen']['shader_objects']['objects']:
            yield s, o


def first_unshared(label_pred):
    """first (section, object, label, pos, kind, array) whose data block is unshared"""
    for s, o in objects():
        for label, pos, kind, a in g._arrays_of(o):
            if label_pred(label) and a['items'] and a['at'] >= o['at'] and refcount[s['index']][a['at']] == 1:
                return s, o, label, pos, kind, a
    return None


def shared_instance(label):
    """first non-empty instance of label; returns (section, [all arrays of the
    same kind sharing its block], kind) when every sharer has that kind"""
    for s, o in objects():
        for lb, pos, kind, a in g._arrays_of(o):
            if lb != label or not a['items']:
                continue
            sharers, kinds = [], set()
            for o2 in s['frozen']['shader_objects']['objects']:
                for lb2, pos2, kind2, a2 in g._arrays_of(o2):
                    if a2['items'] and a2['at'] == a['at']:
                        sharers.append(a2)
                        kinds.add(kind2)
            roots = sum(1 for a3 in s['frozen']['array_data'].values() if a3['ptr'] is not None and a3['items'] and a3['data_at'] == a['at'])
            if kinds == {kind} and not roots:
                return s, sharers, kind
    return None


def flip_item(arr, i, j):
    """returns (mutate, restore) flipping the low bit of sub-field j of item i"""
    it = arr['items'][i]
    if isinstance(it, list):
        old = it[j]
        return (lambda: it.__setitem__(j, old ^ 1)), (lambda: it.__setitem__(j, old))
    old = it
    if isinstance(old, str):
        new = '%016x' % (int(old, 16) ^ 1)
    else:
        new = old ^ 1
    return (lambda: arr['items'].__setitem__(i, new)), (lambda: arr['items'].__setitem__(i, old))


def flip_key(d, k, hexfmt=None):
    old = d[k]
    new = ('%016x' % (int(old, 16) ^ 1)) if isinstance(old, str) else old ^ 1
    return (lambda: d.__setitem__(k, new)), (lambda: d.__setitem__(k, old))


# byte width of each element sub-field (same order as g.ELEM_SUBFIELD_OFFSETS)
SUBW = {'u16': [2], 'u16x2': [2, 2], 'u16x4': [2, 2, 2, 2], 'u16u8u8': [2, 1, 1],
        'u16u16u8u8': [2, 2, 1, 1], 'u64': [8]}
for _k, _offs in g.ELEM_SUBFIELD_OFFSETS.items():
    assert len(SUBW[_k]) == len(_offs) and sum(SUBW[_k]) == g.ELEM[_k][1], _k

# 1. scalar fields of the object base (first object is fine: not shared)
s0, o0 = next(objects())
for label, off, fmt in g.BASE_SCALARS:
    mu, re = flip_key(o0, label)
    run_field('object.' + label, s0, mu, re, o0['at'] + off, __import__('struct').calcsize(fmt))
# 2. indexed pointer: type_index (change to another valid table index)
for s, o in objects():
    if len(s['ptr_types']) > 1:
        old = o['type_index']
        new = old ^ 1 if (old ^ 1) < len(s['ptr_types']) else old - 1
        run_field('object.type_index', s, lambda: o.__setitem__('type_index', new),
                  lambda: o.__setitem__('type_index', old), o['at'] + 192, 8)
        break
# 3. subclass (still-raw) bytes: first object that has them
for s, o in objects():
    if o['subclass_fields_hex']:
        old = o['subclass_fields_hex']
        new = '%02x' % (int(old[:2], 16) ^ 1) + old[2:]
        run_field('object.subclass_fields_hex[0] (raw)', s, lambda: o.__setitem__('subclass_fields_hex', new),
                  lambda: o.__setitem__('subclass_fields_hex', old), o['at'] + g.SHADER_BASE_SIZE, 1)
        break
# 4. every array: each element sub-field, plus max; loose-buffer element fields
for label, off, kind in g.BASE_ARRAYS:
    hit = first_unshared(lambda lb, L=label: lb == L)
    if hit is None:
        # every instance is a de-duplicated block: edit the block in ALL of
        # its sharers (same kind) so the model stays consistent
        sh = shared_instance(label)
        if sh is None:
            res['field_controls'].append({'field': label, 'pass': False, 'note': 'no instance found'})
            continue
        s, sharers, kind = sh
        at = sharers[0]['at']
        for j, so in enumerate(g.ELEM_SUBFIELD_OFFSETS[kind]):
            pairs = [flip_item(a, 0, j) for a in sharers]
            run_field('%s[0].field%d (+%d) [block shared by %d arrays, all edited]' % (label, j, so, len(sharers)), s,
                      lambda P=pairs: [m() for m, _ in P], lambda P=pairs: [r() for _, r in P], at + so, SUBW[kind][j])
        continue
    s, o, lb, pos, kind, a = hit
    if kind == 'loose':
        it = a['items'][0]
        for fld, fo, fw in (('base_index', 0, 2), ('size', 2, 2), ('pad_u32', 4, 4)):
            mu, re = flip_key(it, fld)
            run_field('%s[0].%s' % (label, fld), s, mu, re, a['at'] + fo, fw)
    else:
        for j, so in enumerate(g.ELEM_SUBFIELD_OFFSETS[kind]):
            mu, re = flip_item(a, 0, j)
            run_field('%s[0].field%d (+%d)' % (label, j, so), s, mu, re, a['at'] + so, SUBW[kind][j])
    mu, re = flip_key(a, 'max')
    run_field('%s.max' % label, s, mu, re, pos + 12, 4)
# nested loose-parameter arrays
hit = first_unshared(lambda lb: lb.endswith('.parameters'))
if hit:
    s, o, lb, pos, kind, a = hit
    for j, so in enumerate(g.ELEM_SUBFIELD_OFFSETS['u16x2']):
        mu, re = flip_item(a, 0, j)
        run_field('%s[0].field%d (+%d)' % (lb.split('[')[0] + '[i].parameters', j, so), s, mu, re, a['at'] + so,
                  SUBW['u16x2'][j])
    mu, re = flip_key(a, 'max')
    run_field('loose_parameter_buffers[i].parameters.max', s, mu, re, pos + 12, 4)
# 5. data location ('at'): move a 1-item unshared u16 array into a zero gap
#    after its own header; the first change must be the header's pointer.
done_at = False
for s, o in objects():
    if done_at:
        break
    so_ = s['frozen']['shader_objects']
    region, w = g.encode_objects(s, so_, return_writer=True)
    for label, pos, kind, a in g._arrays_of(o):
        if kind != 'u16' or len(a['items']) != 1 or refcount[s['index']][a['at']] != 1 or a['at'] < o['at']:
            continue
        gap = None
        for q in range(pos + 16, len(w.mask) - 1):
            if not w.mask[q] and not w.mask[q + 1]:
                gap = q
                break
        if gap is None:
            continue
        old = a['at']
        # the pointer's self-relative offset occupies bits 24..63 = bytes 3..7
        run_field('%s.at (data location; pointer offset bits re-derived)' % label, s,
                  lambda: a.__setitem__('at', gap), lambda: a.__setitem__('at', old), pos + 3, 5,
                  extra_spans=((old, 2), (gap, 2)))
        done_at = True
        break

# checker self-test: an edit that spills past the declared field (two
# adjacent scalars changed, only the first declared) MUST be reported as a
# failure by run_field; it is recorded separately, not as a field control.
_m1, _r1 = flip_key(o0, 'structure_layout_hash')
_m2, _r2 = flip_key(o0, 'root_parameter_buffer_index')
run_field('SELFTEST spill: structure_layout_hash declared, root_parameter_buffer_index also edited', s0,
          lambda: (_m1(), _m2()), lambda: (_r1(), _r2()), o0['at'] + 80, 4)
_st = res['field_controls'].pop()
res['checker_selftest_spill_detected'] = (not _st['pass']) and _st['first_diff'] == _st['expected_first_diff']
res['checker_selftest_detail'] = _st

# fail-closed controls
hit = None
for s, o in objects():
    for label, pos, kind, a in g._arrays_of(o):
        if a['items'] and kind != 'loose' and refcount[s['index']][a['at']] > 1:
            hit = (a, label)
            break
    if hit:
        break
if hit:
    mu, re = flip_item(hit[0], 0, 0)
    run_fail_closed('edit ONE sharer of a de-duplicated block (%s)' % hit[1], mu, re, g.ObjectsConflict)
# class index -> class of a different size
for s in secs:
    deps = s['type_deps']
    items = s['frozen']['array_data']['shaders']['items']
    sizes = {i + 1: d['size'] for i, d in enumerate(deps)}
    cur = items[0]['tidx']
    other = [i for i, sz in sizes.items() if sz != sizes[cur]]
    if other:
        oldt = cur
        run_fail_closed('shader pointer class index -> dependency of different size',
                        lambda: items[0].__setitem__('tidx', other[0]), lambda: items[0].__setitem__('tidx', oldt),
                        ValueError)
        break
# vf_type_index set while the section's vf table is empty
mu = lambda: o0.__setitem__('vf_type_index', 0)  # noqa: E731
re_ = lambda: o0.__setitem__('vf_type_index', None)  # noqa: E731
run_fail_closed('vf_type_index=0 with empty vf table', mu, re_, ValueError)
# object moved so no shader pointer references it
oldat = o0['at']
run_fail_closed('object moved away from its shader pointer', lambda: o0.__setitem__('at', oldat + 8),
                lambda: o0.__setitem__('at', oldat), ValueError)

# final sanity: model unchanged after all controls -> identical again
res['post_controls_byte_identical'] = g.encode(model) == data
fc, fl = res['field_controls'], res['fail_closed_controls']
res['counts'] = {'field_controls': len(fc), 'field_pass': sum(1 for c in fc if c['pass']),
                 'fail_closed_controls': len(fl), 'fail_closed_pass': sum(1 for c in fl if c['pass']),
                 'field_controls_all_diffs_inside_field': sum(1 for c in fc if c.get('all_diffs_inside_allowed_spans')),
                 'max_n_diff_bytes': max((c.get('n_diff_bytes') or 0) for c in fc) if fc else 0,
                 'checker_selftest_spill_detected': res['checker_selftest_spill_detected']}
json.dump(res, open(sys.argv[2], 'w'), indent=1)
print(json.dumps(res['counts']), 'post_controls_byte_identical=%s' % res['post_controls_byte_identical'])
for c in fc + fl:
    if not c['pass']:
        print('FAIL', c)
total = len(fc) + len(fl)
if total == 0:
    print('FAIL: zero controls checked')
    sys.exit(1)
ok = all(c['pass'] for c in fc + fl) and res['post_controls_byte_identical'] and res['checker_selftest_spill_detected']
print('checked %d controls: %s' % (total, 'PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
