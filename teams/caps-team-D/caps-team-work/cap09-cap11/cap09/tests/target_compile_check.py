"""Mac-only TARGET compile of the staged runtime units + import-delta check
against the R11 runtime object (review findings B2 and blocker #1).

What it does (read-only on every project tree; objects go to --out):
  1. For each --unit NAME=SOURCE, take R11's own compile command
     (l26-A/pass1/objects/NAME.command.json, the build that produced R11
     bfd95db6), swap in the staged SOURCE, prepend the staged include dirs
     (--include, in order), drop -MD/-MF, and compile with the same
     prospero-clang++ and LLVM 21.1.8 (LLVM_CONFIG). Any compile error = FAIL.
  2. Import delta, the same question the build's G7-import-delta-none gate asks:
     every symbol the new objects leave undefined (U and weak w) must already be
     either DEFINED by R11's runtime-l26-A.o (minus what only the replaced old
     units defined strongly) or already UNDEFINED there (an existing import).
     Anything else would pull a new archive member or become a new import
     (e.g. __cxa_thread_atexit from a thread_local with a destructor) = FAIL.
  3. Duplicate check: a symbol the new objects define strongly that R11 already
     defines strongly outside the replaced units = FAIL (ODR / link clash).
Prints items_checked (undefined symbols checked); exit 0 PASS, 1 FAIL, 2 zero
items or tool missing. No PRX is linked or packaged.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
BV = W / "outputs/2026-10-08-l26-build-variants"
# R11 (bfd95db6) is identified by its runtime object's sha, wherever the
# pipeline has moved it (the next build moves l26-A/ into archive-r6/R11-*/).
R11_RUNTIME_SHA = "f247313c59c49140"
R11_CANDIDATES = [BV / "archive-r6/R11-watch-world-uenum/l26-A", BV / "l26-A"]


def find_r11():
    for root in R11_CANDIDATES:
        o = root / "pass1/runtime-l26-A.o"
        if o.exists() and hashlib.sha256(o.read_bytes()).hexdigest().startswith(R11_RUNTIME_SHA):
            return root
    raise SystemExit("FAIL: R11 build objects (runtime-l26-A.o sha " + R11_RUNTIME_SHA + ") not found")


R11_ROOT = find_r11()
PASS1 = R11_ROOT / "pass1"
LLVM = W / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
NM = LLVM / "llvm-nm"
STRONG = set("TDBRC")  # strong definitions (llvm-nm letters; upper = global)


def nm(path, *flags):
    r = subprocess.run([str(NM), *flags, str(path)], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"llvm-nm failed on {path}: {r.stderr[:400]}")
    rows = []
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and len(parts[-2]) == 1:
            rows.append((parts[-2], parts[-1]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", action="append", required=True, help="NAME=STAGED_SOURCE")
    ap.add_argument("--baseline-unit", action="append", default=[],
                    help="NAME=SOURCE compiled UNPATCHED with the same command; its symbols join the allowed set, so a delta on a "
                         "moved live tree counts only what the staged hunks add (other teams' merged work is not ours)")
    ap.add_argument("--include", action="append", default=[])
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline-object", default=str(PASS1 / "runtime-l26-A.o"))
    print(f"  R11 build objects: {R11_ROOT} (runtime-l26-A.o sha {R11_RUNTIME_SHA}...)")
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    if not NM.exists():
        print("FAIL: llvm-nm missing", NM)
        sys.exit(2)
    env = dict(os.environ, LLVM_CONFIG=str(LLVM / "llvm-config"))
    fails, items = [], 0
    new_objs, old_objs, base_objs = [], [], []
    for spec in a.baseline_unit + a.unit:
        is_base = spec in a.baseline_unit and spec not in a.unit
        name, src = spec.split("=", 1)
        cmd_path = PASS1 / "objects" / f"{name}.command.json"
        argv = json.loads(cmd_path.read_text())["argv"]
        # paths of the moved build folder (e.g. the -ivfsoverlay file) -> where it is now
        old_root = str(BV / "l26-A")
        argv = [x.replace(old_root, str(R11_ROOT)) if x.startswith(old_root) and not Path(x).exists() else x for x in argv]
        srcs = [i for i, x in enumerate(argv) if x.endswith((".cpp", ".cc", ".cxx")) and not x.startswith("-")]
        if len(srcs) != 1:
            print(f"FAIL: {name}: expected one source in the R11 command, found {len(srcs)}")
            sys.exit(1)
        orig_src = argv[srcs[0]]
        argv[srcs[0]] = str(Path(src).resolve())
        o = argv.index("-o")
        obj = out / (f"base-{name}.o" if is_base else f"{name}.o")
        argv[o + 1] = str(obj)
        if "-MD" in argv:
            i = argv.index("-MD")
            del argv[i:i + 3]
        argv = [argv[0]] + [f"-I{d}" for d in a.include] + [f"-D{d}" for d in a.define] + argv[1:]
        r = subprocess.run(argv, capture_output=True, text=True, env=env)
        (out / (obj.stem + ".stderr")).write_text(r.stderr)
        errors = r.stderr.count(" error: ")
        warns = r.stderr.count(" warning: ")
        items += 1
        ok = r.returncode == 0 and obj.exists()
        print(("  pass " if ok else "  FAIL ") + ("baseline " if is_base else "") + f"target compile {name}: {Path(src).name} (R11 command {cmd_path.name} sha={hashlib.sha256(cmd_path.read_bytes()).hexdigest()[:8]}, R11 source was {Path(orig_src).name}) exit={r.returncode} errors={errors} warnings={warns}")
        if not ok:
            print(r.stderr[-4000:])
            fails.append(f"compile {name}")
            continue
        if is_base:
            base_objs.append(obj)
            continue
        new_objs.append(obj)
        old = PASS1 / "objects" / f"{name}.o"
        if old.exists():
            old_objs.append(old)
    if fails:
        print(f"target_compile_check items_checked={items} failed={len(fails)}")
        sys.exit(1)

    base = nm(a.baseline_object)
    base_def = {s for k, s in base if k not in ("U", "w", "v")}
    base_undef = {s for k, s in base if k in ("U", "w", "v")}
    base_strong = {s for k, s in base if k in STRONG}
    old_strong = set()
    for o in old_objs:
        old_strong |= {s for k, s in nm(o) if k in STRONG}
    avail_def = base_def - old_strong
    live_syms = set()
    for o in base_objs:
        live_syms |= {s for k, s in nm(o)}
    if base_objs:
        print(f"  live-tree baseline units: {len(base_objs)} (their {len(live_syms)} symbols are allowed: not added by the staged hunks)")
    new_rows = []
    for o in new_objs:
        new_rows += nm(o)
    new_def = {s for k, s in new_rows if k not in ("U", "w", "v")}
    new_strong = {s for k, s in new_rows if k in STRONG}
    needed = sorted({s for k, s in new_rows if k in ("U", "w", "v")} - new_def)
    added = []
    for s in needed:
        items += 1
        if s not in avail_def and s not in base_undef and s not in live_syms:
            added.append(s)
    live_strong = set()
    for o in base_objs:
        live_strong |= {s for k, s in nm(o) if k in STRONG}
    dup = sorted(new_strong & (base_strong - old_strong) - live_strong)
    items += len(new_strong)
    print(f"  baseline {Path(a.baseline_object).name}: defined={len(base_def)} undefined(imports)={len(base_undef)}; replaced units' strong defs removed={len(old_strong)}")
    print(f"  new objects: {len(new_objs)}, undefined symbols checked={len(needed)}, strong definitions checked={len(new_strong)}")
    for s in added:
        print(f"  FAIL new undefined symbol (new import or new archive member): {s}")
    for s in dup[:20]:
        print(f"  FAIL duplicate strong definition already in R11: {s}")
    if not added:
        print("  pass import delta: every undefined symbol is already defined in or imported by R11 (0 new)")
    if not dup:
        print("  pass no duplicate strong definition against R11")
    fails += added + dup
    print(f"target_compile_check items_checked={items} new_undefined={len(added)} duplicates={len(dup)} failed={len(fails)}")
    if not needed or items == 0:
        print("FAIL: zero symbols checked")
        sys.exit(2)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
