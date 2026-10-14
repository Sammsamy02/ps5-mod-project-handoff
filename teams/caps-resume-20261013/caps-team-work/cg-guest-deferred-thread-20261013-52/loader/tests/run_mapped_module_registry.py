#!/usr/bin/env python3
"""Build and run mapped-module registration checks on the x64 Mac host."""
from pathlib import Path
import hashlib
import subprocess

root = Path(__file__).resolve().parents[1]
fixture = root.parents[1] / "cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll"
expected = "acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce"
if hashlib.sha256(fixture.read_bytes()).hexdigest() != expected:
    raise RuntimeError("synthetic fixture hash mismatch")
out = root / "tests/mapped-module-registry-host"
subprocess.run([
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread",
    "-I" + str(root / "include"), "-I" + str(root / "src"),
    str(root / "tests/mapped_module_registry_test.cpp"),
    str(root / "src/guest_pe_loader.cpp"), str(root / "src/pe_loader.cpp"),
    str(root / "src/pe_tls_registration.cpp"), str(root / "src/static_tls_bank.cpp"),
    str(root / "src/guest_teb_lifecycle.cpp"), str(root / "src/win_api_shim.cpp"),
    str(root / "src/win64_bridge.S"), "-o", str(out),
], check=True, timeout=30)
subprocess.run(["arch", "-x86_64", str(out), str(fixture)], check=True, timeout=20)
if hashlib.sha256(fixture.read_bytes()).hexdigest() != expected:
    raise RuntimeError("synthetic fixture changed")
print(f"UNCHANGED fixture sha256={expected}")
