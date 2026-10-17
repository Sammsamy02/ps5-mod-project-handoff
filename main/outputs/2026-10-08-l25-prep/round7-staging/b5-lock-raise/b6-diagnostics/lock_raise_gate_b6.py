#!/usr/bin/env python3
"""SUPERSEDED (round 8). The B6 copy of the gate only knew std::lock_guard + b6diag::ActionsLock on
one mutex and had no get_mod_ref ban. The one real gate, ../gate/lock_raise_gate.py, now knows the
ActionsLock wrapper, the banned get_mod_ref call and --forbid-raw-guard; this file only calls it
(same command line as before):

    lock_raise_gate_b6.py [--require-tracked] FILE [FILE...]

  --require-tracked  -> --forbid-raw-guard m_thread_actions_mutex (a std guard left on that mutex fails)
Always runs the hard mode: --helpers off --unprotected-calls --metamethod-calls.
Exit codes: 0 PASS, 1 findings / raw guard left, 2 nothing checked."""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GATE = os.path.join(HERE, '..', 'gate', 'lock_raise_gate.py')
def main(argv):
    if not os.path.isfile(GATE):
        print('lock_raise_gate_b6: ERROR unified gate not found: %s' % GATE); return 2
    files = [a for a in argv[1:] if not a.startswith('--')]
    if not files:
        print(__doc__); return 2
    cmd = [sys.executable, '-I', GATE, '--helpers', 'off', '--unprotected-calls', '--metamethod-calls']
    if '--require-tracked' in argv:
        cmd += ['--forbid-raw-guard', 'm_thread_actions_mutex']
    cmd += [os.path.abspath(f) for f in files]
    return subprocess.run(cmd).returncode
if __name__ == '__main__':
    sys.exit(main(sys.argv))
