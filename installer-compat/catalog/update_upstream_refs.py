#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""update_upstream_refs.py -- replace "upstream reference: TBD" in the REAL catalog
with actual upstream UE4SS 3.0.1 file:line citations.

MAC-ONLY HOST TOOL (static source analysis only -- no FTP, no payloads, no
launches, no console contact). Reads read-only:
    the vendored upstream UE4SS 3.0.1 tree (version.cache 3.0.1.0.0)
    the PS5/Prospero port tree (only to resolve port-side line citations)
Writes (json read/write, no manual text editing):
    ./globals.json   (entries gain "upstream_reference"; notes TBD replaced)
    ./types.json     (entries gain "upstream_source_file"; every method/metamethod
                      gains "upstream_implementation"; notes TBD replaced)
Both files are re-wrapped as {catalog_version, ue4ss_version, ue4ss_source,
citation_tree, entries} objects.

Citation resolution rules (documented for provenance):
  * Globals: comment-aware scan (/* */ blocks and // lines excluded) of upstream
    src/Mod/LuaMod.cpp + src/Signatures.cpp for live register_function("Name"),
    lua_setglobal(..., "Name") and make_global("Name") registrations; dotted
    names (Table.Member) resolve to the last live add_pair("Member") before the
    table's make_global("Table"). print/LoadExport/DerefToInt32 families also
    cite their LuaLibrary.cpp binding bodies. Names with NO upstream
    registration (verified: loadfile/dofile -- the port's native-source loader
    override) are marked PORT-ONLY ADDITION, never left TBD.
  * Types/methods: port implementation citations "file:line" are mapped to the
    upstream tree. 32/34 cited files are line-count identical to upstream
    (only trivial platform renames differ), so citations resolve by exact
    same-line content match, falling back to unique whole-file content match
    (the two divergent files: src/Mod/LuaMod.cpp, src/LuaType/LuaUObject.cpp).
    Unresolvable methods would be marked PORT-ONLY ADDITION (none were).
  * Port citations (registration_sites / source_file / implementation) are kept
    unchanged as the secondary reference; citation_tree metadata marks the new
    upstream_* fields as citing the upstream tree.
"""

from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UPSTREAM_ROOT = ("/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/"
                 "native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/"
                 "ue4ss/UE4SS")
PORT_ROOT = ("/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/"
             "native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss")
UPSTREAM_VERSION = "3.0.1"          # generated_src/version.cache = 3.0.1.0.0
CATALOG_VERSION = "2026-10-07-v2"

TBD_SENTENCE = "Source attribution: verified against port; upstream reference: TBD."


# --------------------------------------------------------------------------- #
# upstream source helpers
# --------------------------------------------------------------------------- #

def load_lines(root, rel):
    with open(os.path.join(root, rel), errors="replace") as fh:
        return fh.read().split("\n")


def live_lines(root, rel):
    """{line_no: text} for lines with live (non-comment) code."""
    src = open(os.path.join(root, rel), errors="replace").read()
    live, in_block = {}, False
    for i, line in enumerate(src.split("\n"), 1):
        out, j, n = [], 0, len(line)
        while j < n:
            if in_block:
                k = line.find("*/", j)
                if k == -1:
                    j = n
                else:
                    in_block = False
                    j = k + 2
            else:
                k, lc = line.find("/*", j), line.find("//", j)
                if lc != -1 and (k == -1 or lc < k):
                    out.append(line[j:lc])
                    break
                if k != -1:
                    out.append(line[j:k])
                    in_block = True
                    j = k + 2
                else:
                    out.append(line[j:])
                    break
        text = "".join(out)
        if text.strip():
            live[i] = text
    return live


FUNC_DEF = re.compile(
    r"^\s{0,8}auto\s+(?:static\s+)?([A-Za-z_:]+)\(|^\s{0,8}static\s+auto\s+([A-Za-z_:]+)\(")


def function_map(root, rel):
    """[(line, function_name)] for enclosing-function context."""
    out = []
    for i, line in enumerate(load_lines(root, rel), 1):
        m = FUNC_DEF.match(line)
        if m:
            out.append((i, m.group(1) or m.group(2)))
    return out


def enclosing(funcs, line):
    best = None
    for fl, name in funcs:
        if fl <= line:
            best = name
        else:
            break
    return best


def find_live(root, rel, pattern):
    rx = re.compile(pattern)
    return [(l, t) for l, t in live_lines(root, rel).items() if rx.search(t)]


# --------------------------------------------------------------------------- #
# globals: upstream registration index
# --------------------------------------------------------------------------- #

LUA_LIBRARY_BODIES = {  # supplementary binding-body citations
    "print": ("src/LuaLibrary.cpp", 34, "LuaLibrary::global_print binding body"),
    "LoadExport": ("src/LuaLibrary.cpp", 134, "LuaLibrary::load_export binding body"),
    "DerefToInt32": ("src/LuaLibrary.cpp", 111, "LuaLibrary::deref_to_int32 binding body"),
}


def index_global(name):
    """[(file, line, context)] upstream registration sites for a global name."""
    lm = "src/Mod/LuaMod.cpp"
    lm_funcs = function_map(UPSTREAM_ROOT, lm)
    hits = []
    if "." in name:
        table, member = name.split(".", 1)
        mg = find_live(UPSTREAM_ROOT, lm, r'make_global\(\s*"%s"\s*\)' % table)
        ap = find_live(UPSTREAM_ROOT, lm, r'add_pair\(\s*"%s"\s*,' % member)
        if mg and ap:
            mgl = mg[0][0]
            cands = [l for l, _ in ap if l < mgl]
            if cands:
                ctx = '%s table add_pair("%s") (table made global at :%d)' % (
                    enclosing(lm_funcs, cands[-1]) or "?", member, mgl)
                hits.append((lm, cands[-1], ctx))
        return hits
    for rel in (lm, "src/Signatures.cpp"):
        for l, _ in find_live(UPSTREAM_ROOT, rel,
                              r'register_function(?:<[^>]*>)?\(\s*"%s"\s*[,)]' % name):
            if rel == lm:
                ctx = "%s, register_function" % (enclosing(lm_funcs, l) or "?")
            else:
                ctx = "scan_from_lua_script signature-scan Lua state, register_function"
            hits.append((rel, l, ctx))
    for l, _ in find_live(UPSTREAM_ROOT, lm, r'lua_setglobal\([^)]*"%s"' % name):
        hits.append((lm, l, "%s, lua_setglobal" % (enclosing(lm_funcs, l) or "?")))
    for l, _ in find_live(UPSTREAM_ROOT, lm, r'make_global\(\s*"%s"\s*\)' % name):
        hits.append((lm, l, "%s, make_global" % (enclosing(lm_funcs, l) or "?")))
    body = LUA_LIBRARY_BODIES.get(name)
    if body:
        hits.append(body)
    # de-dup, keep file order then line order
    seen, uniq = set(), []
    for h in hits:
        key = (h[0], h[1])
        if key not in seen:
            seen.add(key)
            uniq.append(h)
    return uniq


# --------------------------------------------------------------------------- #
# types: port file:line -> upstream file:line
# --------------------------------------------------------------------------- #

def map_implementation(port_impl):
    """Resolve a port "file:line" citation to the upstream "file:line" (or None)."""
    rel, ln = port_impl.rsplit(":", 1)
    ln = int(ln)
    p_lines = load_lines(PORT_ROOT, rel)
    u_lines = load_lines(UPSTREAM_ROOT, rel)
    if ln <= len(p_lines) and ln <= len(u_lines) \
            and p_lines[ln - 1].strip() == u_lines[ln - 1].strip():
        return "%s:%d" % (rel, ln)
    want = p_lines[ln - 1].strip() if ln <= len(p_lines) else ""
    cands = [i + 1 for i, l in enumerate(u_lines) if l.strip() == want] if want else []
    if len(cands) == 1:
        return "%s:%d" % (rel, cands[0])
    return None


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #

def insert_after(d, anchor, key, value):
    """Rebuild dict so `key` lands right after `anchor` (or at the end)."""
    items = list(d.items())
    pos = [i for i, (k, _) in enumerate(items) if k == anchor]
    items.insert(pos[0] + 1 if pos else len(items), (key, value))
    d.clear()
    d.update(dict(items))


def rewrite_notes(notes, upstream_text):
    return notes.replace(TBD_SENTENCE,
                         "Source attribution: verified against port (port citations "
                         "retained as secondary); upstream reference: " + upstream_text)


def main():
    if not os.path.isdir(UPSTREAM_ROOT):
        raise SystemExit("error: upstream tree not found: %s" % UPSTREAM_ROOT)
    vc = load_lines(UPSTREAM_ROOT, "generated_src/version.cache")[0].strip()
    if not vc.startswith("3.0.1"):
        raise SystemExit("error: unexpected upstream version.cache: %s" % vc)

    metadata = {
        "catalog_version": CATALOG_VERSION,
        "ue4ss_version": UPSTREAM_VERSION,
        "ue4ss_source": UPSTREAM_ROOT + " (generated_src/version.cache %s)" % vc,
        "citation_tree": "upstream",
    }

    # ---- globals.json ----
    with open(os.path.join(HERE, "globals.json"), encoding="utf-8") as fh:
        globals_entries = json.load(fh)
    n_up, n_portonly = 0, 0
    for e in globals_entries:
        hits = index_global(e["name"])
        if hits:
            refs = ["%s:%d (%s)" % (f, l, ctx) for f, l, ctx in hits]
            insert_after(e, "registration_sites", "upstream_reference", refs)
            e["notes"] = rewrite_notes(e["notes"], "; ".join(refs))
            n_up += 1
        else:
            refs = ["PORT-ONLY ADDITION (no live registration for this name anywhere "
                    "in the vendored upstream UE4SS 3.0.1 tree; port adds it in "
                    "src/Mod/LuaMod.cpp setup_custom_module_loader)"]
            insert_after(e, "registration_sites", "upstream_reference", refs)
            e["notes"] = e["notes"].replace(
                TBD_SENTENCE,
                "Source attribution: PORT-ONLY ADDITION -- no upstream UE4SS 3.0.1 "
                "counterpart exists (port src/Mod/LuaMod.cpp native-source module "
                "loader override).")
            n_portonly += 1
    with open(os.path.join(HERE, "globals.json"), "w", encoding="utf-8") as fh:
        json.dump(dict(metadata, entries=globals_entries), fh, indent=2)
        fh.write("\n")
    print("globals.json: %d entries, %d with upstream citations, %d PORT-ONLY (%s)"
          % (len(globals_entries), n_up, n_portonly,
             ", ".join(e["name"] for e in globals_entries if
                       any("PORT-ONLY" in r for r in e["upstream_reference"]))))

    # ---- types.json ----
    with open(os.path.join(HERE, "types.json"), encoding="utf-8") as fh:
        types_entries = json.load(fh)
    n_exact = n_reloc = n_fail = 0
    failures = []
    for t in types_entries:
        sf = t.get("source_file", "")
        if sf and os.path.exists(os.path.join(UPSTREAM_ROOT, sf)):
            items = list(t.items())
            pos = [i for i, (k, _) in enumerate(items) if k == "source_file"]
            items.insert(pos[0] + 1 if pos else len(items),
                         ("upstream_source_file", sf))
            t.clear()
            t.update(dict(items))
        for m in t.get("methods", []) + t.get("metamethods", []):
            impl = m.get("implementation", "")
            if not impl:
                continue
            up = map_implementation(impl)
            if up:
                insert_after(m, "implementation", "upstream_implementation", up)
                if up == impl:
                    n_exact += 1
                else:
                    n_reloc += 1
            else:
                insert_after(
                    m, "implementation", "upstream_implementation",
                    "PORT-ONLY ADDITION (no content match in upstream %s)"
                    % impl.rsplit(":", 1)[0])
                n_fail += 1
                failures.append((t["type"], m.get("name"), impl))
        total = len(t.get("methods", [])) + len(t.get("metamethods", []))
        primary = t.get("upstream_source_file") or sf
        detail = ("%d/%d method citations resolved in the upstream tree"
                  % (sum(1 for m in t.get("methods", []) + t.get("metamethods", [])
                         if m.get("implementation")
                         and not m.get("upstream_implementation", "").startswith("PORT-ONLY")),
                      total))
        t["notes"] = rewrite_notes(
            t["notes"], "%s; %s (per-method citations in upstream_implementation)"
            % (primary, detail))
    with open(os.path.join(HERE, "types.json"), "w", encoding="utf-8") as fh:
        json.dump(dict(metadata, entries=types_entries), fh, indent=2)
        fh.write("\n")
    print("types.json: %d types; methods: %d exact same-line, %d relocated content "
          "match, %d PORT-ONLY" % (len(types_entries), n_exact, n_reloc, n_fail))
    for f in failures:
        print("  PORT-ONLY:", f)
    return 0


if __name__ == "__main__":
    sys.exit(main())
