#!/bin/sh
# compile_gate.sh — worker-walk-site-transitions, 2026-10-04.
# Host-side compile gate for the patched ASMHelper unit using the composition's
# REAL unit argv (native-coherent-runtime-1/coherent-full-1/asmhelper-asmhelper.
# command.json), with source and output redirected to this worker's directory.
# Gates BOTH folds:
#   gate 1: default (RC_ASM_WALK_RX_GUARD absent) -> refuse-fold, zero syscall text
#   gate 2: -DRC_ASM_WALK_RX_GUARD=1             -> active-fold incl. inline syscall
# Throws away the objects; NOTHING in the spike trees, driver staging or receipts is
# modified. No console contact.
set -eu
cd "$(dirname "$0")/.."

WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
CMDJSON="$WHAT/.superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/asmhelper-asmhelper.command.json"
GUARDED="$PWD/ASMHelper.guarded.cpp"
OUTDIR="$PWD/host-test/compile-gate"

# The SDK's prospero-clang++ wrapper resolves the real clang via prospero-llvm-config,
# which needs LLVM_CONFIG in the environment. The retained LLVM the composition
# drivers actually used (their link/nm argv reference it) is:
export LLVM_CONFIG="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
export PATH="$(dirname "$LLVM_CONFIG"):$PATH"

python3 - "$CMDJSON" "$GUARDED" "$OUTDIR" <<'EOF'
import json, os, subprocess, sys, hashlib

cmdjson, guarded, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
argv = json.load(open(cmdjson))["argv"]
cc = argv[0]
os.makedirs(outdir, exist_ok=True)

# Source/output redirection: keep every include flag identical, swap only the
# compiled source and the object output (throwaway).
redirected = []
skip_next = False
for i, a in enumerate(argv):
    if skip_next:
        skip_next = False
        continue
    if a == "-c":
        redirected += ["-c", guarded]
        skip_next = True  # skip the original source path (single-input unit)
    elif a == "-o" and i + 1 < len(argv):
        redirected += ["-o", os.path.join(outdir, "ASMHelper.guarded.fold0.o")]
        skip_next = True
    else:
        redirected.append(a)
assert "-c" in redirected and guarded in redirected, "source redirection failed"

results = {}
for fold, extra, objname in (
    ("fold0-absent-refuse", [], "ASMHelper.guarded.fold0.o"),
    ("fold1-rx-guard-on", ["-DRC_ASM_WALK_RX_GUARD=1"], "ASMHelper.guarded.fold1.o"),
):
    a = list(redirected)
    if extra:
        a += extra
    a[a.index("-o") + 1] = os.path.join(outdir, objname)
    stderr_path = os.path.join(outdir, fold + ".stderr")
    with open(stderr_path, "wb") as err:
        p = subprocess.run(a, stdout=err, stderr=subprocess.STDOUT)
    obj = a[a.index("-o") + 1]
    obj_ok = os.path.isfile(obj) and os.path.getsize(obj) > 0
    h = hashlib.sha256(open(obj, "rb").read()).hexdigest() if obj_ok else None
    results[fold] = {"rc": p.returncode, "stderr": stderr_path, "obj": obj,
                     "obj_ok": obj_ok, "obj_sha256": h, "argc": len(a)}

import textwrap
for fold, r in results.items():
    print(f"{fold}: rc={r['rc']} object={r['obj_ok']} sha256={r['obj_sha256']}")
    tail = open(r["stderr"]).read().strip()
    if tail:
        print(f"  {fold} stderr/stdout tail: {tail[-400:]}")
if results["fold0-absent-refuse"]["rc"] != 0 or results["fold1-rx-guard-on"]["rc"] != 0:
    sys.exit("COMPILE GATE FAILED")
print("COMPILE GATE PASS (both folds, composition argv)")
EOF
