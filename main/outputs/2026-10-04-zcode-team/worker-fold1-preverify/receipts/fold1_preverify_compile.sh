#!/bin/sh
# fold1_preverify_compile.sh — worker-fold1-preverify, 2026-10-05.
# L11 pre-verify: rebuild BOTH walkguard folds of the staged guarded ASMHelper
# against the CURRENT header set, so the launch-11 compose does not discover
# header drift late.
#
# Argv provenance (launch10 candidate receipts, launch10-lineage):
#   retained-asmhelper-asmhelper argv (148 tokens) from
#   worker-launch10-walkguard-prep/ue4ss-native-supplier-composition-6-l10wg/run-a/commands.json
#   — the exact argv the packaging used for asmhelper-asmhelper (SEC32-3
#   retained-unit replacement: only source/-o/-MF may drift vs the shared
#   coherent-full-1 unit command; asserted below, never assumed).
#
# Builds (throwaway pre-verify objects, never linked into any candidate):
#   fold0: staged ASMHelper.guarded.cpp, fold define ABSENT
#          -> expect shipped fold-0 81dd832b..., 0 syscall instructions
#   fold1: ONE-TOKEN in-file flip (#define RC_ASM_WALK_RX_GUARD 0 -> 1),
#          basename preserved (ASMHelper.guarded.cpp), NO argv define
#          -> the composition-admissible (argc=148) activation route
#          -> old-header-set measurement was f5627427...; drift here = finding
#
# Nothing outside THIS worker's directory is written. No console contact.
set -eu
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
MY="$TEAM/worker-fold1-preverify"
WGP="$TEAM/worker-launch10-walkguard-prep"
CMDJSON="$WGP/ue4ss-native-supplier-composition-6-l10wg/run-a/commands.json"
GUARDED="$WGP/staging/asmhelper-guard/ASMHelper.guarded.cpp"
OUT="$MY/receipts"
NM="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm"
OBJDUMP="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump"

export LLVM_CONFIG="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
export PATH="$(dirname "$LLVM_CONFIG"):$PATH"

python3 - "$CMDJSON" "$GUARDED" "$OUT" "$NM" "$OBJDUMP" "$WGP" <<'EOF'
import json, os, subprocess, sys, hashlib, time

cmdjson, guarded, outdir, nm, objdump, wgp = sys.argv[1:7]

# --- argv reconstruction from the launch10 candidate receipt ---
runs = json.load(open(cmdjson))
argv = None
for e in runs:
    if e.get("name") == "retained-asmhelper-asmhelper":
        argv = list(e["argv"])
assert argv is not None, "retained-asmhelper-asmhelper not in run-a commands.json"

# SEC32-3 equivalence assert vs the shared coherent-full-1 unit command
shared = json.load(open(os.path.join(
    wgp, "..", "..", "..", ".superpowers", "native-dependency-spike",
    "native-coherent-runtime-1", "coherent-full-1",
    "asmhelper-asmhelper.command.json")))["argv"]
assert len(argv) == len(shared) == 148, f"argc drift: {len(argv)} vs {len(shared)}"
diff_idx = [i for i, (a, b) in enumerate(zip(shared, argv)) if a != b]
assert diff_idx == [142, 144, 147], f"drift set not exactly source/-o/-MF: {diff_idx}"
assert argv[141] == "-c" and argv[143] == "-o" and argv[145] == "-MD" and argv[146] == "-MF"

def redirect(src, outbase):
    a = list(argv)
    a[142] = src
    a[144] = os.path.join(outdir, outbase, "ASMHelper.guarded.o")
    a[147] = os.path.join(outdir, outbase, "ASMHelper.guarded.d")
    return a

# --- fold-1 source: ONE token flip, basename preserved ---
fold1_dir = os.path.join(outdir, "fold1")
fold1_src = os.path.join(fold1_dir, "ASMHelper.guarded.cpp")
text = open(guarded).read()
needle = "#define RC_ASM_WALK_RX_GUARD 0"
assert text.count(needle) == 1, "fold-default line not unique"
flipped = text.replace(needle, "#define RC_ASM_WALK_RX_GUARD 1")
open(fold1_src, "w").write(flipped)
tokdelta = sum(1 for a, b in zip(text.split(), flipped.split()) if a != b)
print(f"fold1 source: one-line flip, whitespace-token delta = {tokdelta} (expect 1), basename ASMHelper.guarded.cpp preserved")

probes = [
    ("fold0", redirect(guarded, "fold0")),
    ("fold1", redirect(fold1_src, "fold1")),
]
results = {}
for name, a in probes:
    errpath = os.path.join(outdir, name, "compile.stderr")
    with open(errpath, "wb") as err:
        p = subprocess.run(a, stdout=err, stderr=subprocess.STDOUT)
    obj = a[144]
    ok = os.path.isfile(obj) and os.path.getsize(obj) > 0
    h = hashlib.sha256(open(obj, "rb").read()).hexdigest() if ok else None
    dd = subprocess.run([objdump, "-d", obj], capture_output=True, text=True)
    sysc = dd.stdout.count("\tsyscall")
    ddefined = subprocess.run([nm, "--extern-only", "--defined-only", "--format=posix", obj],
                              capture_output=True, text=True)
    dundef = subprocess.run([nm, "--extern-only", "--undefined-only", "--format=posix", obj],
                            capture_output=True, text=True)
    defined = sorted(l.split()[0] for l in ddefined.stdout.splitlines() if l.strip())
    undefined = sorted(l.split()[0] for l in dundef.stdout.splitlines() if l.strip())
    open(os.path.join(outdir, name, "nm-defined.txt"), "w").write("\n".join(defined) + "\n")
    open(os.path.join(outdir, name, "nm-undefined.txt"), "w").write("\n".join(undefined) + "\n")
    results[name] = {"rc": p.returncode, "sha256": h, "syscalls": sysc,
                     "defined": defined, "undefined": undefined,
                     "stderr": open(errpath).read().strip()}
    print(f"{name}: rc={p.returncode} obj_ok={ok} sha256={h} syscall_insns={sysc} "
          f"defined={len(defined)} undefined={len(undefined)} argc={len(a)}")
    if results[name]["stderr"]:
        print(f"  stderr tail: {results[name]['stderr'][-300:]}")

# --- comparators for the L11 structural gate ---
d0, d1 = results["fold0"], results["fold1"]
print("=== L11 comparators ===")
print(f"fold0_sha={d0['sha256']}")
print(f"fold1_sha={d1['sha256']}")
print(f"fold0_syscalls={d0['syscalls']} fold1_syscalls={d1['syscalls']}")
print(f"defined_surface_equal={d0['defined'] == d1['defined']}")
print(f"undefined_surface_equal={d0['undefined'] == d1['undefined']}")
rc_exports0 = sorted(x for x in d0["defined"] if x.startswith("_ZN2RC3ASM"))
rc_exports1 = sorted(x for x in d1["defined"] if x.startswith("_ZN2RC3ASM"))
print(f"RC::ASM exports fold0 ({len(rc_exports0)}): {rc_exports0}")
print(f"RC::ASM exports fold1 ({len(rc_exports1)}): {rc_exports1}")
print(f"RC::ASM export surface equal={rc_exports0 == rc_exports1}")

# --- include-closure proof: .d diff vs the old-header-set probe receipts ---
probe_dir = os.path.join(wgp, "staging", "asmhelper-guard", "probe")
def dset(path):
    txt = open(path).read()
    # first line: target + deps, continuation lines: deps
    toks = txt.replace("\\\n", " ").split()
    # drop the target token and trailing empty
    return sorted(t for t in toks[1:] if t.endswith((".h", ".hpp", ".ipp", ".inc", ".c", ".cpp", ".cc")))
p0 = dset(os.path.join(probe_dir, "probe0.d"))
p2 = dset(os.path.join(probe_dir, "probe2.d"))
n0 = dset(os.path.join(outdir, "fold0", "ASMHelper.guarded.d"))
n1 = dset(os.path.join(outdir, "fold1", "ASMHelper.guarded.d"))
print("=== include-closure (.d) comparison vs Oct 5 06:57 probe ===")
print(f"probe0 deps={len(p0)} new-fold0 deps={len(n0)} set_equal={p0 == n0}")
print(f"probe2 deps={len(p2)} new-fold1 deps={len(n1)} set_equal={p2 == n1}")
if p0 != n0:
    print("  fold0 closure delta only-in-probe0:", sorted(set(p0) - set(n0))[:10])
    print("  fold0 closure delta only-in-new  :", sorted(set(n0) - set(p0))[:10])
if p2 != n1:
    print("  fold1 closure delta only-in-probe2:", sorted(set(p2) - set(n1))[:10])
    print("  fold1 closure delta only-in-new   :", sorted(set(n1) - set(p2))[:10])
for lbl, ds in (("fold0", n0), ("fold1", n1)):
    freshened = [d for d in ds if "NativeProgramContext.hpp" in d or "live_module_facts.hpp" in d]
    print(f"{lbl}: freshen/capacity headers in TU closure: {freshened}")

# --- header freshness scan under all -I dirs vs probe cutoff ---
incdirs = []
for i, x in enumerate(argv):
    if x == "-I":
        incdirs.append(argv[i + 1])
cutoff = os.path.getmtime(os.path.join(probe_dir, "probe2.o"))
import time as _t
recent = []
total = 0
for d in sorted(set(incdirs)):
    for root, _, files in os.walk(d):
        for f in files:
            p = os.path.join(root, f)
            if f.endswith((".h", ".hpp", ".ipp", ".inc")):
                total += 1
                try:
                    if os.path.getmtime(p) > cutoff:
                        recent.append(p)
                except OSError:
                    pass
print(f"=== header freshness: {total} headers under the {len(set(incdirs))} unique -I dirs, "
      f"{len(recent)} modified after probe cutoff "
      f"({_t.strftime('%Y-%m-%d %H:%M:%S', _t.localtime(cutoff))}) ===")
for p in recent[:20]:
    print("  RECENT:", p)

json.dump({
    "argv_provenance": cmdjson,
    "argv_sha256": hashlib.sha256(open(cmdjson, "rb").read()).hexdigest(),
    "sec32_3_drift_indices_vs_shared_command": diff_idx,
    "guarded_source": guarded,
    "guarded_source_sha256": hashlib.sha256(open(guarded, "rb").read()).hexdigest(),
    "fold1_source": fold1_src,
    "fold1_source_sha256": hashlib.sha256(open(fold1_src, "rb").read()).hexdigest(),
    "results": {k: {kk: vv for kk, vv in v.items() if kk != "stderr"} for k, v in results.items()},
    "closure_set_equal": {"fold0_vs_probe0": p0 == n0, "fold1_vs_probe2": p2 == n1},
    "freshen_headers_in_closure": {"fold0": [d for d in n0 if "NativeProgramContext" in d or "live_module_facts" in d],
                                    "fold1": [d for d in n1 if "NativeProgramContext" in d or "live_module_facts" in d]},
    "headers_scanned": total, "headers_modified_after_probe": len(recent),
    "recent_headers": recent[:50],
}, open(os.path.join(outdir, "evidence", "preverify-measured.json"), "w"), indent=1)
print("PREVERIFY DONE")
EOF
