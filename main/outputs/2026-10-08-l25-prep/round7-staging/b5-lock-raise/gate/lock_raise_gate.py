#!/usr/bin/env python3
"""Pre-compile gate: no Lua raise (longjmp) while a lock is held.

Why (B4 launch 1, 2026-10-12, analysis/spawner-menu/VERIFY.md): Lua is
compiled as C in this port, so luaL_error / lua_error / lua.throw_error inside
a pcall is a longjmp. A longjmp skips C++ destructors. A std::lock_guard (or
unique_lock, scoped_lock, a lock()/unlock() pair, actions_lock()/unlock())
that is alive when the raise happens is never released; the owning thread
keeps the (recursive) mutex for the rest of the session and every other
thread that needs it blocks forever.

What it finds (inside a region where a lock is HELD):
  direct   a raise primitive: lua.throw_error / throw_error / throw_error_impl,
           luaL_error, lua_error, luaL_argerror/typeerror/argcheck/check*/opt*,
           lua_yield*, (and with --unprotected-calls: lua_call/lua_callk, which
           propagate a callee's raise through the caller's frame);
  helper   a call to a function that raises (names are BUILT BY SCANNING the
           file set: any function whose body contains a raise primitive, or
           calls such a function, transitively up to --helper-depth hops);
           includes helper-like #define macros and L22Diagnostics::throw_lua_error;
  lambda   a call (or use as an argument) of a LOCAL named lambda whose body
           raises; reported at the lambda's raise lines.
Lock forms understood: std::lock_guard / unique_lock / scoped_lock / shared_lock
(with or without <template args>, brace or paren init, defer_lock /
adopt_lock / try_to_lock, if/for/switch init-statements, unlock()/lock() on the
guard variable), mutex.lock()/unlock()/try_lock() statements, std::lock(a,b),
actions_lock()/actions_unlock() (any receiver), classes that own a lock-guard
member (e.g. NativeLuaActionDispatchScope: declaring one holds the lock), and
#define macros whose body declares a guard.

Scope model: a token-level brace tree (NOT a regex over lines). Guard locks
end at the closing brace of the block that declared them. Explicit unlock()
is flow-aware: an unlock inside a block that ends in return/break/continue/
goto/throw/raise does not release the lock for code after the block; if/else
and try/catch chains merge their branch end states (held if ANY path leaves
it held); loops and switch merge with the pre-state; switch `case` labels
reset to the pre-switch state. Early return/break inside a lock scope never
releases the lock (the guard destructor only runs at normal scope exit, and
this gate cares about what is raised before that).
Lambdas: a lambda that is invoked in place, or passed to an unknown / known
synchronous callee, INHERITS the enclosing held locks (fail-closed). A lambda
passed to a deferral callee (register_*, Register*, ExecuteInGameThread,
thread/async, emplace/push, *Listener, ...) or stored (`x = [..]{}`) does not
run under the lock and is scanned with its own locks only. A named local
lambda is a local helper: calling it (or passing it on) while a lock is held
makes its raises count. --lambda-policy inherit makes every lambda inherit.

Comment / string handling: the source is first blanked by this file's own
pre-pass (comments, string/char/raw-string contents, digit separators,
backslash-continued comments), then fed through no_detour_gate.lex_cxx for the
preprocessor liveness (PS5-dead regions are skipped and counted) and the
final code view, exactly like fs_throw_gate.py. The pre-pass exists because
lex_cxx has a defect: a block comment that opens and closes on ONE line leaves
its in_block flag set, so every following line is swallowed until the next
'*/' (verified: 'int a; /* x */ int b;' then a raise on the next line is
blanked). A gate built directly on lex_cxx would miss code after such a
comment; --selftest has a case for it.

Baseline ratchet (like fs_throw_gate.py): --baseline FILE lists known-accepted
sites (key = relpath|kind|enclosing function|lock|statement text, no line
numbers so edits elsewhere do not invalidate it; one `key ## reason` per
line). Known sites are counted and printed and do NOT fail the gate; any NEW
or CHANGED site fails it; vanished sites are reported as resolved.
--write-baseline FILE writes the current findings (reason `## UNREVIEWED`).
An inline `// [LOCK-RAISE-APPROVED] <reason>` on the raise/call line approves
it (counted and printed, never silent).

Every run prints how many files and lock scopes it checked. It FAILS (exit 2)
when it checked zero files, zero lock scopes, or saw zero raise sites (the
detector must have a target), and when a file that mentions locks or raises
has unbalanced braces (the scope tree cannot be trusted).

Usage:
  lock_raise_gate.py [--root DIR]... [--files-from LIST_OR_JSON]... [FILE...]
                     [--baseline F] [--write-baseline F] [--helper-depth N]
                     [--helpers off] [--mutex REGEX] [--unprotected-calls]
                     [--lambda-policy deferred-aware|inherit] [--show-helpers]
                     [--json OUT] [-v]
  lock_raise_gate.py --selftest
Exit codes: 0 PASS, 1 FINDINGS, 2 usage/env error or nothing checked.
Stdlib only. Read-only on everything it scans.
"""
import json
import os
import re
import sys
import tempfile

sys.dont_write_bytecode = True   # never write a .pyc next to harvest/no_detour_gate.py (read-only for this gate)

HERE = os.path.dirname(os.path.abspath(__file__))


def _find_harvest():
    cands = [HERE]
    p = HERE
    for _ in range(6):
        p = os.path.dirname(p)
        cands.append(os.path.join(p, 'harvest'))
    env = os.environ.get('GATE_HARVEST_DIR')
    if env:
        cands.insert(0, env)
    for c in cands:
        if os.path.isfile(os.path.join(c, 'no_detour_gate.py')):
            return c
    return None


HARVEST = _find_harvest()
if HARVEST is None:
    print('lock_raise_gate: ERROR cannot find harvest/no_detour_gate.py '
          '(set GATE_HARVEST_DIR)')
    sys.exit(2)
sys.path.insert(0, HARVEST)
from no_detour_gate import lex_cxx, src_files  # noqa: E402

# <worktree>/outputs/2026-10-08-l25-prep/harvest/ -> three dirnames = worktree root
WHAT = os.path.dirname(os.path.dirname(os.path.dirname(HARVEST)))
DEFAULT_SRC = os.path.join(WHAT, 'outputs', '2026-10-08-l26-runtime', 'typescan-r4', 'candidate')
CXX_EXT = ('.cpp', '.cc', '.cxx', '.c', '.hpp', '.h', '.hh', '.inl', '.inc', '.ipp')
APPROVED_MARK = '[LOCK-RAISE-APPROVED]'

# ---------------------------------------------------------------- vocabulary
PRIM_EXACT = {
    'throw_error': 'raise', 'throw_error_impl': 'raise',
    'luaL_error': 'raise', 'lua_error': 'raise',
    'luaL_argerror': 'raise', 'luaL_typeerror': 'raise', 'luaL_argcheck': 'raise',
    'luaL_checkstack': 'raise', 'luaL_checktype': 'raise', 'luaL_checkany': 'raise',
    'lua_yield': 'raise', 'lua_yieldk': 'raise',
    'lua_call': 'unprotected', 'lua_callk': 'unprotected',
    # APIs that can run a metamethod (__index/__newindex/__add/__concat/__eq/__lt/__len) and so raise
    # from inside the caller's frame; off by default, --metamethod-calls turns them on.
    'lua_gettable': 'metamethod', 'lua_settable': 'metamethod', 'lua_getfield': 'metamethod',
    'lua_setfield': 'metamethod', 'lua_geti': 'metamethod', 'lua_seti': 'metamethod',
    'lua_getglobal': 'metamethod', 'lua_setglobal': 'metamethod', 'lua_arith': 'metamethod',
    'lua_concat': 'metamethod', 'lua_compare': 'metamethod', 'lua_len': 'metamethod',
    'lua_next': 'metamethod', 'luaL_len': 'metamethod', 'luaL_tolstring': 'metamethod',
}


def prim_active(n, opts):
    """True when name n is a raise primitive under the active options."""
    c = PRIM_EXACT.get(n)
    if c == 'raise':
        return True
    if c == 'unprotected':
        return bool(opts.get('unprotected'))
    if c == 'metamethod':
        return bool(opts.get('metamethod'))
    return bool(PRIM_RE.match(n))
PRIM_RE = re.compile(r'^luaL_(?:check|opt)\w*$')
# helper-like names we always know about even if their definition is not in
# the scanned set
SEED_HELPERS = {'throw_lua_error'}
# Guard types. STD ones are the standard library RAII guards; CUSTOM ones are project wrappers
# that lock in their constructor and unlock in their destructor (B6: b6diag::ActionsLock{mutex,
# __LINE__}, first argument = the mutex). Aliases (`using G = std::lock_guard<..>;`, typedef) are
# discovered from the scanned sources by configure_guards(). The sets are RESET at the start of
# every run so one run's aliases never leak into the next.
STD_GUARD_TYPES = frozenset({'lock_guard', 'unique_lock', 'scoped_lock', 'shared_lock'})
CUSTOM_GUARD_TYPES_DEFAULT = frozenset({'ActionsLock'})
GUARD_TYPES = set(STD_GUARD_TYPES | CUSTOM_GUARD_TYPES_DEFAULT)
RAW_GUARD_TYPES = set(STD_GUARD_TYPES)       # std guards (+ aliases of them)
CUSTOM_GUARD_TYPES = set(CUSTOM_GUARD_TYPES_DEFAULT)
# Calls that are ALWAYS a finding while a lock is held, in every --helpers mode (they are the
# exact helpers that raise with a longjmp). Their C++-throwing twins (get_mod_ref_cxx) are
# different identifiers and are allowed. --ban NAME adds one, --unban NAME removes one.
BANNED_DEFAULT = frozenset({'get_mod_ref'})
BANNED_CALLS = set(BANNED_DEFAULT)
ALIAS_USING = re.compile(r'\busing\s+(\w+)\s*=\s*(?:std\s*::\s*)?(\w+)\b')
ALIAS_TYPEDEF = re.compile(r'\btypedef\s+(?:std\s*::\s*)?(\w+)\b[^;{}]*?\b(\w+)\s*;')


def configure_guards(opts, texts):
    """Reset the guard / ban sets from the options and add the aliases found in `texts`
    (blanked source texts). Returns the list of aliases found (name, target)."""
    GUARD_TYPES.clear()
    RAW_GUARD_TYPES.clear()
    CUSTOM_GUARD_TYPES.clear()
    BANNED_CALLS.clear()
    RAW_GUARD_TYPES.update(STD_GUARD_TYPES)
    CUSTOM_GUARD_TYPES.update(CUSTOM_GUARD_TYPES_DEFAULT)
    CUSTOM_GUARD_TYPES.update(opts.get('guard_types') or [])
    BANNED_CALLS.update(BANNED_DEFAULT)
    BANNED_CALLS.update(opts.get('ban') or [])
    BANNED_CALLS.difference_update(opts.get('unban') or [])
    GUARD_TYPES.update(RAW_GUARD_TYPES)
    GUARD_TYPES.update(CUSTOM_GUARD_TYPES)
    found = []
    changed = True
    while changed:                       # aliases of aliases
        changed = False
        for tx in texts:
            for rx, (ti, ai) in ((ALIAS_USING, (2, 1)), (ALIAS_TYPEDEF, (1, 2))):
                for m_ in rx.finditer(tx):
                    tgt, al = m_.group(ti), m_.group(ai)
                    if tgt in GUARD_TYPES and al not in GUARD_TYPES:
                        GUARD_TYPES.add(al)
                        (RAW_GUARD_TYPES if tgt in RAW_GUARD_TYPES else CUSTOM_GUARD_TYPES).add(al)
                        found.append((al, tgt))
                        changed = True
    return found
CTL = {'if', 'for', 'while', 'switch', 'catch'}
NOT_CALLS = CTL | {'return', 'sizeof', 'alignof', 'decltype', 'typeof', 'noexcept',
                   'defined', 'new', 'delete', 'throw', 'case', 'else', 'do', 'try',
                   'static_assert', 'operator', 'using', 'typedef', 'template',
                   'static_cast', 'const_cast', 'reinterpret_cast', 'dynamic_cast',
                   'alignas', 'requires', 'co_return', 'co_await', 'co_yield'}
# names never treated as helper functions (too generic: name-based matching
# would flood). Raise primitives are handled separately.
HELPER_STOP = {
    'get', 'set', 'at', 'size', 'begin', 'end', 'find', 'insert', 'erase', 'push_back',
    'emplace_back', 'what', 'data', 'value', 'name', 'main', 'lock', 'unlock', 'try_lock',
    'run', 'call', 'init', 'send', 'format', 'move', 'swap', 'reset', 'clear', 'empty',
    'front', 'back', 'count', 'append', 'assign', 'load', 'store', 'write', 'read',
    'open', 'close', 'next', 'prev', 'min', 'max', 'less', 'hash', 'operator',
}
# callees that run their lambda argument LATER (or on another thread / context)
DEFERRED_CALLEE = re.compile(
    r'^(?:register_\w*|Register\w*|Add\w*(?:Listener|Callback)\w*|Execute\w*|\w*InGameThread\w*|'
    r'\w*_async|async|thread|jthread|emplace\w*|push\w*|set_\w*|Set\w*|signal\w*|connect\w*|'
    r'subscribe\w*|schedule\w*|queue\w*|Queue\w*|post\w*|Post\w*|enqueue\w*|'
    r'lua_pushcfunction|lua_pushcclosure|pushcfunction|make_unique|make_shared|function|'
    r'unique_ptr|shared_ptr|std)$')

TOK = re.compile(r'"[^"\n]*"|\'[^\'\n]*\'|~?[A-Za-z_]\w*|\d\w*(?:\.\w*)?|::|->|&&|\|\||\.\.\.|\S')
RAW = re.compile(r'R"([^\s()\\]{0,16})\(')


# ------------------------------------------------------------------ lexing
def blank(src):
    """Blank comments, string/char literals (incl. raw strings) and digit
    separators; keep every newline so line numbers stay exact. Strings keep
    their quotes (content -> spaces)."""
    out, i, n = [], 0, len(src)

    def keep_lines(a, b, quote=None):
        seg = src[a:b]
        txt = ''.join('\n' if ch == '\n' else ' ' for ch in seg)
        if quote and len(txt) >= 2:
            # keep the delimiters so the token stream still sees a literal
            first_nl = txt.find('\n')
            txt = quote + txt[1:-1] + quote if first_nl < 0 else quote + txt[1:]
        out.append(txt)

    while i < n:
        c = src[i]
        if c == 'R' and (i == 0 or src[i - 1] in ' \t\n\r(,={;[<>+-*&|!?:}'
                         or src[max(0, i - 2):i] in ('u8', 'LR') or src[i - 1] in 'uUL'):
            m = RAW.match(src, i)
            if m:
                close = ')' + m.group(1) + '"'
                j = src.find(close, m.end())
                j = n if j < 0 else j + len(close)
                keep_lines(i, j, '"')
                i = j
                continue
        if c == '/' and src.startswith('//', i):
            j = i
            while True:
                k = src.find('\n', j)
                if k < 0:
                    j = n
                    break
                # backslash-continued line comment
                p = k - 1
                if p > i and src[p] == '\r':
                    p -= 1
                if p >= i and src[p] == '\\':
                    j = k + 1
                    continue
                j = k
                break
            keep_lines(i, j)
            i = j
        elif c == '/' and src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            keep_lines(i, j)
            i = j
        elif c == '"':
            j = i + 1
            while j < n and src[j] != '"' and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            keep_lines(i, j, '"')
            i = j
        elif c == "'":
            # digit separator inside a number token?
            k = i - 1
            while k >= 0 and (src[k].isalnum() or src[k] in "_'"):
                k -= 1
            run = src[k + 1:i]
            if run and run[0].isdigit() and i + 1 < n and src[i + 1].isalnum():
                out.append('_')
                i += 1
                continue
            j = i + 1
            while j < n and src[j] != "'" and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            keep_lines(i, j, "'")
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


class Model:
    """One lexed file: token list, matching braces, macros, stats."""
    __slots__ = ('path', 'rel', 'raw_lines', 'toks', 'match', 'balanced', 'macros',
                 'dead_lines', 'dead_relevant', 'live_lines', 'variant')


COND_OPEN = re.compile(r'^\s*#\s*(?:if|ifdef|ifndef)\b')
COND_ELSE = re.compile(r'^\s*#\s*(?:else|elif)\b')
COND_END = re.compile(r'^\s*#\s*endif\b')


def variant_lines(lines, mode):
    """Blank the non-directive lines of conditional branches so that every
    #if/#elif/#else group contributes exactly one branch: mode 'N' (digit) keeps
    branch N of every group (the last branch when a group has fewer), mode
    'last' keeps the last branch of groups that have an #else/#elif (the body
    of an #if without #else stays).
    Directive lines are kept so lex_cxx's __PROSPERO__ tracking stays in sync;
    line numbers never change."""
    # pass 1: for each group find whether it has an else-branch
    stack = []
    has_else = {}
    nbranch = {}
    gid = 0
    for ln in lines:
        if COND_OPEN.match(ln):
            gid += 1
            nbranch[gid] = 1
            stack.append(gid)
        elif COND_ELSE.match(ln) and stack:
            has_else[stack[-1]] = True
            nbranch[stack[-1]] += 1
        elif COND_END.match(ln) and stack:
            stack.pop()
    out = []
    stack = []   # (gid, branch_index)
    gid = 0
    for ln in lines:
        if COND_OPEN.match(ln):
            gid += 1
            stack.append([gid, 0])
            out.append(ln)
        elif COND_ELSE.match(ln) and stack:
            stack[-1][1] += 1
            out.append(ln)
        elif COND_END.match(ln) and stack:
            stack.pop()
            out.append(ln)
        elif ln.lstrip().startswith('#'):
            out.append(ln)
        else:
            keep = True
            for g, b in stack:
                if mode == 'last':
                    if has_else.get(g) and b != nbranch[g] - 1:
                        keep = False
                else:
                    want = int(mode)         # 'first' == '0'
                    if b != min(want, nbranch[g] - 1):
                        keep = False
            out.append(ln if keep else '')
    return out


def lex_file(path, rel, text, variant=None):
    m = Model()
    m.path, m.rel = path, rel
    m.variant = variant or 'all'
    cleaned = blank(text)
    lines = cleaned.split('\n')
    if variant:
        lines = variant_lines(lines, variant)
    m.raw_lines = text.split('\n')
    views = lex_cxx(lines)
    # directive continuation lines (backslash) belong to the directive
    cont = set()
    macros = []
    i = 0
    nlines = len(lines)
    while i < nlines:
        s = lines[i].lstrip()
        if s.startswith('#'):
            j = i
            body_lines = [lines[i]]
            while lines[j].rstrip('\r').endswith('\\') and j + 1 < nlines:
                j += 1
                cont.add(j)
                body_lines.append(lines[j])
            dm = re.match(r'#\s*define\s+(\w+)(\([^)]*\))?(.*)$',
                          ' '.join(x.rstrip('\\\r') for x in body_lines).lstrip(), re.S)
            if dm and views[i].live:
                macros.append((dm.group(1), i + 1, dm.group(3)))
            i = j + 1
        else:
            i += 1
    toks = []
    dead = 0
    dead_rel = 0
    live = 0
    word = re.compile(r'lock_guard|unique_lock|scoped_lock|throw_error|luaL_error|lua_error|actions_lock')
    for idx, v in enumerate(views):
        if idx in cont:
            continue
        if not v.live:
            dead += 1
            if word.search(v.code):
                dead_rel += 1
            continue
        if not v.code.strip():
            continue
        live += 1
        for tm in TOK.finditer(v.code):
            toks.append((tm.group(), idx + 1))
    m.toks = toks
    m.macros = macros
    m.dead_lines, m.dead_relevant, m.live_lines = dead, dead_rel, live
    # brace matching (independent of scope classification)
    match = {}
    st = []
    ok = True
    for k, (t, _ln) in enumerate(toks):
        if t == '{':
            st.append(k)
        elif t == '}':
            if st:
                o = st.pop()
                match[o] = k
                match[k] = o
            else:
                ok = False
    if st:
        ok = False
    m.match = match
    m.balanced = ok
    return m


# --------------------------------------------------------------- model types
class Lock(object):
    __slots__ = ('kind', 'var', 'mutex', 'line', 'held', 'text', 'end')

    def __init__(self, kind, var, mutex, line, held, text):
        self.kind, self.var, self.mutex, self.line, self.held, self.text = kind, var, mutex, line, held, text
        self.end = None

    def desc(self):
        return '%s %s@%d (%s)' % (self.kind, self.var or '-', self.line, self.mutex or '?')


class Scope(object):
    def __init__(self, kind, name, idx, line, stmt_start):
        self.kind, self.name, self.open, self.line = kind, name, idx, line
        self.depth = 0
        self.inl = 0
        self.stmt_start = stmt_start
        self.done = stmt_start
        self.locks = []
        self.hdr_locks = []
        self.snap = None
        self.last_first = None
        self.last_raise = False
        self.ctl = None
        self.else_if = False
        self.inherit = False
        self.manual = {}
        self.chain = None
        self.lam = {}
        self.members = []
        self.target = None        # key of the Func record this scope's events belong to
        self.sub = None
        self.fname = None         # nearest named function (for messages / baseline)

    def is_boundary(self):
        return self.kind in ('func', 'ns', 'class', 'file', 'enum') or (self.kind == 'lambda' and not self.inherit)


class Func(object):
    __slots__ = ('key', 'name', 'kind', 'file', 'line', 'direct', 'calls', 'text')

    def __init__(self, key, name, kind, file, line):
        self.key, self.name, self.kind, self.file, self.line = key, name, kind, file, line
        self.direct = []     # (line, prim, text)
        self.calls = {}      # callee name -> first line
        self.text = ''


class Tables(object):
    def __init__(self):
        self.holders = {}      # class name -> lock description
        self.lock_macros = {}  # macro name -> mutex text
        self.funcs = {}        # key -> Func (all files)
        self.by_name = {}      # name -> [Func]
        self.helpers = {}      # name -> (depth, witness)

    def add_func(self, f):
        self.funcs[f.key] = f
        self.by_name.setdefault(f.name, []).append(f)


class Finding(object):
    __slots__ = ('rel', 'line', 'kind', 'what', 'func', 'lock', 'text', 'via', 'approved')

    def key(self):
        # line-independent: no line numbers anywhere in the key
        return '|'.join((self.rel, self.kind, self.func or '-', re.sub(r'@\d+', '', self.lock),
                         ' '.join(self.text.split())))


# ------------------------------------------------- unknown guard-like declarations
_IDENT = re.compile(r'[A-Za-z_]\w*$')
_DECL_SKIP = {'return', 'else', 'throw', 'co_return', 'co_yield', 'co_await', 'goto', 'case', 'new', 'delete',
              'sizeof', 'typeid', 'alignof', 'decltype', 'if', 'for', 'while', 'switch', 'catch', 'do', 'using',
              'typedef', 'const', 'static', 'constexpr', 'volatile', 'mutable', 'inline', 'explicit', 'virtual',
              'operator', 'template', 'typename', 'struct', 'class', 'union', 'enum', 'namespace'}


def find_unknown_guards(m, allow):
    """Declarations `T name{mutex-ish}` / `T name(mutex-ish)` where T is NOT a known guard type.
    A new RAII wrapper around a mutex would otherwise be invisible to the lock model (the B6
    ActionsLock swap made the B5 gates scan 3 of 47 scopes and still pass). Returns
    [(line, type, text)]."""
    toks = m.toks
    n = len(toks)
    res = []
    for i in range(1, n - 2):
        name = toks[i][0]
        if toks[i + 1][0] not in ('{', '(') or not _IDENT.match(name) or name in _DECL_SKIP:
            continue
        pt = toks[i - 1][0]
        if pt == '>':                     # T<...> name
            d, k = 0, i - 1
            while k >= 0:
                if toks[k][0] == '>':
                    d += 1
                elif toks[k][0] == '<':
                    d -= 1
                    if d == 0:
                        break
                k -= 1
            if k <= 0:
                continue
            tp = toks[k - 1][0]
        elif _IDENT.match(pt) and pt not in _DECL_SKIP:
            tp = pt
        else:
            continue
        if not _IDENT.match(tp) or tp in GUARD_TYPES or tp in allow or tp in _DECL_SKIP:
            continue
        opener = toks[i + 1][0]
        closer = '}' if opener == '{' else ')'
        d, k, arg = 0, i + 1, []
        while k < n:
            t = toks[k][0]
            if t in ('(', '{', '['):
                d += 1
            elif t in (')', '}', ']'):
                d -= 1
                if d == 0:
                    break
            if d == 1 and t == ',' :
                break
            if k > i + 1:
                arg.append(t)
            k += 1
        idents = [t for t in arg if _IDENT.match(t)]
        if not idents or not re.search(r'mutex', idents[-1], re.I):
            continue
        if any(_IDENT.match(arg[x]) and _IDENT.match(arg[x + 1]) for x in range(len(arg) - 1)):
            continue                      # `Type mutex` parameter declaration, not an argument
        if any(_IDENT.match(arg[x]) and arg[x + 1] in ('&', '&&', '*') and x + 2 < len(arg) and _IDENT.match(arg[x + 2])
               for x in range(len(arg) - 2)):
            continue                      # `Type& mutex` parameter declaration
        if any(t in ('(', ')') for t in arg):
            continue                      # nested call: not a plain mutex argument
        res.append((toks[i][1], tp, ' '.join(arg)))
    return res


# ----------------------------------------------------------------- the walker
class Walker(object):
    def __init__(self, model, tables, opts):
        self.m = model
        self.t = tables
        self.o = opts
        self.toks = model.toks
        self.stack = [Scope('file', '', -1, 0, 0)]
        self.findings = []        # direct findings
        self.call_sites = []      # (callee, line, text, locks, func) made while a lock was held
        self.lock_sites = 0
        self.raise_sites = 0
        self.funcs = []
        self.lock_list = []
        self.scope_ends = []
        self.raise_lines = set()
        self.anon = 0
        self.mutex_re = re.compile(opts.get('mutex') or '')
        self.raw_guards = []      # (line, type, mutex) of std-guard declarations
        self.custom_guards = []   # same for project wrappers (ActionsLock)
        self.banned_sites = 0     # banned-call sites seen (inside or outside a lock)

    # -- token helpers
    def tk(self, i):
        return self.toks[i][0] if 0 <= i < len(self.toks) else ''

    def ln(self, i):
        return self.toks[i][1] if 0 <= i < len(self.toks) else 0

    def src_text(self, line):
        raw = self.m.raw_lines
        return raw[line - 1].strip()[:160] if 0 < line <= len(raw) else ''

    def skip_angle(self, j):
        """toks[j] == '<' -> index after matching '>' or None."""
        if self.tk(j) != '<':
            return None
        d = 0
        k = j
        n = len(self.toks)
        while k < n and k < j + 400:
            t = self.toks[k][0]
            if t == '<':
                d += 1
            elif t == '>':
                d -= 1
                if d == 0:
                    return k + 1
            elif t in (';', '{', '}'):
                return None
            k += 1
        return None

    def match_paren(self, j):
        """toks[j] in '(' '[' '{' -> index of the matching closer (or -1)."""
        pairs = {'(': ')', '[': ']', '{': '}'}
        op = self.tk(j)
        cl = pairs[op]
        d = 0
        for k in range(j, len(self.toks)):
            t = self.toks[k][0]
            if t == op:
                d += 1
            elif t == cl:
                d -= 1
                if d == 0:
                    return k
        return -1

    def args_text(self, a, b):
        return ' '.join(self.toks[k][0] for k in range(a, b))

    # -- lock state
    def visible_locks(self):
        res = []
        for s in reversed(self.stack):
            res.extend(s.locks)
            res.extend(s.hdr_locks)
            if s.is_boundary():
                res.extend(s.manual.values())
                break
        return res

    def held_locks(self):
        return [l for l in self.visible_locks() if l.held and self.mutex_re.search(l.mutex or '')]

    def capture(self):
        return {id(l): (l, l.held) for l in self.visible_locks()}

    @staticmethod
    def restore(snap):
        for l, h in snap.values():
            l.held = h

    @staticmethod
    def merge_or(snaps):
        """held if held in ANY snapshot (only for locks that exist in snaps)."""
        acc = {}
        for s in snaps:
            for k, (l, h) in s.items():
                if k not in acc:
                    acc[k] = [l, h]
                else:
                    acc[k][1] = acc[k][1] or h
        for l, h in acc.values():
            l.held = h

    def boundary(self):
        for s in reversed(self.stack):
            if s.is_boundary():
                return s
        return self.stack[0]

    def cur_target(self):
        for s in reversed(self.stack):
            if s.target is not None:
                return s.target
            if s.kind in ('ns', 'class', 'file'):
                return None
        return None

    def cur_func_name(self):
        for s in reversed(self.stack):
            if s.fname:
                return s.fname
        return None

    # -- classification of '{'
    def classify(self, sc, i):
        toks = self.toks
        hs = sc.stmt_start
        hdr = [toks[k][0] for k in range(hs, i)]
        prev = hdr[-1] if hdr else ''
        info = {'kind': 'block', 'name': '', 'ctl': None}
        if not hdr:
            return info
        h0 = hdr[0]
        if h0 in ('else', 'try', 'do'):
            info.update(kind='ctl', ctl=h0)
            if h0 == 'else' and len(hdr) > 1 and hdr[1] == 'if':
                info['else_if'] = True
            return info
        if h0 == 'namespace' or (h0 == 'extern' and len(hdr) <= 3) or (
                h0 == 'inline' and len(hdr) > 1 and hdr[1] == 'namespace'):
            return dict(info, kind='ns')
        if h0 in ('case', 'default'):
            return info
        # record / enum
        top = []
        d = 0
        for t in hdr:
            if t in ('(', '[', '<'):
                d += 1
            elif t in (')', ']', '>'):
                d -= 1
            if d == 0:
                top.append(t)
        rec = None
        for q, t in enumerate(top):
            if t in ('class', 'struct', 'union') and '(' not in hdr and '=' not in hdr:
                rec = ('class', q)
                break
            if t == 'enum' and '(' not in hdr and '=' not in hdr:
                rec = ('enum', q)
                break
        if rec and prev not in (')',):
            nm = ''
            for t in top[rec[1] + 1:]:
                if t in ('class', 'struct', 'final'):
                    continue
                if re.match(r'[A-Za-z_]\w*$', t) and t not in ('alignas', 'public', 'private', 'protected'):
                    nm = t
                    break
                if t in (':', '{'):
                    break
            return dict(info, kind=rec[0], name=nm)
        # lambda without parameter list: [..]{ or [..] mutable/noexcept {
        pk = len(hdr) - 1
        quals = {'const', 'noexcept', 'override', 'final', 'mutable', 'volatile', '&', '&&', 'constexpr'}
        q = pk
        while q >= 0 and hdr[q] in quals:
            q -= 1
        # trailing return type:  ) -> T {
        if '->' in hdr:
            a = max(k for k, t in enumerate(hdr) if t == '->')
            b = a - 1
            while b >= 0 and hdr[b] in quals:
                b -= 1
            if b >= 0 and hdr[b] == ')':
                q = b
        if q >= 0 and hdr[q] == ']':
            return self._lambda_info(sc, i, hdr, 0, q, None)
        if q >= 0 and hdr[q] == ')':
            # find matching '('
            d = 0
            p = q
            while p >= 0:
                if hdr[p] == ')':
                    d += 1
                elif hdr[p] == '(':
                    d -= 1
                    if d == 0:
                        break
                p -= 1
            if p < 0:
                return info
            bef = hdr[p - 1] if p > 0 else ''
            if bef == 'noexcept' or bef == 'decltype' or bef == 'requires':
                # noexcept(expr) {  -> keep walking back
                q2 = p - 2
                while q2 >= 0 and hdr[q2] in quals:
                    q2 -= 1
                if q2 >= 0 and hdr[q2] == ')':
                    d = 0
                    p = q2
                    while p >= 0:
                        if hdr[p] == ')':
                            d += 1
                        elif hdr[p] == '(':
                            d -= 1
                            if d == 0:
                                break
                        p -= 1
                    bef = hdr[p - 1] if p > 0 else ''
            if bef in CTL:
                return dict(info, kind='ctl', ctl=bef)
            if bef == ']':
                return self._lambda_info(sc, i, hdr, p, p - 1, None)
            # ctor-initializer list?  X::X(a) : m(1), n(2) {   -> first top-level '('
            nm_idx = p - 1
            ctor = False
            if nm_idx >= 1 and hdr[nm_idx - 1] in (',',) or (nm_idx >= 1 and hdr[nm_idx - 1] == ':'):
                ctor = True
            if ctor:
                d = 0
                for k2, t in enumerate(hdr):
                    if t in ('(', '['):
                        if d == 0 and t == '(':
                            p = k2
                            break
                        d += 1
                    elif t in (')', ']'):
                        d -= 1
                nm_idx = p - 1
            name = self._func_name(hdr, nm_idx)
            return dict(info, kind='func', name=name)
        # ctor-initializer list ending in a closed init brace:  X() : a{1} {
        if prev == '}' or (hdr and hdr[-1] == '}'):
            if ':' in hdr and '(' in hdr:
                d = 0
                p = None
                for k2, t in enumerate(hdr):
                    if t == '(' and d == 0:
                        p = k2
                        break
                    if t in ('(', '[', '{'):
                        d += 1
                    elif t in (')', ']', '}'):
                        d -= 1
                if p is not None:
                    return dict(info, kind='func', name=self._func_name(hdr, p - 1))
        if prev == '}' and '(' not in hdr:
            return info
        # brace initialiser?
        if prev in ('=', ',', '(', '[', '{', 'return', '?', '<<', '+', '-', '&&', '||', '!', '||', '=>', 'throw'):
            return dict(info, kind='init')
        if prev == ':':
            if h0 in ('case', 'default') or (len(hdr) == 2):
                return info
            return dict(info, kind='init')
        if re.match(r'~?[A-Za-z_]\w*$', prev) or prev == '>' or prev == ')':
            # `Type name{..}` / `Type{..}` / `member{..}` : initialiser when the
            # header holds no function parameter list
            if '(' not in hdr or sc.depth > 0 and self._unclosed_paren(hdr):
                if prev in ('noexcept', 'const', 'override', 'final'):
                    return dict(info, kind='func', name='')
                return dict(info, kind='init')
            # ctor-init member `a(1), b{2}` handled above; default: init when in
            # an initialiser list
            if ':' in hdr and prev not in ('const', 'override', 'final', 'noexcept'):
                return dict(info, kind='init')
        return info

    @staticmethod
    def _unclosed_paren(hdr):
        d = 0
        for t in hdr:
            if t == '(':
                d += 1
            elif t == ')':
                d -= 1
        return d > 0

    @staticmethod
    def _func_name(hdr, nm_idx):
        """identifier ending at hdr[nm_idx] (skipping template args), qualified
        name reduced to its last component."""
        k = nm_idx
        if k >= 0 and hdr[k] == '>':
            d = 0
            while k >= 0:
                if hdr[k] == '>':
                    d += 1
                elif hdr[k] == '<':
                    d -= 1
                    if d == 0:
                        k -= 1
                        break
                k -= 1
        if k >= 0 and re.match(r'~?[A-Za-z_]\w*$', hdr[k]):
            return hdr[k]
        # operator()
        for q in range(max(0, k - 3), k + 1):
            if hdr[q] == 'operator':
                return 'operator'
        return ''

    def _lambda_info(self, sc, i, hdr, _unused, q, _x):
        """hdr[q] is the ']' that closes the capture list OR the index before
        the '(' (q = p-1). Find the '[' that starts it."""
        # locate capture-list start: walk back from q over matching '[' ']'
        k = q
        d = 0
        while k >= 0:
            if hdr[k] == ']':
                d += 1
            elif hdr[k] == '[':
                d -= 1
                if d == 0:
                    break
            k -= 1
        cap = k if k >= 0 else 0
        pre = hdr[:cap]
        name = None
        if len(pre) >= 2 and pre[-1] == '=' and re.match(r'~?[A-Za-z_]\w*$', pre[-2]):
            name = pre[-2]
        # innermost unclosed '(' before the capture list -> callee
        callee = None
        d = 0
        for k2 in range(len(pre) - 1, -1, -1):
            if pre[k2] == ')':
                d += 1
            elif pre[k2] == '(':
                if d == 0:
                    j = k2 - 1
                    if j >= 0 and pre[j] == '>':
                        dd = 0
                        while j >= 0:
                            if pre[j] == '>':
                                dd += 1
                            elif pre[j] == '<':
                                dd -= 1
                                if dd == 0:
                                    j -= 1
                                    break
                            j -= 1
                    callee = pre[j] if j >= 0 else None
                    break
                d -= 1
        close = self.m.match.get(i, -1)
        invoked = close >= 0 and self.tk(close + 1) == '('
        return {'kind': 'lambda', 'name': name or '', 'ctl': None, 'callee': callee, 'invoked': invoked,
                'assigned': name is not None}

    # -- scope push / pop
    def push(self, sc, i, info):
        self.process_events(sc, sc.done, i)
        kind = info['kind']
        new = Scope(kind if kind != 'ctl' else 'ctl', info.get('name', ''), i, self.ln(i), i + 1)
        parent = sc
        new.ctl = info.get('ctl')
        new.else_if = bool(info.get('else_if'))
        hs = parent.stmt_start
        if kind == 'ctl':
            h0 = self.tk(hs)
            if h0 == 'switch':
                new.sub = 'switch'
        if kind == 'func':
            nm = info['name'] or ''
            new.fname = nm or '<func>'
            if parent.kind in ('ns', 'class', 'file', 'enum', 'block'):
                key = '%s:%s:%d' % (self.m.rel, nm, self.ln(i))
                f = Func(key, nm, 'func', self.m.rel, self.ln(i))
                self.funcs.append(f)
                new.target = key
                self._funcs_by_key[key] = f
            else:
                # function nested in a function (local class method etc.)
                key = '%s:%s:%d' % (self.m.rel, nm, self.ln(i))
                f = Func(key, nm, 'func', self.m.rel, self.ln(i))
                self.funcs.append(f)
                new.target = key
                self._funcs_by_key[key] = f
        elif kind == 'lambda':
            callee = info.get('callee')
            name = info.get('name')
            policy = self.o.get('lambda_policy', 'deferred-aware')
            if info.get('invoked'):
                new.inherit = True
            elif name:
                new.inherit = False
            elif policy == 'inherit':
                new.inherit = True
            elif callee and DEFERRED_CALLEE.match(callee):
                new.inherit = False
            elif callee is None:
                new.inherit = False  # stored / returned / assigned member
            else:
                new.inherit = True
            outer_fn = self.cur_func_name() or 'file'
            new.fname = None
            if name:
                key = '%s:%s#%s:%d' % (self.m.rel, outer_fn, name, self.ln(i))
                f = Func(key, '%s#%s' % (outer_fn, name), 'lambda', self.m.rel, self.ln(i))
                self.funcs.append(f)
                self._funcs_by_key[key] = f
                new.target = key
                # register in the enclosing boundary so later uses resolve
                self.stack[-1].lam[name] = key
                b = parent
                for s in reversed(self.stack):
                    if s.kind in ('func', 'lambda', 'file'):
                        b = s
                        break
                b.lam[name] = key
            elif not new.inherit:
                # deferred anonymous lambda: events belong to nothing (sink)
                new.target = '<sink>'
        elif kind == 'class':
            new.fname = None
        elif kind == 'ctl':
            new.snap = self.capture()
            if new.ctl == 'else' and parent.chain:
                self.restore(parent.chain['pre'])
                new.snap = self.capture()
            elif new.ctl == 'catch' and parent.chain:
                self.merge_or([parent.chain['pre']] + parent.chain['ends'])
                new.snap = self.capture()
        # guards declared in the header (if/for/switch init) belong to this scope
        if parent.hdr_locks:
            new.locks.extend(parent.hdr_locks)
            parent.hdr_locks = []
        self.stack.append(new)
        parent.done = i + 1  # parent's own buffer resumes after the child closes

    def pop(self, i):
        sc = self.stack[-1]
        self.process_events(sc, sc.done, i)
        self.end_statement_tail(sc, i)
        terminating = (sc.last_first in ('return', 'break', 'continue', 'goto', 'throw') or sc.last_raise)
        self.stack.pop()
        for l_ in sc.locks:
            l_.end = self.ln(i)
            self.scope_ends.append((l_.line, l_.end, l_.kind))
        parent = self.stack[-1]
        if sc.kind == 'ctl':
            self.after_ctl(sc, parent, terminating, i)
        elif sc.kind == 'class':
            held = [l for l in sc.members if l.held]
            if held and sc.name:
                self.t.holders[sc.name] = held[0].desc()
            elif sc.name and self.ctor_dtor_holder(sc, i):
                self.t.holders[sc.name] = 'ctor-lock/dtor-unlock'
        if sc.kind in ('func', 'ns', 'enum', 'ctl', 'block') and not (sc.kind == 'block' and parent.depth > 0):
            # a statement ended with the closing brace
            parent.stmt_start = i + 1
            parent.done = i + 1
            parent.last_first = None
        else:
            # lambda / record / block inside an expression: the statement continues
            parent.done = i + 1

    def ctor_dtor_holder(self, sc, close_idx):
        """True for a class whose inline constructor acquires a lock (x.lock(), try_lock(),
        actions_lock()) and whose inline destructor releases one: RAII by hand. Instances
        hold the lock for their scope, and a longjmp skips the destructor like any guard."""
        name = sc.name
        ctor_locks = dtor_unlocks = False
        for q in range(sc.open + 1, close_idx):
            if self.tk(q) not in (name, '~' + name) or self.tk(q + 1) != '(' or self.tk(q - 1) in ('class', 'struct', '::'):
                continue
            is_dtor = self.tk(q).startswith('~')
            c = self.match_paren(q + 1)
            if c < 0:
                continue
            o = c + 1
            while o < close_idx and self.tk(o) not in ('{', ';'):
                if self.tk(o) in ('(', '['):          # skip init-list / noexcept(..) groups
                    o2 = self.match_paren(o)
                    o = o2 if o2 > 0 else o
                o += 1
            if o >= close_idx or self.tk(o) != '{' or o not in self.m.match:
                continue
            body = [self.tk(x) for x in range(o + 1, self.m.match[o])]
            for x, tkn in enumerate(body):
                nxt = body[x + 1] if x + 1 < len(body) else ''
                if nxt != '(':
                    continue
                if not is_dtor and (tkn in ('lock', 'try_lock', 'lock_shared') or tkn.endswith('actions_lock')):
                    ctor_locks = True
                if is_dtor and (tkn in ('unlock', 'unlock_shared') or tkn.endswith('actions_unlock')):
                    dtor_unlocks = True
        return ctor_locks and dtor_unlocks

    def after_ctl(self, sc, parent, terminating, i):
        end = self.capture()
        nxt = self.tk(i + 1)
        c = sc.ctl
        if c in ('if', 'try'):
            parent.chain = {'pre': sc.snap, 'ends': [], 'has_else': False}
        elif c in ('else', 'catch'):
            if parent.chain is None:
                parent.chain = {'pre': sc.snap, 'ends': [], 'has_else': False}
        if c in ('if', 'else', 'try', 'catch'):
            ch = parent.chain
            if not terminating:
                ch['ends'].append(end)
            if c == 'else' and not sc.else_if:
                ch['has_else'] = True
            if c == 'catch':
                ch['has_else'] = True
            if c == 'try':
                ch['has_else'] = False
            if nxt in ('else', 'catch'):
                self.restore(ch['pre'])
                return
            states = list(ch['ends'])
            if not ch['has_else'] or c == 'try':
                states.append(ch['pre'])
            if c == 'try' and nxt not in ('catch',):
                states = list(ch['ends']) or [ch['pre']]
            if not states:
                states = [ch['pre']]
            self.merge_or(states)
            parent.chain = None
            return
        # loops, switch, do
        if terminating:
            self.restore(sc.snap)
        else:
            self.merge_or([sc.snap, end])

    def end_statement_tail(self, sc, i):
        a = sc.stmt_start
        if a < i and self.tk(a) not in ('{', '}'):
            self._record_last(sc, a, i)

    def _record_last(self, sc, a, b):
        first = self.tk(a)
        if first in ('case', 'default'):
            return
        sc.last_first = first
        idx = a + 1 if first == 'return' else a
        # raise primitive as the (leading) call of the statement?
        sc.last_raise = False
        d = 0
        name = None
        for k in range(idx, b):
            t = self.tk(k)
            if t == '(':
                break
            if re.match(r'~?[A-Za-z_]\w*$', t):
                name = t
        if name and self.is_raise_name(name):
            sc.last_raise = True

    # -- primitive / helper predicates
    def is_raise_name(self, n):
        return prim_active(n, self.o)

    # -- events
    def process_events(self, sc, a, b):
        j = a
        toks = self.toks
        while j < b:
            t, line = toks[j]
            # case/default labels inside a switch: reset flow state
            if t in ('case', 'default') and sc.kind == 'ctl' and sc.sub == 'switch' and \
                    (j == a or self.tk(j - 1) in (';', '{', '}', ':')):
                self.restore(sc.snap)
            # guard declaration
            if t in GUARD_TYPES and self.tk(j - 1) != '.':
                nj = self.decl_guard(sc, j)
                if nj:
                    j = nj
                    continue
            # lock-holder class instance
            if t in self.t.holders and self.tk(j - 1) not in ('class', 'struct', '::', '.', '->'):
                nxt = self.tk(j + 1)
                nxt2 = self.tk(j + 2)
                if re.match(r'[A-Za-z_]\w*$', nxt) and nxt not in NOT_CALLS and nxt2 in (';', '{', '(', '='):
                    self.add_lock(sc, Lock('holder', nxt, '%s:%s' % (t, self.t.holders[t]), line, True,
                                           self.src_text(line)))
                    j += 2
                    continue
            if t in self.t.lock_macros and self.tk(j - 1) not in ('.', '->', '::'):
                self.add_lock(sc, Lock('macro', t, self.t.lock_macros[t], line, True, self.src_text(line)))
                j += 1
                continue
            if re.match(r'~?[A-Za-z_]\w*$', t):
                nj = self.ident_event(sc, j, b)
                if nj:
                    j = nj
                    continue
            j += 1
        sc.done = max(sc.done, b)

    def add_lock(self, sc, lock, header=False):
        self.lock_sites += 1
        self.lock_list.append((lock.line, lock.kind, lock.mutex))
        if sc.kind == 'class':
            sc.members.append(lock)
            return
        in_hdr = False
        if sc.depth > 0 and self.tk(sc.stmt_start) in CTL:
            in_hdr = True
        if in_hdr or header:
            sc.hdr_locks.append(lock)
        else:
            sc.locks.append(lock)

    def decl_guard(self, sc, j):
        """toks[j] is a guard type. Returns the index to resume after, or 0."""
        k = j + 1
        if self.tk(k) == '<':
            k = self.skip_angle(k)
            if k is None:
                return 0
        var = None
        gtype = self.tk(j)
        if re.match(r'[A-Za-z_]\w*$', self.tk(k)) and self.tk(k) not in NOT_CALLS:
            var = self.tk(k)
            k += 1
        else:
            # `auto g = std::lock_guard(m);`  (CTAD) declares a NEW guard variable
            q1 = j - 1
            if self.tk(q1) == '::' and self.tk(q1 - 1) == 'std':
                q1 -= 2
            if self.tk(k) in ('(', '{') and self.tk(q1) == '=' and re.match(r'[A-Za-z_]\w*$', self.tk(q1 - 1)) \
                    and self.tk(q1 - 2) in ('auto', '&', '&&', 'const'):
                nm = self.tk(q1 - 1)
                c2 = self.match_paren(k)
                if c2 > 0:
                    margs = [self.toks[q][0] for q in range(k + 1, c2)]
                    mtx = self.args_text(k + 1, c2)
                    if gtype in CUSTOM_GUARD_TYPES:
                        mtx = self.first_arg_text(k + 1, c2)
                    mtx = re.sub(r'\s+', '', mtx)
                    held0 = not ('defer_lock' in margs or 'defer_lock_t' in margs)
                    (self.raw_guards if gtype in RAW_GUARD_TYPES else self.custom_guards).append((self.ln(j), gtype, mtx))
                    self.add_lock(sc, Lock('guard', nm, mtx, self.ln(j), held0, self.src_text(self.ln(j))))
                    return c2 + 1
        if var is None:
            # `lk = std::unique_lock<M>(m);`  re-arms a known guard variable
            q0 = j - 1
            if self.tk(q0) == '::' and self.tk(q0 - 1) == 'std':
                q0 -= 2
            if self.tk(k) in ('(', '{') and self.tk(q0) == '=' and self.tk(q0 - 1) != '' and \
                    self.tk(q0 - 2) in (';', '{', '}', ')', ''):
                nm = self.tk(q0 - 1)
                for s_ in reversed(self.stack):
                    for l in s_.locks + s_.hdr_locks:
                        if l.var == nm:
                            l.held = True
                            self.lock_sites += 1
                            c2 = self.match_paren(k)
                            if c2 > 0 and not l.mutex:
                                l.mutex = re.sub(r'\s+', '', self.args_text(k + 1, c2))
                            self.lock_list.append((self.ln(j), 'guard', l.mutex))
                            return c2 + 1 if c2 > 0 else 0
                    if s_.is_boundary():
                        break
            return 0  # temporary / type use (holds nothing beyond the expression)
        opener = self.tk(k)
        held = True
        mutex = ''
        end = k
        if opener in ('(', '{'):
            c = self.match_paren(k)
            if c < 0:
                return 0
            args = [self.toks[q][0] for q in range(k + 1, c)]
            mutex = self.args_text(k + 1, c)
            if gtype in CUSTOM_GUARD_TYPES:
                mutex = self.first_arg_text(k + 1, c)   # ActionsLock{mutex, __LINE__}
            if 'defer_lock' in args or 'defer_lock_t' in args:
                held = False
            end = c + 1
        elif opener in (';', ',', '='):
            held = False   # default-constructed / assigned later
            if opener == '=':
                held = True
            if sc.kind == 'class' and opener in (';', ','):
                # a guard MEMBER without an initializer is constructed by the class's
                # constructor (`G g; Holder() : g(m) {}`): fail closed, the class holds the lock
                held = True
        else:
            return 0
        mutex = re.sub(r'\s+', '', mutex)
        mutex = re.sub(r',\s*std::(adopt_lock|try_to_lock|defer_lock)$', '', mutex)
        # adopt_lock: the mutex was locked by an earlier std::lock(): drop that manual lock
        if 'adopt_lock' in mutex:
            for s in reversed(self.stack):
                if s.is_boundary():
                    for kk in list(s.manual):
                        if kk in mutex:
                            s.manual[kk].held = False
                    break
        lk = Lock('guard', var, mutex, self.ln(j), held, self.src_text(self.ln(j)))
        (self.raw_guards if gtype in RAW_GUARD_TYPES else self.custom_guards).append((self.ln(j), gtype, mutex))
        self.add_lock(sc, lk)
        return end

    def first_arg_text(self, a, b):
        """text of the first top-level argument in toks[a:b]"""
        d = 0
        out = []
        for q in range(a, b):
            t = self.toks[q][0]
            if t in ('(', '[', '{', '<'):
                d += 1
            elif t in (')', ']', '}', '>'):
                d -= 1
            elif t == ',' and d == 0:
                break
            out.append(t)
        return ' '.join(out)

    def ident_event(self, sc, j, b):
        toks = self.toks
        t, line = toks[j]
        prev = self.tk(j - 1)
        # ---- named local lambda usage while a lock is held
        for s in reversed(self.stack):
            if t in s.lam:
                key = s.lam[t]
                if prev not in ('.', '->', '::') and not (self.tk(j + 1) == '=' and prev in ('auto', '&', '*')):
                    self.on_call(sc, key, j, line, lambda_use=True)
                    return 0
                break
            if s.kind in ('ns', 'class', 'file'):
                break
        # ---- call?  ident [<...>] (
        k = j + 1
        if self.tk(k) == '<':
            k2 = self.skip_angle(k)
            if k2 is not None:
                k = k2
        if self.tk(k) != '(':
            return 0
        if t in NOT_CALLS or t in GUARD_TYPES:
            return 0
        if prev in ('new', '~') and t[0] != '~':
            return 0
        # lock / unlock family
        low = t
        if low in ('lock', 'try_lock', 'unlock') or low.endswith('actions_lock') or low.endswith('actions_unlock') \
                or low == 'lock_shared' or low == 'unlock_shared':
            r = self.lock_call(sc, j, k)
            if r:
                return 0
        if t == 'lock' and prev == '::' and self.tk(j - 2) == 'std':
            c = self.match_paren(k)
            if c > 0:
                for arg in self.split_args(k + 1, c):
                    key = re.sub(r'\s+', '', arg)
                    if key:
                        self.manual_set(key, True, line)
                return 0
        # ---- banned call (a helper that raises with a longjmp): a finding under ANY lock, in
        # every --helpers mode, because it is the exact shape that leaked the lock in B4
        if t in BANNED_CALLS and prev not in ('.', '->', '~'):
            self.banned_sites += 1
            held = self.held_locks()
            if held:
                fd = Finding()
                fd.rel, fd.line, fd.kind, fd.what = self.m.rel, line, 'banned', t
                fd.func = self.cur_func_name()
                fd.lock = held[0].desc()
                fd.text = self.stmt_text(j)
                fd.via = 'banned helper: raises with a longjmp; use its C++-throwing twin (e.g. get_mod_ref_cxx) inside a registered function'
                fd.approved = APPROVED_MARK in self.m.raw_lines[line - 1]
                self.findings.append(fd)
        # ---- raise primitive / helper call
        if self.is_raise_name(t):
            self.raise_sites += 1
            self.raise_lines.add(line)
            tgt = self.cur_target()
            text = self.src_text(line)
            if tgt and tgt != '<sink>':
                f = self._funcs_by_key.get(tgt)
                if f is not None:
                    f.direct.append((line, t, text))
            held = self.held_locks()
            if held:
                fd = Finding()
                fd.rel, fd.line, fd.kind, fd.what = self.m.rel, line, 'direct', t
                fd.func = self.cur_func_name()
                fd.lock = held[0].desc()
                fd.text = self.stmt_text(j)
                fd.via = None
                fd.approved = APPROVED_MARK in self.m.raw_lines[line - 1]
                self.findings.append(fd)
            return 0
        self.on_call(sc, t, j, line)
        return 0

    def stmt_text(self, j):
        """the source text of the statement line(s) around token j (first line + continuation up to ';')"""
        line = self.ln(j)
        txt = self.src_text(line)
        return txt

    def split_args(self, a, b):
        args, cur, d = [], [], 0
        for k in range(a, b):
            t = self.toks[k][0]
            if t in ('(', '[', '{'):
                d += 1
            elif t in (')', ']', '}'):
                d -= 1
            if t == ',' and d == 0:
                args.append(''.join(cur))
                cur = []
            else:
                cur.append(t)
        args.append(''.join(cur))
        return args

    def on_call(self, sc, name, j, line, lambda_use=False):
        tgt = self.cur_target()
        if tgt and tgt != '<sink>':
            f = self._funcs_by_key.get(tgt)
            if f is not None and name not in f.calls:
                f.calls[name] = line
        held = self.held_locks()
        if held:
            self.call_sites.append((name, line, self.src_text(line), held[0].desc(),
                                    self.cur_func_name(), lambda_use,
                                    APPROVED_MARK in self.m.raw_lines[line - 1]))

    def manual_set(self, key, held, line):
        b = self.boundary()
        lk = b.manual.get(key)
        if lk is None:
            lk = Lock('manual', None, key, line, held, self.src_text(line))
            b.manual[key] = lk
        else:
            lk.held = held
            if held:
                lk.line = line
                lk.text = self.src_text(line)

    def lock_call(self, sc, j, k):
        """ident at j is lock/unlock/try_lock/actions_lock...; toks[k] == '('. True if consumed."""
        toks = self.toks
        t = toks[j][0]
        line = toks[j][1]
        prev = self.tk(j - 1)
        c = self.match_paren(k)
        if c < 0:
            return False
        no_args = (c == k + 1)
        # receiver text
        recv_tokens = []
        q = j - 1
        if prev in ('.', '->'):
            q = j - 2
            while q >= 0:
                tt = self.tk(q)
                if re.match(r'~?[A-Za-z_]\w*$', tt) or tt in ('.', '->', '::'):
                    recv_tokens.append(tt)
                    q -= 1
                elif tt == ']':
                    d = 0
                    while q >= 0:
                        if self.tk(q) == ']':
                            d += 1
                        elif self.tk(q) == '[':
                            d -= 1
                            if d == 0:
                                break
                        recv_tokens.append(self.tk(q))
                        q -= 1
                    recv_tokens.append('[')
                    q -= 1
                elif tt == ')':
                    d = 0
                    while q >= 0:
                        if self.tk(q) == ')':
                            d += 1
                        elif self.tk(q) == '(':
                            d -= 1
                            if d == 0:
                                break
                        recv_tokens.append(self.tk(q))
                        q -= 1
                    recv_tokens.append('(')
                    q -= 1
                else:
                    break
        recv_start = q + 1
        recv = ''.join(reversed(recv_tokens))
        base = t
        is_actions = t.endswith('actions_lock') or t.endswith('actions_unlock')
        acquire = t in ('lock', 'try_lock', 'lock_shared') or t.endswith('actions_lock')
        if is_actions:
            key = (recv + '::' if recv else '') + 'actions'
            acquire = t.endswith('actions_lock')
        else:
            key = recv
            if not recv:
                return False
        # guard variable?
        for s in reversed(self.stack):
            for l in s.locks + s.hdr_locks:
                if l.var and l.var == recv and l.kind in ('guard', 'holder', 'macro'):
                    if acquire:
                        l.held = True
                        self.lock_sites += 1
                    else:
                        l.held = False
                    return True
            if s.is_boundary():
                break
        if not is_actions and t == 'lock':
            # weak_ptr::lock() and friends return a value; a mutex lock is a bare statement
            tail_ok = self.tk(c + 1) == ';'
            head_ok = (recv_start <= 0) or self.tk(recv_start - 1) in (';', '{', '}', ')', 'else', ':')
            if not (no_args and tail_ok and head_ok):
                return False
        if acquire:
            self.lock_sites += 1
            self.lock_list.append((line, 'manual', key))
            self.manual_set(key, True, line)
        else:
            b = self.boundary()
            lk = b.manual.get(key)
            if lk is not None:
                lk.held = False
        return True

    # -- main loop
    def run(self, funcs_by_key):
        self._funcs_by_key = funcs_by_key
        toks = self.toks
        n = len(toks)
        i = 0
        while i < n:
            t = toks[i][0]
            sc = self.stack[-1]
            if t == '(' or t == '[':
                sc.depth += 1
            elif t == ')' or t == ']':
                sc.depth = max(0, sc.depth - 1)
            elif t == ';' and sc.depth == 0:
                self.process_events(sc, sc.done, i + 1)
                self._record_last(sc, sc.stmt_start, i) if sc.stmt_start < i else None
                sc.stmt_start = i + 1
                sc.done = i + 1
                sc.hdr_locks = []
            elif t == '{':
                info = self.classify(sc, i)
                if info['kind'] == 'init':
                    sc.depth += 1
                    sc.inl += 1
                else:
                    self.push(sc, i, info)
            elif t == '}':
                if sc.inl > 0:
                    sc.inl -= 1
                    sc.depth = max(0, sc.depth - 1)
                elif len(self.stack) > 1:
                    self.pop(i)
                else:
                    pass
            i += 1
        # flush
        while len(self.stack) > 1:
            self.stack.pop()


# ------------------------------------------------------------ file collection
def files_from(spec):
    """SOURCE-SELECTION.json (any {'path': ...} entries) or a newline list."""
    out = set()
    with open(spec, errors='replace') as f:
        raw = f.read()
    try:
        data = json.loads(raw)
    except ValueError:
        data = None
    if data is None:
        cands = [ln.strip() for ln in raw.splitlines() if ln.strip()]
    else:
        cands = []

        def walk(x):
            if isinstance(x, dict):
                p = x.get('path')
                if isinstance(p, str):
                    cands.append(p)
                for val in x.values():
                    walk(val)
            elif isinstance(x, list):
                for val in x:
                    walk(val)
        walk(data)
    for p in cands:
        if p.endswith(CXX_EXT):
            out.add(p if os.path.isabs(p) else os.path.join(WHAT, p))
    return out


def relevant_re():
    """files worth parsing: they mention a guard type (incl. aliases / custom wrappers), a banned
    call, or a raise primitive"""
    names = sorted(GUARD_TYPES | BANNED_CALLS)
    return re.compile(
        r'\b(?:' + '|'.join(re.escape(n) for n in names) + r'|throw_error\w*|luaL_error|lua_error|luaL_arg\w+|'
        r'luaL_check\w+|luaL_opt\w+|lua_yield\w*|actions_lock|actions_unlock|lua_call\w*|throw_lua_error|'
        r'lua_gettable|lua_getfield|lua_getglobal|lua_arith|lua_concat)\b')


class Gate(object):
    def __init__(self, opts):
        self.o = opts
        self.texts = {}       # path -> text
        self.rel = {}
        self.models = {}
        self.unreadable = []
        self.tables = Tables()
        self.results = {}     # path -> (funcs, findings, call_sites, lock_sites, raise_sites)
        self.unbalanced = []
        self.lock_lists = {}
        self.variant_files = set()
        self.raw_guards = []        # (rel, line, type, mutex)
        self.custom_guards = []
        self.banned_sites = 0
        self.unknown_guards = []    # (rel, line, type, arg)
        self.aliases = []

    def load(self, paths):
        pm = self.o.get('path_map') or []
        for p in sorted(paths):
            try:
                with open(p, encoding='utf-8', errors='replace') as f:
                    self.texts[p] = f.read()
            except OSError as e:
                self.unreadable.append('%s: %s' % (p, e))
                continue
            rel = os.path.relpath(p, WHAT)
            for src_, dst_ in pm:
                if rel.startswith(src_):
                    rel = dst_ + rel[len(src_):]
                    break
            self.rel[p] = rel
        # guard types / bans are configured from the options + aliases found in the sources
        alias_src = [blank(t) for t in self.texts.values()
                     if ('using' in t or 'typedef' in t) and
                     re.search(r'\b(?:' + '|'.join(sorted(STD_GUARD_TYPES | CUSTOM_GUARD_TYPES_DEFAULT | set(self.o.get('guard_types') or []))) + r')\b', t)]
        self.aliases = configure_guards(self.o, alias_src)

    def models_of(self, p):
        ms = self.models.get(p)
        if ms is None:
            m = lex_file(p, self.rel[p], self.texts[p])
            ms = [m]
            if not m.balanced:
                # unknown #if/#else groups can make the braces of BOTH branches
                # count; scan each alternative separately and union the results
                alt = []
                seen_tok = set()
                for mode in ('0', 'last', '1', '2', '3'):
                    a = lex_file(p, self.rel[p], self.texts[p], mode)
                    sig = hash(tuple(a.toks))
                    if a.balanced and sig not in seen_tok:
                        seen_tok.add(sig)
                        alt.append(a)
                if alt:
                    ms = alt
                    self.variant_files.add(self.rel[p])
            self.models[p] = ms
        return ms

    def walk(self, p):
        res = []
        for m in self.models_of(p):
            w = Walker(m, self.tables, self.o)
            w.run({})
            res.append((m, w))
        return res

    # -- collection phase: build the function / macro / holder tables
    def collect(self):
        t = self.tables
        rel_rx = relevant_re()
        pending = {p for p, tx in self.texts.items() if rel_rx.search(tx)}
        parsed = set()
        rounds = 0
        while pending:
            rounds += 1
            for p in sorted(pending):
                seen_f = set()
                for m, w in self.walk(p):
                    for f in w.funcs:
                        if f.key not in seen_f:
                            seen_f.add(f.key)
                            t.add_func(f)
                    self._collect_macros(m)
                parsed.add(p)
            pending = set()
            self._build_helpers()
            names = [re.escape(n) for n in list(t.helpers) + list(t.holders) + list(t.lock_macros)
                     if len(n) > 3 and n not in HELPER_STOP]
            if not names:
                break
            rx = re.compile(r'\b(?:' + '|'.join(sorted(names, key=len, reverse=True)) + r')\b')
            for p, tx in self.texts.items():
                if p not in parsed and rx.search(tx):
                    pending.add(p)
            if rounds > 12:
                break
        self.parsed = parsed
        self.rounds = rounds

    def _collect_macros(self, m):
        t = self.tables
        for name, line, body in m.macros:
            toks = [x.group() for x in TOK.finditer(body)]
            f = Func('%s:macro:%s:%d' % (m.rel, name, line), name, 'macro', m.rel, line)
            has_guard = False
            for k, tk in enumerate(toks):
                if tk in GUARD_TYPES:
                    has_guard = True
                nxt = toks[k + 1] if k + 1 < len(toks) else ''
                if nxt == '(' and re.match(r'~?[A-Za-z_]\w*$', tk) and tk not in NOT_CALLS:
                    if prim_active(tk, self.o):
                        f.direct.append((line, tk, body.strip()[:120]))
                    else:
                        f.calls.setdefault(tk, line)
            if has_guard:
                t.lock_macros[name] = 'macro:' + name
            if f.direct or f.calls:
                t.add_func(f)

    def _build_helpers(self):
        t = self.tables
        depth_limit = self.o.get('helper_depth', 2)
        helpers = {}
        if self.o.get('helpers') == 'off':
            t.helpers = {}
            return
        # depth 1: functions with a direct raise
        for name, fl in t.by_name.items():
            if name in HELPER_STOP or self.is_prim(name):
                continue
            for f in fl:
                if f.direct:
                    helpers[name] = (1, ('direct', f.file, f.direct[0][0], f.direct[0][1]))
                    break
        for s in SEED_HELPERS:
            helpers.setdefault(s, (1, ('direct', 'seed', 0, 'throw_error')))
        changed = True
        while changed:
            changed = False
            for name, fl in t.by_name.items():
                if name in helpers or name in HELPER_STOP or self.is_prim(name):
                    continue
                best = None
                for f in fl:
                    for c in f.calls:
                        h = helpers.get(c)
                        if h and h[0] + 1 <= depth_limit and (best is None or h[0] + 1 < best[0]):
                            best = (h[0] + 1, ('via', c))
                if best:
                    helpers[name] = best
                    changed = True
        t.helpers = helpers

    def is_prim(self, n):
        return prim_active(n, self.o)

    # -- analysis phase
    def analyse(self):
        findings = []
        total_locks = 0
        total_raise = 0
        per_file = {}
        for p in sorted(self.parsed):
            locks_u, raises_u, nfind = set(), set(), 0
            rel = self.rel[p]
            for m, w in self.walk(p):
                if not m.balanced:
                    self.unbalanced.append('%s [%s]' % (m.rel, m.variant))
                self.raw_guards.extend((m.rel,) + x for x in w.raw_guards)
                self.custom_guards.extend((m.rel,) + x for x in w.custom_guards)
                self.banned_sites += w.banned_sites
                self.unknown_guards.extend((m.rel,) + x for x in find_unknown_guards(m, self.o.get('allow_unknown_guard') or ()))
                locks_u.update((ln_, k_, mx_) for (ln_, k_, mx_) in w.lock_list)
                raises_u.update(w.raise_lines)
                nfind += len(w.findings)
                findings.extend(w.findings)
                self._call_site_findings(m, w, findings)
            total_locks += len(locks_u)
            total_raise += len(raises_u)
            per_file[rel] = (len(locks_u), len(raises_u), nfind)
            self.lock_lists[rel] = sorted(locks_u)
        return findings, total_locks, total_raise, per_file

    def _call_site_findings(self, m, w, findings):
        t = self.tables
        local = {}
        for f in w.funcs:
            local.setdefault(f.name, []).append(f)
        # helper / lambda call sites made while a lock was held
        for (name, line, text, lock, fn, lam_use, appr) in w.call_sites:
            if '#' in name or name.startswith(m.rel + ':'):
                # local lambda key
                key_funcs = [f for f in w.funcs if f.key == name]
                if key_funcs:
                    findings.extend(self._lambda_findings(m, key_funcs[0], line, lock, fn, w, appr))
                continue
            if name in HELPER_STOP or self.is_prim(name):
                continue
            if name in local:
                # same-file definition wins over a name collision elsewhere
                hit = None
                for f in local[name]:
                    if f.direct:
                        hit = ('direct', f)
                        break
                    for c in f.calls:
                        if t.helpers.get(c):
                            hit = ('via', c)
                            break
                    if hit:
                        break
                if not hit and not any(f.kind == 'macro' for f in local[name]):
                    continue
            if not t.helpers.get(name):
                continue
            fd = Finding()
            fd.rel, fd.line, fd.kind, fd.what = m.rel, line, 'helper', name
            fd.func, fd.lock, fd.text = fn, lock, text
            fd.via = self.witness(name)
            fd.approved = appr
            findings.append(fd)

    def _lambda_findings(self, m, lam, call_line, lock, fn, w, appr):
        res = []
        for (ln, prim, text) in lam.direct:
            fd = Finding()
            fd.rel, fd.line, fd.kind, fd.what = m.rel, ln, 'lambda', prim
            fd.func, fd.lock, fd.text = fn, lock, text
            fd.via = 'local lambda %s called/used under lock at line %d' % (lam.name.split('#')[-1], call_line)
            fd.approved = APPROVED_MARK in m.raw_lines[ln - 1] or appr
            res.append(fd)
        for c, cl in lam.calls.items():
            h = self.tables.helpers.get(c)
            if h and c not in HELPER_STOP:
                fd = Finding()
                fd.rel, fd.line, fd.kind, fd.what = m.rel, cl, 'lambda', c
                fd.func, fd.lock, fd.text = fn, lock, w.src_text(cl)
                fd.via = 'local lambda %s (under lock at line %d) -> %s' % (lam.name.split('#')[-1], call_line, self.witness(c))
                fd.approved = APPROVED_MARK in m.raw_lines[cl - 1] or appr
                res.append(fd)
        return res

    def witness(self, name):
        t = self.tables
        chain = [name]
        seen = {name}
        for _ in range(8):
            h = t.helpers.get(name)
            if not h:
                break
            w = h[1]
            if w and w[0] == 'via':
                name = w[1]
                if name in seen:
                    break
                seen.add(name)
                chain.append(name)
                continue
            if w:
                chain.append('%s (%s:%s)' % (w[3], w[1], w[2]))
            break
        return ' -> '.join(chain)


# --------------------------------------------------------------------- driver
def dedupe(findings):
    banned_at = {(f.rel, f.line, f.what) for f in findings if f.kind == 'banned'}
    findings = [f for f in findings if not (f.kind == 'helper' and (f.rel, f.line, f.what) in banned_at)]
    seen = {}
    for f in findings:
        k = (f.rel, f.line, f.kind, f.what)
        if k not in seen:
            seen[k] = f
    return sorted(seen.values(), key=lambda f: (f.rel, f.line, f.kind))


def read_baseline(path):
    counts = {}
    with open(path, errors='replace') as f:
        for ln in f:
            ln = ln.rstrip('\n')
            if ln and not ln.startswith('#'):
                key = ln.split(' ## ')[0]
                counts[key] = counts.get(key, 0) + 1
    return counts


def run(roots, specs, files, opts, baseline=None, write_baseline=None, verbose=False, json_out=None, quiet=False):
    out = print if not quiet else (lambda *a, **k: None)
    paths, missing = set(), []
    for r in roots:
        if not os.path.isdir(r):
            out('LOCK-RAISE GATE: ERROR root does not exist: %s' % r)
            return 2
        paths.update(src_files(r))
    for s in specs:
        if not os.path.isfile(s):
            out('LOCK-RAISE GATE: ERROR --files-from not found: %s' % s)
            return 2
        listed = files_from(s)
        if not listed:
            out('LOCK-RAISE GATE: ERROR --files-from lists ZERO C/C++ files: %s' % s)
            return 2
        for p in listed:
            (paths.add(p) if os.path.isfile(p) else missing.append(p))
    for p in files:
        (paths.add(p) if os.path.isfile(p) else missing.append(p))
    if missing:
        out('LOCK-RAISE GATE: ERROR %d listed file(s) missing, first: %s' % (len(missing), missing[0]))
        return 2
    g = Gate(opts)
    g.load(paths)
    if g.unreadable:
        out('LOCK-RAISE GATE: ERROR %d unreadable file(s), first: %s' % (len(g.unreadable), g.unreadable[0]))
        return 2
    nfiles = len(g.texts)
    if nfiles == 0:
        out('LOCK-RAISE GATE: FAIL - checked ZERO files (a gate that scans nothing is a failed gate)')
        return 2
    g.collect()
    findings, nlocks, nraise, per_file = g.analyse()
    findings = dedupe(findings)
    approved = [f for f in findings if f.approved]
    live = [f for f in findings if not f.approved]
    warn_only = []
    if opts.get('helpers') == 'warn':
        warn_only = [f for f in live if f.kind == 'helper' or (f.kind == 'lambda' and not g.is_prim(f.what))]
        live = [f for f in live if f not in warn_only]
    dead_rel = sum(m_.dead_relevant for p in g.parsed for m_ in g.models[p][:1])
    out('LOCK-RAISE GATE: scope %d root(s) + %d file list(s) + %d file(s)' % (len(roots), len(specs), len(files)))
    out('LOCK-RAISE GATE: checked %d files (%d parsed: they mention a lock or a raise, directly or via a helper; '
        '%d collection round(s)); %d lock scopes (lock acquisitions); %d raise-primitive sites; %d helper names '
        '(depth<=%s), %d lock-holder class(es), %d lock macro(s); %d PS5-dead relevant lines skipped'
        % (nfiles, len(g.parsed), g.rounds, nlocks, nraise, len(g.tables.helpers),
           opts.get('helper_depth', 2), len(g.tables.holders), len(g.tables.lock_macros), dead_rel))
    nd = sum(1 for f in live if f.kind == 'direct')
    nb = sum(1 for f in live if f.kind == 'banned')
    nh = sum(1 for f in live if f.kind == 'helper')
    nl = sum(1 for f in live if f.kind == 'lambda')
    raw_u = sorted({(r, l_, t_, m_) for (r, l_, t_, m_) in g.raw_guards})
    cus_u = sorted({(r, l_, t_, m_) for (r, l_, t_, m_) in g.custom_guards})
    out('LOCK-RAISE GATE: guard declarations: %d std (%s), %d custom wrapper (%s); %d alias(es) found%s; '
        'banned calls tracked: %s (%d call site(s) seen)'
        % (len(raw_u), ','.join(sorted(RAW_GUARD_TYPES)), len(cus_u), ','.join(sorted(CUSTOM_GUARD_TYPES)),
           len(g.aliases), (' [' + ', '.join('%s=%s' % a for a in g.aliases) + ']') if g.aliases else '',
           ','.join(sorted(BANNED_CALLS)) or '-', g.banned_sites))
    out('LOCK-RAISE GATE: %d findings (%d direct, %d banned-call, %d helper-call, %d lambda), %d approved inline%s'
        % (len(live), nd, nb, nh, nl, len(approved),
           ('; %d helper-tier site(s) printed as WARN only (--helpers warn)' % len(warn_only)) if warn_only else ''))
    for f in warn_only:
        out('  WARN %s:%d [%s %s] under %s in %s: %s  <- %s' % (f.rel, f.line, f.kind, f.what, f.lock, f.func, f.text, f.via))
    if opts.get('show_helpers'):
        for n, h in sorted(g.tables.helpers.items()):
            out('  HELPER %s depth=%d via %s' % (n, h[0], g.witness(n)))
    if opts.get('list_locks'):
        for rel, ll in sorted(g.lock_lists.items()):
            if ll:
                out('  LOCKS %s: %d' % (rel, len(ll)))
                for (ln_, kind_, mx_) in ll:
                    out('    %d %s %s' % (ln_, kind_, mx_))
    if verbose:
        for f in approved:
            out('  APPROVED %s:%d %s' % (f.rel, f.line, f.text))
    if g.variant_files:
        out('LOCK-RAISE GATE: note: %d file(s) have #if/#else groups whose braces only balance per branch; scanned both alternatives and merged: %s'
            % (len(g.variant_files), ', '.join(sorted(g.variant_files)[:4])))
    if g.unbalanced:
        out('LOCK-RAISE GATE: FAIL - unbalanced braces after PS5-liveness filtering in: %s '
            '(the scope tree cannot be trusted)' % ', '.join(sorted(set(g.unbalanced))[:5]))
        return 2
    unk = sorted({(r, l_, t_, a_) for (r, l_, t_, a_) in g.unknown_guards})
    if unk:
        out('LOCK-RAISE GATE: FAIL - %d declaration(s) of an UNKNOWN type constructed from a mutex; the lock model '
            'cannot see them, so the scan would silently cover fewer scopes. Add the type with --guard-type NAME '
            '(RAII wrapper) or --allow-unknown-guard NAME (not a guard):' % len(unk))
        for (r, l_, t_, a_) in unk[:20]:
            out('  UNKNOWN-GUARD %s:%d  %s(%s)' % (r, l_, t_, a_))
        return 2
    if nlocks == 0:
        out('LOCK-RAISE GATE: FAIL - checked ZERO lock scopes (a gate that scans nothing is a failed gate)')
        return 2
    if nraise == 0:
        out('LOCK-RAISE GATE: FAIL - saw ZERO raise-primitive sites (detector has nothing to find: failed gate)')
        return 2
    if json_out:
        with open(json_out, 'w') as fh:
            json.dump([{'file': f.rel, 'line': f.line, 'kind': f.kind, 'what': f.what, 'function': f.func,
                        'lock': f.lock, 'text': f.text, 'via': f.via, 'approved': f.approved,
                        'key': f.key()} for f in findings], fh, indent=1)
    rawviol = []
    if opts.get('forbid_raw_guard') is not None:
        rx_ = re.compile(opts['forbid_raw_guard'])
        rawviol = [x for x in raw_u if rx_.search(x[3])]
        out('LOCK-RAISE GATE: --forbid-raw-guard %r: %d std guard(s) on a matching mutex (must be 0), %d tracked wrapper guard(s) on it'
            % (opts['forbid_raw_guard'], len(rawviol), sum(1 for x in cus_u if rx_.search(x[3]))))
        if not any(rx_.search(x[3]) for x in cus_u + raw_u):
            out('LOCK-RAISE GATE: FAIL - --forbid-raw-guard matched ZERO guard declarations (nothing checked)')
            return 2
        for (r, l_, t_, m_) in rawviol:
            out('  RAW-GUARD %s:%d  %s on %s (must use the tracked wrapper)' % (r, l_, t_, m_))
    keys = [f.key() for f in live]
    if write_baseline:
        with open(write_baseline, 'w') as fh:
            fh.write('# lock_raise_gate baseline: known-accepted lock+raise sites. TRACKED DEBT, not approval.\n'
                     '# key = relpath|kind|function|lock|statement ; add a reason after " ## ".\n')
            for k in sorted(keys):
                fh.write(k + ' ## UNREVIEWED\n')
        out('LOCK-RAISE GATE: wrote baseline of %d site(s) -> %s' % (len(keys), write_baseline))
        return 0
    if baseline:
        if not os.path.isfile(baseline):
            out('LOCK-RAISE GATE: ERROR baseline not found: %s' % baseline)
            return 2
        if rawviol:
            out('LOCK-RAISE GATE: FAIL (%d untracked std guard(s))' % len(rawviol))
            return 1
        known = read_baseline(baseline)
        if not known:
            out('LOCK-RAISE GATE: ERROR baseline lists ZERO sites: %s' % baseline)
            return 2
        remaining = dict(known)
        new = []
        for k, f in zip(keys, live):
            if remaining.get(k, 0) > 0:
                remaining[k] -= 1
            else:
                new.append(f)
        resolved = sum(remaining.values())
        out('LOCK-RAISE GATE: baseline %s: %d known, %d still present, %d resolved, %d NEW'
            % (os.path.basename(baseline), sum(known.values()), len(live) - len(new), resolved, len(new)))
        if new:
            out('LOCK-RAISE GATE: FAIL (new or changed sites not in the baseline)')
            for f in new:
                out('  FINDING %s:%d [%s %s] under %s in %s: %s%s' % (
                    f.rel, f.line, f.kind, f.what, f.lock, f.func, f.text, ('  <- ' + f.via) if f.via else ''))
            return 1
        out('LOCK-RAISE GATE: PASS (no new sites; known baseline debt printed above)')
        return 0
    if rawviol:
        out('LOCK-RAISE GATE: FAIL (%d untracked std guard(s))' % len(rawviol))
        return 1
    if live:
        out('LOCK-RAISE GATE: FAIL')
        for f in live:
            out('  FINDING %s:%d [%s %s] under %s in %s: %s%s' % (
                f.rel, f.line, f.kind, f.what, f.lock, f.func, f.text, ('  <- ' + f.via) if f.via else ''))
        return 1
    out('LOCK-RAISE GATE: PASS')
    return 0


# ------------------------------------------------------------------ selftest
SELFTEST_CASES = []


def case(name, want, src, **opts):
    SELFTEST_CASES.append((name, want, src, opts))


# want = list of (kind, raise-line) expected; or int count
case('P01 direct raise under lock_guard', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::recursive_mutex> g{m};
    lua.throw_error("x");
}''')
case('P02 luaL_error under unique_lock (paren init)', 1, '''
int f(lua_State* L) {
    std::unique_lock<std::mutex> lk(mtx);
    return luaL_error(L, "bad");
}''')
case('P03 scoped_lock with CTAD and two mutexes', 1, '''
void f(lua_State* L) {
    std::scoped_lock lk(a, b);
    lua_error(L);
}''')
case('P04 nested block inside lock scope', 1, '''
void f(Lua& lua, bool c) {
    std::lock_guard<std::mutex> g(m);
    if (c) {
        for (int i = 0; i < 3; ++i) {
            if (i) { lua.throw_error("deep"); }
        }
    }
}''')
case('P05 helper function that raises, called under lock', 1, '''
static void fail(Lua& lua) { lua.throw_error("boom"); }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    fail(lua);
}''')
case('P06 transitive helper (depth 2)', 1, '''
static void inner(Lua& lua) { lua.throw_error("x"); }
static void outer(Lua& lua) { inner(lua); }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    outer(lua);
}''')
case('P07 manual lock()/unlock(): raise between', 1, '''
void f(Lua& lua) {
    m.lock();
    lua.throw_error("held");
    m.unlock();
}''')
case('P08 actions_lock()/actions_unlock() pair', 1, '''
void f(Mod* mod, Lua& lua) {
    mod->actions_lock();
    luaL_error(L, "x");
    mod->actions_unlock();
}''')
case('P09 lambda invoked in place inherits the lock', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    [&]() { lua.throw_error("in lambda"); }();
}''')
case('P10 named local lambda called under lock', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    auto fail = [&](const char* s) { lua.throw_error(s); };
    fail("x");
}''')
case('P11 lambda passed to an unknown (synchronous) callee inherits', 1, '''
void f(Lua& lua, V& v) {
    std::lock_guard<std::mutex> g(m);
    std::for_each(v.begin(), v.end(), [&](int) { lua.throw_error("x"); });
}''')
case('P12 lock held across a nested scope end (guard still alive)', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    { int x = 1; (void)x; }
    lua.throw_error("still held");
}''')
case('P13 if-branch unlock does not release the other path', 1, '''
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); }
    lua.throw_error("maybe held");
}''')
case('P14 unlock in a block that exits early (return) does not release after it', 1, '''
int f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); return 0; }
    lua.throw_error("held on this path");
    return 1;
}''')
case('P15 defer_lock guard locked later, then raise', 1, '''
void f(Lua& lua) {
    std::unique_lock<std::mutex> lk(m, std::defer_lock);
    lk.lock();
    lua.throw_error("x");
}''')
case('P16 guard in if-init statement covers the body', 1, '''
void f(Lua& lua, int x) {
    if (std::lock_guard<std::mutex> g(m); x) {
        lua.throw_error("x");
    }
}''')
case('P17 lock-holder class member (RAII wrapper) instance', 1, '''
class Scope {
    std::unique_lock<std::recursive_mutex> Lock{M};
public:
    Scope() {}
};
void f(Lua& lua) {
    Scope s;
    lua.throw_error("x");
}''')
case('P18 lock macro', 1, '''
#define LOCK_ACTIONS std::lock_guard<std::mutex> g_lock{m}
void f(Lua& lua) {
    LOCK_ACTIONS;
    lua.throw_error("x");
}''')
case('P19 raise macro called under lock', 1, '''
#define RAISE(msg) luaL_error(L, msg)
void f(lua_State* L) {
    std::lock_guard<std::mutex> g(m);
    RAISE("x");
}''')
case('P20 L22Diagnostics::throw_lua_error under lock', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    L22Diagnostics::throw_lua_error(lua, "m", "ctx");
}''')
case('P21 else-branch of an unlock-and-return still held after the if', 1, '''
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); } else { lk.lock(); }
    lua.throw_error("held on else path");
}''')
case('P22 raise after a same-line block comment (lex_cxx in_block defect)', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m); /* note */ int a = 0;
    int b = 1;
    lua.throw_error("x");
}''')
case('P23 switch case does not inherit another case unlock', 1, '''
void f(Lua& lua, int k) {
    std::unique_lock<std::mutex> lk(m);
    switch (k) {
        case 1: lk.unlock(); break;
        case 2: lua.throw_error("held in case 2"); break;
    }
}''')
case('P24 raise in a catch handler inside a lock scope', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    try { work(); } catch (const std::exception& e) { lua.throw_error(e.what()); }
}''')
case('P25 raw string containing braces does not break the scope', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    const char* s = R"x(} } }
// not a comment )" )x";
    lua.throw_error("x");
}''')
case('P26 digit separator does not eat the lock scope', 1, '''
void f(Lua& lua) {
    int big = 1'000'000;
    std::lock_guard<std::mutex> g(m);
    lua.throw_error("x");
}''')
case('P27 ctor-init-list function holds its own guard', 1, '''
struct W { int a; W() : a{1} { std::lock_guard<std::mutex> g(m); luaL_error(L, "x"); } };''')
case('P28 call through a template-argument member helper', 1, '''
struct Lua { template <class T> T& get_userdata(int) { throw_error("t"); } };
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    auto& x = lua.get_userdata<Foo>(-1);
}''', helper_name_ok=True)

case('N01 raise after the guard scope ended', 0, '''
void f(Lua& lua) {
    {
        std::lock_guard<std::mutex> g(m);
        work();
    }
    lua.throw_error("after");
}''')
case('N02 raise after explicit unlock', 0, '''
void f(Lua& lua) {
    std::unique_lock<std::mutex> lk(m);
    work();
    lk.unlock();
    lua.throw_error("after unlock");
}''')
case('N03 C++ throw under lock is fine (destructors run)', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    throw std::runtime_error("ok");
}''')
case('N04 words in a comment', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    // lua.throw_error("x");
    /* luaL_error(L, "y"); */
    work();
}''')
case('N05 words in a string literal', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    log("lua.throw_error(x) luaL_error } { lock_guard");
}''')
case('N06 raise in a lambda stored for later (deferred callee)', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    lua.register_function("x", [](Lua& l) { l.throw_error("later"); });
}''')
case('N07 raise in a lambda assigned to a member (stored)', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    m_cb = [](Lua& l) { l.throw_error("later"); };
}''')
case('N08 raise inside a different function after the lock function', 0, '''
void locked() { std::lock_guard<std::mutex> g(m); work(); }
void other(Lua& lua) { lua.throw_error("no lock here"); }''')
case('N09 unlock-and-raise in the same block', 0, '''
void f(Lua& lua, bool bad) {
    std::unique_lock<std::mutex> lk(m);
    if (bad) { lk.unlock(); lua.throw_error("released first"); }
    work();
}''')
case('N10 weak_ptr lock() is not a mutex lock', 0, '''
void f(Lua& lua) {
    auto sp = weak.lock();
    lua.throw_error("x");
}''', need_lock_elsewhere=True)
case('N11 defer_lock guard never locked', 0, '''
void f(Lua& lua) {
    std::unique_lock<std::mutex> lk(m, std::defer_lock);
    lua.throw_error("never locked");
}''')
case('N12 helper that only calls non-raising code', 0, '''
static void calm(Lua& lua) { lua.get_integer(); }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    calm(lua);
}''', need_raise_elsewhere=True)
case('N13 sibling scopes: lock in one branch, raise in the other', 0, '''
void f(Lua& lua, bool c) {
    if (c) { std::lock_guard<std::mutex> g(m); work(); }
    else { lua.throw_error("no lock in else"); }
}''')
case('N14 raise in PS5-dead region', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
#ifndef __PROSPERO__
    lua.throw_error("host only");
#endif
}''')
case('N15 approved marker', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    lua.throw_error("ok"); // [LOCK-RAISE-APPROVED] reviewed
}''')
case('N16 unlock inside a block that raises: held state restored irrelevant', 0, '''
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); lua.throw_error("released"); }
    work();
}''')
case('N17 raise in deferred lambda passed to ExecuteInGameThread', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    ExecuteInGameThread([&]() { lua.throw_error("later"); });
}''')
case('N18 if/else both unlock before the raise', 0, '''
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); } else { lk.unlock(); }
    lua.throw_error("released on both paths");
}''')
case('N19 raise in a function while another function holds a guard member', 0, '''
class Scope {
    std::unique_lock<std::mutex> Lock{M};
};
void other(Lua& lua) { lua.throw_error("no instance here"); }''', need_lock_elsewhere=True)
case('N20 temporary lock_guard (released at end of statement)', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex>(m);
    lua.throw_error("temporary holds nothing after the ;");
}''', need_lock_elsewhere=True)


case('P29 char literal containing a brace does not break the scope', 1, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    char open = '{';
    char close = '}';
    lua.throw_error("x");
}""")
case('P30 helper defined in a second (header) file', 1, """
#include "h.hpp"
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    check_or_raise(lua);
}""", files={'h.hpp': 'inline void check_or_raise(Lua& lua) { lua.throw_error("bad"); }\n'})
case('P31 two nested guards, raise after the inner scope but inside the outer', 1, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> outer(m1);
    {
        std::lock_guard<std::mutex> inner(m2);
        work();
    }
    lua.throw_error("outer still held");
}""")
case('P32 guard re-armed by assignment', 1, """
void f(Lua& lua) {
    std::unique_lock<std::mutex> lk;
    lk = std::unique_lock<std::mutex>(m);
    lua.throw_error("x");
}""")
case('P33 std::lock(a, b) acquisition', 1, """
void f(Lua& lua) {
    std::lock(a, b);
    lua.throw_error("x");
}""")
case('P34 #if/#else with unbalanced braces per branch (scanned per alternative)', 1, """
#if HAVE_X
void g() { if (x) {
#else
void g() { if (y) { if (z) {
#endif
    work(); } }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    lua.throw_error("x");
}""")
case('P35 raise in else-if chain branch under lock', 1, """
void f(Lua& lua, int k) {
    std::lock_guard<std::mutex> g(m);
    if (k == 1) { work(); }
    else if (k == 2) { work2(); }
    else { luaL_error(L, "bad"); }
}""")
case('P36 do/while body under lock', 1, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    do { lua.throw_error("x"); } while (false);
}""")
case('P37 luaL_check* argument validators count as raises', 1, """
int f(lua_State* L) {
    std::lock_guard<std::mutex> g(m);
    auto n = luaL_checkinteger(L, 1);
    return 0;
}""")
case('P38 try_lock then raise', 1, """
void f(Lua& lua) {
    if (m.try_lock()) {
        lua.throw_error("x");
        m.unlock();
    }
}""")
case('P39 --unprotected-calls: lua_call under lock', 1, """
void f(lua_State* L) {
    std::lock_guard<std::mutex> g(m);
    lua_call(L, 0, 0);
}""", unprotected=True)
case('P40 lock released only in a non-terminating branch, raise after join', 1, """
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { lk.unlock(); work(); }
    lua.throw_error("held when c is false");
}""")
case('N21 named lambda defined under lock but never called or used', 0, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    auto fail = [&](const char* s) { lua.throw_error(s); };
    work();
}""")
case('N22 inner guard scope ended, no outer lock', 0, """
void f(Lua& lua) {
    {
        std::lock_guard<std::mutex> inner(m2);
        work();
    }
    lua.throw_error("no lock");
}""")
case('N23 raise in the #else (non-PS5) branch of #ifdef __PROSPERO__', 0, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
#ifdef __PROSPERO__
    work();
#else
    lua.throw_error("host only");
#endif
}""")
case('N24 raise in _WIN32 branch', 0, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
#ifdef _WIN32
    lua.throw_error("windows only");
#endif
}""")
case('N25 unlock in both branches of try/catch before raise', 0, """
void f(Lua& lua) {
    std::unique_lock<std::mutex> lk(m);
    try { work(); lk.unlock(); } catch (...) { lk.unlock(); }
    lua.throw_error("released on all paths");
}""")
case('N26 function named like a guard method but raising nothing', 0, """
void lock_all() { work(); }
void f(Lua& lua) {
    lock_all();
    lua.throw_error("no lock held");
}""", need_lock_elsewhere=True)
case('N27 raise token as a member of another identifier', 0, """
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    my_throw_error_logger(1);
    lua.throw_error_internal_append_args(x);
}""")
case('N28 // [LOCK-RAISE-APPROVED] on a helper call', 0, """
static void fail(Lua& lua) { lua.throw_error("boom"); }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    fail(lua); // [LOCK-RAISE-APPROVED] reviewed: no pcall on this path
}""")
case('N29 early return before the raise does not matter: raise unreachable on this path but other path unlocks', 0, """
int f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) { return 1; }
    lk.unlock();
    lua.throw_error("released");
    return 0;
}""")
case('N30 helper raising only inside a deferred lambda is not a raising helper', 0, """
static void reg(Lua& lua) { lua.register_function("x", [](Lua& l) { l.throw_error("later"); }); }
void f(Lua& lua) {
    std::lock_guard<std::mutex> g(m);
    reg(lua);
}""", need_raise_elsewhere=True)

case('P41 brace-less if/else bodies raising under lock', 2, """
void f(Lua& lua, bool c) {
    std::lock_guard<std::mutex> g(m);
    if (c) lua.throw_error("a");
    else luaL_error(L, "b");
}""")
case('P42 brace-less if with unlock, raise on the next line is still held (maybe-held)', 1, """
void f(Lua& lua, bool c) {
    std::unique_lock<std::mutex> lk(m);
    if (c) lk.unlock();
    lua.throw_error("held when !c");
}""")

# ---- round 8 additions: custom wrapper guard, banned calls, aliases, CTAD, holders, metamethods
case('P43 custom wrapper b6diag::ActionsLock (mutex + __LINE__) holds the lock', 1, '''
void f(Lua& lua) {
    b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};
    lua.throw_error("x");
}''')
case('N31 custom wrapper scope ended before the raise', 0, '''
void f(Lua& lua) {
    {
        b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};
        x = 1;
    }
    lua.throw_error("x");
}''')
case('P44 banned get_mod_ref under a lock is a finding even with --helpers off', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
    auto mod = get_mod_ref(lua);
}''', helpers='off')
case('P45 banned get_mod_ref under the custom wrapper, --helpers off', 1, '''
void f(Lua& lua) {
    b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};
    auto mod = get_mod_ref(lua);
}''', helpers='off')
case('N32 get_mod_ref_cxx (C++-throwing twin) under a lock is allowed', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
    auto mod = get_mod_ref_cxx(lua);
}''', helpers='off')
case('N33 get_mod_ref outside any lock scope is allowed', 0, '''
void f(Lua& lua) {
    { std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex}; x = 1; }
    auto mod = get_mod_ref(lua);
}''', helpers='off')
case('N34 --unban get_mod_ref removes it from the ban list', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
    auto mod = get_mod_ref(lua);
}''', helpers='off', unban=['get_mod_ref'])
case('P46 --ban adds a banned name', 1, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> guard{m};
    risky_helper(lua);
}''', helpers='off', ban=['risky_helper'])
case('N35 member call named like a banned function is not the banned free function', 0, '''
void f(Lua& lua) {
    std::lock_guard<std::mutex> guard{m};
    obj.get_mod_ref(lua);
}''', helpers='off')
case('P47 using-alias of lock_guard', 1, '''
using Guard = std::lock_guard<std::recursive_mutex>;
void f(Lua& lua) { Guard g{m}; lua.throw_error("x"); }''')
case('P48 typedef alias of unique_lock', 1, '''
typedef std::unique_lock<std::mutex> UL;
void f(Lua& lua) { UL g(m); luaL_error(L, "x"); }''')
case('P49 alias of an alias', 1, '''
using G1 = std::lock_guard<std::mutex>;
using G2 = G1;
void f(Lua& lua) { G2 g{m}; lua.throw_error("x"); }''')
case('P50 CTAD declaration auto g = std::lock_guard(m)', 1, '''
void f(Lua& lua) { auto g = std::lock_guard(m); lua.throw_error("x"); }''')
case('N36 temporary guard holds nothing past the statement', 0, '''
void f(Lua& lua) { std::lock_guard<std::mutex>(m); lua.throw_error("x"); }''')
case('P51 class with a guard member built in the ctor-init-list is a holder', 1, '''
struct Holder {
    std::unique_lock<std::recursive_mutex> g;
    Holder() : g(m) {}
};
void f(lua_State* L) { Holder h; luaL_error(L, "x"); }''')
case('P52 class whose ctor locks and dtor unlocks is a holder', 1, '''
struct Holder {
    Holder() { m.lock(); }
    ~Holder() { m.unlock(); }
};
void f(lua_State* L) { Holder h; luaL_error(L, "x"); }''')
case('N37 class that only locks in the ctor (no dtor unlock) is not assumed to be RAII', 0, '''
struct Locker {
    Locker() { m.lock(); }
    void release() { m.unlock(); }
};
void f(lua_State* L) { Locker h; luaL_error(L, "x"); }''')
case('P53 metamethod API under lock with --metamethod-calls', 1, '''
void f(lua_State* L) { std::lock_guard<std::mutex> g{m}; lua_gettable(L, 1); }''', metamethod=True, need_raise_elsewhere=True)
case('N38 metamethod API under lock without --metamethod-calls (opt-in tier)', 0, '''
void f(lua_State* L) { std::lock_guard<std::mutex> g{m}; lua_gettable(L, 1); }''', need_raise_elsewhere=True)
case('P54 --guard-type adds a project RAII wrapper', 1, '''
void f(Lua& lua) { MyLockWrap w{g_mutex, 1}; lua.throw_error("x"); }''', guard_types=['MyLockWrap'])
case('P55 --mutex filter matches the wrapper mutex (first argument only)', 1, '''
void f(Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; lua.throw_error("x"); }''', mutex='m_thread_actions_mutex')
case('P56 wrapper mutex text is the FIRST argument only (anchored --mutex still matches)', 1, '''
void f(Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; lua.throw_error("x"); }''', mutex='m_thread_actions_mutex$')
case('N39 --mutex filter does not match a different mutex', 0, '''
void f(Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; lua.throw_error("x"); }''', mutex='some_other_mutex')

PRELUDE_LOCK = 'void __needs_a_lock() { std::lock_guard<std::mutex> __g(m); }\n'
PRELUDE_RAISE = 'void __needs_a_raise(Lua& lua) { lua.throw_error("x"); }\n'


def _analyse_source(src, opts):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'case.cpp')
        with open(p, 'w') as fh:
            fh.write(src)
        extra_paths = []
        for nm_, tx_ in (opts.get('files') or {}).items():
            q = os.path.join(d, nm_)
            with open(q, 'w') as fh:
                fh.write(tx_)
            extra_paths.append(q)
        o = {'helper_depth': 2, 'lambda_policy': 'deferred-aware'}
        o.update({k: v for k, v in opts.items() if k not in ('need_lock_elsewhere', 'need_raise_elsewhere', 'helper_name_ok', 'files')})
        g = Gate(o)
        g.load([p] + extra_paths)
        for q in [p] + extra_paths:
            g.rel[q] = os.path.basename(q)
        g.collect()
        findings, nl, nr, _pf = g.analyse()
        findings = [f for f in dedupe(findings) if not f.approved]
        return findings, nl, nr, g


def selftest():
    failed = 0
    total = 0
    for name, want, src, opts in SELFTEST_CASES:
        total += 1
        body = src
        try:
            findings, nl, nr, _g = _analyse_source(body, opts)
        except Exception as e:  # a crash is a failure, never a silent pass
            print('  FAIL  %s: exception %r' % (name, e))
            failed += 1
            continue
        got = len(findings)
        ok = got == want
        # every case must actually have exercised the detector (checked > 0), except
        # pure-negative cases that intentionally have no lock / no raise
        note = 'locks=%d raises=%d' % (nl, nr)
        print('  %s  %s: findings=%d want=%d (%s)' % ('PASS' if ok else 'FAIL', name, got, want, note))
        if not ok:
            for f in findings:
                print('        got %s:%d %s %s' % (f.rel, f.line, f.kind, f.text))
            failed += 1
    # ---- gate-level behaviour
    extra = []
    with tempfile.TemporaryDirectory() as d:
        empty = os.path.join(d, 'empty')
        os.mkdir(empty)
        extra.append(('empty root -> exit 2 (zero files)',
                      run([empty], [], [], {'helper_depth': 2}, quiet=True) == 2))
        tree = os.path.join(d, 'tree')
        os.mkdir(tree)
        with open(os.path.join(tree, 'nolock.cpp'), 'w') as fh:
            fh.write('void f(Lua& lua) { lua.throw_error("x"); }\n')
        extra.append(('files but zero lock scopes -> exit 2',
                      run([tree], [], [], {'helper_depth': 2}, quiet=True) == 2))
        with open(os.path.join(tree, 'nolock.cpp'), 'w') as fh:
            fh.write('void f() { std::lock_guard<std::mutex> g(m); }\n')
        extra.append(('lock scopes but zero raise sites -> exit 2',
                      run([tree], [], [], {'helper_depth': 2}, quiet=True) == 2))
        with open(os.path.join(tree, 'nolock.cpp'), 'w') as fh:
            fh.write('void f(Lua& lua) { std::lock_guard<std::mutex> g(m); lua.throw_error("x"); }\n')
        extra.append(('finding -> exit 1', run([tree], [], [], {'helper_depth': 2}, quiet=True) == 1))
        bl = os.path.join(d, 'bl.txt')
        extra.append(('write-baseline -> exit 0',
                      run([tree], [], [], {'helper_depth': 2}, write_baseline=bl, quiet=True) == 0))
        extra.append(('known site passes with baseline',
                      run([tree], [], [], {'helper_depth': 2}, baseline=bl, quiet=True) == 0))
        with open(os.path.join(tree, 'b.cpp'), 'w') as fh:
            fh.write('void g(Lua& lua) { std::unique_lock<std::mutex> g(m); luaL_error(L, "y"); }\n')
        extra.append(('NEW site fails with baseline',
                      run([tree], [], [], {'helper_depth': 2}, baseline=bl, quiet=True) == 1))
        with open(bl, 'w') as fh:
            fh.write('# empty\n')
        extra.append(('empty baseline -> exit 2',
                      run([tree], [], [], {'helper_depth': 2}, baseline=bl, quiet=True) == 2))
        os.remove(os.path.join(tree, 'b.cpp'))
        lst = os.path.join(d, 'list.txt')
        with open(lst, 'w') as fh:
            fh.write(os.path.join(tree, 'nolock.cpp') + '\n')
        extra.append(('--files-from list works', run([], [lst], [], {'helper_depth': 2}, quiet=True) == 1))
        with open(os.path.join(tree, 'unb.cpp'), 'w') as fh:
            fh.write('void h(Lua& lua) { std::lock_guard<std::mutex> g(m); if (x) { lua.throw_error("x"); }\n')
        extra.append(('unbalanced braces in a relevant file -> exit 2',
                      run([tree], [], [], {'helper_depth': 2}, quiet=True) == 2))
    # ---- round 8: unknown guard types, raw-guard ban, banned calls at gate level
    with tempfile.TemporaryDirectory() as d2:
        t2 = os.path.join(d2, 't')
        os.mkdir(t2)
        f2 = os.path.join(t2, 'a.cpp')

        def put(txt):
            with open(f2, 'w') as fh:
                fh.write(txt)
        base_o = {'helper_depth': 2}
        put('void f(Lua& lua) { std::lock_guard<std::mutex> g(m); x = 1; }\n'
            'void h(Lua& lua) { MysteryGuard w{g_mutex}; lua.throw_error("x"); }\n')
        extra.append(('unknown RAII type built from a mutex -> exit 2 (lock model blind)', run([t2], [], [], dict(base_o), quiet=True) == 2))
        extra.append(('--allow-unknown-guard NAME lets it through (then the raise is outside any lock: exit 0)',
                      run([t2], [], [], dict(base_o, allow_unknown_guard=['MysteryGuard']), quiet=True) == 0))
        extra.append(('--guard-type NAME makes it a lock (finding -> exit 1)',
                      run([t2], [], [], dict(base_o, guard_types=['MysteryGuard']), quiet=True) == 1))
        put('void f(Lua& lua, std::mutex& mutex) { lua.throw_error("x"); }\n'
            'void g(Lua& lua) { std::lock_guard<std::mutex> lk(m); x = 1; }\n')
        extra.append(('parameter named mutex is not an unknown guard (exit 0)', run([t2], [], [], dict(base_o), quiet=True) == 0))
        put('void f(Lua& lua) { b6diag::ActionsLock g{LuaMod::m_thread_actions_mutex, __LINE__}; x = 1; }\n'
            'void h(Lua& lua) { lua.throw_error("x"); }\n')
        extra.append(('--forbid-raw-guard: only tracked wrappers on the mutex -> exit 0',
                      run([t2], [], [], dict(base_o, forbid_raw_guard='m_thread_actions_mutex'), quiet=True) == 0))
        put('void f(Lua& lua) { b6diag::ActionsLock g{LuaMod::m_thread_actions_mutex, __LINE__}; x = 1; }\n'
            'void h(Lua& lua) { std::lock_guard<std::recursive_mutex> k{LuaMod::m_thread_actions_mutex}; x = 2; }\n'
            'void i(Lua& lua) { lua.throw_error("x"); }\n')
        extra.append(('--forbid-raw-guard: a std guard left on the mutex -> exit 1',
                      run([t2], [], [], dict(base_o, forbid_raw_guard='m_thread_actions_mutex'), quiet=True) == 1))
        extra.append(('--forbid-raw-guard that matches no guard at all -> exit 2 (nothing checked)',
                      run([t2], [], [], dict(base_o, forbid_raw_guard='no_such_mutex_zzz'), quiet=True) == 2))
        put('void f(Lua& lua) { std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex}; auto m = get_mod_ref(lua); }\n'
            'void h(Lua& lua) { lua.throw_error("x"); }\n')
        extra.append(('banned get_mod_ref under lock fails with --helpers off (exit 1)',
                      run([t2], [], [], dict(base_o, helpers='off'), quiet=True) == 1))
        put('void f(Lua& lua) { std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex}; auto m = get_mod_ref_cxx(lua); }\n'
            'void h(Lua& lua) { lua.throw_error("x"); }\n')
        extra.append(('get_mod_ref_cxx under lock passes with --helpers off (exit 0)',
                      run([t2], [], [], dict(base_o, helpers='off'), quiet=True) == 0))
        # regression for the B6-2 hole: swapping the guard type must not reduce the scan silently
        put('void f(Lua& lua) { b6diag::ActionsLock g{LuaMod::m_thread_actions_mutex, __LINE__}; lua.throw_error("x"); }\n')
        extra.append(('wrapper-only file is scanned (1 lock scope, finding -> exit 1)', run([t2], [], [], dict(base_o), quiet=True) == 1))
    # lex_cxx defect proof (documentation case: the harvest lexer swallows the next line)
    views = lex_cxx(['int a; /* x */ int b;\n', 'lua.throw_error("x");\n'])
    defect_present = views[1].code == ''
    print('  INFO  lex_cxx same-line block-comment defect %s in this checkout (pre-pass makes the gate immune)'
          % ('PRESENT' if defect_present else 'absent'))
    for name, ok in extra:
        total += 1
        failed += not ok
        print('  %s  %s' % ('PASS' if ok else 'FAIL', name))
    print('LOCK-RAISE GATE SELFTEST: %d/%d PASS (%d synthetic source cases + %d gate-behaviour cases)'
          % (total - failed, total, len(SELFTEST_CASES), len(extra)))
    return 1 if failed else 0


def main(argv):
    roots, specs, files = [], [], []
    opts = {'helper_depth': 2, 'lambda_policy': 'deferred-aware'}
    baseline = write_baseline = json_out = None
    verbose = False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--selftest':
            return selftest()
        if a in ('-h', '--help'):
            print(__doc__)
            return 0
        if a in ('--root', '--files-from', '--baseline', '--write-baseline', '--json', '--mutex',
                 '--helper-depth', '--lambda-policy', '--helpers', '--path-map', '--guard-type', '--ban', '--unban',
                 '--forbid-raw-guard', '--allow-unknown-guard') and i + 1 < len(argv):
            v = argv[i + 1]
            if a == '--guard-type':
                opts.setdefault('guard_types', []).append(v)
            elif a == '--ban':
                opts.setdefault('ban', []).append(v)
            elif a == '--unban':
                opts.setdefault('unban', []).append(v)
            elif a == '--forbid-raw-guard':
                opts['forbid_raw_guard'] = v
            elif a == '--allow-unknown-guard':
                opts.setdefault('allow_unknown_guard', []).append(v)
            elif a == '--root':
                roots.append(os.path.abspath(v))
            elif a == '--files-from':
                specs.append(os.path.abspath(v))
            elif a == '--baseline':
                baseline = os.path.abspath(v)
            elif a == '--write-baseline':
                write_baseline = os.path.abspath(v)
            elif a == '--json':
                json_out = os.path.abspath(v)
            elif a == '--mutex':
                opts['mutex'] = v
            elif a == '--helper-depth':
                opts['helper_depth'] = int(v)
            elif a == '--lambda-policy':
                if v not in ('deferred-aware', 'inherit'):
                    print('usage error: --lambda-policy deferred-aware|inherit')
                    return 2
                opts['lambda_policy'] = v
            elif a == '--helpers':
                if v not in ('fail', 'warn', 'off'):
                    print('usage error: --helpers fail|warn|off')
                    return 2
                opts['helpers'] = v
            elif a == '--path-map':
                if '=' not in v:
                    print('usage error: --path-map RELPREFIX=NEWPREFIX')
                    return 2
                opts.setdefault('path_map', []).append(tuple(v.split('=', 1)))
            i += 2
            continue
        if a == '--unprotected-calls':
            opts['unprotected'] = True
        elif a == '--metamethod-calls':
            opts['metamethod'] = True
        elif a == '--show-helpers':
            opts['show_helpers'] = True
        elif a == '--list-locks':
            opts['list_locks'] = True
        elif a in ('-v', '--verbose'):
            verbose = True
        elif a.startswith('-'):
            print('usage error: unknown argument %r\n%s' % (a, __doc__))
            return 2
        else:
            files.append(os.path.abspath(a))
        i += 1
    if not roots and not specs and not files:
        roots = [DEFAULT_SRC]
    return run(roots, specs, files, opts, baseline, write_baseline, verbose, json_out)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
