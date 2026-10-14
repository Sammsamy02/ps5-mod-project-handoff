#!/usr/bin/env python3
"""Deterministic synthetic ZIP fixture for the streaming-bridge host test.

Stored (uncompressed) entries only — the pinned intake admits none/zip with
any member content, and stored bytes keep the sha story trivial. Names avoid
every intake-rejected pattern (absolute, drive colon, '..', trailing dot or
space). Writes meta.txt: "rows=<file+dir count>" and "regular_bytes=<sum>".
"""
import sys
import zipfile

def content(seed: int, size: int) -> bytes:
    out = bytearray()
    x = (seed * 2654435761 + 1) & 0xFFFFFFFF
    while len(out) < size:
        x = (x * 1103515245 + 12345) & 0xFFFFFFFF
        out += x.to_bytes(4, "little")
    return bytes(out[:size])

def main() -> None:
    zip_path, meta_path = sys.argv[1], sys.argv[2]
    plan = [
        (True, "mods/", 0),
        (False, "mods/readme.txt", 1234),
        (False, "mods/config.ini", 97),
        (True, "mods/textures/", 0),
        (False, "mods/textures/a.bin", 40000),
        (False, "mods/textures/b.bin", 4096),
        (True, "mods/audio/", 0),
        (False, "mods/audio/loop.raw", 250000),
        (False, "mods/scripts/main.lua", 8192),
        (False, "mods/scripts/util.lua", 33),
    ]
    rows = 0
    regular_bytes = 0
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_STORED) as z:
        for i, (is_dir, name, size) in enumerate(plan):
            if is_dir:
                info = zipfile.ZipInfo(name)
                info.external_attr = (0o40755 << 16) | 0x10  # dir bits + MS-DOS dir flag
                z.writestr(info, b"")
            else:
                z.writestr(name, content(i + 1, size))
                regular_bytes += size
            rows += 1
    with open(meta_path, "w") as f:
        f.write(f"rows={rows}\nregular_bytes={regular_bytes}\n")

if __name__ == "__main__":
    main()
