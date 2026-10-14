#!/usr/bin/env python3
"""Builds the all-lock-scope inventory (markdown table) for SITES.md.
Usage: inventory.py ROOT [ROOT...]   (src/ + include/ under each ROOT)
For each lock scope: file:lines, lock type + mutex, enclosing context (registered
Lua function name via register_function/add_pair, else the enclosing function),
entry kind, raise evidence (direct tokens, banned/ambient helpers, own pcall,
Lua allocation APIs) and an automatic verdict. Exits 1 if it saw zero scopes."""
import os, re, sys, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scan_all_locks import blank, LOCK, RAISE, API, files_of, match_close, FUNCDEF, CFUNCDEF, KEYWORDS

REG = re.compile(r'(?:register_function|add_pair)\s*\(\s*"([^"]+)"')
BANNED = re.compile(r'(?<![\w])get_mod_ref\s*\(')
AMBIENT = re.compile(r'(\bget_function_ref\s*\(|\bauto_construct_object\s*\(|\bRemoteUnrealParam::construct\s*\()')
ALLOC = re.compile(r'\b(luaL_ref|lua_newthread|new_thread|make_ref|lua_pushlstring|lua_pushstring|lua_createtable|lua_newtable|lua_newuserdatauv)\b')
OWNPCALL = re.compile(r'\b(lua_pcall|call_function|execute_string|execute_file)\s*\(')

def contexts(c, t):
    out = []
    for m in REG.finditer(t):
        j = c.find('{', m.end())
        # the lambda body: first '{' after '-> int' / ')' following the lambda introducer
        k = c.find('[', m.end())
        j = c.find('{', c.find(')', k)) if k >= 0 else j
        if j >= 0: out.append((m.start(), match_close(c, j), 'registered Lua fn "%s"' % m.group(1), True))
    for rx in (FUNCDEF, CFUNCDEF):
        for m in rx.finditer(c):
            name = m.group(1)
            if name.split('::')[-1] in KEYWORDS: continue
            j = c.find('{', m.end() - (1 if rx is CFUNCDEF else 0))
            if j < 0: continue
            out.append((m.start(), match_close(c, j), name, False))
    return out

def main(roots):
    rows, nfiles = [], 0
    for root in roots:
        for f in sorted(files_of(root)):
            nfiles += 1
            t = open(f, encoding='utf-8', errors='replace').read(); c = blank(t)
            starts = [0] + [m.end() for m in re.finditer('\n', c)]
            ln = lambda i: bisect.bisect_right(starts, i)
            ctxs = contexts(c, t)
            for m in LOCK.finditer(c):
                d, i, end = 0, m.end(), len(c) - 1
                while i < len(c):
                    if c[i] == '{': d += 1
                    elif c[i] == '}':
                        if d == 0: end = i; break
                        d -= 1
                    i += 1
                body = c[m.end():end]
                inner = [x for x in ctxs if x[0] <= m.start() <= x[1]]
                reg = [x for x in inner if x[3]]
                # innermost registered lambda wins over a wrapping setup function
                pick = min(reg, key=lambda x: x[1] - x[0]) if reg else (min(inner, key=lambda x: x[1] - x[0]) if inner else None)
                ctx = pick[2] if pick else '(file scope / class member)'
                is_reg = bool(reg)
                direct = sorted(set(ln(m.end() + x.start()) for x in RAISE.finditer(body)))
                banned = sorted(set(ln(m.end() + x.start()) for x in BANNED.finditer(body)))
                amb = sorted(set(ln(m.end() + x.start()) for x in AMBIENT.finditer(body)))
                alloc = bool(ALLOC.search(body)); own = bool(OWNPCALL.search(body))
                rel = os.path.relpath(f, roots[0])
                ob = m.end(2) if False else None
                k0 = c.find('{', m.start(2)) if True else -1
                k1 = c.find('(', m.start(2))
                opn = min(x for x in (k0, k1) if x >= 0)
                depth, q = 0, opn
                while q < len(c):
                    if c[q] in '{(': depth += 1
                    elif c[q] in '})':
                        depth -= 1
                        if depth == 0: break
                    q += 1
                mutex_text = t[opn + 1:q].strip()
                rows.append(dict(file=rel, lock=ln(m.start()), end=ln(end), mutex=mutex_text, kind=m.group(1), ctx=ctx, reg=is_reg,
                                 direct=direct, banned=banned, amb=amb, alloc=alloc, own=own))
    print('| # | file:lines | lock (mutex) | context | entry | Lua raise inside | verdict |')
    print('|---|---|---|---|---|---|---|')
    for n, r in enumerate(rows, 1):
        ev = []
        if r['direct']: ev.append('direct raise at ' + ','.join(map(str, r['direct'])))
        if r['banned']: ev.append('get_mod_ref at ' + ','.join(map(str, r['banned'])))
        if r['amb']: ev.append('ambient helper at ' + ','.join(map(str, r['amb'])))
        if r['own']: ev.append('own pcall/call_function')
        if r['alloc']: ev.append('Lua alloc API (ERRMEM only)')
        if r['direct'] or r['banned']: v = 'SITE: needs C++ throw' if r['reg'] else 'SITE (native): needs C++ throw, local catch'
        elif r['amb']: v = 'residual: ambient-pcall only (WARN)'
        elif r['own']: v = 'safe: errors stay inside own pcall / C++ catch'
        elif r['alloc']: v = 'residual: allocation failure only'
        else: v = 'none: no Lua call/raise in scope'
        entry = 'TRY-wrapped (process_lua_function)' if r['reg'] else 'native/C++'
        mx = r['kind'] + ' ' + r['mutex'].replace('|', '/')
        print(f"| {n} | {r['file']}:{r['lock']}-{r['end']} | {mx} | {r['ctx']} | {entry} | {'; '.join(ev) or '-'} | {v} |")
    print(f'\ninventory: files checked={nfiles} lock scopes checked={len(rows)}', file=sys.stderr)
    if nfiles == 0 or not rows:
        print('inventory: FAIL (nothing scanned)', file=sys.stderr); return 1
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
