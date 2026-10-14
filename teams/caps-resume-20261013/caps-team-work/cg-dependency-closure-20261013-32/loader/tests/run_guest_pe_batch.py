#!/usr/bin/env python3
"""Compile and run ordered guarded-DLL batch checks on the x64 Mac host."""
from pathlib import Path
import hashlib
import subprocess

root = Path(__file__).resolve().parents[1]
fixture = root.parents[1] / "cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll"
expected = "acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce"
if hashlib.sha256(fixture.read_bytes()).hexdigest() != expected:
    raise RuntimeError("synthetic fixture hash mismatch")
out = root / "tests/guest-pe-batch-host"
upstream = root.parent.parent / "cg-max-ready-20261013-03/cap13/upstream-inputs"
real_cases = [
    (root.parent / "third_party/TrueFOV-upstream-winmm.dll",
     "5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3"),
    (upstream / "ZeroCompanyMandoWardrobe-upstream-main.dll",
     "821ecad6c9f7e61faa2a07b31c216475768b72c2066369d3b0b88f715b33a40c"),
    (upstream / "TFWWorkbench-upstream-main.dll",
     "24b05dc267dc6e9faa6cbcb4e85a8491733e0a86b86fbac92f5aa1a0950aa6ca"),
    (upstream / "EventViewerMod-upstream-main.dll",
     "83422a25fffd8d0d39c78fe001eac2fd169fc148858a5004ae3af52fd41544e0"),
    (upstream / "KismetDebuggerMod-upstream-main.dll",
     "4e7907661afe4bb49ebd1677b9e631410554b80ed9df1a9b3ea9e1bcd92a4aae"),
]
for path, digest in real_cases:
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"unchanged input hash mismatch: {path}")
subprocess.run([
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread",
    "-I" + str(root / "include"), "-I" + str(root / "src"),
    str(root / "tests/guest_pe_batch_test.cpp"),
    str(root / "src/guest_pe_batch.cpp"), str(root / "src/pe_import_scan.cpp"),
    str(root / "src/guest_pe_loader.cpp"),
    str(root / "src/pe_loader.cpp"), str(root / "src/pe_tls_registration.cpp"),
    str(root / "src/static_tls_bank.cpp"), str(root / "src/guest_teb_lifecycle.cpp"),
    str(root / "src/win_api_shim.cpp"), str(root / "src/win64_bridge.S"),
    "-o", str(out),
], check=True, timeout=30)
subprocess.run(["arch", "-x86_64", str(out), str(fixture),
                *[str(path) for path, _ in real_cases]], check=True, timeout=30)
if hashlib.sha256(fixture.read_bytes()).hexdigest() != expected:
    raise RuntimeError("synthetic fixture changed")
print(f"UNCHANGED fixture sha256={expected}")
for path, digest in real_cases:
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"unchanged input modified: {path}")
print("UNCHANGED five upstream DLL hashes")
