#!/usr/bin/env python3
"""Package isolated guest worker lifecycle from the exact stage-42 base."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile

workspace = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20')
team = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
base = team / 'cg-guest-thread-handles-20261013-47'
stage = team / 'cg-guest-write-map-20261013-48'
outputs = workspace / 'outputs'
changed = [
    'loader/include/guest_write_map.h',
    'loader/src/win_api_shim.h',
    'loader/src/win_api_shim.cpp',
    'loader/tests/guest_pe_thread_test.cpp',
]

def sha(data):
    return hashlib.sha256(data).hexdigest()

source_suffixes = {'.cpp', '.h', '.py', '.S'}
actual = set()
for p in stage.rglob('*'):
    if not p.is_file() or p.suffix not in source_suffixes or 'build' in p.parts:
        continue
    rel = p.relative_to(stage).as_posix()
    old = base / rel
    if not old.exists() or old.read_bytes() != p.read_bytes():
        actual.add(rel)
if actual != set(changed):
    raise SystemExit(f'unexpected changed sources: {sorted(actual ^ set(changed))}')

parts = []
files = {}
for rel in changed:
    old = base / rel
    new = stage / rel
    before = old.read_bytes() if old.exists() else None
    after = new.read_bytes()
    if not after.endswith(b'\n') or (before is not None and not before.endswith(b'\n')):
        raise SystemExit(f'non-newline source needs git no-newline patch: {rel}')
    parts.extend(difflib.unified_diff(
        before.decode().splitlines(keepends=True) if before is not None else [],
        after.decode().splitlines(keepends=True),
        fromfile=f'a/{rel}' if before is not None else '/dev/null',
        tofile=f'b/{rel}', lineterm='\n'))
    files[rel] = {'base_sha256': sha(before) if before is not None else None,
                  'stage_sha256': sha(after), 'size': len(after)}
patch = outputs / 'guest-write-map-increment-20261013.patch'
patch.write_text(''.join(parts))

focused = workspace / 'work/guest-write-stage48-focused.log'
asan = workspace / 'work/guest-write-stage48-asan.log'
regression_dir = workspace / 'work/stage48-regressions'
target_manifest = outputs / 'guest-write-map-target-build-manifest-20261013.json'
target_zip = outputs / 'guest-write-map-target-build-evidence-20261013.zip'
if 'TOTAL: checks=41 failures=0' not in focused.read_text() or \
   'TOTAL: checks=41 failures=0' not in asan.read_text():
    raise SystemExit('focused host or ASan check failed')
for p in regression_dir.glob('*.log'):
    if 'exit=0' not in p.read_text():
        raise SystemExit(f'regression did not exit zero: {p.name}')
regression_logs = sorted(regression_dir.glob('run_*.py.log'))
if len(regression_logs) != 15 or \
   'RESULT: PASS (11 checks, 0 failed)' not in \
       (regression_dir / 'e2e-run.log').read_text():
    raise SystemExit('15 prior runners or e2e fixture missing')
target = json.loads(target_manifest.read_text())
if target['stage'] != str(stage) or target['source_count'] != 15 or \
   not target['all_compile_exit_zero'] or not target['full_link_exit_zero']:
    raise SystemExit('target build manifest does not cover stage 48')
with zipfile.ZipFile(target_zip) as archive:
    if archive.testzip(): raise SystemExit('target ZIP CRC failure')

fixture = team / 'cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
fixture_hash = 'acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce'
if sha(fixture.read_bytes()) != fixture_hash:
    raise SystemExit('original fixture changed')

manifest = outputs / 'guest-write-map-manifest-20261013.json'
manifest.write_text(json.dumps({
    'status': 'offline only; no console guest thread execution',
    'base_stage': str(base), 'stage': str(stage),
    'changed_files': files, 'patch_sha256': sha(patch.read_bytes()),
    'fixture_sha256': fixture_hash,
    'verification': {'focused_host': '41/41', 'focused_asan': '41/41',
                     'prior_host_runners': '15/15 exited 0',
                     'synthetic_e2e': '11/11',
                     'ps5_target_sources': 15, 'ps5_target_link': 'PASS',
                     'console_executed': False},
    'target_build_manifest_sha256': sha(target_manifest.read_bytes()),
}, indent=2) + '\n')

with tempfile.TemporaryDirectory(prefix='stage48-replay-', dir=workspace / 'work') as temp:
    scratch = Path(temp) / 'base'
    shutil.copytree(base, scratch)
    for argv in (['git', 'apply', '--check', str(patch)],
                 ['git', 'apply', str(patch)]):
        result = subprocess.run(argv, cwd=scratch, capture_output=True,
                                text=True, timeout=60)
        if result.returncode:
            raise SystemExit(f'{argv}: {result.stderr}')
    for rel in changed:
        if (scratch / rel).read_bytes() != (stage / rel).read_bytes():
            raise SystemExit(f'exact replay mismatch: {rel}')

archive_path = outputs / 'guest-write-map-host-evidence-20261013.zip'
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch, patch.name)
    archive.write(manifest, manifest.name)
    archive.write(focused, 'evidence/focused.log')
    archive.write(asan, 'evidence/asan.log')
    for log in sorted(regression_dir.glob('*.log')):
        archive.write(log, 'evidence/regressions/' + log.name)
    archive.writestr('evidence/replay.txt',
                     'git apply --check: PASS\ngit apply: PASS\n'
                     'four changed sources: exact-byte replay\n'
                     'original fixture hash: unchanged\n')
with zipfile.ZipFile(archive_path) as archive:
    if archive.testzip(): raise SystemExit('host ZIP CRC failure')
print('changed sources:', len(changed))
print('patch:', patch.stat().st_size, sha(patch.read_bytes()))
print('exact-base replay: PASS')
print('target and host ZIP CRC: PASS')
