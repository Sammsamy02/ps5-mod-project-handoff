#!/usr/bin/env python3
"""Target compile probe: runtime_ack_supplier.cpp as a composition-4 extra unit.

Uses the loader-context argv template from the pinned base receipt (identical
mechanism to the LMF/harness extra units) with a dest-stage mirrored -I to this
worker's staging/include (the real composition stages those five files into
W/include, glob-pinned by the build script). Verifies the unit compiles clean
under -Werror and that its undefined imports are exactly the expected set:
the ack/backend closure (defined by composition-3 units), the harness wiring
(defined by the harness extra unit), and getpid/clock_gettime (frozen import
contract; SDK .so providers verified by the build's extended-baseline gate).
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

WHAT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
SPIKE = WHAT / ".superpowers/native-dependency-spike"
BASE = SPIKE / "ue4ss-native-notify-cancel-1/build-1/receipt.json"
W = SPIKE / "ue4ss-native-tick-file-owner-1"
ME = WHAT / "outputs/2026-10-04-zcode-team/worker-query-s2-arm"
SRC = ME / "integration-patch/new-units/runtime_ack_supplier.cpp"
OUT = ME / "probe/supplier-probe"


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run(name, argv, env=None, check=True):
    p = subprocess.run([str(a) for a in argv], capture_output=True, text=True, env=env)
    (OUT / f"{name}.stdout").write_text(p.stdout)
    (OUT / f"{name}.stderr").write_text(p.stderr)
    print(f"[{name}] exit={p.returncode}")
    if check and p.returncode:
        print(p.stderr[-8000:])
        sys.exit(1)
    return p


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    base = json.loads(BASE.read_text())
    unit = base["selected_runtime_units"]["native-loadercontext"]
    argv = unit["argv"].copy()
    template_source = next(x for x in argv if x.endswith("NativeLoaderContext.cpp"))
    argv[argv.index(template_source)] = str(SRC)
    argv[argv.index("-o") + 1] = str(OUT / "runtime-ack-supplier.o")
    argv[argv.index("-MF") + 1] = str(OUT / "runtime-ack-supplier.d")
    argv[1:1] = ["-I", str(ME / "staging/include"),
                 "-I", str(W / "include"),
                 "-I", str(SPIKE / "ue4ss-native-tick-witness-1" / "include")]
    run("compile-extra-unit-runtime-ack-supplier", argv,
        env=dict(os.environ, LLVM_CONFIG=unit["LLVM_CONFIG"]))
    obj = OUT / "runtime-ack-supplier.o"
    print("object bytes:", obj.stat().st_size)
    print("object sha256:", sha256(obj))

    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "sealed_build", SPIKE / "native-coherent-runtime-1" / "build_runtime.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    llvm_nm = str(build.LLVM / "llvm-nm")
    p = run("nm-undefined-ack-supplier",
            [llvm_nm, "--extern-only", "--undefined-only", "--format=posix", obj])
    syms = sorted({line.split()[0] for line in p.stdout.splitlines() if line.strip()})
    (OUT / "undefined-symbols.json").write_text(json.dumps(syms, indent=2) + "\n")
    print("undefined count:", len(syms))
    for s in syms:
        print("  ", s)

    # Classification: three buckets must cover everything.
    composed_defined = set()
    composed_undefined = set()
    dd = ME / "probe/target-probe-composition-3/nm-composed-defined.stdout"
    if dd.is_file():
        composed_defined = {l.split()[0] for l in dd.read_text().splitlines() if l.strip()}
    du = ME / "probe/target-probe-composition-3/nm-composed-undefined.stdout"
    if du.is_file():
        composed_undefined = {l.split()[0] for l in du.read_text().splitlines() if l.strip()}
    harness_dir = ME / "probe/target-probe-composition-3"
    prior = json.loads((harness_dir / "symbol-classification-vs-composition-3.json").read_text())
    resolved = set(prior["resolved_by_composed_units"]) | set(prior["already_in_composed_baseline"])
    # harness unit defines these (verified by the harness probe's own classification)
    harness_defined = {"_ZN2RC8Platform12QueryHarness6GlobalEv",
                       "_ZN2RC8Platform12QueryHarness7Harness16DisarmLayoutGateEv",
                       "_ZN2RC8Platform12QueryHarness7Harness13ArmLayoutGateERKNS1_15LayoutGateFactsE",
                       "_ZN2RC8Platform12QueryHarness7Harness19OnEngineTickDispatchEv",
                       "_ZN2RC8Platform12QueryHarness7Harness21InstallEngineBindingsERKNS1_14EngineBindingsE",
                       "_ZN2RC8Platform12QueryHarness7Harness28InstallDefaultEngineBindingsEv"}
    libc_like = {"getpid", "clock_gettime"}
    unexpected = [s for s in syms
                  if s not in composed_defined and s not in composed_undefined
                  and s not in harness_defined and s not in libc_like]
    (OUT / "classification.json").write_text(json.dumps({
        "undefined_count": len(syms),
        "resolved_by_composed_units": sorted(s for s in syms if s in composed_defined),
        "already_in_composed_baseline": sorted(s for s in syms if s in composed_undefined),
        "resolved_by_harness_extra_unit": sorted(s for s in syms if s in harness_defined),
        "frozen_import_contract": sorted(s for s in syms if s in libc_like),
        "UNEXPECTED": unexpected,
    }, indent=2) + "\n")
    if unexpected:
        sys.exit("unexpected undefined symbols: " + ",".join(unexpected))
    print("CLASSIFICATION CLEAN: composed units + harness unit + frozen import contract only")


if __name__ == "__main__":
    main()
