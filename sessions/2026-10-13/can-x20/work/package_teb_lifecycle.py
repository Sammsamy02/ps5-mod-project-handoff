#!/usr/bin/env python3
"""Package the isolated guest TEB lifecycle increment and verify replay."""
import difflib
import hashlib
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path

BASE = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-real-tls-evidence-20261013-10')
STAGE = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-teb-lifecycle-20261013-11')
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
NAME = 'guest-teb-lifecycle'
FILES = [
    'loader/src/guest_teb_lifecycle.h',
    'loader/src/guest_teb_lifecycle.cpp',
    'loader/tests/teb_lifecycle_test.cpp',
    'loader/tests/run_teb_lifecycle.py',
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


logs = {}
for key, runner in [
    ('teb-lifecycle-host', 'loader/tests/run_teb_lifecycle.py'),
    ('static-tls-host', 'loader/tests/run_static_tls.py'),
    ('real-dll-tls-prepare', 'loader/tests/run_real_tls_prepare.py'),
]:
    result = subprocess.run(['python3', runner], cwd=STAGE, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120)
    logs[key + '.txt'] = result.stdout + '\nEXIT=' + str(result.returncode) + '\n'
    if result.returncode:
        raise SystemExit(key + ' failed:\n' + logs[key + '.txt'])

patch = []
manifest = {
    'base_stage': str(BASE),
    'staged_stage': str(STAGE),
    'base_relationship': 'four files are absent from the exact previous stage',
    'files': {},
}
for rel in FILES:
    assert not (BASE / rel).exists(), 'unexpected base file ' + rel
    body = (STAGE / rel).read_bytes()
    assert b'\x00' not in body
    new_lines = body.decode('utf-8').splitlines(keepends=True)
    chunk = ''.join(difflib.unified_diff([], new_lines,
                                         fromfile='/dev/null',
                                         tofile='b/' + rel))
    patch.append(chunk)
    manifest['files'][rel] = {
        'base': 'absent', 'staged_sha256': digest(body), 'size': len(body),
    }
patch_data = ''.join(patch).encode('utf-8')
patch_path = OUT / (NAME + '-increment-20261013.patch')
patch_path.write_bytes(patch_data)
manifest['patch_sha256'] = digest(patch_data)

with tempfile.TemporaryDirectory(prefix='cap13-teb-replay-') as scratch:
    result = subprocess.run(['git', 'apply', '--check', str(patch_path)],
                            cwd=scratch, text=True, capture_output=True)
    if result.returncode:
        raise SystemExit('git apply --check failed: ' + result.stderr)
    result = subprocess.run(['git', 'apply', str(patch_path)],
                            cwd=scratch, text=True, capture_output=True)
    if result.returncode:
        raise SystemExit('git apply failed: ' + result.stderr)
    for rel in FILES:
        assert (Path(scratch) / rel).read_bytes() == (STAGE / rel).read_bytes(), rel
logs['patch-replay.txt'] = 'git apply --check: PASS\ngit apply: PASS\n'
logs['patch-replay.txt'] += 'all four files byte-identical to isolated stage: PASS\n'

manifest_path = OUT / (NAME + '-manifest-20261013.json')
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
zip_path = OUT / (NAME + '-evidence-20261013.zip')
with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch_path, patch_path.name)
    archive.write(manifest_path, manifest_path.name)
    for log_name, body in logs.items():
        archive.writestr('logs/' + log_name, body)
print(json.dumps({
    'patch': str(patch_path), 'patch_sha256': manifest['patch_sha256'],
    'manifest': str(manifest_path), 'evidence_zip': str(zip_path),
    'checks': {key: value.count('PASS') for key, value in logs.items()},
}, indent=2))
