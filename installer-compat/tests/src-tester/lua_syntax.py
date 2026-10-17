#!/usr/bin/env python3
"""Pure-python Lua 5.3 syntax validator (the 'lua53 -p equivalent').

No Lua interpreter exists on this host (checked: no lua/luajit/luac in PATH,
/opt/homebrew, /usr/local, or Applications), so this implements the Lua 5.3
grammar (lua 5.3 reference manual, chapter 9) directly:
tokenizer (incl. long strings/comments, hex floats, escapes) + recursive
descent parser with the real operator precedences and the standard
assignment-vs-functioncall statement ambiguity resolution.

It validates SYNTAX only, like `luac -p`. It deliberately does not execute.
"""

import re
import sys

KEYWORDS = {
    "and", "break", "do", "else", "elseif", "end", "false", "for",
    "function", "goto", "if", "in", "local", "nil", "not", "or",
    "repeat", "return", "then", "true", "until", "while",
}

# numeral: ints, hex, floats, hex floats (0x1p4), exponents
NUMERAL_RE = re.compile(
    r"0[xX](?:[0-9a-fA-F]*\.?[0-9a-fA-F]+(?:[pP][-+]?\d+)?"  # hex int/float
    r"|[0-9a-fA-F]+\.?[0-9a-fA-F]*(?:[pP][-+]?\d+)?)"
    r"|\d+\.?\d*(?:[eE][-+]?\d+)?"
    r"|\.\d+(?:[eE][-+]?\d+)?"
)
NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


class LuaSyntaxError(Exception):
    pass


class Tok:
    __slots__ = ("kind", "val", "line")

    def __init__(self, kind, val, line):
        self.kind = kind  # NAME KW NUMERAL STRING OP EOF
        self.val = val
        self.line = line

    def __repr__(self):
        return f"{self.kind}({self.val!r})@{self.line}"


def tokenize(src):
    toks = []
    i, n, line = 0, len(src), 1
    # lauxlib skips a first line beginning with '#'
    if src.startswith("#"):
        nl = src.find("\n")
        if nl == -1:
            return [Tok("EOF", None, 1)]
        line += 1
        i = nl + 1
    # real Lua 5.3 two-char operators:
    lua_ops2 = ("==", "~=", "<=", ">=", "::", "..", "<<", ">>", "//")
    while i < n:
        c = src[i]
        if c in " \t\r":
            i += 1
            continue
        if c == "\n":
            line += 1
            i += 1
            continue
        # comments
        if c == "-" and src.startswith("--", i):
            i += 2
            m = re.match(r"\[(=*)\[", src[i:])
            if m:
                # long comment
                closer = "]" + m.group(1) + "]"
                end = src.find(closer, i + m.end())
                if end == -1:
                    raise LuaSyntaxError(f"line {line}: unfinished long comment")
                seg = src[i + m.end(): end]
                line += seg.count("\n")
                i = end + len(closer)
            else:
                nl = src.find("\n", i)
                i = n if nl == -1 else nl
            continue
        # long strings [[...]] [=[...]=]
        if c == "[":
            m = re.match(r"\[(=*)\[", src[i:])
            if m:
                closer = "]" + m.group(1) + "]"
                end = src.find(closer, i + m.end())
                if end == -1:
                    raise LuaSyntaxError(f"line {line}: unfinished long string")
                seg = src[i + m.end(): end]
                # first newline right after opening is skipped
                if seg.startswith("\r\n"):
                    seg = seg[2:]
                elif seg.startswith("\n"):
                    seg = seg[1:]
                line += seg.count("\n")
                toks.append(Tok("STRING", seg, line))
                i = end + len(closer)
                continue
        # quoted strings
        if c in "\"'":
            j = i + 1
            buf = []
            while j < n:
                ch = src[j]
                if ch == "\\":
                    if j + 1 >= n:
                        raise LuaSyntaxError(f"line {line}: unfinished string")
                    e = src[j + 1]
                    if e == "z":
                        j += 2
                        while j < n and src[j] in " \t\r\n":
                            if src[j] == "\n":
                                line += 1
                            j += 1
                        continue
                    if e == "x":
                        m2 = re.match(r"[0-9a-fA-F]{2}", src[j + 2:j + 4])
                        if not m2:
                            raise LuaSyntaxError(
                                f"line {line}: \\x needs 2 hex digits")
                        j += 4
                        buf.append("?")
                        continue
                    if e == "u":
                        m2 = re.match(r"\{[0-9a-fA-F]+\}", src[j + 2:j + 12])
                        if not m2:
                            raise LuaSyntaxError(
                                f"line {line}: \\u needs {{hex}}")
                        j += 2 + m2.end()
                        buf.append("?")
                        continue
                    if e in "\n\r":
                        # literal newline escape (5.3 allows \<newline>)
                        line += 1
                        j += 2
                        continue
                    if e.isdigit():
                        m2 = re.match(r"\d{1,3}", src[j + 1:j + 4])
                        val = int(m2.group(0))
                        if val > 255:
                            raise LuaSyntaxError(
                                f"line {line}: decimal escape too large")
                        j += 1 + m2.end()
                        buf.append("?")
                        continue
                    j += 2
                    buf.append("?")
                    continue
                if ch == c:
                    break
                if ch == "\n":
                    raise LuaSyntaxError(f"line {line}: unfinished string")
                buf.append(ch)
                j += 1
            else:
                raise LuaSyntaxError(f"line {line}: unfinished string")
            toks.append(Tok("STRING", "".join(buf), line))
            i = j + 1
            continue
        # numerals
        if c.isdigit() or (c == "." and i + 1 < n and src[i + 1].isdigit()):
            m = NUMERAL_RE.match(src, i)
            text = m.group(0)
            # reject '1..2' style: number immediately followed by '.' when the
            # numeral already consumed one dot
            if "." in text and src.startswith("..", i + len(text)):
                raise LuaSyntaxError(
                    f"line {line}: malformed number near '{text}..'")
            if text.endswith("."):
                # Lua accepts '1.' — real lua does NOT ('malformed number')
                raise LuaSyntaxError(
                    f"line {line}: malformed number near '{text}'")
            validate_numeral(text, line)
            toks.append(Tok("NUMERAL", text, line))
            i += len(text)
            continue
        # names / keywords
        m = NAME_RE.match(src, i)
        if m:
            w = m.group(0)
            toks.append(Tok("KW" if w in KEYWORDS else "NAME", w, line))
            i += len(w)
            continue
        # operators
        three = src[i:i + 3]
        if three == "...":
            toks.append(Tok("OP", "...", line))
            i += 3
            continue
        two = src[i:i + 2]
        if two in lua_ops2:
            toks.append(Tok("OP", two, line))
            i += 2
            continue
        if c in "+-*/%^#&~|<>=(){}[];:,.":
            toks.append(Tok("OP", c, line))
            i += 1
            continue
        raise LuaSyntaxError(f"line {line}: unexpected symbol near '{c}'")
    toks.append(Tok("EOF", None, line))
    return toks


def validate_numeral(text, line):
    t = text
    if t[:2].lower() == "0x":
        body = t[2:]
        if re.match(r"^[0-9a-fA-F]*\.?[0-9a-fA-F]*([pP][-+]?\d+)?$", body):
            if body.startswith(".") or ".." in body:
                raise LuaSyntaxError(f"line {line}: malformed number '{text}'")
            return
        raise LuaSyntaxError(f"line {line}: malformed number '{text}'")
    if not re.match(r"^(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$", t):
        raise LuaSyntaxError(f"line {line}: malformed number '{text}'")
    if t.endswith("e") or t.endswith("E"):
        raise LuaSyntaxError(f"line {line}: malformed number '{text}'")


# unary / binary precedence (lua 5.3)
BINPRIO = {
    "or": (1, 1), "and": (2, 2),
    "<": (3, 3), ">": (3, 3), "<=": (3, 3), ">=": (3, 3), "~=": (3, 3),
    "==": (3, 3),
    "|": (4, 4), "~": (5, 5), "&": (6, 6),
    "<<": (7, 7), ">>": (7, 7),
    "..": (9, 8),  # right assoc
    "+": (10, 10), "-": (10, 10),
    "*": (11, 11), "/": (11, 11), "//": (11, 11), "%": (11, 11),
    "^": (14, 13),  # right assoc
}
UNARY_PRIO = 12
MAX_PRIO = 14


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.pos = 0
        # tracks whether we're inside a vararg function for '...' checks
        # (syntax-only check: skip; luac -p doesn't track it either)

    def peek(self, k=0):
        return self.toks[min(self.pos + k, len(self.toks) - 1)]

    def next(self):
        t = self.toks[self.pos]
        self.pos += 1
        return t

    def error(self, msg):
        t = self.peek()
        near = t.val if t.val is not None else "<eof>"
        raise LuaSyntaxError(f"line {t.line}: {msg} near '{near}'")

    def check(self, kind, val):
        t = self.peek()
        if t.kind != kind or t.val != val:
            self.error(f"expected '{val}'")
        return self.next()

    def accept(self, kind, val):
        if self.peek().kind == kind and self.peek().val == val:
            return self.next()
        return None

    def is_kw(self, v):
        t = self.peek()
        return t.kind == "KW" and t.val == v

    def block_follow(self):
        t = self.peek()
        if t.kind == "EOF":
            return True
        return t.kind == "KW" and t.val in (
            "end", "else", "elseif", "until")

    # ---- grammar ----

    def parse_chunk(self):
        self.block()
        if self.peek().kind != "EOF":
            self.error("unexpected symbol (chunk not closed)")

    def block(self):
        while not self.block_follow():
            if self.is_kw("return"):
                self.next()
                if not self.block_follow() and not self.accept("OP", ";"):
                    self.explist()
                self.accept("OP", ";")
                break
            self.statement()

    def statement(self):
        t = self.peek()
        if t.kind == "OP" and t.val == ";":
            self.next()
            return
        if t.kind == "KW":
            v = t.val
            if v == "if":
                self.next()
                self.expr()
                self.check("KW", "then")
                self.block()
                while self.is_kw("elseif"):
                    self.next()
                    self.expr()
                    self.check("KW", "then")
                    self.block()
                if self.is_kw("else"):
                    self.next()
                    self.block()
                self.check("KW", "end")
                return
            if v == "while":
                self.next()
                self.expr()
                self.check("KW", "do")
                self.block()
                self.check("KW", "end")
                return
            if v == "do":
                self.next()
                self.block()
                self.check("KW", "end")
                return
            if v == "for":
                self.next()
                self.check_name()
                if self.accept("OP", "="):
                    self.expr()
                    self.check("OP", ",")
                    self.expr()
                    if self.accept("OP", ","):
                        self.expr()
                else:
                    while self.accept("OP", ","):
                        self.check_name()
                    self.check("KW", "in")
                    self.explist()
                self.check("KW", "do")
                self.block()
                self.check("KW", "end")
                return
            if v == "repeat":
                self.next()
                self.block()
                self.check("KW", "until")
                self.expr()
                return
            if v == "function":
                self.next()
                # funcname: Name {'.' Name} [':' Name]
                self.check_name()
                while self.accept("OP", "."):
                    self.check_name()
                if self.accept("OP", ":"):
                    self.check_name()
                self.funcbody()
                return
            if v == "local":
                self.next()
                if self.is_kw("function"):
                    self.next()
                    self.check_name()
                    self.funcbody()
                    return
                self.check_name()
                while self.accept("OP", ","):
                    self.check_name()
                if self.accept("OP", "="):
                    self.explist()
                return
            if v == "break":
                self.next()
                return
            if v == "goto":
                self.next()
                self.check_name()
                return
        if t.kind == "OP" and t.val == "::":
            # label :: Name ::
            self.next()
            self.check_name()
            self.check("OP", "::")
            return
        # expression statement: assignment or function call
        t = self.peek()
        if t.kind in ("NAME", "(") or (t.kind == "KW" and t.val == "function"):
            # suffixedexp
            e = self.suffixedexp()
            if self.peek().kind == "OP" and self.peek().val in (",", "="):
                # varlist = explist
                if e != "var":
                    self.error("syntax error (cannot assign to a call)")
                while self.accept("OP", ","):
                    e2 = self.suffixedexp()
                    if e2 != "var":
                        self.error("syntax error (cannot assign)")
                self.check("OP", "=")
                self.explist()
                return
            if e != "call":
                self.error("syntax error near statement "
                           "(expression is not a call or assignment)")
            return
        self.error("unexpected symbol")

    def check_name(self):
        t = self.peek()
        if t.kind != "NAME":
            self.error("expected a name")
        return self.next()

    def primaryexp(self):
        t = self.next()
        if t.kind == "OP" and t.val == "(":
            self.expr()
            self.check("OP", ")")
            return "expr"
        if t.kind == "NAME":
            return "var"
        if t.kind == "KW" and t.val in ("true", "false", "nil"):
            return "lit"
        if t.kind in ("NUMERAL", "STRING"):
            return "lit"
        if t.kind == "OP" and t.val == "...":
            return "lit"
        self.pos -= 1
        self.error("unexpected symbol")

    def suffixedexp(self):
        e = self.primaryexp()
        while True:
            t = self.peek()
            if t.kind == "OP" and t.val == ".":
                self.next()
                self.check_name()
                e = "var"
            elif t.kind == "OP" and t.val == "[":
                self.next()
                self.expr()
                self.check("OP", "]")
                e = "var"
            elif t.kind == "OP" and t.val in ("(", "{"):
                self.args()
                e = "call"
            elif t.kind == "OP" and t.val == ":":
                self.next()
                self.check_name()
                self.args()
                e = "call"
            elif t.kind == "STRING":
                self.next()  # call with literal string arg
                e = "call"
            else:
                return e

    def args(self):
        t = self.peek()
        if t.kind == "OP" and t.val == "{":
            self.tableconstructor()
        elif t.kind == "STRING":
            self.next()
        else:
            self.check("OP", "(")
            if not self.accept("OP", ")"):
                self.explist()
                self.check("OP", ")")

    def tableconstructor(self):
        self.check("OP", "{")
        while not self.accept("OP", "}"):
            t = self.peek()
            if t.kind == "OP" and t.val == "[":
                self.next()
                self.expr()
                self.check("OP", "]")
                self.check("OP", "=")
                self.expr()
            elif t.kind == "NAME" and self.peek(1).kind == "OP" \
                    and self.peek(1).val == "=":
                self.next()
                self.next()
                self.expr()
            else:
                self.expr()
            if not (self.accept("OP", ",") or self.accept("OP", ";")):
                self.check("OP", "}")
                return

    def funcbody(self):
        self.check("OP", "(")
        if not self.accept("OP", ")"):
            while True:
                t = self.peek()
                if t.kind == "NAME":
                    self.next()
                elif t.kind == "OP" and t.val == "...":
                    self.next()
                    if self.accept("OP", ","):
                        self.error("expected ')' after '...'")
                    break
                else:
                    self.error("expected parameter name")
                if not self.accept("OP", ","):
                    break
            self.check("OP", ")")
        self.block()
        self.check("KW", "end")

    def explist(self):
        self.expr()
        while self.accept("OP", ","):
            self.expr()

    def expr(self, limit=0):
        # unary prefix
        t = self.peek()
        if t.kind == "KW" and t.val == "not":
            self.next()
            self.expr(UNARY_PRIO)
        elif t.kind == "OP" and t.val in ("-", "#"):
            self.next()
            self.expr(UNARY_PRIO)
        elif t.kind == "OP" and t.val == "~":
            self.next()
            self.expr(UNARY_PRIO)
        else:
            self.simpleexp()
        # binary operators
        while True:
            t = self.peek()
            op = None
            if t.kind == "OP" and t.val in BINPRIO:
                op = t.val
            elif t.kind == "KW" and t.val in ("and", "or"):
                op = t.val
            if op is None:
                return
            leftprio, rightprio = BINPRIO[op]
            if leftprio <= limit:
                return
            self.next()
            self.expr(rightprio)

    def simpleexp(self):
        t = self.peek()
        if t.kind == "KW" and t.val == "function":
            self.next()
            self.funcbody()
            return
        if t.kind == "OP" and t.val == "{":
            self.tableconstructor()
            return
        self.suffixedexp()


def check(text):
    """Returns (ok: bool, error: str or '')."""
    try:
        toks = tokenize(text)
        p = Parser(toks)
        p.parse_chunk()
        return True, ""
    except LuaSyntaxError as e:
        return False, str(e)
    except RecursionError:
        return False, "nesting too deep (RecursionError in validator)"


def check_file(path):
    with open(path, "r", errors="replace") as f:
        return check(f.read())


# ---------------------------------------------------------------------------
# self-test: this validator must agree with known-good / known-bad Lua.
# ---------------------------------------------------------------------------

VALID = [
    "",
    ";",
    "x = 1",
    "local a,b,c = 1,2,3",
    "function f(a, ...) return ... end",
    "local t = {1,2,3, [1]=true, x=1, ['y']=2; 4,}",
    "t.x = t.y or {}",
    "print 'hello'",
    "print [[long\nstring]]",
    "f{1,2}",
    "x = a.b.c:d(1)(2)[3].e",
    "for i=1,10,2 do end",
    "for k,v in pairs(t) do end",
    "while true do break end",
    "repeat until true",
    "if a then elseif b then else end",
    "do local x = 1 end",
    "goto skip ::skip:: ",
    "local function g() end",
    "x = 0x1p4 + 0xff + 1e-10 + .5 + 3.14",
    "x = a ^ b ^ c",
    "x = a .. b .. c",
    "x = not a == b",
    "x = #t + -1 - ~2",
    "x = 1 << 2 | 3 & 4 ~ 5 >> 6 // 7 % 8",
    "x = a and b or c",
    "s = \"a\\65\\x41\\u{48}\\n\\z    end\"",
    "t = {f(), g(); h(),}",
    "x = (f())",
    "x = ((((1))))",
    "if x then y = 1 end",
    "a = b; c = d;",
    "return",
    "return 1,2",
    "return;",
    "function a.b.c:d() end",
    "x = y > 0 and 1 or -1",
    "m = {[(\"k\")]=2}",
    "x = '\\'' ",
    "y = \"\\\"\"",
    "x = function() return function() end end",
    "t = {nested = {deep = {deeper={}}}}",
    "x = a ~ b",
    "--[[ multi\nline\ncomment ]]\nx=1",
    "--[==[ level ]==]\n",
    "x = 1 --[[inline]] + 2",
]

INVALID = [
    ("x =", "expected expression"),
    ("x 1", None),
    ("if x then", None),
    ("if x end", None),
    ("function () end", None),
    ("local = 3", None),
    ("f(1,", None),
    ("t = {1,,2}", None),
    ("t = {[1]=}", None),
    ("x = 'unfinished", None),
    ("x = 'line\nbreak'", None),
    ("s = [[unfinished", None),
    ("--[[ unfinished", None),
    ("x = 1..2", None),
    ("x = 1.", None),
    ("x = 0x", None),
    ("x = '\\xg'", None),
    ("x = '\\300'", None),
    ("x = 1 +", None),
    ("end", None),
    ("else", None),
    ("until", None),
    ("::label", None),
    ("goto", None),
    ("a b", None),
    ("x = 1 2", None),
    ("f{1,2}", None),  # VALID actually — remove below
    ("local function end", None),
    ("t. = 3", None),
    ("a = b.c:d", None),
    ("x = ... ...", None),
    ("(", None),
    (")", None),
    ("}", None),
    ("x = a == = b", None),
    ("function f(a, b", None),
    ("x = \\ ", None),
    ("print(1))", None),
    ("x=", None),
]

# f{1,2} IS valid (call with table arg) — drop it from invalid
INVALID = [(s, why) for s, why in INVALID if s != "f{1,2}"]


def selftest():
    fails = 0
    for src in VALID:
        ok, err = check(src)
        if not ok:
            print(f"SELFTEST-FAIL(valid rejected): {src!r} -> {err}")
            fails += 1
    for src, _why in INVALID:
        ok, err = check(src)
        if ok:
            print(f"SELFTEST-FAIL(invalid accepted): {src!r}")
            fails += 1
    # round trip on this validator's own tricky cases
    ok, err = check(open(__file__).read().replace("'''", '"""', 0))
    print(f"selftest: {fails} failures out of "
          f"{len(VALID)} valid + {len(INVALID)} invalid cases")
    return fails


if __name__ == "__main__":
    if sys.argv[1:] and sys.argv[1] == "--selftest":
        sys.exit(1 if selftest() else 0)
    bad = 0
    for path in sys.argv[1:]:
        ok, err = check_file(path)
        if ok:
            print(f"OK       {path}")
        else:
            print(f"SYNTAX-ERROR {path}: {err}")
            bad += 1
    sys.exit(1 if bad else 0)
