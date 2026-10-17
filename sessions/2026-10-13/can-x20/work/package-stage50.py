#!/usr/bin/env python3
"""Package the exact-stage-45 near-trampoline increment and offline evidence."""
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
base = team / 'cg-cap03-snapshot-guard-20261013-45'
stage = team / 'cg-cap03-near-trampoline-20261013-50'
outputs = workspace / 'outputs'
changed = [
    'src/NearTrampoline.hpp', 'src/NearTrampoline.cpp',
    'tests/near_trampoline_test.cpp', 'tests/run_near_trampoline.py',
]

def sha(data):
    return hashlib.sha256(data).hexdigest()

actual = set()
for file in stage.rglob('*'):
    if not file.is_file() or file.suffix not in {'.cpp', '.hpp', '.py', '.h'} or \
       'build' in file.relative_to(stage).parts:
        continue
    rel = file.relative_to(stage).as_posix()
    old = base / rel
    if not old.exists() or old.read_bytes() != file.read_bytes():
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
    if not after.endswith(b'\n'):
        raise SystemExit(f'non-newline source: {rel}')
    parts.extend(difflib.unified_diff(
        before.decode().splitlines(keepends=True) if before is not None else [],
        after.decode().splitlines(keepends=True),
        fromfile=f'a/{rel}' if before is not None else '/dev/null',
        tofile=f'b/{rel}', lineterm='\n'))
    files[rel] = {'base_sha256': sha(before) if before is not None else None,
                  'stage_sha256': sha(after), 'size': len(after)}
patch = outputs / 'cap03-near-trampoline-increment-20261013.patch'
patch.write_text(''.join(parts))

build = stage / 'build/ps5-obj'
evidence_files = {
    'host-plan-log': stage / 'build/host/detour/test-run.log',
    'host-near-log': stage / 'build/host/near/test-run.log',
    'host-near-asan-log': stage / 'build/host/near-asan/test-run.log',
    'host-protocol-log': build / 'host-protocol.log',
    'zydis-target-build-log': build / 'zydis.log',
    'planner-target-build-log': build / 'planner.log',
    'near-target-build-log': build / 'near.log',
    'codepatch-target-build-log': build / 'codepatch.log',
    'diagnostic-target-build-log': build / 'diagnostic.log',
    'link-log': build / 'link.log',
    'target-symbols': build / 'symbols.log',
    'near-undefined-symbols': build / 'near-undefined.log',
    'zydis-target-object': build / 'Zydis.o',
    'planner-target-object': build / 'DetourPlan.o',
    'near-target-object': build / 'NearTrampoline.o',
    'codepatch-target-object': build / 'CodePatch.o',
    'diagnostic-target-object': build / 'PatchDiagnostic.o',
    'offline-target-elf': build / 'cap03-detour-offline.elf',
}
for label, path in evidence_files.items():
    if not path.is_file():
        raise SystemExit(f'missing evidence: {label}')
if 'checks=24 failures=0' not in evidence_files['host-plan-log'].read_text() or \
   'checks=12 failures=0' not in evidence_files['host-near-log'].read_text() or \
   'checks=12 failures=0' not in evidence_files['host-near-asan-log'].read_text() or \
   '[HOST] SUMMARY: checks=124 failures=0 -> PASS' not in \
       evidence_files['host-protocol-log'].read_text():
    raise SystemExit('host checks did not pass')
for label in ('zydis-target-build-log', 'planner-target-build-log',
              'near-target-build-log', 'codepatch-target-build-log',
              'diagnostic-target-build-log', 'link-log'):
    if 'exit=0' not in evidence_files[label].read_text():
        raise SystemExit(f'failed target build: {label}')
symbols = evidence_files['near-undefined-symbols'].read_text()
for name in ('mmap', 'mprotect', 'munmap'):
    if ' U ' + name not in symbols:
        raise SystemExit(f'target POSIX mapping symbol absent: {name}')

evidence = {label: {'sha256': sha(path.read_bytes()), 'size': path.stat().st_size}
            for label, path in evidence_files.items()}
manifest = outputs / 'cap03-near-trampoline-manifest-20261013.json'
manifest.write_text(json.dumps({
    'status': 'offline only; no console execution or live target patch',
    'base_stage': str(base), 'stage': str(stage),
    'changed_files': files, 'patch_sha256': sha(patch.read_bytes()),
    'evidence': evidence,
    'verification': {'near_host': '12/12', 'near_asan': '12/12',
                     'existing_planner': '24/24', 'host_protocol': '124/124',
                     'ps5_target_objects_and_link': 'PASS',
                     'console_executed': False},
}, indent=2) + '\n')

with tempfile.TemporaryDirectory(prefix='cap03-stage50-replay-',
                                 dir=workspace / 'work') as temp:
    scratch = Path(temp) / 'base'
    shutil.copytree(base, scratch, ignore=shutil.ignore_patterns('build'))
    for args in (['git', 'apply', '--check', str(patch)],
                 ['git', 'apply', str(patch)]):
        run = subprocess.run(args, cwd=scratch, capture_output=True,
                             text=True, timeout=60)
        if run.returncode:
            raise SystemExit(f'{args}: {run.stderr}')
    for rel in changed:
        if (scratch / rel).read_bytes() != (stage / rel).read_bytes():
            raise SystemExit(f'exact replay mismatch: {rel}')

archive_path = outputs / 'cap03-near-trampoline-evidence-20261013.zip'
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch, patch.name)
    archive.write(manifest, manifest.name)
    for label, path in evidence_files.items():
        archive.write(path, f'evidence/{label}{path.suffix}')
    archive.writestr('evidence/replay.txt',
                     'git apply --check: PASS\ngit apply: PASS\n'
                     'four new sources: exact-byte replay\n'
                     'stage 45 existing sources: byte-identical\n')
with zipfile.ZipFile(archive_path) as archive:
    if archive.testzip(): raise SystemExit('ZIP CRC failure')
print('changed files:', len(changed))
print('patch:', patch.stat().st_size, sha(patch.read_bytes()))
print('exact-base replay: PASS')
print('ZIP CRC: PASS; size:', archive_path.stat().st_size)
