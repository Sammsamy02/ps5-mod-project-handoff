#!/usr/bin/env python3
"""Capture owned native dry-run and reject stale frozen source path bindings."""
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parents[1]
WORK = HERE / 'target-work'
FROZEN = HERE.parent / 'cap10-port-closure' / 'target-work'
paths = ['mono/native/Makefile', 'libtool', 'config.status']
bindings = []
for name in paths:
    path = WORK / name
    raw = path.read_bytes()
    stale = str(FROZEN).encode() in raw
    bindings.append(dict(path=name, sha256=hashlib.sha256(raw).hexdigest(), stale_frozen_binding=stale))
    if stale:
        raise SystemExit('Refusing stale frozen build binding: ' + name)
command = ['/usr/bin/make', '-n', '-j1', 'V=1']
result = subprocess.run(command, cwd=WORK / 'mono/native', text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
log = HERE / 'logs/NATIVE-DRY-RUN.log'
log.write_text(result.stdout)
receipt = dict(command=command, cwd=str(WORK / 'mono/native'), exit_code=result.returncode,
               bindings=bindings, log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),
               stale_frozen_path_in_commands=str(FROZEN) in result.stdout,
               max_make_jobs=1, execution=False)
(HERE / 'evidence/NATIVE-DRY-RUN.json').write_text(json.dumps(receipt, indent=2) + '\n')
if result.returncode or receipt['stale_frozen_path_in_commands']:
    raise SystemExit('Native make dry-run failed or references frozen work; inspect log')
print('Native dry-run passes; generated build bindings and commands refer to owned work.')
