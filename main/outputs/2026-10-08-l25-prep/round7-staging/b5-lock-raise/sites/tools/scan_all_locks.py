#!/usr/bin/env python3
"""Read-only survey: every lock scope (any std mutex type) in a source tree,
and for each scope the Lua raises inside it, either DIRECT (throw_error /
luaL_error / lua_error / throw_lua_error / luaL_check*) or via a HELPER (a
named function whose body contains a raise or calls such a helper).
Usage: scan_all_locks.py ROOT [ROOT...]   (scans src/ and include/ under ROOT)
Prints counts; exits 1 if it checked zero files or zero lock scopes."""
import re, sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
RAW = re.compile(r'R"([^\s()\\]{0,16})\(')

def blank(src):
    out, i, n = [], 0, len(src)
    def keep(a, b): out.append(''.join('\n' if ch == '\n' else ' ' for ch in src[a:b]))
    while i < n:
        c = src[i]
        m = RAW.match(src, i) if c == 'R' else None
        if m:
            close = ')' + m.group(1) + '"'
            j = src.find(close, m.end()); j = n if j < 0 else j + len(close); keep(i, j); i = j
        elif src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j; keep(i, j); i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2; keep(i, j); i = j
        elif c in '"\'':
            j = i + 1
            while j < n and src[j] != c and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n); keep(i, j); i = j
        else:
            out.append(c); i += 1
    return ''.join(out)

LOCK = re.compile(r'\b(?:std::)?(lock_guard|unique_lock|scoped_lock|shared_lock)\s*(?:<\s*[^>]*>)?\s*(\w+)\s*[{(]\s*([^})]*)[})]')
RAISE = re.compile(r'(\.\s*throw_error\s*\(|\bthrow_lua_error\s*\(|(?<![\w.])throw_error\s*\(|\bthrow_error_from_buffer\s*\(|\bluaL_error\s*\(|\blua_error\s*\(|\bluaL_arg(?:error|check)\s*\(|\bluaL_check\w*\s*\(|\bluaL_opt\w*\s*\()')
API = re.compile(r'\b(lua_call|lua_callk|lua_getglobal|lua_setglobal|lua_getfield|lua_setfield|lua_gettable|lua_settable|lua_geti|lua_seti|lua_next|lua_concat|lua_len|luaL_len|luaL_tolstring|luaL_ref|lua_createtable|lua_newtable|lua_pushstring|lua_pushlstring|lua_pushfstring|lua_pushvfstring|lua_newuserdatauv|lua_newthread|luaL_traceback|luaL_loadbuffer|luaL_loadstring|luaL_loadfile|luaL_dostring|lua_resume|lua_yield|lua_close|lua_gc|lua_arith|lua_compare|lua_xmove|lua_pcall|lua_pcallk|luaL_getmetatable|luaL_newmetatable|luaL_setmetatable|call_function|execute_string|execute_file|prepare_function_call|get_function_ref|get_table_ref)\s*\(')
CFUNCDEF = re.compile(r'^[ \t]*(?:static\s+|inline\s+|constexpr\s+|extern\s+"C"\s+)*(?:[A-Za-z_][\w:<>*&]*\s+)+\*?&?((?:\w+::)*\w+)\s*\([^;{}()]*(?:\([^()]*\)[^;{}()]*)*\)\s*(?:const\s*)?(?:noexcept\s*)?\{', re.M)
KEYWORDS = {'if','for','while','switch','catch','return','else','sizeof','do'}
FUNCDEF = re.compile(r'^[ \t]*(?:template\s*<[^>]*>\s*)?(?:static\s+|inline\s+|constexpr\s+|\[\[nodiscard\]\]\s+|RC_\w+\s+)*auto\s+((?:\w+::)*~?\w+)\s*\(', re.M)

def files_of(root):
    for sub in ('src', 'include'):
        for dp, dn, fn in os.walk(os.path.join(root, sub)):
            for f in fn:
                if f.endswith(('.cpp', '.hpp', '.h', '.inc')) and not f.endswith(('.orig', '.rej')):
                    yield os.path.join(dp, f)

def match_close(text, open_idx):
    d = 0
    for i in range(open_idx, len(text)):
        if text[i] == '{': d += 1
        elif text[i] == '}':
            d -= 1
            if d == 0: return i
    return len(text) - 1

def main(roots):
    data = {}
    for root in roots:
        for f in files_of(root):
            t = open(f, encoding='utf-8', errors='replace').read()
            data[f] = (t, blank(t))
    # function bodies containing raises
    funcs = {}  # short name -> list of (file, start, end, body)
    for f, (t, c) in data.items():
        for m in FUNCDEF.finditer(c):
            j = c.find('{', m.end())
            k = c.find(';', m.end())
            if j < 0 or (0 <= k < j): continue
            # skip lambdas/params containing braces roughly: take first { after ')' that follows '->'
            e = match_close(c, j)
            short = m.group(1).split('::')[-1]
            funcs.setdefault(short, []).append((f, j, e))
    for f, (t, c) in data.items():
        for m in CFUNCDEF.finditer(c):
            short = m.group(1).split('::')[-1]
            if short in KEYWORDS: continue
            j = m.end() - 1
            funcs.setdefault(short, []).append((f, j, match_close(c, j)))
    raising = {}
    for short, defs in funcs.items():
        for (f, j, e) in defs:
            body = data[f][1][j:e]
            if RAISE.search(body): raising[short] = True
    # fixpoint (names only, overapprox)
    changed = True
    while changed:
        changed = False
        for short, defs in funcs.items():
            if short in raising: continue
            for (f, j, e) in defs:
                body = data[f][1][j:e]
                for r in list(raising):
                    if re.search(r'\b' + re.escape(r) + r'\s*[(<]', body):
                        raising[short] = True; changed = True; break
                if short in raising: break
    # drop obviously non-raising generic names
    nofiles, scopes_total, report = 0, 0, []
    for f, (t, c) in sorted(data.items()):
        nofiles += 1
        starts = [0]
        for m in re.finditer('\n', c): starts.append(m.end())
        import bisect
        def line_of(idx): return bisect.bisect_right(starts, idx)
        for m in LOCK.finditer(c):
            # scope end: the enclosing block close
            d, i = 0, m.end(); end = len(c) - 1
            while i < len(c):
                if c[i] == '{': d += 1
                elif c[i] == '}':
                    if d == 0: end = i; break
                    d -= 1
                i += 1
            scopes_total += 1
            body = c[m.end():end]
            direct = [(line_of(m.end() + x.start()), x.group(0)) for x in RAISE.finditer(body)]
            helpers = []
            for r in raising:
                for x in re.finditer(r'(?<![\w])' + re.escape(r) + r'\s*[(<]', body):
                    helpers.append((line_of(m.end() + x.start()), r))
            report.append(dict(file=os.path.relpath(f, roots[0]), kind=m.group(1), mutex=m.group(3).strip(), lock_line=line_of(m.start()), end_line=line_of(end), direct=direct, helpers=sorted(set(helpers)), api=sorted(set(x.group(1) for x in API.finditer(body)))))
    print(json.dumps(report, indent=0))
    print(f'scan_all_locks: files checked={nofiles} lock scopes checked={scopes_total} raising-helper names={len(raising)}', file=sys.stderr)
    if nofiles == 0 or scopes_total == 0:
        print('scan_all_locks: FAIL (nothing scanned)', file=sys.stderr); return 1
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
