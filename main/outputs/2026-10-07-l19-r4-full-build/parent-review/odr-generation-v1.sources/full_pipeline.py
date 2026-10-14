#!/usr/bin/env python3
"""Run the original two-pass launcher under reviewed R4 admission and additive gates."""
import argparse
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from admission import Admission, OWN, require, read, sha, pin, resolved
from hooks import install
from composition import write


def load_odr_generation(path, digest, rundir):
    """Exactly the pinned R3 generation, with actual this-pass provenance separate."""
    data = read(pin({'path': str(path), 'sha256': digest}, 'ODR generation receipt'))
    require(data.get('schema') == 1 and data['run_directory'] == str(Path(rundir).resolve()),
            'ODR generation run/schema mismatch')
    baseline = read(pin(data['baseline_composition'], 'R3 generation composition'))
    odr = read(pin(data['baseline_ODR'], 'R3 ODR accepted generation'))
    require(odr['result'] == 'PASS', 'R3 ODR generation was not accepted')
    pin(data['original_checker'], 'original unchanged ODR checker')
    require(data['actual_map']['path'] == str(Path(rundir).resolve() / 'runtime.map'), 'actual ODR map path')
    pin(data['actual_map'], 'actual ODR map')
    current = read(pin(data['current_composition'], 'actual composition provenance'))
    eligible = baseline['freshly_compiled_units']
    require(data['eligible_R3_generation_units'] == eligible and len(set(eligible)) == len(eligible),
            'exact R3 generation providers required')
    result = set()
    for name in eligible:
        require(name in current['selected_runtime_units'], 'generation provider missing: ' + name)
        prior, unit = baseline['selected_runtime_units'][name], current['selected_runtime_units'][name]
        pin({'path': str(resolved(unit['source'])), 'sha256': unit['source_sha256']}, 'generation source/' + name)
        obj = pin({'path': str(resolved(unit['object'])), 'sha256': unit['object_sha256']}, 'generation object/' + name)
        actual_fresh = (name in current['freshly_compiled_units']
                        and str(obj).startswith(str(Path(rundir).resolve()) + '/'))
        if not actual_fresh:
            pin({'path': str(resolved(prior['source'])), 'sha256': prior['source_sha256']}, 'R3 generation source/' + name)
            pin({'path': str(resolved(prior['object'])), 'sha256': prior['object_sha256']}, 'R3 generation object/' + name)
            require(unit['source_sha256'] == prior['source_sha256'] and unit['object_sha256'] == prior['object_sha256'],
                    'retained-path generation source/object must equal R3: ' + name)
        result.add(str(obj))
    require(len(result) == len(eligible), 'distinct generation provider objects required')
    return result


def odr_generation_receipt(a, rundir, output=None):
    rundir = Path(rundir).resolve()
    path = Path(output).resolve() if output else rundir / 'R4-ODR-GENERATION.json'
    require(not path.exists(), 'new ODR generation provenance file required')
    write(path, {'schema': 1, 'run_directory': str(rundir),
        'baseline_composition': a.recipes['base_receipt'],
        'baseline_ODR': a.data['odr_baseline_receipt'],
        'current_composition': {'path': str(rundir / 'receipt.json'), 'sha256': sha(rundir / 'receipt.json')},
        'actual_map': {'path': str(rundir / 'runtime.map'), 'sha256': sha(rundir / 'runtime.map')},
        'original_checker': {'path': str(OWN / 'check_odr_emitters_l19.py'), 'sha256': sha(OWN / 'check_odr_emitters_l19.py')},
        'eligible_R3_generation_units': list(a.base['freshly_compiled_units']),
        'actual_this_pass_fresh_compiles': read(rundir / 'receipt.json')['freshly_compiled_units'],
        'actual_prior_accepted_reuse': read(rundir / 'receipt.json')['r4_reused_accepted_pass1_units'],
        'scope': 'hash-backed ODR generation classification only; no compile provenance relabeling'})
    load_odr_generation(path, sha(path), rundir)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--admission', required=True)
    parser.add_argument('--admission-sha256', required=True)
    parser.add_argument('--go', required=True)
    parser.add_argument('--go-sha256', required=True)
    parser.add_argument('--full', action='store_true')
    args = parser.parse_args()
    a = Admission.load(args.admission, args.admission_sha256, args.go, args.go_sha256)
    install(a)
    import launch19_one_pass as launcher
    launcher.EDITION = read(a.pins['edition'])
    launcher.DRIVER = OWN / 'execution/driver_wrapper.py'
    original_run = launcher.run
    prefix = ['--admission', str(a.path), '--admission-sha256', a.digest,
              '--go', str(a.go_path), '--go-sha256', a.go_digest]

    def run(name, argv, tail=4):
        values = list(argv)
        if len(values) > 1 and Path(values[1]) == launcher.DRIVER:
            values = values[:2] + prefix + values[2:]
        if name == 'odr-emitter-check':
            checker = pin(a.data['odr_generation_checker'], 'explicit generation ODR checker')
            require(Path(values[1]) == OWN / 'check_odr_emitters_l19.py', 'original ODR invocation drift')
            generation = odr_generation_receipt(a, values[2])
            values[1] = str(checker)
            values += ['--generation-receipt', str(generation), '--generation-receipt-sha256', sha(generation)]
        return original_run(name, values, tail)

    launcher.run = run
    original_regen = launcher.regen_receipts

    def regen():
        original_regen()
        # Extend only this run's newly regenerated receipt with independently admitted
        # all-source dependencies, accepted FField source/header and new final headers.
        path = OWN / 'receipts/l11-replace-receipt.json'
        receipt = read(path)
        for p, digest in a.allowed.items():
            pin({'path': p, 'sha256': digest}, 'new all-source replace receipt')
            require(p not in receipt['files'] or receipt['files'][p] == digest,
                    'new replacement receipt dependency hash conflict')
            receipt['files'][p] = digest
            receipt.setdefault('reasons', {}).setdefault(p, 'explicit R4 frozen source/header/dependency admission')
        receipt['r4_execution_admission_sha256'] = a.digest
        write(path, receipt)

    launcher.regen_receipts = regen
    sys.argv = [str(OWN / 'launch19_one_pass.py'), '--full' if args.full else 'verify']
    launcher.main()
    if args.full:
        artifacts, composition = [], []
        for i in (1, 2):
            candidate = OWN / ('prx-candidate-l19-' + str(i))
            artifact = read(candidate / 'artifacts-receipt.json')['artifacts']['final_prx']
            pin(artifact, 'actual final artifact/' + str(i))
            artifacts.append(artifact)
            rec = read(OWN / ('runs/run-l19-' + str(i)) / 'receipt.json')
            pin({'path': rec['runtime_object'], 'sha256': rec['runtime_sha256']}, 'actual composition/' + str(i))
            composition.append(rec['runtime_sha256'])
        require(artifacts[0]['sha256'] == artifacts[1]['sha256'] and composition[0] == composition[1],
                'R4 full artifact/composition twin determinism failure')
        write(OWN / 'R4-EXECUTION-RECEIPT.json', {'pass': True, 'inherited_full_pipeline_complete': True,
            'admission': {'path': str(a.path), 'sha256': a.digest},
            'parent_go': {'path': str(a.go_path), 'sha256': a.go_digest},
            'selected_units': 369, 'composition_inputs': 386, 'affected_units': 147,
            'pass1_reused': 144, 'pass1_fresh': 3, 'pass2_fresh': 147,
            'actual_artifacts': artifacts, 'composition_sha256': composition,
            'console_contact': False, 'upload': 'HOLD'})


if __name__ == '__main__':
    main()
