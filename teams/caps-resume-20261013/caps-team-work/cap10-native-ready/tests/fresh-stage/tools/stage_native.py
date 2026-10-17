#!/usr/bin/env python3
"""Fresh owned clone + exact hash-bound final native patches + local generation.

Use from a new sibling component carrying these tools, patches, evidence bindings
and LOCAL-INPUTS.json. Existing target-work is deliberately never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=HERE.parent / 'cap10-port-closure/target-work')
args = parser.parse_args()
work = HERE / 'target-work'
if work.exists():
    raise SystemExit('Use a fresh owned component; target-work already exists.')
bindings = json.loads((HERE / 'evidence/NATIVE-PATCH-BINDINGS.json').read_text())
local = json.loads((HERE / 'LOCAL-INPUTS.json').read_text())
env = dict(os.environ, PATH=local['build_tools'] + '/bin:/usr/bin:/bin:/usr/sbin:/sbin')
for binding in bindings:
    assert hashlib.sha256((args.source / binding['source_path']).read_bytes()).hexdigest() == binding['baseline_sha256']
    assert hashlib.sha256((HERE / binding['patch_path']).read_bytes()).hexdigest() == binding['patch_sha256']
for directory in ['logs', 'evidence']:
    (HERE / directory).mkdir(exist_ok=True)
receipt = dict(commands=[], source=str(args.source), destination=str(work), max_make_jobs=1)


def run(name, command, cwd):
    log = HERE / 'logs' / (name + '.log')
    with log.open('w') as out:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT)
    receipt['commands'].append(dict(command=command, cwd=str(cwd), exit_code=result.returncode,
                                    log_sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
    (HERE / 'evidence/NATIVE-FRESH-STAGE.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if result.returncode:
        raise SystemExit(result.returncode)


run('NATIVE-CLONE', ['/bin/cp', '-cR', str(args.source), str(work)], HERE)
for index, binding in enumerate(bindings):
    command = ['/usr/bin/patch', '--batch', '--fuzz=0', '-p1', '-i', str(HERE / binding['patch_path'])]
    run('NATIVE-PATCH-' + str(index) + '-DRY', command + ['--dry-run'], work)
    run('NATIVE-PATCH-' + str(index), command, work)
    assert hashlib.sha256((work / binding['source_path']).read_bytes()).hexdigest() == binding['patched_sha256']
for tool in ['autoheader', 'autoconf']:
    run('NATIVE-' + tool.upper(), [local['build_tools'] + '/bin/' + tool], work)
print('Fresh owned native sources staged; run configure to replace copied absolute generated bindings.')
