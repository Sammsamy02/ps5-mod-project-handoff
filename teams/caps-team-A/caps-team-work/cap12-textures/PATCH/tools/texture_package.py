#!/usr/bin/env python3
"""Cooked UE5 texture package: where every mip's texel bytes are, and a
DERIVED copy with exactly those bytes changed (Mac-only, generic, fail closed).

Layout (public UE5 format facts; read from the data and cross-checked, two
sources for every byte range):
  source 1  the zen header's bulk-data map (between the name batch and the
            imported public export hashes): per entry SerialOffset i64,
            DuplicateSerialOffset i64, SerialSize i64, Flags u32, pad u32.
            Streamed mips: Flags has PayloadInSeparateFile (0x100), SerialOffset
            is inside the package's BulkData chunk. Inline mips: Flags has
            ForceInlinePayload (0x40), SerialOffset is inside the export.
  source 2  the export's own platform-data mip table: FName pixel format +
            i64 skip offset, SizeX, SizeY, PackedData, FString pixel format,
            FirstMip, NumMips, then per mip: u32 bulk-map index, [inline
            payload], u32 x, u32 y, u32 z; then u32 bIsVirtual (must be 0) and
            the FName None terminator, which must end the export exactly.
  both must agree, sizes must equal the block arithmetic of the pixel format
  for (x, y), mips must halve, streamed payloads must tile the bulk chunk
  exactly, inline payloads must sit right after their index field. Anything
  else (virtual texture, compressed or optional/memory-mapped bulk data,
  duplicate payloads, several exports, opt data, volume/array textures) is
  REFUSED.

derive(...) applies a block transform (bcn_swap.swap_rb) to every payload
range and returns the new export chunk and bulk chunk. Header bytes, the bulk
map, the mip table and every non-payload byte stay identical (re-checked).

PS5 FORM (round 5b, Team B finding: the game's PS5 textures are GPU-tiled):
  layout(..., allow_ps5=True) also reads the game's PS5 form: PackedData bit 30
  (HasOptData) + an 8-byte block {ExtData, NumMipsInTail} after the pixel-format
  string; mips before the tail have the tiled (padded) size of ps5_tiling's public
  rule, the first tail mip is ONE inline tail block, later tail mips are empty.
  derive_ps5(...) rebuilds a linear (PC-cooked) texture package in that form:
  transform per mip (e.g. the R<->B swap), tile (ps5_tiling, both gates), set
  bit 30, insert the block, re-lay the mip table, bulk map, bulk chunk, export
  size and skip offset. Everything else is copied. Re-checked: the result reads
  back with layout(allow_ps5=True) (two sources), de-tiles to exactly the
  transformed linear mips, and its header differs only in the bulk map entries
  and the export serial size.
"""
import os
import struct
import sys

try:
    zen = ZEN  # noqa: F821 -- injected by a loader that execs verified bytes (paktx.load_readers)
except NameError:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import zen  # noqa: E402
try:
    ps5_tiling = PS5_TILING  # noqa: F821 -- injected the same way
except NameError:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import ps5_tiling  # noqa: E402

BLOCK = {'PF_DXT1': (4, 8), 'PF_DXT3': (4, 16), 'PF_DXT5': (4, 16), 'PF_BC4': (4, 8), 'PF_BC5': (4, 16),
         'PF_BC7': (4, 16), 'PF_B8G8R8A8': (1, 4), 'PF_R8G8B8A8': (1, 4)}
F_SEPARATE = 0x100
F_INLINE = 0x40
F_REFUSED = 0x2 | 0x200 | 0x800 | 0x1000 | 0x4000 | 0x8000 | 0x20000
ENTRY = 32
PACKAGE_TAG = bytes.fromhex('c1832a9e')  # PACKAGE_FILE_TAG, ends every cooked package chunk


class LayoutError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise LayoutError(why)


def mip_size(fmt, x, y):
    b, n = BLOCK[fmt]
    return ((x + b - 1) // b) * ((y + b - 1) // b) * n


def bulk_map(data):
    """The header's bulk-data map: [(offset, dup, size, flags, pad)]."""
    names, pos = zen.read_name_batch(data, 60)
    iph = struct.unpack_from('<i', data, 24)[0]
    need(pos + 8 <= iph, 'no bulk-map padding field')
    pad = struct.unpack_from('<Q', data, pos)[0]
    need(pad < 8 and not any(data[pos + 8:pos + 8 + pad]), 'unexpected bulk-map padding')
    bpos = pos + 8 + pad
    size = struct.unpack_from('<q', data, bpos)[0]
    need(size >= 0 and size % ENTRY == 0 and bpos + 8 + size == iph, 'bulk map does not end at the import hashes')
    return [struct.unpack_from('<qqqII', data, bpos + 8 + ENTRY * i) for i in range(size // ENTRY)], bpos


def layout(chunk, bulk_len=None, allow_ps5=False):
    try:
        return _layout(chunk, bulk_len, allow_ps5)
    except struct.error as e:
        raise LayoutError(f'truncated texture data: {e}')


def _layout(chunk, bulk_len, allow_ps5):
    """chunk: the package's export chunk (header + export data).
    bulk_len: length of its BulkData chunk (None = no bulk chunk).
    -> dict with format, size, mips [{x, y, where: 'inline'|'bulk', offset, size}]
       (inline offset = absolute offset in the chunk)."""
    p = zen.ZenPackage(chunk)
    need(len(p.exports) == 1, f'{len(p.exports)} exports (need exactly 1)')
    e = p.exports[0]
    need(e['outer'][0] == 'Null', 'export has an outer')
    exp = p.export_bytes(e)
    need(exp is not None and len(exp) == e['serial_size'], 'export bytes missing')
    eo = e['data_offset']
    need(chunk[eo + len(exp):] in (b'', PACKAGE_TAG), 'bytes after the export other than the package tag')
    entries, _bpos = bulk_map(chunk)
    need(exp.count(b'PF_') == 1, 'not exactly one pixel-format string in the export')
    s = exp.find(b'PF_')
    need(s >= 48, 'pixel-format string too early in the export')
    ln = struct.unpack_from('<i', exp, s - 4)[0]
    need(0 < ln < 32 and exp[s + ln - 1] == 0, 'pixel-format string length')
    fmt = exp[s:s + ln - 1].decode('ascii')
    need(fmt in BLOCK, f'pixel format {fmt} not handled')
    sx, sy, packed = struct.unpack_from('<iiI', exp, s - 16)
    ps5 = None
    if allow_ps5 and packed == 0x40000001:
        ext, ntail = struct.unpack_from('<II', exp, s + ln)
        ps5 = {'ext_data': ext, 'num_mips_in_tail': ntail}
    else:
        need(packed == 1, f'PackedData {packed:#x} (opt data, cube, array or volume: unsupported)')
    fni, fnn, skip = struct.unpack_from('<IIq', exp, s - 48)
    need(fnn == 0 and fni < len(p.names) and p.names[fni] == fmt,
         'pixel-format FName does not name the same format (second source)')
    need(not any(exp[s - 32:s - 16]), 'derived-data placeholder (16 bytes) not zero')
    q = s + ln + (8 if ps5 else 0)
    first, nmips = struct.unpack_from('<ii', exp, q)
    q += 8
    need(first == 0 and 1 <= nmips <= 16, f'mip header first={first} count={nmips}')
    want = None
    if ps5:
        eb = BLOCK[fmt][1]
        need(BLOCK[fmt][0] == 4, f'{fmt}: PS5 form only read for block formats')
        try:
            chain = ps5_tiling.chain_layout(ps5['ext_data'], eb, -(-sx // 4), -(-sy // 4), nmips)
        except (ps5_tiling.TilingRefused, KeyError) as e:
            raise LayoutError(f'PS5 layout ExtData {ps5["ext_data"]}: {e}')
        ft = chain['first_tail']
        need((0 if ft is None else nmips - ft) == ps5['num_mips_in_tail'],
             'NumMipsInTail disagrees with the public rule for this size (sources disagree)')
        want = [m['size'] for m in chain['mips']] + ([chain['block']] + [0] * (nmips - ft - 1) if ft is not None else [])
        ps5['first_tail'] = ft
        ps5['mode_name'] = ps5_tiling.MODE_NAME.get(ps5['ext_data'])
    mips = []
    used = set()
    for k in range(nmips):
        need(q + 4 <= len(exp), 'mip table truncated')
        bi = struct.unpack_from('<I', exp, q)[0]
        q += 4
        need(bi < len(entries) and bi not in used, f'mip {k}: bulk-map index {bi} invalid or reused')
        used.add(bi)
        off, dup, size, flags, _pad = entries[bi]
        need(dup == -1, f'mip {k}: duplicate payload offset set')
        need(not flags & F_REFUSED, f'mip {k}: bulk flags {flags:#x} (compressed/optional/mapped: unsupported)')
        if flags & F_SEPARATE:
            need(not flags & F_INLINE, f'mip {k}: inline and separate flags both set')
            where = 'bulk'
        else:
            need(flags & F_INLINE, f'mip {k}: neither inline nor separate ({flags:#x})')
            need(off == q, f'mip {k}: inline offset {off} != position after its index {q} (sources disagree)')
            where = 'inline'
            q += size
        need(q + 12 <= len(exp), 'mip table truncated')
        x, y, z = struct.unpack_from('<III', exp, q)
        q += 12
        need(z == 1, f'mip {k}: depth {z}')
        if mips:
            need(x == max(1, mips[-1]['x'] // 2) and y == max(1, mips[-1]['y'] // 2), 'mips do not halve')
        else:
            need((x, y) == (sx, sy), 'mip 0 size != platform-data size')
        exp_size = want[k] if want is not None else mip_size(fmt, x, y)
        need(size == exp_size, f'mip {k}: {size} bytes, {fmt} {x}x{y} needs {exp_size}'
             + (' (PS5 tiled form)' if want is not None else ''))
        mips.append({'x': x, 'y': y, 'where': where, 'offset': (eo + off) if where == 'inline' else off,
                     'size': size, 'bulk_index': bi})
    need(len(used) == len(entries), 'bulk-map entries not used by any mip')
    virt = struct.unpack_from('<I', exp, q)[0]
    need(virt == 0, 'virtual texture')
    q += 4
    need(q == (s - 40) + skip, 'skip offset does not land after the platform data')
    nonei, nonen = struct.unpack_from('<II', exp, q)
    need(nonei < len(p.names) and p.names[nonei] == 'None' and nonen == 0, 'no FName None terminator')
    q += 8
    need(q == len(exp), f'{len(exp) - q} bytes after the platform data')
    bulk = sorted((m['offset'], m['size']) for m in mips if m['where'] == 'bulk')
    if bulk:
        need(bulk_len is not None, 'streamed mips but no bulk chunk')
        pos = 0
        for off, size in bulk:
            need(off == pos, 'streamed mips do not tile the bulk chunk')
            pos += size
        need(pos == bulk_len, f'bulk chunk is {bulk_len} bytes, streamed mips need {pos}')
    else:
        need(bulk_len is None, 'bulk chunk present but no streamed mip')
    return {'format': fmt, 'size': [sx, sy], 'mips': mips, 'export_offset': eo, 'export_size': len(exp),
            'package': p.name, 'header_size': p.header_size, 'ps5': ps5, 'pf_offset': s, 'pf_len': ln,
            'mip_table_end': q - 12,
            'sources': ['header bulk-data map', 'export mip table', 'pixel-format FName == FString',
                        f'{fmt} block arithmetic per mip']}


def payload(lay, chunk, bulk):
    """All texel bytes, mip order (inline from the chunk, streamed from the bulk chunk)."""
    out = []
    for m in lay['mips']:
        src = chunk if m['where'] == 'inline' else bulk
        out.append(src[m['offset']:m['offset'] + m['size']])
    return out


def derive(chunk, bulk, transform):
    """transform(fmt, bytes) -> bytes of the same length. -> (new chunk, new bulk, layout, per-mip facts)."""
    lay = layout(chunk, None if bulk is None else len(bulk))
    fmt = lay['format']
    nc = bytearray(chunk)
    nb = bytearray(bulk) if bulk is not None else None
    facts = []
    for m, data in zip(lay['mips'], payload(lay, chunk, bulk)):
        new, f = transform(fmt, data)
        need(len(new) == len(data), 'transform changed a payload size')
        dst = nc if m['where'] == 'inline' else nb
        dst[m['offset']:m['offset'] + m['size']] = new
        facts.append(dict(f, mip=[m['x'], m['y']], where=m['where']))
    nc, nb = bytes(nc), (bytes(nb) if nb is not None else None)
    # re-check: same layout from the derived bytes, and nothing outside the payloads changed
    lay2 = layout(nc, None if nb is None else len(nb))
    need(lay2 == lay, 'derived layout differs')
    inl = sorted((m['offset'], m['offset'] + m['size']) for m in lay['mips'] if m['where'] == 'inline')
    pos = 0
    for a, b in inl + [(len(chunk), len(chunk))]:
        need(nc[pos:a] == chunk[pos:a], 'derived chunk changed outside the texel payloads')
        pos = b
    need(len(nc) == len(chunk), 'derived chunk length changed')
    return nc, nb, lay, facts


def _flags(entries, separate):
    got = {e[3] for e in entries if bool(e[3] & F_SEPARATE) == separate}
    need(len(got) == 1, f'{"separate" if separate else "inline"} bulk entries carry {len(got)} flag values')
    return got.pop()


def derive_ps5(chunk, bulk, transform, choose):
    """Linear (PC) texture package -> the same package in the game's PS5 form.
    transform(fmt, linear mip bytes) -> (bytes, facts); choose(fmt, w, h, nmips) -> ps5_tiling
    choice (raises TilingRefused). -> (new chunk, new bulk, layout, facts, choice, linear mips)."""
    lay = layout(chunk, None if bulk is None else len(bulk))
    need(lay['ps5'] is None, 'already in PS5 form')
    fmt, (w, h) = lay['format'], lay['size']
    n = len(lay['mips'])
    choice = choose(fmt, w, h, n)
    lin, facts = [], []
    for m, data in zip(lay['mips'], payload(lay, chunk, bulk)):
        new, f = transform(fmt, data)
        need(len(new) == len(data), 'transform changed a payload size')
        lin.append(new)
        facts.append(dict(f, mip=[m['x'], m['y']]))
    tiled = ps5_tiling.tile(fmt, w, h, lin, choice)
    ft = choice['chain']['first_tail']
    p = zen.ZenPackage(chunk)
    eo = lay['export_offset']
    exp = chunk[eo:eo + lay['export_size']]
    entries, bpos = bulk_map(chunk)
    fsep, finl = _flags(entries, True), _flags(entries, False)
    s, ln = lay['pf_offset'], lay['pf_len']
    body = bytearray(exp[:s + ln])
    struct.pack_into('<I', body, s - 8, struct.unpack_from('<I', body, s - 8)[0] | (1 << 30))
    body += struct.pack('<II', choice['ext_data'], choice['num_mips_in_tail'])
    body += struct.pack('<ii', 0, n)
    new_entries, nb = [], bytearray()
    for k, m in enumerate(lay['mips']):
        body += struct.pack('<I', k)
        data = tiled[k]
        if ft is None or k < ft:
            new_entries.append((len(nb), -1, len(data), fsep, 0))
            nb += data
        else:
            new_entries.append((len(body), -1, len(data), finl, 0))
            body += data
        body += struct.pack('<III', m['x'], m['y'], 1)
    rest = exp[lay['mip_table_end']:]
    need(len(rest) == 12, 'unexpected bytes after the mip table')
    body += rest
    struct.pack_into('<q', body, s - 40, (len(body) - 8) - (s - 40))
    head = bytearray(chunk[:eo])
    need(len(new_entries) == len(entries), 'bulk map entry count would change')
    for k, ent in enumerate(new_entries):
        struct.pack_into('<qqqII', head, bpos + 8 + ENTRY * k, *ent)
    xoff = struct.unpack_from('<i', head, 32)[0]          # ExportMapOffset (summary field 3)
    need(struct.unpack_from('<Q', head, xoff + 8)[0] == lay['export_size'], 'export map serial size not found')
    struct.pack_into('<Q', head, xoff + 8, len(body))
    tag = chunk[eo + lay['export_size']:]
    nc, nbb = bytes(head) + bytes(body) + tag, (bytes(nb) if nb else None)
    # re-checks: reads back in PS5 form (two sources), same identity, round trip, header diff
    lay2 = layout(nc, None if nbb is None else len(nbb), allow_ps5=True)
    need(lay2['ps5'] == {'ext_data': choice['ext_data'], 'num_mips_in_tail': choice['num_mips_in_tail'],
                         'first_tail': ft, 'mode_name': choice['mode_name']}, 'derived PS5 header fields differ')
    need((lay2['format'], lay2['size'], lay2['package']) == (fmt, [w, h], lay['package']), 'identity changed')
    back = ps5_tiling.detile(fmt, w, h, payload(lay2, nc, nbb), choice)
    need(back == lin, 'derived texture does not de-tile to the transformed linear mips')
    q = zen.ZenPackage(nc)
    need(q.name == p.name and len(q.exports) == 1 and q.imports == p.imports and q.names == p.names,
         'package identity / imports / names changed')
    hs = p.header_size
    need(q.header_size == hs, 'header size changed')
    changed = [i for i in range(hs) if nc[i] != chunk[i]]
    allowed = set(range(bpos + 8, bpos + 8 + ENTRY * len(entries))) | set(range(xoff + 8, xoff + 16))
    need(set(changed) <= allowed, 'derived header changed outside the bulk map and the export size')
    return nc, nbb, lay2, facts, choice, lin
