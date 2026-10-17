#!/usr/bin/env python3
"""Package local review evidence only; no console or network operations."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
def sha(data):
    return hashlib.sha256(data).hexdigest()

stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
packet = HERE / ('claude-review-' + stamp)
packet.mkdir(exist_ok=False)
origins = []
def copy(source, target, expected=None):
    data = source.read_bytes()
    digest = sha(data)
    if expected is not None:
        assert digest == expected, str(source)
    dest = packet / target
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        assert dest.read_bytes() == data
    else:
        dest.write_bytes(data)
    origins.append({'original': str(source), 'packet': str(target), 'bytes': len(data), 'sha256': digest})

for name in ['REVIEW-REPORT.md', 'PARENT-ALLOCATOR-REPRODUCTION.json', 'PARENT-HEADER-REPRODUCTION.json', 'PROGRESS.md']:
    copy(HERE / name, Path(name))
for section in ['allocator', 'inventory', 'mod-api', 'upstream']:
    for src in sorted((HERE / section).rglob('*')):
        if not src.is_file():
            continue
        rel = src.relative_to(HERE)
        if '__pycache__' in rel.parts or src.name == 'selected-map-readers.raw.json':
            continue
        if section == 'allocator' and len(rel.parts) > 1 and rel.parts[1] in ['reproduced', 'parent-verification', 'postfetch-repeat', 'parent-postfetch']:
            continue
        assert src.suffix.lower() not in ['.prx', '.sprx', '.elf', '.o', '.a', '.dylib', '.exe', '.dll', '.bin'], str(src)
        copy(src, rel)

# Full selected text context lets the cloud reviewer inspect code, not just local links.
for pin in json.loads((HERE / 'mod-api/evidence-pins.json').read_text()):
    src = Path(pin['path'])
    copy(src, Path('source-context') / (pin['sha256'][:12] + '-' + src.name), pin['sha256'])
for change in json.loads((HERE / 'upstream/version-sensitive-changes.json').read_text()):
    for pin in change['evidence']:
        src = Path(pin['path'])
        copy(src, Path('source-context') / (pin['sha256'][:12] + '-' + src.name), pin['sha256'])

old = ROOT / 'outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/claude-review-20261008T143432Z-cb5e7f'
for name in ['UE4SS.log', 'runtime-status.txt', 'supplier-diag.this-launch.jsonl', 'klog.filtered.this-launch.txt', 'shadowmount.this-launch.txt', 'autotune.ini', 'main.lua', 'UE4SS-settings.ini', 'PS5Pad.ini', 'HARVEST-RECEIPT.json', 'UPLOAD-RECEIPT.json', 'REVIEW-TABLE.md']:
    copy(old / name, Path('prior-console-evidence') / name)

(packet / 'ORIGIN-INDEX.json').write_text(json.dumps(origins, indent=2) + '\n')
(packet / 'README.md').write_text('''# l21 cloud review packet

Start with REVIEW-REPORT.md. This is a Mac-only audit, not a new runtime build.
The authoritative allocator update is allocator/REPORT-SOURCE-CORROBORATION.md;
the authoritative object-constructor followup is mod-api/OFFICIAL-SCO-COMPARISON.md.
Older reports are retained for provenance and are explicitly superseded there.

Full lookup tables are in inventory/. The targeted startup/menu join is in mod-api/.
The source/API/version reviews include their pinned evidence and limitations.
ORIGIN-INDEX.json maps every original Mac path to its copy in this ZIP, so a cloud
reviewer can resolve absolute source citations without access to the Mac.

prior-console-evidence/ is the ALREADY SAVED PID184 launch, not a new capture or test.
source-context/ contains hash-checked text sources and selection/build receipts.
The game executable, PRX binaries, credentials, and the large redundant raw reader
intermediate are excluded. Reproduction of image measurements requires the user's
authenticated original game image locally; its hash and measurements are included.

The packet contains licensed engine source excerpts/files obtained through the
user's existing source access. Keep it within the user's authorized private review.
No compatibility or target execution claim follows from packaging this ZIP.
''')
manifest = []
for p in sorted(packet.rglob('*')):
    if p.is_file():
        data = p.read_bytes()
        manifest.append({'path': str(p.relative_to(packet)), 'bytes': len(data), 'sha256': sha(data)})
(packet / 'MANIFEST.json').write_text(json.dumps({'scope': 'local review packet', 'files': manifest}, indent=2) + '\n')
zpath = packet.with_suffix('.zip')
with zipfile.ZipFile(zpath, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for p in sorted(packet.rglob('*')):
        if p.is_file():
            z.write(p, str(p.relative_to(packet)))
with zipfile.ZipFile(zpath) as z:
    assert z.testzip() is None
    assert len(z.namelist()) == len(manifest) + 1
    for item in manifest:
        data = z.read(item['path'])
        assert len(data) == item['bytes'] and sha(data) == item['sha256'], item['path']
receipt = {'packet': str(packet), 'zip': str(zpath), 'zip_bytes': zpath.stat().st_size,
           'zip_sha256': sha(zpath.read_bytes()), 'manifest_files_verified': len(manifest),
           'zip_integrity': 'passed', 'console_contact': False, 'new_runtime_build': False}
(HERE / 'PACKET-RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
