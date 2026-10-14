#!/usr/bin/env python3
"""Witness re-run of the launch-10 initializer compile gate.

Frozen 190-arg unit argv from worker-launch10-gmalloc-integration/receipts/compile-gate.argv.json
with ONLY input/output/-MF swapped into this worker's own directory (read-only elsewhere).
Env LLVM_CONFIG pinned to the same llvm-config the controller used.
"""
import json
import os
import subprocess
import sys

WHAT = "/WORKSPACE/.codex/worktrees/translation-core/what"
WITNESS = f"{WHAT}/outputs/2026-10-04-zcode-team/worker-launch10-build-witness/receipts"
ARGV_JSON = (
    f"{WHAT}/outputs/2026-10-04-zcode-team/"
    "worker-launch10-gmalloc-integration/receipts/compile-gate.argv.json"
)
OLD_INPUT = (
    f"{WHAT}/outputs/2026-10-04-zcode-team/"
    "worker-launch10-gmalloc-integration/staging-l10-gmalloc-validator/UnrealInitializer.cpp"
)
NEW_INPUT = (
    f"{WHAT}/outputs/2026-10-04-zcode-team/"
    "worker-launch10-readpath-guard/staging/UnrealInitializer.cpp"
)
OLD_OBJ = (
    f"{WHAT}/outputs/2026-10-04-zcode-team/"
    "worker-launch10-gmalloc-integration/receipts/unreal-unrealinitializer.o"
)
NEW_OBJ = f"{WITNESS}/unreal-unrealinitializer-witness.o"
OLD_DEP = (
    f"{WHAT}/outputs/2026-10-04-zcode-team/worker-launch10-prebuild/"
    "ue4ss-native-supplier-composition-6-l10/run-b/unreal-unrealinitializer.d"
)
NEW_DEP = f"{WITNESS}/unreal-unrealinitializer-witness.d"

argv = json.load(open(ARGV_JSON))
assert isinstance(argv, list), "argv json must be a list"
n0 = len(argv)

swaps = 0
for i, a in enumerate(argv):
    if a == OLD_INPUT:
        argv[i] = NEW_INPUT
        swaps += 1
    elif a == OLD_OBJ:
        argv[i] = NEW_OBJ
        swaps += 1
    elif a == OLD_DEP:
        argv[i] = NEW_DEP
        swaps += 1

assert swaps == 3, f"expected exactly 3 path swaps (input/-o/-MF), got {swaps}"
assert len(argv) == n0, "argv length changed"
print(f"argv args: {len(argv)} (swapped input/-o/-MF = {swaps})", flush=True)

env = dict(os.environ)
env["LLVM_CONFIG"] = (
    f"{WHAT}/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
    "LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
)

with open(f"{WITNESS}/compile-gate.stdout", "wb") as out, open(
    f"{WITNESS}/compile-gate.stderr", "wb"
) as err:
    p = subprocess.run(argv, stdout=out, stderr=err, env=env, cwd=WHAT)

print(f"exit={p.returncode}", flush=True)
sys.stdout.write(
    json.dumps(
        {
            "exit": p.returncode,
            "argv_len": len(argv),
            "object": NEW_OBJ,
            "llvm_config": env["LLVM_CONFIG"],
        },
        indent=2,
    )
    + "\n"
)
sys.exit(0 if p.returncode == 0 else 1)
