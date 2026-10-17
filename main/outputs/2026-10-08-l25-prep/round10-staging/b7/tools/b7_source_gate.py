#!/usr/bin/env python3
"""B7 pre-compile source gate (Mac-only, read-only). Runs BEFORE compile, next to fs_throw_gate.py and
no_detour_gate.py. Five checks over the repo-owned files the build compiles (same file set as the
catch inventory: the l26-A COMPOSITION receipt units + every repo path in their .d files), optionally
with a STAGED tree laid over the real one (--overlay DIR, laid out like the worktree root):

  A  catch-passthrough   every catch(...) / catch(void*) whose try block can reach a Lua call and that
                         does not rethrow (or is not preceded by `catch (void*) { throw; }`) must be in
                         gate/catch-allowlist.txt with a reason. Unlisted site = FAIL. Listed site that
                         vanished = reported. Listed LATENT-H4 sites are WARNINGS (--strict-h4 makes them FAIL).
  B  lock-raise          a Lua raise (direct, or one hop through a helper that raises) inside a scope that holds
                         LuaMod::m_thread_actions_mutex (std::lock_guard / unique_lock / scoped_lock /
                         b6diag::ActionsLock / NativeLuaActionDispatchScope). KEPT AFTER B7 ON PURPOSE: with Lua
                         as C++ the guard is released by unwinding, but rollback builds (Lua as C), noexcept
                         frames and swallowed errors still leak it. Unlisted finding = FAIL.
  C  lua-callback        every raw Lua callback (lua_pushcfunction / lua_pushcclosure / luaL_Reg / wrap_into) whose
                         body can throw a std exception (allocates) and has no TRY / try must be in
                         gate/cfunction-baseline.txt. New unlisted callback = FAIL.
  D  noexcept-lua        noexcept functions / destructors that reach Lua (a Lua error crossing them is
                         std::terminate in B7) must be in gate/noexcept-baseline.txt.
  E  game-boundary       (added after the 2026-10-12 review: native-UFunction hooks ARE live on the PS5, so game frames
                         sit below the Lua hook callbacks.) Every try block in ScriptHookThunks.cpp that hands control
                         back to the game (Dispatch / FirePreCallbacks / FirePostCallbacks) must end in a catch (...)
                         (the staged b7-h4 patch). Also lists the live Lua-calling hook registrations
                         (RegisterPreHook/RegisterPostHook in LuaMod.cpp) so the report names them; zero found = FAIL.

EVERY check prints how many files / sites / functions it looked at and FAILS (exit 2) when that count is zero.
Exit codes: 0 PASS, 1 findings, 2 usage / zero-count.
Usage:
  b7_source_gate.py [--overlay DIR] [--strict-h4] [--only A|B|C|D|E] [--files F ...]   (B only: --files scans just those)
  b7_source_gate.py --selftest
"""
import hashlib, json, re, sys
from collections import Counter
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import catch_inventory as ci
import cfunction_audit as ca
sys.path.pop(0)
GATE = HERE.parent / 'gate'
WHAT = ci.WHAT

# ---------------------------------------------------------------- allow-list files
def read_list(path):
    out = {}
    for line in Path(path).read_text().splitlines():
        if not line.strip() or line.lstrip().startswith('#'): continue
        parts = [p.strip() for p in line.split('##')]
        key = parts[0]
        out[key] = {'raw': line, 'fields': parts[1:]}
    return out

# ---------------------------------------------------------------- check A
def site_key(s): return f"{s['file']}|{s['enclosing']}|{s['catch_type']}|{s['handler_sha8']}"

def check_a(texts, sites, strict_h4):
    allow = read_list(GATE / 'catch-allowlist.txt')
    seeing = [s for s in sites if s['class'] in ('all', 'void*')]
    flagged = [s for s in seeing if s['lua_in_try'] and not s['rethrows'] and not s['void_pass_first']]
    print(f'  A catch-passthrough: files={len(texts)} catch handlers={len(sites)} that match a Lua throw={len(seeing)} flagged (Lua reachable, no pass-through)={len(flagged)}')
    if not texts or not seeing: print('  A FAIL: zero files or zero catch(...) sites scanned'); return 2, []
    found = Counter(site_key(s) for s in flagged)
    findings, warns = [], []
    for k, n in sorted(found.items()):
        entry = allow.get(k)
        want = 1
        if entry:
            m = re.search(r'count=(\d+)', ' '.join(entry['fields'])); want = int(m.group(1)) if m else 1
        if not entry or n != want:
            findings.append(f"UNLISTED catch(...) can swallow a Lua error: {k} x{n} (allow-list says {want if entry else 'absent'})")
        elif any(f.startswith('LATENT-H4') for f in entry['fields']):
            warns.append(k)
    vanished = [k for k in allow if k not in found]
    for k in vanished: print(f'    resolved/vanished allow-list entry (remove it): {k}')
    for f in findings: print('    FINDING ' + f)
    for w in warns: print('    WARN (latent: not reachable on the PS5 today, see allow-list reason): ' + w)
    print(f'    allow-listed and still present: {len(found) - len(findings)} keys, {sum(found.values())} sites; warns={len(warns)}')
    if strict_h4 and warns: print('    FINDING --strict-h4: latent engine-boundary sites remain'); return 1, findings + warns
    return (1 if findings else 0), findings

# ---------------------------------------------------------------- check B (port + extension of lock_raise_gate.py)
LOCK = re.compile(r'(?:lock_guard|unique_lock|scoped_lock)\s*<\s*std::recursive_mutex\s*>\s*\w+\s*[{(]\s*(?:LuaMod::)?m_thread_actions_mutex\s*[})]'
                  r'|\bActionsLock\s+\w+\s*[{(]\s*(?:LuaMod::)?m_thread_actions_mutex\b[^;{}()]*[})]'
                  r'|\bNativeLuaActionDispatchScope\s+\w+\s*;')
RAISE = re.compile(r'(\.\s*throw_error\s*\(|\bthrow_lua_error\s*\(|\bthrow_error\s*\(\s*\w|\bluaL_error\s*\(|\blua_error\s*\(|\bluaL_arg(?:error|check)\s*\(|\bluaL_check\w*\s*\(|\bluaL_opt\w*\s*\(|\blua_yield\w*\s*\()')
RAW = re.compile(r'R"([^\s()\\]{0,16})\(')

def blank(src):
    """Blank comments, string/char literals (incl. raw strings) to spaces, keeping newlines (from lock_raise_gate.py)."""
    out, i, n = [], 0, len(src)
    def keep_lines(a, b): out.append(''.join('\n' if ch == '\n' else ' ' for ch in src[a:b]))
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
            while j < n and src[j] != c and src[j] != '\n': j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            keep_lines(i, j); i = j
        else:
            out.append(c); i += 1
    return ''.join(out)

def lock_scan(text, helpers, label):
    clean = blank(text)
    lines = clean.split('\n')
    scopes, findings = 0, []
    helper_re = re.compile(r'\b(' + '|'.join(re.escape(h) for h in sorted(helpers)) + r')\s*\(') if helpers else None
    for ln, line in enumerate(lines):
        m = LOCK.search(line)
        if not m: continue
        scopes += 1
        depth, end, start_col = 0, None, m.end()
        for k in range(ln, len(lines)):
            seg = lines[k][start_col:] if k == ln else lines[k]
            for ch in seg:
                if ch == '{': depth += 1
                elif ch == '}':
                    depth -= 1
                    if depth < 0: end = k; break
            if end is not None: break
        end = len(lines) - 1 if end is None else end
        for k in range(ln, end + 1):
            seg = lines[k][start_col:] if k == ln else lines[k]
            if RAISE.search(seg): findings.append((label, ln + 1, k + 1, 'direct', text.split('\n')[k].strip()[:100]))
            elif helper_re and helper_re.search(seg): findings.append((label, ln + 1, k + 1, 'helper:' + helper_re.search(seg).group(1), text.split('\n')[k].strip()[:100]))
    return scopes, findings

def raising_helpers(texts):
    """names of functions whose own body contains a direct raise primitive (one hop; call-sites that are
    themselves TRY-wrapped lambdas are NOT excluded: a helper raise under a lock is a finding)."""
    names = {}
    for f, t in texts.items():
        for qual, params, quals, b0 in ci.find_funcs(t):
            b1 = ci.match_brace(t, b0)
            if b1 < 0: continue
            name = qual.split('::')[-1]
            if name in ci.KEYWORDS or len(name) < 4: continue
            if RAISE.search(t[b0:b1 + 1]): names.setdefault(name, set()).add(ci.rel(f))
    return names

def check_b(texts, files_arg):
    base = read_list(GATE / 'lock-raise-baseline.txt') if (GATE / 'lock-raise-baseline.txt').exists() else {}
    cand = {f: t for f, t in texts.items() if 'm_thread_actions_mutex' in t or 'NativeLuaActionDispatchScope' in t}
    helpers = {h for h in raising_helpers(cand) if h not in ('throw_error', 'throw_error_impl', 'throw_error_from_buffer', 'throw_lua_error')}
    total_scopes, findings = 0, []
    for f, t in cand.items():
        raw = t  # texts are already comment/string-stripped by ci.strip(); blank() again keeps line numbers
        s, fi = lock_scan(raw, helpers, ci.rel(f))
        total_scopes += s; findings += fi
    print(f'  B lock-raise: files with the LuaMod lock={len(cand)} lock scopes={total_scopes} raising helpers known={len(helpers)} findings={len(findings)}')
    if not cand or total_scopes == 0: print('  B FAIL: zero files or zero lock scopes scanned'); return 2, []
    keyed = []
    for (label, lock_line, raise_line, kind, txt) in findings:
        norm = re.sub(r'\s+', ' ', txt)
        key = f"{label}|{kind}|{norm}"
        if key in base: continue
        keyed.append(f'{label}:{raise_line} ({kind}) raise inside the LuaMod lock scope opened at line {lock_line}: {txt}')
    for k in keyed: print('    FINDING ' + k)
    print(f'    baseline-accepted findings: {len(findings) - len(keyed)}')
    return (1 if keyed else 0), keyed

# ---------------------------------------------------------------- check C / D
def check_c(overlay):
    rows = ca.collect(overlay)
    allow = read_list(GATE / 'cfunction-baseline.txt')
    risky = [r for r in rows if r['alloc'] and not r['try'] and not r['noexcept'] and r['kind'] != 'named-undefined-here']
    print(f'  C lua-callback: registrations={len(rows)} that allocate and have no TRY/try={len(risky)}')
    if not rows: print('  C FAIL: zero registrations found'); return 2, []
    findings = []
    for r in risky:
        key = f"{r['callback']}|{r['registered_at'].split(':')[0].replace('outputs/2026-10-08-l26-runtime/typescan-r4/candidate/', 'CAND/')}"
        if key not in allow: findings.append(f"UNLISTED raw Lua callback can throw std::exception: {key} (registered at {r['registered_at']})")
    for f in findings: print('    FINDING ' + f)
    print(f'    baseline-accepted: {len(risky) - len(findings)}')
    return (1 if findings else 0), findings

def check_d(h2):
    allow = read_list(GATE / 'noexcept-baseline.txt')
    print(f'  D noexcept-lua: noexcept functions / destructors that reach Lua={len(h2)}')
    findings = []
    for x in h2:
        key = f"{x['file']}|{x['function']}"
        if key not in allow: findings.append(f"UNLISTED noexcept/destructor reaches Lua (a Lua error here is std::terminate): {key}")
    for f in findings: print('    FINDING ' + f)
    print(f'    baseline-accepted: {len(h2) - len(findings)}')
    if not h2: print('  D FAIL: zero noexcept functions found (detector broken)'); return 2, []
    return (1 if findings else 0), findings


# ---------------------------------------------------------------- check E (game boundary, live hooks)
# [round10 B7] handlers may nest ONE brace level (the H4 handler wraps its own log call: try { log } catch (...) {}).
TRY_BLOCK = re.compile(r'\btry\s*\{(?P<body>[^{}]*)\}\s*(?P<handlers>(?:catch\s*\([^)]*\)\s*\{(?:[^{}]|\{[^{}]*\})*\}\s*)+)')
BOUNDARY_CALLS = re.compile(r'\b(FirePreCallbacks|FirePostCallbacks|Dispatch)\s*\(')
HOOK_REG = re.compile(r'\bRegister(Pre|Post)Hook\s*\(\s*&?\s*(\w+)')

def check_e(texts):
    thunks = {f: t for f, t in texts.items() if Path(f).name == 'ScriptHookThunks.cpp'}
    blocks, findings = 0, []
    for f, t in thunks.items():
        for m in TRY_BLOCK.finditer(t):
            if not BOUNDARY_CALLS.search(m.group('body')): continue
            blocks += 1
            if not re.search(r'catch\s*\(\s*\.\.\.\s*\)', m.group('handlers')):
                line = t.count('\n', 0, m.start()) + 1
                findings.append(f'{ci.rel(f)}:{line} try block hands control back to the game with no catch (...): a Lua error (Lua as C++: a thrown pointer) would enter game frames = std::terminate')
    regs = []
    for f, t in texts.items():
        if Path(f).name != 'LuaMod.cpp': continue
        for m in HOOK_REG.finditer(t):
            regs.append((ci.rel(f), t.count('\n', 0, m.start()) + 1, m.group(1) + 'Hook', m.group(2)))
    print(f'  E game-boundary: ScriptHookThunks.cpp files={len(thunks)} game-facing try blocks={blocks} without catch(...)={len(findings)}; '
          f'live hook registrations in LuaMod.cpp={len(regs)} ({", ".join(sorted({r[3] for r in regs}))})')
    if not thunks or blocks == 0 or not regs:
        print('  E FAIL: zero thunk files, zero game-facing try blocks or zero hook registrations scanned (detector broken)'); return 2, []
    for f in findings: print('    FINDING ' + f)
    return (1 if findings else 0), findings

# ---------------------------------------------------------------- driver
def run(overlay, strict_h4, only):
    files = ci.file_set()
    texts = ci.load_texts(files, overlay)
    funcs, raising, sites, h2 = ci.analyze(texts)
    print(f'b7_source_gate: files={len(files)} functions={len(funcs)} overlay={overlay}')
    rc, allf = 0, []
    steps = {'A': lambda: check_a(texts, sites, strict_h4), 'B': lambda: check_b(texts, None), 'C': lambda: check_c(overlay), 'D': lambda: check_d(h2), 'E': lambda: check_e(texts)}
    for k, fn in steps.items():
        if only and k != only: continue
        r, f = fn()
        rc = max(rc, r); allf += f
    print('b7_source_gate: ' + ('PASS' if rc == 0 else 'FAIL (zero-count)' if rc == 2 else f'FAIL ({len(allf)} finding(s))'))
    return rc

# ---------------------------------------------------------------- self-test
def selftest():
    ok = True
    def expect(cond, what):
        nonlocal ok
        print(('  PASS  ' if cond else '  FAIL  ') + what); ok = ok and cond
    base = '''
void f(const Lua& lua) {
    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
    lua.throw_error("x");
}
'''
    s, fi = lock_scan(base, set(), 't1'); expect(s == 1 and len(fi) == 1, 'direct raise under std::lock_guard is found')
    s, fi = lock_scan(base.replace('lock_guard<std::recursive_mutex> guard', 'ActionsLock guard').replace('{LuaMod::m_thread_actions_mutex}', '{LuaMod::m_thread_actions_mutex, __LINE__}'), set(), 't2')
    expect(s == 1 and len(fi) == 1, 'b6diag-style ActionsLock scope is recognised')
    s, fi = lock_scan('void f() {\n  NativeLuaActionDispatchScope d;\n  luaL_error(L, "x");\n}\n', set(), 't3'); expect(s == 1 and len(fi) == 1, 'NativeLuaActionDispatchScope holds the lock')
    s, fi = lock_scan('void f() {\n  {\n    std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};\n    int a = 1;\n  }\n  lua.throw_error("after");\n}\n', set(), 't5')
    expect(s == 1 and len(fi) == 0, 'raise AFTER the guard block ended is not a finding')
    s, fi = lock_scan('void f() {\n  std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};\n  // lua.throw_error("c")\n  const char* m = "luaL_error(L)";\n  throw std::runtime_error(m);\n}\n', set(), 't6')
    expect(s == 1 and len(fi) == 0, 'comments, strings and a plain C++ throw under the guard are not findings (the B5 form)')
    s, fi = lock_scan('void f() {\n  std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};\n  auto* m = get_mod_ref(lua);\n}\n', {'get_mod_ref'}, 't7')
    expect(s == 1 and len(fi) == 1 and fi[0][3].startswith('helper:'), 'one-hop helper that raises (get_mod_ref shape) is found')
    s, fi = lock_scan('void f() {\n  std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex};\n  auto* m = get_mod_ref_cxx(lua);\n}\n', {'get_mod_ref'}, 't8')
    expect(len(fi) == 0, 'the C++-throwing twin (get_mod_ref_cxx) is not a finding')
    # check A key logic on synthetic sites
    fake = [{'class': 'all', 'lua_in_try': ['D:x'], 'rethrows': False, 'void_pass_first': False, 'file': 'f.cpp', 'enclosing': 'g', 'catch_type': '...', 'handler_sha8': 'abcd1234'}]
    expect(site_key(fake[0]) == 'f.cpp|g|...|abcd1234', 'allow-list key has no line numbers')
    # check E on synthetic thunk text
    good = 'void Thunk(){ try { Dispatch(1); }\n catch (std::exception& e) { f(); }\n catch (...) { g(); } }'
    bad = 'void Thunk(){ try { Dispatch(1); }\n catch (std::exception& e) { f(); } }'
    expect(len([m for m in TRY_BLOCK.finditer(good) if BOUNDARY_CALLS.search(m.group('body')) and re.search(r'catch\s*\(\s*\.\.\.\s*\)', m.group('handlers'))]) == 1, 'check E: thunk try block with catch(...) is accepted')
    expect(len([m for m in TRY_BLOCK.finditer(bad) if BOUNDARY_CALLS.search(m.group('body')) and not re.search(r'catch\s*\(\s*\.\.\.\s*\)', m.group('handlers'))]) == 1, 'check E: thunk try block with std::exception only is a finding')
    nested = 'void Thunk(){ try { Dispatch(1); }\n catch (std::exception& e) { f(); }\n catch (...) { try { g(); } catch (...) {} } }'
    expect(len([m for m in TRY_BLOCK.finditer(nested) if BOUNDARY_CALLS.search(m.group('body')) and re.search(r'catch\s*\(\s*\.\.\.\s*\)', m.group('handlers'))]) == 1, 'check E: catch(...) whose handler wraps its own log call (one nested level) is accepted')
    expect(HOOK_REG.search('x = f->RegisterPreHook(&lua_unreal_script_function_hook_pre, d);').group(2) == 'lua_unreal_script_function_hook_pre', 'check E: hook registration detector reads the callback name')
    print('selftest: ' + ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1

if __name__ == '__main__':
    a = sys.argv[1:]
    if a == ['--selftest']: sys.exit(selftest())
    overlay = a[a.index('--overlay') + 1] if '--overlay' in a else None
    only = a[a.index('--only') + 1] if '--only' in a else None
    sys.exit(run(overlay, '--strict-h4' in a, only))
