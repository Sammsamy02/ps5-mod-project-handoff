#!/usr/bin/env python3
"""Write a SYNTHETIC, unencrypted IoStore .utoc (TOC v8) whose directory index
lists the given virtual paths. Development fixture only: it carries no chunk
payloads and proves nothing about any real game. Used to exercise the
content-index -> compat pipeline without game data.

    synthetic_utoc.py OUT.utoc Townfall/Content/Items/BP_A.uasset ...

Paths are relative to the "../../../" mount point, '/'-separated.
"""
import struct
import sys

NONE = 0xFFFFFFFF


def fstring(text):
    data = text.encode("utf-8") + b"\0"
    return struct.pack("<I", len(data)) + data


def directory_index(paths):
    strings, string_ids = [], {}

    def sid(name):
        if name not in string_ids:
            string_ids[name] = len(strings)
            strings.append(name)
        return string_ids[name]

    # dirs[i] = [name_id, first_child, next_sibling, first_file]; row 0 = root (no name)
    dirs = [[NONE, NONE, NONE, NONE]]
    children = {0: {}}
    files = []  # [name_id, next_file, user_data]
    for path in sorted(set(paths)):
        parts = path.split("/")
        if not path or any(p in ("", ".", "..") for p in parts):
            raise SystemExit(f"invalid path: {path!r}")
        node = 0
        for part in parts[:-1]:
            if part not in children[node]:
                row = len(dirs)
                dirs.append([sid(part), NONE, NONE, NONE])
                children[row] = {}
                children[node][part] = row
                # prepend to the parent's child list
                dirs[row][2] = dirs[node][1]
                dirs[node][1] = row
            node = children[node][part]
        row = len(files)
        files.append([sid(parts[-1]), dirs[node][3], row])
        dirs[node][3] = row
    out = fstring("../../../") + struct.pack("<I", len(dirs))
    out += b"".join(struct.pack("<4I", *d) for d in dirs)
    out += struct.pack("<I", len(files)) + b"".join(struct.pack("<3I", *f) for f in files)
    out += struct.pack("<I", len(strings)) + b"".join(fstring(s) for s in strings)
    return out


def utoc(index):
    # Header layout mirrors tests/content_index_tests.cpp fixtures (2 chunk
    # entries, 1 compression block, 1 method name, Indexed flag 0x08).
    header = b"-==--==--==--==-" + bytes([8, 0, 0, 0])
    header += struct.pack("<8I", 144, 2, 3, 12, 1, 32, 65536, len(index))
    header += struct.pack("<I", 1) + struct.pack("<Q", 7) + b"\0" * 16 + bytes([8, 0, 0, 0])
    header += struct.pack("<I", 1) + struct.pack("<Q", 1 << 30) + struct.pack("<I", 1) + b"\0" * 44
    assert len(header) == 144
    return header + b"\0" * 120 + index + b"\0" * 48


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    with open(argv[1], "xb") as out:
        out.write(utoc(directory_index(argv[2:])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
