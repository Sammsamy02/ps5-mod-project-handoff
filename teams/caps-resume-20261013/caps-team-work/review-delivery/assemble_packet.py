"""Package already-reviewed isolated work; do not compile, integrate or contact a target."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs')
PREFIX = 'GPT-7-10-MAC-FINAL'
DAY = '2026-10-13'
selected = {}
verified = []

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

def add(path, name=None):
    path = Path(path)
    assert path.is_file() and not path.is_symlink(), path
    if name is None:
        name = str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else path.name
    assert not name.startswith('/') and '..' not in Path(name).parts
    assert name not in selected or selected[name] == path
    selected[name] = path

def from_manifest(component, manifest, include=lambda p: True):
    base = ROOT/'caps-team-work'/component
    file = base/manifest
    data = json.loads(file.read_text())
    for row in data['files']:
        path = Path(row.get('source', row.get('path')))
        if not path.is_absolute():
            path = base/path
        raw = path.read_bytes()
        assert len(raw) == row['bytes'] and digest(raw) == row['sha256'], path
        assert path.is_relative_to(base), path
        if include(path):
            packet_name = str(base.relative_to(ROOT)/row['packet_path']) if 'packet_path' in row else None
            add(path, packet_name)
    add(file)
    verified.append({'component': component, 'manifest': str(file.relative_to(ROOT)),
                     'sha256': digest(file.read_bytes()), 'files_checked': len(data['files'])})

old = json.loads((OUT/f'GPT-7-10-REVIEW-MANIFEST-{DAY}.json').read_text())
for row in old['files']:
    name = row['path']
    if name.startswith(('caps-team-work/cap07-step2/', 'caps-team-work/cap10/')) and '/derived/' not in name:
        add(ROOT/name)

from_manifest('cap07-step2', 'BUILD-INTEGRATION-RECEIPT.json')
stage = ROOT/'caps-team-work/cap07-step2/derived/review-final'
for row in json.loads((stage/'INTEGRATION.json').read_text())['files']:
    path = stage/row['path']
    assert digest(path.read_bytes()) == row['sha256'] and path.stat().st_size == row['bytes']
    add(path)
from_manifest('cap10', 'CURRENT-SOURCE-MANIFEST.json')
from_manifest('cap10-mono-build', 'evidence/HANDOFF-FILES.json')
from_manifest('cap10-native-deps', 'evidence/HANDOFF-FILES.json')
from_manifest('cap10-static-corlib-host', 'evidence/HANDOFF-FILES.json')
from_manifest('cap10-cross-aot', 'HANDOFF-MANIFEST.json',
              lambda p: p.stat().st_size < 500_000 or p.name in ('mscorlib.full-interp.ps5.o', 'mscorlib.dll'))
from_manifest('cap10-port-closure', 'HANDOFF-MANIFEST.json')

for name in ['cap10-port-research/LOCAL-INVENTORY.json', 'cap10-mono-build/logs/OWNER-CHECK.log',
             'cap10-mono-build/logs/OWNER-CORLIB-AOT.log', 'cap10-mono-build/logs/OWNER-CORLIB-AOT.THREADS2-FAIL.log',
             'cap10-mono-build/logs/OWNER-COMPILE-LINK.log']:
    add(ROOT/'caps-team-work'/name)
add(Path(__file__).resolve())
handoff = OUT/f'{PREFIX}-HANDOFF-{DAY}.md'
add(handoff)

manifest = {'scope': 'Reviewed Mac component sources, target artifacts and evidence; no console or PRX proof',
            'component_manifests_verified': verified, 'files': [],
            'excluded': ['Large upstream source/bootstrap/build trees (retained locally with reproduction recipes)',
                         'Host runtime/compiler binaries', 'Target link-only executables',
                         'Duplicate corlib archive and generated 114 MB assembly text',
                         'SDK/LLVM installation, game files and proprietary SDK material']}
for name, path in sorted(selected.items()):
    raw = path.read_bytes()
    manifest['files'].append({'path': name, 'bytes': len(raw), 'sha256': digest(raw)})
manifest_path = OUT/f'{PREFIX}-MANIFEST-{DAY}.json'
with manifest_path.open('x') as stream:
    json.dump(manifest, stream, indent=2)
    stream.write('\n')
archive = OUT/f'{PREFIX}-PACKET-{DAY}.zip'
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as packet:
    for name, path in sorted(selected.items()):
        packet.write(path, name)
    packet.write(manifest_path, manifest_path.name)
with zipfile.ZipFile(archive) as packet:
    assert packet.testzip() is None
    assert len(packet.namelist()) == len(set(packet.namelist())) == len(selected)+1
    for row in manifest['files']:
        raw = packet.read(row['path'])
        assert len(raw) == row['bytes'] and digest(raw) == row['sha256'], row['path']
    assert packet.read(manifest_path.name) == manifest_path.read_bytes()
receipt = {'archive': archive.name, 'bytes': archive.stat().st_size,
           'sha256': digest(archive.read_bytes()), 'members': len(selected)+1,
           'crc_verified': True, 'all_member_hashes_verified': True,
           'manifest_sha256': digest(manifest_path.read_bytes()),
           'uncompressed_file_bytes': sum(row['bytes'] for row in manifest['files']),
           'target_execution': False, 'prx_build_or_deploy': False, 'main_tree_modified': False}
with (OUT/f'{PREFIX}-ARCHIVE-RECEIPT-{DAY}.json').open('x') as stream:
    json.dump(receipt, stream, indent=2)
    stream.write('\n')
print(json.dumps(receipt, indent=2))
