#!/usr/bin/env python3
"""Local object/import boundary check. This never creates or deploys a PRX."""
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LLVM = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LIB = ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib/libkernel.so"
NAMES = ["UnrealInitializer.cpp", "runtime_start_bridge_r14_diag_spanpub.cpp"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def nm(path, dynamic=False):
    argv = [str(LLVM / "llvm-nm"), "--extern-only", "--format=posix"]
    if dynamic:
        argv.append("-D")
    r = subprocess.run(argv + [str(path)], check=True, capture_output=True, text=True)
    rows = [line.split() for line in r.stdout.splitlines() if line.strip()]
    return ({x[0] for x in rows if x[1] == "U"},
            {x[0] for x in rows if x[1] not in ("U", "w", "v")}, r.stdout)


def aggregate(folder):
    undefined, defined, objects = set(), set(), []
    for name in NAMES:
        path = HERE / folder / (name + ".o")
        u, d, _ = nm(path)
        undefined |= u
        defined |= d
        objects.append({"path": str(path), "sha256": sha(path)})
    return undefined-defined, defined, objects


if __name__ == "__main__":
    before, _, baseline = aggregate("baseline-build")
    after, defined, current = aggregate("build")
    _, provider, provider_stdout = nm(LIB, dynamic=True)
    (HERE / "receipts/libkernel-symbols.txt").write_text(provider_stdout)
    added = after-before
    expected = {"_openat", "sceKernelRead"}
    bridges = {"bridge_kernel_write_copyin_probe_until", "bridge_copyin_range_faulted"}
    checks = {
        "only_expected_new_imports": added == expected,
        "new_imports_declared_by_pinned_sdk": expected <= provider,
        "new_bridge_calls_have_definitions": bridges <= defined,
        "new_bridge_calls_not_unresolved": not bool(bridges & after),
    }
    contract_path = ROOT / "outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l12-2/supplier-contract.json"
    old = json.loads(contract_path.read_text())
    donor = next(row["ImportSymbol"] for row in old["contract"]["imports"]
                 if row["ImportSymbol"]["Name"] == "sceKernelOpen")
    extension = []
    for name in sorted(expected):
        row = dict(donor, Name=name, PublishedName=name)
        extension.append(row)
    (HERE / "receipts/l13-import-extension.json").write_text(json.dumps({
        "scope": "SDK declaration evidence and candidate packaging input only; not loader resolution",
        "base_contract": str(contract_path), "base_contract_sha256": sha(contract_path),
        "imports": extension,
        "provider": str(LIB), "provider_sha256": sha(LIB),
        "must_apply_at": "bootstrap/combined-object contract extension, not pre-bootstrap runtime-only import checks",
        "full_prx_packaging_run": False, "console_resolution_tested": False,
    }, indent=2) + "\n")
    receipt = {
        "scope": "Mac-only compiled-object linkage and SDK declarations",
        "checks": checks, "status": "PASS" if all(checks.values()) else "FAIL",
        "added_strong_undefined": sorted(added), "removed_strong_undefined": sorted(before-after),
        "baseline_objects": baseline, "current_objects": current,
        "provider": str(LIB), "provider_sha256": sha(LIB),
        "full_prx_packaging_run": False, "console_contact": False,
    }
    (HERE / "receipts/symbol-boundary.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"status": receipt["status"], "checks": checks, "added": sorted(added)}, indent=2))
    raise SystemExit(0 if all(checks.values()) else 1)
