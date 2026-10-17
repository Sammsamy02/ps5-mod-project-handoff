#!/usr/bin/env python3
"""Synthetic UE5.6 IoStore/Zen writer for host tests (test-only, never shipped).

Writes minimal but structurally complete containers: TOC v8 + stored UCAS,
container header v5, Zen packages (container-header-v5 summary layout),
a ScriptObjects global container, optional directory index.  Layouts follow
the same public format facts the reader uses (retoc / CUE4Parse)."""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "PATCH", "pak-intake"))
import ue_formats as uf  # noqa: E402

M64 = (1 << 64) - 1
NULL = M64


def name_batch(names):
    out = struct.pack("<I", len(names))
    if not names:
        return out
    enc = [n.encode("latin-1") for n in names]
    out += struct.pack("<IQ", sum(len(e) for e in enc), uf.FNAME_HASH_ALGORITHM_ID)
    out += b"".join(struct.pack("<Q", uf.cityhash64(n.lower().encode())) for n in names)
    out += b"".join(struct.pack(">h", len(e)) for e in enc)
    out += b"".join(enc)
    return out


def script_index(path):
    """Synthetic ScriptImport index for a /Script path (test-only numbering)."""
    return (1 << 62) | (uf.cityhash64(path.encode()) & ((1 << 62) - 1))


def package_import(pkg_i, hash_i):
    return (2 << 62) | (pkg_i << 32) | hash_i


def export_index(i):
    return i


def public_hash(object_path):
    return uf.cityhash64(object_path.lower().encode()) or 1


class Pkg:
    """Description of one synthetic package."""

    def __init__(self, name, exports, imports=(), imported_packages=(), bulk_sizes=(), shader_hashes=0, extra_names=()):
        self.name = name
        # exports: list of dict(name, cls(raw), payload(bytes), public(bool), template(raw)?)
        self.exports = exports
        # imports: list of raw FPackageObjectIndex values; package imports use package_import()
        self.imports = list(imports)
        # imported_packages: list of (package name, [public export hashes...]) referenced by package_import
        self.imported_packages = list(imported_packages)
        self.bulk_sizes = list(bulk_sizes)
        self.shader_hashes = shader_hashes
        self.extra_names = list(extra_names)

    @property
    def pid(self):
        return uf.package_id_of(self.name)


def build_zen(p: Pkg) -> bytes:
    short = p.name.rsplit("/", 1)[-1]
    names = [p.name] + sorted({e["name"] for e in p.exports} | {short} | set(p.extra_names))
    idx = {n: i for i, n in enumerate(names)}
    nb = name_batch(names)
    bulk = struct.pack("<Q", 0) + struct.pack("<q", 32 * len(p.bulk_sizes))
    off = 0
    for sz in p.bulk_sizes:
        bulk += struct.pack("<qqqIB3s", off, -1, sz, 0x10501, 0, b"\0\0\0")
        off += sz
    hashes = []
    for _, hs in p.imported_packages:
        hashes.extend(hs)
    iph_b = b"".join(struct.pack("<Q", h) for h in hashes)
    imp_b = b"".join(struct.pack("<Q", r) for r in p.imports)
    n = len(p.exports)
    exp_b = b""
    for e in p.exports:
        pub = public_hash(p.name + "." + e["name"]) if e.get("public", True) else 0
        exp_b += struct.pack("<QQIIQQQQQIB3s", 0, len(e["payload"]), idx[e["name"]], 0, NULL, e["cls"],
                             NULL, e.get("template", NULL), pub, 1, 0, b"\0\0\0")
    bundle_b = b"".join(struct.pack("<II", i, 0) for i in range(n)) + b"".join(struct.pack("<II", i, 1) for i in range(n))
    deph_b = b"".join(struct.pack("<iIIII", -1, 0, 0, 0, 0) for _ in range(n))
    depe_b = b""
    ipn_names = [nm for nm, _ in p.imported_packages]
    ipn_b = name_batch(ipn_names) + b"".join(struct.pack("<i", 0) for _ in ipn_names)
    pos = 60 + len(nb) + len(bulk)
    iph = pos
    imp = iph + len(iph_b)
    exp = imp + len(imp_b)
    cimp = exp + len(exp_b)
    cexp = cimp
    bundle = cexp
    deph = bundle + len(bundle_b)
    depe = deph + len(deph_b)
    ipn = depe + len(depe_b)
    hsize = ipn + len(ipn_b)
    summary = struct.pack("<6I", 0, hsize, idx[p.name], 0, 0, hsize) + \
        struct.pack("<7i", iph, imp, exp, bundle, deph, depe, ipn) + struct.pack("<2i", cimp, cexp)
    assert len(summary) == 60
    head = summary + nb + bulk + iph_b + imp_b + exp_b + bundle_b + deph_b + depe_b + ipn_b
    assert len(head) == hsize
    return head + b"".join(e["payload"] for e in p.exports)


def container_header(container_id, store):
    """store: list of (pid, [imported pids], shader_hash_count)."""
    ids = [pid for pid, _, _ in store]
    entries = b""
    tail = b""
    base_tail = 16 * len(store)
    for i, (pid, imps, nsh) in enumerate(store):
        field = 16 * i
        imp_rel = (base_tail + len(tail)) - field if imps else 0
        tail += b"".join(struct.pack("<Q", x) for x in imps)
        sh_rel = (base_tail + len(tail)) - (field + 8) if nsh else 0
        tail += b"".join(bytes([0x40 + k]) * 20 for k in range(nsh))
        entries += struct.pack("<IIII", len(imps), imp_rel, nsh, sh_rel)
    buf = entries + tail
    out = struct.pack("<IIQ", uf.CONTAINER_HEADER_SIGNATURE, 5, container_id)
    out += struct.pack("<i", len(ids)) + b"".join(struct.pack("<Q", x) for x in ids)
    out += struct.pack("<i", len(buf)) + buf
    out += struct.pack("<i", 0) + struct.pack("<i", 0)  # optional segment: none
    out += name_batch([]) + struct.pack("<i", 0) + struct.pack("<i", 0) + struct.pack("<qq", 0, 0)
    return out


def chunk_id(obj_id, index, ctype):
    return struct.pack("<QHBB", obj_id, index, 0, ctype)


def dir_index(mount, files):
    """files: list of (toc row, relative path). One root dir, full paths as file names."""
    def fstr(s):
        b = s.encode("latin-1") + b"\0"
        return struct.pack("<i", len(b)) + b
    strings = [p for _, p in files]
    out = fstr(mount)
    out += struct.pack("<I", 1) + struct.pack("<4I", 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 if files else 0xFFFFFFFF)
    out += struct.pack("<I", len(files))
    for i, (row, _) in enumerate(files):
        nxt = i + 1 if i + 1 < len(files) else 0xFFFFFFFF
        out += struct.pack("<3I", i, nxt, row)
    out += struct.pack("<I", len(strings)) + b"".join(fstr(s) for s in strings)
    return out


def write_container(path_stem, container_id, chunks, block_size=65536, mount="../../../",
                    version=8, flags=8, compress_names=0):
    """chunks: list of (chunk_id bytes, data bytes, virtual path or None). Writes .utoc/.ucas."""
    ucas = bytearray()
    blocks = []
    offs = []
    logical = 0
    for cid, data, _ in chunks:
        start = logical
        offs.append((start, len(data)))
        p = 0
        while p < len(data):
            part = data[p:p + block_size]
            blocks.append((len(ucas), len(part), len(part), 0))
            ucas += part
            p += len(part)
        logical = ((start + len(data) + block_size - 1) // block_size) * block_size
    files = [(i, vp) for i, (_, _, vp) in enumerate(chunks) if vp]
    di = dir_index(mount, files) if (flags & 8) else b""
    n = len(chunks)
    hdr = bytearray(144)
    hdr[0:16] = uf.TOC_MAGIC
    hdr[16] = version
    struct.pack_into("<9I", hdr, 20, 144, n, len(blocks), 12, compress_names, 32, block_size, len(di), 1)
    struct.pack_into("<Q", hdr, 56, container_id)
    hdr[80] = flags
    struct.pack_into("<I", hdr, 84, 0)
    struct.pack_into("<Q", hdr, 88, M64)
    struct.pack_into("<I", hdr, 96, 0)
    body = bytes(hdr)
    body += b"".join(cid for cid, _, _ in chunks)
    body += b"".join(o.to_bytes(5, "big") + ln.to_bytes(5, "big") for o, ln in offs)
    for boff, cs, us, m in blocks:
        body += boff.to_bytes(5, "little") + cs.to_bytes(3, "little") + us.to_bytes(3, "little") + bytes([m])
    body += b"\0" * (32 * compress_names)
    body += di
    for _, data, _ in chunks:
        body += uf.io_hash(data) + b"\0\0\0\0"
    with open(path_stem + ".utoc", "wb") as f:
        f.write(body)
    with open(path_stem + ".ucas", "wb") as f:
        f.write(bytes(ucas))
    return path_stem + ".utoc", path_stem + ".ucas"


def write_package_container(path_stem, container_id, pkgs, extra_chunks=(), store_overrides=None,
                            mount="../../../", block_size=65536, header_chunk=True):
    chunks = []
    store = []
    for p in pkgs:
        data = build_zen(p)
        vp = "Game/Content/" + p.name.split("/Game/", 1)[-1] + ".uasset" if p.name.startswith("/Game/") else None
        chunks.append((chunk_id(p.pid, 0, 1), data, vp))
        imps = [uf.package_id_of(nm) for nm, _ in p.imported_packages]
        store.append((p.pid, imps, p.shader_hashes))
    if store_overrides:
        store = store_overrides(store)
    chunks.extend(extra_chunks)
    if header_chunk:
        chunks.append((chunk_id(container_id, 0, 6), container_header(container_id, store), None))
    return write_container(path_stem, container_id, chunks, block_size=block_size, mount=mount)


def write_script_objects(path_stem, paths):
    """paths: list of '/Script/Module.Class' (module objects added automatically)."""
    names = []
    entries = []
    seen = {}

    def add(name, outer, idx):
        if name not in names:
            names.append(name)
        entries.append((names.index(name), idx, outer))

    for p in paths:
        mod, _, cls = p.partition(".")
        if mod not in seen:
            seen[mod] = script_index(mod)
            add(mod, NULL, seen[mod])
        if cls:
            add(cls, seen[mod], script_index(p))
    data = name_batch(names) + struct.pack("<i", len(entries))
    for ni, g, outer in entries:
        data += struct.pack("<IIQQQ", ni, 0, g, outer, NULL)
    return write_container(path_stem, M64, [(chunk_id(0, 0, 5), data, None)], flags=0)


def texture_payload(pf, sx, sy, packed=1, opt=None, nmips=1):
    body = b"\x00" * 16
    body += struct.pack("<iiI", sx, sy, packed)
    s = pf.encode() + b"\0"
    body += struct.pack("<i", len(s)) + s
    if opt is not None:
        body += struct.pack("<II", *opt)
    body += struct.pack("<ii", 0, nmips) + b"\x00" * 8
    return body
