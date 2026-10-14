#!/usr/bin/env python3
"""lock_raise_gate_b7.py -- the round-7 "error raised while a lock is held" gate, kept and extended for B7.
Same command line and exit codes as evidence/launch-l26-B4-launch1-20261012/analysis/spawner-menu/lock_raise_gate.py:

    lock_raise_gate_b7.py FILE [FILE...]

Prints how many files and lock scopes it checked; exits 1 when it checked zero files or zero lock scopes, or when
any raise sits inside a scope that holds LuaMod::m_thread_actions_mutex. Extensions over the original:
  * guard forms: std::lock_guard / unique_lock / scoped_lock (as before) + b6diag::ActionsLock (B6) +
    NativeLuaActionDispatchScope (holds the lock while it lives)
  * raise forms: also throw_lua_error and lua_yield*
  * one-hop helpers: a call to a function (defined in the scanned files) whose own body contains a raise is a finding
    (the B4 get_mod_ref case that the B5 sites track found by hand)
  * kept ON PURPOSE after B7: with Lua as C++ the guard is released by unwinding, but the rollback build (Lua as C),
    noexcept frames and any swallowed error still leak it, so the gate does not relax in C++ mode.
Reuses the scanner in b7_source_gate.py (single source of truth). --selftest runs its fixtures.
"""
import sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import b7_source_gate as g
import catch_inventory as ci
sys.path.pop(0)


def main(argv):
    if argv[1:] == ['--selftest']: return g.selftest()
    files = argv[1:]
    if not files: print(__doc__); return 2
    texts, scopes, findings, checked = {}, 0, [], 0
    for f in files:
        try: texts[f] = ci.strip(Path(f).read_text(errors='replace'))
        except OSError as e: print(f'ERROR cannot read {f}: {e}'); return 1
    # strip() blanks comments/strings; helpers = functions in the scanned files whose body has a direct raise
    helpers = {h for t in texts.values() for h in g.raising_helpers({Path(f): t for f, t in texts.items()})
               if h not in ('throw_error', 'throw_error_impl', 'throw_error_from_buffer', 'throw_lua_error')}
    for f, t in texts.items():
        s, fi = g.lock_scan(t, helpers, f)
        checked += 1; scopes += s; findings += fi
        for (_, lock_line, raise_line, kind, txt) in fi:
            print(f'FINDING {f}:{raise_line} ({kind}) raise inside m_thread_actions_mutex scope opened at line {lock_line}: {txt}')
    print(f'lock_raise_gate_b7: files checked={checked} lock scopes checked={scopes} helpers known={len(helpers)} findings={len(findings)}')
    if checked == 0 or scopes == 0: print('lock_raise_gate_b7: FAIL (nothing scanned)'); return 1
    if findings: print('lock_raise_gate_b7: FAIL'); return 1
    print('lock_raise_gate_b7: PASS'); return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
