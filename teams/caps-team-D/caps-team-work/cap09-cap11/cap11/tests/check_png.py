"""Independent PNG check (Mac-only): decode atlas.png with Python's zlib + a
from-spec chunk parser (CRC verified), compare every pixel's alpha with the
raw atlas written by the host test. Prints items_checked; fails on zero."""
import struct
import sys
import zlib

png = open(sys.argv[1], "rb").read()
alpha = open(sys.argv[2], "rb").read()
items = 0
fails = []


def check(c, what):
    global items
    items += 1
    print(("  pass " if c else "  FAIL ") + what)
    if not c:
        fails.append(what)


check(png[:8] == b"\x89PNG\r\n\x1a\n", "PNG signature")
pos = 8
chunks = []
while pos < len(png):
    (n,) = struct.unpack(">I", png[pos:pos + 4])
    t = png[pos + 4:pos + 8]
    data = png[pos + 8:pos + 8 + n]
    (crc,) = struct.unpack(">I", png[pos + 8 + n:pos + 12 + n])
    check(zlib.crc32(t + data) & 0xFFFFFFFF == crc, f"chunk {t.decode()} CRC")
    chunks.append((t, data))
    pos += 12 + n
check([c[0] for c in chunks] == [b"IHDR", b"IDAT", b"IEND"], "chunk order IHDR IDAT IEND")
w, h, depth, ctype, comp, flt, inter = struct.unpack(">IIBBBBB", chunks[0][1])
check((depth, ctype, comp, flt, inter) == (8, 6, 0, 0, 0), f"IHDR RGBA8 {w}x{h}")
raw = zlib.decompress(chunks[1][1])
check(len(raw) == h * (w * 4 + 1), "decompressed size")
check(len(alpha) == w * h, "raw atlas size matches")
bad = 0
for y in range(h):
    row = raw[y * (w * 4 + 1):(y + 1) * (w * 4 + 1)]
    if row[0] != 0:
        bad += w
        continue
    for x in range(w):
        r, g, b, a = row[1 + x * 4:5 + x * 4]
        if (r, g, b) != (255, 255, 255) or a != alpha[y * w + x]:
            bad += 1
check(bad == 0, f"all {w*h} pixels decode to white + the atlas coverage")
print(f"check_png items_checked={items} failed={len(fails)}")
sys.exit(2 if items == 0 else (1 if fails else 0))
