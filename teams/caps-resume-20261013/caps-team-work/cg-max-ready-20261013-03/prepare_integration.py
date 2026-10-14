#!/usr/bin/env python3
"""Pin and dry-run the #13 construction overlay without writing to the candidate."""
from __future__ import annotations

import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
BASE = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4')
FILES = (
    'include/Mod/NativeModAbi.h',
    'include/Platform/NativeModHostCore.hpp',
    'candidate/src/Mod/NativeModEngineServices.inc',
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    patches = []
    rows = []
    for rel in FILES:
        source_rel = rel.removeprefix('candidate/')
        base_data = (BASE / rel).read_bytes()
        staged_data = (ROOT / 'cap13/construct' / source_rel).read_bytes()
        base_text = base_data.decode('utf-8').splitlines(keepends=True)
        staged_text = staged_data.decode('utf-8').splitlines(keepends=True)
        patches.extend(difflib.unified_diff(base_text, staged_text,
                       fromfile='a/' + rel, tofile='b/' + rel))
        rows.append({'relative_path': rel, 'base_sha256': digest(base_data),
                     'staged_sha256': digest(staged_data)})
    manifest_path = ROOT / 'integration-manifest.json'
    if manifest_path.exists():
        pinned = json.loads(manifest_path.read_text())['files']
        assert rows == pinned, 'candidate or staged source diverged from pinned integration manifest'
    patch_text = ''.join(patches)
    assert patch_text and all(row['base_sha256'] != row['staged_sha256'] for row in rows)
    patch_path = ROOT / 'construct-object-integration.patch'
    patch_path.write_text(patch_text)
    with tempfile.TemporaryDirectory(dir=ROOT / 'evidence') as temp:
        scratch = Path(temp)
        for rel in FILES:
            target = scratch / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((BASE / rel).read_bytes())
        run = subprocess.run(['patch', '-p1', '--batch', '--forward'], cwd=scratch,
                             input=patch_text, text=True, capture_output=True)
        assert run.returncode == 0, run.stdout + run.stderr
        for row in rows:
            assert digest((scratch / row['relative_path']).read_bytes()) == row['staged_sha256']
    manifest = {'scope': 'Mac-only #13 construction overlay; candidate read-only',
                'base_root': str(BASE), 'patch_sha256': digest(patch_path.read_bytes()),
                'files': rows, 'scratch_patch_apply': 'PASS'}
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f"integration preview: {len(rows)} files; scratch patch apply PASS")


if __name__ == '__main__':
    main()
