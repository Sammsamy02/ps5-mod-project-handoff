#!/usr/bin/env python3
"""Real reviewed import and module-policy wiring in an isolated x64 fixture."""
from pathlib import Path
import hashlib
import subprocess

root = Path(__file__).resolve().parents[1]
fixture = root.parents[1] / "cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll"
expected = "acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce"
truefov = root.parent / "third_party/TrueFOV-upstream-winmm.dll"
truefov_hash = "5f23eee1c7479e75699d26d339ea69269eb0da0c33fc9be4cd60957ee63302a3"
decorated = root.parents[1] / "cg-max-ready-20261013-03/cap13/upstream-inputs/ZeroCompanyMandoWardrobe-upstream-main.dll"
decorated_hash = "821ecad6c9f7e61faa2a07b31c216475768b72c2066369d3b0b88f715b33a40c"
before = hashlib.sha256(fixture.read_bytes()).hexdigest()
if before != expected:
    raise RuntimeError("synthetic fixture hash mismatch")
if hashlib.sha256(truefov.read_bytes()).hexdigest() != truefov_hash:
    raise RuntimeError("TrueFOV hash mismatch")
if hashlib.sha256(decorated.read_bytes()).hexdigest() != decorated_hash:
    raise RuntimeError("decorated import sample hash mismatch")
out = root / "tests/guarded-shim-policy-host"
subprocess.run([
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread",
    "-I" + str(root / "include"), "-I" + str(root / "src"),
    str(root / "tests/guarded_shim_policy_test.cpp"),
    str(root / "src/guest_pe_loader.cpp"), str(root / "src/pe_loader.cpp"),
    str(root / "src/pe_tls_registration.cpp"), str(root / "src/static_tls_bank.cpp"),
    str(root / "src/guest_teb_lifecycle.cpp"), str(root / "src/win_api_shim.cpp"),
    str(root / "src/win64_bridge.S"), "-o", str(out),
], check=True, timeout=30)
subprocess.run(["arch", "-x86_64", str(out), str(fixture), str(truefov),
                str(decorated)], check=True, timeout=20)
after = hashlib.sha256(fixture.read_bytes()).hexdigest()
if after != before:
    raise RuntimeError("synthetic fixture changed")
if hashlib.sha256(truefov.read_bytes()).hexdigest() != truefov_hash or \
        hashlib.sha256(decorated.read_bytes()).hexdigest() != decorated_hash:
    raise RuntimeError("original DLL changed")
print(f"UNCHANGED fixture sha256={after}")
