#!/usr/bin/env python3
"""Package pre-registration TLS callback executable gate with replay."""
import difflib
import hashlib
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path

BASE = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-thread-notification-policy-20261013-22')
STAGE = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-tls-callback-exec-gate-20261013-23')
OUT = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs')
NAME = 'tls-callback-exec-gate'
FILES = [
    'loader/src/pe_tls_registration.cpp',
    'loader/src/pe_tls_registration.h',
    'loader/tests/pe_tls_registration_test.cpp',
    'loader/tests/real_tls_prepare.cpp',
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(argv, timeout=30):
    result = subprocess.run(argv, cwd=STAGE, capture_output=True,
                            text=True, timeout=timeout)
    body = result.stdout + result.stderr + '\nEXIT=' + str(result.returncode) + '\n'
    if result.returncode:
        raise SystemExit(body)
    return body


logs = {
    'static-tls-host.txt': run(['python3', 'loader/tests/run_static_tls.py']),
    'real-dll-tls-prepare.txt': run(['python3', 'loader/tests/run_real_tls_prepare.py']),
    'thread-notification-policy-host.txt': run(['python3', 'loader/tests/run_thread_notification_policy.py']),
    'win32-sync-host.txt': run(['python3', 'loader/tests/run_win32_sync_import.py']),
    'lazy-import-resolution-host.txt': run(['python3', 'loader/tests/run_lazy_import_resolution.py']),
    'typed-import-install-host.txt': run(['python3', 'loader/tests/run_typed_import_install.py']),
    'context-bridge-host.txt': run(['python3', 'loader/tests/run_context_bridge.py']),
    'import-registry-host.txt': run(['python3', 'loader/tests/run_import_registry.py']),
    'teb-lifecycle-host.txt': run(['python3', 'loader/tests/run_teb_lifecycle.py']),
    'loader-preflight.txt': run(['python3', 'loader/tests/run_preflight.py']),
}
fixture = STAGE.parent / 'cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
fixture_expected = 'acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce'
fixture_before = sha(fixture.read_bytes())
assert fixture_before == fixture_expected, 'fixture hash differs from pinned input'
logs['fixture-e2e-build.txt'] = run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wno-unused-parameter', '-Wno-unused-variable',
    '-Iloader/include', '-Iloader/src', '-I../cap13/BUILD/src',
    'loader/tests/e2e_host.cpp', 'loader/src/pe_loader.cpp',
    'loader/src/dllmain_invoker.cpp', 'loader/src/win_api_shim.cpp',
    'loader/src/win64_bridge.S', '-o', 'loader/tests/e2e-host-typed-import',
])
logs['bridge-e2e.txt'] = run([
    'arch', '-x86_64', 'loader/tests/e2e-host-typed-import', 'bridge',
], timeout=20)
logs['bridge6-e2e.txt'] = run([
    'arch', '-x86_64', 'loader/tests/e2e-host-typed-import', 'bridge6',
], timeout=20)
logs['fixture-e2e.txt'] = run([
    'arch', '-x86_64', 'loader/tests/e2e-host-typed-import', 'fixture',
    '../cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll',
], timeout=20)
fixture_after = sha(fixture.read_bytes())
assert fixture_after == fixture_before, 'fixture input changed during regression'
logs['fixture-integrity.txt'] = ('pinned_sha256=' + fixture_expected + '\n'
                                 'after_sha256=' + fixture_after + '\nPASS unchanged\n')

patch = []
manifest = {'base_stage': str(BASE), 'staged_stage': str(STAGE), 'files': {}}
for rel in FILES:
    old_path = BASE / rel
    old = old_path.read_bytes() if old_path.exists() else None
    new = (STAGE / rel).read_bytes()
    old_lines = old.decode('utf-8').splitlines(keepends=True) if old is not None else []
    new_lines = new.decode('utf-8').splitlines(keepends=True)
    patch.append(''.join(difflib.unified_diff(old_lines, new_lines,
        fromfile='a/' + rel if old is not None else '/dev/null',
        tofile='b/' + rel)))
    manifest['files'][rel] = {'base_sha256': sha(old) if old is not None else 'absent',
                              'staged_sha256': sha(new), 'size': len(new)}
patch_bytes = ''.join(patch).encode('utf-8')
patch_path = OUT / (NAME + '-increment-20261013.patch')
patch_path.write_bytes(patch_bytes)
manifest['patch_sha256'] = sha(patch_bytes)

with tempfile.TemporaryDirectory(prefix='cap13-tlscallback-replay-') as scratch:
    scratch_path = Path(scratch)
    for rel in FILES:
        if (BASE / rel).exists():
            target = scratch_path / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((BASE / rel).read_bytes())
    run_check = subprocess.run(['git', 'apply', '--check', str(patch_path)],
                               cwd=scratch, capture_output=True, text=True)
    if run_check.returncode:
        raise SystemExit(run_check.stderr)
    run_apply = subprocess.run(['git', 'apply', str(patch_path)],
                               cwd=scratch, capture_output=True, text=True)
    if run_apply.returncode:
        raise SystemExit(run_apply.stderr)
    for rel in FILES:
        assert (scratch_path / rel).read_bytes() == (STAGE / rel).read_bytes(), rel
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
print(json.dumps({'patch': str(patch_path), 'manifest': str(manifest_path),
                  'evidence_zip': str(zip_path),
                  'patch_sha256': manifest['patch_sha256']}, indent=2))
