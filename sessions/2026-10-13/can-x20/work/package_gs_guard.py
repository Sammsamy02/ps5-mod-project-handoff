"""Build and verify an exact-base Mac loader GS guard patch."""

import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
BASE = ROOT / 'cg-console-wait-20261013-04'
STAGE = ROOT / 'cg-gs-guard-20261013-07'
OUTPUTS = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
PATCH = OUTPUTS / 'pe-loader-gs-guard-increment-20261013.patch'
MANIFEST = OUTPUTS / 'pe-loader-gs-guard-manifest-20261013.json'
ZIP = OUTPUTS / 'pe-loader-gs-guard-evidence-20261013.zip'
FILES = ['loader/src/pe_loader.cpp', 'loader/include/pe_loader.h',
         'loader/src/pe_types.h', 'loader/tests/test_preflight.cpp',
         'loader/tests/run_preflight.py']

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

diffs = []
manifest = {'patch_base': str(BASE), 'staged_source': str(STAGE),
            'applies_after': 'pe-loader-hardening-integration-20261013.patch',
            'scope': 'conservative executable-byte 0x65 refusal; no TEB emulation',
            'false_positive_limit': 'embedded code data or immediates containing 0x65 are refused',
            'files': {}}
for relative in FILES:
    old = BASE / relative
    new = STAGE / relative
    diffs.extend(difflib.unified_diff(old.read_text().splitlines(keepends=True),
                 new.read_text().splitlines(keepends=True),
                 fromfile='a/' + relative, tofile='b/' + relative))
    manifest['files'][relative] = {'base_sha256': sha(old), 'staged_sha256': sha(new)}
PATCH.write_text(''.join(diffs))
manifest['patch_sha256'] = sha(PATCH)
with tempfile.TemporaryDirectory(prefix='gs-guard-patch-', dir=OUTPUTS.parent / 'work') as directory:
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
    for path in (PATCH, MANIFEST, STAGE / 'REPORT/evidence/gs-prefix-preflight.log',
                 OUTPUTS.parent / 'work/gs-guard-e2e-manifest.json',
                 OUTPUTS.parent / 'work/gs-guard-e2e-results-r2/RESULT.json',
                 OUTPUTS.parent / 'work/gs-guard-e2e-results-r2/fixture-r3.log',
                 OUTPUTS.parent / 'work/gs-guard-e2e-results-r2/zero-real-tls-r3.log',
                 OUTPUTS.parent / 'work/gs-guard-e2e-results-r2/tfw-real-tls-r3.log'):
        archive.write(path, arcname=path.name)
print('verified guard patch against exact previous loader stage; wrote manifest and evidence ZIP')
