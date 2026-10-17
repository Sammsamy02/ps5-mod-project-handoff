#!/usr/bin/env python3
"""Target-side compilability probe for the native-query-harness extra unit.

Reconstructs the exact --extra-unit compile that
.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py
performs (loader-context argv template, -I tick-file-owner-1/include,
-I tick-witness-1/include, same -MF depfile discipline) and runs it against
src/NativeQueryHarness.cpp with the pinned prospero-clang++ from the base
receipt. Emits the object, depfile and a symbol classification INSIDE this
worker output dir. Does NOT run build_runtime.py's composition/link (that is
the controller's serialized step), contact any console, or execute target code.
"""
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKER = HERE.parent
SRC = WORKER / "src" / "NativeQueryHarness.cpp"
SPIKE = WORKER.parents[2] / ".superpowers" / "native-dependency-spike"
BASE = SPIKE / "ue4ss-native-notify-cancel-1" / "build-1" / "receipt.json"
W = SPIKE / "ue4ss-native-tick-file-owner-1"
OUT = HERE / "target-probe"


def run(name, argv, env=None):
    p = subprocess.run([str(a) for a in argv], capture_output=True, text=True, env=env)
    (OUT / f"{name}.stdout").write_text(p.stdout)
    (OUT / f"{name}.stderr").write_text(p.stderr)
    print(f"[{name}] exit={p.returncode}")
    if p.returncode:
        print(p.stderr[-5000:])
        sys.exit(1)
    return p


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if not SRC.is_file():
        raise SystemExit("missing source")
    base = json.loads(BASE.read_text())
    unit = base["selected_runtime_units"]["native-loadercontext"]
    argv = unit["argv"].copy()
    template_source = next(x for x in argv if x.endswith("NativeLoaderContext.cpp"))
    # build_runtime.py extra-unit shape: template argv, source swapped, then
    # argv[1:1] = ['-I', W/include, '-I', tick-witness-1/include]; no defines needed.
    argv[argv.index(template_source)] = str(SRC)
    argv[argv.index("-o") + 1] = str(OUT / "native-query-harness.o")
    argv[argv.index("-MF") + 1] = str(OUT / "native-query-harness.d")
    argv[1:1] = ["-I", str(W / "include"),
                 "-I", str(SPIKE / "ue4ss-native-tick-witness-1" / "include")]
    run("compile-extra-unit-native-query-harness", argv,
        env=dict(os.environ, LLVM_CONFIG=unit["LLVM_CONFIG"]))  # same env handoff as build_runtime.py

    spec = importlib.util.spec_from_file_location("sealed_build",
                                                  SPIKE / "native-coherent-runtime-1" / "build_runtime.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    llvm_nm = build.LLVM / "llvm-nm"
    undefined = {line.split()[0] for line in
                 run("nm-undefined", [llvm_nm, "--extern-only", "--undefined-only",
                                      "--format=posix", OUT / "native-query-harness.o"]).stdout.splitlines()
                 if line.strip()}
    # Which of those does the composed runtime already define (resolve at ld -r)?
    composed_path = SPIKE / "ue4ss-native-tick-file-owner-1" / "native-1" / "coherent-tick-file-witness-runtime.o"
    if composed_path.is_file():
        defined = {line.split()[0] for line in
                   run("nm-composed-defined", [llvm_nm, "--extern-only", "--defined-only",
                                               "--format=posix", composed_path]).stdout.splitlines()
                   if line.strip()}
    else:
        defined = set()
    baseline_undefined = set(base["undefined_delta"]["current"])
    report = {
        "object": str(OUT / "native-query-harness.o"),
        "object_bytes": (OUT / "native-query-harness.o").stat().st_size,
        "unit_undefined_symbols": sorted(undefined),
        "resolved_by_composed_units": sorted(undefined & defined),
        "unit_new_undefined_after_composition_estimate": sorted(undefined - defined - baseline_undefined),
        "sdk_baseline_undefined_already_present": sorted(undefined & baseline_undefined),
        "composition_object_present": composed_path.is_file(),
    }
    (OUT / "symbol-classification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: (v if not isinstance(v, list) else f"{len(v)} symbols") for k, v in report.items()}, indent=2))


if __name__ == "__main__":
    main()
