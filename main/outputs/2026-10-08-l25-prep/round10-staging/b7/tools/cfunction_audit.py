#!/usr/bin/env python3
"""B7 raw-Lua-callback audit (Mac-only, read-only).

Finds every C++ function / lambda handed to Lua as a callback that is NOT wrapped by
process_lua_function's TRY (lua_pushcfunction / lua_pushcclosure / luaL_Reg tables /
lua_atpanic / lua_setwarnf / lua_newstate allocator / lua_load reader) in the in-repo
runtime files the B4 build compiled, and for each body reports:
  alloc   uses something that can throw std::bad_alloc or std::exception (std::string, fmt::format,
          std::vector, new, make_unique/shared, std::map/unordered_map insert, .at(), throw std::...)
  try     contains a try block
  catch_all  has catch(...) (would swallow a Lua error unless it rethrows)
  noexcept   declared noexcept
  raises  calls a Lua-raising primitive (luaL_error / lua_error / luaL_check* / lua_call / ...)
Why: with Lua as C++, a std::exception that escapes such a callback is caught by Lua's own
catch(...) (ldo.c:60), turned into status -1, and the lock accounting is then off by one
(abort at the next lua_unlock). Today (Lua as C) the same exception flies through Lua's C
frames to the nearest C++ catch. Neither is good; this list is the work list.
Fails (exit 1) when it finds zero registrations.
Usage: python3 -I cfunction_audit.py   (writes ../audit/CFUNCTION-AUDIT.json and .log)
"""
import json, re, sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import catch_inventory as ci  # same strip()/match_brace()/find_funcs() as the catch inventory
sys.path.pop(0)

WHAT = ci.WHAT
CAND = WHAT / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
FILES = sorted(list((CAND / 'src').rglob('*.cpp')) + list((CAND / 'src').rglob('*.inc')) +
               list((CAND / 'include').rglob('*.hpp')) + list((CAND.parent / 'include').rglob('*.hpp')))
FILES = [f for f in FILES if not f.name.endswith('.rej') and '.orig' not in f.name]

REG = re.compile(r'lua_push(?:c)?(?:function|closure)\s*\(\s*([\w.>\-]+)\s*,\s*(?:&)?([A-Za-z_][\w:]*|\[[^\]]*\]\s*\([^)]*\)\s*(?:->\s*\w+\s*)?\{)')
LREG = re.compile(r'luaL_Reg\s+\w+\s*\[\s*\]\s*\{([^;]*)\};', re.S)
ALLOC = re.compile(r'std::(?:string|wstring|vector|map|unordered_map|set|unordered_set|make_unique|make_shared|format|to_string|function)\b|fmt::format|\bnew\b|\.at\s*\(|throw\s+std::|to_utf8_string|ensure_str|normalize_path')
RAISES = re.compile(r'\b(luaL_error|lua_error|luaL_arg\w+|luaL_check\w+|luaL_opt\w+|lua_call|lua_callk|lua_yield\w*|lua_concat|lua_gettable|lua_settable|lua_getfield|lua_setfield|lua_newtable|lua_createtable|lua_newuserdatauv|lua_pushfstring|lua_pushlstring|lua_pushstring|luaL_ref|luaL_traceback|lua_getglobal|lua_setglobal|lua_geti|lua_seti|lua_next|lua_len|luaL_len|luaL_tolstring|throw_error\w*)\s*\(')


def body_of(t, start):
    b0 = t.find('{', start)
    if b0 < 0: return ''
    b1 = ci.match_brace(t, b0)
    return t[b0:b1 + 1] if b1 > 0 else ''


def collect(overlay=None):
    texts = {}
    for f in FILES:
        src = f
        if overlay is not None and (Path(overlay) / f.relative_to(WHAT)).exists(): src = Path(overlay) / f.relative_to(WHAT)
        texts[f] = ci.strip(src.read_text(errors='replace'))
    defs = {}
    for f, t in texts.items():
        for qual, params, quals, b0 in ci.find_funcs(t):
            name = qual.split('::')[-1]
            b1 = ci.match_brace(t, b0)
            if b1 < 0: continue
            defs.setdefault(name, []).append((f, t.count('\n', 0, b0) + 1, quals, t[b0:b1 + 1], qual))
    rows = []

    def add(kind, f, line, name, quals, body, qual=None):
        rows.append({'kind': kind, 'registered_at': f'{f.relative_to(WHAT)}:{line}', 'callback': name, 'qual': qual,
                     'noexcept': bool(re.search(r'\bnoexcept\b', quals or '')), 'alloc': sorted(set(m.group(0) for m in ALLOC.finditer(body)))[:8],
                     'try': bool(re.search(r'\btry\b|\bTRY\s*\(', body)), 'catch_all': bool(re.search(r'catch\s*\(\s*\.\.\.\s*\)', body)),
                     'raises': sorted(set(m.group(1) for m in RAISES.finditer(body)))[:8], 'body_lines': body.count('\n') + 1})

    for f, t in texts.items():
        for m in re.finditer(r'lua_push(?:c)?(?:function|closure)\s*\(', t):
            args_start = m.end()
            depth, j = 1, args_start
            while j < len(t) and depth:
                depth += (t[j] == '(') - (t[j] == ')'); j += 1
            args = t[args_start:j - 1]
            parts = re.split(r',(?![^(]*\))', args, maxsplit=1)
            if len(parts) < 2: continue
            fn = parts[1].strip()
            line = t.count('\n', 0, m.start()) + 1
            if fn.startswith('['):
                add('lambda', f, line, '<lambda>', '', body_of(t, args_start + args.find('[')))
            else:
                name = re.sub(r'^&', '', fn.split(',')[0].strip()).split('::')[-1]
                name = re.sub(r'[^\w]', '', name)
                if name in defs:
                    ff, ll, quals, body, qual = defs[name][0]
                    add('named', f, line, name, quals, body, qual)
                else:
                    add('named-undefined-here', f, line, name, '', '')
        for m in re.finditer(r'\bwrap_into\s*\(\s*L\s*,\s*\w+\s*,\s*"[^"]*"\s*,\s*(\w+)\s*\)', t):
            name = m.group(1)
            if name in defs:
                ff, ll, quals, body, qual = defs[name][0]
                add('wrap_into', f, t.count('\n', 0, m.start()) + 1, name, quals, body, qual)
        for m in LREG.finditer(t):
            for fm in re.finditer(r'\{\s*"[^"]*"\s*,\s*(\w+)\s*\}', m.group(1)):
                name = fm.group(1)
                if name in defs:
                    ff, ll, quals, body, qual = defs[name][0]
                    add('luaL_Reg', f, t.count('\n', 0, m.start()) + 1, name, quals, body, qual)
    # allocator / panic / warn / reader (fixed list: look them up by name)
    for name in ('lms_counting_alloc', 'lms_lua_panic', 'lms_warnfoff', 'lms_warnfon', 'lms_warnfcont', 'native_lua_reader', 'native_lua_dofile_continue'):
        if name in defs:
            ff, ll, quals, body, qual = defs[name][0]
            add('lua-hook', ff, ll, name, quals, body, qual)
    # dedupe by (callback, registered_at)
    seen, out = set(), []
    for r in rows:
        k = (r['callback'], r['registered_at'])
        if k in seen: continue
        seen.add(k); out.append(r)
    out.sort(key=lambda r: r['registered_at'])
    return out


def main():
    out = collect()
    log = [f'files scanned: {len(FILES)}; Lua callback registrations found: {len(out)}']
    for r in out:
        flag = []
        if r['alloc'] and not r['try']: flag.append('ALLOC-NO-TRY')
        if r['catch_all']: flag.append('CATCH-ALL')
        if r['noexcept']: flag.append('NOEXCEPT')
        log.append(f"{r['kind']:10s} {r['registered_at'].replace('outputs/2026-10-08-l26-runtime/typescan-r4/candidate/','CAND/')} {r['callback']:34s} lines={r['body_lines']:<4d} "
                   f"{' '.join(flag):26s} alloc={','.join(r['alloc'])[:60]} raises={','.join(r['raises'])[:50]}")
    out_dir = HERE.parent / 'audit'
    (out_dir / 'CFUNCTION-AUDIT.json').write_text(json.dumps({'files': len(FILES), 'rows': out}, indent=1) + '\n')
    (out_dir / 'cfunction-audit.log').write_text('\n'.join(log) + '\n')
    print('\n'.join(log))
    if not out:
        print('FAIL: zero registrations found'); return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
