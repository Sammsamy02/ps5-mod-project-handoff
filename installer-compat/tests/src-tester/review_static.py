#!/usr/bin/env python3
"""Static adversarial review of sibling modules.

Runs per-module static checks:
  - entrypoint discovery + rough interface report
  - Lua syntax validation (lua_syntax.check, the luac -p equivalent)
  - danger-pattern scan (network/console contact, shell=True, eval/exec,
    path escapes, unbounded rmtree, yaml.load, pickle...)
  - size/complexity stats

Mac-only; reads modules, writes findings to results/static_<module>.json.
Never edits module files.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import MODULES, module_dir, module_files  # noqa: E402
import lua_syntax  # noqa: E402

RESULTS = "/tmp/sideproject/tester/results"

DANGER = [
    ("net-socket", re.compile(r"\bimport\s+socket|\bsocket\.socket\(|urllib\.request|ftplib|import\s+ftplib", re.I),
     "network/console contact in Mac-only module"),
    ("http", re.compile(r"\brequests\.(get|post|put)|httpx\.|aiohttp|urlopen\(", re.I),
     "outbound HTTP from Mac-only module"),
    ("shell-true", re.compile(r"shell\s*=\s*True"),
     "subprocess shell=True (injection risk)"),
    ("eval", re.compile(r"^\s*(eval|exec)\s*\(", re.M),
     "eval/exec use"),
    ("pickle", re.compile(r"\bpickle\.loads?\(|yaml\.load\((?!\s*Loader)"),
     "unsafe deserialization"),
    ("rm-rf-abs", re.compile(r"rmtree\(\s*[\"']/"),
     "rmtree on absolute path literal"),
    ("chmod-777", re.compile(r"chmod\([^)]*0o777|chmod\([^)]*'777"),
     "world-writable permissions"),
    ("ftp-payload", re.compile(r"\bftp\b|\bpayload\b|eboot|\.sprx|\.self\b", re.I),
     "console/payload references (must not appear in Mac-only code)"),
]

PY_OK_EXTENSIONS = (".py",)
LUA_OK_EXTENSIONS = (".lua",)


def review_module(name):
    d = module_dir(name)
    rep = {"module": name, "exists": os.path.isdir(d), "files": [],
           "lua_syntax": [], "danger": [], "entrypoints": [], "stats": {}}
    if not rep["exists"]:
        return rep
    files = module_files(name)
    rep["files"] = files
    n_py = n_lua = n_doc = 0
    total_loc = 0
    for rel in files:
        p = os.path.join(d, rel)
        ext = os.path.splitext(rel)[1].lower()
        try:
            with open(p, "rb") as f:
                raw = f.read()
        except OSError as e:
            rep["danger"].append({"file": rel, "tag": "unreadable",
                                  "detail": str(e)})
            continue
        if ext in PY_OK_EXTENSIONS or ext in (".json", ".txt", ".md", ".sh"):
            n_py += 1 if ext == ".py" else 0
            text = raw.decode("utf-8", errors="replace")
            total_loc += text.count("\n")
            for tag, rx, why in DANGER:
                for m in rx.finditer(text):
                    ln = text.count("\n", 0, m.start()) + 1
                    rep["danger"].append({
                        "file": rel, "line": ln, "tag": tag,
                        "detail": f"{why}: ...{text[max(0,m.start()-30):m.end()+30]!r}..."
                    })
        if ext in LUA_OK_EXTENSIONS:
            n_lua += 1
            text = raw.decode("utf-8", errors="replace")
            ok, err = lua_syntax.check(text)
            rep["lua_syntax"].append({"file": rel, "ok": ok, "error": err})
            total_loc += text.count("\n")
        if ext in (".md", ".txt"):
            n_doc += 1
    for rel in files:
        base = os.path.basename(rel)
        if base in ("main.py", "cli.py", "__main__.py", "run.py", "app.py",
                    "install.py", "uninstall.py", "check.py", "detect.py"):
            rep["entrypoints"].append(rel)
    rep["stats"] = {"python_files": n_py, "lua_files": n_lua, "doc_files": n_doc,
                    "approx_loc": total_loc, "total_files": len(files)}
    return rep


def main():
    os.makedirs(RESULTS, exist_ok=True)
    targets = sys.argv[1:] or MODULES
    for name in targets:
        rep = review_module(name)
        out = os.path.join(RESULTS, f"static_{name}.json")
        with open(out, "w") as f:
            json.dump(rep, f, indent=1)
        s = rep["stats"]
        print(f"{name}: files={s.get('total_files', 0)} "
              f"py={s.get('python_files', 0)} lua={s.get('lua_files', 0)} "
              f"loc~{s.get('approx_loc', 0)} "
              f"lua_errs={sum(1 for r in rep['lua_syntax'] if not r['ok'])} "
              f"danger={len(rep['danger'])}")
        for dng in rep["danger"][:12]:
            print(f"    ! [{dng['tag']}] {dng.get('file')}:{dng.get('line','')} "
                  f"{dng.get('detail','')[:110]}")


if __name__ == "__main__":
    main()
