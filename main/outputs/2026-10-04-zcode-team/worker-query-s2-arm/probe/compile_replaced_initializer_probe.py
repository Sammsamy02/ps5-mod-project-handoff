#!/usr/bin/env python3
"""Compile probe: the S2-arm-patched UnrealInitializer.cpp under the exact
affected-unit target argv (base receipt unreal-unrealinitializer argv + the
build mechanism's -I insertions). The harness header cannot be staged into the
read-only W/include for this probe, so a mirrored -I to worker-query-harness/src
is inserted first (worker-ack-integration-deep dest-stage technique); the real
composition-4 consumes the hash-identical staged copy from W/include.

Outputs (all under this worker dir): native-unrealinitializer-s2arm.{o,d,stdout,stderr}
plus nm classification of the object's undefined symbols.
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
STAGED = WHAT / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp"
HARNESS_SRC_DIR = WHAT / "outputs/2026-10-04-zcode-team/worker-query-harness/src"
OUT = WHAT / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/probe/initializer-probe"

EXPECTED_STAGED_SHA = "933d08a2f3e4dea1f80d216bd99779871b6e5b3ae1f26028d357d824051e2542"


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run(name, argv, env=None):
    p = subprocess.run([str(a) for a in argv], capture_output=True, text=True, env=env)
    (OUT / f"{name}.stdout").write_text(p.stdout)
    (OUT / f"{name}.stderr").write_text(p.stderr)
    print(f"[{name}] exit={p.returncode}")
    if p.returncode:
        print(p.stderr[-8000:])
        sys.exit(1)
    return p


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if sha256(STAGED) != EXPECTED_STAGED_SHA:
        sys.exit("staged initializer hash drift")
    base = json.loads(BASE.read_text())
    unit = base["selected_runtime_units"]["unreal-unrealinitializer"]
    argv = unit["argv"].copy()
    old_source = next(x for x in argv if x.endswith("UnrealInitializer.cpp"))
    argv[argv.index(old_source)] = str(STAGED)
    argv[argv.index("-o") + 1] = str(OUT / "native-unrealinitializer-s2arm.o")
    argv[argv.index("-MF") + 1] = str(OUT / "native-unrealinitializer-s2arm.d")
    # dest-stage mirror -I first, then the mechanism's own two -I dirs.
    argv[1:1] = ["-I", str(HARNESS_SRC_DIR),
                 "-I", str(W / "include"),
                 "-I", str(SPIKE / "ue4ss-native-tick-witness-1" / "include")]
    run("compile-native-unrealinitializer-s2arm", argv,
        env=dict(os.environ, LLVM_CONFIG=unit["LLVM_CONFIG"]))
    print("object bytes:", (OUT / "native-unrealinitializer-s2arm.o").stat().st_size)
    print("object sha256:", sha256(OUT / "native-unrealinitializer-s2arm.o"))

    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "sealed_build", SPIKE / "native-coherent-runtime-1" / "build_runtime.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    llvm_nm = str(build.LLVM / "llvm-nm")
    p = run("nm-undefined-s2arm-initializer",
            [llvm_nm, "--extern-only", "--undefined-only", "--format=posix",
             OUT / "native-unrealinitializer-s2arm.o"])
    syms = sorted({line.split()[0] for line in p.stdout.splitlines() if line.strip()})
    queryharness = [s for s in syms if "QueryHarness" in s]
    (OUT / "undefined-symbols.json").write_text(
        json.dumps({"undefined_count": len(syms), "query_harness_symbols": queryharness}, indent=2) + "\n")
    print("undefined count:", len(syms))
    print("query-harness undefined:", queryharness)
    expected = {"_ZN2RC8Platform12QueryHarness6GlobalEv",
                "_ZN2RC8Platform12QueryHarness7Harness13ArmLayoutGateERKNS1_15LayoutGateFactsE",
                "_ZN2RC8Platform12QueryHarness7Harness16DisarmLayoutGateEv",
                "_ZN2RC8Platform12QueryHarness7Harness20OnEngineTickDispatchEv"}
    missing = expected - set(queryharness)
    if missing:
        sys.exit(f"missing expected query-harness imports: {missing}")
    print("EXPECTED HARNESS IMPORTS PRESENT (link-time resolved by the harness extra unit)")


if __name__ == "__main__":
    main()
