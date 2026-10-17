"""Build and verify an incremental, exact-base GS audit patch."""

import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
BASE = ROOT / 'cg-universal-admission-20261013-05'
STAGE = ROOT / 'cg-teb-audit-20261013-06'
OUTPUTS = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
WORK = OUTPUTS.parent / 'work'
PATCH = OUTPUTS / 'pe-analyzer-gs-audit-increment-20261013.patch'
MANIFEST = OUTPUTS / 'pe-analyzer-gs-audit-manifest-20261013.json'
ZIP = OUTPUTS / 'pe-analyzer-gs-audit-evidence-20261013.zip'
FILES = [
    'TOOL/pe_analyzer.py',
    'TOOL/admission_dossier.py',
    'TOOL/loader_capabilities.json',
    'TOOL/gs_audit.py',
    'TOOL/test_teb_audit.py',
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

diffs = []
manifest = {'patch_base': str(BASE), 'staged_source': str(STAGE),
            'applies_after': 'pe-analyzer-universal-admission-20261013.patch',
            'requires_offline_decoder': 'Python Capstone 5.0.7',
            'scope': 'read-only candidate audit; loader runtime enforcement and GS semantics remain unimplemented',
            'files': {}}
for relative in FILES:
    old = BASE / relative
    new = STAGE / relative
    old_text = old.read_text().splitlines(keepends=True) if old.exists() else []
    new_text = new.read_text().splitlines(keepends=True)
    diffs.extend(difflib.unified_diff(old_text, new_text,
                 fromfile='a/' + relative if old.exists() else '/dev/null',
                 tofile='b/' + relative))
    manifest['files'][relative] = {'base_sha256': sha(old) if old.exists() else None,
                                   'staged_sha256': sha(new)}
PATCH.write_text(''.join(diffs))
manifest['patch_sha256'] = sha(PATCH)

with tempfile.TemporaryDirectory(prefix='gs-audit-patch-', dir=WORK) as directory:
    scratch = Path(directory)
    shutil.copytree(BASE / 'TOOL', scratch / 'TOOL')
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
    for path in (PATCH, MANIFEST, OUTPUTS / 'DLL-GS-ACCESS-AUDIT-20261013.json',
                 STAGE / 'REPORT/evidence/gs-audit-focused-tests.log',
                 STAGE / 'REPORT/evidence/gs-audit-analyzer-regression.log'):
        archive.write(path, arcname=path.name)
print('verified patch against exact previous stage; wrote manifest and evidence ZIP')
