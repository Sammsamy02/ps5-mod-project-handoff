#!/usr/bin/env python3
"""SUPERSEDED (round 8). This used to be a second, smaller gate (regex based, std guards only). Two
gates drifted apart and the B6 lock wrapper made the small ones scan 3 of 47 scopes and still
pass. It is now a thin wrapper around the ONE gate, ../../gate/lock_raise_gate.py, run in the hard
mode (--helpers off: direct raises, local lambdas and the banned get_mod_ref call fail; plus
--unprotected-calls --metamethod-calls).

Usage (unchanged): lock_raise_gate_v2.py ROOT [ROOT...]   |   lock_raise_gate_v2.py --file F [F...]
Exit codes are the unified gate's: 0 PASS, 1 findings, 2 nothing checked / usage."""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GATE = os.path.join(HERE, '..', '..', 'gate', 'lock_raise_gate.py')
def main(argv):
    if not os.path.isfile(GATE):
        print('lock_raise_gate_v2: ERROR unified gate not found: %s' % GATE); return 2
    if len(argv) < 2:
        print(__doc__); return 2
    cmd = [sys.executable, '-I', GATE, '--helpers', 'off', '--unprotected-calls', '--metamethod-calls']
    if argv[1] == '--file':
        if len(argv) < 3:
            print('lock_raise_gate_v2: ERROR --file needs at least one file'); return 2
        cmd += [os.path.abspath(f) for f in argv[2:]]
    else:
        for r in argv[1:]:
            cmd += ['--root', os.path.abspath(r)]
    return subprocess.run(cmd).returncode
if __name__ == '__main__':
    sys.exit(main(sys.argv))
