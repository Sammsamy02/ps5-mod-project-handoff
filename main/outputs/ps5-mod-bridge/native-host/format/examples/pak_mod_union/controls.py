#!/usr/bin/env python3
"""Focused ordinary-host controls; creates only authored, tiny PAK fixtures."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


def make_pak(path, mount, entries, corrupt=False):
    def string(value):
        data = value.encode('ascii') + b'\0'
        return struct.pack('<i', len(data)) + data

    def header(offset, data):
        return (struct.pack('<QQQI', offset, len(data), len(data), 0)
                + hashlib.sha1(data).digest() + struct.pack('<BI', 0, 0))

    body = bytearray()
    rows = []
    for name, data in entries:
        rows.append((name, header(len(body), data)))
        body += header(0, data) + data
    index = (string(mount) + struct.pack('<I', len(rows))
             + b''.join(string(name) + metadata for name, metadata in rows))
    result = (body + index + struct.pack('<IIQQ', 0x5a6f12e1, 3, len(body), len(index))
              + hashlib.sha1(index).digest())
    if corrupt:
        result[53] ^= 1
    path.write_bytes(result)


def run(argv, directory, name):
    proc = subprocess.run([str(a) for a in argv], capture_output=True, timeout=30)
    (directory / (name + '.stdout')).write_bytes(proc.stdout)
    (directory / (name + '.stderr')).write_bytes(proc.stderr)
    return proc


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    if len(sys.argv) != 5:
        raise SystemExit('usage: controls.py MERGER TRANSCODER REPAK FRESH_DIRECTORY')
    merger, transcode, repak = map(lambda x: Path(x).resolve(strict=True), sys.argv[1:4])
    directory = Path(sys.argv[4]).resolve()
    directory.mkdir(exist_ok=False)
    base_rows = [('Project/Config/Base.ini', b'base=original\n'),
                 ('Project/Content/Textures/T.bin', b'unchanged texture test bytes')]
    mod_rows = [('Config/Base.ini', b'base=modified\n'),
                ('Content/New.bin', b'added test asset')]
    make_pak(directory / 'base-v3.pak', '../../../', base_rows)
    make_pak(directory / 'mod-v3.pak', '../../../Project/', mod_rows)
    original_base = (directory / 'base-v3.pak').read_bytes()
    proc = run([transcode, directory / 'base-v3.pak', directory / 'base-v11.pak', '1234'],
               directory, 'prepare-base')
    require(proc.returncode == 0, 'fixture base transcode failed')
    base = directory / 'base-v11.pak'
    before = base.read_bytes()
    output = directory / 'merged.pak'
    proc = run([merger, base, directory / 'mod-v3.pak', output], directory, 'replace-add')
    require(proc.returncode == 0, 'replacement/addition merge failed')
    report = json.loads(proc.stdout)
    require(report['artifact_complete'], 'merge did not complete')
    expected = {'../../../Project/Config/Base.ini': b'base=modified\n',
                '../../../Project/Content/Textures/T.bin': base_rows[1][1],
                '../../../Project/Content/New.bin': b'added test asset'}
    proc = run([repak, 'hash-list', '--strip-prefix', '', output], directory, 'decoded-hashes')
    require(proc.returncode == 0, 'independent extraction failed')
    actual = {}
    for line in proc.stdout.decode().splitlines():
        digest, path = line.split(' ', 1)
        require(path not in actual, 'duplicate oracle path')
        actual[path] = digest
    require(actual == {k: hashlib.sha256(v).hexdigest() for k, v in expected.items()},
            'replacement/addition contents or paths differ')
    proc = run([repak, 'info', output], directory, 'info')
    require(proc.returncode == 0 and b'000004d2' in proc.stdout.lower(), 'base seed not retained')
    cases = [
        ('wrong-parent-depth', '../../Project/', [('Config/Base.ini', b'mod')], False),
        ('case-file-alias', '../../../Project/', [('config/Base.ini', b'mod')], False),
        ('case-directory-alias', '../../../Project/', [('config/New.ini', b'mod')], False),
        ('file-directory-alias', '../../../Project/', [('Content', b'mod')], False),
        ('bad-payload-sha1', '../../../Project/', [('Config/Base.ini', b'mod')], True),
    ]
    refusals = []
    for name, mount, entries, corrupt in cases:
        mod = directory / (name + '.pak')
        make_pak(mod, mount, entries, corrupt)
        proc = run([merger, base, mod, directory / (name + '-output.pak')], directory, name)
        result = json.loads(proc.stdout)
        require(proc.returncode != 0 and not result['artifact_complete'], name + ' was not refused')
        refusals.append({'case': name, 'stage': result.get('stage'), 'error': result.get('error')})
    completed = output.read_bytes()
    proc = run([merger, base, directory / 'mod-v3.pak', output], directory, 'existing-output')
    require(proc.returncode != 0 and output.read_bytes() == completed,
            'existing output was not preserved')
    require(base.read_bytes() == before and (directory / 'base-v3.pak').read_bytes() == original_base,
            'source base changed')
    result = {'status': 'pass', 'replacement_and_addition_exact': True,
              'preserved_base_hash_seed': 1234, 'refusals': refusals,
              'existing_output_preserved': True, 'source_bases_unchanged': True,
              'console_actions': False}
    (directory / 'receipt.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
