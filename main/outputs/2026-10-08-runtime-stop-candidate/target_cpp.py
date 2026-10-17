from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
HERE = Path(__file__).resolve().parent
OUT = HERE / 'target-cpp-with-pinned-toolchain'
OUT.mkdir(exist_ok=False)

def pin(path):
    path = Path(path)
    return {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

runtime_receipt = ROOT / 'outputs/2026-10-07-l19-r4-full-build/runs/run-l19-2/receipt.json'
package_receipt = ROOT / 'outputs/2026-10-07-l19-r4-full-build/prx-candidate-l19-2/build-receipt.json'
toolchain_receipt = ROOT / 'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
runtime = json.loads(runtime_receipt.read_text())
package = json.loads(package_receipt.read_text())
llvm = json.loads(toolchain_receipt.read_text())['recipes']['unreal-uobject']['LLVM_CONFIG']
env = dict(os.environ, LLVM_CONFIG=llvm)
nm = Path(llvm).parent / 'llvm-nm'

ack = runtime['selected_runtime_units']['module-runtime-ack-supplier']
bridge = next(c for c in reversed(package['commands'])
              if '-c' in c.get('argv', [])
              and c['argv'][c['argv'].index('-c') + 1].endswith('/runtime_start_bridge.cpp'))
bridge_source = bridge['argv'][bridge['argv'].index('-c') + 1]
specs = [
    ('runtime_ack_supplier', ack['argv'], ack['source'], ack['object'],
     HERE / 'runtime_ack_supplier.cpp'),
    ('runtime_start_bridge', bridge['argv'], bridge_source,
     bridge['argv'][bridge['argv'].index('-o') + 1],
     HERE / 'runtime_start_bridge_r14_diag_spanpub.cpp'),
]
records = []
for name, original_argv, original_source, original_object, candidate in specs:
    source = ROOT / original_source if not Path(original_source).is_absolute() else Path(original_source)
    argv = list(original_argv)
    assert argv.count(original_source) == 1
    argv[argv.index(original_source)] = str(candidate)
    argv[argv.index('-o') + 1] = str(OUT / (name + '.o'))
    argv[argv.index('-MF') + 1] = str(OUT / (name + '.d'))
    # Only relocation of the source's quoted-header search root is added.
    argv[1:1] = ['-iquote', str(source.parent)]
    run = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True)
    (OUT / (name + '.stdout.log')).write_text(run.stdout)
    (OUT / (name + '.stderr.log')).write_text(run.stderr)
    record = {'name': name, 'exit': run.returncode, 'argv': argv,
              'source': pin(candidate), 'baseline_source': pin(source),
              'baseline_object': pin(original_object)}
    if run.returncode == 0:
        obj = OUT / (name + '.o')
        record['object'] = pin(obj)
        record['dependency'] = pin(OUT / (name + '.d'))
        symbols = {}
        for label, target in [('baseline', original_object), ('candidate', str(obj))]:
            n = subprocess.run([str(nm), '--undefined-only', '--format=posix', target],
                               cwd=ROOT, capture_output=True, text=True)
            assert n.returncode == 0, n.stderr
            (OUT / (name + '.' + label + '.undefined.txt')).write_text(n.stdout)
            symbols[label] = set(line.split()[0] for line in n.stdout.splitlines())
        record['undefined_added'] = sorted(symbols['candidate'] - symbols['baseline'])
        record['undefined_removed'] = sorted(symbols['baseline'] - symbols['candidate'])
    records.append(record)
    print(name, 'SDK compile', run.returncode)
    if run.returncode:
        print(run.stderr[-1800:])
receipt = {'scope': 'Whole actual selected supplier and bridge translation units; target compilation only',
           'runtime_recipe': pin(runtime_receipt), 'package_recipe': pin(package_receipt),
           'toolchain_recipe': pin(toolchain_receipt), 'LLVM_CONFIG': llvm,
           'records': records, 'native_execution': False, 'package_promotion': False}
(OUT / 'TARGET-CPP-RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
raise SystemExit(0 if all(r['exit'] == 0 for r in records) else 1)
