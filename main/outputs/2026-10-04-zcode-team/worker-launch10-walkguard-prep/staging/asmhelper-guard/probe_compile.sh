#!/bin/sh
# probe_compile.sh — worker-launch10-walkguard-prep, 2026-10-05.
# Pre-pass probe: replay the composition's REAL asmhelper-asmhelper unit argv
# (native-coherent-runtime-1/coherent-full-1/asmhelper-asmhelper.command.json,
# 148 args) with ONLY the source, -o and -MF tokens redirected into THIS
# worker's directory (the SEC32-3 redirection set; -MF redirected too, which
# the WST compile gate did not do — see launch-11 REPORT section 6 incident).
#
# Probes (throwaway objects, never linked into any candidate):
#   probe 0: WST guarded source, fold define ABSENT  -> expect WST fold-0 81dd832b...
#   probe 1: WST guarded source + -DRC_ASM_WALK_RX_GUARD=1 -> expect WST fold-1 6604ce2d...
#   probe 2: ONE-TOKEN source flip of the fold-default line
#            (#define RC_ASM_WALK_RX_GUARD 0 -> 1), no argv define -> the
#            candidate's one-define-flip route under the ALREADY-proven edition.
#   probe 3: pristine port unit (control) -> expect the shipped shared object's
#            content identity is NOT reproduced here (basename differs), only
#            rc=0 is asserted.
# Nothing in the spike trees, driver staging or receipts is modified. No console contact.
set -eu
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
MY="$TEAM/worker-launch10-walkguard-prep"
CMDJSON="$WHAT/.superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/asmhelper-asmhelper.command.json"
OUTDIR="$MY/staging/asmhelper-guard/probe"
NM="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm"
OBJDUMP="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump"

export LLVM_CONFIG="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
export PATH="$(dirname "$LLVM_CONFIG"):$PATH"

python3 - "$CMDJSON" "$MY" "$OUTDIR" "$NM" "$OBJDUMP" <<'EOF'
import json, os, subprocess, sys, hashlib

cmdjson, my, outdir, nm, objdump = sys.argv[1:6]
argv = json.load(open(cmdjson))["argv"]
os.makedirs(outdir, exist_ok=True)

def redirect(src, outname, extra_defines):
    # ONLY source/-o/-MF redirected; every other token identical.
    a = list(argv)
    assert "-MF" in a and a.count("-c") == 1
    a[a.index("-MF") + 1] = os.path.join(outdir, outname + ".d")
    a[a.index("-o") + 1] = os.path.join(outdir, outname + ".o")
    a[a.index("-c") + 1] = src
    for d in extra_defines:
        a.append(d)  # probe 1 ONLY (documents the argv route; the composition's
                     # SEC32-3 assertion would refuse this 4th drift — measured)
    return a

guarded = os.path.join(my, "staging/asmhelper-guard/ASMHelper.guarded.cpp")
pristine = os.path.join(my, "staging/asmhelper-guard/ASMHelper.baseline.cpp")

# probe 2 source: ONE token changed on the fold-default line
flipped = os.path.join(outdir, "ASMHelper.guarded.fold1flip.cpp")
text = open(guarded).read()
needle = "#define RC_ASM_WALK_RX_GUARD 0"
assert text.count(needle) == 1, "fold-default line not unique"
open(flipped, "w").write(text.replace(needle, "#define RC_ASM_WALK_RX_GUARD 1"))

probes = [
    ("probe0-fold0-absent", redirect(guarded, "probe0", [])),
    ("probe1-fold1-argv-define", redirect(guarded, "probe1", ["-DRC_ASM_WALK_RX_GUARD=1"])),
    ("probe2-fold1-source-flip", redirect(flipped, "probe2", [])),
    ("probe3-pristine-control", redirect(pristine, "probe3", [])),
]
for name, a in probes:
    with open(os.path.join(outdir, name + ".stderr"), "wb") as err:
        p = subprocess.run(a, stdout=err, stderr=subprocess.STDOUT)
    obj = a[a.index("-o") + 1]
    ok = os.path.isfile(obj) and os.path.getsize(obj) > 0
    h = hashlib.sha256(open(obj, "rb").read()).hexdigest() if ok else None
    sysc = None
    if ok:
        d = subprocess.run([objdump, "-d", obj], capture_output=True, text=True)
        sysc = d.stdout.count("\tsyscall")
    print(f"{name}: rc={p.returncode} obj_ok={ok} sha256={h} syscall_insns={sysc} argc={len(a)}")
    tail = open(os.path.join(outdir, name + ".stderr")).read().strip()
    if tail:
        print(f"  stderr tail: {tail[-300:]}")

def syms(obj):
    r = subprocess.run([nm, "--extern-only", "--defined-only", "--format=posix", obj],
                       capture_output=True, text=True)
    return {l.split()[0] for l in r.stdout.splitlines() if l.strip()}

if all(os.path.isfile(p[1][a.index("-o") + 1]) for p, a in []):
    pass
for label, obj in (("probe0", "probe0.o"), ("probe1", "probe1.o"), ("probe2", "probe2.o")):
    path = os.path.join(outdir, obj)
    if os.path.isfile(path):
        s = syms(path)
        rc_exports = sorted(x for x in s if x.startswith("_ZN2RC3ASM"))
        print(f"{label}: RC::ASM exports ({len(rc_exports)}): {rc_exports}")
print("PROBE DONE")
EOF
