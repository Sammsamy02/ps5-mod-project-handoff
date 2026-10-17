#!/usr/bin/env python3
"""Make an exact-base rollup from context bridge stage 14 to stage 23."""
import difflib
import hashlib
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
BASE = ROOT / 'cg-context-bridge-20261013-14'
STAGE = ROOT / 'cg-tls-callback-exec-gate-20261013-23'
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
NAME = 'imports-teb-tls-15-23-rollup-20261013'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sources(stage):
    return {p.relative_to(stage) for p in (stage / 'loader').rglob('*')
            if p.is_file() and p.suffix in {'.cpp', '.h', '.S', '.py'}}


files = []
for rel in sorted(sources(BASE) | sources(STAGE)):
    old = (BASE / rel).read_bytes() if (BASE / rel).exists() else None
    new = (STAGE / rel).read_bytes() if (STAGE / rel).exists() else None
    if old != new:
        if new is None:
            raise SystemExit(f'unexpected deletion: {rel}')
        files.append(rel)

patch_chunks = []
manifest = {'base_stage': str(BASE), 'staged_stage': str(STAGE), 'files': {}}
for rel in files:
    old = (BASE / rel).read_bytes() if (BASE / rel).exists() else None
    new = (STAGE / rel).read_bytes()
    before = old.decode('utf-8').splitlines(keepends=True) if old is not None else []
    after = new.decode('utf-8').splitlines(keepends=True)
    patch_chunks.append(''.join(difflib.unified_diff(
        before, after,
        fromfile='a/' + str(rel) if old is not None else '/dev/null',
        tofile='b/' + str(rel))))
    manifest['files'][str(rel)] = {
        'base_sha256': sha(old) if old is not None else 'absent',
        'staged_sha256': sha(new), 'size': len(new)}

patch_bytes = ''.join(patch_chunks).encode('utf-8')
patch_path = OUT / (NAME + '.patch')
patch_path.write_bytes(patch_bytes)
manifest['patch_sha256'] = sha(patch_bytes)
manifest['changed_file_count'] = len(files)

with tempfile.TemporaryDirectory(prefix='cap13-15-23-rollup-replay-') as scratch:
    scratch_root = Path(scratch)
    for rel in files:
        source = BASE / rel
        if source.exists():
            target = scratch_root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
    for argv in (['git', 'apply', '--check', str(patch_path)],
                 ['git', 'apply', str(patch_path)]):
        result = subprocess.run(argv, cwd=scratch, capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stderr)
    for rel in files:
        if (scratch_root / rel).read_bytes() != (STAGE / rel).read_bytes():
            raise SystemExit(f'replay mismatch: {rel}')

manifest_path = OUT / (NAME + '-manifest.json')
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
zip_path = OUT / (NAME + '-evidence.zip')
latest = OUT / 'tls-callback-exec-gate-evidence-20261013.zip'
with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as out_zip:
    out_zip.write(patch_path, patch_path.name)
    out_zip.write(manifest_path, manifest_path.name)
    out_zip.writestr('logs/rollup-replay.txt',
        f'git apply --check: PASS\ngit apply: PASS\n'
        f'{len(files)} files byte-identical to stage 23: PASS\n')
    with zipfile.ZipFile(latest) as prior:
        for name in prior.namelist():
            if name.startswith('logs/'):
                out_zip.writestr(name, prior.read(name))

print(json.dumps({'patch': str(patch_path), 'manifest': str(manifest_path),
                  'evidence_zip': str(zip_path), 'changed_files': len(files),
                  'patch_sha256': manifest['patch_sha256']}, indent=2))
