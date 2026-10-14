# Reproduce saved-image SCO checks without a runtime build

Run from `/WORKSPACE/.codex/worktrees/translation-core/what`. Python's installed Capstone is sufficient; this only reads the saved game image and writes audit evidence. It does not access the console. Labels are runtime-label addresses from the prior saved audit, with runtime-label = ELF vaddr + 0x400000. The ELF segment table, not a guessed file delta, maps each label to a file offset.

```python
from pathlib import Path
import struct, hashlib, re, json, capstone
root = Path.cwd()
out = root / 'outputs/2026-10-08-l21-audit/mod-api'
image = root / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
data = image.read_bytes()
assert len(data) == 194241424
assert hashlib.sha256(data).hexdigest() == '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff = struct.unpack_from('<Q', data, 32)[0]
ents, number = struct.unpack_from('<HH', data, 54)
segments = []
for n in range(number):
    p = struct.unpack_from('<IIQQQQQQ', data, phoff + n * ents)
    if p[0] == 1:
        segments.append((p[3], p[2], p[5]))
def file_offset(runtime_label):
    vaddr = runtime_label - 0x400000
    for start, offset, size in segments:
        if start <= vaddr < start + size:
            return offset + vaddr - start
    raise ValueError(hex(runtime_label))
for name, expected_count in [('StaticConstructObject', 114), ('ObjectInitializer', 86)]:
    asm = root / 'outputs/2026-10-07-l19-static-layout-check' / (name + '.asm')
    count = 0
    for line in asm.read_text().splitlines():
        m = re.match(r'(0x[0-9a-f]+)\s+\(f .*?\)\s+([0-9a-f]+)', line)
        if m:
            address, code = int(m[1], 16), bytes.fromhex(m[2])
            offset = file_offset(address)
            assert data[offset:offset + len(code)] == code, hex(address)
            count += 1
    assert count == expected_count
    print(name, count, 'instruction byte checks passed')
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for name, address, size in [('sco', 0x890aa0, 0x1d5),
                            ('object-initializer', 0x891db0, 0x16b),
                            ('sco-parameter-constructor', 0x893530, 0x4b)]:
    offset = file_offset(address)
    lines = [f'{i.address:#x}\t{i.bytes.hex()}\t{i.mnemonic} {i.op_str}'
             for i in md.disasm(data[offset:offset + size], address)]
    (out / (name + '-fresh-reproduced.tsv')).write_text('\n'.join(lines) + '\n')
```

Expected: 114 + 86 matching instruction encodings. The parameter constructor returns at0x89357a. This recipe demonstrates bytes and decoding, not a live function call, full type layout, dynamic call graph, or fresh executable identity. The optional raw rel32 candidate list in `SCO-IMAGE-VALIDATION.json` is diagnostic and is not required to reproduce the field observations.
