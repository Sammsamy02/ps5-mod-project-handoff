from pathlib import Path
import subprocess, json

# [L27][EV string-seam] full-battery re-run (agent-EV 2026-10-09) after the
# seam edits, same commands/discipline as tests/run.py. SKIPPED HERE (never
# silently): test_scan.py + the 'scan' fixture — a concurrent agent is
# mid-edit on actual_scan.cpp (mtime 19:03 today, references guard members
# the checked-in header does not define yet); re-running it mid-edit would
# test THEIR in-flight state, not the tree. Retried separately afterwards.
# Written to HOST-TESTS-EV-RERUN.json (HOST-TESTS.json keeps the last full
# battery incl. scan).

p = Path(__file__).resolve().parent.parent
root = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
results = []

x = subprocess.run(['python3', str(p / 'tests/test_custom.py')], capture_output=True, text=True)
print('test_custom.py', x.returncode)
assert x.returncode == 0, x.stderr

for name, src in [('custom', 'actual_custom.cpp'), ('policy', 'test_policy.cpp'), ('getters', 'actual_getters.cpp'), ('backend', 'actual_backend.cpp')]:
    for mode in ['plain', 'asan-ubsan']:
        binary = p / 'tests' / f'{name}-{mode}'
        a = ['clang++', '-std=c++20', '-include', 'utility', '-I', str(p / 'include'), '-I', str(root / 'outputs/2026-10-07-l19-local-profile-r4/units'), str(p / 'tests' / src), '-o', str(binary)]
        if mode != 'plain':
            a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
        x = subprocess.run(a, capture_output=True, text=True)
        assert x.returncode == 0, x.stderr
        x = subprocess.run([str(binary)], capture_output=True, text=True)
        r = dict(name=name, mode=mode, exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
        results.append(r)
        print(name, mode, x.returncode, x.stdout.strip())
        assert x.returncode == 0, x.stderr

(p / 'results/HOST-TESTS-EV-RERUN.json').write_text(json.dumps(results, indent=2))

# eq-pinadmission suite (verbatim run.py block)
eq_results = []
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'eqpins-{mode}'
    a = ['clang++', '-std=c++20', '-I', str(p.parent / 'profile' / 'units'), '-I', str(root / 'outputs/2026-10-08-combined-outer-lifecycle/candidate-v2'), '-I', str(p / 'include'), '-I', str(root / 'outputs/2026-10-07-l19-local-profile-r4/units'), str(p / 'tests/actual_eq_pinadmission.cpp'), '-o', str(binary)]
    if mode != 'plain':
        a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    x = subprocess.run(a, capture_output=True, text=True)
    assert x.returncode == 0, x.stderr
    x = subprocess.run([str(binary)], capture_output=True, text=True)
    r = dict(name='eq-pinadmission', mode=mode, exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
    eq_results.append(r)
    print('eq-pinadmission', mode, x.returncode, x.stdout.strip())
    assert x.returncode == 0, x.stderr
(p / 'results/HOST-TESTS-L26-EQ-PINADMISSION.json').write_text(json.dumps(eq_results, indent=2))

# er-pooldecoder suite (verbatim run.py block; re-extracts the CURRENT
# candidate/src/NameTypes.cpp decoder region with marker+sha pinning)
import hashlib
er_dir = p / 'results' / 'er-pooldecoder-verify'
er_regions = er_dir / 'regions'
er_regions.mkdir(parents=True, exist_ok=True)
nt = (p / 'candidate/src/NameTypes.cpp').read_text().splitlines(keepends=True)
start = next(i for i, l in enumerate(nt) if l.strip() == 'namespace FNamePoolDecoder')
end = next(i for i in range(start, len(nt)) if nt[i].strip() == '} // namespace FNamePoolDecoder')
er_inc = ''.join(nt[start:end + 1])
assert 'kCurrentBlockOffset = 0x8' in er_inc and 'rc::engtostring::kProbeId' in er_inc and 'try_tostring' in er_inc
(er_regions / 'er_pooldecoder.inc').write_text(er_inc)
er_manifest = {'source': str(p / 'candidate/src/NameTypes.cpp'), 'source_sha256': hashlib.sha256((p / 'candidate/src/NameTypes.cpp').read_bytes()).hexdigest(),
               'region_lines': [start + 1, end + 1], 'region_sha256': hashlib.sha256(er_inc.encode()).hexdigest()}
(er_dir / 'er-region-manifest.json').write_text(json.dumps(er_manifest, indent=1))
er_results = []
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'er-pooldecoder-{mode}'
    a = ['clang++', '-std=c++20', '-I', str(er_regions), '-I', str(p / 'include'), str(p / 'tests/actual_er_pooldecoder.cpp'), '-o', str(binary)]
    if mode != 'plain':
        a += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g']
    x = subprocess.run(a, capture_output=True, text=True)
    assert x.returncode == 0, x.stderr
    for suite in ['happy', 'refuse', 'race', 'truncation']:
        x = subprocess.run([str(binary), suite], capture_output=True, text=True)
        r = dict(name=f'er-pooldecoder-{suite}', mode=mode, exit=x.returncode, stdout=x.stdout, stderr=x.stderr, command=a)
        er_results.append(r)
        print(f'er-pooldecoder-{suite}', mode, x.returncode, 'FAIL' if x.returncode else 'pass')
        assert x.returncode == 0, x.stderr + x.stdout
(er_dir / 'HOST-TESTS.json').write_text(json.dumps(er_results, indent=2))

print('ev-rerun (minus in-flight scan): ALL PASS')
