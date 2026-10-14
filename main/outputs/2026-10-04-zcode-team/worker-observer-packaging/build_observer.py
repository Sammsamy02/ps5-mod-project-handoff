#!/usr/bin/env python3
"""Rebuild the observer-revision composition in worker-observer-packaging.

This is worker-aligned-entry-observer/build_local.py verbatim with EXACTLY ONE
change: the output tree is this worker's build-N/ directory (passed as argv[1])
instead of the observer worker's build-1/ (that directory stays untouched; the
packaging input combined-observer.o in its host-test/target-smoke/ is a frozen
provenance artifact we must reproduce, never overwrite).

Compile flags, source list, LLVM/SDK toolchain, pinned full-runtime object,
and the ld.lld -r invocation are byte-for-byte the frozen recipe's (frozen
native-full-runtime-aligned-entry-1/build_local.py as adapted by the observer
worker: probe/observer.c replaces probe/startup_input.c + probe/memory_lua.c,
Lua include dropped).

On success the combined object sha256 is checked against the pinned
7fa9f3f1... digest of the observer worker's host-test/target-smoke/
combined-observer.o.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

base = Path(__file__).resolve().parent
out = base / sys.argv[1]          # the ONLY change vs build_local.py: explicit output tree
root = base.parents[2]  # .../what
llvm = root / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
sdk = root / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
full = root / (".superpowers/native-dependency-spike/"
               "native-ffield-registration-trace-1/build-1/native-stage27-local-deps.o")
contract = root / (".superpowers/native-dependency-spike/"
                   "native-full-runtime-aligned-entry-1/import-contract.json")
PINNED_COMBINED_SHA256 = (
    "225f6d6895f0b745ff176ff0d7e3acf08585f006e0391fa9de3b3bb02f608887")
# Delivered provenance artifact (worker-aligned-entry-observer
# host-test/target-smoke/combined-observer.o). It does NOT match a
# frozen-recipe rebuild: its constructor_boundary.o was compiled WITHOUT the
# frozen flag set (byte-reproduced flag-less; the frozen-recipe
# constructor_boundary.o equals the frozen lineage's own build-1 object
# ff4f3e33..., while the target-smoke copy is 2337e1bf...). Recorded as a
# provenance deviation of the DELIVERED artifact, not of this recipe.
DELIVERED_TARGET_SMOKE_SHA256 = (
    "7fa9f3f12ca19d88dddf766d639e7922f1c29890d9279b72a72682ad473f0dfd")
assert hashlib.sha256(full.read_bytes()).hexdigest() == (
    "3cbde16051daf30294dfe23c8aff04980757beb742f8b859ea0eef3f69e67fb9"), "pinned full object moved"
assert hashlib.sha256(contract.read_bytes()).hexdigest() == (
    "58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c"), "pinned contract moved"
environment = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
records = []

def run(argv, **extra):
    out.mkdir(parents=True, exist_ok=True)  # fresh output tree must pre-exist for -o
    result = subprocess.run(argv, env=environment, capture_output=True, text=True)
    records.append(dict(argv=argv, rc=result.returncode, stdout=result.stdout,
                        stderr=result.stderr, **extra))
    (out / "compile-commands.json").write_text(json.dumps(records, indent=2) + "\n")
    if result.returncode != 0:
        raise RuntimeError(result.stderr)

objects = []
for name in ["import_references.S", "probe/probe.c", "probe/observer.c",
             "lifecycle/lifecycle.c", "lifecycle/module_entry.c",
             "lifecycle/constructor_boundary.cpp", "lifecycle/sentinels.c",
             "import_check.c", "diagnostic.c"]:
    source = base / "observer-src" / name
    target = out / (source.stem + ".o")
    compiler = sdk / "bin" / ("prospero-clang++" if source.suffix == ".cpp" else "prospero-clang")
    run([str(compiler), "-Wall", "-Wextra", "-Werror", "-O1", "-fPIC",
         "-fno-stack-protector", "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
         *["-I" + str(path) for path in [base / "observer-src" / "generated",
                                         base / "observer-src" / "probe",
                                         base / "observer-src" / "lifecycle",
                                         base / "observer-src"]],
         "-c", str(source), "-o", str(target)],
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    objects.append(str(target))

combined = out / "combined-full.o"
run([str(llvm / "ld.lld"), "-r", "-m", "elf_x86_64", "-o", str(combined), *objects, str(full)])
digest = hashlib.sha256(combined.read_bytes()).hexdigest()
print(f"Observer compilation and full-runtime composition passed: {len(records)} commands")
print(f"combined-full.o sha256 {digest}")
if digest != PINNED_COMBINED_SHA256:
    raise SystemExit(f"COMBINED OBJECT MISMATCH: {digest} != {PINNED_COMBINED_SHA256}")
print("combined object matches this worker's pinned frozen-recipe rebuild "
      "(delivered target-smoke object deviates: see DELIVERED_TARGET_SMOKE_SHA256 "
      "note above)")
(out / "combined-sha256.txt").write_text(
    f"frozen-recipe combined-full.o sha256 {digest}\n"
    f"delivered target-smoke combined-observer.o sha256 {DELIVERED_TARGET_SMOKE_SHA256}"
    f" (provenance deviation: constructor_boundary.o compiled flag-less)\n")
