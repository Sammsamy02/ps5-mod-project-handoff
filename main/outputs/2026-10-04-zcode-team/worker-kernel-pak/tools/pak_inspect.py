#!/usr/bin/env python3
"""pak_inspect.py — host-side UE PAK/UTOC inspector for the PS5 overlay effort.

Reads a (PC or PS5) Unreal .pak and reports:
  - footer facts: magic, version, index offset/size/hash, index encryption,
    compression-method slots
  - primary index facts: mount point, entry count, index layout variant
  - full entry list (names + offsets + sizes + flags) where the layout allows
  - companion .utoc/.ucas detection and minimal utoc header decode
  - PS5-compatibility flags: entry-type census (.uasset/.uexp/.ubulk/
    .ushaderbytecode/.ini...), encryption, compression vs PS5 target pak

Two v11 primary-index layouts are supported, both byte-proven against local
corpora (see PAK-OVERLAY.md §3):
  A) "game-compact" — the Townfall PS5 shipping layout (region table with
     SHA1-verified path-hash + name-directory regions, 12-byte entry records).
     Proven by outputs/2026-10-04-zcode-team/worker-pak-index.
  B) "classic" — per-entry serialized records used by repak v0.2.3 output and
     the PC Leon mod stub pak.

Read-only. No console contact. No network.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

PAK_MAGIC = 0x5A6F12E1
FOOTER_MAX_SIZE = 221
COMPRESSION_SLOT_COUNT = 5
SLOT_STRIDE = 32
# game-compact layout constants (worker-pak-index evidence)
GAME_TABLE_LEAD = 0x25E05A1C


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def read_fstring(b, o):
    """UE FString: i32 len (incl NUL), len bytes utf-8 (pak index encoding)."""
    n = i32(b, o)
    o += 4
    if n == 0:
        return "", o
    if n < 0 or n > len(b) - o:
        raise ParseError(f"FString length {n} out of bounds at {o - 4:#x}")
    raw = b[o : o + n]
    o += n
    return raw[: n - 1].decode("utf-8", "replace"), o


def sha1_hex(data):
    return hashlib.sha1(data).hexdigest()


class ParseError(Exception):
    pass


def parse_footer(f, file_size):
    f.seek(max(0, file_size - FOOTER_MAX_SIZE))
    tail = f.read(FOOTER_MAX_SIZE)
    pos = tail.rfind(struct.pack("<I", PAK_MAGIC))
    if pos < 0:
        raise ParseError("pak magic 0x5A6F12E1 not found in last 221 bytes")
    o = pos
    magic = u32(tail, o)
    version = u32(tail, o + 4)
    index_offset = u64(tail, o + 8)
    index_size = u64(tail, o + 16)
    index_hash = tail[o + 24 : o + 44]
    encrypted_index = tail[o + 44]
    methods_start = o + 45
    methods = []
    for i in range(COMPRESSION_SLOT_COUNT):
        s = tail[methods_start + i * SLOT_STRIDE : methods_start + (i + 1) * SLOT_STRIDE]
        methods.append(s.split(b"\x00")[0].decode("ascii", "replace"))
    footer_size = file_size - (pos + (file_size - FOOTER_MAX_SIZE if file_size >= FOOTER_MAX_SIZE else 0) - (FOOTER_MAX_SIZE - len(tail)))  # informational only
    return {
        "magic_ok": magic == PAK_MAGIC,
        "version": version,
        "index_offset": index_offset,
        "index_size": index_size,
        "index_hash_sha1": index_hash.hex(),
        "index_sha1_actual": None,  # filled by caller if index read
        "encrypted_index": bool(encrypted_index),
        "compression_methods": methods,
        "footer_region": [file_size - (len(tail) - pos), file_size],
        "footer_bytes": len(tail) - pos,
    }


def try_game_compact_index(blob, file_size=None):
    """Layout A. Returns dict or raises ParseError.

    Structural validation, no fixed magic: the leading u32 is data-dependent
    (observed 0x25E05A1C on pakchunk0-ps5, 0x25EB747C on the 1-entry
    ModBridge diagnostic pak — consistent with a packed bit-width header for
    encoded entries). Acceptance requires: both region descriptors well-formed
    (present in {0,1}, in-file extents), entry-blob size == count*12, and the
    12-byte entry array closing the index exactly.
    """
    mount, o = read_fstring(blob, 0)
    count = i32(blob, o)
    o += 4
    if count < 0:
        raise ParseError("negative count")
    t = o
    # The u32 after the count is the path-hash seed: byte-proved by matching
    # repak's documented --path-hash-seed 4084654653 (0xF376E23D) in the
    # repak-built spike pak at exactly this position. Townfall's own pak
    # carries 0x25E05A1C here. Seeds are arbitrary; no family gate.
    seed = u32(blob, t)
    if len(blob) < t + 96:
        raise ParseError("index too short for compact region table")
    regions = []
    for i in range(2):
        present = u32(blob, t + 8 + i * 40)
        off = u64(blob, t + 12 + i * 40)
        size = u64(blob, t + 20 + i * 40)
        sha = blob[t + 28 + i * 40 : t + 48 + i * 40]
        if present not in (0, 1):
            raise ParseError(f"region {i} present={present}")
        if present:
            if file_size is not None and (off + size > file_size or off < 0 or size < 4):
                raise ParseError(f"region {i} extent [{off},{off+size}) out of file")
        regions.append(
            {"present": present, "offset": off, "size": size, "sha1_hex": sha.hex()}
        )
    blob_size = u32(blob, t + 88)
    master_flags = u32(blob, t + 92)
    entries_off = t + 96
    if entries_off + count * 12 != len(blob):
        raise ParseError(
            f"entry array [{entries_off:#x}..{entries_off + count * 12:#x}) "
            f"does not close index of {len(blob):#x}"
        )
    if blob_size != count * 12:
        raise ParseError(f"entry blob size {blob_size} != count*12 {count*12}")
    return {
        "layout": "game-compact",
        "mount_point": mount,
        "count": count,
        "path_hash_seed": f"{seed:#x}",
        "regions": regions,
        "entry_blob_size": blob_size,
        "master_flags": f"{master_flags:#x}",
        "entries_off": entries_off,
    }


def try_classic_index(blob, count_cap=200000):
    """Layout B: mount, count, then per-entry classic records.

    Classic v10/v11 entry record (UnrealPak/repak):
      FString name; u64 offset; u64 size; u32 uncompressed_size;
      u32 compression_method; u32 flags; u8 compression_blocks_count;
      [if blocks: u32 block_size + blocks]; u8 encrypted; u32 compression_block_size;
      u8[20] hash
    With the common no-compression shortcut (method=0x20 'None' blocks=0) this
    is name(4+n) + 8+8+4+4+4+1+1+4+20 = 58 + name. We validate by walking to
    exactly the end of the index.
    """
    mount, o = read_fstring(blob, 0)
    count = i32(blob, o)
    o += 4
    if count < 0 or count > count_cap:
        raise ParseError("implausible classic count")
    entries = []
    start = o
    for _ in range(count):
        name, o = read_fstring(blob, o)
        if o + 45 > len(blob):
            raise ParseError("classic entry overruns index")
        offset = u64(blob, o)
        size = u64(blob, o + 8)
        usize = u32(blob, o + 16)
        method = u32(blob, o + 20)
        flags = u32(blob, o + 24)
        nblocks = blob[o + 28]
        o += 29
        if method != 0x20:  # compressed entries carry a block list
            raise ParseError(f"compressed entry (method {method:#x}) unsupported in classic walker")
        encrypted = blob[o]
        block_size = u32(blob, o + 1)
        o += 5
        sha = blob[o : o + 20]
        o += 20
        entries.append(
            {
                "name": name,
                "offset": offset,
                "size": size,
                "uncompressed_size": usize,
                "compression_method": method,
                "flags": f"{flags:#x}",
                "encrypted": bool(encrypted),
                "block_size": block_size,
                "hash_sha1": sha.hex(),
            }
        )
    if count and o != len(blob):
        raise ParseError(f"classic walk ends at {o:#x}, index is {len(blob):#x}")
    return {
        "layout": "classic",
        "mount_point": mount,
        "count": count,
        "entries": entries,
        "entries_off": start,
    }


def walk_name_directory(blob, region):
    """Region C walk: i32 ndirs; per dir FString + i32 nfiles; per file FString + u32 ref."""
    off, size = region["offset"], region["size"]
    data = blob["reader"](off, size) if isinstance(blob, dict) else blob
    o = 0
    ndirs = i32(data, o)
    o += 4
    files = []
    for _ in range(ndirs):
        dname, o = read_fstring(data, o)
        nfiles = i32(data, o)
        o += 4
        for _ in range(nfiles):
            fname, o = read_fstring(data, o)
            ref = u32(data, o)
            o += 4
            files.append((f"{dname}{fname}", ref))
    if o != size:
        raise ParseError(f"directory walk consumed {o} of {size}")
    return ndirs, files


def entry_flags_desc(flags):
    """Compact-entry flag bits: semantics not asserted. Sampled original
    entries carrying 0xE0000000 decoded as plaintext (mount-research original
    payload receipts), so bit31 must not be read as 'encrypted'."""
    bits = [str(i) for i in range(32) if flags & (1 << i)]
    return f"bits [{','.join(bits)}] set" if flags else "0"


def inspect_pak(path):
    out = {"path": path, "bytes": os.path.getsize(path)}
    with open(path, "rb") as f:
        footer = parse_footer(f, os.path.getsize(path))
        out["footer"] = footer
        f.seek(footer["index_offset"])
        index = f.read(footer["index_size"])
        out["footer"]["index_sha1_actual"] = sha1_hex(index)
        out["index_sha1_match"] = (
            out["footer"]["index_sha1_actual"] == footer["index_hash_sha1"]
        )
        out["file_size"] = os.path.getsize(path)
        layout = None
        errors = {}
        for fn, kw in ((try_game_compact_index, {"file_size": os.path.getsize(path)}), (try_classic_index, {})):
            try:
                layout = fn(index, **kw)
                break
            except ParseError as e:
                errors[fn.__name__] = str(e)
        out["layout_errors"] = errors
        entries = []
        if layout is None:
            out["index"] = None
        else:
            out["index"] = {k: v for k, v in layout.items() if k != "entries"}
            if layout["layout"] == "classic":
                entries = layout["entries"]
            else:
                # game-compact: resolve names via region 2 (name directory)
                f2 = open(path, "rb")

                def reader(off, size):
                    f2.seek(off)
                    return f2.read(size)

                dir_region = layout["regions"][1]
                ndirs, files = walk_name_directory({"reader": reader}, dir_region)
                compact = []
                eo = layout["entries_off"]
                for i in range(layout["count"]):
                    off, size, flags = struct.unpack_from("<III", index, eo + i * 12)
                    compact.append({"offset": off, "size": size, "flags": flags})
                by_ref = {}
                for name, ref in files:
                    by_ref[ref] = name
                for i, rec in enumerate(compact):
                    name = by_ref.get(i * 12, f"<unresolved#{i}>")
                    entries.append(
                        {
                            "name": name,
                            "offset": rec["offset"],
                            "size": rec["size"],
                            "flags": f"{rec['flags']:#x}",
                            "flags_desc": entry_flags_desc(rec["flags"]),
                        }
                    )
                # verify region SHAs
                for r in layout["regions"]:
                    if r["present"]:
                        data = reader(r["offset"], r["size"])
                        r["sha1_actual"] = sha1_hex(data)
                        r["sha1_match"] = r["sha1_actual"] == r["sha1_hex"]
                f2.close()
            out["entries"] = entries
    return out


UTOC_MAGIC = 0xC5D6A039
PS5_PREFIX = b"-==--==--==--==-"


def inspect_utoc(path):
    """Minimal .utoc header probe.

    Stock UE utoc: {u32 magic 0xC5D6A039; u32 version; u32 header_size; u32
    toc_entry_count; u32 compression_block_count; ...}. The Townfall PS5 and
    Leon PC utocs both carry a 16-byte b'-==-'*4 prefix in place of/over the
    magic and the first stock fields, with version at file+0x10 (byte-proved:
    Leon version 8, header size 0x90, 162 entries, 2,948 blocks match the
    frozen native iostore reader receipts). Probe both positions.
    """
    with open(path, "rb") as f:
        b = f.read(0x50)
    res = {"path": path}
    if len(b) >= 0x44 and b[:16] == PS5_PREFIX:
        base = 0x10
        res["ps5_prefix"] = True
        res["version"] = u32(b, base)
        res["header_size"] = u32(b, base + 4)
        res["toc_entry_count"] = u32(b, base + 8)
        res["compression_block_count"] = u32(b, base + 12)
        res["chunk_id_size"] = u32(b, base + 16)
        res["compression_block_size"] = u32(b, base + 28)
    elif len(b) >= 0x40 and u32(b, 0) == UTOC_MAGIC:
        res["version"] = u32(b, 4)
        res["header_size"] = u32(b, 8)
        res["toc_entry_count"] = u32(b, 12)
        res["compression_block_count"] = u32(b, 16)
    else:
        res["error"] = "utoc magic not found at either stock or PS5-prefixed position"
    return res


def compatibility_report(insp, utoc=None):
    rep = []
    foot = insp.get("footer", {})
    if foot.get("version") == 11:
        rep.append(("pak_version", "ok", "v11 matches PS5 target pakchunk0-ps5.pak (v11)"))
    elif foot.get("version") is not None:
        rep.append(("pak_version", "warn", f"v{foot['version']} differs from target v11; engine may reject index"))
    if foot.get("encrypted_index"):
        rep.append(("index_encryption", "blocker", "encrypted index requires AES key the PS5 runtime does not have"))
    comps = [c for c in foot.get("compression_methods", []) if c]
    if comps:
        rep.append(("compression", "warn", f"non-empty compression slots {comps}; PS5 target pak uses None — decompression requires matching codec (Oodle OK, PC-only codecs not)"))
    entries = insp.get("entries", [])
    census = {}
    for e in entries:
        ext = e["name"].rsplit(".", 1)[-1].lower() if "." in e["name"] else "(none)"
        census[ext] = census.get(ext, 0) + 1
    rep.append(("entry_census", "info", json.dumps(census, sort_keys=True)))
    for e in entries:
        low = e["name"].lower()
        if low.endswith((".ushaderbytecode", ".ushadermap")):
            rep.append(("shader_bytecode", "blocker", f"{e['name']}: PC shader library (SM5/SM6 D3D) cannot load on PS5 (GNF)"))
            break
        if low.endswith((".uasset", ".uexp", ".umap")):
            rep.append(("cooked_packages", "warn", "PC-cooked packages present: package summary links PC texture/shader formats; PS5 load unverified and generally requires recooking for console target"))
            break
    if utoc:
        rep.append(("utoc", "info", json.dumps(utoc)))
    return rep


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pak")
    ap.add_argument("--utoc")
    ap.add_argument("--json")
    ap.add_argument("--max-entries-listed", type=int, default=40)
    args = ap.parse_args()
    insp = inspect_pak(args.pak)
    utoc = inspect_utoc(args.utoc) if args.utoc else None
    rep = compatibility_report(insp, utoc)
    listed = insp.get("entries", [])[: args.max_entries_listed]
    summary = {
        "file": os.path.basename(args.pak),
        "bytes": insp["bytes"],
        "footer": insp["footer"],
        "index": insp.get("index"),
        "layout_errors": insp.get("layout_errors"),
        "index_sha1_match": insp.get("index_sha1_match"),
        "entry_count": len(insp.get("entries", [])),
        "entries_sample": listed,
        "utoc": utoc,
        "compatibility": [{"aspect": a, "level": lvl, "note": n} for a, lvl, n in rep],
    }
    text = json.dumps(summary, indent=1)
    if args.json:
        with open(args.json, "w") as f:
            f.write(text)
    print(text)


if __name__ == "__main__":
    main()
