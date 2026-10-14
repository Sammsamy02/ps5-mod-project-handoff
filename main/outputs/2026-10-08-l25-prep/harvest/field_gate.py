#!/usr/bin/env python3
"""field_gate.py (round 10, FIELDS track) — pre-compile gate: every engine member field the
runtime reads must exist in this game's PROVEN layout data, i.e. the compiled [NativeLayout]
member declaration the PS5 runtime actually stages (UE4SSProgram::load_unreal_offsets_from_file
uses the compiled declaration and never reads ue4ss/MemberVariableLayout.ini when it exists).
Launch 8 (2026-10-12) failed because new code read UClass::ClassFlags and
FUObjectItem::SerialNumber, which the layout did not declare.

Changes against the 2026-10-12 gate (harvest/field_gate.py):
  * NO baseline acceptance by default. The B4-snapshot "accepted debt" is opt-in only
    (--baseline-snapshot PATH) and is reported as debt, never silently.
  * FIELD-GATE-OK on a line is accepted only with a reason: `FIELD-GATE-OK: <reason>` (>= 12
    characters). A bare marker is reported and does NOT accept the finding.
  * Proven set = the compiled MEMBER declaration only (the first string literal). The vtable
    literal is no longer mixed in; the console ini is opt-in (--ini PATH) because the PS5 runtime
    does not read it while a compiled declaration exists.
  * Version-dead code is pruned before analysis: if/else-if/else chains whose conditions are
    pure Version::IsAtLeast/IsAtMost/IsBelow/IsAbove/IsEqual/IsDebug predicates are evaluated
    for the declaration's EngineMajor.EngineMinor (IsDebug = false when NonShipping=0); untaken
    branch bodies are blanked. `if (<true-for-target>) { return ...; }` blanks the rest of its
    block. Only fully decidable conditions are pruned; anything else stays (conservative).
  * Class-aware resolution: an accessor/helper name defined by several classes (GetChildren,
    GetOwner, GetSize, GetFlags, GetEnum, GetWorld, ...) is resolved through the receiver's class
    (and its bases) when the receiver type is evident: `this`/unqualified call inside a C:: or
    in-class body, `X::Name(`, static_cast/reinterpret_cast/bit_cast/Cast<T>, or the nearest
    declaration `T* v` / `T& v` / `auto v = static_cast<T*>(...)` of the receiver variable.
    Unknown receivers keep the previous conservative union.
  * Comment/string stripping keeps every newline (raw strings, multi-line literals and digit
    separators no longer shift line numbers).
  * `S::MemberOffsets` iteration inside a #define whose parameter is S (OUTPUT_MEMBER_OFFSETS_*,
    SHIFT_MEMBER_OFFSETS) is map iteration, not a field read; it is counted and listed.

Prints files, calls, members, pruned branches and every acceptance with its reason.
Exit 1 on any unproven, unaccepted member read; exit 2 when anything required is missing or
when zero files / zero calls / zero proven members / zero accessors were checked.
"""
import argparse, bisect, os, re, shutil, sys, tarfile, tempfile, json

W = '/WORKSPACE/.codex/worktrees/translation-core/what'
PORT = W + '/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port'
CAND = W + '/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
DECL = W + '/outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp'

SRC_EXT = ('.cpp', '.hpp', '.h', '.inl', '.inc')
MIN_REASON = 12


# ----------------------------------------------------------------------------- text handling
def strip_comments_strings(text):
    """Blank comments and string/char literals; every newline is kept so line numbers match."""
    out = []
    i, n = 0, len(text)

    def blank(s):
        return re.sub(r'[^\n]', ' ', s)

    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i)); i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(blank(text[i:j])); i = j
        elif c == '"' and i > 0 and text[i - 1] == 'R' and (i < 2 or not (text[i - 2].isalnum() or text[i - 2] == '_') or text[i - 3:i - 1] == 'u8' or text[i - 2] in 'LuU'):
            k = text.find('(', i + 1)
            delim = text[i + 1:k] if k > 0 else ''
            end = text.find(')' + delim + '"', k + 1) if k > 0 else -1
            j = n if end < 0 else end + len(delim) + 2
            out.append('"' + blank(text[i + 1:j - 1]) + ('"' if j <= n and end >= 0 else '')); i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"' and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append('"' + blank(text[i + 1:j]) + ('"' if j < n and text[j] == '"' else ''))
            i = j + 1 if j < n and text[j] == '"' else j
        elif c == "'":
            # digit separator (1'000'000, 0xFFFF'FFFF): not a literal
            if i > 0 and (text[i - 1].isalnum()) and i + 1 < n and text[i + 1].isalnum():
                out.append(' '); i += 1; continue
            j = i + 1
            while j < n and text[j] != "'" and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append("'" + blank(text[i + 1:j]) + ("'" if j < n and text[j] == "'" else ''))
            i = j + 1 if j < n and text[j] == "'" else j
        else:
            out.append(c); i += 1
    # Line continuations (backslash-newline, e.g. multi-line #define bodies) are joined into a
    # plain newline, so `if (...) \<newline>{` sees its brace body. Same length, same line numbers.
    return ''.join(out).replace(chr(92) + chr(10), ' ' + chr(10))


class GateIntegrityError(Exception):
    """Version pruning would blank text it cannot account for: the gate stops (exit 2)."""


def _balanced(s):
    """True when s has no unmatched '{'/'}' or '('/')' (depth never negative, ends at 0)."""
    for o, c in (('{', '}'), ('(', ')')):
        d = 0
        for ch in s:
            if ch == o:
                d += 1
            elif ch == c:
                d -= 1
                if d < 0:
                    return False
        if d != 0:
            return False
    return True


def match_close(text, i, open_c, close_c):
    """text[i] == open_c; return index of the matching close (or len)."""
    depth = 0
    for j in range(i, len(text)):
        ch = text[j]
        if ch == open_c:
            depth += 1
        elif ch == close_c:
            depth -= 1
            if depth == 0:
                return j
    return len(text)


# ----------------------------------------------------------------------------- version pruning
VPRED = re.compile(r'(?:\b(?:RC::)?(?:Unreal::)?Version::)(IsAtLeast|IsAtMost|IsBelow|IsAbove|IsEqual)\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)|(?:\b(?:RC::)?(?:Unreal::)?Version::)IsDebug\s*\(\s*\)')


# Target constants: build-composition facts the gate verifies from source before using them.
# Each entry: atom regex -> (value, [(file, exact text that must be present)], description).
TARGET_CONST_SPECS = [
    (r'(?:\b(?:RC::)?(?:Unreal::)?FUObjectHashTables::)IsAvailable\s*\(\s*\)', False,
     [(W + '/.superpowers/native-dependency-spike/ue4ss-native-hash-tables-1/source/NativeHashTablesUnavailablePolicy.hpp', 'inline bool IsAvailable() { return false; }'),
      (W + '/.superpowers/native-dependency-spike/ue4ss-native-hash-tables-1/source/UObjectHashTablesNativeUnavailable.cpp', 'bool FUObjectHashTables::IsAvailable() { return HashTablesNativePolicy::IsAvailable(); }')],
     'FUObjectHashTables::IsAvailable() is the PS5 unavailable policy (returns false)'),
    (r'(?:\b(?:RC::)?(?:Unreal::)?(?:UnrealInitializer::)?)ShouldUseHashTables\s*\(\s*\)', False,
     [('{ROOT}/src/UnrealInitializer.cpp', 'return !StaticStorage::GlobalConfig.bForceGUObjectArrayForIteration && FUObjectHashTables::IsAvailable();')],
     'UnrealInitializer::ShouldUseHashTables() = !flag && FUObjectHashTables::IsAvailable() (false)'),
]
# unqualified IsAvailable() is the hash-table one only inside the hash-table sources
FILE_CONST_SPECS = [(r'UObjectHashTables[^/]*\.(?:cpp|hpp)$', r'(?<![:\w.>])IsAvailable\s*\(\s*\)', False)]
ACTIVE_CONSTS = []          # filled by main(): [(compiled regex, value)]
ACTIVE_FILE_CONSTS = []
NO_FILTER = False


def _split_top(expr, op):
    parts, depth, cur, i = [], 0, '', 0
    while i < len(expr):
        ch = expr[i]
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if depth == 0 and expr.startswith(op, i):
            parts.append(cur); cur = ''; i += len(op); continue
        cur += ch; i += 1
    parts.append(cur)
    return parts


def tv(expr, atoms):
    """Kleene three-valued evaluation: True/False when decidable, None otherwise."""
    e = expr.strip()
    while e.startswith('(') and match_close(e, 0, '(', ')') == len(e) - 1:
        e = e[1:-1].strip()
    if not e:
        return None
    depth = 0
    for ch in e:
        depth += ch in '(['
        depth -= ch in ')]'
        if depth == 0 and ch == '?':
            return None
    ors = _split_top(e, '||')
    if len(ors) > 1:
        vals = [tv(x, atoms) for x in ors]
        return True if True in vals else (None if None in vals else False)
    ands = _split_top(e, '&&')
    if len(ands) > 1:
        vals = [tv(x, atoms) for x in ands]
        return False if False in vals else (None if None in vals else True)
    if e.startswith('!') and not e.startswith('!='):
        v = tv(e[1:], atoms)
        return None if v is None else (not v)
    for rx, val in atoms:
        if rx.fullmatch(e):
            return val
    return None


def eval_version(cond, target, debug, file_consts=()):
    def vval(m):
        if m.group(1) is None:
            return debug
        a, b = (int(m.group(2)), int(m.group(3)))
        M, m_ = target
        return {'IsAtLeast': (M, m_) >= (a, b), 'IsAtMost': (M, m_) <= (a, b), 'IsBelow': (M, m_) < (a, b),
                'IsAbove': (M, m_) > (a, b), 'IsEqual': (M, m_) == (a, b)}[m.group(1)]
    if not VPRED.search(cond) and not any(rx.search(cond) for rx, _ in ACTIVE_CONSTS) and not any(rx.search(cond) for rx, _ in file_consts):
        return None
    atoms = []
    for m in VPRED.finditer(cond):
        atoms.append((re.compile(re.escape(m.group(0))), vval(m)))
    atoms.extend(ACTIVE_CONSTS)
    atoms.extend(file_consts)
    return tv(cond, [(rx, v) for rx, v in atoms if v is not None])


def _stmt_span(text, i):
    """Statement/body starting at or after i: returns (start, end_exclusive)."""
    n = len(text)
    while i < n and text[i].isspace():
        i += 1
    if i < n and text[i] == '{':
        j = match_close(text, i, '{', '}')
        return i, j + 1
    # single statement: up to ';' at paren/brace depth 0 (an inner if/else chain is rare here)
    depth = 0
    j = i
    while j < n:
        ch = text[j]
        if ch in '({[':
            depth += 1
        elif ch in ')}]':
            depth -= 1
        elif ch == ';' and depth <= 0:
            return i, j + 1
        j += 1
    return i, n


def prune_versions(text, target, debug, stats, file_consts=(), problems=None):
    chars = list(text)

    def blank(a, b):
        # self-check: a pruned span must be one whole, balanced statement or block inside its
        # enclosing block; a span that runs to the end of the text or crosses a block edge is a
        # parser miss (e.g. an unseen brace body) and must never silently blank code.
        if problems is not None and (b >= len(text) or not _balanced(text[a:b])):
            problems.append((text.count('\n', 0, a) + 1, text.count('\n', 0, min(b, len(text))) + 1))
            return
        for k in range(a, min(b, len(chars))):
            if chars[k] != '\n':
                chars[k] = ' '

    pos = 0
    for m in re.finditer(r'\bif\s*\(', text):
        s = m.start()
        if s < pos and chars[s] == ' ':
            continue
        pre = text[max(0, s - 12):s]
        if re.search(r'\belse\s*$', pre):
            continue                                       # handled as part of its chain
        if chars[s] == ' ':
            continue                                       # already blanked
        chain = []                                         # (cond, body_start, body_end)
        else_body = None
        k = m.end() - 1
        while True:
            ce = match_close(text, k, '(', ')')
            cond = text[k + 1:ce]
            bs, be = _stmt_span(text, ce + 1)
            chain.append((cond, bs, be))
            t = be
            mm = re.match(r'\s*else\b', text[t:])
            if not mm:
                break
            t += mm.end()
            mi = re.match(r'\s*if\s*\(', text[t:])
            if mi:
                k = t + mi.end() - 1
                continue
            else_body = _stmt_span(text, t)
            break
        taken = None
        for idx, (cond, bs, be) in enumerate(chain):
            v = eval_version(cond, target, debug, file_consts)
            if v is None:
                break
            if v is False:
                blank(bs, be); stats['pruned_branches'] += 1
                if idx == len(chain) - 1 and else_body is None:
                    taken = 'none'
                continue
            taken = idx
            for (_, bs2, be2) in chain[idx + 1:]:
                blank(bs2, be2); stats['pruned_branches'] += 1
            if else_body:
                blank(*else_body); stats['pruned_branches'] += 1
            # guard-return: `if (true-for-target) { return ...; }` with no else -> rest of block dead
            body = ''.join(chars[bs:be]).strip()
            straight = re.fullmatch(r'\{[^{}]*\breturn\b[^;{}]*;\s*\}|return\b[^;{}]*;', body) and not re.search(r'\b(if|for|while|switch|goto|break|continue|case)\b', body)
            if len(chain) == 1 and else_body is None and straight:
                depth = 0
                j = be
                while j < len(text):
                    ch = text[j]
                    if ch == '{':
                        depth += 1
                    elif ch == '}':
                        if depth == 0:
                            break
                        depth -= 1
                    j += 1
                if problems is not None and j >= len(text):
                    problems.append((text.count('\n', 0, be) + 1, text.count('\n') + 1))
                else:
                    blank(be, j); stats['guard_returns'] += 1
            break
        pos = chain[-1][2]
    return ''.join(chars)


# ----------------------------------------------------------------------------- parsing helpers
SIG_TAIL = re.compile(r'((?:[A-Za-z_][A-Za-z0-9_]*\s*::\s*)*)([A-Za-z_~][A-Za-z0-9_]*)\s*\([^(){};]*(?:\([^(){};]*\)[^(){};]*)*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:->\s*[^{;()]*)?\s*$')
CLASS_HEAD = re.compile(r'\b(?:class|struct)\s+(?:alignas\s*\([^)]*\)\s*)?(?:[A-Z][A-Z0-9_]*_API\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::\s*([^{;()]*))?$')
KEYWORDS = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'static_assert', 'alignas', 'requires', 'TRY', 'else', 'do'}


def scopes(text):
    """Yield (kind, name, cls, start, end): kind 'fn' bodies with their class (qualifier or
    enclosing class/struct), kind 'class' for class bodies (used for hierarchy)."""
    stack = []
    for i, c in enumerate(text):
        if c == '{':
            tail = text[max(0, i - 400):i]
            cm = CLASS_HEAD.search(tail)
            if cm and not SIG_TAIL.search(tail):
                stack.append(('class', cm.group(1), cm.group(2), i + 1))
                continue
            m = SIG_TAIL.search(tail)
            if m and m.group(2) not in KEYWORDS:
                qual = [q.strip() for q in m.group(1).split('::') if q.strip()]
                cls = qual[-1] if qual else None
                if cls is None:
                    for fr in reversed(stack):
                        if fr[0] == 'class':
                            cls = fr[1]; break
                        if fr[0] == 'fn':
                            cls = fr[2]; break
                stack.append(('fn', m.group(2), cls, i + 1))
            else:
                stack.append(('blk', None, None, i + 1))
        elif c == '}' and stack:
            kind, name, extra, start = stack.pop()
            if kind == 'fn':
                yield 'fn', name, extra, start, i
            elif kind == 'class':
                yield 'class', name, extra, start, i


def bases_of(base_text):
    """Direct bases in declaration order; for a templated base (TProperty<T, FProperty>) the
    template itself comes first, then its identifier arguments (the port's property templates
    take the real base class as an argument)."""
    out, args = [], []
    text = base_text or ''
    depth, cur, parts = 0, '', []
    for ch in text:
        if ch == '<':
            depth += 1
        elif ch == '>':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(cur); cur = ''
        else:
            cur += ch
    parts.append(cur)
    for b in parts:
        targs = re.findall(r'<(.*)>', b, re.S)
        b = re.sub(r'<.*', '', b, flags=re.S)
        b = re.sub(r'\b(public|protected|private|virtual)\b', '', b).strip()
        if b:
            out.append(b.split('::')[-1].strip())
        for t in targs:
            args.extend(x.split('::')[-1] for x in re.findall(r'[A-Za-z_][A-Za-z0-9_:]*', t))
    return out + [a for a in args if a not in out]


CAST_TAIL = re.compile(r'(?:static_cast|reinterpret_cast|const_cast|dynamic_cast|bit_cast|Cast|CastField|ExactCast)\s*<\s*(?:const\s+)?(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)\s*\*?\s*>\s*$')
VAR_TAIL = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$')


def receiver_class(text, pos, floor, own_cls, classes):
    """Class of the receiver expression that ends right before text[pos] ('->' or '.')."""
    head = text[max(floor, pos - 600):pos].rstrip()
    if head.endswith(')'):
        # cast expression: find the matching '(' and look for a cast template just before it
        depth = 0
        for j in range(len(head) - 1, -1, -1):
            if head[j] == ')':
                depth += 1
            elif head[j] == '(':
                depth -= 1
                if depth == 0:
                    cm = CAST_TAIL.search(head[:j])
                    if cm and cm.group(1) in classes:
                        return cm.group(1)
                    return None
        return None
    vm = VAR_TAIL.search(head)
    if not vm:
        return None
    var = vm.group(1)
    if var == 'this':
        return own_cls
    region = text[max(0, floor - 400):pos - len(head) + vm.start()]
    best = None
    pats = [
        re.compile(r'\b(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^<>;{}]*>)?\s*(?:\*|&)+\s*(?:const\s+)?' + re.escape(var) + r'\s*(?:[=;,){\[]|\s)'),
        re.compile(r'\bauto\s*[*&]*\s*' + re.escape(var) + r'\s*=\s*(?:std::)?(?:static_cast|reinterpret_cast|bit_cast|Cast|CastField|ExactCast)\s*<\s*(?:const\s+)?(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)'),
    ]
    for p in pats:
        for m in p.finditer(region):
            if m.group(1) in classes and (best is None or m.start() > best[0]):
                best = (m.start(), m.group(1))
    return best[1] if best else None


# ----------------------------------------------------------------------------- inputs
def decl_literals(path):
    raw = open(path, encoding='utf-8', errors='replace').read()
    lits = []
    for ln in raw.splitlines():
        s = ln.strip()
        if s.startswith('"'):
            try:
                lits.append(json.loads(s.rstrip(',')))
            except ValueError:
                pass
    return lits


def parse_layout(text):
    proven, section, ident = set(), None, {}
    for ln in text.splitlines():
        ln = ln.strip()
        m = re.match(r'^\[([A-Za-z0-9_:]+)\]$', ln)
        if m:
            section = m.group(1); continue
        m = re.match(r'^([A-Za-z0-9_]+)\s*=\s*(\S+)', ln)
        if m and section:
            if section == 'NativeLayout':
                ident[m.group(1)] = m.group(2)
            else:
                proven.add((section.split('::')[-1], m.group(1)))
    return proven, ident


def source_files(roots):
    for r in roots:
        for dp, dns, fs in os.walk(r):
            dns[:] = sorted(x for x in dns if not x.startswith('.') and x not in ('generated_include',))
            for f in sorted(fs):
                if f.endswith(SRC_EXT) and not f.endswith(('.orig', '.rej')):
                    yield os.path.join(dp, f)


def accessor_map(dirs):
    acc = {}                                              # name -> {struct: {(struct, member)}}
    defn = re.compile(r'([A-Za-z_][A-Za-z0-9_:<>]*)::(Get[A-Za-z0-9_]+)\s*\(\s*\)\s*(?:const)?\s*\n?\s*\{(.*?)\n\}', re.S)
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if not (f.startswith('MemberVariableLayout_SrcWrapper_') and f.endswith('.hpp')):
                continue
            text = open(os.path.join(d, f), encoding='utf-8', errors='replace').read()
            for m in defn.finditer(text):
                struct, name, body = m.group(1).split('::')[-1], m.group(2), m.group(3)
                k = re.search(r'MemberOffsets\.find\(STR\("([A-Za-z0-9_]+)"\)\)', body)
                if k:
                    acc.setdefault(name, {}).setdefault(struct, set()).add((struct, k.group(1)))
    return acc


class Model:
    """Helper closure keyed by (function name, owner). owner = class for member functions
    (qualifier or enclosing class), a namespace qualifier, or None for free functions."""
    def __init__(self, acc, target, debug, stats):
        self.acc = acc
        self.target, self.debug, self.stats = target, debug, stats
        self.bases = {}                                   # real classes -> direct bases
        self.defs = {}                                    # (name, owner) -> [calls per overload]
        self.reads = {}                                   # (name, owner) -> members
        self.text_cache = {}
        for name, by in acc.items():
            for cls, mem in by.items():
                self.reads[(name, cls)] = set(mem)

    def load(self, path):
        if path in self.text_cache:
            return self.text_cache[path]
        try:
            raw = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            raw = ''
        fc = [(re.compile(rx), v) for frx, rx, v in FILE_CONST_SPECS if ACTIVE_FILE_CONSTS and re.search(frx, path)]
        stripped = strip_comments_strings(raw)
        problems = []
        code = prune_versions(stripped, self.target, self.debug, self.stats, fc, problems)
        # self-check: pruning removes whole blocks only, so '{' and '}' must drop by the same
        # amount, and no pruned span may be unbalanced or run to the end of the file.
        d_open = stripped.count('{') - code.count('{')
        d_close = stripped.count('}') - code.count('}')
        if problems or d_open != d_close:
            raise GateIntegrityError('%s: version pruning self-check failed (%d unaccountable span(s)%s; braces removed open=%d close=%d)' %
                                     (path, len(problems), (' at lines ' + ', '.join('%d-%d' % x for x in problems[:5])) if problems else '', d_open, d_close))
        self.stats['pruned_files'] = self.stats.get('pruned_files', 0) + (1 if code != stripped else 0)
        self.stats['loaded_files'] = self.stats.get('loaded_files', 0) + 1
        self.text_cache[path] = (raw, code)
        return raw, code

    def index_classes(self, roots):
        for p in source_files(roots):
            _, code = self.load(p)
            for m in re.finditer(r'\b(?:class|struct)\s+(?:alignas\s*\([^)]*\)\s*)?(?:[A-Z][A-Z0-9_]*_API\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::\s*([^{;()]*))?\{', code):
                lst = self.bases.setdefault(m.group(1), [])
                for x in bases_of(m.group(2)):
                    if x not in lst and x != m.group(1):
                        lst.append(x)
        for by in self.acc.values():
            for cls in by:
                self.bases.setdefault(cls, [])

    def is_class(self, x):
        return x is not None and x in self.bases

    def chain(self, cls):
        out, todo = [], [cls]
        while todo:
            c = todo.pop(0)
            if c in out or c is None:
                continue
            out.append(c)
            todo.extend(self.bases.get(c, []))
        return out

    def calls_in(self, code, start, end, cls):
        res = []
        for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', code[start:end]):
            name = m.group(1)
            if name in KEYWORDS:
                continue
            a = start + m.start()
            look = code[max(start, a - 4):a]
            am = re.search(r'(->|\.)\s*$', look)
            if am:
                arrow = a - len(look) + am.start()
                rc = receiver_class(code, arrow, start, cls, self.bases)
                if rc:
                    self.stats['typed_receivers'] += 1
                res.append(('member', name, rc))
                continue
            qm = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*::\s*$', code[max(start, a - 80):a])
            if qm:
                res.append(('qual', name, qm.group(1)))
            else:
                res.append(('unq', name, cls))
        return res

    def index_defs(self, roots):
        bodies = []
        for p in source_files(roots):
            _, code = self.load(p)
            for kind, name, owner, s, e in scopes(code):
                if kind == 'fn':
                    bodies.append((code, name, owner, s, e))
        for code, name, owner, s, e in bodies:
            self.defs.setdefault((name, owner), []).append(self.calls_in(code, s, e, owner if self.is_class(owner) else None))

    def visible_tokens(self, cls):
        """Names through which an object of class cls can enter a file: the class, its
        subclasses, and functions whose declared return type mentions any of them."""
        if not hasattr(self, '_vis'):
            self._vis = {}
            self._subs = {}
            for c, bs in self.bases.items():
                for b in bs:
                    self._subs.setdefault(b, set()).add(c)
            self._all = '\n'.join(code for (_, code) in self.text_cache.values())
        if cls in self._vis:
            return self._vis[cls]
        names, todo = set(), [cls]
        while todo:
            c = todo.pop()
            if c in names:
                continue
            names.add(c)
            todo.extend(self._subs.get(c, ()))
        toks = set(names)
        alt = '|'.join(re.escape(n) for n in sorted(names))
        for rx in (r'\b(?:' + alt + r')\b\s*(?:<[^;{}()]*>)?\s*[*&]+\s*(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)\s*\(',
                   r'<[^<>;{}()]*\b(?:' + alt + r')\b\s*\*[^<>;{}()]*>\s*[*&]*\s*(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)\s*\(',
                   r'([A-Za-z_][A-Za-z0-9_]*)\s*\([^(){};]*\)\s*(?:const\s*)?(?:noexcept\s*)?->\s*[^;{]*\b(?:' + alt + r')\b'):
            for m in re.finditer(rx, self._all):
                if m.group(1) not in KEYWORDS:
                    toks.add(m.group(1))
        self._vis[cls] = toks
        return toks

    def defined(self, name, owner):
        return (name, owner) in self.reads or (name, owner) in self.defs

    def owners(self, name):
        return {k[1] for k in self.reads if k[0] == name} | {k[1] for k in self.defs if k[0] == name}

    def union_all(self, name):
        """Unknown receiver: previous conservative semantics. Accessor names union every class;
        other names propagate only when every owner's definition reads something."""
        owners = self.owners(name)
        if not owners:
            return set()
        vals = [self.reads.get((name, o), set()) for o in owners]
        if name in self.acc or all(vals):
            got = set()
            for v in vals:
                got |= v
            return got
        return set()

    def union_free(self, name):
        free = [o for o in self.owners(name) if not self.is_class(o)]
        if not free:
            return None
        got = set()
        for o in free:
            got |= self.reads.get((name, o), set())
        return got

    def lookup(self, name, cls):
        for k in self.chain(cls):
            if self.defined(name, k):
                return self.reads.get((name, k), set())
        return None

    def resolve(self, kind, name, x):
        if kind == 'member':
            if x:
                got = self.lookup(name, x)
                if got is not None:
                    return got
            return self.union_all(name)
        if kind == 'qual':
            if self.is_class(x):
                got = self.lookup(name, x)
                if got is not None:
                    return got
                return self.union_all(name)
            if self.defined(name, x):
                return self.reads.get((name, x), set())
            got = self.union_free(name)
            return got if got is not None else self.union_all(name)
        # unqualified: own class chain first, then free functions, then everything
        if x:
            got = self.lookup(name, x)
            if got is not None:
                return got
        got = self.union_free(name)
        return got if got is not None else self.union_all(name)

    def closure(self):
        changed = True
        rounds = 0
        while changed and rounds < 60:
            changed = False
            rounds += 1
            for key, overloads in self.defs.items():
                got = set()
                for calls in overloads:
                    for (kind, callee, x) in calls:
                        if callee == key[0] and (x == key[1] or kind == 'unq'):
                            continue
                        got |= self.resolve(kind, callee, x)
                if got - self.reads.get(key, set()):
                    self.reads.setdefault(key, set()).update(got)
                    changed = True
        return {k[0] for k in self.reads if self.reads[k]}


DIRECT_FIND = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)::MemberOffsets\.find\(\s*STR\(\s*"([A-Za-z0-9_]+)"\s*\)\s*\)')
DIRECT_ANY = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)::MemberOffsets\b(?!\.end\b)')
MEMBER_CALL = re.compile(r'(->|\.)\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(')
OK_RE = re.compile(r'FIELD-GATE-OK\s*:?\s*(.*)$')


def macro_params(raw_lines):
    """line index -> set of macro parameter names for lines inside a #define body."""
    out = {}
    i = 0
    while i < len(raw_lines):
        m = re.match(r'\s*#\s*define\s+[A-Za-z_][A-Za-z0-9_]*\(([^)]*)\)', raw_lines[i])
        if m:
            params = {p.strip() for p in m.group(1).split(',') if p.strip()}
            j = i
            while True:
                out[j] = params
                if not raw_lines[j].rstrip().endswith('\\') or j + 1 >= len(raw_lines):
                    break
                j += 1
            i = j + 1
        else:
            i += 1
    return out


def scan(model, roots, rel_to, read_names):
    findings, calls, files, macro_iter = [], 0, 0, []
    file_ids = {}
    for p in source_files(roots):
        files += 1
        raw, code = model.load(p)
        raw_lines = raw.split('\n')
        code_lines = code.split('\n')
        mparams = macro_params(raw_lines)
        # function scopes for receiver typing
        fns = [(s, e, cls) for kind, name, cls, s, e in scopes(code) if kind == 'fn']
        line_starts = [0]
        for ln in code_lines:
            line_starts.append(line_starts[-1] + len(ln) + 1)

        def ok_of(idx):
            rl = raw_lines[idx] if idx < len(raw_lines) else ''
            m = OK_RE.search(rl)
            if not m:
                return None
            return m.group(1).strip()

        for idx, cl in enumerate(code_lines):
            if 'MemberOffsets' not in cl:
                continue
            rl = raw_lines[idx] if idx < len(raw_lines) else ''
            direct = list(DIRECT_FIND.finditer(rl))
            for m in direct:
                calls += 1
                findings.append((os.path.relpath(p, rel_to), idx + 1, 'MemberOffsets.find', (m.group(1), m.group(2)), ' '.join(cl.split()), ok_of(idx)))
            spans = [m.span() for m in direct]
            for m in DIRECT_ANY.finditer(cl):
                if any(a <= m.start() < b for a, b in spans):
                    continue
                if m.group(1) in mparams.get(idx, set()):
                    macro_iter.append((os.path.relpath(p, rel_to), idx + 1, m.group(1)))
                    continue
                calls += 1
                findings.append((os.path.relpath(p, rel_to), idx + 1, 'MemberOffsets', (m.group(1), '<unresolved>'), ' '.join(cl.split()), ok_of(idx)))
        for m in MEMBER_CALL.finditer(code):
            name = m.group(2)
            if name not in read_names:
                continue
            pos = m.start(1)
            floor, own = 0, None
            for s, e, cls in fns:
                if s <= pos <= e and s >= floor:
                    floor, own = s, cls
            rc = receiver_class(code, pos, floor, own, model.bases)
            if rc:
                model.stats['typed_call_sites'] += 1
            members = model.resolve('member', name, rc)
            if rc is None and members and not NO_FILTER:
                # unknown receiver: a class whose name, subclasses and producers never appear in
                # this file cannot be the receiver here
                ids = file_ids.get(p)
                if ids is None:
                    ids = file_ids[p] = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', code))
                kept = {mm for mm in members if model.visible_tokens(mm[0]) & ids}
                model.stats['token_filtered'] += len(members) - len(kept)
                for mm in sorted(members - kept):
                    model.stats.setdefault('dropped', []).append('%s:%d %s() %s::%s' % (os.path.relpath(p, rel_to), bisect.bisect_right(line_starts, pos), name, mm[0], mm[1]))
                members = kept
            if not members:
                continue
            calls += 1
            idx = bisect.bisect_right(line_starts, pos) - 1
            for member in sorted(members):
                findings.append((os.path.relpath(p, rel_to), idx + 1, name, member, ' '.join(code_lines[idx].split()), ok_of(idx)))
    return files, calls, findings, macro_iter


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=CAND, help='tree to gate (default: the candidate tree)')
    ap.add_argument('--decl', default=DECL, help='compiled declaration header (members = first string literal)')
    ap.add_argument('--ini', default=None, help='OPT-IN extra layout ini (the PS5 runtime ignores it while a compiled declaration exists)')
    ap.add_argument('--baseline-snapshot', default=None, help='OPT-IN: tar.gz snapshot whose findings are reported as accepted DEBT')
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--no-receiver-filter', action='store_true', help='audit mode: keep members of classes that never appear in the file (stricter, more false positives)')
    ap.add_argument('--list-out', help='write every engine field the scanned code reads (proven or not, with site counts)')
    ap.add_argument('--explain', nargs=2, metavar=('FUNCTION', 'STRUCT::MEMBER'), help='print one helper path from FUNCTION to the member read')
    a = ap.parse_args()

    global NO_FILTER
    NO_FILTER = a.no_receiver_filter
    if not os.path.isfile(a.decl):
        print('FIELD GATE: FAIL (compiled declaration missing: %s)' % a.decl); return 2
    lits = decl_literals(a.decl)
    if not lits:
        print('FIELD GATE: FAIL (no string literal in %s)' % a.decl); return 2
    proven, ident = parse_layout(lits[0])
    sources = ['compiled member declaration %s (%d members)' % (os.path.basename(a.decl), len(proven))]
    if a.ini:
        if not os.path.isfile(a.ini):
            print('FIELD GATE: FAIL (opt-in ini missing: %s)' % a.ini); return 2
        extra, _ = parse_layout(open(a.ini, encoding='utf-8', errors='replace').read())
        sources.append('OPT-IN ini %s (%d members, %d not in the declaration)' % (a.ini, len(extra), len(extra - proven)))
        proven |= extra
    try:
        target = (int(ident['EngineMajor']), int(ident['EngineMinor']))
    except (KeyError, ValueError):
        print('FIELD GATE: FAIL (declaration has no EngineMajor/EngineMinor)'); return 2
    debug = False if ident.get('NonShipping') == '0' else None
    print('FIELD GATE: proven sources: ' + '; '.join(sources))
    print('FIELD GATE: target engine %d.%d (version-dead branches pruned for this version; IsDebug=%s)' % (target[0], target[1], debug))

    consts_ok = True
    for rx, val, checks, desc in TARGET_CONST_SPECS:
        ok = True
        for f, needle in checks:
            f = f.replace('{ROOT}', a.root)
            try:
                ok = ok and needle in open(f, encoding='utf-8', errors='replace').read()
            except OSError:
                ok = False
        if ok:
            ACTIVE_CONSTS.append((re.compile(rx), val))
            print('FIELD GATE: target constant %s (verified in %s)' % (desc, ', '.join(os.path.basename(c[0]) for c in checks)))
        else:
            consts_ok = False
            print('FIELD GATE: target constant NOT verified, not used: %s' % desc)
    if consts_ok and ACTIVE_CONSTS:
        ACTIVE_FILE_CONSTS.append(True)
    stats = {'pruned_branches': 0, 'guard_returns': 0, 'typed_receivers': 0, 'typed_call_sites': 0, 'token_filtered': 0}
    acc = accessor_map([PORT + '/unreal/generated_include', a.root + '/generated_include'])
    model = Model(acc, target, debug, stats)
    helper_roots = [PORT + '/unreal/include', PORT + '/unreal/src', a.root + '/include/Unreal', a.root + '/src']
    model.index_classes(helper_roots + [a.root + '/include'])
    model.index_defs(helper_roots)
    read_names = model.closure()
    if a.explain:
        fn, sm = a.explain
        want = tuple(sm.split('::')[-2:])
        frontier = [((fn, o), [(fn, o)]) for o in model.owners(fn) if want in model.reads.get((fn, o), set())]
        seen = set()
        while frontier:
            key, path = frontier.pop(0)
            if key in seen:
                continue
            seen.add(key)
            if key[0] in acc and key[1] in acc[key[0]] and want in acc[key[0]][key[1]]:
                print('EXPLAIN: ' + ' -> '.join('%s::%s' % (o, n) for n, o in path)); break
            for calls in model.defs.get(key, []):
                for kind, callee, x in calls:
                    for o in model.owners(callee):
                        if want in model.reads.get((callee, o), set()) and want in model.resolve(kind, callee, x):
                            frontier.append(((callee, o), path + [(callee, o)]))
        return 0
    n_acc = sum(len(v) for v in acc.values())
    print('FIELD GATE: proven members %d; accessors %d (%d names); classes indexed %d; functions that read members (incl. helpers) %d' %
          (len(proven), n_acc, len(acc), len(model.bases), len(read_names)))
    if not proven or not acc:
        print('FIELD GATE: FAIL (zero proven members or zero accessors parsed)'); return 2

    def unproven(fs):
        return [f for f in fs if (f[3][0], f[3][1]) not in proven]

    files, calls, findings, macro_iter = scan(model, [a.root + '/src', a.root + '/include'], a.root, read_names)
    print('FIELD GATE: scanned %d files, %d member-reading calls, %d (call, member) pairs; %d version-dead branches pruned, %d guard-returns; %d call sites with a resolved receiver class' %
          (files, calls, len(findings), stats['pruned_branches'], stats['guard_returns'], stats['typed_call_sites']))
    print('FIELD GATE: %d (call, member) pairs dropped for unknown receivers whose class never appears in the file' % stats['token_filtered'])
    print('FIELD GATE: pruning self-check OK: %d files loaded, %d with version-dead code blanked, 0 unaccountable spans' %
          (stats.get('loaded_files', 0), stats.get('pruned_files', 0)))
    print('FIELD GATE: %d MemberOffsets map iterations inside macros (no field read): %s' %
          (len(macro_iter), ', '.join('%s:%d(%s)' % x for x in macro_iter) or '-'))
    if files == 0 or calls == 0:
        print('FIELD GATE: FAIL (scanned nothing)'); return 2

    base_keys = set()
    if a.baseline_snapshot:
        if not os.path.isfile(a.baseline_snapshot):
            print('FIELD GATE: FAIL (opt-in baseline snapshot missing: %s)' % a.baseline_snapshot); return 2
        tmp = tempfile.mkdtemp(prefix='fieldgate-')
        with tarfile.open(a.baseline_snapshot) as t:
            t.extractall(tmp)
        broot = None
        for dp, dns, _ in os.walk(tmp):
            if dp.endswith('typescan-r4/candidate'):
                broot = dp; break
        if broot:
            bmodel = Model(acc, target, debug, dict(stats))
            bmodel.bases, bmodel.defs, bmodel.reads = model.bases, model.defs, model.reads
            _, _, bfind, _ = scan(bmodel, [broot + '/src', broot + '/include'], broot, read_names)
            base_keys = {(f[0], f[3], f[4]) for f in unproven(bfind)}
            print('FIELD GATE: OPT-IN baseline %s: %d unproven sites accepted as DEBT' % (os.path.basename(a.baseline_snapshot), len(base_keys)))
        shutil.rmtree(tmp, ignore_errors=True)

    fields = {}
    for f in findings:
        fields.setdefault(f[3], set()).add((f[0], f[1]))
    n_unproven_fields = sum(1 for k in fields if k not in proven)
    print('FIELD GATE: %d distinct engine fields read: %d proven, %d not in this game\'s layout data' %
          (len(fields), len(fields) - n_unproven_fields, n_unproven_fields))
    if a.list_out:
        with open(a.list_out, 'w') as out:
            out.write('# engine fields read by %s (struct::member  proven|UNPROVEN  call-sites)\n' % a.root)
            for k in sorted(fields):
                out.write('%s::%s\t%s\t%d\n' % (k[0], k[1], 'proven' if k in proven else 'UNPROVEN', len(fields[k])))
        print('FIELD GATE: field list -> %s' % a.list_out)

    bad, ok_reason, ok_bare, debt = [], [], [], []
    for f in unproven(findings):
        reason = f[5]
        if reason is not None and len(reason) >= MIN_REASON:
            ok_reason.append(f)
        elif (f[0], f[3], f[4]) in base_keys:
            debt.append(f)
        else:
            if reason is not None:
                ok_bare.append(f)
            bad.append(f)
    for f in bad:
        print('  NEW UNPROVEN %s:%d %s() reads %s::%s   | %s' % (f[0], f[1], f[2], f[3][0], f[3][1], f[4][:120]))
    for f in ok_bare:
        print('  FIELD-GATE-OK WITHOUT REASON (not accepted) %s:%d' % (f[0], f[1]))
    for f in ok_reason:
        print('  accepted FIELD-GATE-OK %s:%d %s::%s  reason: %s' % (f[0], f[1], f[3][0], f[3][1], f[5][:160]))
    if a.verbose:
        for d in stats.get('dropped', []):
            print('  receiver-filter dropped %s' % d)
        for f in debt:
            print('  DEBT (opt-in baseline) %s:%d %s() reads %s::%s' % (f[0], f[1], f[2], f[3][0], f[3][1]))
        for k in sorted(fields):
            if k in proven:
                print('  proven %s::%s (%d sites)' % (k[0], k[1], len(fields[k])))
    print('FIELD GATE: %d unproven member read(s); accepted: %d FIELD-GATE-OK with reason, %d opt-in baseline debt; %d bare FIELD-GATE-OK rejected' %
          (len(bad), len(ok_reason), len(debt), len(ok_bare)))
    verdict = 'FAIL' if bad else ('PASS (WITH OPT-IN BASELINE DEBT %d)' % len(debt) if debt else 'PASS')
    print('FIELD GATE: %s' % verdict)
    return 1 if bad else 0


def _main_checked():
    try:
        return main()
    except GateIntegrityError as e:
        print('FIELD GATE: FAIL (gate self-check: %s)' % e)
        return 2


if __name__ == '__main__':
    sys.exit(_main_checked())
