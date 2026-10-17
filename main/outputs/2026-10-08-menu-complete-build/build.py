#!/usr/bin/env python3
"""One changed TU, two fresh complete links; never changes frozen inputs."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BASE = ROOT / 'outputs/2026-10-08-r5-thread-final-integration-v2'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def pin(p):
    p = Path(p).resolve()
    return {'path': str(p), 'sha256': sha(p), 'size': p.stat().st_size}

def write(p, value):
    p.write_text(json.dumps(value, indent=2) + '\n')

def run(argv, out, label, env):
    result = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True, timeout=300)
    (out / (label + '.stdout')).write_text(result.stdout)
    (out / (label + '.stderr')).write_text(result.stderr)
    write(out / (label + '.json'), {'argv': argv, 'returncode': result.returncode})
    if result.returncode:
        raise RuntimeError(label + ' failed; inspect saved stderr')
    return result.stdout

def main():
    candidate = HERE / 'candidate/LuaUObject.cpp'
    assert sha(candidate) == '48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638'
    baseline_selection = BASE / 'SOURCE-SELECTION.json'
    assert sha(baseline_selection) == '5fb5f8d55ed8b7f42ce4734ca6d4850ca3f68b658c363ec984f16f5d3b2f1b43'
    selection = json.loads(baseline_selection.read_text())
    for role in ('source_pins', 'receipt_pins', 'artifact_pins'):
        for row in selection[role]:
            assert sha(row['path']) == row['sha256'], row['path']
    source_pins = copy.deepcopy(selection['source_pins'])
    source_pins.append(pin(candidate))
    # The accepted parent dependency closure is kept, including the old source.
    # The new TU includes no new headers. Its emitted graph is retained below.
    results = []
    for passno in (1, 2):
        base_receipt = BASE / f'pass{passno}/COMPOSITION-RECEIPT.json'
        base = json.loads(base_receipt.read_text())
        units = copy.deepcopy(base['selected_units'])
        u = next(r for r in units if r['name'] == 'lua-luauobject')
        assert sha(u['source']) == u['source_sha256']
        assert sha(u['object']) == u['object_sha256']
        out = HERE / f'pass{passno}'
        out.mkdir()
        argv = list(u['argv'])
        obj = out / 'lua-luauobject.o'
        dep = out / 'lua-luauobject.d'
        for flag, value in (('-c', candidate), ('-o', obj), ('-MF', dep)):
            assert argv.count(flag) == 1
            argv[argv.index(flag) + 1] = str(value)
        llvm = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
        env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config'))
        run(argv, out, 'compile', env)
        # Check header dependency equality after removing only source/output names.
        def normalize(text, source, output):
            return text.replace(str(source), '<SOURCE>').replace(str(output), '<OUTPUT>').split()
        assert normalize(dep.read_text(), candidate, obj) == normalize(Path(u['dependency']).read_text(), u['source'], u['object']), 'unexpected include graph delta'
        old_object = u['object']
        u.update(source=str(candidate), source_sha256=sha(candidate), object=str(obj), object_sha256=sha(obj),
                 dependency=str(dep), dependency_sha256=sha(dep), argv=argv,
                 selection='fresh_parameter_bounds_TU', LLVM_CONFIG=str(llvm / 'llvm-config'))
        for row in units:
            assert sha(row['source']) == row['source_sha256'], row['name']
            assert sha(row['object']) == row['object_sha256'], row['name']
        link = json.loads(Path(base['link_command']['path']).read_text())
        cmd = [str(obj) if a == old_object else a for a in link['argv']]
        runtime = out / 'runtime-menu-complete.o'
        cmd[cmd.index('-o') + 1] = str(runtime)
        cmd = ['-Map=' + str(out / 'runtime.map') if a.startswith('-Map=') else a for a in cmd]
        inputs = [a for i, a in enumerate(cmd) if a.endswith(('.o', '.a')) and i != cmd.index('-o') + 1]
        assert len(inputs) == len(set(inputs)) == 391
        for row in units:
            assert inputs.count(row['object']) == 1
        for row in link['fixed_inputs']:
            assert sha(row['path']) == row['sha256']
        run(cmd, out, 'link', env)
        nm = llvm / 'llvm-nm'
        after = run([str(nm), '-u', str(runtime)], out, 'undefined', env)
        before = (BASE / f'pass{passno}/final.undefined.nm').read_text()
        names = lambda text: {line.split()[-1] for line in text.splitlines() if line.split()}
        assert names(after) == names(before), 'bounds TU changed runtime imports'
        current = copy.deepcopy(base)
        current.update(object=pin(runtime), selected_units=units,
                       parent_composition=pin(base_receipt), fresh_current_pass=1, retained_current_units=373,
                       scope='Complete parent graph plus one tested bounds preflight. Mac-only; lifecycle review is still unfinished.',
                       compile_receipt=pin(out / 'compile.json'))
        write(out / 'LINK-COMMAND.json', {'argv': cmd, 'ordered_inputs': inputs, 'fixed_inputs': link['fixed_inputs']})
        current['link_command'] = pin(out / 'LINK-COMMAND.json')
        write(out / 'COMPOSITION-RECEIPT.json', current)
        results.append({'pass': passno, 'runtime': pin(runtime), 'unit': u,
                        'composition': pin(out / 'COMPOSITION-RECEIPT.json')})
        print(json.dumps({'pass': passno, 'runtime': pin(runtime), 'undefined_changed': False}), flush=True)
    assert Path(results[0]['unit']['object']).read_bytes() == Path(results[1]['unit']['object']).read_bytes()
    assert Path(results[0]['runtime']['path']).read_bytes() == Path(results[1]['runtime']['path']).read_bytes()
    new_selection = {'source_pins': source_pins,
                     'receipt_pins': selection['receipt_pins'] + [r['composition'] for r in results],
                     'artifact_pins': selection['artifact_pins'] + [r['runtime'] for r in results],
                     'parent_selection': pin(baseline_selection), 'native_execution': False,
                     'selected_change': pin(candidate)}
    write(HERE / 'SOURCE-SELECTION.json', new_selection)
    write(HERE / 'TWIN-RECEIPT.json', {'status': 'MAC_ONLY_IDENTICAL_COMPLETE_RUNTIME_PAIR', 'passes': results,
                                     'source_selection': pin(HERE / 'SOURCE-SELECTION.json'),
                                     'selected_units': 374, 'full_link_inputs': 391, 'new_imports': [],
                                     'independent_cleanup_acceptance': False, 'native_execution': False,
                                     'native_menu': False, 'console_contact': False})

if __name__ == '__main__':
    main()
