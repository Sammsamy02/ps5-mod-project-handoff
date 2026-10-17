#!/usr/bin/env python3
"""B6 source gate (Mac-only, run on the staged tree before any compile).

Checks, each printing how many items it looked at, and FAILS (exit 1) when any check
looked at zero items or found a problem:
  1. no new #include in any of the four files (base vs work, as sets and as counts)
  2. every line REMOVED by b6.diff is a known, intended replacement (the 45 lock_guard sites
     map one-to-one onto b6diag::ActionsLock lines with the same mutex; 1 unique_lock; the
     handful of explicitly listed lines). Anything else = unreviewed change.
  3. every marker string the verify_build manifest will look for is present in the staged
     source as a literal, and the pre-existing markers B6 must not lose are still there
  4. the CORE slices are brace-balanced, contain no #include and no 'new'/'delete'/'malloc'
  5. no raw std lock on m_thread_actions_mutex is left, 46 tracked ActionsLock scopes, no raise / banned
     get_mod_ref under any lock (delegates to the ONE gate, ../gate/lock_raise_gate.py)
  6. the diagnostics never call into Lua from the pump (no lua_ API call between the
     b6_pump_1hz markers other than via the already-listed names)
usage: b6_source_gate.py   (paths are relative to this file)"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ['src/LuaMod.cpp', 'src/LuaPathShim.cpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp', 'include/LuaMadeSimple/LuaMadeSimple.hpp']
failures = []


def rd(path):
    return open(os.path.join(ROOT, path), encoding='utf-8').read()


def report(name, checked, ok, detail=''):
    status = 'PASS' if ok and checked > 0 else 'FAIL'
    if checked == 0:
        detail = (detail + ' (nothing checked)').strip()
    print('%-4s %-34s checked=%-5d %s' % (status, name, checked, detail))
    if status == 'FAIL':
        failures.append(name)


# 1 -- includes
inc_re = re.compile(r'^\s*#\s*include\s+[<"][^>"]+[>"]', re.M)
checked, bad = 0, []
for f in FILES:
    b = inc_re.findall(rd('base/' + f))
    w = inc_re.findall(rd('work/' + f))
    checked += len(w)
    if sorted(x.strip() for x in b) != sorted(x.strip() for x in w):
        bad.append(f)
report('no new #include', checked, not bad, 'files differing: %s' % bad if bad else '(%d include lines compared)' % checked)

# 2 -- removed lines
diff = open(os.path.join(ROOT, 'b6.diff'), encoding='utf-8').read().split('\n')
removed = [l[1:] for l in diff if l.startswith('-') and not l.startswith('---')]
added = [l[1:] for l in diff if l.startswith('+') and not l.startswith('+++')]
lock_re = re.compile(r'^(\s*)std::lock_guard<std::recursive_mutex> guard\{((?:LuaMod::)?m_thread_actions_mutex)\};$')
new_re = re.compile(r'^(\s*)b6diag::ActionsLock guard\{((?:LuaMod::)?m_thread_actions_mutex), __LINE__\};$')
rem_locks = [lock_re.match(l) for l in removed]
add_locks = [new_re.match(l) for l in added]
rem_keys = sorted((m.group(1), m.group(2)) for m in rem_locks if m)
add_keys = sorted((m.group(1), m.group(2)) for m in add_locks if m)
allowed = {
    'if (int status = lua_pcall(L, num_params, num_return_values, err_handler_abs); status != LUA_OK)',
    'return TRY(lua_state, [&] {',
    'L22Diagnostics::observe([&]() {',
    'Output::send(STR("[l22] widget/object constructed via StaticConstructObject class={} object={} (non-null return)\\n"),',
    'static_cast<void*>(param_class), static_cast<void*>(created_object));',
    '});',
    'std::unique_lock<std::recursive_mutex> Lock{LuaMod::m_thread_actions_mutex};',
    'Output::send(STR("[SHIM] write-area dir {} created={} usable={}\\n"),',
    'ensure_str(key), created, usable);',
}
unknown = [l for l, m in zip(removed, rem_locks) if not m and l.strip() not in allowed]
ok2 = (rem_keys == add_keys and len(rem_keys) == 45 and not unknown)
report('removed lines are intended', len(removed), ok2,
       '%d lock_guard->ActionsLock pairs, %d other removed lines, unknown=%d%s' % (len(rem_keys), len(removed) - len(rem_keys), len(unknown),
                                                                                  (' ' + str(unknown[:3])) if unknown else ''))

# 3 -- markers
MARKERS = [
    ('work/src/LuaMod.cpp', '[LOCKLEAK] mod=%s fn=%s kind=%s', 'narrow'),
    ('work/src/LuaMod.cpp', '[LOCKWAIT] site_line=%d thread=%s waited_ms=%lld', 'narrow'),
    ('work/src/LuaMod.cpp', '[LOCKWAIT] still waiting site_line=%d', 'narrow'),
    ('work/src/LuaMod.cpp', '[LOCKWAIT] summary: ', 'narrow'),
    ('work/src/LuaMod.cpp', '[PERF] dt=%lldms lua_ms={', 'narrow'),
    ('work/src/LuaMod.cpp', '[l22] widget/object construct summary: ', 'narrow'),
    ('work/src/LuaMod.cpp', '[MEM] state=pump gc_kb=', 'narrow'),
    # pre-existing markers that must survive
    ('work/src/LuaMod.cpp', '[l22] widget/object constructed via StaticConstructObject', 'wide'),
    ('work/src/LuaMod.cpp', '[MEM] state={} gc_kb={}', 'wide'),
    ('work/src/LuaPathShim.cpp', '[SHIM] write-area dir {} created={} usable={} why={}', 'wide'),
    ('work/src/LuaPathShim.cpp', '[SHIM] write-fail mod={} path={} errno={} strerror={}', 'wide'),
    ('work/src/LuaPathShim.cpp', '[SHIM] write-area {} -> {}/{}', 'wide'),
]
miss = []
for f, text, _enc in MARKERS:
    if text not in rd(f):
        miss.append(text)
report('marker literals present', len(MARKERS), not miss, 'missing: %s' % miss if miss else '')

# 4 -- CORE slices
def slice_of(path, b, e):
    t = rd(path).split('\n')
    bi = [i for i, l in enumerate(t) if b in l]
    ei = [i for i, l in enumerate(t) if e in l]
    return '\n'.join(t[bi[0]:ei[0] + 1]) if len(bi) == 1 and len(ei) == 1 else ''

def strip_code(text):
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'"(\\.|[^"\\])*"', '""', text)
    return text

slices = [
    ('regionB', slice_of('work/src/LuaMod.cpp', '// [B6-CORE-BEGIN]', '// [B6-CORE-END]')),
    ('regionA', slice_of('work/src/LuaMadeSimple/LuaMadeSimple.cpp', '// [B6-LEAKCORE-BEGIN]', '// [B6-LEAKCORE-END]')),
    ('api', slice_of('work/include/LuaMadeSimple/LuaMadeSimple.hpp', '// [B6-API-BEGIN]', '// [B6-API-END]')),
]
problems = []
for name, txt in slices:
    code = strip_code(txt)
    if not txt or code.count('{') != code.count('}'):
        problems.append('%s unbalanced/empty' % name)
    if re.search(r'#\s*include', code):
        problems.append('%s has #include' % name)
    if re.search(r'\b(malloc|calloc|realloc|free|new)\b|(?<!= )\bdelete\b', code):
        problems.append('%s allocates' % name)
    if re.search(r'\b(mprotect|mmap|dlsym|__asm|asm\s*\()', code):
        problems.append('%s touches memory protection / asm' % name)
report('CORE slices clean', len(slices), not problems, '; '.join(problems))

# 5 -- every lock site tracked, no raise / banned call under any lock (the ONE gate, gate/lock_raise_gate.py)
GATE = os.environ.get('B6_LOCK_GATE') or os.path.join(os.path.dirname(ROOT), 'gate', 'lock_raise_gate.py')
proc = subprocess.run([sys.executable, '-I', GATE, '--helpers', 'off', '--unprotected-calls', '--metamethod-calls',
                       '--forbid-raw-guard', 'm_thread_actions_mutex', os.path.join(ROOT, 'work/src/LuaMod.cpp')],
                      capture_output=True, text=True)
m = re.search(r'guard declarations: (\d+) std \([^)]*\), (\d+) custom wrapper', proc.stdout)
tracked = int(m.group(2)) if m else 0
m2 = re.search(r'--forbid-raw-guard [^:]*: (\d+) std guard\(s\) on a matching mutex \(must be 0\), (\d+) tracked', proc.stdout)
m3 = re.search(r'(\d+) findings \(', proc.stdout)
okk = (proc.returncode == 0 and m2 is not None and int(m2.group(1)) == 0 and int(m2.group(2)) == 46 and tracked >= 46)
report('lock_raise_gate (one gate) --forbid-raw-guard', tracked, okk,
       'exit %d, tracked wrapper guards=%d (want 46 on m_thread_actions_mutex), raw std on it=%s, findings=%s'
       % (proc.returncode, tracked, m2.group(1) if m2 else '?', m3.group(1) if m3 else '?'))

# 6 -- the pump makes no Lua call
src = rd('work/src/LuaMod.cpp')
a = src.index('[[maybe_unused]] static auto b6_pump_1hz() -> void')
b = src.index('static auto process_simple_actions(', a)
pump = strip_code(src[a:b])
lua_calls = sorted(set(re.findall(r'\b(lua_\w+|luaL_\w+)\s*\(', pump)))
report('pump body makes no Lua API call', len(pump.split('\n')), not lua_calls, 'found: %s' % lua_calls if lua_calls else '')

print('b6_source_gate: %s' % ('FAIL: ' + ', '.join(failures) if failures else 'PASS'))
sys.exit(1 if failures else 0)
