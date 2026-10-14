#!/usr/bin/env python3
"""Run the isolated loader's prior host gates and original PE fixture."""
from pathlib import Path
import hashlib
import subprocess

workspace = Path('/WORKSPACE/Documents/Codex/2026-10-13/can-x20')
stage = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-heap-resize-20261013-59')
logs = workspace / 'work/stage59-regressions'
logs.mkdir(parents=True, exist_ok=True)
names = [
    'run_dynamic_module_lookup.py', 'run_guest_pe_batch.py',
    'run_mapped_module_registry.py', 'run_guarded_shim_policy.py',
    'run_guarded_pe_activation.py', 'run_preflight.py',
    'run_static_tls.py', 'run_teb_lifecycle.py',
    'run_context_bridge.py', 'run_import_registry.py',
    'run_lazy_import_resolution.py', 'run_typed_import_install.py',
    'run_win32_sync_import.py', 'run_thread_notification_policy.py',
    'run_real_tls_prepare.py',
]
for name in names:
    run = subprocess.run(['python3', 'loader/tests/' + name], cwd=stage,
                         capture_output=True, text=True, timeout=120)
    (logs / (name + '.log')).write_text(run.stdout + run.stderr +
                                       f'\nexit={run.returncode}\n')
    print(name, run.returncode, flush=True)
    if run.returncode:
        raise SystemExit(f'{name} failed')

fixture = stage.parent / 'cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
digest = 'acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce'
if hashlib.sha256(fixture.read_bytes()).hexdigest() != digest:
    raise SystemExit('fixture hash changed before e2e')
build = [
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wno-unused-parameter', '-Wno-unused-variable', '-Iloader/include',
    '-Iloader/src', '-I../cap13/BUILD/src', 'loader/tests/e2e_host.cpp',
    'loader/src/pe_loader.cpp', 'loader/src/dllmain_invoker.cpp',
    'loader/src/win_api_shim.cpp', 'loader/src/win64_bridge.S',
    '-o', 'loader/tests/e2e-host-stage59',
]
run = subprocess.run(build, cwd=stage, capture_output=True, text=True,
                     timeout=90)
(logs / 'e2e-build.log').write_text(run.stdout + run.stderr +
                                    f'\nexit={run.returncode}\n')
if run.returncode: raise SystemExit('e2e build failed')
run = subprocess.run(['arch', '-x86_64', 'loader/tests/e2e-host-stage59',
                      'fixture', str(fixture)], cwd=stage, capture_output=True,
                     text=True, timeout=30)
(logs / 'e2e-run.log').write_text(run.stdout + run.stderr +
                                  f'\nexit={run.returncode}\n')
if run.returncode or 'RESULT: PASS (11 checks, 0 failed)' not in run.stdout:
    raise SystemExit('e2e fixture failed')
if hashlib.sha256(fixture.read_bytes()).hexdigest() != digest:
    raise SystemExit('fixture hash changed after e2e')
print('e2e fixture 11/11 and unchanged hash: PASS', flush=True)
