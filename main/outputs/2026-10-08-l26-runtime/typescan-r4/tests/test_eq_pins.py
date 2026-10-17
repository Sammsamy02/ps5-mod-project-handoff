from pathlib import Path
import subprocess, json
p = Path(__file__).resolve().parent.parent
root = Path.cwd()
eq_results = []
for mode in ['plain', 'asan-ubsan']:
    binary = p / 'tests' / f'eqpins-{mode}'
    a = ['clang++', '-std=c++20',
         '-I', str(p.parent / 'profile' / 'units'),
         '-I', str(root / 'outputs/2026-10-08-combined-outer-lifecycle/candidate-v2'),
         '-I', str(p / 'include'),
         '-I', str(root / 'outputs/2026-10-07-l19-local-profile-r4/units'),
         str(p / 'tests/actual_eq_pinadmission.cpp'), '-o', str(binary)]
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
