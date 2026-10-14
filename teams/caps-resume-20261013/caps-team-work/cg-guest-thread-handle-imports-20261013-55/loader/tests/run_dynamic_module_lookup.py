#!/usr/bin/env python3
"""Build and run guarded dynamic-module lookup checks on x64 Mac."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / "tests/dynamic-module-lookup-host"
subprocess.run([
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g",
    "-Wall", "-Wextra", "-Werror", "-pthread",
    "-I" + str(root / "include"), "-I" + str(root / "src"),
    str(root / "tests/dynamic_module_lookup_test.cpp"),
    str(root / "src/win_api_shim.cpp"), str(root / "src/win64_bridge.S"),
    "-o", str(out),
], check=True, timeout=30)
subprocess.run(["arch", "-x86_64", str(out)], check=True, timeout=20)
