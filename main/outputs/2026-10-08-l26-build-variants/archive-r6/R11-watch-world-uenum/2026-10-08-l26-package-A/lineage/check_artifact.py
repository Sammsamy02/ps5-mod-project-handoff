"""Check the locally built module and SDK wrapper without running native code."""
from pathlib import Path
import hashlib
import json
import struct as s

base = Path(__file__).resolve().parent
out = base / 'build-1'
bare = (out / 'bare-sdk/modbridge_runtime_aligned_entry.prx').read_bytes()
wrapped = (out / 'modbridge_runtime_aligned_entry.prx').read_bytes()
assert bare[:4] == b'\x7fELF' and wrapped[:4] == bytes.fromhex('4f153d1d')
count = s.unpack_from('<I2HQ2H4x', wrapped, 8)[4]
elf_at = 32 + 32 * count
phoff = s.unpack_from('<Q', bare, 32)[0]
phnum = s.unpack_from('<H', bare, 56)[0]
header_end = max(64, phoff + 56 * phnum)
expected_header = bytearray(bare[:header_end])
# Pinned SDK make_fself.py deliberately removes the embedded section count.
s.pack_into('<H', expected_header, 60, 0)
assert wrapped[elf_at:elf_at + header_end] == expected_header
info_at = (elf_at + header_end + 15) & ~15
assert s.unpack_from('<4Q32s', wrapped, info_at)[4] == hashlib.sha256(bare).digest()
segments = []
for j in range(1, count, 2):
    props, offset, stored, _ = s.unpack_from('<4Q', wrapped, 32 + 32 * j)
    index = (props >> 20) & 65535
    ph = s.unpack_from('<II6Q', bare, phoff + 56 * index)
    assert stored == ph[5]
    assert wrapped[offset:offset + stored] == bare[ph[2]:ph[2] + ph[5]]
    segments.append({'index': index, 'type': hex(ph[0]), 'bytes': stored})
shoff = s.unpack_from('<Q', bare, 40)[0]
shnum, shstr = s.unpack_from('<HH', bare, 60)
sections = [s.unpack_from('<II4QII2Q', bare, shoff + 64 * i) for i in range(shnum)]
st = sections[shstr]
strings = bare[st[4]:st[4] + st[5]]
named = {strings[x[0]:].split(b'\0')[0].decode(): x for x in sections}
assert named['.init_array'][5] // 8 == 232
assert named['.fini_array'][5] // 8 == 2
for name in ['.eh_frame', '.eh_frame_hdr', '.gcc_except_table']:
    assert named[name][5] > 0
phdrs = [s.unpack_from('<II6Q', bare, phoff + 56 * i) for i in range(phnum)]
module = [p for p in phdrs if p[0] == 0x61000002]
assert len(module) == 1
assert s.unpack_from('<I', bare, module[0][2] + 20)[0] == 0x11000000
dynamic = next(p for p in phdrs if p[0] == 2)
tags = dict(s.unpack_from('<QQ', bare, dynamic[2] + i)
            for i in range(0, dynamic[5], 16))
assert tags[12] != 0 and tags[13] != 0  # Explicit DT_INIT / DT_FINI.
assert all(tags.get(t, 0) == 0 for t in [25, 26, 27, 28])
result = dict(status='LOCAL_PACKAGE_CHECKS_PASSED', bytes=len(wrapped),
              sha256=hashlib.sha256(wrapped).hexdigest(),
              bare_sha256=hashlib.sha256(bare).hexdigest(),
              wrapped_segments_equal=segments, init_slots=232, fini_slots=2,
              explicit_start_stop=True, loader_array_ownership=False,
              sdk='0x11000000', native_executed=False)
(out / 'artifact-checks.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
