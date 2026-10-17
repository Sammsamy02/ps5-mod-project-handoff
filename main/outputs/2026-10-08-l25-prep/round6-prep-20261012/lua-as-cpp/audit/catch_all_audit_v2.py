#!/usr/bin/env python3
"""catch(...) / noexcept audit v2 (call graph) for Lua-as-C++ (Mac-only, read-only).

Round 1 (catch_all_audit.py) only looked for Lua calls written directly in a
try block, and missed LuaUObject.cpp:142 and LuaMadeSimple.cpp:1586. v2 flags
a non-rethrowing catch(...) when its try block contains ANY of:
  D  a direct Lua API call (lua_* / luaL_*), or a member call on a Lua object
     (lua. / lua-> / m_lua / data_owner ...)
  A  a call whose argument list mentions a Lua state (L, lua, lua_state,
     LuaState, get_lua_state(), *Invocation, Invocation->Lua ...)
  G  a call to a function that can raise a Lua error through the call graph:
     a name defined in the scanned code whose body (transitively) contains a
     D or A call. Name-based (overloads merged, so it over-approximates).
It also lists `noexcept` functions and destructors whose body reaches Lua the
same way (hazard H2: a Lua error crossing them calls std::terminate).
Rethrowing handlers (`throw;`) and handlers that start with
`catch (void*) { throw; }` (the staged fix) are not flagged.
Also counts throw-expressions whose operand is visibly a pointer (the staged
fix assumes no code throws object pointers besides Lua itself).
Prints how many files/sites/functions it checked; exits non-zero on zero.
Usage: python3 -I catch_all_audit_v2.py
"""
import json, re, sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import catch_all_audit as v1  # read-only reuse: file set, comment stripper, brace matcher
sys.path.pop(0)

KEYWORDS = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'alignof', 'static_assert',
            'noexcept', 'throw', 'new', 'delete', 'typeid', 'requires', 'co_await', 'co_return', 'defined', 'else',
            'do', 'case', 'try', 'operator', 'alignas', 'static_cast', 'reinterpret_cast', 'const_cast', 'dynamic_cast'}
DIRECT = re.compile(r'\b(lua_\w+|luaL_\w+)\s*\(|\b(lua|m_lua|data_owner|lua_instance|Lua)\s*(\.|->)\s*\w+\s*[(<]')
LUA_ARG = re.compile(r'(?<![\w.])(L|lua|lua_state|LuaState|m_lua|lua_instance|data_owner)(?!\w)|get_lua_state\s*\(|\bInvocation\b|->\s*Lua\b|\.Lua\b')
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*(?:<[^<>;(){}]*>)?\s*\(')
QUALS = re.compile(r'^(?:\s|const\b|override\b|final\b|mutable\b|noexcept(?:\s*\([^()]*\))?|&&|&)*(?:->\s*[^{;=()]+?)?\s*$')

def find_funcs(t):
    """Yield (name, params, quals, body_start) for every '{' that follows ')'
    plus qualifiers (a function body). Linear scan, no backtracking regex.
    Constructor init lists (': a(x), b(y) {') are not recognised (minor gap)."""
    for b in re.finditer(r'\{', t):
        i = b.start()
        j = t.rfind(')', max(0, i - 300), i)
        if j < 0: continue
        quals = t[j + 1:i]
        if not QUALS.match(quals): continue
        depth = 0; k = j
        while k >= 0 and j - k < 4000:
            if t[k] == ')': depth += 1
            elif t[k] == '(':
                depth -= 1
                if depth == 0: break
            k -= 1
        if k < 0 or depth: continue
        m = re.search(r'((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*$', t[max(0, k - 200):k])
        if not m: continue
        yield m.group(1), t[k + 1:j], quals, i

def call_args(t, open_paren):
    depth = 0
    for j in range(open_paren, min(len(t), open_paren + 4000)):
        if t[j] == '(': depth += 1
        elif t[j] == ')':
            depth -= 1
            if depth == 0: return t[open_paren + 1:j]
    return ''

def calls_in(body):
    out = []
    for m in CALL.finditer(body):
        name = m.group(1)
        if name in KEYWORDS: continue
        args = call_args(body, m.end() - 1)
        out.append((name, args))
    return out

def main():
    # same file set as v1
    rec = json.loads((v1.BUILD / 'pass1/COMPOSITION-RECEIPT.json').read_text())
    import shlex
    files = set()
    for u in rec['selected_units']:
        src = Path(u['source'])
        if src.suffix in ('.cpp', '.cc', '.cxx'): files.add(src.resolve())
        dep = Path(u['dependency'])
        if dep.exists():
            for x in shlex.split(dep.read_text().replace('\\\n', ' '))[1:]:
                p = Path(x).resolve()
                if p.suffix in ('.hpp', '.h', '.inl', '.inc') and not p.is_relative_to(v1.SDK.resolve()) \
                        and not p.is_relative_to(v1.LUARAW.resolve()) and '/.superpowers/native-dependency-spike/tools/' not in str(p):
                    files.add(p)
    files = sorted(f for f in files if f.exists())
    texts = {f: v1.strip_comments_and_strings(f.read_text(errors='replace')) for f in files}
    # function index
    funcs = []  # (file, line, qualname, params, quals, body)
    for f, t in texts.items():
        for qual, params, quals, b0 in find_funcs(t):
            name = qual.split('::')[-1]
            if name in KEYWORDS or name.lstrip('~') in KEYWORDS: continue
            b1 = v1.match_brace(t, b0)
            if b1 < 0: continue
            funcs.append({'file': f, 'line': t.count('\n', 0, b0) + 1, 'qual': qual, 'name': name,
                          'params': params, 'quals': quals, 'body': t[b0:b1 + 1]})
    for fn in funcs:
        fn['calls'] = calls_in(fn['body'])
        fn['direct'] = bool(DIRECT.search(fn['body'])) or any(LUA_ARG.search(a) for _, a in fn['calls'])
    by_name = {}
    for fn in funcs: by_name.setdefault(fn['name'], []).append(fn)
    # G closure, collision-safe: a NAME can reach Lua only if EVERY definition
    # of that name (all overloads / same-named methods) reaches Lua. Generic
    # names (get, copy, end, add ...) that also have non-Lua definitions are
    # therefore NOT propagated -- that removes name-collision noise, at the
    # price of missing an overloaded name that only sometimes reaches Lua
    # (those calls are still caught by rules D and A at the site when they
    # pass a Lua state).
    raising = {}  # name -> witness chain
    for name, defs in by_name.items():
        if all(fn['direct'] for fn in defs):
            fn = defs[0]; raising[name] = [f"{fn['qual']} ({rel(fn['file'])}:{fn['line']}) calls Lua directly"]
    changed = True
    while changed:
        changed = False
        for name, defs in by_name.items():
            if name in raising: continue
            wit = None
            for fn in defs:
                hit = next((c for c, _ in fn['calls'] if c in raising and c != name), None) if not fn['direct'] else '<direct>'
                if hit is None: wit = None; break
                wit = wit or (fn, hit)
            if wit:
                fn, hit = wit
                raising[name] = [f"{fn['qual']} ({rel(fn['file'])}:{fn['line']}) -> {hit}"] + raising.get(hit, [])[:4]
                changed = True
    # catch(...) sites
    sites = []
    for f, t in texts.items():
        for m in re.finditer(r'\btry\s*\{', t):
            b0 = m.end() - 1; b1 = v1.match_brace(t, b0)
            if b1 < 0: continue
            body = t[b0:b1 + 1]; k = b1 + 1; fixed = False
            while True:
                h = re.match(r'\s*catch\s*\(([^)]*)\)\s*\{', t[k:])
                if not h: break
                hb0 = k + h.end() - 1; hb1 = v1.match_brace(t, hb0)
                if hb1 < 0: break
                handler = t[hb0:hb1 + 1]; kind = h.group(1).strip()
                if re.fullmatch(r'void\s*\*', kind) and re.search(r'\bthrow\s*;', handler): fixed = True
                if kind == '...':
                    line = t.count('\n', 0, k + t[k:].find('catch')) + 1
                    reasons = []
                    if DIRECT.search(body): reasons.append('D:' + DIRECT.search(body).group(0).strip()[:40])
                    for c, a in calls_in(body):
                        if LUA_ARG.search(a): reasons.append(f'A:{c}(...{LUA_ARG.search(a).group(0)}...)'); break
                    for c, _ in calls_in(body):
                        if c in raising: reasons.append('G:' + c + ' => ' + ' | '.join(raising[c][:3])); break
                    sites.append({'file': rel(f), 'line': line, 'try_line': t.count('\n', 0, m.start()) + 1,
                                  'rethrows': bool(re.search(r'\bthrow\s*;', handler)), 'void_ptr_rethrow_first': fixed,
                                  'reasons': reasons})
                k = hb1 + 1
    flagged = [s for s in sites if s['reasons'] and not s['rethrows'] and not s['void_ptr_rethrow_first']]
    # H2: noexcept functions / destructors reaching Lua
    h2 = []
    for fn in funcs:
        ne = re.search(r'\bnoexcept\b(?!\s*\(\s*false\s*\))', fn['quals'])
        dtor = fn['name'].startswith('~')
        if not (ne or dtor): continue
        why = None
        if fn['direct']: why = 'calls Lua directly'
        else:
            for c, _ in fn['calls']:
                if c in raising: why = 'G:' + c + ' => ' + ' | '.join(raising[c][:2]); break
        if why: h2.append({'file': rel(fn['file']), 'line': fn['line'], 'function': fn['qual'], 'kind': 'destructor' if dtor else 'noexcept', 'why': why})
    # pointer throws
    ptr_throw = []
    for f, t in texts.items():
        for m in re.finditer(r'\bthrow\s+(new\b|&|this\b|nullptr\b|NULL\b|\(\s*\w+\s*\*\s*\))', t):
            ptr_throw.append(f'{rel(f)}:{t.count(chr(10), 0, m.start()) + 1}')
    print(f'files scanned: {len(files)}; functions indexed: {len(funcs)}; names that can reach Lua: {len(raising)}')
    print(f'catch(...) handlers: {len(sites)}; rethrowing: {sum(s["rethrows"] for s in sites)}; '
          f'non-rethrowing that can see a Lua error (D/A/G): {len(flagged)}')
    for s in sorted(flagged, key=lambda s: (s['file'], s['line'])):
        print(f"  REVIEW {s['file']}:{s['line']} (try at :{s['try_line']}) {' ; '.join(s['reasons'])[:300]}")
    print(f'noexcept functions / destructors that can reach Lua (H2): {len(h2)}')
    for x in h2: print(f"  H2 {x['file']}:{x['line']} {x['kind']} {x['function']} -- {x['why'][:200]}")
    print(f'throw-expressions with a visible pointer operand: {len(ptr_throw)} {ptr_throw[:10]}')
    out = {'files_scanned': len(files), 'functions_indexed': len(funcs), 'raising_names': len(raising),
           'catch_all_sites': len(sites), 'flagged': flagged, 'h2': h2, 'pointer_throws': ptr_throw,
           'raising_witness': {k: v for k, v in sorted(raising.items())}}
    (HERE / 'CATCH-ALL-AUDIT-V2.json').write_text(json.dumps(out, indent=1) + '\n')
    if not files or not sites or not funcs:
        print('FAIL: a zero count (files, catch sites or functions)'); return 1
    print('DONE (lists sites for hand review; does not pass/fail code). Results: CATCH-ALL-AUDIT-V2.json')
    return 0

def rel(p): return str(p).replace(str(v1.WHAT) + '/', '')

if __name__ == '__main__':
    sys.exit(main())
