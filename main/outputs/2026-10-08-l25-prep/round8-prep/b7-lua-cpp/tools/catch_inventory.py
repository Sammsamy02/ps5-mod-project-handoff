#!/usr/bin/env python3
"""B7 catch-site inventory (Mac-only, read-only). Extends round-6 catch_all_audit_v2.py.

What is new compared with v2:
  * ALL catch handlers, not only catch(...): catch(...), catch(void*), catch(std::exception&)
    and every other typed handler.
  * File set = every in-repo file the B4 build compiled or included, from the COMPOSITION
    receipt: 378 unit sources + every repo path in their .d files (4,957 deps, incl. the
    1,777 generated FunctionBodies/*.cpp that v2 skipped because it kept only header deps).
    libc++/SDK headers and Lua's own C sources are excluded (listed separately in the report).
  * Marks, per site: try-block calls Lua (D/A/G rules from v2, name-collision-safe closure),
    try-block is a protected call (lua_pcall / call_function), handler rethrows (throw; or
    rethrow_exception), enclosing function is noexcept / destructor.
  * Key fact encoded in the classification: Lua's C++ throw is `throw(c)` with c a
    `struct lua_longjmp *` (ldo.c:58). A pointer is never matched by catch(std::exception&)
    or by any non-pointer type, and not by catch(const char*) etc. Only catch(...) and
    catch(void*) (and catch(lua_longjmp*), impossible outside ldo.c: incomplete type) match.

Output: ../audit/CATCH-INVENTORY.json and ../audit/catch-inventory.log (relative to this file).
Fails (exit 1) on zero files, zero functions or zero catch sites.
Usage: python3 -I catch_inventory.py
"""
import hashlib, json, re, sys
from pathlib import Path
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / 'audit'
WHAT = HERE.parents[4]
REC = WHAT / 'outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json'
SDK = (WHAT / 'outputs/ps5-mod-bridge').resolve()
LUARAW = (WHAT / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw').resolve()
EXT = {'.cpp', '.cc', '.cxx', '.hpp', '.h', '.inl', '.inc'}

KEYWORDS = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'alignof', 'static_assert',
            'noexcept', 'throw', 'new', 'delete', 'typeid', 'requires', 'co_await', 'co_return', 'defined', 'else',
            'do', 'case', 'try', 'operator', 'alignas', 'static_cast', 'reinterpret_cast', 'const_cast', 'dynamic_cast'}
DIRECT = re.compile(r'\b(lua_\w+|luaL_\w+)\s*\(|\b(lua|m_lua|data_owner|lua_instance|Lua)\s*(\.|->)\s*\w+\s*[(<]')
LUA_ARG = re.compile(r'(?<![\w.])(L|lua|lua_state|LuaState|m_lua|lua_instance|data_owner)(?!\w)|get_lua_state\s*\(|\bInvocation\b|->\s*Lua\b|\.Lua\b')
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*(?:<[^<>;(){}]*>)?\s*\(')
QUALS = re.compile(r'^(?:\s|const\b|override\b|final\b|mutable\b|noexcept(?:\s*\([^()]*\))?|&&|&)*(?:->\s*[^{;=()]+?)?\s*$')
PCALL = re.compile(r'\blua_pcallk?\s*\(|\bcall_function\s*\(|\bprotected_\w+\s*\(')


def strip(t):
    out, i, n = [], 0, len(t)
    blank = lambda s: ''.join('\n' if ch == '\n' else ' ' for ch in s)
    while i < n:
        c = t[i]
        if t.startswith('//', i):
            j = t.find('\n', i); j = n if j < 0 else j
            out.append(' ' * (j - i)); i = j
        elif t.startswith('/*', i):
            j = t.find('*/', i + 2); j = n if j < 0 else j + 2
            out.append(blank(t[i:j])); i = j
        elif c == 'R' and t.startswith('R"', i) and (i == 0 or not (t[i - 1].isalnum() or t[i - 1] == '_') or t[i - 1] in 'uUL8'):
            d0 = i + 2; d1 = t.find('(', d0)
            delim = t[d0:d1] if d1 >= 0 else ''
            end = t.find(')' + delim + '"', d1 + 1) if d1 >= 0 else -1
            j = n if end < 0 else end + len(delim) + 2
            out.append(blank(t[i:j])); i = j
        elif c in '"\'':
            j = i + 1
            while j < n and t[j] != c and t[j] != '\n':
                j += 2 if t[j] == '\\' else 1
            seg = t[i + 1:min(j, n)]
            closed = j < n and t[j] == c
            out.append(c + blank(seg) + (c if closed else '')); i = j + 1 if closed else j
        else:
            out.append(c); i += 1
    return ''.join(out)


def match_brace(t, i):
    depth = 0
    for j in range(i, len(t)):
        if t[j] == '{': depth += 1
        elif t[j] == '}':
            depth -= 1
            if depth == 0: return j
    return -1


def find_funcs(t):
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


def call_args(t, op):
    depth = 0
    for j in range(op, min(len(t), op + 4000)):
        if t[j] == '(': depth += 1
        elif t[j] == ')':
            depth -= 1
            if depth == 0: return t[op + 1:j]
    return ''


def calls_in(body):
    out = []
    for m in CALL.finditer(body):
        if m.group(1) in KEYWORDS: continue
        out.append((m.group(1), call_args(body, m.end() - 1)))
    return out


def rel(p): return str(p).replace(str(WHAT) + '/', '')


def classify_type(kind):
    k = re.sub(r'\s+', ' ', kind.strip())
    if k == '...': return 'all'
    if re.fullmatch(r'(const )?void ?\*( ?const)?', k) or re.fullmatch(r'(const )?void ?\* ?\w*', k): return 'void*'
    if '*' in k: return 'other-pointer'
    if re.search(r'exception|_error|bad_|runtime_error|logic_error|std::', k): return 'std-exception-family'
    return 'other-typed'


def file_set():
    rec = json.loads(REC.read_text())
    files = set()
    for u in rec['selected_units']:
        src = Path(u['source']).resolve()
        if src.suffix in EXT: files.add(src)
        dep = u['dependency']; dep = Path(dep['path'] if isinstance(dep, dict) else dep)
        if not dep.exists(): continue
        for tok in dep.read_text(errors='replace').replace('\\\n', ' ').split():
            if tok.endswith(':') or not tok.startswith('/'): continue
            p = Path(tok).resolve()
            if p.suffix not in EXT: continue
            if p.is_relative_to(SDK) or p.is_relative_to(LUARAW): continue
            if '/.superpowers/native-dependency-spike/tools/' in str(p): continue
            if 'include/c++/' in str(p) or '/prospero' in str(p).lower(): continue
            files.add(p)
    return sorted(f for f in files if f.exists() and str(f).startswith(str(WHAT)))


def load_texts(files, overlay=None):
    """stripped text per file; if overlay (a directory laid out like the worktree root) holds a file with the
    same worktree-relative path, the overlay copy is analysed instead (used to test a STAGED tree)."""
    texts = {}
    for f in files:
        src = f
        if overlay is not None:
            cand = Path(overlay) / f.relative_to(WHAT)
            if cand.exists(): src = cand
        texts[f] = strip(src.read_text(errors='replace'))
    return texts


def analyze(texts):
    funcs = []
    for f, t in texts.items():
        for qual, params, quals, b0 in find_funcs(t):
            name = qual.split('::')[-1]
            if name in KEYWORDS or name.lstrip('~') in KEYWORDS: continue
            b1 = match_brace(t, b0)
            if b1 < 0: continue
            funcs.append({'file': f, 'line': t.count('\n', 0, b0) + 1, 'b0': b0, 'b1': b1, 'qual': qual, 'name': name,
                          'params': params, 'quals': quals, 'body': t[b0:b1 + 1]})
    for fn in funcs:
        fn['calls'] = calls_in(fn['body'])
        fn['direct'] = bool(DIRECT.search(fn['body'])) or any(LUA_ARG.search(a) for _, a in fn['calls'])
    by_name = {}
    for fn in funcs: by_name.setdefault(fn['name'], []).append(fn)
    raising = {}
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
                hit = '<direct>' if fn['direct'] else next((c for c, _ in fn['calls'] if c in raising and c != name), None)
                if hit is None: wit = None; break
                wit = wit or (fn, hit)
            if wit:
                fn, hit = wit
                raising[name] = [f"{fn['qual']} ({rel(fn['file'])}:{fn['line']}) -> {hit}"] + raising.get(hit, [])[:4]
                changed = True
    funcs_by_file = {}
    for fn in funcs: funcs_by_file.setdefault(fn['file'], []).append(fn)
    sites = []
    for f, t in texts.items():
        for m in re.finditer(r'\bcatch\s*\(([^)]*)\)\s*\{', t):
            hb0 = m.end() - 1; hb1 = match_brace(t, hb0)
            if hb1 < 0: continue
            handler = t[hb0:hb1 + 1]
            j = m.start()
            body = ''; try_line = None
            for _ in range(8):
                k = j
                while k > 0 and t[k - 1] in ' \t\r\n': k -= 1
                if k > 0 and t[k - 1] == '}':
                    depth = 0; q = k - 1
                    while q >= 0:
                        if t[q] == '}': depth += 1
                        elif t[q] == '{':
                            depth -= 1
                            if depth == 0: break
                        q -= 1
                    if q < 0: break
                    pre = t[max(0, q - 600):q].rstrip()
                    if re.search(r'\btry\s*$', pre) or re.search(r'\btry\s*:[^{}]*$', pre):
                        body = t[q:k]; try_line = t.count('\n', 0, q) + 1; break
                    cm = re.search(r'\bcatch\s*\([^)]*\)\s*$', pre)
                    if cm:
                        j = max(0, q - 600) + cm.start(); continue
                break
            kind = m.group(1).strip()
            ctype = classify_type(kind)
            line = t.count('\n', 0, m.start()) + 1
            reasons = []
            if body:
                if DIRECT.search(body): reasons.append('D:' + DIRECT.search(body).group(0).strip()[:40])
                for c, a in calls_in(body):
                    if LUA_ARG.search(a): reasons.append(f'A:{c}(...{LUA_ARG.search(a).group(0)}...)'); break
                for c, _ in calls_in(body):
                    if c in raising: reasons.append('G:' + c + ' => ' + ' | '.join(raising[c][:3])); break
            enc = None
            for fn in funcs_by_file.get(f, []):
                if fn['b0'] < m.start() < fn['b1'] and (enc is None or fn['b0'] > enc['b0']): enc = fn
            rethrow = bool(re.search(r'\bthrow\s*;|\brethrow_exception\s*\(', handler))
            # a catch(void*) { throw; } handler directly before this catch(...) makes it safe for Lua errors
            before = t[max(0, m.start() - 600):m.start()].rstrip()
            void_first = bool(re.search(r'catch\s*\(\s*void\s*\*\s*\)\s*\{[^{}]*\bthrow\s*;[^{}]*\}\s*$', before)) or \
                bool(re.search(r'catch\s*\(\s*void\s*\*\s*\)\s*\{[^{}]*\bthrow\s*;[^{}]*\}', before[-400:]))
            enc_ne = bool(enc and re.search(r'\bnoexcept\b(?!\s*\(\s*false\s*\))', enc['quals']))
            enc_dtor = bool(enc and enc['name'].startswith('~'))
            sites.append({'file': rel(f), 'line': line, 'try_line': try_line, 'catch_type': kind, 'class': ctype,
                          'lua_in_try': reasons, 'pcall_in_try': bool(PCALL.search(body)),
                          'rethrows': rethrow, 'void_pass_first': void_first, 'handler_len_lines': handler.count('\n'),
                          'handler_calls_lua': bool(DIRECT.search(handler) or re.search(r'\bthrow_error\w*\s*\(', handler)),
                          'enclosing': enc['qual'] if enc else None, 'enclosing_line': enc['line'] if enc else None,
                          'enclosing_noexcept': enc_ne, 'enclosing_dtor': enc_dtor,
                          'handler_sha8': hashlib.sha256(re.sub(r'\s+', ' ', handler).encode()).hexdigest()[:8]})
    h2 = []
    for fn in funcs:
        ne = re.search(r'\bnoexcept\b(?!\s*\(\s*false\s*\))', fn['quals']); dtor = fn['name'].startswith('~')
        if not (ne or dtor): continue
        why = None
        if fn['direct']: why = 'calls Lua directly'
        else:
            for c, _ in fn['calls']:
                if c in raising: why = 'G:' + c + ' => ' + ' | '.join(raising[c][:2]); break
        if why: h2.append({'file': rel(fn['file']), 'line': fn['line'], 'function': fn['qual'],
                           'kind': 'destructor' if dtor else 'noexcept', 'why': why})
    return funcs, raising, sites, h2


def main():
    overlay = None
    if len(sys.argv) >= 3 and sys.argv[1] == '--overlay': overlay = sys.argv[2]
    files = file_set()
    texts = load_texts(files, overlay)
    funcs, raising, sites, h2 = analyze(texts)
    from collections import Counter
    cls = Counter(s['class'] for s in sites)
    # sites that can see a Lua throw
    seeing = [s for s in sites if s['class'] in ('all', 'void*')]
    flagged = [s for s in seeing if s['lua_in_try'] and not s['rethrows'] and not s['void_pass_first']]
    log = []
    log.append(f'files scanned: {len(files)} (repo-owned compiled/included, libc++/SDK/LuaRaw excluded); functions indexed: {len(funcs)}; names that can reach Lua: {len(raising)}')
    log.append(f'catch handlers (all kinds): {len(sites)}; by class: {dict(cls)}')
    log.append(f'handlers that match a Lua C++ throw (catch(...) or catch(void*)): {len(seeing)}; of which rethrow: {sum(s["rethrows"] for s in seeing)}')
    log.append(f'  ... with Lua reachable in the try block and NO rethrow (REVIEW): {len(flagged)}')
    for s in sorted(flagged, key=lambda s: (s['file'], s['line'])):
        log.append(f"  REVIEW {s['file']}:{s['line']} (try :{s['try_line']}) in {s['enclosing']} {'[noexcept]' if s['enclosing_noexcept'] else ''} | {' ; '.join(s['lua_in_try'])[:240]}")
    log.append(f'noexcept functions / destructors that can reach Lua (H2): {len(h2)}')
    for x in h2: log.append(f"  H2 {x['file']}:{x['line']} {x['kind']} {x['function']} -- {x['why'][:160]}")
    stdsites = [s for s in sites if s['class'] == 'std-exception-family' and s['lua_in_try']]
    log.append(f'std-exception-family handlers with Lua reachable in the try block (cannot catch a Lua throw; listed for the report): {len(stdsites)}')
    for s in sorted(stdsites, key=lambda s: (s['file'], s['line'])):
        log.append(f"  STD {s['file']}:{s['line']} catch({s['catch_type']}) in {s['enclosing']} pcall_in_try={s['pcall_in_try']}")
    OUT.mkdir(exist_ok=True)
    (OUT / ('CATCH-INVENTORY.json' if overlay is None else 'CATCH-INVENTORY-staged.json')).write_text(json.dumps({'files': len(files), 'functions': len(funcs), 'sites': sites, 'h2': h2,
                                                          'class_counts': dict(cls)}, indent=1) + '\n')
    (OUT / ('catch-inventory.log' if overlay is None else 'catch-inventory-staged.log')).write_text('\n'.join(log) + '\n')
    print('\n'.join(log))
    if not files or not funcs or not sites:
        print('FAIL: a zero count (files, functions or catch sites)'); return 1
    print('DONE')
    return 0


if __name__ == '__main__':
    sys.exit(main())
