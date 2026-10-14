#!/usr/bin/env python3
"""S2-arm prewire probe: worker-query-harness extra unit vs the COMPOSITION-3 object.

Byte-for-byte mechanism of
outputs/2026-10-04-zcode-team/worker-query-harness-integration/extra_unit_compile_probe_supplier_composition_1.py
(loader-context argv template from the pinned base receipt, source swap,
-I tick-file-owner-1/include + -I tick-witness-1/include, -MF depfile,
LLVM_CONFIG handoff, pinned prospero-clang++), reclassified against the
SUPPLIER-COMPOSITION-3 RUN-1 object (the current composed runtime):

  .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/
      ue4ss-native-supplier-composition-3/run-1/coherent-tick-file-witness-runtime.o

whose sha256 must equal worker-composition-3 REPORT section 5 runtime_sha256
(e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350).

Composed baseline = the composed object's OWN undefined set (SDK + extended
baseline incl. _DYNAMIC, sceKernelGetModuleInfo). Prediction under test:
  unit_undefined - defined(composed) - undefined(composed) == {}   (zero NEW)
Writes ONLY inside this worker output dir. No composition/link (controller
serialized step), no network, no console, no target execution.
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKER = HERE.parent
WHAT = WORKER.parents[2]
SPIKE = WHAT / ".superpowers" / "native-dependency-spike"
SRC = (WHAT / "outputs/2026-10-04-zcode-team/worker-query-harness/src/NativeQueryHarness.cpp").resolve()
BASE = SPIKE / "ue4ss-native-notify-cancel-1" / "build-1" / "receipt.json"
W = SPIKE / "ue4ss-native-tick-file-owner-1"
COMPOSED_DIR = W / "ue4ss-native-supplier-composition-3" / "run-1"
COMPOSED = COMPOSED_DIR / "coherent-tick-file-witness-runtime.o"
EXPECTED_SHA = "e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350"
EXPECTED_BYTES = 117712568
OUT = HERE / "target-probe-composition-3"

SRC_SHA = "1b9cec495343d5c80a2d096519128ad887c927f1376e3de722232ac8fadadd45"


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
        print(p.stderr[-5000:])
        sys.exit(1)
    return p


def nm_set(llvm_nm, flags, obj, name):
    p = run(name, [llvm_nm, "--extern-only", flags, "--format=posix", obj])
    return {line.split()[0] for line in p.stdout.splitlines() if line.strip()}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if not SRC.is_file() or sha256(SRC) != SRC_SHA:
        raise SystemExit("harness source missing or hash drift")
    composed_sha, composed_bytes = sha256(COMPOSED), COMPOSED.stat().st_size
    print(f"composed sha256={composed_sha} bytes={composed_bytes}")
    if (composed_sha, composed_bytes) != (EXPECTED_SHA, EXPECTED_BYTES):
        raise SystemExit("composed object does not match composition-3 checkpoint e9679aa5")

    base = json.loads(BASE.read_text())
    unit = base["selected_runtime_units"]["native-loadercontext"]
    argv = unit["argv"].copy()
    template_source = next(x for x in argv if x.endswith("NativeLoaderContext.cpp"))
    argv[argv.index(template_source)] = str(SRC)
    argv[argv.index("-o") + 1] = str(OUT / "native-query-harness.o")
    argv[argv.index("-MF") + 1] = str(OUT / "native-query-harness.d")
    argv[1:1] = ["-I", str(W / "include"),
                 "-I", str(SPIKE / "ue4ss-native-tick-witness-1" / "include")]
    run("compile-extra-unit-native-query-harness", argv,
        env=dict(os.environ, LLVM_CONFIG=unit["LLVM_CONFIG"]))

    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "sealed_build", SPIKE / "native-coherent-runtime-1" / "build_runtime.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    llvm_nm = build.LLVM / "llvm-nm"

    unit_undefined = nm_set(llvm_nm, "--undefined-only", OUT / "native-query-harness.o", "nm-undefined")
    composed_defined = nm_set(llvm_nm, "--defined-only", COMPOSED, "nm-composed-defined")
    composed_undefined = nm_set(llvm_nm, "--undefined-only", COMPOSED, "nm-composed-undefined")

    # Cross-check the nm-measured composed baseline against the run-1 receipt artifact.
    receipt_undefined = None
    dd = COMPOSED_DIR / "undefined-delta.json"
    if dd.is_file():
        try:
            data = json.loads(dd.read_text())
            cand = data.get("current") if isinstance(data, dict) else None
            receipt_undefined = sorted(cand) if isinstance(cand, list) else sorted(data.keys())
        except Exception as exc:  # provenance-preserving: record, never fail on shape
            receipt_undefined = [f"unparsed: {exc}"]

    delta = sorted(unit_undefined - composed_defined - composed_undefined)
    report = {
        "composed_object": str(COMPOSED),
        "composed_sha256": composed_sha,
        "composed_bytes": composed_bytes,
        "checkpoint_match": True,
        "unit_source": str(SRC),
        "unit_source_sha256": SRC_SHA,
        "object": str(OUT / "native-query-harness.o"),
        "object_bytes": (OUT / "native-query-harness.o").stat().st_size,
        "object_sha256": sha256(OUT / "native-query-harness.o"),
        "unit_undefined_count": len(unit_undefined),
        "composed_defined_count": len(composed_defined),
        "composed_undefined_count": len(composed_undefined),
        "resolved_by_composed_units": sorted(unit_undefined & composed_defined),
        "already_in_composed_baseline": sorted(unit_undefined & composed_undefined),
        "NEW_UNDEFINED_AFTER_COMPOSITION_DELTA": delta,
        "run1_undefined_delta_json_current": receipt_undefined,
    }
    (OUT / "symbol-classification-vs-composition-3.json").write_text(
        json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: (v if not isinstance(v, list) else f"{len(v)} symbols")
                      for k, v in report.items()}, indent=2))
    print("DELTA:", delta if delta else "EMPTY (zero new undefined symbols)")
    if delta:
        sys.exit(2)


if __name__ == "__main__":
    main()
