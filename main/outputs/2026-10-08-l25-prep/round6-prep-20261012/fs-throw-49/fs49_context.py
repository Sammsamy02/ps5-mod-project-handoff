#!/usr/bin/env python3
"""fs49_context.py -- per-site context for the 49 known throwing std::filesystem
calls (fs-throw-49 track, Mac-only, READ-ONLY on every input).

For every throwing call site that harvest/fs_throw_gate.py's OWN scanner finds in
the given files (same lexer, same patterns, same PS5-liveness rule), print:
  file:line, the filesystem function, the innermost enclosing function header,
  every enclosing lambda, and every enclosing `try` block in the SAME file with
  the line and caught type of each of its catch clauses.

It only answers "is there a try/catch around this call in this file". Callers in
other functions/files (and threads) are traced by hand in CLASSIFICATION.md.

Usage:
    python3 -B fs49_context.py FILE [FILE...]
    python3 -B fs49_context.py --files-from LIST   (newline-separated paths)
Exit: 0 = at least one site reported; 2 = ZERO files or ZERO sites checked
(a checker that checks nothing is a failed checker), or a file unreadable.
"""
import os
import re
import sys

sys.dont_write_bytecode = True  # never write __pycache__ into harvest/
WHAT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), *(['..'] * 4)))
HARVEST = os.path.join(WHAT, 'outputs', '2026-10-08-l25-prep', 'harvest')
sys.path.insert(0, HARVEST)
import fs_throw_gate as G  # noqa: E402
from no_detour_gate import lex_cxx  # noqa: E402

CONTROL = re.compile(r'^(?:else\s+)?(?:if|for|while|switch|do|else)\b')
LAMBDA = re.compile(r'\]\s*(?:\([^()]*(?:\([^()]*\)[^()]*)*\))?\s*(?:mutable\s*)?(?:noexcept\s*)?(?:->\s*[\w:<>,\s*&]+)?\s*$')
FUNC_NAME = re.compile(r'([~\w:]+)\s*\((?:[^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:->\s*[\w:<>,\s*&]+)?\s*(?::.*)?$')


def blocks_of(text, line_of):
    """Return [(open_off, close_off, header, kind, catches)] for every {...}."""
    out, stack = [], []
    last_stmt = 0
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '{':
            header = ' '.join(text[last_stmt:i].split())
            stack.append([i, header])
            last_stmt = i + 1
        elif c == '}':
            if stack:
                o, header = stack.pop()
                out.append([o, i, header, classify(header), []])
            last_stmt = i + 1
        elif c == ';':
            last_stmt = i + 1
        i += 1
    unbalanced = len(stack)
    # attach catch clauses to try blocks: after a try block's '}', any number of
    # 'catch (...) { }' blocks follow.
    by_open = {b[0]: b for b in out}
    for b in out:
        if b[3] != 'try':
            continue
        j = b[1] + 1
        while True:
            m = re.match(r'\s*(?:#[^\n]*\n\s*)*catch\s*\(', text[j:])
            if not m:
                break
            p = j + m.end() - 1
            depth, k = 0, p
            while k < n:
                if text[k] == '(':
                    depth += 1
                elif text[k] == ')':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            ctype = ' '.join(text[p + 1:k].split())
            ob = text.find('{', k)
            cb = by_open.get(ob)
            b[4].append((line_of(j + m.start() + len(m.group(0)) - 6).no, ctype))
            if not cb:
                break
            j = cb[1] + 1
    return out, unbalanced


def classify(h):
    if re.search(r'(?:^|\W)try$', h):
        return 'try'
    if h.startswith('catch'):
        return 'catch'
    if h.startswith('namespace') or h.startswith('extern "'):
        return 'namespace'
    if re.match(r'^(?:template\s*<.*>\s*)?(?:class|struct|union|enum)\b', h):
        return 'type'
    if LAMBDA.search(h) and '[' in h:
        return 'lambda'
    if CONTROL.match(h) or h.startswith('}'):
        return 'control'
    if '(' in h and FUNC_NAME.search(h):
        return 'function'
    return 'block'


def scan(path, rel):
    with open(path, errors='replace') as f:
        views = lex_cxx(f.readlines())
    aliases = {'fs'}
    for v in views:
        m = G.ALIAS_DECL.search(v.code)
        if m:
            aliases.add(m.group(1))
    call_re, itr_re = G.build_patterns(aliases)
    parts, starts, pos = [], [], 0
    for v in views:
        starts.append(pos)
        code = v.code if v.live else ''
        parts.append(code)
        pos += len(code) + 1
    text = '\n'.join(parts)

    def line_of(off):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= off:
                lo = mid
            else:
                hi = mid - 1
        return views[lo]

    blocks, unbalanced = blocks_of(text, line_of)
    sites = []
    for kind, rx in (('call', call_re), ('iter', itr_re)):
        for m in rx.finditer(text):
            args = G.args_of(text, m.end() - 1)
            if kind == 'iter' and args == []:
                continue
            if args is not None and G.is_ec_call(args):
                continue
            v = line_of(m.start())
            if G.APPROVED_MARK in v.raw:
                continue
            enc = sorted((b for b in blocks if b[0] < m.start() < b[1]), key=lambda b: -b[0])
            fn = next((b for b in enc if b[3] == 'function'), None)
            lambdas = [b for b in enc if b[3] == 'lambda']
            tries = [b for b in enc if b[3] == 'try']
            sites.append(dict(
                where=f'{rel}:{v.no}', fn=m.group(1), code=v.code.strip()[:110],
                func=(f'{line_of(fn[0]).no}: {fn[2][-150:]}' if fn else '(none found)'),
                lambdas=[f'{line_of(b[0]).no}: {b[2][-100:]}' for b in lambdas],
                tries=[(line_of(b[0]).no, line_of(b[1]).no, b[4]) for b in tries]))
    return sites, unbalanced


def main(argv):
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == '--files-from' and i + 1 < len(argv):
            with open(argv[i + 1]) as f:
                files += [ln.strip() for ln in f if ln.strip() and not ln.startswith('#')]
            i += 2
            continue
        files.append(argv[i])
        i += 1
    nfiles = nsites = 0
    for p in files:
        ap = p if os.path.isabs(p) else os.path.join(WHAT, p)
        if not os.path.isfile(ap):
            print(f'FS49-CONTEXT: ERROR missing file: {p}')
            return 2
        rel = os.path.relpath(ap, WHAT)
        sites, unbalanced = scan(ap, rel)
        nfiles += 1
        if unbalanced:
            print(f'FS49-CONTEXT: WARNING {rel}: {unbalanced} unbalanced brace(s) in live code; '
                  f'block chains for this file need manual confirmation')
        for s in sites:
            nsites += 1
            print(f"[{nsites}] {s['where']}  {s['fn']}")
            print(f"     code:     {s['code']}")
            print(f"     function: {s['func']}")
            for lam in s['lambdas']:
                print(f'     lambda:   {lam}')
            if not s['tries']:
                print('     try:      NONE in this file around the call')
            for (o, c, catches) in s['tries']:
                cs = '; '.join(f'L{ln} catch({t})' for ln, t in catches) or '(no catch parsed)'
                print(f'     try:      L{o}-L{c} -> {cs}')
    print(f'FS49-CONTEXT: checked {nfiles} files, {nsites} throwing call sites')
    if nfiles == 0 or nsites == 0:
        print('FS49-CONTEXT: FAIL -- checked ZERO files or ZERO sites')
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
