#!/usr/bin/env python3
"""Package exact-base cap03 detour planner with build and replay evidence."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile

workspace = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20')
team = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work')
base = team / 'cg-cap03-v2-query-guard-20261013-30'
stage = team / 'cg-cap03-detour-plan-20261013-43'
outputs = workspace / 'outputs'
changed = [
    'src/DetourPlan.cpp', 'src/DetourPlan.hpp',
    'tests/detour_plan_test.cpp', 'tests/run_detour_plan.py',
    'vendor/zydis/LICENSE', 'vendor/zydis/Zydis.c',
    'vendor/zydis/Zydis.h',
]

def sha(data):
    return hashlib.sha256(data).hexdigest()

# Assert the existing stage-30 source files are byte-identical.
for original in base.rglob('*'):
    if not original.is_file() or 'build' in original.relative_to(base).parts:
        continue
    copied = stage / original.relative_to(base)
    if not copied.is_file() or copied.read_bytes() != original.read_bytes():
        raise SystemExit(f'base source drift: {original.relative_to(base)}')

parts = []
files = {}
for rel in changed:
    old = base / rel
    new = stage / rel
    before = old.read_bytes() if old.exists() else None
    after = new.read_bytes()
    if before == after:
        raise SystemExit(f'listed file unchanged: {rel}')
    # Git records the final no-newline marker on the generated C file;
    # a plain unified diff would concatenate the next file's header.
    if before is not None:
        raise SystemExit(f'new-file-only package expected: {rel}')
    diff = subprocess.run(['git', 'diff', '--no-index', '--', '/dev/null', rel],
                          cwd=stage, capture_output=True, text=True, timeout=60)
    if diff.returncode != 1:
        raise SystemExit(f'new-file diff failed: {rel}: {diff.stderr}')
    parts.append(diff.stdout)
    files[rel] = {'base_sha256': sha(before) if before is not None else None,
                  'stage_sha256': sha(after), 'size': len(after)}

patch = outputs / 'cap03-detour-plan-increment-20261013.patch'
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
        raise SystemExit(f'missing evidence: {label} {path}')
evidence = {label: {'sha256': sha(path.read_bytes()), 'size': path.stat().st_size}
            for label, path in evidence_files.items()}

if 'checks=19 failures=0' not in evidence_files['host-plan-log'].read_text():
    raise SystemExit('focused test log does not show 19/19')
if '[HOST] SUMMARY: checks=112 failures=0 -> PASS' not in evidence_files['host-protocol-log'].read_text():
    raise SystemExit('protocol log does not show 112/112')
for label in ('zydis-target-build-log', 'planner-target-build-log',
              'codepatch-target-build-log', 'diagnostic-target-build-log', 'link-log'):
    if 'exit=0' not in evidence_files[label].read_text():
        raise SystemExit(f'build did not report success: {label}')

manifest = outputs / 'cap03-detour-plan-manifest-20261013.json'
manifest.write_text(json.dumps({
    'status': 'offline only; no console execution',
    'base_stage': str(base), 'stage': str(stage),
    'decoder': {'name': 'Zydis v4.1.1',
                'source_commit': 'a2278f1d254e492f6a6b39f6cb5d1f5d515659dc',
                'zycore_commit': '0b2432ced0884fd152b471d97ecf0258ff4d859f',
                'license': 'MIT', 'form': 'official-generated amalgamation'},
    'changed_files': files, 'patch_sha256': sha(patch.read_bytes()),
    'evidence': evidence,
    'verification': {'focused_host_checks': '19/19',
                     'existing_host_protocol': '112/112',
                     'ps5_target_objects_and_link': 'PASS',
                     'console_executed': False},
}, indent=2) + '\n')

with tempfile.TemporaryDirectory(prefix='cap03-stage43-replay-', dir=workspace / 'work') as temp:
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

zip_path = outputs / 'cap03-detour-plan-evidence-20261013.zip'
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch, patch.name)
    archive.write(manifest, manifest.name)
    for label, path in evidence_files.items():
        archive.write(path, f'evidence/{label}{path.suffix}')
    archive.writestr('evidence/replay.txt',
                     'git apply --check: PASS\ngit apply: PASS\n'
                     'byte-identical stage 30 source: PASS\n'
                     'seven added files exact replay: PASS\n')
with zipfile.ZipFile(zip_path) as archive:
    bad = archive.testzip()
    if bad:
        raise SystemExit(f'ZIP CRC failed: {bad}')
print('changed files:', len(changed))
print('patch:', patch.stat().st_size, sha(patch.read_bytes()))
print('exact-base replay: PASS')
print('ZIP CRC: PASS; size:', zip_path.stat().st_size)
