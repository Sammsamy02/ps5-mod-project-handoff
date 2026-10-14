#!/usr/bin/env python3
"""Verify exact application/inverse and run the source policy regression."""
import hashlib
import json
import pathlib
import subprocess
import tempfile

import stage_tls_policy as stage

HERE = pathlib.Path(__file__).resolve().parent
original = stage.SOURCE.read_bytes()
derived = stage.transform(original)
assert stage.inverse(derived) == original
for invalid in [original + b'\n', derived]:
    try:
        stage.transform(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError('drift or reapplication was accepted')
patch = HERE / 'prospero-pthread-tls.patch'
with tempfile.TemporaryDirectory(prefix='patch-check-', dir=HERE) as name:
    copied = pathlib.Path(name) / 'configure.ac'
    copied.write_bytes(original)
    result = subprocess.run(['/usr/bin/patch', '-p1', '-i', str(patch)],
                            cwd=name, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert copied.read_bytes() == derived

logs = HERE / 'logs'
logs.mkdir(exist_ok=True)
for label, source, expected in [('baseline-red', stage.SOURCE, 1),
                                 ('staged-green', HERE / 'derived/configure.ac', 0)]:
    command = ['python3', str(HERE / 'test_tls_policy.py'), '--source', str(source), '-v']
    result = subprocess.run(command, capture_output=True, text=True)
    (logs / (label + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == expected, label
    if label == 'baseline-red':
        assert 'FAILED (failures=2)' in result.stderr
    else:
        assert 'Ran 4 tests' in result.stderr and result.stderr.endswith('OK\n')

receipt = {'scope': 'Mac-only staged configure source; not full Autoconf generation or runtime proof',
           'checks': 7, 'source_files_checked': 1, 'exact_patch_application': True,
           'exact_inverse': True, 'drift_refused': True, 'reapplication_refused': True,
           'baseline_policy_regressions_fail': 2, 'staged_policy_tests_pass': 4,
           'derived_sha256': hashlib.sha256(derived).hexdigest()}
(HERE / 'VERIFICATION.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
