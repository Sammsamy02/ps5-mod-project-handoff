#!/usr/bin/env python3
"""Derived-copy rewrite of ONE package import inside a Zen package (Mac-only).

Why: a "closest-match" substitution (point a mod material instance at a
PS5-cooked base parent) must change BOTH places the engine uses to resolve
the parent import:
  1. the container store entry's ImportedPackages[k]   (overlay_builder does this)
  2. inside the package: ImportedPackageNames[k] and the ImportedPublicExportHash
     that the import-map row points at                    (this tool does this)
Changing only (1) leaves (2) naming the old export hash, which the new parent
package does not export (public export hash = CityHash64 of the UTF-16LE
lower-case export name, verified 127/127 on real packages) -> unresolved import.

The input bytes are never modified; output is a NEW package byte string.
Export payloads, the export map, the name map and the import map are kept
byte-identical; only the imported-public-export-hash slot, the imported
package name batch and the HeaderSize field change. The result is re-parsed
and every invariant re-checked before it is returned (fail closed).

Generic: nothing game- or mod-specific; the target names come from the
caller's plan (which takes them from the game's own TOC directory index).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iostore_lib as io  # noqa: E402
from cityhash64 import cityhash64, fname_hash  # noqa: E402

SUMMARY_SIZE = 52  # through ImportedPackageNamesOffset (container header version 5 layout)


def public_export_hash(export_relative_path: str) -> int:
    return cityhash64(export_relative_path.lower().encode("utf-16-le"))


def build_name_batch(names) -> bytes:
    enc = []
    for n in names:
        try:
            enc.append((n.encode("ascii"), False))
        except UnicodeEncodeError:
            enc.append((n.encode("utf-16-le"), True))
    out = struct.pack("<I", len(names))
    if not names:
        return out
    out += struct.pack("<I", sum(len(b) for b, _ in enc))
    out += struct.pack("<Q", io.FNAME_HASH_ALGORITHM_ID)
    out += b"".join(struct.pack("<Q", fname_hash(n)) for n in names)
    for (b, wide) in enc:
        ln = len(b) // 2 if wide else len(b)
        if ln > 0x7FFF:
            raise io.FormatError("name too long")
        out += struct.pack(">h", (ln - 32768) if wide else ln)
    out += b"".join(b for b, _ in enc)
    return out


def split_number(name: str):
    """'Foo_3' -> ('Foo', 4) using the FName number convention; else (name, 0)."""
    base, sep, tail = name.rpartition("_")
    if sep and tail.isdigit() and (tail == "0" or not tail.startswith("0")):
        return base, int(tail) + 1
    return name, 0


def rewrite_import(pkg_bytes: bytes, from_package: str, from_export: str,
                   to_package: str, to_export: str) -> (bytes, dict):
    pkg = io.parse_zen(pkg_bytes)
    names = list(pkg.imported_package_names)
    if from_package not in names:
        raise io.FormatError(f"package does not import {from_package}")
    if to_package in names:
        raise io.FormatError(f"package already imports {to_package}; merge rewrite unsupported")
    k = names.index(from_package)
    old_hash = public_export_hash(from_export)
    new_hash = public_export_hash(to_export)
    # import-map rows that target (k, hash-slot) -> collect the hash slots used
    slots = set()
    for raw in pkg.imports:
        if io.obj_index_kind(raw) != "PackageImport":
            continue
        body = raw & ((1 << 62) - 1)
        pi, hi = body >> 32, body & 0xFFFFFFFF
        if pi == k:
            if pkg.imported_hashes[hi] != old_hash:
                raise io.FormatError(
                    f"import of {from_package} uses export hash {pkg.imported_hashes[hi]:016x}, "
                    f"not {old_hash:016x} ({from_export}); refusing to guess")
            slots.add(hi)
    if not slots:
        raise io.FormatError("no import-map row references the source package")
    for hi in slots:
        if sum(1 for raw in pkg.imports if io.obj_index_kind(raw) == "PackageImport"
               and ((raw & ((1 << 62) - 1)) & 0xFFFFFFFF) == hi
               and ((raw & ((1 << 62) - 1)) >> 32) != k):
            raise io.FormatError("hash slot shared with another package import; refusing")

    b = bytearray(pkg_bytes)
    off_hashes = struct.unpack_from("<i", b, 24)[0]
    off_pkgnames = struct.unpack_from("<i", b, 48)[0]
    header_size = pkg.header_size
    for hi in slots:
        struct.pack_into("<Q", b, off_hashes + 8 * hi, new_hash)
    names[k] = to_package
    plain, numbers = [], []
    for n in names:
        base, num = split_number(n)
        plain.append(base)
        numbers.append(num)
    new_tail = build_name_batch(plain) + b"".join(struct.pack("<i", x) for x in numbers)
    out = bytes(b[:off_pkgnames]) + new_tail
    new_header_size = len(out)
    out = bytearray(out + bytes(pkg_bytes[header_size:]))
    struct.pack_into("<I", out, 4, new_header_size)
    out = bytes(out)

    # ---- fail-closed re-verification
    new = io.parse_zen(out)
    if new.imported_package_names[k] != to_package:
        raise io.FormatError("rewrite verification: package name not updated")
    if [x for i, x in enumerate(new.imported_package_names) if i != k] != \
            [x for i, x in enumerate(pkg.imported_package_names) if i != k]:
        raise io.FormatError("rewrite verification: other imported names changed")
    if new.imports != pkg.imports or new.names != pkg.names:
        raise io.FormatError("rewrite verification: import map or name map changed")
    for hi, (a, c) in enumerate(zip(pkg.imported_hashes, new.imported_hashes)):
        if (hi in slots and c != new_hash) or (hi not in slots and a != c):
            raise io.FormatError("rewrite verification: export hash slots wrong")
    if len(new.exports) != len(pkg.exports):
        raise io.FormatError("rewrite verification: export count changed")
    for e0, e1 in zip(pkg.exports, new.exports):
        if pkg.export_bytes(e0) != new.export_bytes(e1):
            raise io.FormatError("rewrite verification: export payload changed")
    receipt = {"from_package": from_package, "from_export": from_export, "from_hash": f"{old_hash:016x}",
               "to_package": to_package, "to_export": to_export, "to_hash": f"{new_hash:016x}",
               "imported_package_index": k, "hash_slots": sorted(slots),
               "header_size_before": header_size, "header_size_after": new_header_size,
               "bytes_before": len(pkg_bytes), "bytes_after": len(out),
               "sha256_before": hashlib.sha256(pkg_bytes).hexdigest(),
               "sha256_after": hashlib.sha256(out).hexdigest(),
               "exports_byte_identical": len(pkg.exports)}
    return out, receipt


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp", required=True, help="raw Zen package chunk (read-only)")
    ap.add_argument("--from-package", required=True)
    ap.add_argument("--from-export", required=True)
    ap.add_argument("--to-package", required=True)
    ap.add_argument("--to-export", required=True)
    ap.add_argument("--out", required=True, help="new file; must not exist")
    a = ap.parse_args(argv)
    if os.path.exists(a.out):
        print("refused: output exists")
        return 2
    data = open(a.inp, "rb").read()
    try:
        out, rec = rewrite_import(data, a.from_package, a.from_export, a.to_package, a.to_export)
    except io.FormatError as e:
        print(json.dumps({"result": "refused", "detail": str(e)}))
        return 2
    with open(a.out, "wb") as f:
        f.write(out)
    print(json.dumps({"result": "rewritten", **rec}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
