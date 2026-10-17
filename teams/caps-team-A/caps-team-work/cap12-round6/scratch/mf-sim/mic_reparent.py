#!/usr/bin/env python3
"""Derive a re-parented copy of a zen MaterialInstanceConstant package (Mac-only).

Purpose (capability #12 fallback 'reuse the game's own compiled shaders'): a mod
MIC whose parent material has no shader code for the target platform is copied
into a DERIVED package whose parent import points at a base-game material that
does have target shaders. The mod's original file is never modified; output is
new bytes for a derived overlay row.

What changes (and nothing else):
  * the ImportedPublicExportHashes slot used by the parent import
    -> CityHash64(lower(utf16(new export name)))   (verified law: 127/127 Leon exports)
    which MUST equal the hash in the new parent's own fetched header export map
    (--base-header, mandatory: two sources, or nothing is written; exit 1)
  * the ImportedPackageNames entry for the parent package -> new package name
    (name batch rebuilt; it is the last header region, so only HeaderSize moves;
    CookedHeaderSize and all export bytes stay identical)
  * optional parameter renames: entries of the package's own name map (property
    data refers to names by index, so renaming an entry renames that parameter)
The caller must also repoint the package's container-header store entry import
(old package id -> new package id, same slot). Values printed in the receipt.
Fails closed on anything unexpected.
"""
import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cityhash  # noqa: E402
import zen  # noqa: E402


def _rb(p):
    with open(p, 'rb') as fh:
        return fh.read()


def _jl(p):
    with open(p) as fh:
        return json.load(fh)


def _jd(obj, p):
    with open(p, 'w') as fh:
        json.dump(obj, fh, indent=1)


def name_batch(names):
    for n in names:
        zen.need(n.isascii() and 0 < len(n) < 0x8000, f'unsupported name {n!r}')
    out = struct.pack('<I', len(names))
    if not names:
        return out
    out += struct.pack('<IQ', sum(len(n) for n in names), zen.HASH_ALGO)
    out += b''.join(struct.pack('<Q', cityhash.cityhash64(n.lower().encode())) for n in names)
    out += b''.join(struct.pack('>H', len(n)) for n in names)
    out += b''.join(n.encode() for n in names)
    return out


def split_number(s):
    base, _, num = s.rpartition('_')
    if base and num.isdigit() and str(int(num)) == num:
        return base, int(num) + 1
    return s, 0


def rename_names(data, renames, _allow_structural=False):
    """Rename entries of the package's own name map and re-lay-out the header.

    Region [60, ImportedPublicExportHashesOffset) is: name batch, u64 pad count,
    pad bytes (zero), i64 bulk-data-map size, bulk-data-map bytes (retoc zen.rs
    reader order). The pad keeps the bulk map 8-byte aligned; it is recomputed.
    Every later header offset moves by the same delta; export bytes are untouched.
    A name entry is shared by every use in the package (property data refers to
    names by index), so a rename source must not be the package, an export or a
    path name. _allow_structural is for host-test stand-in headers only.
    """
    p = zen.ZenPackage(data)
    offs = list(struct.unpack_from('<9i', data, 24))
    iph = offs[0]
    names, pos = zen.read_name_batch(data, 60)
    zen.need(pos + 8 <= iph, 'no bulk-data padding field')
    pad = struct.unpack_from('<Q', data, pos)[0]
    zen.need(pad < 8 and not any(data[pos + 8:pos + 8 + pad]), 'unexpected padding')
    bpos = pos + 8 + pad
    zen.need((bpos) % 8 == 0, 'original bulk map not 8-aligned')
    bsize = struct.unpack_from('<q', data, bpos)[0]
    zen.need(0 <= bsize and bpos + 8 + bsize == iph, 'unexpected layout before import hashes')
    own = list(names)
    done = []
    structural = {p.name} | {e['name'] for e in p.exports}
    for old, new in renames.items():
        zen.need(old in own, f'rename source {old!r} not in name map')
        zen.need(new not in own, f'rename target {new!r} already present')
        # a name entry is shared by every use in the package; refuse when it is
        # also the package or an export name (renaming would change identity)
        zen.need(_allow_structural or (old not in structural and not old.startswith('/')),
                 f'rename source {old!r} is a package/export/path name, not only a parameter')
        own[own.index(old)] = new
        done.append([old, new])
    nb = name_batch(own)
    npos = 60 + len(nb)
    npad = (-(npos + 8)) % 8
    region = nb + struct.pack('<Q', npad) + bytes(npad) + data[bpos:iph]
    delta = len(region) - (iph - 60)
    zen.need(delta % 8 == 0, 'alignment would change')
    out = bytearray(data[:60]) + region + data[iph:]
    struct.pack_into('<9i', out, 24, *[o + delta if o > 0 else o for o in offs])
    struct.pack_into('<I', out, 4, p.header_size + delta)
    q = zen.ZenPackage(bytes(out))
    for x, y in zip(p.exports, q.exports):
        zen.need(p.export_bytes(x) == q.export_bytes(y), 'export bytes changed')
    zen.need(q.imported_packages == p.imported_packages, 'imports changed')
    return bytes(out), done


def export_hash_from_header(header_bytes, export_name):
    """Second source for the new parent's public export hash: the base package's
    own export map (header bytes fetched from the game's container)."""
    zen.need(len(header_bytes) >= 64, 'base header too short')
    hs = struct.unpack_from('<I', header_bytes, 4)[0]
    zen.need(hs <= len(header_bytes), 'base header prefix shorter than its HeaderSize')
    q = zen.ZenPackage(header_bytes[:hs], header_only=True)
    hits = [e['public_hash'] for e in q.exports if e['name'] == export_name and e['outer'][0] == 'Null']
    zen.need(len(hits) == 1, f'base package has {len(hits)} exports named {export_name!r}')
    return q.name, hits[0]


def reparent(data, old_package, new_package, new_export, renames=None, base_header=None):
    renamed = []
    if renames:
        data, renamed = rename_names(data, renames)
    p = zen.ZenPackage(data)
    zen.need(len(p.exports) >= 1, 'no exports')
    (iph, imp, exp, bundle, dep_h, dep_e, ipn, cimp, cexp) = struct.unpack_from('<9i', data, 24)
    zen.need(old_package in p.imported_packages, f'{old_package} not imported')
    pkg_slot = p.imported_packages.index(old_package)
    zen.need(new_package not in p.imported_packages, 'new parent already imported')
    slots = [v[1] for k, v in p.imports if k == 'PackageImport' and v[0] == pkg_slot]
    zen.need(len(slots) == 1, f'expected exactly one import from {old_package}, got {len(slots)}')
    hash_slot = slots[0]
    zen.need(sum(1 for k, v in p.imports if k == 'PackageImport' and v[1] == hash_slot) == 1,
             'public-hash slot shared by several imports')
    # names region must end exactly at HeaderSize (fail closed otherwise)
    names, pos = zen.read_name_batch(data, ipn)
    zen.need(pos + 4 * len(names) == p.header_size, 'ImportedPackageNames is not the last header region')
    out = bytearray(data[:ipn])
    # 1) public export hash slot
    old_hash = struct.unpack_from('<Q', out, iph + 8 * hash_slot)[0]
    new_hash = cityhash.ue_name_hash(new_export)
    zen.need(base_header is not None,
             'second source missing: pass the new parent package header (--base-header); nothing written')
    bname, bhash = export_hash_from_header(base_header, new_export)
    zen.need(bname == new_package, f'base header is {bname}, not {new_package}')
    zen.need(bhash == new_hash, 'base export hash disagrees with the hash law')
    hash_source = 'cityhash law + base package export map (two sources)'
    struct.pack_into('<Q', out, iph + 8 * hash_slot, new_hash)
    # 3) imported package names
    pk = list(p.imported_packages)
    pk[pkg_slot] = new_package
    pairs = [split_number(n) for n in pk]
    tail = name_batch([b for b, _ in pairs]) + b''.join(struct.pack('<i', k) for _, k in pairs)
    out += tail
    new_header = len(out)
    struct.pack_into('<I', out, 4, new_header)
    out += data[p.header_size:]
    q = zen.ZenPackage(bytes(out))
    for a, b in zip(p.exports, q.exports):
        zen.need(p.export_bytes(a) == q.export_bytes(b), 'export bytes changed')
    zen.need(q.imported_packages[pkg_slot] == new_package, 'repoint failed')
    receipt = {'package': p.name, 'parent_import_slot': pkg_slot, 'public_hash_slot': hash_slot,
               'old_parent': old_package, 'new_parent': new_package, 'new_parent_export': new_export,
               'old_public_hash': f'{old_hash:016x}', 'new_public_hash': f'{new_hash:016x}',
               'old_header_size': p.header_size, 'new_header_size': new_header,
               'store_entry_import_change': {'slot': pkg_slot,
                                             'old_package_id': f'{cityhash.ue_name_hash(old_package):016x}',
                                             'new_package_id': f'{cityhash.ue_name_hash(new_package):016x}'},
               'renamed_parameters': renamed, 'export_bytes_identical': True,
               'new_public_hash_source': hash_source}
    return bytes(out), receipt


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--in', dest='inp', required=True, help='mod MIC package chunk bytes (read-only)')
    ap.add_argument('--old-parent', required=True)
    ap.add_argument('--new-parent', required=True)
    ap.add_argument('--new-export', required=True)
    ap.add_argument('--rename', nargs='*', default=[], help='OLD=NEW parameter renames')
    ap.add_argument('--base-header', help='MANDATORY: fetched header bytes of the new parent package '
                                          '(second source for its export hash); exit 1 without it')
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    if not a.base_header:
        print('mic_reparent: FAIL: --base-header is mandatory (two sources for the new export hash); 0 written')
        return 1
    if os.path.abspath(a.out) == os.path.abspath(a.inp):
        print('mic_reparent: FAIL: refusing to overwrite input; 0 written')
        return 1
    with open(a.inp, 'rb') as fh:
        data = fh.read()
    ren = dict(r.split('=', 1) for r in a.rename)
    try:
        out, rec = reparent(data, a.old_parent, a.new_parent, a.new_export, ren, _rb(a.base_header))
    except zen.ZenError as e:
        print(f'mic_reparent: FAIL: {e}; 0 written')
        return 1
    with open(a.out, 'xb') as fh:
        fh.write(out)
    with open(a.out + '.receipt.json', 'x') as fh:
        json.dump(rec, fh, indent=1)
    print('mic_reparent: 1 package rewritten;', json.dumps(rec))
    return 0


if __name__ == '__main__':
    sys.exit(main())
