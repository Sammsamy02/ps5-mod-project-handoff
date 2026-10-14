"""Read-only IoStore (.utoc/.ucas) + Zen package reader for the texture diagnosis.

Mac-only, stdlib-only. Independent Python implementation of public format
facts (layout cross-checked against the retained MIT retoc sources:
.superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/retoc/retoc/src/{lib.rs,zen.rs,name_map.rs,container_header.rs}).
No third-party or Epic source text is copied.

Scope (fails closed outside it):
  * TOC version 8 only (ReplaceIoChunkHashWithIoHash), unencrypted, unsigned.
  * Chunk payload reads only for compression method index 0 ("None").
  * Zen packages: unversioned UE5 with container header version 5
    (SoftPackageReferencesOffset) layout.
Nothing here writes to its inputs.
"""
from __future__ import annotations

import hashlib
import os
import struct
from dataclasses import dataclass, field

TOC_MAGIC = b"-==--==--==--==-"
CONTAINER_HEADER_MAGIC = 0x496F436E  # "nCoI" little-endian on disk
FNAME_HASH_ALGORITHM_ID = 0xC1640000
PACKAGE_FILE_TAG = struct.pack("<I", 0x9E2A83C1)

CHUNK_TYPES = {
    0: "Invalid", 1: "ExportBundleData", 2: "BulkData", 3: "OptionalBulkData",
    4: "MemoryMappedBulkData", 5: "ScriptObjects", 6: "ContainerHeader",
    7: "ExternalFile", 8: "ShaderCodeLibrary", 9: "ShaderCode",
    10: "PackageStoreEntry", 11: "DerivedData", 12: "EditorDerivedData",
    13: "PackageResource",
}

FLAG_COMPRESSED, FLAG_ENCRYPTED, FLAG_SIGNED, FLAG_INDEXED = 1, 2, 4, 8


class BlockTable:
    """Lazy view over the 12-byte compression block records (keeps big TOCs cheap)."""

    def __init__(self, raw: bytes):
        self.raw = raw

    def __len__(self) -> int:
        return len(self.raw) // 12

    def __getitem__(self, k: int):
        if not 0 <= k < len(self):
            raise FormatError(f"block index {k} out of range")
        e = self.raw[12 * k:12 * k + 12]
        return (int.from_bytes(e[0:5], "little"), int.from_bytes(e[5:8], "little"),
                int.from_bytes(e[8:11], "little"), e[11])


class FormatError(Exception):
    """Raised whenever input is outside the supported, verified profile."""


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


class Reader:
    def __init__(self, data: bytes, pos: int = 0):
        self.d = data
        self.p = pos

    def need(self, n: int) -> None:
        if n < 0 or self.p + n > len(self.d):
            raise FormatError(f"truncated read of {n} at {self.p} (len {len(self.d)})")

    def raw(self, n: int) -> bytes:
        self.need(n)
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def u8(self) -> int:
        return self.raw(1)[0]

    def u16(self) -> int:
        return struct.unpack("<H", self.raw(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.raw(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.raw(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.raw(8))[0]

    def i64(self) -> int:
        return struct.unpack("<q", self.raw(8))[0]

    def fstring(self) -> str:
        n = self.i32()
        if n == 0:
            return ""
        if n > 0:
            if n > 1 << 20:
                raise FormatError("fstring too long")
            return self.raw(n).rstrip(b"\0").decode("utf-8", "replace")
        n = -n
        if n > 1 << 20:
            raise FormatError("fstring too long")
        return self.raw(2 * n).decode("utf-16-le", "replace").rstrip("\0")


# --------------------------------------------------------------------------- TOC
@dataclass
class Toc:
    path: str
    sha256: str
    version: int
    entry_count: int
    block_count: int
    block_size: int
    method_names: list
    container_id: int
    flags: int
    chunk_ids: list            # list of 12-byte raw ids
    offlens: list              # list of (offset, length)
    blocks: "BlockTable"       # lazy (offset, csize, usize, method) records
    mount_point: str
    paths: dict                # toc index -> path (mount-relative joined)
    metas: list                # list of (hash20 bytes, flags)
    dir_index_size: int

    def chunk_type(self, i: int) -> int:
        return self.chunk_ids[i][11]

    def chunk_u64(self, i: int) -> int:
        return struct.unpack("<Q", self.chunk_ids[i][:8])[0]

    def chunk_hex(self, i: int) -> str:
        return self.chunk_ids[i].hex()


def parse_toc(path: str) -> Toc:
    with open(path, "rb") as f:
        data = f.read()
    r = Reader(data)
    if r.raw(16) != TOC_MAGIC:
        raise FormatError(f"{path}: bad TOC magic")
    version = r.u8()
    r.u8(); r.u16()
    header_size = r.u32()
    entry_count = r.u32()
    block_count = r.u32()
    block_entry_size = r.u32()
    method_count = r.u32()
    method_len = r.u32()
    block_size = r.u32()
    dir_size = r.u32()
    partition_count = r.u32()
    container_id = r.u64()
    key_guid = r.raw(16)
    flags = r.u8()
    r.u8(); r.u16()
    seeds_count = r.u32()
    r.u64()  # partition size
    nohash_count = r.u32()
    if version != 8:
        raise FormatError(f"{path}: TOC version {version} unsupported (only 8)")
    if header_size != 144 or block_entry_size != 12:
        raise FormatError(f"{path}: unexpected header/block entry size")
    if flags & (FLAG_ENCRYPTED | FLAG_SIGNED) or key_guid != b"\0" * 16:
        raise FormatError(f"{path}: encrypted/signed container refused")
    if partition_count != 1:
        raise FormatError(f"{path}: multi-partition refused")
    r.p = header_size
    ids = [r.raw(12) for _ in range(entry_count)]
    offlens = []
    for _ in range(entry_count):
        b = r.raw(10)
        offlens.append((int.from_bytes(b[0:5], "big"), int.from_bytes(b[5:10], "big")))
    r.raw(4 * seeds_count)
    r.raw(4 * nohash_count)
    blocks = BlockTable(r.raw(12 * block_count))
    methods = ["None"]
    for _ in range(method_count):
        methods.append(r.raw(method_len).split(b"\0")[0].decode("ascii", "replace"))
    dir_start = r.p
    mount, paths = "", {}
    if dir_size and (flags & FLAG_INDEXED):
        mount, paths = _parse_directory_index(data[dir_start:dir_start + dir_size])
        # make every path relative to the common "../../../" root so TOCs with
        # deeper mount points (e.g. per-chunk sub-mounts) compare correctly
        prefix = mount
        while prefix.startswith("../"):
            prefix = prefix[3:]
        if prefix and not prefix.endswith("/"):
            prefix += "/"
        if prefix:
            paths = {k: prefix + v for k, v in paths.items()}
    r.p = dir_start + dir_size
    metas = []
    for _ in range(entry_count):
        h = r.raw(20)
        fl = r.u8()
        r.raw(3)
        metas.append((h, fl))
    if r.p != len(data):
        raise FormatError(f"{path}: {len(data) - r.p} trailing TOC bytes")
    return Toc(path, hashlib.sha256(data).hexdigest(), version, entry_count, block_count, block_size,
               methods, container_id, flags, ids, offlens, blocks, mount, paths, metas, dir_size)


def _parse_directory_index(buf: bytes):
    r = Reader(buf)
    mount = r.fstring()
    nd = r.i32()
    dirs = [(r.u32(), r.u32(), r.u32(), r.u32()) for _ in range(nd)]
    nf = r.i32()
    files = [(r.u32(), r.u32(), r.u32()) for _ in range(nf)]
    ns = r.i32()
    strings = [r.fstring() for _ in range(ns)]
    if r.p != len(buf):
        raise FormatError("directory index not fully consumed")
    NONE = 0xFFFFFFFF
    out = {}
    stack = [(0, "")] if dirs else []
    seen = 0
    while stack:
        di, prefix = stack.pop()
        seen += 1
        if seen > nd + 1:
            raise FormatError("directory index cycle")
        name, first_child, next_sib, first_file = dirs[di]
        here = prefix + (strings[name] + "/" if name != NONE else "")
        fi = first_file
        while fi != NONE:
            fname, nxt, user = files[fi]
            out[user] = here + strings[fname]
            fi = nxt
        if next_sib != NONE:
            stack.append((next_sib, prefix))
        if first_child != NONE:
            stack.append((first_child, here))
    return mount, out


def read_chunk(toc: Toc, ucas_path: str, i: int, fh=None) -> bytes:
    """Read the logical bytes of TOC entry i (method None blocks only)."""
    off, length = toc.offlens[i]
    if length == 0:
        return b""
    bs = toc.block_size
    first, last = off // bs, (off + length - 1) // bs
    own = fh is None
    f = open(ucas_path, "rb") if own else fh
    try:
        size = os.fstat(f.fileno()).st_size
        out = bytearray()
        for b in range(first, last + 1):
            boff, csize, usize, meth = toc.blocks[b]
            if meth != 0:
                raise FormatError(f"block {b} uses method {toc.method_names[meth]}; only None supported")
            if csize != usize:
                raise FormatError(f"block {b}: stored {csize} != uncompressed {usize} under None")
            if boff + csize > size:
                raise FormatError(f"block {b} beyond UCAS EOF")
            f.seek(boff)
            blk = f.read(csize)
            lo = off - b * bs if b == first else 0
            hi = (off + length) - b * bs if b == last else usize
            out += blk[lo:hi]
    finally:
        if own:
            f.close()
    if len(out) != length:
        raise FormatError("chunk length mismatch after block reads")
    return bytes(out)


def chunk_physical_ranges(toc: Toc, i: int):
    """Exact UCAS byte ranges (start, end) holding TOC entry i, merged."""
    off, length = toc.offlens[i]
    bs = toc.block_size
    first, last = off // bs, (off + length - 1) // bs
    rngs = []
    for b in range(first, last + 1):
        boff, csize, usize, meth = toc.blocks[b]
        lo = off - b * bs if b == first else 0
        hi = (off + length) - b * bs if b == last else usize
        s, e = boff + lo, boff + hi
        if meth != 0:
            s, e = boff, boff + csize  # whole compressed block needed
        if rngs and rngs[-1][1] == s:
            rngs[-1] = (rngs[-1][0], e)
        else:
            rngs.append((s, e))
    return rngs


# ------------------------------------------------------------------- name batches
def read_name_batch(r: Reader) -> list:
    num = r.u32()
    if num == 0:
        return []
    if num > 1 << 22:
        raise FormatError("name batch too large")
    r.u32()  # string bytes
    if r.u64() != FNAME_HASH_ALGORITHM_ID:
        raise FormatError("unknown FName hash algorithm id")
    r.raw(8 * num)
    lens = [struct.unpack(">h", r.raw(2))[0] for _ in range(num)]
    names = []
    for ln in lens:
        if ln < 0:
            n = ln + 32768  # sign bit flags UTF-16; low 15 bits are the char count
            names.append(r.raw(2 * n).decode("utf-16-le", "replace"))
        else:
            names.append(r.raw(ln).decode("latin-1"))
    return names


def mapped_name(names: list, index: int, number: int) -> str:
    idx = index & 0x3FFFFFFF
    if idx >= len(names):
        raise FormatError(f"name index {idx} out of range {len(names)}")
    base = names[idx]
    return base if number == 0 else f"{base}_{number - 1}"


# ----------------------------------------------------------------- script objects
def parse_script_objects(buf: bytes) -> dict:
    """global_index -> {'name', 'outer', 'cdo'}; also resolves 'path'."""
    r = Reader(buf)
    names = read_name_batch(r)
    n = r.i32()
    if n <= 0 or n > 1 << 22:
        raise FormatError("bad script object count")
    objs = {}
    for _ in range(n):
        ni, nn = r.u32(), r.u32()
        gi, outer, cdo = r.u64(), r.u64(), r.u64()
        objs[gi] = {"name": mapped_name(names, ni, nn), "outer": outer, "cdo": cdo}
    if r.p != len(buf):
        raise FormatError("script objects not fully consumed")
    for gi, o in objs.items():
        parts, cur, guard = [], gi, 0
        while cur in objs and guard < 64:
            parts.append(objs[cur]["name"])
            cur = objs[cur]["outer"]
            guard += 1
        parts.reverse()
        o["path"] = parts[0] + ("." + ".".join(parts[1:]) if len(parts) > 1 else "")
    return objs


# --------------------------------------------------------------- container header
def parse_container_header(buf: bytes) -> dict:
    r = Reader(buf)
    if r.u32() != CONTAINER_HEADER_MAGIC:
        raise FormatError("container header magic mismatch")
    version = r.u32()
    if version != 5:
        raise FormatError(f"container header version {version} unsupported (only 5)")
    cid = r.u64()
    n = r.i32()
    pids = [r.u64() for _ in range(n)]
    blen = r.i32()
    base = r.p
    store = r.raw(blen)
    entries = {}
    for i, pid in enumerate(pids):
        e = 16 * i
        imp_n, imp_off, sh_n, sh_off = struct.unpack_from("<IIII", store, e)
        imps = list(struct.unpack_from(f"<{imp_n}Q", store, e + imp_off)) if imp_n else []
        shs = [store[e + 8 + sh_off + 20 * k: e + 8 + sh_off + 20 * k + 20].hex() for k in range(sh_n)]
        if any(len(s) != 40 for s in shs):
            raise FormatError("shader hash out of range")
        entries[pid] = {"imports": imps, "shader_map_hashes": shs}
    del base
    return {"version": version, "container_id": cid, "entries": entries}


# ------------------------------------------------------------------- zen packages
EXPORT_ENTRY_SIZE = 72
BULK_ENTRY_SIZE = 32


@dataclass
class ZenPackage:
    name: str
    package_flags: int
    header_size: int
    cooked_header_size: int
    names: list
    bulk_map: list
    imported_hashes: list
    imports: list            # raw u64s
    exports: list            # dicts
    imported_package_names: list
    data: bytes = field(repr=False, default=b"")

    def export_bytes(self, e: dict) -> bytes:
        return self.data[e["data_start"]:e["data_end"]]


def obj_index_kind(raw: int) -> str:
    return {0: "Export", 1: "ScriptImport", 2: "PackageImport", 3: "Null"}[raw >> 62]


def parse_zen(buf: bytes) -> ZenPackage:
    r = Reader(buf)
    has_ver = r.u32()
    header_size = r.u32()
    name_i, name_n = r.u32(), r.u32()
    flags = r.u32()
    cooked_header_size = r.u32()
    off_hashes = r.i32()
    off_imports = r.i32()
    off_exports = r.i32()
    off_bundle = r.i32()
    off_dep_hdr = r.i32()
    off_dep_ent = r.i32()
    off_pkgnames = r.i32()
    if has_ver:
        raise FormatError("versioned zen package unsupported")
    off_cell_imp = r.i32()
    off_cell_exp = r.i32()
    if not (0 < header_size <= len(buf)):
        raise FormatError("bad header size")
    names = read_name_batch(r)
    bulk = []
    if r.p < off_hashes:
        pad = r.u64()
        if pad > 64:
            raise FormatError("implausible bulk padding")
        r.raw(pad)
        size = r.i64()
        if size % BULK_ENTRY_SIZE:
            raise FormatError("bulk map size not a multiple of entry size")
        for _ in range(size // BULK_ENTRY_SIZE):
            so, dso, ss = r.i64(), r.i64(), r.i64()
            fl = r.u32()
            ci = r.u8()
            r.raw(3)
            bulk.append({"offset": so, "dup_offset": dso, "size": ss, "flags": fl, "cooked_index": ci})
    if r.p != off_hashes:
        raise FormatError(f"header region mismatch before imported hashes ({r.p} != {off_hashes})")
    hashes = list(struct.unpack_from(f"<{(off_imports - off_hashes) // 8}Q", buf, off_hashes))
    imports = list(struct.unpack_from(f"<{(off_exports - off_imports) // 8}Q", buf, off_imports))
    nexp, rem = divmod(off_cell_imp - off_exports, EXPORT_ENTRY_SIZE)
    if rem or nexp <= 0:
        raise FormatError("export map size invalid")
    exports = []
    for k in range(nexp):
        b = off_exports + k * EXPORT_ENTRY_SIZE
        (cso, css, oni, onn, outer, cls, sup, tmpl, peh, oflags, filt) = struct.unpack_from("<QQIIQQQQQIB", buf, b)
        exports.append({"name": mapped_name(names, oni, onn), "outer": outer, "class": cls, "super": sup,
                        "template": tmpl, "public_export_hash": peh, "object_flags": oflags,
                        "filter": filt, "cooked_serial_offset": cso, "size": css})
    # Export payloads follow the header in export-bundle Serialize order (sequential layout).
    end_bundle = off_dep_hdr if off_dep_hdr > 0 else off_pkgnames
    nent, rem = divmod(end_bundle - off_bundle, 8)
    if rem or nent != 2 * nexp:
        raise FormatError("export bundle entries do not hold Create+Serialize per export")
    pos = header_size
    serialized = 0
    for k in range(nent):
        li, cmd = struct.unpack_from("<II", buf, off_bundle + 8 * k)
        if li >= nexp or cmd not in (0, 1):
            raise FormatError("bad export bundle entry")
        if cmd == 1:
            e = exports[li]
            if "data_start" in e:
                raise FormatError("export serialized twice")
            e["data_start"], e["data_end"] = pos, pos + e["size"]
            pos += e["size"]
            serialized += 1
    tail = buf[pos:]
    if serialized != nexp or tail not in (b"", PACKAGE_FILE_TAG):
        raise FormatError(f"export payload layout mismatch (end {pos} vs package {len(buf)})")
    pr = Reader(buf, off_pkgnames)
    pkg_names = read_name_batch(pr)
    nums = [pr.i32() for _ in range(len(pkg_names))]
    pkg_names = [n if k == 0 else f"{n}_{k - 1}" for n, k in zip(pkg_names, nums)]
    if pr.p > header_size:
        raise FormatError("imported package names overrun header")
    return ZenPackage(mapped_name(names, name_i, name_n), flags, header_size, cooked_header_size,
                      names, bulk, hashes, imports, exports, pkg_names, buf)


def resolve_index(pkg: ZenPackage, raw: int, script: dict) -> str:
    kind = obj_index_kind(raw)
    if kind == "Null":
        return "None"
    if kind == "Export":
        k = raw & ((1 << 62) - 1)
        return f"export:{pkg.exports[k]['name']}" if k < len(pkg.exports) else f"export:#{k}"
    if kind == "ScriptImport":
        o = script.get(raw)
        if o is None:
            raise FormatError(f"script import {raw:016x} not in global script objects")
        return o["path"]
    body = raw & ((1 << 62) - 1)
    pi, hi = body >> 32, body & 0xFFFFFFFF
    pn = pkg.imported_package_names[pi] if pi < len(pkg.imported_package_names) else f"#pkg{pi}"
    return f"{pn}:{pkg.imported_hashes[hi]:016x}" if hi < len(pkg.imported_hashes) else f"{pn}:#{hi}"


def load_script_objects(global_utoc: str, global_ucas: str) -> dict:
    toc = parse_toc(global_utoc)
    idx = [i for i in range(toc.entry_count) if toc.chunk_type(i) == 5]
    if len(idx) != 1:
        raise FormatError("expected exactly one ScriptObjects chunk in global container")
    return parse_script_objects(read_chunk(toc, global_ucas, idx[0]))
