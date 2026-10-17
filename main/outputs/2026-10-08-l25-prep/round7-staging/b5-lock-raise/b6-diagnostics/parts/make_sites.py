#!/usr/bin/env python3
"""Writes SITES.txt: the __LINE__ of every b6diag::ActionsLock declaration in work/src/LuaMod.cpp with
its enclosing function (heuristic text scan). [LOCKWAIT] site_line= / holder_line= are these numbers,
valid for THIS staged file only (any offset when patching shifts them: re-run after applying).
Prints how many sites it found; exits 1 when none."""
import os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
lines = open(os.path.join(root, 'work/src/LuaMod.cpp'), encoding='utf-8').read().split('\n')
decl = re.compile(r'b6diag::ActionsLock\s+\w+\s*[{(]\s*(?:LuaMod::)?m_thread_actions_mutex\s*,\s*__LINE__\s*[})]')
reg = re.compile(r'register_function\("(\w+)"')
fn = re.compile(r'^(?: {0,8})(?:static\s+|inline\s+|\[\[maybe_unused\]\]\s+)*(?:auto|void|bool|int)\s+(?:static\s+)?((?:\w+::)*\w+)\s*\(')
cls = re.compile(r'^ {0,8}(?:class|struct)\s+(\w+)')
out = []
for i, l in enumerate(lines):
    if not decl.search(l):
        continue
    ctx = None
    for j in (range(i, max(-1, i - 8), -1) if ' Lock{' in l else ()):  # a lock member declared right inside a class
        m = cls.match(lines[j])
        if m:
            ctx = 'C++ ' + m.group(1); break
    for j in range(i, max(-1, i - 600), -1) if ctx is None else ():
        m = reg.search(lines[j])
        if m:
            ctx = 'Lua function ' + m.group(1); break
        m = fn.match(lines[j])
        if m and m.group(1) not in ('if', 'for', 'while'):
            ctx = 'C++ ' + m.group(1); break
    out.append('%5d  %s' % (i + 1, ctx or '?'))
if not out:
    print('make_sites: no ActionsLock declarations found'); sys.exit(1)
hdr = ['# Lock site map for B6 (work/src/LuaMod.cpp = the file b6.diff produces on top of final-b5).',
       '# [LOCKWAIT] site_line= and holder_line= are these line numbers (__LINE__ of the ActionsLock declaration).',
       '# Context is the nearest enclosing register_function / function found by a text scan (heuristic).']
open(os.path.join(root, 'SITES.txt'), 'w').write('\n'.join(hdr + out) + '\n')
print('make_sites: %d sites' % len(out))
