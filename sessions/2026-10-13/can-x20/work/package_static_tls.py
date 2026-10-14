"""Build and verify an exact-base static TLS core patch."""

import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
BASE = ROOT / 'cg-gs-guard-20261013-07'
STAGE = ROOT / 'cg-static-tls-core-20261013-09'
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
PATCH = OUT / 'static-tls-core-increment-20261013.patch'
MANIFEST = OUT / 'static-tls-core-manifest-20261013.json'
ZIP = OUT / 'static-tls-core-evidence-20261013.zip'
FILES = [
    'loader/src/static_tls_bank.h',
    'loader/src/static_tls_bank.cpp',
    'loader/src/pe_tls_registration.h',
    'loader/src/pe_tls_registration.cpp',
    'loader/tests/static_tls_bank_test.cpp',
    'loader/tests/pe_tls_registration_test.cpp',
    'loader/tests/run_static_tls.py',
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

diffs = []
manifest = {
    'patch_base': str(BASE), 'staged_source': str(STAGE),
    'applies_after': 'pe-loader-gs-guard-increment-20261013.patch',
    'scope': 'platform-neutral per-thread TLS blocks and mapped-image registration; no guest GS or callbacks',
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
with tempfile.TemporaryDirectory(prefix='static-tls-patch-', dir=OUT.parent / 'work') as directory:
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
    for path in (PATCH, MANIFEST, OUT / 'STATIC-TLS-CORE-OFFLINE-20261013.md',
                 STAGE / 'REPORT/evidence/static-tls-bank-host.log'):
        archive.write(path, arcname=path.name)
print('verified static TLS patch against exact guarded loader stage')
