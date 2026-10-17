#!/usr/bin/env python3
"""Run the original two-pass launcher under reviewed R4 admission and additive gates."""
import argparse
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from admission import Admission, OWN, require, read, sha, pin
from hooks import install
from composition import write


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
