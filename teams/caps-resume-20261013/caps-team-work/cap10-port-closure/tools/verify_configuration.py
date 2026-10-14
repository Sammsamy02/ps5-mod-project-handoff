#!/usr/bin/env python3
"""Verify real patch application, canonicalization and generated shell policy.

Full target configure/build receipts are separate; these checks do not claim
that shell-fragment policy tests execute a target runtime.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parents[1]
commands = []


def run(name, command, expected=0, cwd=None, input=None):
    result = subprocess.run(command, cwd=cwd, input=input, text=True, capture_output=True)
    (HERE / 'logs' / (name + '.stdout')).write_text(result.stdout)
    (HERE / 'logs' / (name + '.stderr')).write_text(result.stderr)
    commands.append(dict(name=name, command=command, returncode=result.returncode))
    assert result.returncode == expected, (name, result.stderr)
    return result


for file in [item['path'] for item in json.loads((HERE / 'evidence/CONFIGURATION-STAGE.json').read_text())['bindings']]:
    with tempfile.TemporaryDirectory(dir=HERE / 'probe', prefix='patch-') as tmp:
        target = Path(tmp) / file
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(HERE / 'pristine' / file, target)
        patch = str(HERE / 'patches' / (Path(file).name + '.patch'))
        run(file.replace('/', '-') + '-apply', ['/usr/bin/patch', '--batch', '-p1', '-i', patch], cwd=tmp)
        assert target.read_bytes() == (HERE / 'derived' / file).read_bytes()
        run(file.replace('/', '-') + '-reverse', ['/usr/bin/patch', '--batch', '-R', '-p1', '-i', patch], cwd=tmp)
        assert target.read_bytes() == (HERE / 'pristine' / file).read_bytes()
baseline = run('tuple-baseline-rejection', ['/bin/sh', str(HERE / 'pristine/config.sub'), 'x86_64-sie-ps5'], expected=1)
assert 'not recognized' in baseline.stderr
for tuple_ in ['x86_64-sie-ps5', 'x86_64-unknown-freebsd9', 'aarch64-apple-darwin25']:
    result = run('tuple-' + tuple_, ['/bin/sh', str(HERE / 'derived/config.sub'), tuple_])
    assert result.stdout.strip() == tuple_
generated = (HERE / 'target-work/configure').read_text()
run('generated-configure-shell-syntax', ['/bin/sh', '-n', str(HERE / 'target-work/configure')])


def block(start, end):
    assert generated.count(start) == 1 and generated.count(end) == 1
    return generated.split(start, 1)[1].split(end, 1)[0]


guard = block('# Prospero compiler identity guard: begin', '# Prospero compiler identity guard: end')
tls = block('# Prospero TLS policy: begin', '# Prospero TLS policy: end')
prelude = 'as_fn_error () { echo "$2" >&2; test "$1" -gt 0 && exit "$1"; exit 1; }\n'
checks = [
    ('proper-target', 'x86_64-sie-ps5', 'yes', 0),
    ('host-compiler-refused', 'x86_64-sie-ps5', 'no', 1),
    ('fake-freebsd-identity-refused', 'x86_64-unknown-freebsd9', 'yes', 1),
    ('unsupported-cpu-refused', 'aarch64-sie-ps5', 'yes', 1),
    ('other-target-preserved', 'x86_64-apple-darwin24', 'no', 0),
]
for name, host, prospero, expected in checks:
    run('guard-' + name, ['/bin/sh'], input=prelude + f'host={host}\nhost_prospero={prospero}\n' + guard, expected=expected)
for name, target, explicit, selection, expected, answer in [
    ('target-default', 'yes', 'no', '__thread', 0, 'pthread'),
    ('target-pthread', 'yes', 'yes', 'pthread', 0, 'pthread'),
    ('target-compiler-refused', 'yes', 'yes', '__thread', 1, None),
    ('other-default', 'no', 'no', '__thread', 0, '__thread'),
    ('other-pthread', 'no', 'yes', 'pthread', 0, 'pthread'),
]:
    result = run('tls-' + name, ['/bin/sh'], expected=expected,
                 input=prelude + f'host_prospero={target}\nmono_tls_explicit={explicit}\nwith_tls={selection}\n' + tls + '\necho "$with_tls"\n')
    if answer:
        assert result.stdout.strip() == answer
receipt = dict(scope='patch/canonicalization/generated-shell tests, not runtime behavior',
               commands=commands, checks=len(commands),
               generated_configure_sha256=hashlib.sha256(generated.encode()).hexdigest())
(HERE / 'evidence/CONFIGURATION-VERIFICATION.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('PASS', len(commands), 'configuration checks')
