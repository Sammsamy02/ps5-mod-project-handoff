#!/usr/bin/env python3
"""Read-only host TLS preparation on five unchanged Windows x64 DLLs."""

import hashlib
import json
import subprocess
from pathlib import Path

stage = Path(__file__).resolve().parents[2]
root = stage / 'loader'
team = stage.parent
out = stage / 'REPORT/evidence'
out.mkdir(parents=True, exist_ok=True)
exe = root / 'tests/real-tls-prepare-host'
subprocess.run([
    'clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall',
    '-Wextra', '-Werror', '-pthread', '-I' + str(root / 'src'),
    str(root / 'tests/real_tls_prepare.cpp'),
    str(root / 'src/static_tls_bank.cpp'),
    str(root / 'src/pe_tls_registration.cpp'), '-o', str(exe),
], check=True, timeout=30)

upstream = team / 'cg-max-ready-20261013-03/cap13/upstream-inputs'
cases = [
    ('TrueFOV', stage / 'third_party/TrueFOV-upstream-winmm.dll', 0,
     '5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3'),
    ('ZeroCompanyMandoWardrobe', upstream / 'ZeroCompanyMandoWardrobe-upstream-main.dll', 0,
     '821ecad6c9f7e61faa2a07b31c216475768b72c2066369d3b0b88f715b33a40c'),
    ('TFWWorkbench', upstream / 'TFWWorkbench-upstream-main.dll', 0,
     '24b05dc267dc6e9faa6cbcb4e85a8491733e0a86b86fbac92f5aa1a0950aa6ca'),
    ('EventViewerMod', upstream / 'EventViewerMod-upstream-main.dll', 2,
     '83422a25fffd8d0d39c78fe001eac2fd169fc148858a5004ae3af52fd41544e0'),
    ('KismetDebuggerMod', upstream / 'KismetDebuggerMod-upstream-main.dll', 2,
     '4e7907661afe4bb49ebd1677b9e631410554b80ed9df1a9b3ea9e1bcd92a4aae'),
]

rows = []
for name, path, callbacks, expected_hash in cases:
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    if before != expected_hash:
        raise RuntimeError(name + ': input hash differs from pinned source')
    result = subprocess.run(['arch', '-x86_64', str(exe), str(path), str(callbacks)],
                            capture_output=True, text=True, timeout=20)
    after = hashlib.sha256(path.read_bytes()).hexdigest()
    row = {'name': name, 'path': str(path), 'sha256': before,
           'sha256_after': after, 'expected_callbacks': callbacks,
           'exit_code': result.returncode, 'output': result.stdout + result.stderr}
    rows.append(row)
    print(name, result.stdout.strip() or result.stderr.strip(),
          'UNCHANGED' if before == after else 'CHANGED')
    if result.returncode or before != after:
        break

payload = {'scope': 'copy sections, relocate, prepare static TLS; no DLL execution',
           'all_passed': len(rows) == len(cases) and all(
               row['exit_code'] == 0 and row['sha256'] == row['sha256_after']
               for row in rows), 'cases': rows}
(out / 'real-static-tls-prepare.json').write_text(json.dumps(payload, indent=2) + '\n')
raise SystemExit(0 if payload['all_passed'] else 1)
