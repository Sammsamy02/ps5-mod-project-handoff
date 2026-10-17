#!/usr/bin/env python3
"""Survey many downloaded mods: which UE4SS API does the PS5 Lua runtime lack, and how often?

    python3 -I tools/survey_mods.py PATH [PATH ...] [--runtime MANIFEST] [--json OUT]

Each PATH is a mod archive or an unpacked mod folder. A folder that holds only
archives and folders (no loose files, no Scripts folder) is a folder of mods and
is expanded one level.
Every mod is read by the real core (`modbridge analyze`, then
`modbridge lua-api --usage --runtime`), the same path the companion uses for a
job card. Nothing is executed or installed. Prints a Markdown report: one row
per mod, then the missing API names ranked by how many mods they block.
This is static API coverage, not a runtime test.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
from modbridge_reports import lua_api_union, lua_api_unavailable  # noqa: E402

ARCHIVES = {".zip", ".7z", ".rar", ".tar", ".gz", ".tgz", ".xz", ".bz2"}
DEFAULT_RUNTIME = PROJECT / "examples" / "runtimes" / "ps5-ue4ss-port.json"


def is_mod_folder(path):
    """A folder is one mod when it holds a non-archive file (main.lua, enabled.txt, ...) or a Scripts folder."""
    for child in path.iterdir():
        if child.name.startswith("."):
            continue
        if child.is_file() and child.suffix.lower() not in ARCHIVES:
            return True
        if child.is_dir() and child.name.lower() == "scripts":
            return True
    return False


def expand(paths):
    """Mods to survey: each archive or mod folder; a folder of mods is expanded one level, sorted."""
    mods = []
    for path in map(Path, paths):
        if path.is_file():
            mods.append(path)
        elif path.is_dir() and not is_mod_folder(path):
            mods.extend(sorted(child for child in path.iterdir() if not child.name.startswith(".")
                               and (child.is_dir() or child.suffix.lower() in ARCHIVES)))
        elif path.is_dir():
            mods.append(path)
        else:
            raise SystemExit(f"survey: not a file or folder: {path}")
    return mods


def core(binary, *arguments):
    result = subprocess.run([str(binary), *map(str, arguments)], capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        try:
            code = json.loads(result.stderr)["error"]["code"]
        except (ValueError, KeyError, TypeError):
            code = "operation_failed"
        raise RuntimeError(code)
    return result.stdout


def survey_one(binary, runtime, mod, scratch):
    """One row: {"mod","lua_files","status","missing","partial","missing_modules","uses","error"}."""
    row = {"mod": mod.name, "lua_files": 0, "status": None, "missing": [], "partial": [],
           "missing_modules": [], "uses": {}, "error": None}
    inventory_path = scratch / "inventory.json"
    try:
        core(binary, "analyze", mod, "--output", inventory_path)
        inventory = json.loads(inventory_path.read_text())
    except (RuntimeError, OSError, ValueError) as error:
        row["status"], row["error"] = "unreadable", str(error) or "operation_failed"
        return row
    if inventory.get("inventory_complete") is not True:
        # Part of the package could not be read: its scripts' usage would be understated, so no verdict.
        issues = [issue.get("code") for issue in inventory.get("issues", []) if isinstance(issue, dict)]
        row["status"], row["error"] = "unreadable", next((code for code in issues if isinstance(code, str)), "inventory_incomplete")
        return row
    row["lua_files"] = sum(1 for entry in inventory.get("entries", []) if isinstance(entry, dict) and entry.get("kind") == "lua")
    usage = lua_api_union(inventory)
    if usage is None:
        row["status"] = "no_lua"
        return row
    if lua_api_unavailable(usage):
        row["status"], row["error"] = "unknown", "usage_unavailable"
        return row
    row["uses"] = usage["functions"]
    usage_path = scratch / "usage.json"
    usage_path.write_text(json.dumps(usage))
    try:
        coverage = json.loads(core(binary, "lua-api", "--usage", usage_path, "--runtime", runtime))
    except (RuntimeError, ValueError) as error:
        row["status"], row["error"] = "unknown", str(error) or "assessment_failed"
        return row
    row["status"] = coverage["status"]
    for key in ("missing", "partial", "missing_modules"):
        row[key] = coverage[key]
    row["console_wired"] = coverage["console_wired"]
    return row


def rank(rows):
    """Missing API names by mods blocked (then total uses, then name)."""
    blocked, uses = {}, {}
    for row in rows:
        for name in row["missing"]:
            blocked[name] = blocked.get(name, 0) + 1
            uses[name] = uses.get(name, 0) + row["uses"].get(name, 0)
    return [{"name": name, "mods_blocked": blocked[name], "uses": uses[name]}
            for name in sorted(blocked, key=lambda name: (-blocked[name], -uses[name], name))]


def markdown(rows, ranking, runtime_label):
    checked = [row for row in rows if row["status"] in ("covered", "partial", "blocked")]
    counts = {}
    for row in rows:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    lines = [f"# Lua runtime survey ({runtime_label})", "",
             "Static API coverage from reading each mod's scripts; not a runtime test.", "",
             "Mods: " + ", ".join(f"{status} {count}" for status, count in sorted(counts.items())), "",
             "| Mod | Lua files | Status | Missing |", "|---|---|---|---|"]
    for row in rows:
        detail = ", ".join(row["missing"] + [m + " (module)" for m in row["missing_modules"]]) or (row["error"] or "")
        lines.append(f"| {row['mod']} | {row['lua_files']} | {row['status']} | {detail} |")
    if ranking:
        lines += ["", "## Missing API by mods blocked", "", "| API | Mods blocked | Uses |", "|---|---|---|"]
        lines += [f"| `{item['name']}` | {item['mods_blocked']} of {len(checked)} checked | {item['uses']} |" for item in ranking]
    if any(row.get("console_wired") is False for row in rows):
        lines += ["", "The runtime manifest says the Lua runtime is not running on the console yet."]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="+", help="mod archives, mod folders, or folders of them")
    parser.add_argument("--runtime", type=Path, default=DEFAULT_RUNTIME, help="modbridge.lua-runtime/1 manifest")
    parser.add_argument("--binary", type=Path, default=PROJECT / "build" / "modbridge")
    parser.add_argument("--json", type=Path, help="also write the rows and ranking as JSON here")
    args = parser.parse_args(argv)
    if not args.binary.is_file():
        raise SystemExit("survey: build the core first (make all)")
    runtime_label = json.loads(args.runtime.read_text()).get("runtime", "runtime")
    rows = []
    with tempfile.TemporaryDirectory(prefix="modbridge-survey-") as scratch:
        for mod in expand(args.paths):
            rows.append(survey_one(args.binary, args.runtime, mod, Path(scratch)))
    ranking = rank(rows)
    if args.json:
        args.json.write_text(json.dumps({"rows": rows, "ranking": ranking}, indent=2, sort_keys=True) + "\n")
    sys.stdout.write(markdown(rows, ranking, runtime_label))
    return 0


if __name__ == "__main__":
    sys.exit(main())
