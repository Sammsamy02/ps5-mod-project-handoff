#!/usr/bin/env python3
"""Independent bounded offline geometry/hash oracle for synthetic writer output."""
from pathlib import Path
import hashlib
import json
import struct
import sys

EXPECTED = {
    'Engine/Content/UI/Icon.bin': bytes([0, 1, 2, 255, 128, 0, 42, 17, 99]),
    'Example/Config/base.ini': b'mode=original\r\n',
    'empty.bin': b'',
    'readme.txt': b'bounded synthetic bytes\n',
}


def fnv(path, seed):
    value = (0xcbf29ce484222325 + seed) & ((1 << 64) - 1)
    for byte in path.lower().encode('utf-16-le'):
        value = ((value ^ byte) * 0x100000001b3) & ((1 << 64) - 1)
    return value


class Cursor:
    def __init__(self, data):
        assert len(data) <= 4 * 1024 * 1024
        self.data = data
        self.at = 0

    def take(self, size):
        assert 0 <= size <= len(self.data) - self.at
        result = self.data[self.at:self.at + size]
        self.at += size
        return result

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def u64(self):
        return struct.unpack('<Q', self.take(8))[0]

    def string(self):
        size = self.u32()
        assert 1 <= size <= 1025
        data = self.take(size)
        assert data[-1:] == b'\0' and b'\0' not in data[:-1]
        return data[:-1].decode('ascii')


def directory(cursor):
    count = cursor.u32()
    assert count <= 2048
    rows = {}
    dirs = []
    for _ in range(count):
        prefix = cursor.string()
        assert prefix.endswith('/') and prefix not in dirs
        dirs.append(prefix)
        n = cursor.u32()
        assert n <= 256
        files = []
        for _ in range(n):
            name = cursor.string()
            assert name not in files and '/' not in name
            files.append(name)
            path = ('' if prefix == '/' else prefix) + name
            assert path not in rows
            rows[path] = cursor.u32()
        assert files == sorted(files)
    assert dirs == sorted(dirs)
    return rows, dirs


def inspect(path, expected, mount, seed):
    data = path.read_bytes()
    assert 221 <= len(data) <= 16384
    footer = Cursor(data[-221:])
    assert footer.take(17) == bytes(17)
    assert footer.u32() == 0x5a6f12e1 and footer.u32() == 11
    offset, size = footer.u64(), footer.u64()
    primary_hash = footer.take(20)
    assert footer.take(160) == bytes(160) and footer.at == 221
    assert offset <= len(data) - 221 and size <= len(data) - 221 - offset
    primary = data[offset:offset + size]
    assert hashlib.sha1(primary).digest() == primary_hash
    c = Cursor(primary)
    assert c.string() == mount and c.u32() == len(expected) and c.u64() == seed
    assert c.u32() == 1
    po, ps, ph = c.u64(), c.u64(), c.take(20)
    assert c.u32() == 1
    do, ds, dh = c.u64(), c.u64(), c.take(20)
    encoded = c.take(c.u32())
    assert len(encoded) == 12 * len(expected) and c.u32() == 0 and c.at == len(primary)
    assert po == offset + size and do == po + ps and do + ds == len(data) - 221
    assert hashlib.sha1(data[po:po+ps]).digest() == ph
    assert hashlib.sha1(data[do:do+ds]).digest() == dh
    path_index = Cursor(data[po:po+ps])
    assert path_index.u32() == len(expected)
    pairs = [ (path_index.u64(), path_index.u32()) for _ in expected ]
    pruned, pruned_dirs = directory(path_index)
    assert path_index.at == ps
    full_cursor = Cursor(data[do:do+ds])
    full, full_dirs = directory(full_cursor)
    assert full_cursor.at == ds and full == pruned and full_dirs == pruned_dirs
    assert set(full) == set(expected)
    assert set(full.values()) == set(range(0, len(encoded), 12))
    want_pairs = [(fnv(name, seed), full[name]) for name in sorted(expected)]
    assert pairs == want_pairs and len(set(h for h, _ in pairs)) == len(expected)
    payloads = []
    end = 0
    for name in sorted(expected):
        flags, start, length = struct.unpack_from('<III', encoded, full[name])
        assert flags == 0xe0000000 and start == end and length == len(expected[name])
        header = data[start:start+53]
        assert len(header) == 53
        placeholder, stored, decoded, method = struct.unpack_from('<QQQI', header)
        assert placeholder == 0 and stored == decoded == length and method == 0
        assert header[48:] == bytes(5)
        body = data[start+53:start+53+length]
        assert body == expected[name] and hashlib.sha1(body).digest() == header[28:48]
        end = start + 53 + length
        payloads.append({'path': name, 'bytes': length, 'sha256': hashlib.sha256(body).hexdigest()})
    assert end == offset
    return {'artifact': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'mount': mount, 'path_hash_seed': seed, 'files': len(full), 'directories': len(full_dirs),
            'full_pruned_maps_match': True, 'path_pairs_match': True, 'payloads': payloads}


def main():
    if len(sys.argv) != 2:
        raise SystemExit('one generated output directory required')
    out = Path(sys.argv[1]).resolve()
    records = [inspect(out/'prepared.pak', EXPECTED, '../../../', 0xf375bbbd),
               inspect(out/'custom.pak', EXPECTED, '../Shared/', (1 << 64) - 1),
               inspect(out/'empty.pak', {}, '../../../', 0xf375bbbd)]
    original = (out/'prepared.pak').read_bytes()
    mutated = bytearray(original)
    mutated[53] ^= 1  # First sorted physical record's actual Icon.bin payload.
    (out/'payload-corrupt.pak').write_bytes(mutated)
    assert original == (out/'prepared.pak').read_bytes()
    (out/'independent-geometry.json').write_text(json.dumps({'result': 'PASS', 'records': records,
        'corrupt_control': {'path': str(out/'payload-corrupt.pak'), 'offset': 53,
                            'sha256': hashlib.sha256(mutated).hexdigest()},
        'engine_acceptance_verified': False, 'authenticity_verified': False}, indent=2)+'\n')
    print('independent-geometry archives=3 exact_maps=true pairs=true payload_sha1=true')


if __name__ == '__main__':
    main()
