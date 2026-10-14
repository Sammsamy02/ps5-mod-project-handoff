#!/usr/bin/env python3
"""catch(...) audit for compiling Lua as C++ (Mac-only, read-only).

With Lua compiled as C++, a Lua error is a C++ throw. Any `catch (...)` that
runs while a Lua error unwinds through it will now SEE that error. If it does
not rethrow (`throw;`), the Lua error is swallowed: the host test proves this
leaves Lua's global lock held and the pcall status/message wrong.

Scope: every C++ source the B4 build compiled (l26-A pass1 receipt units) plus
every in-repo header those units consumed (from their .d files), excluding
the SDK/libc++ and Lua's own sources. For each try-block with a catch(...)
handler it reports: file:line, whether the try block calls Lua directly
(lua_*/luaL_* or LuaMadeSimple raising helpers), and whether the catch(...)
rethrows. Prints counts; exits non-zero if zero files or zero sites scanned.
Usage: python3 -I catch_all_audit.py
"""
import json, re, shlex, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[4]
BUILD = WHAT / 'outputs/2026-10-08-l26-build-variants/l26-A'
SDK = WHAT / 'outputs/ps5-mod-bridge'
LUARAW = WHAT / '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw'
LUA_CALL = re.compile(r'\b(lua_\w+|luaL_\w+|throw_error\w*|get_userdata\w*|call_function\w*|TRY)\s*[(<]')
LUA_MEMBER = re.compile(r'\b(lua|m_lua|data_owner|lua_instance)\s*(\.|->)\s*\w+\s*[(<]')
PCALL = re.compile(r'\blua_pcallk?\s*\(|\bpcall\w*\s*\(|\bcall_function\s*\(')

def strip_comments_and_strings(t):
    out, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith('//', i):
            j = t.find('\n', i); j = n if j < 0 else j
            out.append(' ' * (j - i)); i = j
        elif t.startswith('/*', i):
            j = t.find('*/', i + 2); j = n if j < 0 else j + 2
            out.append(''.join('\n' if ch == '\n' else ' ' for ch in t[i:j])); i = j
        elif c == 'R' and t.startswith('R"', i) and (i == 0 or not (t[i - 1].isalnum() or t[i - 1] == '_') or t[i - 1] in 'uUL8'):
            d0 = i + 2; d1 = t.find('(', d0)
            delim = t[d0:d1] if d1 >= 0 else ''
            end = t.find(')' + delim + '"', d1 + 1) if d1 >= 0 else -1
            j = n if end < 0 else end + len(delim) + 2
            out.append(''.join('\n' if ch == '\n' else ' ' for ch in t[i:j])); i = j
        elif c in '"\'':
            j = i + 1
            while j < n and t[j] != c and t[j] != '\n':
                j += 2 if t[j] == '\\' else 1
            seg = t[i + 1:min(j, n)]
            out.append(c + ''.join('\n' if ch == '\n' else ' ' for ch in seg) + (c if j < n and t[j] == c else '')); i = j + 1 if j < n and t[j] == c else j
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

def scan(path):
    raw = Path(path).read_text(errors='replace')
    t = strip_comments_and_strings(raw)
    sites = []
    for m in re.finditer(r'\btry\s*\{', t):
        b0 = m.end() - 1; b1 = match_brace(t, b0)
        if b1 < 0: continue
        body = t[b0:b1 + 1]
        k = b1 + 1
        while True:
            h = re.match(r'\s*catch\s*\(([^)]*)\)\s*\{', t[k:])
            if not h: break
            hb0 = k + h.end() - 1; hb1 = match_brace(t, hb0)
            if hb1 < 0: break
            if h.group(1).strip() == '...':
                handler = t[hb0:hb1 + 1]
                line = t.count('\n', 0, k + t[k:].find('catch')) + 1
                calls = sorted({x.group(1) for x in LUA_CALL.finditer(body)} | {x.group(0).split('(')[0].split('<')[0].strip() for x in LUA_MEMBER.finditer(body)})
                sites.append({'file': str(path), 'line': line,
                              'try_line': t.count('\n', 0, m.start()) + 1,
                              'lua_calls_in_try': calls,
                              'pcall_in_try': bool(PCALL.search(body)),
                              'rethrows': bool(re.search(r'\bthrow\s*;', handler)),
                              'handler_raises_lua_error': bool(re.search(r'\b(luaL_error|lua_error|throw_error\w*)\s*\(', handler))})
            k = hb1 + 1
    return sites

def main():
    rec = json.loads((BUILD / 'pass1/COMPOSITION-RECEIPT.json').read_text())
    files = set()
    for u in rec['selected_units']:
        src = Path(u['source'])
        if src.suffix in ('.cpp', '.cc', '.cxx'): files.add(src.resolve())
        dep = Path(u['dependency'])
        if dep.exists():
            for x in shlex.split(dep.read_text().replace('\\\n', ' '))[1:]:
                p = Path(x).resolve()
                if p.suffix in ('.hpp', '.h', '.inl', '.inc') and not p.is_relative_to(SDK.resolve()) and not p.is_relative_to(LUARAW.resolve()) and '/.superpowers/native-dependency-spike/tools/' not in str(p):
                    files.add(p)
    files = sorted(f for f in files if f.exists())
    sites = []
    for f in files: sites.extend(scan(f))
    flagged = [s for s in sites if s['lua_calls_in_try'] and not s['rethrows']]
    rel = lambda p: str(p).replace(str(WHAT) + '/', '')
    print(f'files scanned: {len(files)}; catch(...) handlers found: {len(sites)}; '
          f'rethrowing: {sum(s["rethrows"] for s in sites)}; '
          f'with Lua calls in the try block and NO rethrow (review): {len(flagged)}')
    for s in flagged:
        print(f"  REVIEW {rel(s['file'])}:{s['line']} (try at :{s['try_line']}) calls={','.join(s['lua_calls_in_try'][:6])}"
              f"{' pcall-in-try' if s['pcall_in_try'] else ''}{' handler-raises' if s['handler_raises_lua_error'] else ''}")
    out = {'files_scanned': len(files), 'catch_all_sites': len(sites), 'flagged_for_review': len(flagged),
           'sites': [dict(s, file=rel(s['file'])) for s in sites]}
    (HERE / 'CATCH-ALL-AUDIT.json').write_text(json.dumps(out, indent=1) + '\n')
    if not files or not sites:
        print('FAIL: zero files or zero catch(...) sites scanned'); return 1
    print('DONE (audit lists sites; it does not pass/fail code). Results: CATCH-ALL-AUDIT.json')
    return 0

if __name__ == '__main__':
    sys.exit(main())
