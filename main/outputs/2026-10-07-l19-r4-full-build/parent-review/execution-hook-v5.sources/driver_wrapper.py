#!/usr/bin/env python3
"""R4 admitted composition hook + actual inherited packaging driver."""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from admission import Admission, OWN, require
from hooks import install
from composition import Composer


def load_arguments(argv):
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--admission', required=True)
    parser.add_argument('--admission-sha256', required=True)
    parser.add_argument('--go', required=True)
    parser.add_argument('--go-sha256', required=True)
    args, remaining = parser.parse_known_args(argv)
    admission = Admission.load(args.admission, args.admission_sha256, args.go, args.go_sha256)
    return admission, remaining


def main():
    admission, argv = load_arguments(sys.argv[1:])
    require('--allow-unedited' not in argv and '--no-bridge-unit' not in argv
            and '--reuse-composition-run' not in argv, 'driver gate bypass/reused composition refused')
    install(admission)
    import build_runtime_composition6_l19 as driver
    composer = Composer(admission)

    def compose(args, logs):
        number = {'run-l19-1': 1, 'run-l19-2': 2}.get(Path(args.composition_run).name)
        require(number in (1, 2), 'driver composition path must select explicit pass1/pass2')
        return composer.compose(args.composition_run, number)

    driver.compose_phase = compose
    # The frozen launcher still supplies its historical Program argument. Replace that
    # one argument explicitly with the new reviewed source before the real parser/gates.
    require(argv.count('--program-source') == 1, 'one explicit Program source argument required')
    argv[argv.index('--program-source') + 1] = admission.records['Program']['path']
    sys.argv = [str(OWN / 'build_runtime_composition6_l19.py'), *argv]
    driver.main()


if __name__ == '__main__':
    main()
