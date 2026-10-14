#!/usr/bin/env python3
"""worker-r3-diffs-authoring: compile gate for DIFF 1 + DIFF 2 against the REAL
composition unit argvs (compile-recipes.json) with the real PS5 cross-toolchain.
Compiles baseline and patched copies of each unit, plus -Werror variants of both.
Writes ONLY under this worker's output dir."""
import json, os, subprocess, sys, time
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
ME = W / "outputs/2026-10-04-zcode-team/worker-r3-diffs-authoring"
RECIPE = W / ".superpowers/native-dependency-spike/native-coherent-runtime-1/compile-recipes.json"
OWNER_INCLUDE = W / ".superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include"
STAGING_INCLUDE = W / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/include"

units = {u["name"]: u for u in json.loads(RECIPE.read_text())["units"]}
LLVM_BIN = None
for a in units["luamod"]["argv"]:
    if "LLVM-21" in a:
        LLVM_BIN = str(Path(a.replace("@ROOT@", str(W))).parent)
        break
ENV = dict(os.environ, LLVM_CONFIG=units["luamod"]["LLVM_CONFIG"].replace("@ROOT@", str(W)), PATH=f"{LLVM_BIN}:{os.environ['PATH']}")

def expand(v, out):
    if isinstance(v, str): return v.replace("@ROOT@", str(W)).replace("@OUTPUT@", str(out))
    if isinstance(v, list): return [expand(x, out) for x in v]
    return v

def compile_variant(unit_name, source, tag, extra_flags, extra_includes=()):
    unit = units[unit_name]
    out = ME / "work/gate" / f"{unit_name}-{tag}"
    out.mkdir(parents=True, exist_ok=True)
    argv = expand(unit["argv"], out)
    # both recipe layouts exist: luamod = <source> -c -o <obj>, initializer = -c <source> -o <obj>
    src_arg = unit["source"].replace("@ROOT@", str(W))
    si = argv.index(src_arg)
    argv[si] = str(source)
    oi = argv.index("-o")
    argv[oi + 1] = str(out / f"{unit_name}.o")
    if "-MF" in argv:
        argv[argv.index("-MF") + 1] = str(out / f"{unit_name}.d")
    # patched tags shadow headers (SettingsManager.hpp enum) via include-path precedence,
    # mirroring how a composition staging tree would carry the edited header
    incs = list(extra_includes)
    if "patched" in tag:
        incs.insert(0, ME / "work/patched")
    for inc in incs:
        argv.insert(1, "-I"); argv.insert(2, str(inc))
    argv += extra_flags
    t0 = time.monotonic()
    p = subprocess.run(argv, env=ENV, capture_output=True, text=True)
    rec = {
        "unit": unit_name, "tag": tag, "source": str(source),
        "werror": "-Werror" in extra_flags,
        "exit": p.returncode, "seconds": round(time.monotonic() - t0, 1),
        "object": str(out / f"{unit_name}.o"),
        "object_sha256": None, "stderr_path": str(out / "compile.stderr"),
        "n_diagnostics": p.stderr.count("warning:") + p.stderr.count("error:"),
        "errors": [l for l in p.stderr.splitlines() if "error:" in l][:20],
        "warnings": [l for l in p.stderr.splitlines() if "warning:" in l][:40],
    }
    if p.returncode == 0:
        import hashlib
        rec["object_sha256"] = hashlib.sha256(Path(rec["object"]).read_bytes()).hexdigest()
    (out / "compile.stderr").write_text(p.stderr)
    (out / "compile.stdout").write_text(p.stdout)
    (out / "command.json").write_text(json.dumps({"argv": argv}, indent=1) + "\n")
    return rec

def norm_warn(line):
    # strip position prefix -> message-kind key for baseline parity comparison
    msg = line.split(":", 3)[-1].strip()
    return " ".join(msg.split())

def warning_parity(results):
    by = {(r["unit"], "base" if r["tag"].startswith("base") else "patched"): r for r in results if r["tag"] in ("base", "patched")}
    parity = {}
    for unit in {r["unit"] for r in results}:
        b = by.get((unit, "base")); p = by.get((unit, "patched"))
        if not b or not p: continue
        bw = {norm_warn(l) for l in b["warnings"]}
        pw = {norm_warn(l) for l in p["warnings"]}
        parity[unit] = {"base_warning_kinds": len(bw), "patched_warning_kinds": len(pw),
                        "new_warning_kinds_introduced_by_diff": sorted(pw - bw)}
    return parity

jobs = [
    ("luamod", ME / "work/base/LuaMod.cpp", "base", [], ()),
    ("luamod", ME / "work/base/LuaMod.cpp", "base-werror", ["-Werror"], ()),
    ("luamod", ME / "work/patched/LuaMod.cpp", "patched", [], ()),
    ("luamod", ME / "work/patched/LuaMod.cpp", "patched-werror", ["-Werror"], ()),
    ("unreal-unrealinitializer", ME / "work/base/UnrealInitializer.cpp", "base", [], (OWNER_INCLUDE, STAGING_INCLUDE)),
    ("unreal-unrealinitializer", ME / "work/base/UnrealInitializer.cpp", "base-werror", ["-Werror"], (OWNER_INCLUDE, STAGING_INCLUDE)),
    ("unreal-unrealinitializer", ME / "work/patched/UnrealInitializer.cpp", "patched", [], (OWNER_INCLUDE, STAGING_INCLUDE)),
    ("unreal-unrealinitializer", ME / "work/patched/UnrealInitializer.cpp", "patched-werror", ["-Werror"], (OWNER_INCLUDE, STAGING_INCLUDE)),
    ("unreal-unrealinitializer", ME / "work/patched/UnrealInitializer.cpp", "patched-armed", ["-DUE4SS_R3_MARSHAL_MODE=1"], (OWNER_INCLUDE, STAGING_INCLUDE)),
    ("unreal-unrealinitializer", ME / "work/patched/UnrealInitializer.cpp", "patched-armed-werror", ["-DUE4SS_R3_MARSHAL_MODE=1", "-Werror"], (OWNER_INCLUDE, STAGING_INCLUDE)),
]
results = []
for unit_name, src, tag, flags, incs in jobs:
    r = compile_variant(unit_name, src, tag, flags, incs)
    results.append(r)
    print(f"{unit_name:26s} {tag:15s} exit={r['exit']:3d} {r['seconds']:6.1f}s diags={r['n_diagnostics']:3d}"
          + (f" err0={r['errors'][0][:100]}" if r["errors"] else ""), flush=True)

# The honest -Werror standard: the BASELINE units do not pass -Werror (22/5 promoted
# pre-existing warnings under the pinned argv), so the gate is KIND-PARITY: the patched
# variants (disarmed + armed) may not add a single -Werror kind the baseline doesn't have,
# and must compile clean (exit 0) without -Werror.
def werr_kinds(tag, unit):
    import re
    out = set()
    for l in (ME / "work/gate" / f"{unit}-{tag}" / "compile.stderr").read_text().splitlines():
        m = re.search(r"(error|warning): (.*)", l)
        if m: out.add(m.group(1) + ": " + m.group(2))
    return out

werror_parity = {}
for unit in ("luamod", "unreal-unrealinitializer"):
    base_k = werr_kinds("base-werror", unit)
    entry = {}
    for tag in (["patched-werror"] if unit == "luamod" else ["patched-werror", "patched-armed-werror"]):
        k = werr_kinds(tag, unit)
        entry[tag] = {"new_kinds_vs_baseline": sorted(k - base_k), "missing_kinds_vs_baseline": sorted(base_k - k)}
    werror_parity[unit] = entry

clean_exit = all(r["exit"] == 0 for r in results if not r["werror"])
no_new_werror_kinds = all(not v["new_kinds_vs_baseline"] for u in werror_parity.values() for v in u.values())

(ME / "receipts" / "compile-gate.json").write_text(json.dumps({
    "toolchain": {"prospero_clangpp": str(W / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++"),
                   "llvm_bin": LLVM_BIN},
    "recipe_sha256": __import__("hashlib").sha256(RECIPE.read_bytes()).hexdigest(),
    "extra_includes_initializer": [str(OWNER_INCLUDE), str(STAGING_INCLUDE)],
    "patched_include_shadow": str(ME / "work/patched"),
    "note": "mirrors composition-5 W_INCLUDE staging flow for the replaced initializer unit; -Werror added on top of the pinned unit argv for the gate variants; patched tags prepend work/patched so the edited SettingsManager.hpp (DIFF 1 enum) shadows the pristine include-tree header",
    "warning_parity": warning_parity(results),
    "werror_parity": werror_parity,
    "gate": {"patched_compiles_clean": clean_exit, "no_new_werror_kinds": no_new_werror_kinds, "verdict": "PASS" if (clean_exit and no_new_werror_kinds) else "FAIL"},
    "results": results}, indent=1) + "\n")
sys.exit(0 if (clean_exit and no_new_werror_kinds) else 1)
