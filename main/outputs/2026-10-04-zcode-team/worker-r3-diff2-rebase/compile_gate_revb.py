#!/usr/bin/env python3
"""worker-r3-diff2-rebase: compile gate for DIFF 2 rev B under the EXACT
composition initializer unit argv (the notify-cancel build-1 receipt argv for
unreal-unrealinitializer, 186 args, with the 4 args the composition script
prepends at argv[1:1]: -I ue4ss-native-tick-file-owner-1/include
-I ue4ss-native-tick-witness-1/include; source/-o/-MF redirected; LLVM_CONFIG
env), exactly as REPLACE-ARGV.md documents the attested c85c42f7 compile-gate.

Variants: control (unpatched c85c42f7), fold0 (rev-B patched, no define),
fold1 (rev-B patched + -DUE4SS_R3_MARSHAL_MODE=1), each +/- -Werror.
Gates: fold0 object sha256 == control object sha256 (byte-faithful fold 0);
control object reproduces the attested fe639a85... object byte-exactly;
fold1 compiles clean and adds no new -Werror diagnostic kind vs the baseline.
Writes ONLY under this worker's output dir."""
import hashlib, json, os, re, subprocess, sys, time
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
ME = W / "outputs/2026-10-04-zcode-team/worker-r3-diff2-rebase"
RECEIPT = W / ".superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json"
OWNER = W / ".superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1"
SPIKES = OWNER.parent
ATTESTED_OBJ_SHA = "fe639a859b2f33d1ecc4d000e9e1980c7b53413269acbb4ffe823e973dcc496f"

unit = json.loads(RECEIPT.read_text())["selected_runtime_units"]["unreal-unrealinitializer"]
argv0 = list(unit["argv"])
argv0[1:1] = ["-I", str(OWNER / "include"), "-I", str(SPIKES / "ue4ss-native-tick-witness-1/include")]
src_arg = next(x for x in argv0 if x.endswith("UnrealInitializer.cpp"))
oi, mi = argv0.index("-o"), argv0.index("-MF")
ENV = dict(os.environ, LLVM_CONFIG=unit["LLVM_CONFIG"])

TARGET = W / "outputs/2026-10-04-zcode-team/worker-initializer-integration/staging/UnrealInitializer.cpp"
SOURCES = {
    "control": TARGET,
    "fold0": ME / "work/patched/staging/UnrealInitializer.cpp",
    "fold1": ME / "work/patched/staging/UnrealInitializer.cpp",
}
# The staging-local quoted includes (SeedTable.h / SeedValidation.hpp) live next to the
# attested source; the control variant therefore compiles the attested file AT ITS HOME
# PATH under the EXACT attested argv. The patched copies (which live under this worker's
# work dir) get ONE terminal -I pointing at the attested staging dir; no other header in
# the include chain shares those names, so resolution is identical and the token stream
# is unaffected.
STAGING_LOCAL_INCLUDE = W / "outputs/2026-10-04-zcode-team/worker-initializer-integration/staging"

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def run_variant(tag, source, extra_flags, preprocess_only=False):
    out = ME / "work/gate" / tag
    out.mkdir(parents=True, exist_ok=True)
    argv = [str(x) for x in argv0]
    argv[argv.index(src_arg)] = str(source)
    argv[oi + 1] = str(out / "unreal-unrealinitializer.o")
    argv[mi + 1] = str(out / "unreal-unrealinitializer.d")
    argv += extra_flags
    if preprocess_only:
        # -E only: drop the object/dependency machinery, emit the preprocessed TU
        argv = [a for a in argv if a not in ("-c", "-o", "-MD", "-MF",
                                             str(out / "unreal-unrealinitializer.o"),
                                             str(out / "unreal-unrealinitializer.d"))]
        argv += ["-E", "-o", str(out / "unreal-unrealinitializer.i")]
    t0 = time.monotonic()
    p = subprocess.run(argv, env=ENV, capture_output=True, text=True)
    rec = {"tag": tag, "source": str(source), "extra_flags": extra_flags,
           "exit": p.returncode, "seconds": round(time.monotonic() - t0, 1),
           "stderr_path": str(out / "compile.stderr"),
           "n_diagnostics": p.stderr.count("warning:") + p.stderr.count("error:"),
           "errors": [l for l in p.stderr.splitlines() if "error:" in l][:20],
           "warnings": [l for l in p.stderr.splitlines() if "warning:" in l][:40]}
    if preprocess_only:
        rec["preprocessed"] = str(out / "unreal-unrealinitializer.i")
        ipath = out / "unreal-unrealinitializer.i"
        rec["preprocessed_emitted"] = ipath.is_file()
        rec["preprocessed_sha256_linemarkers_stripped"] = sha_stripped(ipath) if ipath.is_file() else None
    elif p.returncode == 0:
        obj = out / "unreal-unrealinitializer.o"
        rec["object"] = str(obj)
        rec["object_bytes"] = obj.stat().st_size
        rec["object_sha256"] = sha(obj)
    (out / "compile.stderr").write_text(p.stderr)
    (out / "command.json").write_text(json.dumps({"argv": argv}, indent=1) + "\n")
    return rec

def sha_stripped(p):
    # Drop linemarkers, then drop blank lines (clang -E preserves source blank
    # spacing where stripped comments sat; tokens are what matter). Token-bearing
    # lines are compared verbatim.
    body = [l for l in Path(p).read_text(errors="replace").splitlines()
            if l.strip() and not l.startswith("#")]
    return hashlib.sha256("\n".join(body).encode()).hexdigest()

def werr_kinds(tag):
    out = set()
    for l in (ME / "work/gate" / tag / "compile.stderr").read_text().splitlines():
        m = re.search(r"(error|warning): (.*)", l)
        if m: out.add(m.group(1) + ": " + m.group(2))
    return out

results = {}
jobs = [
    ("control", "control", []),
    ("control-werror", "control", ["-Werror"]),
    ("fold0", "fold0", []),
    ("fold0-werror", "fold0", ["-Werror"]),
    ("fold1", "fold1", ["-DUE4SS_R3_MARSHAL_MODE=1"]),
    ("fold1-werror", "fold1", ["-DUE4SS_R3_MARSHAL_MODE=1", "-Werror"]),
]
for tag, src, flags in jobs:
    extra = list(flags)
    if src != "control":
        extra += ["-I", str(STAGING_LOCAL_INCLUDE)]
    r = run_variant(tag, SOURCES[src], extra)
    results[tag] = r
    print(f"{tag:14s} exit={r['exit']:3d} {r['seconds']:6.1f}s diags={r['n_diagnostics']:3d}"
          + (f" obj={r.get('object_sha256','')[:16]}" if "object_sha256" in r else "")
          + (f" err0={r['errors'][0][:90]}" if r["errors"] else ""), flush=True)

# fold-0 preprocessor token-stream equivalence (corroborates the object gate)
results["preproc"] = {
    "control": run_variant("control-E", SOURCES["control"], [], preprocess_only=True),
    "fold0": run_variant("fold0-E", SOURCES["fold0"], ["-I", str(STAGING_LOCAL_INCLUDE)], preprocess_only=True),
}
pp_eq = results["preproc"]["control"]["preprocessed_sha256_linemarkers_stripped"] == \
        results["preproc"]["fold0"]["preprocessed_sha256_linemarkers_stripped"]
results["preproc"]["linemarker_stripped_equal"] = pp_eq

ctrl_sha = results["control"].get("object_sha256")
f0_sha = results["fold0"].get("object_sha256")
f1_sha = results["fold1"].get("object_sha256")
gate = {
    "attested_control_reproduced": ctrl_sha == ATTESTED_OBJ_SHA,
    "fold0_object_identity_vs_control": (f0_sha is not None and f0_sha == ctrl_sha),
    "fold0_preprocessor_equivalence": pp_eq,
    "fold1_compiles_clean": results["fold1"]["exit"] == 0,
    "fold1_object_differs_from_fold0": f1_sha is not None and f1_sha != f0_sha,
    "werror_parity": {
        "fold0_vs_control": {
            "new_kinds": sorted(werr_kinds("fold0-werror") - werr_kinds("control-werror")),
            "missing_kinds": sorted(werr_kinds("control-werror") - werr_kinds("fold0-werror")),
        },
        "fold1_vs_control": {
            "new_kinds": sorted(werr_kinds("fold1-werror") - werr_kinds("control-werror")),
            "missing_kinds": sorted(werr_kinds("control-werror") - werr_kinds("fold1-werror")),
        },
    },
}
gate["verdict"] = "PASS" if all([
    gate["attested_control_reproduced"], gate["fold0_object_identity_vs_control"],
    gate["fold0_preprocessor_equivalence"], gate["fold1_compiles_clean"],
    gate["fold1_object_differs_from_fold0"],
    not gate["werror_parity"]["fold0_vs_control"]["new_kinds"],
    not gate["werror_parity"]["fold1_vs_control"]["new_kinds"]]) else "FAIL"

receipt = {
    "worker": "worker-r3-diff2-rebase",
    "argv_provenance": {
        "receipt": str(RECEIPT), "unit": "unreal-unrealinitializer",
        "argv_args": len(argv0), "prepended": ["-I", str(OWNER / "include"), "-I", str(SPIKES / "ue4ss-native-tick-witness-1/include")],
        "control_compiled_at_home_path": True,
        "patched_variants_terminal_include": ["-I", str(STAGING_LOCAL_INCLUDE)],
        "mechanism_doc": "worker-initializer-integration/REPLACE-ARGV.md; worker-composition-4-fix/build_runtime_composition4_fixed.py line 94 (argv[1:1] prepend)",
    },
    "attested_object_sha256": ATTESTED_OBJ_SHA,
    "objects": {t: results[t].get("object_sha256") for t in ("control", "fold0", "fold1")},
    "gate": gate,
    "results": results,
}
(ME / "receipts" / "compile-gate.json").write_text(json.dumps(receipt, indent=1) + "\n")
print(json.dumps(gate, indent=1))
sys.exit(0 if gate["verdict"] == "PASS" else 1)
