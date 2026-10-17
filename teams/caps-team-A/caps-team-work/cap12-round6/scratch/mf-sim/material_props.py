#!/usr/bin/env python3
"""Reader C: blend mode and shading model read from a cooked Material's OWN
property block (Mac-only, read-only, generic UE5; no game, mod or asset names,
no hard-coded property indices or enum values).

A zen-cooked UObject export starts with an unversioned property block:
  fragments u16 (bits 0-6 skip, bit 7 has-zeroes, bit 8 is-last, bits 9-15 value
  count), then a zero mask when any fragment has zeroes (bits = values in such
  fragments; u8 if <= 8 bits, u16 if <= 16, else u32 words), then the values of
  the present, non-zero properties in schema order. (Public serializer format,
  the same for every UE5 class; the class schema itself is NOT used.)

Which schema index holds which property, and what each value means, is LEARNED
from the game's own cooked Materials against their asset-registry tags
(BlendMode, ShadingModel, ShadingModels). A field is learned only if exactly one
schema index explains EVERY training material:
  presence   index present  <=>  registry value != one single default label
  value      enum fields: the first byte, a 1:1 map byte <-> registry label
             (absent or zero-masked = the default label);
             the shading-model bit field: a nested unversioned struct with one
             value (u16 header 0x0300) whose u16 equals the registry
             'ShadingModelField' number (absent = the default number).
Values are walked in order; a present property before a learned one whose size
is not learned makes that material unreadable (fail closed). Any training
contradiction -> the field is not learned -> nothing is readable.

Leave-one-out: for a base candidate, the decoder is learned WITHOUT that
material, so its own registry tags never feed its own reader-C value; reader C
and the registry are then two sources (two different files of the game).
For a mod material (no registry) reader C gives the mod's own settings; the enum
ShadingModel and the ShadingModels bit field are two separately learned fields
and must agree, and reader C's blend group must agree with the existing mod
readers A/B (shader types / base-pass buffer), which also proves the learned
schema applies to that mod's cook.
"""
import struct

BLEND, MODEL, FIELD = 'BlendMode', 'ShadingModel', 'ShadingModels'
MIN_TRAIN = 20          # training materials needed before any field is learned
MIN_VALUES = 2          # distinct non-default registry labels seen for an enum field
RAW_SIZES = (1, 2, 4, 8)  # sizes tried for other properties that precede a learned field
MIN_RAW_ROWS = 3        # training rows that must really test a learned raw size


class PropsError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise PropsError(why)


def parse_header(b):
    """-> (entries [(schema index, is_zero)], offset of the first value)."""
    need(b is not None and len(b) >= 2, 'no export bytes')
    p, idx, entries, frags = 0, 0, [], []
    while True:
        need(p + 2 <= len(b), 'property header truncated')
        v = struct.unpack_from('<H', b, p)[0]
        p += 2
        skip, hz, last, num = v & 0x7F, (v >> 7) & 1, (v >> 8) & 1, v >> 9
        idx += skip
        frags.append((idx, num, hz))
        idx += num
        need(len(frags) <= 512 and idx <= 4096, 'implausible property header')
        if last:
            break
    nz = sum(num for _i, num, hz in frags if hz)
    mask = []
    if nz:
        if nz <= 8:
            need(p + 1 <= len(b), 'zero mask truncated')
            word, nbytes = b[p], 1
        elif nz <= 16:
            need(p + 2 <= len(b), 'zero mask truncated')
            word, nbytes = struct.unpack_from('<H', b, p)[0], 2
        else:
            nbytes = 4 * ((nz + 31) // 32)
            need(p + nbytes <= len(b), 'zero mask truncated')
            word = int.from_bytes(b[p:p + nbytes], 'little')
        mask = [(word >> k) & 1 for k in range(nz)]
        p += nbytes
    k = 0
    for start, num, hz in frags:
        for j in range(num):
            z = False
            if hz:
                z = bool(mask[k])
                k += 1
            entries.append((start + j, z))
    return entries, p


def _walk(b, sizes, wanted):
    """Values of the wanted indices, walking present values whose sizes are known.
    sizes: {index: 'byte' | 'struct16' | n (raw bytes, value not used)}. -> {index: value or 'absent' or 'zero'} or raise."""
    entries, p = parse_header(b)
    out = {i: 'absent' for i in wanted}
    last = max(wanted)
    for i, z in entries:
        if i > last:
            break
        if z:
            if i in wanted:
                out[i] = 'zero'
            continue
        kind = sizes.get(i)
        need(kind is not None, f'property {i} before a learned field has no learned size')
        if kind == 'byte':
            need(p + 1 <= len(b), 'value truncated')
            v, p = b[p], p + 1
        elif isinstance(kind, int):
            need(p + kind <= len(b), 'value truncated')
            v, p = None, p + kind
        else:
            need(p + 4 <= len(b), 'value truncated')
            h, v = struct.unpack_from('<HH', b, p)
            need(h == 0x0300, 'nested struct header is not one-value/last')
            p += 4
        if i in wanted:
            out[i] = v
    return out


def _field_number(tags):
    v = (tags.get(FIELD) or '')
    if '=' not in v:
        return None
    try:
        return int(v.split('=', 1)[1].strip(' )'))
    except ValueError:
        return None


class Decoder:
    """Learned from [(package, export bytes, registry tags)]."""

    def __init__(self, train):
        self.ok, self.why, self.fields = False, None, {}
        rows = []
        for pkg, b, tags in train:
            try:
                entries, _p = parse_header(b)
            except PropsError:
                continue
            labels = {BLEND: tags.get(BLEND), MODEL: tags.get(MODEL), FIELD: _field_number(tags)}
            if None in labels.values():
                continue
            rows.append((pkg, b, {i for i, _z in entries}, labels))
        self.n_train = len(rows)
        if len(rows) < MIN_TRAIN:
            self.why = f'only {len(rows)} training materials (need {MIN_TRAIN})'
            return
        cand = {}
        for f in (BLEND, MODEL, FIELD):
            labs = [r[3][f] for r in rows]
            hits = []
            for i in sorted(set().union(*(r[2] for r in rows))):
                absent = {lab for r, lab in zip(rows, labs) if i not in r[2]}
                present = {lab for r, lab in zip(rows, labs) if i in r[2]}
                if len(absent) == 1 and not (absent & present) and present:
                    hits.append((i, next(iter(absent))))
            if not hits:
                self.why = f'{f}: no schema index explains every training material'
                return
            cand[f] = hits
        good = []
        for fb in cand[BLEND]:
            for fm in cand[MODEL]:
                for ff in cand[FIELD]:
                    if len({fb[0], fm[0], ff[0]}) < 3:
                        continue
                    found = {BLEND: fb, MODEL: fm, FIELD: ff}
                    r = self._check(rows, found)
                    if r is not None:
                        good.append((found, r))
        if len(good) != 1:
            self.why = (f'{len(good)} assignments of schema indices decode every training material '
                        '(need exactly 1)')
            return
        found, (sizes, maps, wanted, used) = good[0]
        sizes, self.raw_learned = self._learn_raw(rows, found, sizes, maps)
        again = self._check(rows, found, sizes)
        if again is None:
            self.why = 'learned sizes of preceding properties break the decode'
            return
        sizes, maps, wanted, used = again
        self.n_used = used
        self.found, self.sizes, self.maps, self.wanted = found, sizes, maps, wanted
        self.ok = True

    @classmethod
    def _learn_raw(cls, rows, found, sizes, maps):
        """Sizes of other properties that precede a learned field: a size is kept
        only if exactly one of RAW_SIZES decodes EVERY row holding that property
        consistently AND at least MIN_RAW_ROWS of those rows really test it (the
        walk passes the property and reaches a present learned field after it).
        'rows' in the result counts only those testing rows."""
        last = max(found[f][0] for f in found)
        learned = {}
        sizes = dict(sizes)
        others = sorted({i for r in rows for i in r[2] if i < last and i not in sizes})
        for u in others:
            holders = [r for r in rows if u in r[2]]
            ok = []
            for n in RAW_SIZES:
                trial = {**sizes, u: n}
                verdicts = [cls._row_ok(r, found, trial, maps, u) for r in holders]
                if False in verdicts:
                    continue
                tested = verdicts.count('tested')
                if tested >= MIN_RAW_ROWS:
                    ok.append((n, tested))
            if len(ok) == 1:
                sizes[u] = ok[0][0]
                learned[u] = {'bytes': ok[0][0], 'rows': ok[0][1]}
        return sizes, learned

    @staticmethod
    def _row_ok(row, found, sizes, maps, u):
        """'tested' (u passed and a present learned field after it decoded right),
        'blocked' (another unknown property: no verdict, or nothing after u to test),
        or False (contradiction)."""
        _pkg, b, pres, labels = row
        wanted = [found[f][0] for f in (BLEND, MODEL, FIELD)]
        try:
            vals = _walk(b, sizes, wanted)
        except PropsError as e:
            return 'blocked' if 'no learned size' in str(e) else False
        for f in (BLEND, MODEL):
            v = vals[found[f][0]]
            if v == 'absent':
                continue
            code = 0 if v == 'zero' else v
            if maps[f].get(code) != labels[f]:
                return False
        v = vals[found[FIELD][0]]
        got = found[FIELD][1] if v == 'absent' else (0 if v == 'zero' else v)
        if got != labels[FIELD]:
            return False
        after = [found[f][0] for f in found if found[f][0] > u and found[f][0] in pres]
        return 'tested' if after else 'blocked'

    @staticmethod
    def _check(rows, found, sizes=None):
        """Value consistency of one index assignment over all decodable rows, or None."""
        sizes = sizes or {found[BLEND][0]: 'byte', found[MODEL][0]: 'byte', found[FIELD][0]: 'struct16'}
        wanted = [found[f][0] for f in (BLEND, MODEL, FIELD)]
        maps = {BLEND: {}, MODEL: {}}
        used = 0
        for pkg, b, _pres, labels in rows:
            try:
                vals = _walk(b, sizes, wanted)
            except PropsError as e:
                if 'no learned size' in str(e):
                    continue                 # other properties before ours: not usable for training
                return None                  # this assignment contradicts the row's bytes
            used += 1
            for f in (BLEND, MODEL):
                i, _default = found[f]
                v = vals[i]
                if v == 'absent':
                    continue                 # presence rule already checked
                code = 0 if v == 'zero' else v
                if maps[f].setdefault(code, labels[f]) != labels[f]:
                    return None
            i, default = found[FIELD]
            v = vals[i]
            got = default if v == 'absent' else (0 if v == 'zero' else v)
            if got != labels[FIELD]:
                return None
        for f in (BLEND, MODEL):
            if len(set(maps[f].values())) != len(maps[f]) or len(maps[f]) < MIN_VALUES:
                return None
        if used < MIN_TRAIN:
            return None
        return sizes, maps, wanted, used

    from_expression = 'MSM_FromMaterialExpression'   # registry label of the per-pixel enum value

    def read(self, b, bits):
        """-> {'readable', blend, models, model_enum, field, why}. bits: shading-model bit -> name."""
        if not self.ok:
            return {'readable': False, 'why': 'reader C not learned: ' + (self.why or '?')}
        try:
            vals = _walk(b, self.sizes, self.wanted)
        except PropsError as e:
            return {'readable': False, 'why': 'reader C: ' + str(e)}
        out = {'readable': False}
        res = {}
        for f in (BLEND, MODEL):
            i, default = self.found[f]
            v = vals[i]
            if v == 'absent':
                res[f] = default
            else:
                code = 0 if v == 'zero' else v
                if code not in self.maps[f]:
                    out['why'] = f'reader C: {f} byte {code} never seen in the game'
                    return out
                res[f] = self.maps[f][code]
        i, default = self.found[FIELD]
        v = vals[i]
        field = default if v == 'absent' else (0 if v == 'zero' else v)
        models = set()
        for k in range(16):
            if field >> k & 1:
                if k not in bits:
                    out['why'] = f'reader C: shading-model bit {k} has no name in the registry'
                    return out
                models.add(bits[k])
        if not models:
            out['why'] = 'reader C: empty shading-model field'
            return out
        enum = res[MODEL]
        # the enum and the bit field are two separately learned fields: they must agree
        if enum == self.from_expression:
            out['field_only'] = True          # per-pixel model: only the bit field names the models (one field)
        elif models != {enum}:
            out['why'] = f'reader C: enum {enum} disagrees with bit field {sorted(models)}'
            return out
        out.update(readable=True, blend=res[BLEND], model_enum=enum, field=field, models=sorted(models))
        return out


def top_export_bytes(z):
    top = [e for e in z.exports if e['outer'][0] == 'Null']
    return z.export_bytes(top[0]) if len(top) == 1 else None


def training_set(whole, registry, material_class):
    """[(package, export bytes, registry tags)] for whole-chunk base Materials."""
    out = []
    for pkg, z in sorted(whole.items()):
        a = registry.asset(pkg)
        if not a or a['class'] != material_class:
            continue
        b = top_export_bytes(z)
        if b:
            out.append((pkg, b, a['tags']))
    return out


class ReaderC:
    """Caches one full decoder plus leave-one-out decoders on demand."""

    def __init__(self, train, bits):
        self.train, self.bits = train, bits
        self.full = Decoder(train)
        self._loo = {}

    def base(self, pkg, blob, exclude):
        """Reader C for a base material, decoder learned without `exclude` packages."""
        key = frozenset(exclude)
        if key not in self._loo:
            self._loo[key] = Decoder([t for t in self.train if t[0] not in key])
        return self._loo[key].read(blob, self.bits)

    def mod(self, blob):
        return self.full.read(blob, self.bits)
