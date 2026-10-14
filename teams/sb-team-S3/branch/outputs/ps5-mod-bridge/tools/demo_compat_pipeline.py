#!/usr/bin/env python3
"""End-to-end demo of the compatibility pipeline on a REAL mod bundle with
SYNTHETIC game data:

  modbridge analyze BUNDLE          -> Lua references (real, from the mod)
  tools/synthetic_utoc.py           -> fake .utoc listing the referenced assets
  synthetic reflection dump         -> fake class list for the referenced classes
  modbridge content-index ...       -> content index for console 02.003.000
  modbridge compat --index ...      -> verdict

The game data is invented to exercise the wiring; the verdict says nothing
about the real game. A real verdict needs the game's own .utoc files (and a
reflection dump for classes) fed to `modbridge content-index`.

    demo_compat_pipeline.py BUNDLE_DIR [--modbridge build/modbridge]
"""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent


def run(cmd):
    result = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"command failed: {cmd[:2]}\n{result.stderr}")
    return result.stdout


def lua_references(modbridge, bundle, work):
    inventory = work / "inventory.json"
    run([modbridge, "analyze", bundle, "--output", inventory])
    groups = {k: set() for k in ("assets", "classes", "functions", "short_class_names")}
    dynamic = False
    for entry in json.loads(inventory.read_text())["entries"]:
        facts = entry.get("facts", {})
        if "lua_references" in facts:
            for key in groups:
                groups[key] |= set(facts["lua_references"][key])
            dynamic |= facts["lua_references"]["dynamic"]
        elif "lua_references_unavailable" in facts:
            dynamic = True
    refs = {k: sorted(v) for k, v in groups.items()}
    refs["dynamic"] = dynamic
    return refs


def scenario(modbridge, work, name, refs, drop_asset):
    assets = [a for a in refs["assets"] if a != drop_asset]
    paths = ["Townfall/Content/" + a[len("/Game/"):] + ".uasset" for a in assets if a.startswith("/Game/")]
    utoc = work / f"{name}.utoc"
    run([sys.executable, HERE / "synthetic_utoc.py", utoc] + paths)
    # Short names resolve against indexed classes, so give each one a game class.
    classes = sorted(set(refs["classes"]) | {f"/Script/Townfall.{s}" for s in refs["short_class_names"]})
    dump = {"schema": "modbridge.reflection-dump/1", "platform": "ps5",
            "image": {"sha256": "0" * 64, "address_kind": "vaddr"}, "engine_version": "5.6",
            "functions": [{"name": f, "address": hex(0x1000 + 16 * i)} for i, f in enumerate(refs["functions"])],
            "classes": classes}
    (work / f"{name}.dump.json").write_text(json.dumps(dump))
    index = work / f"{name}.index.json"
    run([modbridge, "content-index", "--project", "Townfall", "--console-version", "02.003.000",
         "--utoc", utoc, "--reflection-dump", work / f"{name}.dump.json", "--complete", "--output", index])
    (work / "param.json").write_text('{"contentVersion":"02.003.000"}')
    (work / "files.json").write_text("[]")
    (work / "refs.json").write_text(json.dumps(refs))
    return json.loads(run([modbridge, "compat", "--profile", ROOT / "examples/game-profiles/PPSA33286.json",
                           "--index", index, "--param", work / "param.json", "--files", work / "files.json",
                           "--refs", work / "refs.json"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("bundle")
    parser.add_argument("--modbridge", default=str(ROOT / "build/modbridge"))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        work = pathlib.Path(tmp)
        refs = lua_references(args.modbridge, args.bundle, work)
        print("mod references:", {k: (len(v) if isinstance(v, list) else v) for k, v in refs.items()})
        cases = [("all_present", None)]
        if refs["assets"]:
            cases.append(("one_asset_missing", refs["assets"][-1]))
        for name, dropped in cases:
            result = scenario(args.modbridge, work, name, refs, dropped)
            content = result["content"]
            print(f"\n[{name}] verdict: {result['verdict']}  "
                  f"(present {content['present']} of {content['checked']})")
            for reason in result["reasons"]:
                print(f"  - {reason['code']}: {reason['detail']}")
            missing = [n for group in content["missing"].values() for n in group]
            if missing:
                print("  missing:", ", ".join(missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
