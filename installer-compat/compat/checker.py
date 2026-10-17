#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""checker.py -- static Lua mod compatibility grader against a PS5 runtime API catalog.

MAC-ONLY HOST TOOL (host-tested on macOS). Pure static text analysis of .lua
sources: no FTP, no payloads, no launches, no console contact, no Lua execution.

Usage:
    python3 checker.py <mod.lua|mod_dir> <catalog.json> [--json OUT.json]
                       [--format text|json] [--max-sites N]

Catalog schema (minimal form used by the catalog agents):
    [{"name": "FindFirstOf", "category": "reflection",
      "ps5_runtime_status": "implemented+console-proven"}, ...]
Recognized ps5_runtime_status values:
    implemented+console-proven | implemented-untested | missing | behaves-differently
    | pc-only-input
Optional fields: "kind" ("global"|"method"|"static"|"type"|"constant"),
"type" (owner type for methods/statics), "workaround", "notes",
"pc_only_input" (bool: PC-keyboard input dependency; wins over an EMPTY
ps5_runtime_status so Key.*/ModifierKey.* keybind usage is reported as a
PC-only input dependency instead of "status-blank"/"not in catalog"),
"constants" (documented constant names carried on a type entry).
A {"catalog_version": ..., "entries": [...]} wrapper object is also accepted.

Grades (in precedence order):
    does not parse     a scanned .lua file failed the heuristic syntax check
                       (unterminated string/comment, unbalanced blocks,
                       invalid token): its calls are not extracted and the
                       report names the file and line; outranks everything
    unknown            ANY API the mod calls resolves to a catalog entry
                       with an EMPTY ps5_runtime_status (not yet filled): no
                       verdict is possible until the catalog is filled in
                       ("unknown - runtime status not filled") -- known PS5
                       differences (PC-only keyboard input) are listed WITHIN
                       this result and do NOT upgrade the grade; also the
                       grade when the mod uses APIs not in the catalog at all
    unsupported        a "missing" API is central (count > 3, a registration
                       call, or in the main code path: top level or a handler
                       registered via RegisterHook/RegisterKeyBind/Loop/...)
    partly-compatible  some calls missing / behave-differently, none central.
                       With NO blank statuses anywhere, a PC-only input
                       dependency (keyboard keybinds, no PS5 keyboard) also
                       lands here with its own reason.
    compatible         every cataloged API the mod calls is implemented on PS5

Best-effort static type tracking (documented limits, no execution):
    * local x = FName(...)                       -> x : FName
    * local x = FindFirstOf("T")                 -> x : T
    * local x = FindAllOf("T")                   -> x : T[] (elements T)
    * local x = GetPlayerController()            -> x : APlayerController
    * for _, it in pairs(x) where x : T[]        -> it : T
    * method calls on untyped vars (params, self) are reported as UNRESOLVED
      and deliberately do NOT count against the grade (usually game BP methods)
"""

from __future__ import annotations

import argparse
import bisect
import datetime
import json
import os
import re
import sys

OK_STATUSES = {"implemented+console-proven", "implemented-untested"}
PC_ONLY_INPUT_STATUS = "pc-only-input"
ALL_STATUSES = OK_STATUSES | {"missing", "behaves-differently",
                              PC_ONLY_INPUT_STATUS}


def is_blank_status(status):
    """ps5_runtime_status not filled in: None/missing, empty, or whitespace."""
    if status is None:
        return True
    return isinstance(status, str) and not status.strip()


def bucket_for_status(status):
    """Map a catalog entry's ps5_runtime_status to a call bucket."""
    if status in OK_STATUSES:
        return "ok"
    if is_blank_status(status):
        return "status-blank"  # cataloged but unfilled: cannot verify
    if status not in ALL_STATUSES:
        return "unknown-status"  # filled with an unrecognized value
    return status  # "missing" | "behaves-differently" | "pc-only-input"


def bucket_for_entry(entry, status):
    """Bucket for a resolved catalog ENTRY (not just its status).

    An entry flagged "pc_only_input": true (or filled with the dedicated
    "pc-only-input" status) is a PC-only KEYBOARD input dependency: the API /
    constant table exists and its constants are readable, but every value
    denotes a PC keyboard/mouse key -- on PS5 (no keyboard) the keybind needs
    runtime/config-side controller mapping (see PS5Pad.ini). The flag wins
    over an EMPTY ps5_runtime_status so such usage is reported as a PC-only
    input dependency, distinct from "missing" (API absent) and from
    "not in catalog" (API unknown).
    """
    if isinstance(entry, dict) and entry.get("pc_only_input"):
        return PC_ONLY_INPUT_STATUS
    if status == PC_ONLY_INPUT_STATUS:
        return PC_ONLY_INPUT_STATUS
    return bucket_for_status(status)

# High-signal registration/wiring APIs (per studio task spec).
HIGH_SIGNAL_REGISTRATIONS = {
    "RegisterKeyBind",
    "RegisterHook",
    "ExecuteInGameThread",
    "Loop",
    "NotifyOnNewObject",
    "RegisterConsoleCommandHandler",
}

LUA_KEYWORDS = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while", "self",
}

# Lua standard library: assumed present by any runtime, never graded --
# UNLESS the catalog itself registers the name (UE4SS replaces print and the
# PS5 port replaces dofile/loadfile; those are runtime APIs, see classify).
LUA_BUILTIN_GLOBALS = {
    "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs",
    "load", "loadfile", "next", "pairs", "pcall", "print", "rawequal",
    "rawget", "rawlen", "rawset", "require", "select", "setmetatable",
    "tonumber", "tostring", "type", "unpack", "xpcall",
}
LUA_BUILTIN_TABLES = {
    "os", "table", "string", "math", "io", "coroutine", "debug", "package",
    "utf8", "bit",
}

# Common factory globals -> return type (best-effort).
FACTORY_RETURNS = {
    "GetPlayerController": ("APlayerController", None),
    "GetLocalPlayer": ("ULocalPlayer", None),
    "GetWorld": ("UWorld", None),
    "GetEngine": ("UEngine", None),
}

# Best-effort inheritance fallback for method lookup.
EXPLICIT_BASES = {
    "APlayerController": ["AActor", "UObject"],
    "APlayerCameraManager": ["AActor", "UObject"],
    "APawn": ["AActor", "UObject"],
    "ACharacter": ["APawn", "AActor", "UObject"],
    "AActor": ["UObject"],
    "UWorld": ["UObject"],
    "UGameInstance": ["UObject"],
    "ULocalPlayer": ["UPlayer", "UObject"],
    "UPlayer": ["UObject"],
}


def base_chain(type_name):
    if type_name in EXPLICIT_BASES:
        return list(EXPLICIT_BASES[type_name])
    if type_name.startswith("A"):
        return ["AActor", "UObject"]
    if type_name.startswith("U") or type_name.startswith("I"):
        return ["UObject"]
    return []


IDENT_RE = re.compile(r"[A-Za-z_]\w*")
GLOBAL_CALL_RE = re.compile(r"(?<![\w.:])([A-Za-z_]\w*)\s*\(")
METHOD_CALL_RE = re.compile(r"(?<![\w.])([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)\s*\(")
STATIC_CALL_RE = re.compile(r"(?<![\w.:])([A-Za-z_]\w*)\s*\.\s*([A-Za-z_]\w*)\s*\(")
TYPE_REF_RE = re.compile(r"(?<![\w.:])([A-Za-z_]\w*)\s*\.\s*([A-Za-z_]\w*)")

DEFAULT_SUGGESTIONS = {
    "missing": (
        "Runtime-side: provide this API in the PS5 Lua runtime bridge (or shim "
        "it onto a console-proven equivalent). No mod edit is needed once the "
        "bridge supplies it."
    ),
    "behaves-differently": (
        "Runtime-side: wrap/normalize this API in the bridge so console "
        "behavior matches PC semantics and document the difference in the "
        "catalog entry."
    ),
    "unknown": (
        "Runtime-side: add this API to the runtime catalog with a tested "
        "ps5_runtime_status (it may be PC-only, game-specific, or simply "
        "uncataloged)."
    ),
    "pc-only-input": (
        "keyboard keybinds -> needs runtime/config controller mapping on PS5 "
        "(PS5 has no keyboard; map controller chords, e.g. L3+R3 = F7, via "
        "PS5Pad.ini or an equivalent runtime translation layer)"
    ),
}

GRADE_EXPLAIN = {
    "compatible": (
        "every cataloged API this mod calls is implemented on the PS5 runtime"
    ),
    "partly-compatible": (
        "some calls are missing / behave differently on PS5, but none of them "
        "are central to the mod's function"
    ),
    "unsupported": (
        "the mod depends on a PS5-missing API that is central to its function "
        "(called more than 3 times, a registration call, or in the main code "
        "path)"
    ),
    "unknown": (
        "the mod uses APIs that are not in the runtime catalog; compatibility "
        "cannot be verified"
    ),
    "does not parse": (
        "a scanned .lua file failed the heuristic syntax check (unterminated "
        "string/comment, unbalanced blocks, or an invalid token); nothing "
        "else in it can be analyzed"
    ),
}


# --------------------------------------------------------------------------
# Source stripping: blank out comments and string contents, preserving length
# and newlines, so regexes never fire inside them while offsets stay valid.
# --------------------------------------------------------------------------

def strip_lua(text):
    out = list(text)
    n = len(text)

    def blank(a, b):
        for k in range(a, min(b, n)):
            if out[k] != "\n":
                out[k] = " "

    i = 0
    while i < n:
        c = text[i]
        if c == "-" and text.startswith("--", i):
            m = re.match(r"--\[(=*)\[", text[i:])
            if m:  # block comment
                close = "]" + m.group(1) + "]"
                j = text.find(close, i + m.end())
                end = n if j == -1 else j + len(close)
                blank(i, end)
                i = end
            else:  # line comment (keep the newline)
                j = text.find("\n", i)
                end = n if j == -1 else j
                blank(i, end)
                i = end
        elif c == '"' or c == "'":
            j = i + 1
            while j < n:
                cj = text[j]
                if cj == "\\":
                    blank(j, min(j + 2, n))
                    j += 2
                    continue
                if cj == c or cj == "\n":
                    break
                j += 1
            blank(i, min(j + 1, n))
            i = min(j + 1, n)
        elif c == "[":
            m = re.match(r"\[(=*)\[", text[i:])
            if m:  # long bracket string [[...]] / [=[...]=]
                close = "]" + m.group(1) + "]"
                j = text.find(close, i + m.end())
                end = n if j == -1 else j + len(close)
                blank(i, end)
                i = end
            else:
                i += 1
        else:
            i += 1
    return "".join(out)


# --------------------------------------------------------------------------
# Heuristic Lua syntax check (finding #15): catches the most common failure
# modes -- unterminated string / long string / block comment, characters
# outside Lua's lexical set, unbalanced blocks -- WITHOUT a full parser.
# The block walk runs on strip_lua() output, so keywords inside strings and
# comments never count. A clean result is NOT a proof of valid Lua
# (construction errors like "local y = = 2" are invisible to it).
# --------------------------------------------------------------------------

# every character that can appear in valid Lua 5.1-5.4 source OUTSIDE
# strings and comments: whitespace, identifier/number characters, and the
# operator/punctuation set (+ - * / % ^ # = ~ < > ( ) { } [ ] ; : , . & |
# -- includes // << >> .. ... whose characters are all in this set)
LUA_LEXICAL_CHARS = set(
    " \t\r\n"
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
    "+-*/%^#=~<>(){}[];:,.&|")


def lua_syntax_error(text):
    """Heuristic syntax check for one Lua source (no full parser, no exec).

    Returns None when nothing suspicious is found, else (message, line) for
    the first problem: an unterminated short string / long string / block
    comment, a character outside Lua's lexical set (invalid token), or a
    block/function left open at EOF (or closed too often). Line numbers are
    1-based and refer to the original text.
    """
    n = len(text)
    if n == 0:
        return None

    line_starts = [0]
    for i, ch in enumerate(text):
        if ch == "\n":
            line_starts.append(i + 1)

    def line_of(off):
        return bisect.bisect_right(line_starts, off)

    # Lua skips a leading UTF-8 BOM and a "#!" (shebang) line
    i = 0
    if text.startswith("\ufeff"):
        i = 1
    if text.startswith("#!", i):
        j = text.find("\n", i)
        if j == -1:
            return None          # shebang-only file: nothing left to check
        i = j + 1
    skip_to = i                  # pass 2 must also ignore the BOM/shebang

    # -- pass 1: strings and comments; records unterminated ones ----------
    while i < n:
        c = text[i]
        if c == "-" and text.startswith("--", i):
            m = re.match(r"--\[(=*)\[", text[i:])
            if m:  # block comment --[[ ... ]] / --[=[ ... ]=]
                close = "]" + m.group(1) + "]"
                j = text.find(close, i + m.end())
                if j == -1:
                    return ("unterminated long comment", line_of(i))
                i = j + len(close)
            else:  # line comment (the newline itself stays for pass 2)
                j = text.find("\n", i)
                i = n if j == -1 else j
        elif c == '"' or c == "'":
            j = i + 1
            closed = False
            while j < n:
                cj = text[j]
                if cj == "\\":
                    if j + 1 < n and text[j + 1] == "z":
                        # Lua 5.2+ \z skips following whitespace incl. the
                        # newline, so the string may legally span lines
                        k = j + 2
                        while k < n and text[k] in " \t\r\n":
                            k += 1
                        j = k
                        continue
                    j += 2
                    continue
                if cj == c:
                    closed = True
                    break
                if cj == "\n":
                    # Lua short strings never span lines
                    return ("unterminated string", line_of(i))
                j += 1
            if not closed:
                return ("unterminated string", line_of(i))
            i = j + 1
        elif c == "[":
            m = re.match(r"\[(=*)\[", text[i:])
            if m:  # long string [[ ... ]] / [=[ ... ]=]
                close = "]" + m.group(1) + "]"
                j = text.find(close, i + m.end())
                if j == -1:
                    return ("unterminated long string", line_of(i))
                i = j + len(close)
            else:
                i += 1
        else:
            i += 1

    # -- pass 2 runs on the STRIPPED source (strings/comments blanked) -----
    stripped = strip_lua(text)

    # invalid tokens: a character Lua's lexer could never accept
    for k, ch in enumerate(stripped):
        if k < skip_to:
            continue
        if ch not in LUA_LEXICAL_CHARS:
            return ("invalid character %r" % ch, line_of(k))

    # block balance: function/if/for/while/do push, end/until pop; the "do"
    # of a for/while header belongs to that block, not a new one
    stack = []  # [kind, offset, pending_do]
    for m in IDENT_RE.finditer(stripped):
        w, off = m.group(0), m.start()
        if w == "function":
            stack.append(["function", off, False])
        elif w in ("if", "for", "while"):
            stack.append([w, off, True])
        elif w == "do":
            if stack and stack[-1][2]:
                stack[-1][2] = False
            else:
                stack.append(["do", off, False])
        elif w == "repeat":
            stack.append(["repeat", off, False])
        elif w == "until":
            if not stack or stack[-1][0] != "repeat":
                return ("'until' without a matching repeat", line_of(off))
            stack.pop()
        elif w == "end":
            if not stack:
                return ("'end' without a block to close", line_of(off))
            stack.pop()
    if stack:
        kind, off = stack[0][0], stack[0][1]
        extra = "" if len(stack) == 1 else " (+%d more)" % (len(stack) - 1)
        return ("unclosed '%s' block%s" % (kind, extra), line_of(off))
    return None


# --------------------------------------------------------------------------
# Catalog
# --------------------------------------------------------------------------

class Catalog(object):
    def __init__(self, entries, path, version=None):
        self.path = path
        self.version = version
        self.globals = {}
        self.types = {}
        self.methods = {}
        self.statics = {}
        self.unrecognized_statuses = []
        self.blank_status_count = 0
        for e in entries:
            if not isinstance(e, dict) or "name" not in e:
                continue
            name = e["name"]
            status = e.get("ps5_runtime_status")
            if is_blank_status(status):
                self.blank_status_count += 1
            elif status not in ALL_STATUSES:
                self.unrecognized_statuses.append((name, status))
            kind = e.get("kind")
            if not kind:
                if ":" in name:
                    kind = "method"
                elif "." in name:
                    kind = "static"
                elif isinstance(e.get("category"), str) and "type" in e["category"].lower():
                    kind = "type"
                else:
                    kind = "global"
            if kind == "method":
                owner = e.get("type") or name.split(":", 1)[0]
                meth = name.split(":", 1)[-1]
                self.methods[(owner, meth)] = e
                self.types.setdefault(owner, e)  # methods imply their owner type
            elif kind in ("static", "constant"):
                owner = e.get("type") or name.split(".", 1)[0]
                member = name.split(".", 1)[-1]
                self.statics[(owner, member)] = e
                self.types.setdefault(owner, e)
            elif kind == "type":
                self.types[name] = e
            else:
                self.globals[name] = e

    @classmethod
    def load(cls, path):
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        version = None
        if isinstance(data, dict):
            version = data.get("catalog_version")
            entries = data.get("entries", [])
        else:
            entries = data
        if not isinstance(entries, list):
            raise ValueError(
                "catalog must be a JSON array of API objects "
                "(or {catalog_version, entries}); got %s" % type(entries).__name__)
        return cls(entries, path, version)

    def lookup_global(self, name):
        if name in self.globals:
            return self.globals[name], None
        if name in self.types:  # type used as constructor: FName(...)
            return self.types[name], "type-constructor"
        return None, None

    def lookup_method(self, owner, meth):
        e = self.methods.get((owner, meth))
        if e:
            return e, None
        for b in base_chain(owner):
            e = self.methods.get((b, meth))
            if e:
                return e, b
        return None, None

    def lookup_static(self, owner, member):
        e = self.statics.get((owner, member))
        if e:
            return e, None
        # some runtimes expose statics with colon syntax; accept method entries
        e = self.methods.get((owner, member))
        if e:
            return e, "method-as-static"
        return None, None

    def lookup_type_ref(self, owner, member):
        if member:
            e, via = self.lookup_static(owner, member)
            if e:
                return e, via
        if owner in self.types:
            return self.types[owner], None
        return None, None

    def size(self):
        return (len(self.globals) + len(self.types) + len(self.methods)
                + len(self.statics))


# --------------------------------------------------------------------------
# Per-file Lua analysis (best-effort static, no execution)
# --------------------------------------------------------------------------

class LuaFile(object):
    def __init__(self, path, text, catalog):
        self.path = path
        self.text = text
        self.catalog = catalog
        self.stripped = strip_lua(text)
        self.line_starts = [0]
        for i, ch in enumerate(text):
            if ch == "\n":
                self.line_starts.append(i + 1)
        self.function_scopes = []
        self.local_funcs = set()
        self.local_names = set()
        self.local_tables = set()
        self.var_decls = {}  # name -> [ {offset, type, elem}, ... ]

    # -- helpers -----------------------------------------------------------

    def line_of(self, off):
        return bisect.bisect_right(self.line_starts, off)

    def snippet_at(self, off):
        line = self.line_of(off)
        lines = self.text.splitlines()
        if 0 < line <= len(lines):
            return lines[line - 1].strip()[:110]
        return ""

    def _matching_paren(self, open_idx):
        depth = 0
        for i in range(open_idx, len(self.stripped)):
            ch = self.stripped[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return i
        return None

    def _open_paren_after(self, pos):
        s = self.stripped
        i = pos
        while i < len(s):
            if s[i] in " \t\r\n":
                i += 1
            elif s[i] == "(":
                return i
            else:
                return None
        return None

    def _after_function_kw(self, ident_start):
        prefix = self.stripped[:ident_start].rstrip()
        return prefix.endswith("function")

    @staticmethod
    def _first_string(s):
        m = re.search(r'"([^"]*)"|\'([^\']*)\'', s)
        if m:
            return m.group(1) if m.group(1) is not None else m.group(2)
        return None

    # -- structure walk ------------------------------------------------------

    def parse(self):
        s = self.stripped
        stack = []
        for m in IDENT_RE.finditer(s):
            w, off = m.group(0), m.start()
            if w == "function":
                name, params, header_end = self._parse_function_header(off)
                stack.append({"kind": "function", "name": name, "start": off,
                              "end": None, "params": params, "handler": False,
                              "header_end": header_end})
                if name:
                    self.local_funcs.add(name)
                    self.local_names.add(name)
            elif w in ("if", "for", "while"):
                if w == "for":
                    self._type_for_loop(off)
                stack.append({"kind": "block",
                              "pending_do": w in ("for", "while"),
                              "start": off})
            elif w == "do":
                if stack and stack[-1].get("pending_do"):
                    stack[-1]["pending_do"] = False
                else:
                    stack.append({"kind": "block", "start": off})
            elif w == "repeat":
                stack.append({"kind": "repeat", "start": off})
            elif w in ("until", "end"):
                if stack:
                    top = stack.pop()
                    if top["kind"] == "function":
                        top["end"] = off
                        self.function_scopes.append(top)
            elif w == "local":
                self._parse_local(off)

    def _parse_function_header(self, off):
        seg = self.stripped[off: off + 400]
        m = re.match(
            r"function\s+([A-Za-z_]\w*(?:\s*[.:]\s*[A-Za-z_]\w*)*)?\s*\(", seg)
        name = re.sub(r"\s+", "", m.group(1)) if m and m.group(1) else None
        if not m:
            return name, [], off
        popen = off + m.end() - 1
        close = self._matching_paren(popen)
        if close is None:
            return name, [], off + m.end()
        params = [t for t in IDENT_RE.findall(self.stripped[popen + 1: close])
                  if t not in LUA_KEYWORDS]
        self.local_names.update(params)  # best-effort, per-file
        return name, params, close + 1

    def _parse_local(self, off):
        seg = self.stripped[off: off + 600]
        if re.match(r"local\s+function\b", seg):
            return  # handled by the 'function' keyword branch
        m = re.match(r"local\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)", seg)
        if not m:
            return
        names = [x.strip() for x in m.group(1).split(",")]
        for x in names:
            self.local_names.add(x)
        am = re.match(
            r"local\s+[A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*\s*=\s*([^\n]*)", seg)
        if not am:
            return
        rhs_s = am.group(1)
        rhs_start = off + am.start(1)
        rhs_o = self.text[rhs_start: rhs_start + len(rhs_s)]
        t, elem = self._infer_type(rhs_s, rhs_o)
        first = names[0]
        if t:
            self.var_decls.setdefault(first, []).append(
                {"offset": off, "type": t, "elem": elem})
        if rhs_s.lstrip().startswith("{"):
            self.local_tables.add(first)

    def _infer_type(self, rhs_s, rhs_o):
        m = re.match(r"\s*([A-Za-z_]\w*)\s*\(", rhs_s)
        if m:
            fn = m.group(1)
            if fn == "FindFirstOf":
                return (self._first_string(rhs_o) or "UObject"), None
            if fn == "FindAllOf":
                cls = self._first_string(rhs_o) or "UObject"
                return cls + "[]", cls
            if fn in FACTORY_RETURNS:
                return FACTORY_RETURNS[fn]
            if fn in self.catalog.types:
                return fn, None
            return None, None
        m = re.match(r"\s*([A-Za-z_]\w*)\s*\.\s*[A-Za-z_]\w*\s*\(", rhs_s)
        if m and m.group(1) in self.catalog.types:
            return m.group(1), None
        return None, None

    def _type_for_loop(self, off):
        seg = self.stripped[off: off + 400]
        nl = seg.find("\n")
        header = seg if nl == -1 else seg[:nl]
        m = re.match(
            r"for\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s+in\s+(.+)", header)
        if not m:
            return
        names = [x.strip() for x in m.group(1).split(",")]
        expr = m.group(2).strip()
        pm = re.match(r"i?pairs\s*\(\s*([A-Za-z_]\w*)\s*\)", expr)
        if not pm:
            return
        decls = self.var_decls.get(pm.group(1))
        if not decls:
            return
        elem = decls[-1].get("elem")
        if elem:
            target = names[-1] if names[-1] != "_" else names[0]
            self.var_decls.setdefault(target, []).append(
                {"offset": off, "type": elem, "elem": None})

    def _var_type(self, name, use_off):
        decls = self.var_decls.get(name)
        if not decls:
            return None
        best = None
        for d in decls:
            if d["offset"] <= use_off:
                best = d
        if best is None:
            best = decls[0]
        return best["type"]

    # -- call/reference extraction -------------------------------------------

    def extract(self):
        s = self.stripped
        calls = []
        static_spans = []
        for m in STATIC_CALL_RE.finditer(s):
            if self._after_function_kw(m.start(1)):
                continue
            static_spans.append((m.start(2), m.end()))
            calls.append({"kind": "static", "left": m.group(1),
                          "member": m.group(2), "offset": m.start()})
        for m in METHOD_CALL_RE.finditer(s):
            if self._after_function_kw(m.start(1)):
                continue
            calls.append({"kind": "method", "var": m.group(1),
                          "member": m.group(2), "offset": m.start()})
        for m in GLOBAL_CALL_RE.finditer(s):
            name = m.group(1)
            if name in LUA_KEYWORDS:
                continue
            if self._after_function_kw(m.start(1)):
                continue
            calls.append({"kind": "global", "name": name, "offset": m.start()})
        refs = []
        for m in TYPE_REF_RE.finditer(s):
            if self._after_function_kw(m.start(1)):
                continue
            if any(a <= m.start(2) < b for a, b in static_spans):
                continue  # already captured as a static call
            refs.append({"kind": "type-ref", "left": m.group(1),
                         "member": m.group(2), "offset": m.start()})
        return calls, refs

    def detect_handlers(self, calls):
        """Mark functions wired through registration calls as main code path."""
        for c in calls:
            if c["kind"] != "global":
                continue
            if c["name"] not in HIGH_SIGNAL_REGISTRATIONS:
                continue
            popen = self._open_paren_after(c["offset"] + len(c["name"]))
            if popen is None:
                continue
            close = self._matching_paren(popen)
            if close is None:
                continue
            inner = self.stripped[popen + 1: close]
            ids = {t for t in IDENT_RE.findall(inner)
                   if t not in LUA_KEYWORDS and t != c["name"]}
            named = {i for i in ids if i in self.local_funcs}
            for sc in self.function_scopes:
                base = sc["name"].split(":")[-1].split(".")[-1] if sc["name"] else None
                if base and base in named:
                    sc["handler"] = True
                if popen <= sc["start"] <= close:  # anon fn defined in the args
                    sc["handler"] = True

    def context_of(self, off):
        end_default = len(self.stripped)
        chain = [sc for sc in self.function_scopes
                 if sc["start"] <= off <= (sc["end"] if sc["end"] else end_default)]
        if not chain:
            return "top-level", True
        inner = max(chain, key=lambda sc: sc["start"])
        if any(sc["handler"] for sc in chain):
            return "handler (%s)" % (inner["name"] or "anonymous"), True
        return "function %s" % (inner["name"] or "anonymous"), False


# --------------------------------------------------------------------------
# Classification of every call/reference against the catalog
# --------------------------------------------------------------------------

def _looks_like_namespace(name):
    # UE-ish type/namespace (FName, UObject, ImGui, ModServices, EInputEvent)
    return bool(name) and name[0].isupper()


def classify_file(lf, catalog):
    calls, refs = lf.extract()
    lf.detect_handlers(calls)
    items = []

    def add(api, bucket, c, status=None, category=None, via=None, note=None,
            registration=False, entry=None):
        where, main_path = lf.context_of(c["offset"])
        items.append({
            "api": api, "bucket": bucket, "status": status,
            "category": category, "kind": c["kind"], "via": via, "note": note,
            "registration": registration, "entry": entry,
            "line": lf.line_of(c["offset"]),
            "snippet": lf.snippet_at(c["offset"]),
            "where": where, "main_path": main_path,
        })

    for c in calls + refs:
        if c["kind"] == "global":
            name = c["name"]
            if (name in lf.local_funcs or name in lf.local_names
                    or name in lf.local_tables):
                continue  # mod-internal function/variable
            e, via = catalog.lookup_global(name)
            if e:
                # Cataloged runtime API wins over the stdlib assumption:
                # UE4SS replaces some Lua builtins (print/dofile/loadfile) and the
                # real catalog registers them, so they must be graded
                # against the catalog (including an EMPTY status), not
                # silently skipped as "builtin".
                status = e.get("ps5_runtime_status")
                bucket = bucket_for_entry(e, status)
                add(name, bucket, c, status=status, category=e.get("category"),
                    via=via, registration=name in HIGH_SIGNAL_REGISTRATIONS,
                    entry=e)
            elif name in LUA_BUILTIN_GLOBALS or name in LUA_BUILTIN_TABLES:
                add(name, "builtin", c)
            else:
                add(name, "unknown", c, note="global not in catalog")

        elif c["kind"] == "method":
            var, meth = c["var"], c["member"]
            t = lf._var_type(var, c["offset"])
            if t and t.endswith("[]"):
                t = t[:-2]
            if not t:
                add("%s:%s" % (var, meth), "unresolved", c,
                    note="untyped variable (parameter/self); not attributable")
                continue
            if t in catalog.types:
                e, via = catalog.lookup_method(t, meth)
                if e:
                    status = e.get("ps5_runtime_status")
                    bucket = bucket_for_entry(e, status)
                    add(e["name"], bucket, c, status=status,
                        category=e.get("category"),
                        via=("on %s" % t), entry=e)
                else:
                    add("%s:%s" % (t, meth), "unknown", c,
                        note="method on runtime type %s not in catalog" % t)
            else:
                e, via = catalog.lookup_method(t, meth)
                if e:
                    status = e.get("ps5_runtime_status")
                    bucket = bucket_for_entry(e, status)
                    add(e["name"], bucket, c, status=status,
                        category=e.get("category"),
                        via=("on %s via base %s" % (t, via)) if via else ("on %s" % t),
                        entry=e)
                else:
                    add("%s:%s" % (t, meth), "unresolved", c,
                        note="game-class method not in catalog "
                             "(assumed game-side BP API, not a runtime API)")

        elif c["kind"] == "static":
            left, member = c["left"], c["member"]
            if left in LUA_BUILTIN_TABLES:
                add("%s.%s" % (left, member), "builtin", c)
                continue
            if left in lf.local_tables or left in lf.local_names or left == "self":
                continue  # mod-local namespace
            e, via = catalog.lookup_static(left, member)
            if e:
                status = e.get("ps5_runtime_status")
                bucket = bucket_for_entry(e, status)
                add(e["name"], bucket, c, status=status,
                    category=e.get("category"), via=via, entry=e)
            elif _looks_like_namespace(left):
                add("%s.%s" % (left, member), "unknown", c,
                    note="namespace/type %s not in catalog" % left)
            # lowercase non-local tables are treated as mod/framework-internal

        else:  # type-ref / constant reference (no call)
            left, member = c["left"], c["member"]
            if (left in LUA_BUILTIN_TABLES or left in lf.local_tables
                    or left in lf.local_names or left == "self"):
                continue
            e, via = catalog.lookup_type_ref(left, member)
            if e:
                status = e.get("ps5_runtime_status")
                bucket = bucket_for_entry(e, status)
                # Resolved through the owner TYPE entry (e.g. Key.F7 -> the
                # "Key" constant-table type): report the SPECIFIC constant
                # referenced, not just the table name.
                api = e["name"]
                if e.get("kind") == "type":
                    api = "%s.%s" % (left, member)
                add(api, bucket, c, status=status,
                    category=e.get("category"), via=via, entry=e)
            elif _looks_like_namespace(left):
                add("%s.%s" % (left, member), "unknown", c,
                    note="type/constant reference %s not in catalog" % left)
    return items


# --------------------------------------------------------------------------
# Aggregation, grading, report
# --------------------------------------------------------------------------

def collect_lua_files(path):
    if os.path.isfile(path):
        if path.endswith(".lua"):
            return [path]
        raise ValueError("not a .lua file: %s" % path)
    if os.path.isdir(path):
        out = []
        for root, dirs, files in os.walk(path):
            dirs.sort()
            for f in sorted(files):
                if f.endswith(".lua"):
                    out.append(os.path.join(root, f))
        if out:
            return out
        raise ValueError("no .lua files under %s" % path)
    raise ValueError("mod path not found: %s" % path)


def _entry_public(entry):
    if not isinstance(entry, dict):
        return None
    keys = ("name", "category", "ps5_runtime_status", "workaround", "notes")
    return {k: entry[k] for k in keys if k in entry}


def _pc_only_input_note(pc_only_input):
    """Shared note text for a PC-only KEYBOARD input dependency list."""
    return (
        "PC-only input dependency: %d keyboard keybind constant(s) used "
        "(%s); PS5 has no keyboard, so these keybinds need runtime/config "
        "controller mapping (see PS5Pad.ini)" % (
            sum(r["count"] for r in pc_only_input),
            ", ".join(r["api"] for r in pc_only_input[:6])
            + ("" if len(pc_only_input) <= 6 else
               " ... +%d more" % (len(pc_only_input) - 6))))


def _suggestion(rec):
    e = rec.get("entry") or {}
    if e.get("workaround"):
        return e["workaround"]
    if rec["bucket"] == "pc-only-input":
        return DEFAULT_SUGGESTIONS["pc-only-input"]
    if rec["bucket"] == "status-blank":
        return ("Catalog-side: fill in ps5_runtime_status for this entry "
                "(currently empty); compatibility cannot be graded until "
                "the catalog says whether the PS5 runtime has it.")
    if rec["bucket"] == "unknown":
        return DEFAULT_SUGGESTIONS["unknown"]
    if rec["bucket"] == "behaves-differently":
        return e.get("notes") or DEFAULT_SUGGESTIONS["behaves-differently"]
    return e.get("notes") or DEFAULT_SUGGESTIONS["missing"]


def scan(mod_path, catalog_path, max_sites=6):
    catalog = Catalog.load(catalog_path)
    files = collect_lua_files(mod_path)
    root = mod_path if os.path.isdir(mod_path) else os.path.dirname(os.path.abspath(mod_path))

    per_file = {}
    apis = {}
    total_lines = 0
    file_lines = {}
    parse_errors = []

    for path in files:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        nlines = len(text.splitlines())
        total_lines += nlines
        rel = os.path.relpath(path, root) if root else os.path.basename(path)
        file_lines[rel] = nlines

        # parse-failure detection (finding #15): a file that fails the
        # heuristic syntax check grades "does not parse" with the file and
        # line; its calls are NOT extracted (they would be garbage).
        syn = lua_syntax_error(text)
        if syn is not None:
            msg, line = syn
            parse_errors.append({"file": rel, "line": line, "error": msg})
            per_file[rel] = []
            continue

        lf = LuaFile(path, text, catalog)
        lf.parse()
        items = classify_file(lf, catalog)
        per_file[rel] = items
        for it in items:
            rec = apis.setdefault(it["api"], {
                "api": it["api"], "bucket": it["bucket"], "status": it["status"],
                "category": it["category"], "kind": it["kind"], "via": it["via"],
                "note": it["note"], "entry": it["entry"], "registration": False,
                "main_path_any": False, "count": 0, "sites": [],
            })
            rec["count"] += 1
            rec["registration"] = rec["registration"] or bool(it["registration"])
            rec["main_path_any"] = rec["main_path_any"] or bool(it["main_path"])
            rec["sites"].append({"file": rel, "line": it["line"],
                                 "snippet": it["snippet"], "where": it["where"],
                                 "main_path": it["main_path"]})

    for rec in apis.values():
        rec["central"] = (rec["count"] > 3 or rec["main_path_any"]
                          or rec["registration"])

    def count_bucket(b):
        return sum(r["count"] for r in apis.values() if r["bucket"] == b)

    matched_calls = sum(count_bucket(b) for b in
                        ("ok", "missing", "behaves-differently", "unknown-status",
                         "status-blank", PC_ONLY_INPUT_STATUS))
    unknown_calls = count_bucket("unknown")
    unresolved_calls = count_bucket("unresolved")
    builtin_calls = count_bucket("builtin")
    total_calls = matched_calls + unknown_calls + unresolved_calls

    missing = [r for r in apis.values() if r["bucket"] == "missing"]
    behaves = [r for r in apis.values() if r["bucket"] in
               ("behaves-differently", "unknown-status")]
    status_blank = [r for r in apis.values() if r["bucket"] == "status-blank"]
    pc_only_input = [r for r in apis.values() if r["bucket"] == PC_ONLY_INPUT_STATUS]
    unknown_apis = [r for r in apis.values() if r["bucket"] == "unknown"]
    unresolved_apis = [r for r in apis.values() if r["bucket"] == "unresolved"]
    central_missing = [r for r in missing if r["central"]]

    sort_key = lambda r: (not r["central"], -r["count"], r["api"])
    missing.sort(key=sort_key)
    behaves.sort(key=lambda r: (-r["count"], r["api"]))
    status_blank.sort(key=lambda r: (-r["count"], r["api"]))
    pc_only_input.sort(key=lambda r: (not r["central"], -r["count"], r["api"]))
    unknown_apis.sort(key=lambda r: (-r["count"], r["api"]))
    unresolved_apis.sort(key=lambda r: (-r["count"], r["api"]))

    notes = []
    grade_reason = None
    if parse_errors:
        # GRADE "does not parse" (finding #15): a file that fails the
        # heuristic Lua syntax check cannot be analyzed at all -- its call
        # extraction would be garbage. This outranks every other verdict.
        grade = "does not parse"
        first = parse_errors[0]
        more = ("" if len(parse_errors) == 1
                else " (+%d more file(s) with syntax errors)"
                % (len(parse_errors) - 1))
        grade_reason = ("does not parse - %s:%d: %s%s" % (
            first["file"], first["line"], first["error"], more))
        notes.append(
            "Lua syntax check failed for %d file(s) -- %s -- the file(s) "
            "were excluded from API scanning and no compatibility verdict "
            "is possible until the source parses" % (
                len(parse_errors),
                "; ".join("%s:%d (%s)" % (e["file"], e["line"], e["error"])
                          for e in parse_errors[:6])
                + ("" if len(parse_errors) <= 6 else
                   " ... +%d more" % (len(parse_errors) - 6))))
    elif status_blank:
        # GRADE POLICY (Claude round-5 review): when ANY API the mod calls
        # has a blank ps5_runtime_status, NO verdict is possible -- the
        # grade is "unknown - runtime status not filled". This outranks
        # every other outcome, including the PC-only input dependency:
        # a confirmed keyboard difference is listed as a KNOWN PS5
        # DIFFERENCE inside this unknown result and does NOT upgrade the
        # grade to partly-compatible (it did before round 5; claiming
        # partly-compatible while core APIs are unverifiable overstated
        # what the catalog actually knows).
        grade = "unknown"
        notes.append(
            "runtime status not filled: %d cataloged API(s) this mod calls "
            "have an EMPTY ps5_runtime_status; compatibility cannot be "
            "verified until the catalog is filled in" % len(status_blank))
        if pc_only_input:
            notes.append(
                "KNOWN PS5 DIFFERENCE (listed within the unknown grade; does "
                "not upgrade it): %s" % _pc_only_input_note(pc_only_input))
    elif central_missing:
        grade = "unsupported"
    elif missing or behaves:
        grade = "partly-compatible"
    elif pc_only_input:
        # PC-only input dependency with NO blank statuses anywhere: the
        # APIs/constant tables EXIST and are cataloged (constants readable
        # even without a game), but they are PC keyboard semantics -- a PS5
        # difference needing translation, not a "missing" API and not an
        # unknown one.
        grade = "partly-compatible"
        notes.append(_pc_only_input_note(pc_only_input))
    elif unknown_apis:
        grade = "unknown"
    elif matched_calls == 0:
        grade = "unknown"
        notes.append("no catalog APIs detected; nothing to verify"
                     + (" (only Lua builtins used)" if builtin_calls else ""))
    else:
        grade = "compatible"
    if unresolved_apis:
        notes.append(
            "%d method call(s) on untyped variables could not be attributed "
            "statically; usually game-side methods, they do not count against "
            "the grade" % unresolved_calls)
    if builtin_calls:
        notes.append("Lua standard-library calls are assumed available and are "
                     "excluded from catalog matching (%d call sites)"
                     % builtin_calls)
    notes.append("type/constant references are counted as API usage")
    if catalog.blank_status_count:
        notes.append(
            "catalog has %d entries with an EMPTY ps5_runtime_status "
            "(unfilled); APIs the mod calls that resolve to them are listed "
            "under status_blank (or under pc_only_input when the entry is "
            "flagged as a PC-only input dependency) and make the grade "
            "unknown" % catalog.blank_status_count)
    if catalog.unrecognized_statuses:
        shown = catalog.unrecognized_statuses[:20]
        notes.append(
            "catalog entries with unrecognized (non-empty) ps5_runtime_status "
            "(treated as behaves-differently): %s%s" % (
                ", ".join("%s=%r" % (n, s) for n, s in shown),
                "" if len(catalog.unrecognized_statuses) <= 20 else
                " ... +%d more" % (len(catalog.unrecognized_statuses) - 20)))

    if grade_reason is None:
        grade_reason = GRADE_EXPLAIN[grade]
    if grade == "unknown" and status_blank:
        filled = "%d cataloged API(s) this mod calls have an empty " \
                 "ps5_runtime_status" % len(status_blank)
        if pc_only_input:
            # the keyboard difference rides along INSIDE the unknown result
            # as a known PS5 difference; it does not upgrade the grade
            pc_shown = ", ".join(r["api"] for r in pc_only_input[:3]) + (
                "" if len(pc_only_input) <= 3
                else " ... +%d more" % (len(pc_only_input) - 3))
            grade_reason = (
                "unknown - runtime status not filled (%s); KNOWN PS5 "
                "DIFFERENCE: PC-only input dependency - keyboard keybinds "
                "(%s) need runtime/config controller mapping on PS5 (PS5 "
                "has no keyboard; see PS5Pad.ini)" % (filled, pc_shown))
        elif unknown_apis:
            grade_reason = (
                "unknown - runtime status not filled (%s) and %d API(s) not "
                "in the catalog at all" % (filled, len(unknown_apis)))
        else:
            grade_reason = "unknown - runtime status not filled (%s)" % filled
    if grade == "partly-compatible" and pc_only_input and not (missing or behaves):
        pc_all = [r["api"] for r in pc_only_input]
        shown = ", ".join(pc_all[:3]) + ("" if len(pc_all) <= 3
                                         else " ... +%d more" % (len(pc_all) - 3))
        grade_reason = (
            "partly-compatible - PC-only input dependency: keyboard keybinds "
            "(%s) need runtime/config controller mapping on PS5 (PS5 has no "
            "keyboard; see PS5Pad.ini)" % shown)

    def api_out(rec):
        sites = rec["sites"][:max_sites]
        out = {
            "api": rec["api"], "status": rec["status"], "category": rec["category"],
            "kind": rec["kind"], "via": rec["via"], "note": rec["note"],
            "call_count": rec["count"], "central": rec["central"],
            "registration": rec["registration"],
            "call_sites": sites, "sites_total": len(rec["sites"]),
            "catalog_entry": _entry_public(rec["entry"]),
            "suggestion": _suggestion(rec),
        }
        return out

    # Known PS5 differences ride INSIDE the unknown result as informational
    # entries (they never form a separate grade). Full call-site detail for
    # these stays in the pc_only_input section.
    known_ps5_differences = []
    if grade == "unknown" and pc_only_input:
        for r in pc_only_input:
            known_ps5_differences.append({
                "kind": "pc-only-input",
                "api": r["api"],
                "difference": ("keyboard keybind constant; PS5 has no "
                               "keyboard -> needs runtime/config controller "
                               "mapping (see PS5Pad.ini)"),
            })

    details = {}
    for rel, items in per_file.items():
        agg = {}
        for it in items:
            a = agg.setdefault(it["api"], {
                "api": it["api"], "bucket": it["bucket"], "status": it["status"],
                "kind": it["kind"], "count": 0, "lines": []})
            a["count"] += 1
            a["lines"].append(it["line"])
        for a in agg.values():
            a["lines"] = a["lines"][:10]
        m = sum(a["count"] for a in agg.values() if a["bucket"] in
                ("ok", "missing", "behaves-differently", "unknown-status",
                 "status-blank", PC_ONLY_INPUT_STATUS))
        u = sum(a["count"] for a in agg.values() if a["bucket"] in
                ("unknown", "unresolved"))
        b = sum(a["count"] for a in agg.values() if a["bucket"] == "builtin")
        details[rel] = {
            "lines": file_lines.get(rel, 0),
            "total_calls": m + u, "matched_calls": m, "unmatched_calls": u,
            "builtin_calls": b,
            "apis": sorted(agg.values(), key=lambda a: (-a["count"], a["api"])),
        }
    for e in parse_errors:
        if e["file"] in details:
            details[e["file"]]["parse_error"] = "%s (line %d)" % (
                e["error"], e["line"])

    result = {
        "tool": "compat-checker 1.1",
        "mac_only": True,
        "host_environment": "macOS static host scan; no FTP, no payloads, "
                            "no launches, no console contact",
        "generated_at": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "mod_path": os.path.abspath(mod_path),
        "catalog": os.path.abspath(catalog_path),
        "catalog_version": catalog.version,
        "grade": grade,
        "grade_reason": grade_reason,
        "parse_errors": parse_errors,
        "total_calls": total_calls,
        "matched_calls": matched_calls,
        "unmatched_calls": unknown_calls + unresolved_calls,
        "unknown_calls": unknown_calls,
        "unresolved_calls": unresolved_calls,
        "builtin_calls": builtin_calls,
        "missing": [api_out(r) for r in missing],
        "behaves_differently": [api_out(r) for r in behaves],
        "pc_only_input": [api_out(r) for r in pc_only_input],
        "known_ps5_differences": known_ps5_differences,
        "status_blank": [api_out(r) for r in status_blank],
        "unknown": [{"api": r["api"], "kind": r["kind"], "note": r["note"],
                     "call_count": r["count"],
                     "call_sites": r["sites"][:max_sites],
                     "sites_total": len(r["sites"]),
                     "suggestion": _suggestion(r)} for r in unknown_apis],
        "unresolved_method_calls": [{"api": r["api"], "note": r["note"],
                                     "call_count": r["count"],
                                     "call_sites": r["sites"][:max_sites]}
                                    for r in unresolved_apis],
        "high_signal_registrations": [
            {"api": r["api"], "status": r["status"], "count": r["count"]}
            for r in apis.values() if r["registration"]],
        "details_per_file": details,
        "notes": notes,
    }
    return result


def render_summary(result):
    L = []
    bar = "=" * 74
    L.append(bar)
    L.append(" COMPATIBILITY CHECKER - static Lua scan")
    L.append(" Mac-only host tool. No FTP, no payloads, no launches, "
             "no console contact, no mod edits.")
    L.append(bar)
    L.append("Mod:     %s (%d file(s), %d lines)" % (
        result["mod_path"], len(result["details_per_file"]),
        sum(d["lines"] for d in result["details_per_file"].values())))
    L.append("Catalog: %s (%s)" % (os.path.basename(result["catalog"]),
                                   result.get("catalog_version") or "no version field"))
    L.append("")
    L.append("GRADE: %s" % result["grade"].upper())
    L.append("       %s" % result["grade_reason"])
    L.append("")

    if result.get("parse_errors"):
        L.append("PARSE ERRORS - heuristic syntax check failed; the file(s) "
                 "were NOT scanned:")
        for e in result["parse_errors"][:6]:
            L.append("  - %s:%d  %s" % (e["file"], e["line"], e["error"]))
        if len(result["parse_errors"]) > 6:
            L.append("  ... +%d more" % (len(result["parse_errors"]) - 6))
        L.append("")
    L.append("Calls:  total %d | catalog-matched %d | unmatched %d "
             "(unknown %d, unresolved %d) | lua-builtins %d" % (
                 result["total_calls"], result["matched_calls"],
                 result["unmatched_calls"], result["unknown_calls"],
                 result["unresolved_calls"], result["builtin_calls"]))

    def section(title, records, show_suggestion=True):
        L.append("")
        L.append(title)
        if not records:
            L.append("  (none)")
            return
        for i, r in enumerate(records, 1):
            central = ""
            if r.get("central") is True:
                central = " [CENTRAL]"
            elif r.get("central") is False:
                central = " [peripheral]"
            L.append("  [%d] %s (%s)%s - %d call site(s)" % (
                i, r["api"], r.get("status") or r.get("note") or "-",
                central, r["call_count"]))
            for s in r["call_sites"][:3]:
                L.append("        %s:%d  %s  {%s}" % (
                    s["file"], s["line"], (s["snippet"] or "")[:70], s["where"]))
            if r.get("sites_total", r["call_count"]) > 3:
                L.append("        ... +%d more site(s)" % (
                    r["sites_total"] - 3))
            if show_suggestion and r.get("suggestion"):
                s = r["suggestion"]
                label = "Runtime-side workaround"
                low = s.lower()
                if low.startswith("runtime-side:"):
                    s = s.split(":", 1)[1].strip()
                elif low.startswith("catalog-side:"):
                    s = s.split(":", 1)[1].strip()
                    label = "Catalog-side action"
                L.append("        %s: %s" % (label, s))

    missing = result["missing"]
    if missing:
        section("BLOCKERS - missing on PS5 AND central to the mod:", 
                [m for m in missing if m["central"]])
        section("RISKS - missing on PS5 but peripheral:", 
                [m for m in missing if not m["central"]])
    section("BEHAVES DIFFERENTLY on PS5:", result["behaves_differently"])
    if result.get("known_ps5_differences"):
        L.append("")
        L.append("KNOWN PS5 DIFFERENCES - confirmed differences listed WITHIN "
                 "the unknown grade (they do NOT upgrade it):")
        for d in result["known_ps5_differences"]:
            L.append("  - %s [%s]: %s" % (d["api"], d["kind"], d["difference"]))
    section("PC-ONLY INPUT - PS5 difference: exists but needs translation "
            "(keyboard keybinds -> runtime/config controller mapping, "
            "e.g. PS5Pad.ini):",
            result.get("pc_only_input", []))
    section("STATUS BLANK - in catalog but ps5_runtime_status not filled "
            "(unverifiable; grade is unknown):",
            result.get("status_blank", []))
    section("UNKNOWN - not in the runtime catalog:", result["unknown"])
    if result["unresolved_method_calls"]:
        L.append("")
        L.append("UNRESOLVED - not statically attributable (do not affect grade):")
        L.append("  " + ", ".join("%s (x%d)" % (u["api"], u["call_count"])
                                 for u in result["unresolved_method_calls"]))

    L.append("")
    L.append("Per-file:")
    for rel, d in result["details_per_file"].items():
        L.append("  %-40s %d calls (%d matched, %d unmatched, %d builtins)" % (
            rel, d["total_calls"], d["matched_calls"], d["unmatched_calls"],
            d["builtin_calls"]))
        if d.get("parse_error"):
            L.append("      ! DOES NOT PARSE: %s" % d["parse_error"])
    if result["notes"]:
        L.append("")
        L.append("Notes:")
        for n in result["notes"]:
            L.append("  - " + n)
    L.append("")
    L.append("Verdict: all workarounds above are RUNTIME-SIDE (bridge/catalog "
             "work).")
    L.append("This tool never edits, executes, or ships the mod, and performs "
             "zero console contact.")
    L.append(bar)
    return "\n".join(L)


def main(argv=None):
    p = argparse.ArgumentParser(
        description="Static Lua mod compatibility checker (Mac-only host tool).")
    p.add_argument("mod", help=".lua file or directory of .lua files")
    p.add_argument("catalog", help="API catalog JSON (array of API objects)")
    p.add_argument("--json", metavar="PATH", help="also write the JSON report here")
    p.add_argument("--format", choices=("text", "json"), default="text",
                   help="stdout format (default: human-readable text)")
    p.add_argument("--max-sites", type=int, default=6,
                   help="max call sites listed per API in JSON (default 6)")
    args = p.parse_args(argv)

    try:
        result = scan(args.mod, args.catalog, args.max_sites)
    except (OSError, ValueError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    except json.JSONDecodeError as e:
        print("error: catalog is not valid JSON: %s" % e, file=sys.stderr)
        return 2

    payload = json.dumps(result, indent=2)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            fh.write(payload + "\n")
    if args.format == "json":
        print(payload)
    else:
        print(render_summary(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
