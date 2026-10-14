"""Build an exact-base patch for the unchanged-DLL TLS host harness."""

import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
BASE = ROOT / 'cg-static-tls-core-20261013-09'
STAGE = ROOT / 'cg-real-tls-evidence-20261013-10'
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
PATCH = OUT / 'real-dll-static-tls-evidence-increment-20261013.patch'
MANIFEST = OUT / 'real-dll-static-tls-evidence-manifest-20261013.json'
ZIP = OUT / 'real-dll-static-tls-evidence-20261013.zip'
FILES = ['loader/tests/real_tls_prepare.cpp', 'loader/tests/run_real_tls_prepare.py']

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

diffs = []
manifest = {
    'patch_base': str(BASE), 'staged_source': str(STAGE),
    'applies_after': 'static-tls-core-increment-20261013.patch',
    'scope': 'read-only five-DLL host TLS preparation; no guest execution',
    'files': {},
}
for relative in FILES:
    new = STAGE / relative
    assert new.is_file() and not (BASE / relative).exists()
    diffs.extend(difflib.unified_diff([], new.read_text().splitlines(keepends=True),
                 fromfile='/dev/null', tofile='b/' + relative))
    manifest['files'][relative] = {'base_sha256': None, 'staged_sha256': sha(new)}
PATCH.write_text(''.join(diffs))
manifest['patch_sha256'] = sha(PATCH)
with tempfile.TemporaryDirectory(prefix='real-tls-patch-', dir=OUT.parent / 'work') as directory:
    scratch = Path(directory)
    shutil.copytree(BASE / 'loader', scratch / 'loader')
    proc = subprocess.run(['patch', '--batch', '--forward', '-p1', '-d', str(scratch),
                           '-i', str(PATCH)], capture_output=True, text=True)
    if proc.returncode:
        raise RuntimeError(proc.stdout + proc.stderr)
    for relative in FILES:
        if (scratch / relative).read_bytes() != (STAGE / relative).read_bytes():
            raise RuntimeError('patch byte mismatch: ' + relative)
    manifest['scratch_patch_reproduces_staged_bytes'] = True
MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n')
with zipfile.ZipFile(ZIP, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for path in (PATCH, MANIFEST, OUT / 'REAL-DLL-STATIC-TLS-OFFLINE-20261013.md',
                 OUT / 'REAL-DLL-STATIC-TLS-PREPARE-20261013.json',
                 STAGE / 'REPORT/evidence/real-static-tls-prepare.log',
                 STAGE / 'REPORT/evidence/static-tls-core-regression.log'):
        archive.write(path, arcname=path.name)
print('verified unchanged-DLL TLS harness patch against exact static core stage')
