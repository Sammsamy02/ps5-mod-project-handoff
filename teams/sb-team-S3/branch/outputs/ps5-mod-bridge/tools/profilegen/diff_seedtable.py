"""Field-by-field comparison of two ``SeedTable.h`` files (a hand-built or ``gen_seed_table.py`` one and ours).

Both files are parsed with the same tolerant line parser (namespace constants, the ``SlotSeed`` struct, the
slot initialisers whose derivation literal is C-unescaped and JSON-decoded), then compared in four groups:

* **value**      the same field holds a different value (labels, flags, constants, the image hash);
* **presence**   a field exists in only one file;
* **derivation** the derivation JSON of a slot differs (summarised: key sets, lengths, how the anchor bytes relate);
* **order/format** field order, header comments, guard, includes, namespace, escaping style, line endings.

``compare_profile`` renders our header from a profile for each GMalloc choice and compares each with the current
file; ``markdown`` turns the result into ``docs/profiles/SEEDTABLE-DIFF.md``.  Nothing here knows any game's value:
the "current" file is an argument, and the notes name the profile symbol whose label equals a differing value.

    python3 -m tools.profilegen.diff_seedtable --profile DIR/runtime-profile.json --current CURRENT/SeedTable.h \\
        --markdown docs/profiles/SEEDTABLE-DIFF.md
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

from . import emit_seedtable

_CONST_RX = re.compile(r"^\s*inline constexpr (\S+) (\w+) = (.+?);(?:\s*//\s?(.*))?\s*$")
_SLOT_RX = re.compile(r'^\s*inline constexpr SlotSeed (\w+)\{ (true|false), (true|false), (0x[0-9a-fA-F]+), '
                      r'"((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)" \}; // slot (\d+) \((\w+)\)\s*$')
_UCN_RX = re.compile(r"(?<!\\)(?:\\\\)*\\u[0-9a-fA-F]{4}")
_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "a": "\a", "b": "\b", "f": "\f", "v": "\v", "?": "?"}


def c_unescape(body: str) -> str:
    """The value of a C++ string literal body (simple, universal-character-name, hex and octal escapes)."""
    out: List[str] = []
    i = 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        n = body[i + 1] if i + 1 < len(body) else ""
        if n == "u":
            out.append(chr(int(body[i + 2:i + 6], 16)))
            i += 6
        elif n == "U":
            out.append(chr(int(body[i + 2:i + 10], 16)))
            i += 10
        elif n == "x":
            j = i + 2
            while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                j += 1
            out.append(chr(int(body[i + 2:j], 16)))
            i = j
        elif n in "01234567":
            j = i + 1
            while j < len(body) and j < i + 4 and body[j] in "01234567":
                j += 1
            out.append(chr(int(body[i + 1:j], 8)))
            i = j
        elif n in _ESCAPES:
            out.append(_ESCAPES[n])
            i += 2
        else:
            raise ValueError("unsupported escape \\%s" % n)
    return "".join(out)


class Parsed:
    """What the parser found in one header."""

    def __init__(self, text: str) -> None:
        self.text = text
        self.eol = "\r\n" if "\r\n" in text else "\n"
        self.final_newline = text.endswith("\n")
        self.lines = text.replace("\r\n", "\n").split("\n")
        if self.lines and self.lines[-1] == "":
            self.lines.pop()
        self.header_comments: List[str] = []
        self.guard: Optional[str] = None
        self.define: Optional[str] = None
        self.includes: List[str] = []
        self.namespace: Optional[str] = None
        self.consts: Dict[str, Dict[str, Any]] = {}
        self.slots: Dict[str, Dict[str, Any]] = {}
        self.struct_fields: List[str] = []
        self.order: List[str] = []                 # names in file order (constants, "SlotSeed", slot constants)
        self.namespace_comments: List[str] = []
        self.unrecognised: List[str] = []
        self.has_endif = False
        self.non_ascii_lines = [i + 1 for i, l in enumerate(self.lines) if any(ord(ch) > 127 for ch in l)]
        self.max_line = max((len(l) for l in self.lines), default=0)
        self._parse()

    def _parse(self) -> None:
        in_ns = False
        in_struct = False
        seen_guard = False
        for line in self.lines:
            s = line.strip()
            if not seen_guard and s.startswith("//"):
                self.header_comments.append(s)
                continue
            if s.startswith("#ifndef "):
                self.guard, seen_guard = s.split(None, 1)[1], True
            elif s.startswith("#define "):
                self.define = s.split(None, 1)[1]
            elif s.startswith("#include "):
                self.includes.append(s.split(None, 1)[1])
            elif s == "#endif":
                self.has_endif = True
            elif s.startswith("namespace "):
                self.namespace = s.split(None, 1)[1]
            elif s == "{" and not in_struct and self.namespace and not in_ns:
                in_ns = True
            elif in_struct:
                if s == "};":
                    in_struct = False
                elif s != "{":
                    self.struct_fields.append(s.rstrip(";"))
            elif s.startswith("struct SlotSeed"):
                in_struct = True
                self.order.append("SlotSeed")
            elif s == "}" and in_ns:
                in_ns = False
            elif s.startswith("//") and in_ns:
                self.namespace_comments.append(s)
            else:
                m = _SLOT_RX.match(line)
                if m:
                    const, present, data, label, name, body, slot_id, kind = m.groups()
                    raw = c_unescape(body)
                    try:
                        parsed = json.loads(raw) if raw else None
                    except ValueError:
                        parsed = None
                    self.slots[const] = {"present": present == "true", "data_kind": data == "true", "label": int(label, 16),
                                         "name": name, "derivation_literal": body, "derivation_text": raw,
                                         "derivation": parsed, "slot": int(slot_id), "kind": kind}
                    self.order.append(const)
                    continue
                m = _CONST_RX.match(line)
                if m:
                    ctype, name, value, comment = m.groups()
                    number = None
                    if re.fullmatch(r"0x[0-9a-fA-F]+", value):
                        number = int(value, 16)
                    self.consts[name] = {"type": ctype, "text": value, "number": number, "comment": comment or ""}
                    self.order.append(name)
                    continue
                if s and not s.startswith(("//", "#")):
                    self.unrecognised.append(s)


# ---------------------------------------------------------------------------------------------
# what a constant is, for the notes column (generic: the format's own names, no per-game value)
CONST_NOTES = {
    "kHeapLo": "retired heap envelope: the profile has no heap span, so the emitter writes an unbounded range (or --seed-heap-span)",
    "kHeapHi": "retired heap envelope: the profile has no heap span, so the emitter writes an unbounded range (or --seed-heap-span)",
    "kBaseObserved": "load.base of the profile",
    "kFNamePoolLabel": "symbols.name_pool of the profile",
    "kGMallocInstanceLabel": "symbols.gmalloc_instance of the profile",
    "kGMallocVtableLabel": "symbols.gmalloc_vtable of the profile",
}


class Difference:
    def __init__(self, group: str, field: str, current: str, emitted: str, note: str = "") -> None:
        self.group, self.field, self.current, self.emitted, self.note = group, field, current, emitted, note

    def as_dict(self) -> Dict[str, str]:
        return {"group": self.group, "field": self.field, "current": self.current, "emitted": self.emitted, "note": self.note}


def _short(v: Any, n: int = 70) -> str:
    s = str(v)
    return s if len(s) <= n else s[:n - 3] + "..."


def _names_for(doc: Optional[Dict[str, Any]], value: Optional[int]) -> List[str]:
    """Profile symbols (``symbols.<name>``) whose label equals ``value``."""
    if doc is None or value is None:
        return []
    out = []
    for name, node in (doc.get("symbols") or {}).items():
        env = node.get("label") if isinstance(node, dict) else None
        if isinstance(env, dict) and isinstance(env.get("value"), str):
            try:
                if int(env["value"], 16) == value:
                    out.append("symbols.%s" % name)
            except ValueError:
                pass
    return out


def _hex_prefix_relation(a: str, b: str) -> Optional[str]:
    if re.fullmatch(r"[0-9a-f]+", a or "") and re.fullmatch(r"[0-9a-f]+", b or ""):
        if a == b:
            return "identical bytes"
        if b.startswith(a):
            return "the emitted bytes extend the current ones (%d -> %d bytes, same leading %d bytes)" % (len(a) // 2, len(b) // 2, len(a) // 2)
        if a.startswith(b):
            return "the current bytes extend the emitted ones (%d -> %d bytes)" % (len(a) // 2, len(b) // 2)
        return "different bytes (%d vs %d bytes)" % (len(a) // 2, len(b) // 2)
    return None


def _derivation_summary(cur: Dict[str, Any], new: Dict[str, Any]) -> List[str]:
    c, n = cur.get("derivation"), new.get("derivation")
    out = []
    if not isinstance(c, dict) or not isinstance(n, dict):
        return ["derivation is %s in the current file and %s in the emitted one" % (
            "a JSON object" if isinstance(c, dict) else ("empty" if not cur.get("derivation_text") else "not JSON"),
            "a JSON object" if isinstance(n, dict) else ("empty" if not new.get("derivation_text") else "not JSON"))]
    out.append("keys: current %s; emitted %s" % (list(c), list(n)))
    only_c = [k for k in c if k not in n]
    only_n = [k for k in n if k not in c]
    if only_c:
        out.append("keys only in the current file: %s" % ", ".join(only_c))
    if only_n:
        out.append("keys only in the emitted file: %s" % ", ".join(only_n))
    for k in c:
        if k in n:
            if c[k] == n[k]:
                out.append("%s: identical text" % k)
            else:
                rel = _hex_prefix_relation(str(c[k]), str(n[k])) if k == "anchor_bytes" else None
                out.append("%s: text differs (%d vs %d characters)%s" % (k, len(str(c[k])), len(str(n[k])), "; " + rel if rel else ""))
    out.append("total derivation text: %d characters current, %d emitted" % (len(cur["derivation_text"]), len(new["derivation_text"])))
    return out


class Diff:
    """Everything that differs between a current header and an emitted one."""

    def __init__(self, current: Parsed, emitted: Parsed, doc: Optional[Dict[str, Any]] = None, model: Any = None) -> None:
        self.current, self.emitted, self.doc, self.model = current, emitted, doc, model
        self.differences: List[Difference] = []
        self.rows: List[Tuple[str, str, str, str]] = []              # (field, current, emitted, status)
        self.derivations: Dict[str, List[str]] = {}
        self._compare()

    # -- helpers -------------------------------------------------------------------------------
    def add(self, group: str, field: str, current: Any, emitted: Any, note: str = "") -> None:
        self.differences.append(Difference(group, field, str(current), str(emitted), note))

    def row(self, field: str, cur: Any, new: Any) -> None:
        status = "same" if cur == new else ("only in current" if new is None else ("only in emitted" if cur is None else "differs"))
        self.rows.append((field, "-" if cur is None else str(cur), "-" if new is None else str(new), status))

    def by_group(self, group: str) -> List[Difference]:
        return [d for d in self.differences if d.group == group]

    # -- comparison ----------------------------------------------------------------------------
    def _compare(self) -> None:
        c, e = self.current, self.emitted
        names = list(c.consts) + [n for n in e.consts if n not in c.consts]
        for n in names:
            cc, ee = c.consts.get(n), e.consts.get(n)
            self.row(n, cc["text"] if cc else None, ee["text"] if ee else None)
            if cc and ee:
                if cc["text"] != ee["text"] or cc["type"] != ee["type"]:
                    owners = _names_for(self.doc, cc["number"])
                    note = "current value equals the profile's %s" % ", ".join(owners) if owners else ""
                    note = "; ".join(x for x in (CONST_NOTES.get(n, ""), note) if x)
                    self.add("value", n, cc["text"], ee["text"], note)
                elif cc["comment"] != ee["comment"]:
                    self.add("order/format", n + " trailing comment", cc["comment"], ee["comment"])
            elif cc:
                why = next((x for x in (self.model.support_notes if self.model else []) if x.startswith(n + " is not emitted: ")), "")
                why = why.split(": ", 1)[1] if why else ""
                self.add("presence", n, cc["text"], "(not emitted)",
                         "constant exists in the current file only" + ("; the profile cannot supply it: " + why if why else ""))
            else:
                self.add("presence", n, "(absent)", ee["text"], "constant exists in the emitted file only")
        for n in list(c.slots) + [n for n in e.slots if n not in c.slots]:
            cs, es = c.slots.get(n), e.slots.get(n)
            if cs is None or es is None:
                self.row(n, "slot %d" % cs["slot"] if cs else None, "slot %d" % es["slot"] if es else None)
                self.add("presence", n, "slot present" if cs else "(absent)", "slot present" if es else "(not emitted)")
                continue
            for field in ("present", "data_kind", "label", "name"):
                a, b = cs[field], es[field]
                av = hex(a) if field == "label" else str(a).lower() if isinstance(a, bool) else a
                bv = hex(b) if field == "label" else str(b).lower() if isinstance(b, bool) else b
                self.row("%s.%s" % (n, field), av, bv)
                if a != b:
                    owners = _names_for(self.doc, a) if field == "label" else []
                    note = "current value equals the profile's %s" % ", ".join(owners) if owners else ""
                    self.add("value", "%s.%s" % (n, field), av, bv, note)
            for field in ("slot", "kind"):
                if cs[field] != es[field]:
                    self.add("order/format", "%s.%s" % (n, field), cs[field], es[field], "slot id / kind comment differs")
            same = cs["derivation_text"] == es["derivation_text"]
            self.row("%s.derivation" % n, "%d chars" % len(cs["derivation_text"]), "%d chars" % len(es["derivation_text"]))
            if not same:
                self.derivations[n] = _derivation_summary(cs, es)
                self.add("derivation", n + ".derivation", "%d chars" % len(cs["derivation_text"]),
                         "%d chars" % len(es["derivation_text"]), "text differs (see the summary)")
        # order ------------------------------------------------------------------------------------
        co, eo = [x for x in c.order if x in e.order], [x for x in e.order if x in c.order]
        self.row("(order of shared fields)", " ".join(co), " ".join(eo))
        if co != eo:
            self.add("order/format", "field order", " ".join(co), " ".join(eo), "shared fields appear in a different order")
        # format -----------------------------------------------------------------------------------
        for label, a, b in (("include guard", c.guard, e.guard), ("#define", c.define, e.define),
                            ("includes", c.includes, e.includes), ("namespace", c.namespace, e.namespace),
                            ("struct SlotSeed fields", c.struct_fields, e.struct_fields),
                            ("line ending", repr(c.eol), repr(e.eol)), ("final newline", c.final_newline, e.final_newline),
                            ("#endif present", c.has_endif, e.has_endif)):
            self.row("(format) " + label, a, b)
            if a != b:
                self.add("order/format", label, a, b)
        self.row("(format) header comment lines", len(c.header_comments), len(e.header_comments))
        if c.header_comments[:1] != e.header_comments[:1]:
            self.add("order/format", "header comment, line 1", c.header_comments[0] if c.header_comments else "(none)",
                     e.header_comments[0] if e.header_comments else "(none)", "names the producer")
        if c.header_comments[1:2] != e.header_comments[1:2]:
            self.add("order/format", "header comment, line 2", (c.header_comments + [""])[1], (e.header_comments + [""])[1])
        if len(c.header_comments) != len(e.header_comments):
            self.add("order/format", "header comment lines", len(c.header_comments), len(e.header_comments),
                     "extra provenance lines in the %s file" % ("emitted" if len(e.header_comments) > len(c.header_comments) else "current"))
        self.row("(format) comment lines inside the namespace", len(c.namespace_comments), len(e.namespace_comments))
        if len(c.namespace_comments) != len(e.namespace_comments):
            self.add("order/format", "comment lines inside the namespace", len(c.namespace_comments), len(e.namespace_comments),
                     "the emitted file explains each decision (heap span, base provenance, GMalloc choice) where it applies")
        cu = any(_UCN_RX.search(s["derivation_literal"]) for s in c.slots.values())
        eu = any(_UCN_RX.search(s["derivation_literal"]) for s in e.slots.values())
        self.row("(format) raw \\uXXXX sequences inside derivation literals", cu, eu)
        if cu != eu:
            self.add("order/format", "escaping of derivation literals",
                     "raw \\uXXXX inside the literals (the older generator escapes only the double quote, so C++ reads each as one "
                     "universal-character-name)" if cu else "no raw \\uXXXX",
                     "no raw \\uXXXX" if not eu else "raw \\uXXXX",
                     "the emitter escapes the backslash before the quote, so the compiled string is exactly the compact JSON text; "
                     "the profile text here is ASCII, so no \\uXXXX occurs")
        self.row("(format) non-ASCII lines", len(c.non_ascii_lines), len(e.non_ascii_lines))
        self.row("(format) longest line", c.max_line, e.max_line)
        if c.unrecognised or e.unrecognised:
            self.add("order/format", "lines the parser did not recognise", _short(c.unrecognised), _short(e.unrecognised))

    # -- numbers -------------------------------------------------------------------------------
    def counts(self) -> Dict[str, int]:
        return {g: len(self.by_group(g)) for g in ("value", "presence", "derivation", "order/format")}


def parse(text: str) -> Parsed:
    return Parsed(text)


def compare(current_text: str, emitted_text: str, doc: Optional[Dict[str, Any]] = None, model: Any = None) -> Diff:
    return Diff(Parsed(current_text), Parsed(emitted_text), doc, model)


def compare_profile(doc: Dict[str, Any], current_text: str, gmalloc: str = "dispatch", heap_span: Optional[str] = None) -> Tuple[Diff, str]:
    text, model = emit_seedtable.render_profile(doc, emit_seedtable.SeedOptions(gmalloc, heap_span))
    return compare(current_text, text, doc, model), text


# ---------------------------------------------------------------------------------------------
GROUP_TITLES = {
    "value": "Value differences",
    "presence": "Presence differences",
    "derivation": "Derivation-text differences, summarised",
    "order/format": "Ordering and format differences",
}


def _cell(v: str) -> str:
    return "`" + _short(v, 90).replace("`", "'").replace("|", "\\|") + "`" if v not in ("", "-") else "-"


def _table(diffs: List[Difference]) -> List[str]:
    lines = ["| # | field | current | emitted | note |", "|---|---|---|---|---|"]
    for i, d in enumerate(diffs, 1):
        lines.append("| %d | `%s` | %s | %s | %s |" % (i, d.field, _cell(d.current), _cell(d.emitted), d.note.replace("|", "\\|")))
    return lines


def markdown(doc: Dict[str, Any], current_path: str, current_text: str, results: Dict[str, Tuple[Diff, str]],
             command: str, extra_inputs: Optional[Dict[str, str]] = None) -> str:
    """The report.  ``results`` maps a variant name (``dispatch``/``base_slot``) to ``(diff, emitted_text)``."""
    sha = hashlib.sha256(current_text.encode("utf-8")).hexdigest()
    first = results["dispatch"][0]
    out: List[str] = []
    out += ["# SeedTable.h: emitted versus the runtime's current file", "",
            "Generated by `tools/profilegen/diff_seedtable.py` from a profile that `tools/profilegen` produced from the "
            "decrypted executable; do not edit by hand. Status: **generated**; nothing in this report has been observed on the console.", "",
            "## Inputs", ""]
    out += ["* current file (read only): `%s`, sha256 `%s`, %d bytes" % (current_path, sha, len(current_text.encode("utf-8")))]
    out += ["* profile: `%s`, digest `%s`" % (doc.get("profile_id"), str(doc.get("profile_digest"))[:16])]
    for k, v in (extra_inputs or {}).items():
        out += ["* %s: `%s`" % (k, v)]
    out += ["* command: `%s`" % command, "",
            "Emitted file: `emit_seedtable.py`, GMalloc variants `dispatch` (the generator's value, the default) and `base_slot` "
            "(`--seed-gmalloc base_slot`), heap span unbounded (the default).", "", "## Summary", "",
            "| group | default (`dispatch`) | `--seed-gmalloc base_slot` |", "|---|---|---|"]
    for g in GROUP_TITLES:
        out.append("| %s | %d | %d |" % (g, first.counts()[g], results["base_slot"][0].counts()[g] if "base_slot" in results else 0))
    out += ["", "Everything in the current file that the profile does not carry, and everything the emitter had to decide, is "
            "listed below; no difference is hidden in a default.", ""]
    for g, title in GROUP_TITLES.items():
        diffs = first.by_group(g)
        out += ["## %s, default `dispatch`" % title, ""]
        if not diffs:
            out += ["None.", ""]
            continue
        out += _table(diffs) + [""]
        if g == "derivation":
            for name, lines in first.derivations.items():
                out += ["**%s**" % name] + ["* " + l for l in lines] + [""]
    if "base_slot" in results:
        second = results["base_slot"][0]
        a = {(d.group, d.field): d for d in first.differences}
        b = {(d.group, d.field): d for d in second.differences}
        out += ["## With `--seed-gmalloc base_slot`", "",
                "Only the entries that change relative to the default; every other difference above is the same.", ""]
        changed = [d for k, d in b.items() if k not in a or a[k].current != d.current or a[k].emitted != d.emitted]
        gone = [d for k, d in a.items() if k not in b]
        rows = changed + [Difference(d.group, d.field, d.current, d.emitted + "  (equal under base_slot)", "no longer differs") for d in gone]
        if rows:
            out += _table(rows) + [""]
        else:
            out += ["No change.", ""]
    out += ["## Field-by-field (default `dispatch`)", "", "| field | current | emitted | status |", "|---|---|---|---|"]
    for field, cur, new, status in first.rows:
        out.append("| `%s` | %s | %s | %s |" % (field.replace("|", "\\|"), _cell(cur), _cell(new), status))
    out += ["", "## Regenerating this report", "", "```sh",
            "python3 -m tools.profilegen --eboot <decrypted-eboot.bin> --param <param.json> --game-profiles examples/game-profiles \\",
            "    --out <profile dir> --emit-seedtable <profile dir>/runtime/SeedTable.h        # add --seed-gmalloc base_slot for the other choice",
            "python3 -m tools.profilegen.diff_seedtable --profile <profile dir>/runtime-profile.json \\",
            "    --current <the runtime's SeedTable.h> --markdown docs/profiles/SEEDTABLE-DIFF.md", "```", "",
            "## Reading the differences", "",
            "* The current file's GMalloc seed is the raw allocator pointer the lazy-create function fetches "
            "(`symbols.gmalloc_base_slot`); the generator's value is the variable `FMemory::Malloc` and `FMemory::Free` load. "
            "Which one the runtime should seed is a console question; the emitter makes either with `--seed-gmalloc`.",
            "* `kHeapLo`/`kHeapHi` belong to the retired heap envelope; the profile has no heap span, so the default is an "
            "unbounded range (any non-zero `Blocks[0]` passes) and `--seed-heap-span omit` drops the constants.",
            "* Constants the current file has and the profile cannot supply are not invented: they are left out with a comment. "
            "A runtime build that still reads one (the staged unit's V1/V2 validators read `kGMallocInstanceLabel` and "
            "`kGMallocVtableLabel`, its pool check reads `kHeapLo`/`kHeapHi`) fails to compile until the constant exists, which is "
            "loud by design.",
            "* With `--seed-gmalloc dispatch` the V1 validator (`[GMalloc] == instance`) was written for the `base_slot` variable; "
            "the dispatch variable may hold a wrapper over the instance, so V1 may refuse that table at run time.", ""]
    return "\n".join(out)


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.profilegen.diff_seedtable", description=__doc__.split("\n")[0])
    ap.add_argument("--profile", required=True, help="runtime-profile.json")
    ap.add_argument("--current", required=True, help="the SeedTable.h to compare with (read only)")
    ap.add_argument("--markdown", help="write the report here (docs/profiles/SEEDTABLE-DIFF.md)")
    ap.add_argument("--command", default="", help="text recorded in the report as the command that produced the profile")
    ap.add_argument("--input", action="append", default=[], help="NAME=VALUE extra input line for the report (repeatable)")
    ns = ap.parse_args(argv)
    try:
        with open(ns.profile, encoding="utf-8") as fh:
            doc = json.load(fh)
        with open(ns.current, "rb") as fh:
            cur = fh.read().decode("utf-8")
        results = {c: compare_profile(doc, cur, c) for c in emit_seedtable.GMALLOC_CHOICES}
    except emit_seedtable.SeedTableRefused as exc:
        print("diff_seedtable: refused: %s" % exc, file=sys.stderr)
        return 4
    except (OSError, ValueError) as exc:
        print("diff_seedtable: %s" % exc, file=sys.stderr)
        return 2
    extra = dict(i.split("=", 1) for i in ns.input if "=" in i)
    report = markdown(doc, ns.current, cur, results, ns.command or "(not recorded)", extra)
    if ns.markdown:
        with open(ns.markdown, "w", encoding="utf-8") as fh:
            fh.write(report)
            if not report.endswith("\n"):
                fh.write("\n")
    else:
        print(report)
    for name, (d, _t) in results.items():
        print("diff_seedtable: %s: %s" % (name, d.counts()), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
