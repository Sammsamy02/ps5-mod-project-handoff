#!/usr/bin/env python3
"""Adversarial suite for compat-checker. Mac-only, host-tested."""

import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import CHECKER, CATALOG, CORPUS  # noqa: E402

LUA = os.path.join(CORPUS, "lua")
ENV = dict(os.environ, PYTHONIOENCODING="utf-8")


def run_checker(path, catalog=CATALOG, extra=()):
    p = subprocess.run(
        [sys.executable, CHECKER, path, catalog, "--format", "json", *extra],
        capture_output=True, text=True, timeout=180, env=ENV)
    try:
        doc = json.loads(p.stdout)
    except ValueError:
        doc = None
    return p.returncode, doc, p


def write(sb, name, text):
    p = os.path.join(sb.path, name)
    mode = "wb" if isinstance(text, bytes) else "w"
    with open(p, mode) as f:
        f.write(text)
    return p


def apis(doc):
    """All flagged API names (unknown/missing/behaves lists)."""
    out = {}
    for k in ("missing", "behaves_differently", "unknown"):
        for a in doc.get(k) or []:
            out[a["api"]] = a.get("bucket", k)
    return out


def register(h):
    # ------------------------------------------------------ syntax errors
    @h.add("compat.syntax_error_file", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "syntax_err.lua"))
        A.ok(rc in (0, 2), f"rc={rc} stderr={p.stderr[:150]}")
        if doc:
            A.ok(doc.get("grade"), "no grade emitted for broken file")
            print(f"    [observed] syntax-err file -> grade={doc.get('grade')} "
                  f"total_calls={doc.get('total_calls')}")

    @h.add("compat.syntax_error_late_file", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "syntax_err_late.lua"))
        A.ok(rc in (0, 2), f"rc={rc}")

    @h.add("compat.utf8_lookalike_syntax_error", "compat")
    def _(sb=None):
        # fullwidth parens are a syntax error in real Lua
        rc, doc, p = run_checker(os.path.join(LUA, "utf16_lookalike.lua"))
        A.ok(rc in (0, 2), f"rc={rc}")

    @h.add("compat.null_bytes", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "null_bytes.lua"))
        A.ok(rc in (0, 2), f"rc={rc}")

    # ----------------------------------------------- comment/string traps
    @h.add("compat.api_only_in_comments_ignored", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "api_comment_only.lua"))
        A.eq(rc, 0)
        flagged = apis(doc)
        A.ok("FindNextOf" not in flagged,
             "API mentioned only in comments counted as a call")
        A.eq(doc.get("total_calls"), 0,
             f"phantom calls from comments: {doc.get('total_calls')}")

    @h.add("compat.api_only_in_strings_ignored", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(
            os.path.join(LUA, "string_in_comments_trap.lua"))
        A.eq(rc, 0)
        flagged = apis(doc)
        A.ok("FindNextOf" not in flagged,
             "API name inside a string literal counted as a call")

    @h.add("compat.level2_block_comments_ignored", "compat")
    def _(sb=None):
        p = write(sb, "lvl.lua",
                  '--[==[\nFindFirstOf("Item")\n]==]\nLog("x")\n')
        rc, doc, _ = run_checker(p)
        A.eq(rc, 0)
        flagged = apis(doc)
        A.ok("FindFirstOf" not in flagged,
             "call inside --[==[ ]==] comment counted")
        A.eq(doc.get("total_calls"), 1,
             f"expected exactly the one real Log call, got "
             f"{doc.get('total_calls')}")

    # ------------------------------------------------------ obfuscation
    @h.add("compat.obfuscated_indexed_calls", "compat")
    def _(sb=None):
        """String-indexed API calls (_G['FindFirstOf'](...)) are invisible to
        the static analyzer. Acceptable as a documented limit IF the report
        surfaces the risk (e.g. notes dynamic access)."""
        rc, doc, p = run_checker(
            os.path.join(LUA, "obfuscated_indexed.lua"))
        A.ok(rc == 0, f"rc={rc}")
        flagged = apis(doc)
        detected = "FindFirstOf" in flagged
        flags_dynamic = ("dynamic" in json.dumps(doc).lower()
                         or "obfusc" in json.dumps(doc).lower())
        print(f"    [observed] _G-indexed call: detected={detected} "
              f"dynamic-flagged={flags_dynamic} grade={doc.get('grade')} "
              f"total_calls={doc.get('total_calls')}")

    @h.add("compat.loadstring_loader", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(
            os.path.join(LUA, "obfuscated_loadstring.lua"))
        A.ok(rc == 0, f"rc={rc}")
        print(f"    [observed] loadstring loader -> grade={doc.get('grade')}")

    @h.add("compat.charcode_dispatch", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(
            os.path.join(LUA, "obfuscated_charcode.lua"))
        A.ok(rc == 0, f"rc={rc}")

    @h.add("compat.dynamic_env_dispatch", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(
            os.path.join(LUA, "dynamic_dispatch_env.lua"))
        A.ok(rc == 0, f"rc={rc}")

    @h.add("compat.metamethod_bypass", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "metamethod_bypass.lua"))
        A.ok(rc == 0, f"rc={rc}")

    # --------------------------------------------------- structural abuse
    @h.add("compat.huge_single_line", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "huge_line.lua"))
        A.ok(rc == 0, f"rc={rc} {p.stderr[:120]}")

    @h.add("compat.deep_nesting", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "deep_nesting.lua"))
        A.ok(rc == 0, f"rc={rc} {p.stderr[:120]}")

    @h.add("compat.empty_and_comments_only", "compat")
    def _(sb=None):
        for f in ("empty.lua", "only_comments.lua"):
            rc, doc, p = run_checker(os.path.join(LUA, f))
            A.ok(rc in (0, 2), f"{f} rc={rc}")
            if rc == 0:
                A.ok(doc.get("grade"), f"{f}: no grade")

    @h.add("compat.shebang_and_crlf", "compat")
    def _(sb=None):
        for f in ("shebang.lua", "crlf.lua"):
            rc, doc, p = run_checker(os.path.join(LUA, f))
            A.ok(rc == 0, f"{f} rc={rc}")

    @h.add("compat.goto_spaghetti", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(LUA, "goto_spaghetti.lua"))
        A.ok(rc == 0, f"rc={rc}")

    # -------------------------------------------------------- catalogs
    @h.add("compat.catalog_edge_cases", "compat")
    def _(sb=None):
        good = write(sb, "m.lua", "local it = FindFirstOf('Item')\n")
        cat_bad = [
            ("empty", "[]"),
            ("wrapper", '{"catalog_version":"x","entries":[]}'),
            ("notjson", "nope"),
            ("nullentry", '[null, {"name":"FindFirstOf",'
             '"ps5_runtime_status":"implemented+console-proven"}]'),
            ("badstatus", '[{"name":"FindFirstOf","ps5_runtime_status":"yolo"}]'),
            ("wrongshape", '{"entries": 42}'),
        ]
        for label, body in cat_bad:
            cp = write(sb, f"cat_{label}.json", body)
            rc, doc, p = run_checker(good, catalog=cp)
            A.ok(rc in (0, 2), f"catalog {label}: rc={rc} "
                               f"{p.stderr[:120]}")
            if label == "badstatus" and rc == 0:
                # unrecognized status must degrade the report (grade or note)
                body = json.dumps(doc)
                A.ok(doc.get("grade") != "compatible" or
                     "unknown-status" in body or
                     doc.get("unmatched_calls", 0) > 0,
                     "unrecognized catalog status silently treated as ok")

    @h.add("compat.missing_paths", "compat")
    def _(sb=None):
        rc, doc, p = run_checker(os.path.join(sb.path, "nope.lua"))
        A.ok(rc != 0, "missing file silently accepted")
        rc2, doc2, p2 = run_checker(sb.path)  # empty dir
        A.ok(rc2 != 0, "empty dir silently accepted")

    # ------------------------------------------------ grade expectations
    @h.add("compat.grades_sane", "compat")
    def _(sb=None):
        cat = write(sb, "cat.json", json.dumps([
            {"name": "FindFirstOf", "category": "reflection",
             "ps5_runtime_status": "implemented+console-proven"},
            {"name": "LoadAsset", "category": "assets",
             "ps5_runtime_status": "missing"},
            {"name": "IterateGameDirectories", "category": "io",
             "ps5_runtime_status": "behaves-differently"},
        ]))
        # clean mod -> compatible (cataloged API only; Log is unknown->hmm,
        # use only cataloged implemented calls for the clean case)
        clean = write(sb, "clean.lua",
                      "local it = FindFirstOf('Item')\n")
        rc, doc, _ = run_checker(clean, catalog=cat)
        A.eq(rc, 0)
        A.contains([doc["grade"]], "compatible", f"grade={doc['grade']}")
        # peripheral missing call: inside a plain helper function, count<=3,
        # not top-level, not a handler -> partly-compatible
        periph = write(sb, "periph.lua",
                       "local it = FindFirstOf('Item')\n"
                       "local function helper()\n"
                       "  LoadAsset('/x')\n"
                       "end\n")
        rc2, doc2, _ = run_checker(periph, catalog=cat)
        A.contains([doc2["grade"]], "partly-compatible",
                   f"grade={doc2['grade']}")
        # central missing call (top-level) -> unsupported
        central = write(sb, "central.lua",
                        "local a = LoadAsset('/x')\nlocal b = LoadAsset('/y')\n"
                        "local c = LoadAsset('/z')\nlocal d = LoadAsset('/w')\n")
        rc3, doc3, _ = run_checker(central, catalog=cat)
        A.contains([doc3["grade"]], "unsupported", f"grade={doc3['grade']}")
        # uncataloged API -> unknown
        unc = write(sb, "unc.lua", "SomeWeirdAPI(1)\n")
        rc4, doc4, _ = run_checker(unc, catalog=cat)
        A.contains([doc4["grade"]], "unknown", f"grade={doc4['grade']}")
