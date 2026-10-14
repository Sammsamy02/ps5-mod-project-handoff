"""[F1 gate extension] List every source file the #9/#11 patch makes reachable,
so fs_throw_gate covers more than the 17 staged files (Mac-only, read-only on
every project tree; writes only the --out list).

Reachable =
  * the full include closure (clang -MM, i.e. every non-system header) of the
    three patched units, computed with R11's OWN compile commands
    (l26-A/pass1/objects/<unit>.command.json, the build of R11 bfd95db6) and the
    staged include dirs prepended exactly as target_compile_check.py does:
      cppmod  = the cap11 CppMod.cpp (cap09 + cap11 composition, a superset of cap09),
      program = the staged UE4SSProgram.cpp, luamod = the staged LuaMod.cpp;
  * the native module loader the patch now executes on the console (it is a
    prebuilt object in the R11 link, cppmod-module.o): CppModModule.cpp + .hpp.
Also writes <out>.cppmod: the closure of the cppmod unit + the loader only = the
code that was never executed on the console before the patch (no CppMod was ever
built under the snapshot in R11). run_gates.sh holds that set to ZERO throwing
filesystem calls (no baseline debt accepted there).
--odr-unused SYMBOL (repeatable): known baseline debt that sits in a header of
the cppmod closure is accepted by run_gates.sh only if the cppmod unit never
uses the function holding it. Proof: the cppmod unit is compiled at -O0 (no
inlining, so every inline function it uses is emitted) and llvm-nm -C must show
NO definition whose name contains SYMBOL. A use = FAIL.
Prints items_checked (files listed); exit 0 ok, 1 compile/dependency failure or
an --odr-unused symbol that IS used, 2 zero files.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
sys.path.insert(0, str(Path(__file__).resolve().parent / "cap09/tests"))
import target_compile_check as tcc  # noqa: E402  (R11 locator + LLVM paths; read-only)

LOADER = W / ".superpowers/native-dependency-spike/native-coherent-cpp-loader-1/build-1/source"
CXX_EXT = (".cpp", ".cc", ".cxx", ".c", ".hpp", ".h", ".hh", ".inl", ".inc", ".ipp")


def r11_argv(unit, src, includes):
    argv = json.loads((tcc.PASS1 / "objects" / f"{unit}.command.json").read_text())["argv"]
    old_root = str(tcc.BV / "l26-A")
    argv = [x.replace(old_root, str(tcc.R11_ROOT)) if x.startswith(old_root) and not Path(x).exists() else x for x in argv]
    srcs = [i for i, x in enumerate(argv) if x.endswith((".cpp", ".cc", ".cxx")) and not x.startswith("-")]
    if len(srcs) != 1:
        raise SystemExit(f"FAIL: {unit}: expected one source in the R11 command, found {len(srcs)}")
    argv[srcs[0]] = str(Path(src).resolve())
    if "-MD" in argv:
        i = argv.index("-MD")
        del argv[i:i + 3]
    return [argv[0]] + [f"-I{d}" for d in includes] + argv[1:]


def odr_unused(src, includes, symbols, tmp, controls=()):
    argv = r11_argv("cppmod", src, includes)
    argv = ["-O0" if x in ("-O1", "-O2", "-O3", "-Os", "-Oz") else x for x in argv]
    obj = Path(tmp) / "cppmod-O0.o"
    argv[argv.index("-o") + 1] = str(obj)
    env = dict(os.environ, LLVM_CONFIG=str(tcc.LLVM / "llvm-config"))
    r = subprocess.run(argv, capture_output=True, text=True, env=env)
    if r.returncode != 0 or not obj.exists():
        print(r.stderr[-3000:])
        raise SystemExit("FAIL: -O0 compile of cppmod for the ODR-use check")
    r = subprocess.run([str(tcc.NM), "-C", "--defined-only", str(obj)], capture_output=True, text=True)
    defined = r.stdout.splitlines()
    bad = 0
    for s in symbols:
        hits = [l for l in defined if s in l]
        print(("  FAIL " if hits else "  pass ") + f"cppmod unit (-O0, {len(defined)} defined symbols) " +
              ("USES " + s + ": " + hits[0][:120] if hits else "never uses " + s))
        bad += bool(hits)
    for s in controls:  # positive control: the check must SEE a function the unit does use
        hits = [l for l in defined if s in l]
        print(("  pass " if hits else "  FAIL ") + f"control: cppmod unit uses {s} and the -O0 symbol check sees it" + ("" if hits else " (check is blind)"))
        bad += not hits
    return bad


def deps(unit, src, includes, tmp):
    argv = r11_argv(unit, src, includes)
    o = argv.index("-o")
    del argv[o:o + 2]
    if "-c" in argv:
        argv.remove("-c")
    depfile = Path(tmp) / f"{unit}.d"
    argv = argv + ["-MM", "-MF", str(depfile)]
    env = dict(os.environ, LLVM_CONFIG=str(tcc.LLVM / "llvm-config"))
    r = subprocess.run(argv, capture_output=True, text=True, env=env)
    if r.returncode != 0 or not depfile.exists():
        print(r.stderr[-3000:])
        raise SystemExit(f"FAIL: dependency scan of {unit} exit={r.returncode}")
    text = depfile.read_text().replace("\\\n", " ")
    files = set()
    for tok in text.split(":", 1)[1].split():
        p = Path(tok)
        if tok.endswith(CXX_EXT) and p.exists():
            files.add(str(p.resolve()))
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", action="append", required=True, help="NAME=SOURCE")
    ap.add_argument("--include", action="append", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--odr-unused", action="append", default=[])
    ap.add_argument("--odr-used-control", action="append", default=[])
    a = ap.parse_args()
    allf, cpp_only = set(), set()
    odr_bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        if a.odr_unused:
            cpp_src = dict(s.split("=", 1) for s in a.unit).get("cppmod")
            if not cpp_src:
                raise SystemExit("FAIL: --odr-unused needs --unit cppmod=...")
            odr_bad = odr_unused(cpp_src, a.include, a.odr_unused, tmp, a.odr_used_control)
        for spec in a.unit:
            name, src = spec.split("=", 1)
            d = deps(name, src, a.include, tmp)
            print(f"  {name}: {len(d)} reachable source/header files ({Path(src).name})")
            allf |= d
            if name == "cppmod":
                cpp_only |= d
    loader = {str((LOADER / "CppModModule.cpp").resolve()), str((LOADER / "Platform/CppModModule.hpp").resolve())}
    for f in loader:
        if not Path(f).exists():
            print(f"FAIL: loader file missing: {f}")
            sys.exit(1)
    allf |= loader
    cpp_only |= loader
    Path(a.out).write_text("\n".join(sorted(allf)) + "\n")
    Path(a.out + ".cppmod").write_text("\n".join(sorted(cpp_only)) + "\n")
    print(f"reach_files items_checked={len(allf)} newly_executed={len(cpp_only)} -> {a.out}")
    print(f"reach_files odr_unused_checked={len(a.odr_unused)} controls={len(a.odr_used_control)} odr_failures={odr_bad}")
    sys.exit(2 if not allf or not cpp_only else (1 if odr_bad else 0))


if __name__ == "__main__":
    main()
