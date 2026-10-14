#!/usr/bin/env python3
"""Package exact-stage-44 expected-original guard with build evidence."""
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
base = team / 'cg-cap03-branch-islands-20261013-44'
stage = team / 'cg-cap03-snapshot-guard-20261013-45'
outputs = workspace / 'outputs'
changed = ['src/CodePatch.cpp', 'src/CodePatch.hpp',
           'src/DetourPlan.cpp', 'src/DetourPlan.hpp',
           'tests/detour_plan_test.cpp', 'tests/host_test_protocol.cpp']

def sha(data):
    return hashlib.sha256(data).hexdigest()

for original in base.rglob('*'):
    if not original.is_file() or 'build' in original.relative_to(base).parts:
        continue
    rel = original.relative_to(base)
    copied = stage / rel
    if rel.as_posix() not in changed and (
            not copied.is_file() or copied.read_bytes() != original.read_bytes()):
        raise SystemExit(f'unchanged base source drift: {rel}')

parts = []
files = {}
for rel in changed:
    before = (base / rel).read_bytes()
    after = (stage / rel).read_bytes()
    if before == after or not before.endswith(b'\n') or not after.endswith(b'\n'):
        raise SystemExit(f'bad changed source: {rel}')
    parts.extend(difflib.unified_diff(
        before.decode().splitlines(keepends=True),
        after.decode().splitlines(keepends=True),
        fromfile=f'a/{rel}', tofile=f'b/{rel}', lineterm='\n'))
    files[rel] = {'base_sha256': sha(before), 'stage_sha256': sha(after),
                  'size': len(after)}
patch = outputs / 'cap03-snapshot-guard-increment-20261013.patch'
patch.write_text(''.join(parts))

build = stage / 'build/ps5-obj'
evidence_files = {
    'host-plan-log': stage / 'build/host/detour/test-run.log',
    'host-protocol-log': build / 'host-protocol.log',
    'zydis-target-build-log': build / 'zydis.log',
    'planner-target-build-log': build / 'planner.log',
    'codepatch-target-build-log': build / 'codepatch.log',
    'diagnostic-target-build-log': build / 'diagnostic.log',
    'link-log': build / 'link.log',
    'target-symbols': build / 'symbols.log',
    'zydis-target-object': build / 'Zydis.o',
    'planner-target-object': build / 'DetourPlan.o',
    'codepatch-target-object': build / 'CodePatch.o',
    'diagnostic-target-object': build / 'PatchDiagnostic.o',
    'offline-target-elf': build / 'cap03-detour-offline.elf',
}
for label, path in evidence_files.items():
    if not path.is_file():
        raise SystemExit(f'missing evidence: {label}')
evidence = {label: {'sha256': sha(path.read_bytes()), 'size': path.stat().st_size}
            for label, path in evidence_files.items()}
if 'checks=24 failures=0' not in evidence_files['host-plan-log'].read_text():
    raise SystemExit('focused host tests did not pass 24/24')
if '[HOST] SUMMARY: checks=124 failures=0 -> PASS' not in evidence_files['host-protocol-log'].read_text():
    raise SystemExit('host protocol did not pass 124/124')
for label in ('zydis-target-build-log', 'planner-target-build-log',
              'codepatch-target-build-log', 'diagnostic-target-build-log', 'link-log'):
    if 'exit=0' not in evidence_files[label].read_text():
        raise SystemExit(f'failed target build: {label}')

manifest = outputs / 'cap03-snapshot-guard-manifest-20261013.json'
manifest.write_text(json.dumps({
    'status': 'offline only; no console execution',
    'base_stage': str(base), 'stage': str(stage),
    'changed_files': files, 'patch_sha256': sha(patch.read_bytes()),
    'evidence': evidence,
    'verification': {'focused_host_checks': '24/24',
                     'host_protocol': '124/124',
                     'ps5_target_objects_and_link': 'PASS',
                     'console_executed': False},
}, indent=2) + '\n')

with tempfile.TemporaryDirectory(prefix='cap03-stage45-replay-', dir=workspace / 'work') as temp:
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

zip_path = outputs / 'cap03-snapshot-guard-evidence-20261013.zip'
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch, patch.name)
    archive.write(manifest, manifest.name)
    for label, path in evidence_files.items():
        archive.write(path, f'evidence/{label}{path.suffix}')
    archive.writestr('evidence/replay.txt',
                     'git apply --check: PASS\ngit apply: PASS\n'
                     'unchanged stage 44 source: byte-identical\n'
                     'six changed files: exact replay\n')
with zipfile.ZipFile(zip_path) as archive:
    bad = archive.testzip()
    if bad:
        raise SystemExit(f'ZIP CRC failure: {bad}')
print('changed files:', len(changed))
print('patch:', patch.stat().st_size, sha(patch.read_bytes()))
print('exact-base replay: PASS')
print('ZIP CRC: PASS; size:', zip_path.stat().st_size)
