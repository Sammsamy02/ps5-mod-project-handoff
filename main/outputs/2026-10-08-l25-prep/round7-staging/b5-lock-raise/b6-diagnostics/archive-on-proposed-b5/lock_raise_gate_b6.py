#!/usr/bin/env python3
"""B6 variant of the proposed lock_raise_gate.py (copy; the original is untouched).
Difference: it also recognises the B6 tracked wrapper
    b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};
as a lock scope, and it prints how many RAW std lock scopes on that mutex are left
(--require-tracked turns any raw scope into a failure: B6 must track every site).

Original text follows.

PROPOSED gate (not wired into any build): no Lua raise while holding
LuaMod::m_thread_actions_mutex.

Why: Lua is compiled as C on PS5, so a Lua raise (luaL_error / lua_error /
lua.throw_error inside a pcall) is a longjmp that skips C++ destructors.
A std::lock_guard alive at that moment is never released, the owning thread
keeps the recursive mutex forever, and every other thread that needs it
(keybind dispatch, LoopAsync -> ExecuteInGameThread) blocks for the rest of
the session (launch-l26-B4-launch1-20261012, analysis/spawner-menu).

Usage: lock_raise_gate.py FILE [FILE...]
Prints how many files and lock scopes it checked; exits 1 when it checked
zero files or zero lock scopes (a gate that scans nothing is a failed gate),
or when any raise call sits inside a lock scope. Heuristic brace matcher:
strings, char literals and comments are blanked before matching."""
import re, sys

RAW_LOCK = re.compile(r'(lock_guard|unique_lock|scoped_lock)\s*<\s*std::recursive_mutex\s*>\s*\w+\s*[{(]\s*(LuaMod::)?m_thread_actions_mutex\s*[})]')
TRACKED_LOCK = re.compile(r'\bb6diag::ActionsLock\s+\w+\s*[{(]\s*(LuaMod::)?m_thread_actions_mutex\s*,\s*__LINE__\s*[})]')
LOCK = re.compile(RAW_LOCK.pattern + '|' + TRACKED_LOCK.pattern)
RAISE = re.compile(r'(\.\s*throw_error\s*\(|\bthrow_lua_error\s*\(|\bthrow_error\s*\(\s*\w|\bluaL_error\s*\(|\blua_error\s*\(|\bluaL_arg(error|check)\s*\(|\bluaL_check\w*\s*\(|\bluaL_opt\w*\s*\()')

RAW = re.compile(r'R"([^\s()\\]{0,16})\(')

def blank(src):
    """Blank comments, string/char literals (incl. raw strings with a
    delimiter) to spaces, keeping every newline so line numbers stay exact."""
    out, i, n = [], 0, len(src)
    def keep_lines(a, b):
        out.append(''.join('\n' if ch == '\n' else ' ' for ch in src[a:b]))
    while i < n:
        c = src[i]
        m = RAW.match(src, i) if c == 'R' else None
        if m:
            close = ')' + m.group(1) + '"'
            j = src.find(close, m.end()); j = n if j < 0 else j + len(close)
            keep_lines(i, j); i = j
        elif src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
            keep_lines(i, j); i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
            keep_lines(i, j); i = j
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            keep_lines(i, j); i = j
        else:
            out.append(c); i += 1
    return ''.join(out)

def scan(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    clean = blank(text)
    lines = clean.split('\n')
    findings, scopes, raw = [], 0, 0
    for ln, line in enumerate(lines):
        m = LOCK.search(line)
        if not m:
            continue
        scopes += 1
        if RAW_LOCK.search(line):
            raw += 1
        depth, end = 0, None
        start_col = m.end()
        for k in range(ln, len(lines)):
            seg = lines[k][start_col:] if k == ln else lines[k]
            for ch in seg:
                if ch == '{': depth += 1
                elif ch == '}':
                    depth -= 1
                    if depth < 0:
                        end = k; break
            if end is not None: break
        end = len(lines) - 1 if end is None else end
        for k in range(ln, end + 1):
            seg = lines[k][start_col:] if k == ln else lines[k]
            if RAISE.search(seg):
                findings.append((ln + 1, k + 1, text.split('\n')[k].strip()[:110]))
    return scopes, findings, raw

def main(argv):
    require_tracked = '--require-tracked' in argv
    files = [a for a in argv[1:] if not a.startswith('--')]
    total_scopes, total_findings, checked, total_raw = 0, 0, 0, 0
    for f in files:
        try:
            scopes, findings, raw = scan(f)
        except OSError as e:
            print(f'ERROR cannot read {f}: {e}'); return 1
        checked += 1; total_scopes += scopes; total_findings += len(findings); total_raw += raw
        for lock_line, raise_line, text in findings:
            print(f'FINDING {f}:{raise_line} raise inside m_thread_actions_mutex scope opened at line {lock_line}: {text}')
    print(f'lock_raise_gate_b6: files checked={checked} lock scopes checked={total_scopes} (raw std scopes={total_raw}, tracked={total_scopes - total_raw}) findings={total_findings}')
    if checked == 0 or total_scopes == 0:
        print('lock_raise_gate: FAIL (nothing scanned)'); return 1
    if total_findings:
        print('lock_raise_gate: FAIL'); return 1
    if require_tracked and total_raw:
        print(f'lock_raise_gate: FAIL ({total_raw} raw std lock scope(s) left untracked)'); return 1
    print('lock_raise_gate: PASS'); return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
